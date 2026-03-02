#include "audio_pipeline.h"
#include "i2s_audio.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include "signal_generator.h"
#ifdef DSP_ENABLED
#include "dsp_pipeline.h"
#include "dsps_mulc.h"
#include "dsps_add.h"
#endif
#ifdef DAC_ENABLED
#include "dac_hal.h"
#endif
#ifndef NATIVE_TEST
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include <cstring>
#include <cmath>
#include "audio_input_source.h"
#ifdef USB_AUDIO_ENABLED
#include "usb_audio.h"
#endif

// ===== Constants =====
static const int FRAMES      = I2S_DMA_BUF_LEN;    // 256 stereo frames per DMA buffer
static const int RAW_SAMPLES = FRAMES * 2;          // 512 int32_t per buffer (L+R interleaved)
static const float MAX_24BIT_F = 8388607.0f;        // 2^23 - 1

// ===== DMA Buffers — MUST be in internal SRAM (DMA cannot access PSRAM) =====
// 4 input lanes + 1 output + 1 DSP bridge: ~12 KB of internal SRAM
static int32_t _rawBuf[AUDIO_PIPELINE_MAX_INPUTS][RAW_SAMPLES];
static int32_t _dacBuf[RAW_SAMPLES];
// DSP bridge: float↔RJ-int32 scratch (shared across lanes, never concurrent)
static int32_t _dspBridgeBuf[RAW_SAMPLES];

// ===== Float Working Buffers (PSRAM-allocated on ESP32, static on native) =====
static float *_laneL[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float *_laneR[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float *_outL = nullptr;
static float *_outR = nullptr;

// Noise gate fade-out: last open-gate frame per ADC lane (PSRAM, 4 KB total)
static float *_gatePrevL[2] = {};
static float *_gatePrevR[2] = {};

// DSP swap hold: last good pipeline output frame (PSRAM, 2 KB total)
static float *_swapHoldL = nullptr;
static float *_swapHoldR = nullptr;

#ifdef NATIVE_TEST
static float _laneL_buf[AUDIO_PIPELINE_MAX_INPUTS][I2S_DMA_BUF_LEN];
static float _laneR_buf[AUDIO_PIPELINE_MAX_INPUTS][I2S_DMA_BUF_LEN];
static float _outL_buf[I2S_DMA_BUF_LEN];
static float _outR_buf[I2S_DMA_BUF_LEN];
static float _gatePrevL_buf[2][I2S_DMA_BUF_LEN];
static float _gatePrevR_buf[2][I2S_DMA_BUF_LEN];
static float _swapHoldL_buf[I2S_DMA_BUF_LEN];
static float _swapHoldR_buf[I2S_DMA_BUF_LEN];
#endif

// ===== Routing Matrix =====
// gain[out_ch][in_ch]: 8 output channels × 8 input channels, linear gain
static float _matrixGain[AUDIO_PIPELINE_MATRIX_SIZE][AUDIO_PIPELINE_MATRIX_SIZE] = {};

// ===== Runtime Bypass Flags =====
static bool _inputBypass[AUDIO_PIPELINE_MAX_INPUTS] = {};
static bool _dspBypass[AUDIO_PIPELINE_MAX_INPUTS]   = {false, false, true, true};
static bool _matrixBypass = false;   // Matrix active: routes ADC1 L/R + Siggen L/R → DAC L/R
static bool _outputBypass = false;

// ===== Registered Input Sources =====
static AudioInputSource _sources[AUDIO_PIPELINE_MAX_INPUTS] = {
    AUDIO_INPUT_SOURCE_INIT, AUDIO_INPUT_SOURCE_INIT,
    AUDIO_INPUT_SOURCE_INIT, AUDIO_INPUT_SOURCE_INIT,
};

// ===== USB Source Adapter Functions (ESP32 only) =====
#ifdef USB_AUDIO_ENABLED
static uint32_t _usb_src_read(int32_t *dst, uint32_t frames) {
    return usb_audio_read(dst, frames);
}
static bool _usb_src_isActive(void) {
    return usb_audio_is_streaming();
}
static uint32_t _usb_src_getSampleRate(void) {
    return usb_audio_get_negotiated_rate();
}
#endif

// ===== Noise Gate Fade State =====
// _gateFadeCount: counts remaining fade-out buffers (2→1→0) after gate closes.
// Pre-armed to 2 while gate is open so first-close buffer fades rather than cuts.
static int _gateFadeCount[2] = {0, 0};

// ===== DSP Swap Hold State =====
// Set by audio_pipeline_notify_dsp_swap() (called from Core 0 before dsp_swap_config
// sets _swapRequested). Causes pipeline_write_output() to use _swapHoldL/_swapHoldR
// for one iteration, bridging the DSP-skipped buffer gap with the last good frame.
static volatile bool _swapPending = false;

// ===== Task Handle =====
#ifndef NATIVE_TEST
static TaskHandle_t _pipelineTaskHandle = NULL;
#endif

// ===== Raw ADC Diagnostic Snapshot =====
// Written by pipeline task, read by main-loop dump — dirty-flag safe (5s interval).
struct PipelineDiagSnapshot {
    int32_t raw[8];       // First 4 stereo pairs from ADC1 (L0,R0,L1,R1,L2,R2,L3,R3)
    int32_t maxAbs;       // Max |value| in full buffer — signal presence indicator
    int32_t minVal;       // Min signed value in full buffer
    int32_t maxVal;       // Max signed value in full buffer
    uint32_t nonZero;     // Count of non-zero samples — detects all-zero DMA
    bool     captured;    // Set once first capture completes
};
static volatile PipelineDiagSnapshot _adcDiag = {};

// ===== Helpers =====
static inline float clampf(float x) {
    if (x >  1.0f) return  1.0f;
    if (x < -1.0f) return -1.0f;
    return x;
}

// Convert interleaved left-justified int32 → float32 normalized [-1, +1]
static void to_float(const int32_t *raw, float *L, float *R, int frames) {
    for (int f = 0; f < frames; f++) {
        L[f] = (float)(raw[f * 2]     >> 8) / MAX_24BIT_F;
        R[f] = (float)(raw[f * 2 + 1] >> 8) / MAX_24BIT_F;
    }
}

// Convert float32 [-1, +1] → interleaved left-justified int32 for DAC
static void to_int32_lj(const float *L, const float *R, int32_t *raw, int frames) {
    for (int f = 0; f < frames; f++) {
        raw[f * 2]     = (int32_t)(clampf(L[f]) * MAX_24BIT_F) << 8;
        raw[f * 2 + 1] = (int32_t)(clampf(R[f]) * MAX_24BIT_F) << 8;
    }
}

// ===== Pipeline Stages =====

static void pipeline_sync_flags() {
    AppState &s = AppState::getInstance();
    // ADC lanes: web UI toggle (adcEnabled) OR internal bypass flag either suppress the read
    _inputBypass[0] = !s.adcEnabled[0] || s.pipelineInputBypass[0];
    _inputBypass[1] = !s.adcEnabled[1] || s.pipelineInputBypass[1];
    // Siggen + USB: only internal bypass flag applies (no adcEnabled for these lanes)
    _inputBypass[2] = s.pipelineInputBypass[2];
    _inputBypass[3] = s.pipelineInputBypass[3];
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _dspBypass[i] = s.pipelineDspBypass[i];
    }
    _matrixBypass = s.pipelineMatrixBypass;
    _outputBypass = s.pipelineOutputBypass;
}

static void pipeline_read_inputs() {
    // Lane 0: ADC1
    if (!_inputBypass[0]) {
        size_t bytes_read = 0;
        bool ok = i2s_audio_read_adc1(_rawBuf[0], sizeof(_rawBuf[0]), &bytes_read, 500);
        if (!ok || bytes_read == 0) {
            memset(_rawBuf[0], 0, sizeof(_rawBuf[0]));
        }
    } else {
        memset(_rawBuf[0], 0, sizeof(_rawBuf[0]));
    }

    // Lane 1: ADC2 (5ms timeout — non-blocking if ADC2 not present)
    if (!_inputBypass[1] && i2s_audio_adc2_ok()) {
        size_t bytes_read = 0;
        bool ok = i2s_audio_read_adc2(_rawBuf[1], sizeof(_rawBuf[1]), &bytes_read, 5);
        if (!ok || bytes_read == 0) {
            memset(_rawBuf[1], 0, sizeof(_rawBuf[1]));
        }
    } else {
        memset(_rawBuf[1], 0, sizeof(_rawBuf[1]));
    }

    // Lane 2: Signal Generator (never blocks)
    if (!_inputBypass[2] && siggen_is_active() && siggen_is_software_mode()) {
        siggen_fill_buffer(_rawBuf[2], FRAMES, AppState::getInstance().audioSampleRate);
    } else {
        memset(_rawBuf[2], 0, sizeof(_rawBuf[2]));
    }

    // Lane 3: USB Audio
#ifdef USB_AUDIO_ENABLED
    if (!_inputBypass[3] && _sources[3].isActive && _sources[3].isActive()) {
        // Apply host volume/mute as pre-matrix gain
        AppState &as = AppState::getInstance();
        _sources[3].gainLinear = as.usbAudioMute
            ? 0.0f : usb_audio_get_volume_linear();
        uint32_t got = _sources[3].read ? _sources[3].read(_rawBuf[3], FRAMES) : 0;
        if (got < (uint32_t)FRAMES) {
            memset(&_rawBuf[3][got * 2], 0, (FRAMES - got) * 2 * sizeof(int32_t));
        }
        // Apply gain in-place (host volume/mute)
        if (_sources[3].gainLinear != 1.0f) {
            float g = _sources[3].gainLinear;
            for (int i = 0; i < FRAMES * 2; i++) {
                _rawBuf[3][i] = (int32_t)((float)_rawBuf[3][i] * g);
            }
        }
    } else {
        memset(_rawBuf[3], 0, sizeof(_rawBuf[3]));
    }
#else
    memset(_rawBuf[3], 0, sizeof(_rawBuf[3]));
#endif
}

// Noise gate for ADC lanes: prevents PCM1808 noise floor from reaching the DAC.
// Hysteresis thresholds (5 dB window) prevent rapid toggling near threshold.
// Fade-out over 2 buffers (~10.7 ms) using PSRAM prev-frame copy prevents click.
//   OPEN  threshold -65 dBFS: 10^(-65/20) = 5.62e-4 → sq = 3.16e-7 × 512 ≈ 1.62e-4
//   CLOSE threshold -70 dBFS: 10^(-70/20) = 3.16e-4 → sq = 1.00e-7 × 512 ≈ 5.12e-5
static const float GATE_OPEN_THRESH  = 1.62e-4f;  // -65 dBFS
static const float GATE_CLOSE_THRESH = 5.12e-5f;  // -70 dBFS (5 dB hysteresis window)

static bool _gateOpen[2] = {false, false};  // Gate state per ADC lane (for diagnostics)

static void pipeline_to_float() {
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        if (!_laneL[i] || !_laneR[i]) continue;
        to_float(_rawBuf[i], _laneL[i], _laneR[i], FRAMES);

        // Noise gate: ADC lanes only (siggen/USB are always clean)
        if (i == AUDIO_INPUT_ADC1 || i == AUDIO_INPUT_ADC2) {
            float sumSq = 0.0f;
            for (int f = 0; f < FRAMES; f++) {
                sumSq += _laneL[i][f] * _laneL[i][f] + _laneR[i][f] * _laneR[i][f];
            }
            // Hysteresis: open at -65 dBFS, stay open until -70 dBFS
            bool open = _gateOpen[i]
                ? (sumSq >= GATE_CLOSE_THRESH)
                : (sumSq >= GATE_OPEN_THRESH);

            if (open) {
                _gateOpen[i] = true;
                _gateFadeCount[i] = 2;  // Pre-arm: 2 fade buffers ready for next close
                // Save last clean frame into PSRAM for fade-out
                if (_gatePrevL[i]) memcpy(_gatePrevL[i], _laneL[i], FRAMES * sizeof(float));
                if (_gatePrevR[i]) memcpy(_gatePrevR[i], _laneR[i], FRAMES * sizeof(float));
                // Pass through: _laneL[i] / _laneR[i] unchanged
            } else {
                _gateOpen[i] = false;
                if (_gateFadeCount[i] > 0 && _gatePrevL[i] && _gatePrevR[i]) {
                    // Fade: count=2 → gain=1.0 (hold last frame), count=1 → gain=0.5
                    float gain = (float)_gateFadeCount[i] / 2.0f;
                    for (int f = 0; f < FRAMES; f++) {
                        _laneL[i][f] = _gatePrevL[i][f] * gain;
                        _laneR[i][f] = _gatePrevR[i][f] * gain;
                    }
                    _gateFadeCount[i]--;
                } else {
                    // Fully gated: write silence
                    memset(_laneL[i], 0, FRAMES * sizeof(float));
                    memset(_laneR[i], 0, FRAMES * sizeof(float));
                }
            }
        }
    }
}

static void pipeline_run_dsp() {
#ifdef DSP_ENABLED
    // Temporary float→RJ-int32→dsp_process_buffer→RJ-int32→float bridge.
    // dsp_process_buffer() uses 8388607.0f (right-justified 24-bit) internally.
    // Lanes 0 & 1 (ADC1/ADC2); lanes 2 & 3 bypass DSP by default.
    // Replace with float-native DSP in a future stage.
    for (int lane = 0; lane < 2; lane++) {
        if (_dspBypass[lane] || !_laneL[lane] || !_laneR[lane]) continue;

        // float [-1,+1] → right-justified int32 [-8388607, +8388607]
        for (int f = 0; f < FRAMES; f++) {
            _dspBridgeBuf[f * 2]     = (int32_t)(_laneL[lane][f] * MAX_24BIT_F);
            _dspBridgeBuf[f * 2 + 1] = (int32_t)(_laneR[lane][f] * MAX_24BIT_F);
        }

        // DSP stages: EQ, crossover, compressor, limiter, etc. (modifies in-place)
        dsp_process_buffer(_dspBridgeBuf, FRAMES, lane);

        // right-justified int32 → float [-1,+1]
        for (int f = 0; f < FRAMES; f++) {
            _laneL[lane][f] = (float)_dspBridgeBuf[f * 2]     / MAX_24BIT_F;
            _laneR[lane][f] = (float)_dspBridgeBuf[f * 2 + 1] / MAX_24BIT_F;
        }
    }
#else
    (void)_dspBypass;
#endif
}

static void pipeline_mix_matrix() {
    if (!_outL || !_outR) return;

    if (_matrixBypass) {
        // Identity passthrough: ADC1 L/R → output L/R
        if (_laneL[0] && _laneR[0]) {
            memcpy(_outL, _laneL[0], FRAMES * sizeof(float));
            memcpy(_outR, _laneR[0], FRAMES * sizeof(float));
        }
        return;
    }

#ifdef DSP_ENABLED
    // Full 8×8 matrix: 8 input channels from 4 stereo lanes → DAC L/R (out ch 0 & 1).
    // Remaining output channels (2-7) reserved for future multi-DAC routing.
    // Uses SIMD dsps_mulc_f32 + dsps_add_f32 (same pattern as dsp_routing_apply).
    const float *inCh[AUDIO_PIPELINE_MATRIX_SIZE] = {
        _laneL[0], _laneR[0],
        _laneL[1], _laneR[1],
        _laneL[2], _laneR[2],
        _laneL[3], _laneR[3]
    };
    float *outCh[2] = { _outL, _outR };

    // Lazy-allocate PSRAM scratch buffer for scaled copy
    static float *_matrixTemp = nullptr;
    if (!_matrixTemp) {
        _matrixTemp = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!_matrixTemp) _matrixTemp = (float *)calloc(FRAMES, sizeof(float));
    }
    if (!_matrixTemp) return;

    for (int o = 0; o < 2; o++) {
        memset(outCh[o], 0, FRAMES * sizeof(float));
        for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
            float gain = _matrixGain[o][i];
            if (gain == 0.0f || !inCh[i]) continue;
            dsps_mulc_f32(inCh[i], _matrixTemp, FRAMES, gain, 1, 1);
            dsps_add_f32(outCh[o], _matrixTemp, outCh[o], FRAMES, 1, 1, 1);
        }
    }

    // Update DSP swap hold buffer with last good output (skip during pending swap)
    if (!_swapPending && _swapHoldL && _swapHoldR) {
        memcpy(_swapHoldL, _outL, FRAMES * sizeof(float));
        memcpy(_swapHoldR, _outR, FRAMES * sizeof(float));
    }
#else
    // No ESP-DSP available (native without lib): fall back to identity
    if (_laneL[0] && _laneR[0]) {
        memcpy(_outL, _laneL[0], FRAMES * sizeof(float));
        memcpy(_outR, _laneR[0], FRAMES * sizeof(float));
    }
    if (!_swapPending && _swapHoldL && _swapHoldR) {
        memcpy(_swapHoldL, _outL, FRAMES * sizeof(float));
        memcpy(_swapHoldR, _outR, FRAMES * sizeof(float));
    }
#endif
}

static void pipeline_write_output() {
    if (_outputBypass || !_outL || !_outR) return;
#ifdef DAC_ENABLED
    // During a DSP config swap, use the last good output frame (held in PSRAM)
    // instead of the swap-gap buffer (which may contain un-DSP'd or zeroed data).
    const float *outL = (_swapPending && _swapHoldL) ? _swapHoldL : _outL;
    const float *outR = (_swapPending && _swapHoldR) ? _swapHoldR : _outR;
    to_int32_lj(outL, outR, _dacBuf, FRAMES);
    dac_output_write(_dacBuf, FRAMES);
    _swapPending = false;  // Clear after one iteration
#endif
}

// ===== Metering State (persistent across pipeline iterations) =====
static struct {
    float vu1 = 0, vu2 = 0, vuCombined = 0;
    float peak1 = 0, peak2 = 0, peakCombined = 0;
    unsigned long peakHold1 = 0, peakHold2 = 0, peakHoldComb = 0;
} _meterState;

// Stage 5: full RMS/VU/peak metering for ADC1 per pipeline buffer (~5.33ms).
// This is the ONLY write path from the pipeline to the sensing layer.
// Waveform/FFT accumulation remains in i2s_audio.cpp (ring-buffer state).
static void pipeline_update_metering() {
    if (!_laneL[0] || !_laneR[0]) return;

    float sumSqL = 0.0f, sumSqR = 0.0f;
    for (int f = 0; f < FRAMES; f++) {
        sumSqL += _laneL[0][f] * _laneL[0][f];
        sumSqR += _laneR[0][f] * _laneR[0][f];
    }
    float rms1 = sqrtf(sumSqL / FRAMES);
    float rms2 = sqrtf(sumSqR / FRAMES);
    float rmsCombined = sqrtf((sumSqL + sumSqR) / (FRAMES * 2));
    float dbfs = (rmsCombined > 1e-9f) ? 20.0f * log10f(rmsCombined) : -96.0f;

    float dt_ms = (float)FRAMES * 1000.0f / (float)AppState::getInstance().audioSampleRate;
    _meterState.vu1        = audio_vu_update(_meterState.vu1, rms1, dt_ms);
    _meterState.vu2        = audio_vu_update(_meterState.vu2, rms2, dt_ms);
    _meterState.vuCombined = audio_vu_update(_meterState.vuCombined, rmsCombined, dt_ms);

#ifndef NATIVE_TEST
    unsigned long now = millis();
#else
    unsigned long now = 0;
#endif
    _meterState.peak1 = audio_peak_hold_update(
        _meterState.peak1, rms1, &_meterState.peakHold1, now, dt_ms);
    _meterState.peak2 = audio_peak_hold_update(
        _meterState.peak2, rms2, &_meterState.peakHold2, now, dt_ms);
    _meterState.peakCombined = audio_peak_hold_update(
        _meterState.peakCombined, rmsCombined, &_meterState.peakHoldComb, now, dt_ms);

    AdcAnalysis adc0 = {};
    adc0.rms1        = rms1;
    adc0.rms2        = rms2;
    adc0.rmsCombined = rmsCombined;
    adc0.vu1         = _meterState.vu1;
    adc0.vu2         = _meterState.vu2;
    adc0.vuCombined  = _meterState.vuCombined;
    adc0.peak1       = _meterState.peak1;
    adc0.peak2       = _meterState.peak2;
    adc0.peakCombined = _meterState.peakCombined;
    adc0.dBFS        = dbfs;
    i2s_audio_update_analysis_metering(adc0);

    // USB lane metering (same VU ballistics as ADC1)
#ifdef USB_AUDIO_ENABLED
    if (!_inputBypass[3] && _sources[3].isActive && _sources[3].isActive()
        && _laneL[3] && _laneR[3]) {
        float usbSumSqL = 0.0f, usbSumSqR = 0.0f;
        for (int f = 0; f < FRAMES; f++) {
            usbSumSqL += _laneL[3][f] * _laneL[3][f];
            usbSumSqR += _laneR[3][f] * _laneR[3][f];
        }
        float usbRmsL = sqrtf(usbSumSqL / FRAMES);
        float usbRmsR = sqrtf(usbSumSqR / FRAMES);
        float usbDt = (float)FRAMES * 1000.0f / (float)AppState::getInstance().audioSampleRate;
        _sources[3]._vuSmoothedL = audio_vu_update(_sources[3]._vuSmoothedL, usbRmsL, usbDt);
        _sources[3]._vuSmoothedR = audio_vu_update(_sources[3]._vuSmoothedR, usbRmsR, usbDt);
        _sources[3].vuL = (_sources[3]._vuSmoothedL > 1e-9f)
            ? 20.0f * log10f(_sources[3]._vuSmoothedL) : -90.0f;
        _sources[3].vuR = (_sources[3]._vuSmoothedR > 1e-9f)
            ? 20.0f * log10f(_sources[3]._vuSmoothedR) : -90.0f;
        AppState &asUsb = AppState::getInstance();
        asUsb.usbAudioVuL = _sources[3].vuL;
        asUsb.usbAudioVuR = _sources[3].vuR;
        asUsb.markUsbAudioVuDirty();
    }
#endif
}

// ===== FreeRTOS Task =====
#ifndef NATIVE_TEST
// ~5000ms / (DMA_BUF_LEN * 1000 / sample_rate + 2ms yield) ≈ every 2500 iterations at 48kHz/512
static const uint32_t DUMP_INTERVAL_LOOPS = 2500;

static void audio_pipeline_task_fn(void * /*param*/) {
    // Create I2S channels here (Core 1) so the DMA ISR is pinned to Core 1,
    // isolated from WiFi TX/RX interrupts on Core 0 that cause audio pops.
    i2s_audio_init_channels();

    esp_task_wdt_add(NULL);
    uint32_t loopCount = 0;
    while (true) {
        esp_task_wdt_reset();

        if (AppState::getInstance().audioPaused) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        pipeline_sync_flags();
        pipeline_read_inputs();
        pipeline_to_float();
        pipeline_run_dsp();
        pipeline_mix_matrix();
        pipeline_write_output();
        pipeline_update_metering();
        // Feed raw ADC1 data into waveform/FFT accumulator for WebSocket graph display.
        // Uses pre-float int32 data; adcIndex 0 = ADC1.
        i2s_audio_push_waveform_fft(_rawBuf[0], FRAMES, 0);

        // Schedule periodic serial dump every ~5s (via main loop dirty-flag pattern)
        if (++loopCount >= DUMP_INTERVAL_LOOPS) {
            loopCount = 0;
            // Capture raw ADC1 diagnostic snapshot before requesting dump
            for (int i = 0; i < 8; i++) _adcDiag.raw[i] = _rawBuf[0][i];
            int32_t maxAbs = 0, minVal = INT32_MAX, maxVal = INT32_MIN;
            uint32_t nonZero = 0;
            for (int f = 0; f < RAW_SAMPLES; f++) {
                int32_t v = _rawBuf[0][f];
                if (v != 0) nonZero++;
                int32_t a = (v < 0) ? -v : v;
                if (a > maxAbs) maxAbs = a;
                if (v < minVal) minVal = v;
                if (v > maxVal) maxVal = v;
            }
            _adcDiag.maxAbs   = maxAbs;
            _adcDiag.minVal   = minVal;
            _adcDiag.maxVal   = maxVal;
            _adcDiag.nonZero  = nonZero;
            _adcDiag.captured = true;
            i2s_audio_request_dump();
        }

        // Yield 2 ticks so IDLE0 can feed the Task Watchdog Timer.
        // DMA has 8 buffers = ~42ms runway; 2ms yield is safe.
        vTaskDelay(2);
    }
}
#endif

// ===== Matrix Init =====
// Channel map: lane0(ADC1)=ch0/1, lane1(ADC2)=ch2/3, lane2(siggen)=ch4/5, lane3(USB)=ch6/7
// ADC1 L/R → DAC L/R, Siggen L/R → DAC L/R (additive — siggen only audible when enabled from UI)
static void init_matrix_siggen_direct() {
    memset(_matrixGain, 0, sizeof(_matrixGain));
    // ADC1 L (in_ch 0) → DAC L (out_ch 0)
    _matrixGain[0][0] = 1.0f;
    // ADC1 R (in_ch 1) → DAC R (out_ch 1)
    _matrixGain[1][1] = 1.0f;
    // Siggen L (in_ch 4) → DAC L (out_ch 0), additive
    _matrixGain[0][4] = 1.0f;
    // Siggen R (in_ch 5) → DAC R (out_ch 1), additive
    _matrixGain[1][5] = 1.0f;
}

// ===== Public API =====

void audio_pipeline_init() {
#ifdef DSP_ENABLED
    dsp_init();
#endif
#ifdef DAC_ENABLED
    dac_output_init();
#endif

    // Allocate float working buffers
#ifdef NATIVE_TEST
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _laneL[i] = _laneL_buf[i];
        _laneR[i] = _laneR_buf[i];
    }
    _outL = _outL_buf;
    _outR = _outR_buf;
    for (int i = 0; i < 2; i++) {
        _gatePrevL[i] = _gatePrevL_buf[i];
        _gatePrevR[i] = _gatePrevR_buf[i];
    }
    _swapHoldL = _swapHoldL_buf;
    _swapHoldR = _swapHoldR_buf;
#else
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _laneL[i] = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        _laneR[i] = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!_laneL[i]) _laneL[i] = (float *)calloc(FRAMES, sizeof(float));
        if (!_laneR[i]) _laneR[i] = (float *)calloc(FRAMES, sizeof(float));
    }
    _outL = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
    _outR = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
    if (!_outL) _outL = (float *)calloc(FRAMES, sizeof(float));
    if (!_outR) _outR = (float *)calloc(FRAMES, sizeof(float));
    // Noise gate fade-out: PSRAM prev-frame buffers (~4 KB total)
    for (int i = 0; i < 2; i++) {
        _gatePrevL[i] = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        _gatePrevR[i] = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!_gatePrevL[i]) _gatePrevL[i] = (float *)calloc(FRAMES, sizeof(float));
        if (!_gatePrevR[i]) _gatePrevR[i] = (float *)calloc(FRAMES, sizeof(float));
    }
    // DSP swap hold: PSRAM last-good-output buffer (~2 KB total)
    _swapHoldL = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
    _swapHoldR = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
    if (!_swapHoldL) _swapHoldL = (float *)calloc(FRAMES, sizeof(float));
    if (!_swapHoldR) _swapHoldR = (float *)calloc(FRAMES, sizeof(float));
#endif

    init_matrix_siggen_direct();

#ifdef USB_AUDIO_ENABLED
    // Register USB as pipeline lane 3 input source
    AudioInputSource usbSrc = AUDIO_INPUT_SOURCE_INIT;
    usbSrc.name = "USB";
    usbSrc.lane = AUDIO_SRC_LANE_USB;
    usbSrc.read = _usb_src_read;
    usbSrc.isActive = _usb_src_isActive;
    usbSrc.getSampleRate = _usb_src_getSampleRate;
    audio_pipeline_register_source(AUDIO_SRC_LANE_USB, &usbSrc);

    // Default routing: USB L (in_ch 6) → DAC L (out_ch 0), USB R (in_ch 7) → DAC R (out_ch 1)
    _matrixGain[0][6] = 1.0f;
    _matrixGain[1][7] = 1.0f;
#endif

    // Sync bypass flags from AppState
    AppState &s = AppState::getInstance();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _inputBypass[i] = s.pipelineInputBypass[i];
        _dspBypass[i]   = s.pipelineDspBypass[i];
    }
    _matrixBypass = s.pipelineMatrixBypass;
    _outputBypass = s.pipelineOutputBypass;

#ifndef NATIVE_TEST
    xTaskCreatePinnedToCore(
        audio_pipeline_task_fn,
        "audio_cap",
        TASK_STACK_SIZE_AUDIO,
        NULL,
        TASK_PRIORITY_AUDIO,
        &_pipelineTaskHandle,
        TASK_CORE_AUDIO  // Core 1 — isolates audio from WiFi system tasks on Core 0
    );
#endif

    LOG_I("[Audio] Pipeline initialized — ADC1+Siggen through matrix, ADC2/USB bypassed");
}

void audio_pipeline_bypass_input(int lane, bool bypass) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return;
    _inputBypass[lane] = bypass;
    AppState::getInstance().pipelineInputBypass[lane] = bypass;
}

void audio_pipeline_bypass_dsp(int lane, bool bypass) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return;
    _dspBypass[lane] = bypass;
    AppState::getInstance().pipelineDspBypass[lane] = bypass;
}

void audio_pipeline_bypass_matrix(bool bypass) {
    _matrixBypass = bypass;
    AppState::getInstance().pipelineMatrixBypass = bypass;
}

void audio_pipeline_bypass_output(bool bypass) {
    _outputBypass = bypass;
    AppState::getInstance().pipelineOutputBypass = bypass;
}

void audio_pipeline_set_matrix_gain(int out_ch, int in_ch, float gain_linear) {
    if (out_ch < 0 || out_ch >= AUDIO_PIPELINE_MATRIX_SIZE) return;
    if (in_ch  < 0 || in_ch  >= AUDIO_PIPELINE_MATRIX_SIZE) return;
    _matrixGain[out_ch][in_ch] = gain_linear;
}

void audio_pipeline_set_matrix_gain_db(int out_ch, int in_ch, float gain_db) {
    float linear = (gain_db <= -96.0f) ? 0.0f : powf(10.0f, gain_db / 20.0f);
    audio_pipeline_set_matrix_gain(out_ch, in_ch, linear);
}

float audio_pipeline_get_matrix_gain(int out_ch, int in_ch) {
    if (out_ch < 0 || out_ch >= AUDIO_PIPELINE_MATRIX_SIZE) return 0.0f;
    if (in_ch  < 0 || in_ch  >= AUDIO_PIPELINE_MATRIX_SIZE) return 0.0f;
    return _matrixGain[out_ch][in_ch];
}

bool audio_pipeline_is_matrix_bypass() {
    return _matrixBypass;
}

void audio_pipeline_notify_dsp_swap() {
    // Called from dsp_swap_config() (Core 0) before _swapRequested is set.
    // Signals pipeline_write_output() (Core 1) to use the PSRAM hold buffer for
    // one iteration, bridging the DSP-skipped buffer gap with the last good frame.
    _swapPending = true;
}

void audio_pipeline_register_source(int lane, const AudioInputSource *src) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS || !src) return;
    _sources[lane] = *src;  // Value copy
}

float audio_pipeline_get_lane_vu_l(int lane) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return -90.0f;
    return _sources[lane].vuL;
}

float audio_pipeline_get_lane_vu_r(int lane) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return -90.0f;
    return _sources[lane].vuR;
}

void audio_pipeline_dump_raw_diag() {
    if (!_adcDiag.captured) return;

    // --- What to look for ---
    // Left-justified (CORRECT): data in bits 31..8, bottom 8 bits = 0
    //   raw[0] pattern: 0xXXXXXX00  (e.g. 0x00034500 = noise floor ~+843)
    //   raw[0] >> 8  = ±small value — confirms to_float() is reading right bits
    // Right-justified (WRONG): data in bits 23..0, top 8 bits = 0
    //   raw[0] pattern: 0x00XXXXXX  (e.g. 0x00000345 = same noise, different position)
    //   raw[0] >> 8  = near 0 — to_float() discards all useful bits (sounds silent/corrupt)
    // DC offset: all values biased, minVal and maxVal both far from 0
    // No signal: maxAbs very small (<= 0x00010000) AND nonZero count low

    LOG_I("[Audio] === ADC1 Raw Diagnostic ===");
    LOG_I("[Audio]   raw[0..7] hex: %08lX %08lX %08lX %08lX  %08lX %08lX %08lX %08lX",
          (unsigned long)_adcDiag.raw[0], (unsigned long)_adcDiag.raw[1],
          (unsigned long)_adcDiag.raw[2], (unsigned long)_adcDiag.raw[3],
          (unsigned long)_adcDiag.raw[4], (unsigned long)_adcDiag.raw[5],
          (unsigned long)_adcDiag.raw[6], (unsigned long)_adcDiag.raw[7]);
    LOG_I("[Audio]   decoded >>8: %d %d %d %d %d %d %d %d",
          (int)(_adcDiag.raw[0] >> 8), (int)(_adcDiag.raw[1] >> 8),
          (int)(_adcDiag.raw[2] >> 8), (int)(_adcDiag.raw[3] >> 8),
          (int)(_adcDiag.raw[4] >> 8), (int)(_adcDiag.raw[5] >> 8),
          (int)(_adcDiag.raw[6] >> 8), (int)(_adcDiag.raw[7] >> 8));
    LOG_I("[Audio]   buf min=%08lX max=%08lX maxAbs=%08lX nonZero=%lu/%d",
          (unsigned long)_adcDiag.minVal, (unsigned long)_adcDiag.maxVal,
          (unsigned long)_adcDiag.maxAbs, (unsigned long)_adcDiag.nonZero, RAW_SAMPLES);
    // Bottom 8 bits of raw[0]: if all samples have non-zero low byte → NOT left-justified
    uint8_t lowByte = (uint8_t)(_adcDiag.raw[0] & 0xFF);
    LOG_I("[Audio]   raw[0] low byte=0x%02X  (0x00 → left-justified OK; non-zero → format mismatch)",
          (unsigned int)lowByte);
    LOG_I("[Audio]   gate ADC1=%s ADC2=%s  (OPEN=audio present, CLOSED=noise floor only)",
          _gateOpen[0] ? "OPEN" : "CLOSED",
          _gateOpen[1] ? "OPEN" : "CLOSED");
}
