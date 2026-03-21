#include "audio_pipeline.h"
#include "i2s_audio.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#ifdef DSP_ENABLED
#include "dsp_pipeline.h"
#include "output_dsp.h"
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
#ifndef NATIVE_TEST
#include <LittleFS.h>
#include <ArduinoJson.h>
#endif
#include "audio_input_source.h"
#include "audio_output_sink.h"

// ===== Compile-time dimension invariants =====
static_assert(AUDIO_PIPELINE_MAX_INPUTS * 2 <= AUDIO_PIPELINE_MATRIX_SIZE,
    "Matrix columns must accommodate all stereo input channels");
static_assert(AUDIO_OUT_MAX_SINKS * 2 <= AUDIO_PIPELINE_MATRIX_SIZE,
    "Matrix rows must accommodate all stereo output channels");

// ===== Constants =====
static const int FRAMES      = I2S_DMA_BUF_LEN;    // 256 stereo frames per DMA buffer
static const int RAW_SAMPLES = FRAMES * 2;          // 512 int32_t per buffer (L+R interleaved)
static const float MAX_24BIT_F = 8388607.0f;        // 2^23 - 1

// ===== DMA Buffers — MUST be in internal SRAM (DMA cannot access PSRAM) =====
// Lazily allocated on first audio_pipeline_set_source() / audio_pipeline_set_sink() call
// (ESP32 path only — native test keeps static 2D arrays below).
#ifndef NATIVE_TEST
static int32_t *_rawBuf[AUDIO_PIPELINE_MAX_INPUTS] = {};
static int32_t *_sinkBuf[AUDIO_OUT_MAX_SINKS] = {};
#else
static int32_t _rawBuf_storage[AUDIO_PIPELINE_MAX_INPUTS][RAW_SAMPLES];
static int32_t _sinkBuf_storage[AUDIO_OUT_MAX_SINKS][RAW_SAMPLES];
static int32_t *_rawBuf[AUDIO_PIPELINE_MAX_INPUTS];
static int32_t *_sinkBuf[AUDIO_OUT_MAX_SINKS];
#endif

// ===== Float Working Buffers (PSRAM-allocated on ESP32, static on native) =====
static float *_laneL[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float *_laneR[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float *_outCh[AUDIO_PIPELINE_MATRIX_SIZE] = {};

// Noise gate fade-out: last open-gate frame per ADC lane (PSRAM, 4 KB total)
static float *_gatePrevL[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float *_gatePrevR[AUDIO_PIPELINE_MAX_INPUTS] = {};

// DSP swap hold: last good pipeline output frame (PSRAM, 2 KB total)
static float *_swapHoldCh[AUDIO_PIPELINE_MATRIX_SIZE] = {};

#ifdef NATIVE_TEST
static float _laneL_buf[AUDIO_PIPELINE_MAX_INPUTS][I2S_DMA_BUF_LEN];
static float _laneR_buf[AUDIO_PIPELINE_MAX_INPUTS][I2S_DMA_BUF_LEN];
static float _outCh_buf[AUDIO_PIPELINE_MATRIX_SIZE][I2S_DMA_BUF_LEN];
static float _gatePrevL_buf[AUDIO_PIPELINE_MAX_INPUTS][I2S_DMA_BUF_LEN];
static float _gatePrevR_buf[AUDIO_PIPELINE_MAX_INPUTS][I2S_DMA_BUF_LEN];
static float _swapHoldCh_buf[AUDIO_PIPELINE_MATRIX_SIZE][I2S_DMA_BUF_LEN];
#endif

// ===== Routing Matrix =====
// gain[out_ch][in_ch]: 8 output channels × 8 input channels, linear gain
static float _matrixGain[AUDIO_PIPELINE_MATRIX_SIZE][AUDIO_PIPELINE_MATRIX_SIZE] = {};

// ===== Runtime Bypass Flags =====
static bool _inputBypass[AUDIO_PIPELINE_MAX_INPUTS] = {};
static bool _dspBypass[AUDIO_PIPELINE_MAX_INPUTS]   = {false, false, true, true, false, false, false, false};
static bool _matrixBypass = false;   // Matrix active: routes input lanes to output channels via gain matrix
static bool _outputBypass = false;

// ===== Registered Input Sources =====
static AudioInputSource _sources[AUDIO_PIPELINE_MAX_INPUTS] = {
    AUDIO_INPUT_SOURCE_INIT, AUDIO_INPUT_SOURCE_INIT,
    AUDIO_INPUT_SOURCE_INIT, AUDIO_INPUT_SOURCE_INIT,
    AUDIO_INPUT_SOURCE_INIT, AUDIO_INPUT_SOURCE_INIT,
    AUDIO_INPUT_SOURCE_INIT, AUDIO_INPUT_SOURCE_INIT,
};

// ===== Registered Output Sinks =====
static AudioOutputSink _sinks[AUDIO_OUT_MAX_SINKS] = {
    AUDIO_OUTPUT_SINK_INIT, AUDIO_OUTPUT_SINK_INIT,
    AUDIO_OUTPUT_SINK_INIT, AUDIO_OUTPUT_SINK_INIT,
    AUDIO_OUTPUT_SINK_INIT, AUDIO_OUTPUT_SINK_INIT,
    AUDIO_OUTPUT_SINK_INIT, AUDIO_OUTPUT_SINK_INIT,
};
static volatile int _sinkCount = 0;

// ===== No-sink warning flag (reset when first sink is registered) =====
static bool _noSinkWarned = false;

// ===== Noise Gate Fade State =====
// _gateFadeCount: counts remaining fade-out buffers (2→1→0) after gate closes.
// Pre-armed to 2 while gate is open so first-close buffer fades rather than cuts.
static int _gateFadeCount[AUDIO_PIPELINE_MAX_INPUTS] = {};

// ===== DSP Swap Hold State =====
// Set by audio_pipeline_notify_dsp_swap() (called from Core 0 before dsp_swap_config
// sets _swapRequested). Causes pipeline_write_output() to use _swapHoldCh[]
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
    // All lanes: adcEnabled (set by HAL bridge) AND internal bypass flag
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _inputBypass[i] = !s.audio.adcEnabled[i] || s.pipelineInputBypass[i];
        _dspBypass[i] = s.pipelineDspBypass[i];
    }
    _matrixBypass = s.pipelineMatrixBypass;
    _outputBypass = s.pipelineOutputBypass;
}

static void pipeline_read_inputs() {
    const size_t bufBytes = FRAMES * 2 * sizeof(int32_t);

    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        if (!_rawBuf[lane]) continue;  // Not yet allocated (no source registered)
        if (_inputBypass[lane]) {
            memset(_rawBuf[lane], 0, bufBytes);
            continue;
        }

        // --- Registered source (ADC, USB, SigGen, or any HAL-managed input) ---
        if (_sources[lane].read) {
            bool active = !_sources[lane].isActive || _sources[lane].isActive();
            if (active) {
                uint32_t got = _sources[lane].read(_rawBuf[lane], FRAMES);
                if (got < (uint32_t)FRAMES) {
                    memset(&_rawBuf[lane][got * 2], 0, (FRAMES - got) * 2 * sizeof(int32_t));
                }
                // Apply pre-matrix gain (host volume for USB, input trim for ADC)
                if (_sources[lane].gainLinear != 1.0f) {
                    float g = _sources[lane].gainLinear;
                    for (int s = 0; s < FRAMES * 2; s++) {
                        _rawBuf[lane][s] = (int32_t)((float)_rawBuf[lane][s] * g);
                    }
                }
            } else {
                memset(_rawBuf[lane], 0, bufBytes);
            }
        }
        // --- No source registered for this lane ---
        else {
            memset(_rawBuf[lane], 0, bufBytes);
        }
    }
}

// Noise gate for ADC lanes: prevents PCM1808 noise floor from reaching the DAC.
// Hysteresis thresholds (5 dB window) prevent rapid toggling near threshold.
// Fade-out over 2 buffers (~10.7 ms) using PSRAM prev-frame copy prevents click.
//   OPEN  threshold -65 dBFS: 10^(-65/20) = 5.62e-4 → sq = 3.16e-7 × 512 ≈ 1.62e-4
//   CLOSE threshold -70 dBFS: 10^(-70/20) = 3.16e-4 → sq = 1.00e-7 × 512 ≈ 5.12e-5
static const float GATE_OPEN_THRESH  = 1.62e-4f;  // -65 dBFS
static const float GATE_CLOSE_THRESH = 5.12e-5f;  // -70 dBFS (5 dB hysteresis window)

static bool _gateOpen[AUDIO_PIPELINE_MAX_INPUTS] = {};  // Gate state per ADC lane (for diagnostics)

static void pipeline_to_float() {
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        if (!_rawBuf[i] || !_laneL[i] || !_laneR[i]) continue;
        to_float(_rawBuf[i], _laneL[i], _laneR[i], FRAMES);

        // Noise gate: hardware ADC lanes only (siggen/USB are always clean)
        if (_sources[i].isHardwareAdc) {
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
    // Float-native DSP — no int32 bridge needed (saves ~2KB + 4 conversion loops)
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        if (_dspBypass[lane] || !_laneL[lane] || !_laneR[lane]) continue;
        dsp_process_buffer_float(_laneL[lane], _laneR[lane], FRAMES, lane);
    }
#else
    (void)_dspBypass;
#endif
}

static void pipeline_mix_matrix() {
    // Need at least ch0+ch1 allocated
    if (!_outCh[0] || !_outCh[1]) return;

    if (_matrixBypass) {
        // Identity passthrough: ADC1 L/R → output ch 0/1, rest zeroed
        if (_laneL[0] && _laneR[0]) {
            memcpy(_outCh[0], _laneL[0], FRAMES * sizeof(float));
            memcpy(_outCh[1], _laneR[0], FRAMES * sizeof(float));
        }
        for (int o = 2; o < AUDIO_PIPELINE_MATRIX_SIZE; o++) {
            if (_outCh[o]) memset(_outCh[o], 0, FRAMES * sizeof(float));
        }
        return;
    }

#ifdef DSP_ENABLED
    // Full matrix: all input lanes → output channels (loop-based, bounds-safe)
    const float *inCh[AUDIO_PIPELINE_MATRIX_SIZE] = {};
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS && lane * 2 + 1 < AUDIO_PIPELINE_MATRIX_SIZE; lane++) {
        inCh[lane * 2]     = _laneL[lane];
        inCh[lane * 2 + 1] = _laneR[lane];
    }

    // Lazy-allocate PSRAM scratch buffer for scaled copy
    static float *_matrixTemp = nullptr;
    if (!_matrixTemp) {
        _matrixTemp = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!_matrixTemp) _matrixTemp = (float *)calloc(FRAMES, sizeof(float));
    }
    if (!_matrixTemp) return;

    for (int o = 0; o < AUDIO_PIPELINE_MATRIX_SIZE; o++) {
        if (!_outCh[o]) continue;
        memset(_outCh[o], 0, FRAMES * sizeof(float));
        for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
            float gain = _matrixGain[o][i];
            if (gain == 0.0f || !inCh[i]) continue;
            dsps_mulc_f32(inCh[i], _matrixTemp, FRAMES, gain, 1, 1);
            dsps_add_f32(_outCh[o], _matrixTemp, _outCh[o], FRAMES, 1, 1, 1);
        }
    }

    // Update DSP swap hold buffer with last good output (skip during pending swap)
    if (!_swapPending) {
        for (int o = 0; o < AUDIO_PIPELINE_MATRIX_SIZE; o++) {
            if (_swapHoldCh[o] && _outCh[o]) {
                memcpy(_swapHoldCh[o], _outCh[o], FRAMES * sizeof(float));
            }
        }
    }
#else
    // No ESP-DSP available (native without lib): fall back to identity
    if (_laneL[0] && _laneR[0]) {
        memcpy(_outCh[0], _laneL[0], FRAMES * sizeof(float));
        memcpy(_outCh[1], _laneR[0], FRAMES * sizeof(float));
    }
    for (int o = 2; o < AUDIO_PIPELINE_MATRIX_SIZE; o++) {
        if (_outCh[o]) memset(_outCh[o], 0, FRAMES * sizeof(float));
    }
    if (!_swapPending) {
        for (int o = 0; o < AUDIO_PIPELINE_MATRIX_SIZE; o++) {
            if (_swapHoldCh[o] && _outCh[o]) {
                memcpy(_swapHoldCh[o], _outCh[o], FRAMES * sizeof(float));
            }
        }
    }
#endif
}

static void pipeline_run_output_dsp() {
#ifdef DSP_ENABLED
    for (int ch = 0; ch < AUDIO_PIPELINE_MATRIX_SIZE; ch++) {
        if (!_outCh[ch]) continue;
        output_dsp_process(ch, _outCh[ch], FRAMES);
    }
#endif
}

static void pipeline_write_output() {
    if (_outputBypass || !_outCh[0] || !_outCh[1]) return;
#ifdef DAC_ENABLED
    if (_sinkCount > 0) {
        // Sink dispatch path: iterate all slots up to AUDIO_OUT_MAX_SINKS so that
        // slot-indexed sinks with gaps between them (e.g., slot 0 empty, slot 1 active)
        // are still dispatched. Empty slots are skipped by the write/isReady checks below.
        for (int s = 0; s < AUDIO_OUT_MAX_SINKS; s++) {
            AudioOutputSink *sink = &_sinks[s];
            if (!sink->write || !sink->isReady || !sink->isReady()) continue;
            if (sink->muted) continue;

            int chL = sink->firstChannel;
            int chR = (sink->channelCount >= 2) ? chL + 1 : chL;
            if (chL >= AUDIO_PIPELINE_MATRIX_SIZE) continue;
            if (chR >= AUDIO_PIPELINE_MATRIX_SIZE) chR = chL;

            const float *srcL = (_swapPending && _swapHoldCh[chL]) ? _swapHoldCh[chL] : _outCh[chL];
            const float *srcR = (_swapPending && _swapHoldCh[chR]) ? _swapHoldCh[chR] : _outCh[chR];
            if (!srcL || !srcR) continue;
            if (!_sinkBuf[s]) continue;  // DMA buffer not yet allocated for this slot

            if (sink->gainLinear != 1.0f) {
                float g = sink->gainLinear;
                for (int f = 0; f < FRAMES; f++) {
                    float l = clampf(srcL[f] * g);
                    float r = clampf(srcR[f] * g);
                    _sinkBuf[s][f * 2]     = (int32_t)(l * MAX_24BIT_F) << 8;
                    _sinkBuf[s][f * 2 + 1] = (int32_t)(r * MAX_24BIT_F) << 8;
                }
            } else {
                to_int32_lj(srcL, srcR, _sinkBuf[s], FRAMES);
            }
            sink->write(_sinkBuf[s], FRAMES);

            // Compute output sink VU metering
            {
                float sinkSumSqL = 0, sinkSumSqR = 0;
                float sg = sink->gainLinear;
                for (int f = 0; f < FRAMES; f++) {
                    float l = srcL[f] * sg;
                    float r = srcR[f] * sg;
                    sinkSumSqL += l * l;
                    sinkSumSqR += r * r;
                }
                float sinkRmsL = sqrtf(sinkSumSqL / FRAMES);
                float sinkRmsR = sqrtf(sinkSumSqR / FRAMES);
                float sinkDt = (float)FRAMES * 1000.0f / (float)AppState::getInstance().audio.sampleRate;
                sink->_vuSmoothedL = audio_vu_update(sink->_vuSmoothedL, sinkRmsL, sinkDt);
                sink->_vuSmoothedR = audio_vu_update(sink->_vuSmoothedR, sinkRmsR, sinkDt);
                sink->vuL = (sink->_vuSmoothedL > 1e-9f) ? 20.0f * log10f(sink->_vuSmoothedL) : -90.0f;
                sink->vuR = (sink->_vuSmoothedR > 1e-9f) ? 20.0f * log10f(sink->_vuSmoothedR) : -90.0f;
            }
        }
    } else {
        // No sinks registered — output silence
        if (!_noSinkWarned) {
            LOG_W("[Pipeline] No sinks registered, output silent");
            _noSinkWarned = true;
        }
    }

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

    float dt_ms = (float)FRAMES * 1000.0f / (float)AppState::getInstance().audio.sampleRate;
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

    // Per-source VU metering for all active non-ADC1 lanes
    for (int lane = 1; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        if (_inputBypass[lane] || !_sources[lane].read) continue;
        if (!_sources[lane].isActive || !_sources[lane].isActive()) continue;
        if (!_laneL[lane] || !_laneR[lane]) continue;

        float srcSumSqL = 0.0f, srcSumSqR = 0.0f;
        for (int f = 0; f < FRAMES; f++) {
            srcSumSqL += _laneL[lane][f] * _laneL[lane][f];
            srcSumSqR += _laneR[lane][f] * _laneR[lane][f];
        }
        float srcRmsL = sqrtf(srcSumSqL / FRAMES);
        float srcRmsR = sqrtf(srcSumSqR / FRAMES);
        float srcDt = (float)FRAMES * 1000.0f / (float)AppState::getInstance().audio.sampleRate;
        _sources[lane]._vuSmoothedL = audio_vu_update(_sources[lane]._vuSmoothedL, srcRmsL, srcDt);
        _sources[lane]._vuSmoothedR = audio_vu_update(_sources[lane]._vuSmoothedR, srcRmsR, srcDt);
        _sources[lane].vuL = (_sources[lane]._vuSmoothedL > 1e-9f)
            ? 20.0f * log10f(_sources[lane]._vuSmoothedL) : -90.0f;
        _sources[lane].vuR = (_sources[lane]._vuSmoothedR > 1e-9f)
            ? 20.0f * log10f(_sources[lane]._vuSmoothedR) : -90.0f;
    }

    // Update appState USB VU if a USB source is active (find by name)
#ifdef USB_AUDIO_ENABLED
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        if (_sources[lane].name && strcmp(_sources[lane].name, "USB Audio") == 0) {
            AppState &asUsb = AppState::getInstance();
            asUsb.usbAudio.vuL = _sources[lane].vuL;
            asUsb.usbAudio.vuR = _sources[lane].vuR;
            asUsb.markUsbAudioVuDirty();
            break;
        }
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

        if (AppState::getInstance().audio.paused) {
#ifndef NATIVE_TEST
            if (AppState::getInstance().audio.taskPausedAck) {
                xSemaphoreGive(AppState::getInstance().audio.taskPausedAck);
            }
#endif
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        pipeline_sync_flags();
        pipeline_read_inputs();
        pipeline_to_float();
        pipeline_run_dsp();
        pipeline_mix_matrix();
        pipeline_run_output_dsp();
        pipeline_write_output();
        pipeline_update_metering();
        // Feed raw ADC1 data into waveform/FFT accumulator for WebSocket graph display.
        // Uses pre-float int32 data; adcIndex 0 = ADC1.
        if (_rawBuf[0]) {
            i2s_audio_push_waveform_fft(_rawBuf[0], FRAMES, 0);
        }

        // Schedule periodic serial dump every ~5s (via main loop dirty-flag pattern)
        if (++loopCount >= DUMP_INTERVAL_LOOPS) {
            loopCount = 0;
            // Capture raw ADC1 diagnostic snapshot before requesting dump
            if (_rawBuf[0]) {
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
            }
            i2s_audio_request_dump();

            // Log stack high-water mark for safety monitoring
            UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
            if (hwm < 512) {
                LOG_W("[Audio] Stack HWM LOW: %u words free", (unsigned)hwm);
            }
        }

        // Yield 2 ticks so loopTask (also on Core 1) gets scheduling time.
        // loopTask runs the main loop (HTTP, WebSocket, MQTT) at priority 1.
        // DMA has I2S_DMA_BUF_COUNT buffers = ~64ms runway at 48kHz; 2ms yield is safe.
        vTaskDelay(2);
    }
}
#endif

// ===== Matrix Init =====
// Default routing: ADC1 L/R (in_ch 0/1) → DAC L/R (out_ch 0/1), direct passthrough.
// SigGen and USB routing is set when HAL bridge registers those sources (additive via matrix).
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
    dac_boot_prepare();
#endif

    // Allocate float working buffers
#ifdef NATIVE_TEST
    // Native test: point all pointer arrays at the static storage defined above
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _rawBuf[i]  = _rawBuf_storage[i];
        _laneL[i]   = _laneL_buf[i];
        _laneR[i]   = _laneR_buf[i];
    }
    for (int i = 0; i < AUDIO_OUT_MAX_SINKS; i++) {
        _sinkBuf[i] = _sinkBuf_storage[i];
    }
    for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
        _outCh[i] = _outCh_buf[i];
    }
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _gatePrevL[i] = _gatePrevL_buf[i];
        _gatePrevR[i] = _gatePrevR_buf[i];
    }
    for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
        _swapHoldCh[i] = _swapHoldCh_buf[i];
    }
#else
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _laneL[i] = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        _laneR[i] = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!_laneL[i]) _laneL[i] = (float *)calloc(FRAMES, sizeof(float));
        if (!_laneR[i]) _laneR[i] = (float *)calloc(FRAMES, sizeof(float));
    }
    for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
        _outCh[i] = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!_outCh[i]) _outCh[i] = (float *)calloc(FRAMES, sizeof(float));
    }
    // Noise gate fade-out: PSRAM prev-frame buffers
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _gatePrevL[i] = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        _gatePrevR[i] = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!_gatePrevL[i]) _gatePrevL[i] = (float *)calloc(FRAMES, sizeof(float));
        if (!_gatePrevR[i]) _gatePrevR[i] = (float *)calloc(FRAMES, sizeof(float));
    }
    // DSP swap hold: PSRAM last-good-output buffer
    for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
        _swapHoldCh[i] = (float *)heap_caps_calloc(FRAMES, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!_swapHoldCh[i]) _swapHoldCh[i] = (float *)calloc(FRAMES, sizeof(float));
    }
#endif

    init_matrix_siggen_direct();
    audio_pipeline_load_matrix();  // Load persisted matrix (overwrites defaults if file exists)

    // Sync bypass flags from AppState
    AppState &s = AppState::getInstance();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _inputBypass[i] = s.pipelineInputBypass[i];
        _dspBypass[i]   = s.pipelineDspBypass[i];
    }
    _matrixBypass = s.pipelineMatrixBypass;
    _outputBypass = s.pipelineOutputBypass;

#ifndef NATIVE_TEST
    AppState::getInstance().audio.taskPausedAck = xSemaphoreCreateBinary();
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

    LOG_I("[Audio] Pipeline initialized — sources registered via HAL bridge at device-available");
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

void audio_pipeline_set_source(int lane, const AudioInputSource *src) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS || !src) return;
    if (lane * 2 + 1 >= AUDIO_PIPELINE_MATRIX_SIZE) return;
#ifndef NATIVE_TEST
    // Lazy-allocate DMA raw buffer for this lane on first source registration.
    // Must be in internal SRAM — DMA cannot access PSRAM.
    if (!_rawBuf[lane]) {
        _rawBuf[lane] = (int32_t *)calloc(RAW_SAMPLES, sizeof(int32_t));
        if (!_rawBuf[lane]) {
            LOG_E("[Audio] Failed to allocate rawBuf for lane %d", lane);
            return;
        }
        LOG_D("[Audio] Allocated rawBuf[%d] (%u bytes internal SRAM)", lane,
              (unsigned)(RAW_SAMPLES * sizeof(int32_t)));
    }
    vTaskSuspendAll();
#endif
    _sources[lane] = *src;  // Value copy — atomic w.r.t. task preemption
#ifndef NATIVE_TEST
    xTaskResumeAll();
#endif
}

void audio_pipeline_remove_source(int lane) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return;
    AudioInputSource empty = AUDIO_INPUT_SOURCE_INIT;
#ifndef NATIVE_TEST
    vTaskSuspendAll();
#endif
    _sources[lane] = empty;
#ifndef NATIVE_TEST
    xTaskResumeAll();
#endif
}

// DEPRECATED alias
void audio_pipeline_register_source(int lane, const AudioInputSource *src) {
    audio_pipeline_set_source(lane, src);
}

const AudioInputSource* audio_pipeline_get_source(int lane) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return nullptr;
    if (!_sources[lane].read) return nullptr;
    return &_sources[lane];
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

    const char* lane0Name = (_sources[0].read && _sources[0].name) ? _sources[0].name : "Lane 0";
    LOG_I("[Audio] === Raw Diagnostic: %s ===", lane0Name);
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
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        if (!_sources[i].read) continue;
        LOG_I("[Audio]   gate lane%d (%s) = %s", i,
              _sources[i].name ? _sources[i].name : "?",
              _gateOpen[i] ? "OPEN" : "CLOSED");
    }
}

// ===== Output Sink API =====

void audio_pipeline_register_sink(const AudioOutputSink *sink) {
    if (!sink || _sinkCount >= AUDIO_OUT_MAX_SINKS) return;
#ifndef NATIVE_TEST
    // Suspend scheduler to prevent audio task (priority 3) from preempting
    // mid-struct-copy and reading a partially-written sink with garbage function pointers.
    vTaskSuspendAll();
#endif
    _sinks[_sinkCount] = *sink;  // Value copy — atomic w.r.t. task preemption
    _sinkCount = _sinkCount + 1;
#ifndef NATIVE_TEST
    xTaskResumeAll();
#endif
    LOG_I("[Audio] Sink registered: %s ch=%d,%d (count=%d)",
          sink->name ? sink->name : "?",
          sink->firstChannel, sink->firstChannel + sink->channelCount - 1,
          (int)_sinkCount);
}

void audio_pipeline_clear_sinks() {
    // Caller must set appState.audio.paused=true and take audioTaskPausedAck before calling.
    // The audio task gives the semaphore as soon as it sees audioPaused=true, guaranteeing
    // it is yielding by the time this runs. A single volatile write is atomic on RISC-V.
    _sinkCount = 0;
    LOG_I("[Audio] All sinks cleared");
}

int audio_pipeline_get_sink_count() {
    return _sinkCount;
}

const AudioOutputSink* audio_pipeline_get_sink(int idx) {
    if (idx < 0 || idx >= _sinkCount) return nullptr;
    return &_sinks[idx];
}

void audio_pipeline_set_sink(int slot, const AudioOutputSink *sink) {
    if (slot < 0 || slot >= AUDIO_OUT_MAX_SINKS || !sink) return;
    if (sink->firstChannel + sink->channelCount > AUDIO_PIPELINE_MATRIX_SIZE) return;
#ifndef NATIVE_TEST
    // Lazy-allocate DMA output buffer for this sink slot on first registration.
    // Must be in internal SRAM — DMA cannot access PSRAM.
    if (!_sinkBuf[slot]) {
        _sinkBuf[slot] = (int32_t *)calloc(RAW_SAMPLES, sizeof(int32_t));
        if (!_sinkBuf[slot]) {
            LOG_E("[Audio] Failed to allocate sinkBuf for slot %d", slot);
            return;
        }
        LOG_D("[Audio] Allocated sinkBuf[%d] (%u bytes internal SRAM)", slot,
              (unsigned)(RAW_SAMPLES * sizeof(int32_t)));
    }
    // Reset the no-sink warning so it fires again if all sinks are later removed
    _noSinkWarned = false;
    vTaskSuspendAll();
#endif
    _sinks[slot] = *sink;
#ifndef NATIVE_TEST
    xTaskResumeAll();
#endif
    // Update _sinkCount to reflect highest occupied slot + 1
    _sinkCount = 0;
    for (int i = 0; i < AUDIO_OUT_MAX_SINKS; i++) {
        if (_sinks[i].write) _sinkCount = i + 1;
    }
#ifndef NATIVE_TEST
    AppState::getInstance().markChannelMapDirty();
#endif
    LOG_I("[Audio] Sink set at slot %d: %s ch=%d,%d (count=%d)",
          slot, sink->name ? sink->name : "?",
          sink->firstChannel, sink->firstChannel + sink->channelCount - 1,
          (int)_sinkCount);
}

void audio_pipeline_set_sink_muted(uint8_t slot, bool muted) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return;
#ifndef NATIVE_TEST
    vTaskSuspendAll();
#endif
    _sinks[slot].muted = muted;  // Single bool write — no heap allocation
#ifndef NATIVE_TEST
    xTaskResumeAll();
#endif
}

bool audio_pipeline_is_sink_muted(uint8_t slot) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return false;
    return _sinks[slot].muted;
}

void audio_pipeline_set_sink_volume(uint8_t slot, float gain) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return;
    _sinks[slot].volumeGain = gain;  // Single float write — atomic on ESP32-P4 RISC-V
}

float audio_pipeline_get_sink_volume(uint8_t slot) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return 0.0f;
    return _sinks[slot].volumeGain;
}

void audio_pipeline_remove_sink(int slot) {
    if (slot < 0 || slot >= AUDIO_OUT_MAX_SINKS) return;
    if (!_sinks[slot].write) return;  // Already empty
    const char *name = _sinks[slot].name;
#ifndef NATIVE_TEST
    vTaskSuspendAll();
#endif
    memset(&_sinks[slot], 0, sizeof(AudioOutputSink));
    _sinks[slot].gainLinear = 1.0f;
    _sinks[slot].volumeGain = 1.0f;
    _sinks[slot].vuL = -90.0f;
    _sinks[slot].vuR = -90.0f;
    _sinks[slot].halSlot = 0xFF;
#ifndef NATIVE_TEST
    xTaskResumeAll();
#endif
    // Update _sinkCount
    _sinkCount = 0;
    for (int i = 0; i < AUDIO_OUT_MAX_SINKS; i++) {
        if (_sinks[i].write) _sinkCount = i + 1;
    }
#ifndef NATIVE_TEST
    AppState::getInstance().markChannelMapDirty();
#endif
    LOG_I("[Audio] Sink removed from slot %d: %s (count=%d)",
          slot, name ? name : "?", (int)_sinkCount);
}

// ===== Matrix Persistence =====

void audio_pipeline_save_matrix() {
#ifndef NATIVE_TEST
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int o = 0; o < AUDIO_PIPELINE_MATRIX_SIZE; o++) {
        JsonArray row = arr.add<JsonArray>();
        for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
            row.add(_matrixGain[o][i]);
        }
    }
    File f = LittleFS.open("/pipeline_matrix.json", "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
        LOG_I("[Audio] Matrix saved to /pipeline_matrix.json");
    }
#endif
}

void audio_pipeline_load_matrix() {
#ifndef NATIVE_TEST
    File f = LittleFS.open("/pipeline_matrix.json", "r");
    if (!f) return;  // No saved matrix — keep defaults

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        LOG_W("[Audio] Matrix load parse error: %s", err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    int oldSize = (int)arr.size();
    if (oldSize == 0 || oldSize > AUDIO_PIPELINE_MATRIX_SIZE) {
        LOG_W("[Audio] Matrix load: invalid size %d (max %d)", oldSize, AUDIO_PIPELINE_MATRIX_SIZE);
        return;
    }
    // Backward compat: smaller matrix (e.g. 8x8 from older firmware) placed in top-left corner
    if (oldSize < AUDIO_PIPELINE_MATRIX_SIZE) {
        // Zero-fill entire matrix, then overlay the saved portion
        for (int o = 0; o < AUDIO_PIPELINE_MATRIX_SIZE; o++)
            for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++)
                _matrixGain[o][i] = 0.0f;
        LOG_I("[Pipeline] Migrated %dx%d matrix to %dx%d", oldSize, oldSize,
              AUDIO_PIPELINE_MATRIX_SIZE, AUDIO_PIPELINE_MATRIX_SIZE);
    }
    int loadSize = (oldSize < AUDIO_PIPELINE_MATRIX_SIZE) ? oldSize : AUDIO_PIPELINE_MATRIX_SIZE;
    for (int o = 0; o < loadSize; o++) {
        JsonArray row = arr[o].as<JsonArray>();
        int rowSize = (int)row.size();
        if (rowSize > AUDIO_PIPELINE_MATRIX_SIZE) rowSize = AUDIO_PIPELINE_MATRIX_SIZE;
        for (int i = 0; i < rowSize; i++) {
            _matrixGain[o][i] = row[i].as<float>();
        }
    }
    LOG_I("[Audio] Matrix loaded from /pipeline_matrix.json (%dx%d)", oldSize, oldSize);
#endif
}
