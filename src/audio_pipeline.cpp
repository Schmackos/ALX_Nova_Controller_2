// ===================================================================
// audio_pipeline.cpp — 8-lane stereo pipeline with 32×32 routing matrix
// ===================================================================
//
// Architecture:
//   8 input lanes (AudioInputSource) → per-input DSP (biquad/gain) →
//   32×32 routing matrix → per-output DSP (output_dsp) →
//   16 output sink slots (AudioOutputSink, HAL-managed)
//
// Internal format: float32 [-1.0, +1.0] throughout.
//
// Key invariants:
//   - Matrix bounds: static_assert MAX_INPUTS*2 <= MATRIX_SIZE and
//     MAX_SINKS*2 <= MATRIX_SIZE.  set_source() validates
//     lane*2+1 < MATRIX_SIZE, set_sink() validates
//     firstChannel + channelCount <= MATRIX_SIZE.
//   - DMA buffers: lane 0 + slot 0 pre-allocated at boot (4KB internal
//     SRAM via heap_caps_calloc MALLOC_CAP_DMA); remaining lanes/slots
//     lazy-allocated on first use.  DIAG_AUDIO_DMA_ALLOC_FAIL emitted
//     on failure; AudioState.dmaAllocFailed tracks affected lanes/slots.
//   - Pause protocol: callers that teardown I2S drivers MUST use
//     audio_pipeline_request_pause() / audio_pipeline_resume() (defined
//     at the end of this file).  Never set appState.audio.paused
//     directly. The audio task gives taskPausedAck when it observes the
//     flag and yields, guaranteeing it has exited i2s_read().
//   - Thread safety: set_sink/set_source/remove_sink/remove_source use an
//     atomic sentinel pattern (null write/read pointer with RELEASE/ACQUIRE
//     memory ordering) instead of vTaskSuspendAll — no global scheduler
//     suspension. set_sink_muted and set_sink_volume use single aligned
//     writes (naturally atomic on RISC-V). VU metering uses snap-read float.
// ===================================================================

#include "audio_pipeline.h"
#include "i2s_audio.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include "heap_budget.h"
#include "diag_journal.h"
#include "app_events.h"
#include "psram_alloc.h"
#include "asrc.h"
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

// ===== Atomic Slot Sentinel Pattern =====
// Sink/source slot registration uses an atomic sentinel pattern instead of
// vTaskSuspendAll(), avoiding system-wide scheduling suspension during slot ops.
//
// Pattern for set_sink(slot, sink):
//   1. Null out _sinks[slot].write atomically (__ATOMIC_RELEASE) — audio task skips slot
//   2. memcpy the full struct body (non-critical, task is not touching it)
//   3. Set the real write pointer atomically (__ATOMIC_RELEASE) — makes slot live
//
// Pattern for remove_sink(slot):
//   1. Null out write pointer atomically (__ATOMIC_RELEASE) — task stops calling immediately
//   2. Clean up remaining fields (no rush, task won't read them)
//
// Pattern for set_source / remove_source: identical, using `read` as the sentinel.
//
// On ESP32-P4 RISC-V, aligned pointer stores/loads are single bus transactions.
// __ATOMIC_RELEASE / __ATOMIC_ACQUIRE ordering ensures all preceding stores are
// visible to the Core 1 audio task before it observes the non-null sentinel.
//
// All slot-indexed APIs (set_sink, remove_sink, set_source, remove_source,
// register_sink) use the atomic sentinel pattern. ScopedSchedulerSuspend
// (vTaskSuspendAll) is no longer used in this file.

// ===== Compile-time dimension invariants =====
static_assert(AUDIO_PIPELINE_MAX_INPUTS * 2 <= AUDIO_PIPELINE_MATRIX_SIZE,
    "Matrix columns must accommodate all stereo input channels");
static_assert(AUDIO_OUT_MAX_SINKS * 2 <= AUDIO_PIPELINE_MATRIX_SIZE,
    "Matrix rows must accommodate all stereo output channels");

// ===== Constants =====
static const int FRAMES      = I2S_DMA_BUF_LEN;    // 256 stereo frames per DMA buffer
static const int RAW_SAMPLES = FRAMES * 2;          // 512 int32_t per buffer (L+R interleaved)
static const float MAX_24BIT_F = 8388607.0f;        // 2^23 - 1

// Lane float buffers must be large enough for ASRC maximum output (upsampling expands frames)
static_assert(ASRC_OUTPUT_FRAMES_MAX >= I2S_DMA_BUF_LEN,
    "Lane buffers must accommodate ASRC maximum output");

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

// Noise gate fade-out: last open-gate frame per ADC lane (PSRAM, 16 KB total (8 lanes x 2 x 1024B))
static float *_gatePrevL[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float *_gatePrevR[AUDIO_PIPELINE_MAX_INPUTS] = {};

// DSP swap hold: last good pipeline output frame (PSRAM, 16 KB total (16 ch x 1024B))
static float *_swapHoldCh[AUDIO_PIPELINE_MATRIX_SIZE] = {};

#ifdef NATIVE_TEST
static float _laneL_buf[AUDIO_PIPELINE_MAX_INPUTS][ASRC_OUTPUT_FRAMES_MAX];
static float _laneR_buf[AUDIO_PIPELINE_MAX_INPUTS][ASRC_OUTPUT_FRAMES_MAX];
static float _outCh_buf[AUDIO_PIPELINE_MATRIX_SIZE][I2S_DMA_BUF_LEN];
static float _gatePrevL_buf[AUDIO_PIPELINE_MAX_INPUTS][I2S_DMA_BUF_LEN];
static float _gatePrevR_buf[AUDIO_PIPELINE_MAX_INPUTS][I2S_DMA_BUF_LEN];
static float _swapHoldCh_buf[AUDIO_PIPELINE_MATRIX_SIZE][I2S_DMA_BUF_LEN];
#endif

// ===== Routing Matrix =====
// gain[out_ch][in_ch]: 8 output channels × 8 input channels, linear gain
static float _matrixGain[AUDIO_PIPELINE_MATRIX_SIZE][AUDIO_PIPELINE_MATRIX_SIZE] = {};

// ===== Per-Lane ASRC Output Frame Count =====
static int _laneFrames[AUDIO_PIPELINE_MAX_INPUTS]; // Actual frame count after ASRC (≤ ASRC_OUTPUT_FRAMES_MAX)

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

// ===== Atomic Slot Sentinel Accessors =====
// Used by both the Core 1 audio task (read path) and the Core 0 slot API (write path).
// These are defined here — after _sinks/_sources — so they can reference the arrays.

// Core 1 read path: atomically load sink write-callback (null = slot empty, skip it)
static inline void (*slot_sink_write_fn(int s))(const int32_t*, int) {
    return __atomic_load_n(&_sinks[s].write, __ATOMIC_ACQUIRE);
}

// Core 0 write path: atomically store sink write-callback (null clears, non-null makes live)
static inline void slot_sink_store_write_fn(int s, void (*fn)(const int32_t*, int)) {
    __atomic_store_n(&_sinks[s].write, fn, __ATOMIC_RELEASE);
}

// Core 1 read path: atomically load source read-callback (null = lane empty, skip it)
static inline uint32_t (*slot_source_read_fn(int lane))(int32_t*, uint32_t) {
    return __atomic_load_n(&_sources[lane].read, __ATOMIC_ACQUIRE);
}

// Core 0 write path: atomically store source read-callback
static inline void slot_source_store_read_fn(int lane, uint32_t (*fn)(int32_t*, uint32_t)) {
    __atomic_store_n(&_sources[lane].read, fn, __ATOMIC_RELEASE);
}

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

// ===== Pipeline Timing Metrics =====
// Written by audio_pipeline_task_fn (Core 1), read by main-loop via accessor.
// Fields are independent aligned primitives — snap-read is safe on ESP32-P4 RISC-V.
static PipelineTimingMetrics _timingMetrics = {};

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

// ===== DoP (DSD-over-PCM) Detection State =====
// DoP v1.1: the top byte (bits 31..24) of each left-justified 32-bit sample alternates
// between 0x05 and 0xFA across consecutive frames when DSD content is present.
// We confirm across 3 consecutive DMA buffers before setting isDsd to avoid false positives.
// Clears after 3 consecutive non-DoP buffers to handle stream transitions gracefully.
#define DOP_MARKER_A    0x05u
#define DOP_MARKER_B    0xFAu
#define DOP_CONFIRM_THR 3    // Consecutive DoP buffers required to assert isDsd
#define DOP_CLEAR_THR   3    // Consecutive non-DoP buffers required to de-assert isDsd
static int8_t _dopConfirmCount[AUDIO_PIPELINE_MAX_INPUTS] = {};  // >0: confirm pending; <0: clear pending

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
        // __ATOMIC_ACQUIRE load ensures we see the fully-written struct if read != NULL
        // (paired with __ATOMIC_RELEASE store in slot_source_store_read_fn on Core 0).
        auto readFn = slot_source_read_fn(lane);
        if (readFn) {
            bool active = !_sources[lane].isActive || _sources[lane].isActive();
            if (active) {
                uint32_t got = readFn(_rawBuf[lane], FRAMES);
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

                // DoP (DSD-over-PCM) detection: check alternating 0x05/0xFA markers in
                // the top byte of left-justified int32 samples. Hardware ADC lanes only —
                // software sources (SigGen, USB) cannot carry DoP content.
                if (_sources[lane].isHardwareAdc && FRAMES >= 2) {
                    uint8_t b0 = (uint8_t)((uint32_t)_rawBuf[lane][0] >> 24);
                    uint8_t b1 = (uint8_t)((uint32_t)_rawBuf[lane][2] >> 24);  // frame 1, L sample
                    bool isDop = ((b0 == DOP_MARKER_A && b1 == DOP_MARKER_B) ||
                                  (b0 == DOP_MARKER_B && b1 == DOP_MARKER_A));

                    bool wasDsd = _sources[lane].isDsd;
                    if (isDop) {
                        if (_dopConfirmCount[lane] < 0) _dopConfirmCount[lane] = 0;
                        if (_dopConfirmCount[lane] < DOP_CONFIRM_THR) _dopConfirmCount[lane]++;
                        if (!wasDsd && _dopConfirmCount[lane] >= DOP_CONFIRM_THR) {
                            _sources[lane].isDsd = true;
                            appState.audio.laneDsd[lane] = true;
                            diag_emit(DIAG_AUDIO_DSD_DETECTED, DIAG_SEV_INFO,
                                      (uint8_t)lane, "Audio", "DoP DSD detected");
                            LOG_I("[Audio] DoP DSD detected on lane %d", lane);
                            app_events_signal(EVT_FORMAT_CHANGE);
                        }
                    } else {
                        if (_dopConfirmCount[lane] > 0) _dopConfirmCount[lane] = 0;
                        if (_dopConfirmCount[lane] > -DOP_CLEAR_THR) _dopConfirmCount[lane]--;
                        if (wasDsd && _dopConfirmCount[lane] <= -DOP_CLEAR_THR) {
                            _sources[lane].isDsd = false;
                            appState.audio.laneDsd[lane] = false;
                            LOG_I("[Audio] DoP DSD cleared on lane %d", lane);
                            app_events_signal(EVT_FORMAT_CHANGE);
                        }
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

// Resample per-lane float buffers via ASRC when the source rate differs from
// the pipeline's operating rate (48kHz). DSD lanes are skipped automatically.
// Must be called after pipeline_to_float() and before pipeline_run_dsp()
// so DSP biquad coefficients (computed for 48kHz) are applied to 48kHz data.
static void pipeline_resample_inputs() {
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        _laneFrames[lane] = FRAMES;  // Default for non-ASRC and passthrough lanes
        if (!_laneL[lane] || !_laneR[lane]) continue;
        // DSD lanes must not be SRC'd — polyphase filter would corrupt the DoP bitstream
        if (_sources[lane].isDsd) {
            asrc_bypass(lane);
            continue;
        }
        if (!asrc_is_active(lane)) continue;

        // ASRC processes FRAMES input samples and writes up to ASRC_OUTPUT_FRAMES_MAX output.
        // Lane buffers are sized ASRC_OUTPUT_FRAMES_MAX to accommodate upsampled expansion.
        int outFrames = asrc_process_lane(lane, _laneL[lane], _laneR[lane], FRAMES);
        _laneFrames[lane] = outFrames;

        // Zero-fill buffer tail for downsampled lanes to prevent stale (unresampled)
        // input data from leaking through DSP and matrix stages. When srcRate > dstRate
        // (e.g. 96kHz→48kHz), ASRC produces fewer frames than FRAMES; without zero-fill,
        // positions [outFrames..FRAMES-1] retain raw input-rate floats from pipeline_to_float().
        if (outFrames < FRAMES) {
            memset(&_laneL[lane][outFrames], 0, (size_t)(FRAMES - outFrames) * sizeof(float));
            memset(&_laneR[lane][outFrames], 0, (size_t)(FRAMES - outFrames) * sizeof(float));
        }
        // When upsampling (outFrames > FRAMES), extra samples beyond FRAMES are valid but
        // unused by downstream stages (DSP/matrix operate on fixed FRAMES). The ASRC phase
        // accumulator is persistent, so no audio drift occurs from this truncation.
    }
}

static void pipeline_run_dsp() {
#ifdef DSP_ENABLED
    // Float-native DSP — no int32 bridge needed (saves ~2KB + 4 conversion loops)
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        // Skip DSP for DSD lanes: applying biquad IIR to DoP data corrupts the bitstream
        if (_dspBypass[lane] || _sources[lane].isDsd || !_laneL[lane] || !_laneR[lane]) continue;
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
        _matrixTemp = (float *)psram_alloc(FRAMES, sizeof(float), "pipe_matrix");
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
            // __ATOMIC_ACQUIRE load ensures we see the fully-written struct if writeFn != NULL
            // (paired with __ATOMIC_RELEASE store in slot_sink_store_write_fn on Core 0).
            auto writeFn = slot_sink_write_fn(s);
            if (!writeFn) continue;
            AudioOutputSink *sink = &_sinks[s];
            if (!sink->isReady || !sink->isReady()) continue;
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
            writeFn(_sinkBuf[s], FRAMES);

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

    // Watchdog: task registered with ESP task WDT. Reset every loop iteration.
    // Recovery: on WDT timeout, default IDF handler reboots device.
    // Future: consider graceful audio pipeline restart before reboot.
    // Note: IDF5.5 esp_task_wdt_delete() has a linked-list corruption bug on
    // task termination — not a concern here since this task never exits.
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

        // --- Timing: input read ---
        uint32_t _tE2eStart      = micros();
        uint32_t _tInputStart    = _tE2eStart;
        pipeline_read_inputs();
        uint32_t _tInputEnd      = micros();

        pipeline_to_float();
        pipeline_resample_inputs();

        // --- Timing: per-input DSP ---
        uint32_t _tInputDspStart = micros();
        pipeline_run_dsp();
        uint32_t _tInputDspEnd   = micros();

        // --- Timing: matrix mix (backward compat: _tFrameStart stays here) ---
        uint32_t _tFrameStart    = micros();
        uint32_t _tMatrixStart   = _tFrameStart;
        pipeline_mix_matrix();
        uint32_t _tMatrixEnd     = micros();

        // --- Timing: output DSP ---
        uint32_t _tOutDspStart   = _tMatrixEnd;
        pipeline_run_output_dsp();
        uint32_t _tOutDspEnd     = micros();

        // --- Timing: sink write ---
        uint32_t _tSinkStart     = _tOutDspEnd;
        pipeline_write_output();
        uint32_t _tSinkEnd       = micros();

        pipeline_update_metering();

        // Commit timing snapshot — compute buffer period from DMA config constants.
        // FRAMES is stereo frame count; at 48kHz one mono sample = 1/48000 s.
        // Buffer period = FRAMES samples / 48000 Hz × 1e6 µs ≈ 2667 µs for FRAMES=128.
        {
            uint32_t _tFrameEnd   = _tSinkEnd;
            uint32_t frameUs      = _tFrameEnd     - _tFrameStart;
            uint32_t matrixUs     = _tMatrixEnd    - _tMatrixStart;
            uint32_t outDspUs     = _tOutDspEnd    - _tOutDspStart;
            uint32_t inputReadUs  = _tInputEnd     - _tInputStart;
            uint32_t inputDspUs   = _tInputDspEnd  - _tInputDspStart;
            uint32_t sinkWriteUs  = _tSinkEnd      - _tSinkStart;
            // Buffer period in µs: FRAMES stereo pairs at 48 kHz (compile-time constant)
            static const uint32_t BUF_PERIOD_US =
                (uint32_t)((uint64_t)FRAMES * 1000000ULL / 48000ULL);
            float cpuPct = (BUF_PERIOD_US > 0)
                           ? (frameUs * 100.0f / (float)BUF_PERIOD_US)
                           : 0.0f;
            _timingMetrics.totalFrameUs    = frameUs;
            _timingMetrics.matrixMixUs     = matrixUs;
            _timingMetrics.outputDspUs     = outDspUs;
            _timingMetrics.totalCpuPercent = cpuPct;
            _timingMetrics.inputReadUs     = inputReadUs;
            _timingMetrics.perInputDspUs   = inputDspUs;
            _timingMetrics.sinkWriteUs     = sinkWriteUs;
            _timingMetrics.totalE2eUs      = _tSinkEnd - _tE2eStart;
        }
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
    asrc_init();

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
    {
        for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
            _laneL[i] = (float *)psram_alloc(ASRC_OUTPUT_FRAMES_MAX, sizeof(float), "pipe_lanes");
            _laneR[i] = (float *)psram_alloc(ASRC_OUTPUT_FRAMES_MAX, sizeof(float), "pipe_lanes");
        }
    }
    {
        for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
            _outCh[i] = (float *)psram_alloc(FRAMES, sizeof(float), "pipe_outCh");
        }
    }
    // Noise gate fade-out: PSRAM prev-frame buffers
    {
        for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
            _gatePrevL[i] = (float *)psram_alloc(FRAMES, sizeof(float), "pipe_gate");
            _gatePrevR[i] = (float *)psram_alloc(FRAMES, sizeof(float), "pipe_gate");
        }
    }
    // DSP swap hold: PSRAM last-good-output buffer
    {
        for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
            _swapHoldCh[i] = (float *)psram_alloc(FRAMES, sizeof(float), "pipe_swap");
        }
    }
    // ===== DMA buffer allocation (internal SRAM) =====
    // Pre-allocate lane 0 rawBuf + slot 0 sinkBuf at init (always-on onboard devices).
    // Remaining lanes/slots are lazy-allocated in set_source()/set_sink().
    // DMA buffers MUST be in internal SRAM — PSRAM is not DMA-accessible.
    {
        int dmaAllocCount = 0;
        int dmaFailCount = 0;
        uint16_t dmaFailMask = 0;
        size_t totalRawBytes = 0;
        size_t totalSinkBytes = 0;

        // rawBuf lane 0: always-on onboard ADC1 — pre-allocate
        _rawBuf[0] = (int32_t *)heap_caps_calloc(RAW_SAMPLES, sizeof(int32_t),
                                                  MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        if (_rawBuf[0]) {
            dmaAllocCount++;
            totalRawBytes += RAW_SAMPLES * sizeof(int32_t);
        } else {
            dmaFailCount++;
            dmaFailMask |= 1u;
        }
        // Lanes 1-7: lazy-allocated in audio_pipeline_set_source()

        // sinkBuf slot 0: onboard DAC — pre-allocate
        _sinkBuf[0] = (int32_t *)heap_caps_calloc(RAW_SAMPLES, sizeof(int32_t),
                                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        if (_sinkBuf[0]) {
            dmaAllocCount++;
            totalSinkBytes += RAW_SAMPLES * sizeof(int32_t);
        } else {
            dmaFailCount++;
            dmaFailMask |= (1u << 8);
        }
        // Slots 1-15: lazy-allocated in audio_pipeline_set_sink()

        if (totalRawBytes > 0)  heap_budget_record("pipeline_rawBuf",  totalRawBytes,  false);
        if (totalSinkBytes > 0) heap_budget_record("pipeline_sinkBuf", totalSinkBytes, false);

        if (dmaFailCount > 0) {
            AppState::getInstance().audio.dmaAllocFailed = true;
            AppState::getInstance().audio.dmaAllocFailMask = dmaFailMask;
            LOG_E("[Audio] DMA pre-alloc: %d/%d buffers failed (mask=0x%04X)",
                  dmaFailCount, dmaAllocCount + dmaFailCount, dmaFailMask);
            char msg[32];
            snprintf(msg, sizeof(msg), "%d DMA buf fail", dmaFailCount);
            diag_emit(DIAG_AUDIO_DMA_ALLOC_FAIL, DIAG_SEV_ERROR,
                      (uint8_t)dmaFailCount, "Audio", msg);
        } else {
            LOG_I("[Audio] Pre-allocated %d DMA buffers (%u bytes internal SRAM, remaining lazy)",
                  dmaAllocCount, (unsigned)(totalRawBytes + totalSinkBytes));
        }
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

PipelineTimingMetrics audio_pipeline_get_timing() {
    return _timingMetrics;  // Struct copy — snap-read is safe (independent aligned fields)
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

bool audio_pipeline_set_source(int lane, const AudioInputSource *src) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS || !src) return false;
    if (lane * 2 + 1 >= AUDIO_PIPELINE_MATRIX_SIZE) return false;
#ifndef NATIVE_TEST
    // Lazy DMA allocation: only lane 0 is pre-allocated at init.
    // Other lanes are allocated on first use.
    if (!_rawBuf[lane]) {
        if (AppState::getInstance().debug.heapCritical) {
            LOG_W("[Audio] Heap critical — refusing rawBuf alloc for lane %d", lane);
            diag_emit(DIAG_AUDIO_DMA_ALLOC_FAIL, DIAG_SEV_WARN,
                      (uint8_t)lane, "Audio", "rawBuf refused");
            AppState::getInstance().audio.dmaAllocFailed = true;
            AppState::getInstance().audio.dmaAllocFailMask |= (1u << lane);
            return false;
        }
        _rawBuf[lane] = (int32_t *)heap_caps_calloc(RAW_SAMPLES, sizeof(int32_t),
                                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        if (!_rawBuf[lane]) {
            LOG_E("[Audio] Failed to allocate rawBuf for lane %d", lane);
            diag_emit(DIAG_AUDIO_DMA_ALLOC_FAIL, DIAG_SEV_ERROR,
                      (uint8_t)lane, "Audio", "rawBuf alloc fail");
            AppState::getInstance().audio.dmaAllocFailed = true;
            AppState::getInstance().audio.dmaAllocFailMask |= (1u << lane);
            return false;
        }
        LOG_D("[Audio] Allocated rawBuf[%d] (%u bytes internal SRAM)", lane,
              (unsigned)(RAW_SAMPLES * sizeof(int32_t)));
        heap_budget_record("pipe_rawBuf_lazy", RAW_SAMPLES * sizeof(int32_t), false);
    }
    // Atomic sentinel swap (no scheduler suspend needed):
    // 1. Null the sentinel so the audio task stops using this lane immediately.
    // 2. Copy all non-sentinel fields into the slot.
    // 3. Store the real read pointer last with RELEASE ordering — makes the slot live.
    //    The audio task's ACQUIRE load in slot_source_read_fn() guarantees it sees
    //    the complete struct before observing the non-null sentinel.
    slot_source_store_read_fn(lane, nullptr);  // Step 1: disable lane
    {
        auto realRead = src->read;
        AudioInputSource tmp = *src;
        tmp.read = nullptr;               // Don't clobber the just-cleared sentinel yet
        _sources[lane] = tmp;             // Step 2: copy body (task ignores lane, read==NULL)
        slot_source_store_read_fn(lane, realRead);  // Step 3: make live (RELEASE barrier)
    }
#endif
    return true;
}

void audio_pipeline_remove_source(int lane) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return;
    // Step 1: atomically null the sentinel — audio task stops using this lane immediately.
    slot_source_store_read_fn(lane, nullptr);
    // Step 2: reset remaining fields (no rush, task won't touch them now).
    AudioInputSource empty = AUDIO_INPUT_SOURCE_INIT;
    empty.read = nullptr;  // Already null; reinforce for memcpy
    _sources[lane] = empty;
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

void audio_pipeline_set_source_gain(int lane, float gainLinear) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return;
    if (!_sources[lane].read) return;
    _sources[lane].gainLinear = gainLinear;
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
    int slot = _sinkCount;
    // Use same atomic sentinel pattern as set_sink(): null first, copy body, store write last.
    slot_sink_store_write_fn(slot, nullptr);
    {
        auto realWrite = sink->write;
        AudioOutputSink tmp = *sink;
        tmp.write = nullptr;
        _sinks[slot] = tmp;
        _sinkCount = slot + 1;  // Increment count before making slot live
        slot_sink_store_write_fn(slot, realWrite);
    }
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

bool audio_pipeline_set_sink(int slot, const AudioOutputSink *sink) {
    if (slot < 0 || slot >= AUDIO_OUT_MAX_SINKS || !sink) return false;
    if (sink->firstChannel + sink->channelCount > AUDIO_PIPELINE_MATRIX_SIZE) return false;
#ifndef NATIVE_TEST
    // Lazy DMA allocation: only slot 0 is pre-allocated at init.
    // Other slots are allocated on first use.
    if (!_sinkBuf[slot]) {
        if (AppState::getInstance().debug.heapCritical) {
            LOG_W("[Audio] Heap critical — refusing sinkBuf alloc for slot %d", slot);
            diag_emit(DIAG_AUDIO_DMA_ALLOC_FAIL, DIAG_SEV_WARN,
                      (uint8_t)slot, "Audio", "sinkBuf refused");
            AppState::getInstance().audio.dmaAllocFailed = true;
            AppState::getInstance().audio.dmaAllocFailMask |= (1u << (slot + 8));
            return false;
        }
        _sinkBuf[slot] = (int32_t *)heap_caps_calloc(RAW_SAMPLES, sizeof(int32_t),
                                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        if (!_sinkBuf[slot]) {
            LOG_E("[Audio] Failed to allocate sinkBuf for slot %d", slot);
            diag_emit(DIAG_AUDIO_DMA_ALLOC_FAIL, DIAG_SEV_ERROR,
                      (uint8_t)slot, "Audio", "sinkBuf alloc fail");
            AppState::getInstance().audio.dmaAllocFailed = true;
            AppState::getInstance().audio.dmaAllocFailMask |= (1u << (slot + 8));
            return false;
        }
        LOG_D("[Audio] Allocated sinkBuf[%d] (%u bytes internal SRAM)", slot,
              (unsigned)(RAW_SAMPLES * sizeof(int32_t)));
        heap_budget_record("pipe_sinkBuf_lazy", RAW_SAMPLES * sizeof(int32_t), false);
    }
    // Reset the no-sink warning so it fires again if all sinks are later removed
    _noSinkWarned = false;
    // Atomic sentinel swap (no scheduler suspend needed):
    // 1. Null the sentinel — audio task stops dispatching to this slot immediately.
    // 2. Copy all non-sentinel fields into the slot.
    // 3. Store the real write pointer with RELEASE ordering — makes the slot live.
    //    The audio task's ACQUIRE load in slot_sink_write_fn() guarantees it sees
    //    the complete struct before observing the non-null sentinel.
    slot_sink_store_write_fn(slot, nullptr);  // Step 1: disable slot
    {
        auto realWrite = sink->write;
        AudioOutputSink tmp = *sink;
        tmp.write = nullptr;               // Don't clobber the cleared sentinel yet
        _sinks[slot] = tmp;               // Step 2: copy body (task ignores slot, write==NULL)
        slot_sink_store_write_fn(slot, realWrite);  // Step 3: make live (RELEASE barrier)
    }
#endif
    // Update _sinkCount to reflect highest occupied slot + 1 (use atomic read for consistency)
    _sinkCount = 0;
    for (int i = 0; i < AUDIO_OUT_MAX_SINKS; i++) {
        if (slot_sink_write_fn(i)) _sinkCount = i + 1;
    }
#ifndef NATIVE_TEST
    AppState::getInstance().markChannelMapDirty();
#endif
    LOG_I("[Audio] Sink set at slot %d: %s ch=%d,%d (count=%d)",
          slot, sink->name ? sink->name : "?",
          sink->firstChannel, sink->firstChannel + sink->channelCount - 1,
          (int)_sinkCount);
    return true;
}

void audio_pipeline_set_sink_muted(uint8_t slot, bool muted) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return;
    // Single aligned bool write — atomic on ESP32-P4 RISC-V. No scheduler suspend needed.
    _sinks[slot].muted = muted;
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
    if (!slot_sink_write_fn(slot)) return;  // Already empty — atomic read
    const char *name = _sinks[slot].name;
    // Step 1: atomically null the sentinel — audio task stops dispatching immediately.
    slot_sink_store_write_fn(slot, nullptr);
    // Step 2: reset remaining fields (no rush, audio task won't read them now).
    memset(&_sinks[slot], 0, sizeof(AudioOutputSink));
    _sinks[slot].gainLinear = 1.0f;
    _sinks[slot].volumeGain = 1.0f;
    _sinks[slot].vuL = -90.0f;
    _sinks[slot].vuR = -90.0f;
    _sinks[slot].halSlot = 0xFF;
    // Update _sinkCount (use atomic read for consistency with sentinel pattern)
    _sinkCount = 0;
    for (int i = 0; i < AUDIO_OUT_MAX_SINKS; i++) {
        if (slot_sink_write_fn(i)) _sinkCount = i + 1;
    }
#ifndef NATIVE_TEST
    AppState::getInstance().markChannelMapDirty();
#endif
    LOG_I("[Audio] Sink removed from slot %d: %s (count=%d)",
          slot, name ? name : "?", (int)_sinkCount);
}

// ===== Cross-core Pause/Resume Protocol =====

#ifndef NATIVE_TEST
bool audio_pipeline_request_pause(uint32_t timeout_ms) {
    AppState& as = AppState::getInstance();
    as.audio.paused = true;
    if (as.audio.taskPausedAck) {
        BaseType_t took = xSemaphoreTake(as.audio.taskPausedAck,
                                          pdMS_TO_TICKS(timeout_ms));
        if (took != pdTRUE) {
            LOG_W("[Audio] Pause semaphore timeout (%lums)", (unsigned long)timeout_ms);
            return false;
        }
    }
    return true;
}

void audio_pipeline_resume() {
    AppState::getInstance().audio.paused = false;
}
#endif

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

// ===== Format Negotiation =====
// Called from main-loop context every ~5s (or on EVT_FORMAT_CHANGE).
// Reads each active source's getSampleRate(), stores per-lane values in
// appState.audio.laneSampleRates, and detects mismatches across active sinks.
//
// A mismatch exists when:
//   - Two or more active sources report different non-zero sample rates, OR
//   - Any active source rate differs from any active sink's configured sampleRate
//     (when the sink has sampleRate > 0).
//
// On state change: emits DIAG_AUDIO_RATE_MISMATCH / signals EVT_FORMAT_CHANGE.
bool audio_pipeline_check_format() {
    bool mismatch = false;
    uint32_t firstActiveRate = 0;

    // Collect per-lane sample rates from active sources
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        const AudioInputSource* src = audio_pipeline_get_source(lane);
        uint32_t rate = 0;
        if (src && src->getSampleRate) {
            rate = src->getSampleRate();
        }
        appState.audio.laneSampleRates[lane] = rate;

        if (rate > 0) {
            if (firstActiveRate == 0) {
                firstActiveRate = rate;
            } else if (rate != firstActiveRate) {
                mismatch = true;
            }
        }
    }

    // Also check against registered sink sample rates
    if (!mismatch && firstActiveRate > 0) {
        int sinkCount = audio_pipeline_get_sink_count();
        for (int s = 0; s < sinkCount && !mismatch; s++) {
            const AudioOutputSink* sink = audio_pipeline_get_sink(s);
            if (sink && sink->sampleRate > 0 && sink->sampleRate != firstActiveRate) {
                mismatch = true;
            }
        }
    }

    // Emit diagnostic and signal event only on state transitions
    bool wasMismatch = appState.audio.rateMismatch;
    if (mismatch != wasMismatch) {
        appState.audio.rateMismatch = mismatch;
        if (mismatch) {
            char msg[48];
            snprintf(msg, sizeof(msg), "rate mismatch: %luHz vs other", (unsigned long)firstActiveRate);
            diag_emit(DIAG_AUDIO_RATE_MISMATCH, DIAG_SEV_WARN, 0, "Audio", msg);
            LOG_W("[Audio] %s", msg);
        } else {
            LOG_I("[Audio] Sample rate mismatch resolved");
        }
        app_events_signal(EVT_FORMAT_CHANGE);
    }

    return mismatch;
}

// ---------------------------------------------------------------------------
// audio_pipeline_set_lane_src()
// ---------------------------------------------------------------------------

void audio_pipeline_set_lane_src(int lane, uint32_t srcRate, uint32_t dstRate) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return;

    if (srcRate == 0) {
        // Deactivate ASRC for all lanes (e.g., on source removal)
        for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
            asrc_set_ratio(i, 0, 0);
            appState.audio.laneSrcActive[i] = false;
        }
        return;
    }

    asrc_set_ratio(lane, srcRate, dstRate);
    bool active = asrc_is_active(lane);
    appState.audio.laneSrcActive[lane] = active;

    LOG_I("[Audio] ASRC lane %d: %luHz->%luHz active=%d",
          lane, (unsigned long)srcRate, (unsigned long)dstRate, (int)active);
}
