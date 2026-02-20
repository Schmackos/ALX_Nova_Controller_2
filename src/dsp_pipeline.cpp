#ifdef DSP_ENABLED

#include "dsp_pipeline.h"
#include "dsp_coefficients.h"
#include "dsp_biquad_gen.h"
#include "dsps_biquad.h"
#include "dsps_fir.h"
#include "dsps_mulc.h"
#include "dsps_mul.h"
#include "dsps_add.h"
#include "dsp_convolution.h"
#include "dsp_crossover.h"
#include "dsp_api.h"
#include "audio_quality.h"
#include "app_state.h"
#include <math.h>
#include <string.h>

#ifndef NATIVE_TEST
#include "debug_serial.h"
#include <esp_heap_caps.h>
#else
// Stubs for native test builds
#define LOG_I(...)
#define LOG_W(...)
#define LOG_E(...)
static unsigned long _mockMicros = 0;
static inline unsigned long mock_micros() { return _mockMicros; }
#define esp_timer_get_time() mock_micros()
#endif

// ===== Constants =====
static const float MAX_24BIT_F = 8388607.0f;

// ===== Double-buffered State (PSRAM on ESP32, static on native) =====
#ifdef NATIVE_TEST
static DspState _states[2];
#else
static DspState *_states = nullptr;
#endif
static volatile int _activeIndex = 0;
static volatile bool _processingActive = false;
static DspMetrics _metrics;

// ===== Emergency Safety Limiter State =====
static EmergencyLimiterState _emergencyLimiter = {};

// ===== DSP Swap Synchronization =====
#ifndef NATIVE_TEST
static SemaphoreHandle_t _swapMutex = NULL;
#endif
static volatile bool _swapRequested = false;

// ===== FIR Data Pool (PSRAM on ESP32, static on native) =====
// Each slot: taps[256] + delay[256+8] ~= 2.1KB. 2 states × 2 slots × 2.1KB ~= 8.3KB total
// Delay array is DSP_MAX_FIR_TAPS + 8 because ESP-DSP S3 SIMD reads ahead of the delay line.
#ifdef NATIVE_TEST
static float _firTaps[2][DSP_MAX_FIR_SLOTS][DSP_MAX_FIR_TAPS];
static float _firDelay[2][DSP_MAX_FIR_SLOTS][DSP_MAX_FIR_TAPS + 8];
#else
// Flat 1D pools — indexed as [stateIndex * DSP_MAX_FIR_SLOTS + firSlot] * elements
static float *_firTapsPool = nullptr;   // 2 * DSP_MAX_FIR_SLOTS * DSP_MAX_FIR_TAPS floats
static float *_firDelayPool = nullptr;  // 2 * DSP_MAX_FIR_SLOTS * (DSP_MAX_FIR_TAPS + 8) floats
#endif
static bool _firSlotUsed[DSP_MAX_FIR_SLOTS];

// ===== Delay Data Pool (dynamically allocated to save DRAM) =====
// Each slot: line[4800] = 19.2KB. Allocated on-demand when delay stages are added.
// Saves 76.8KB of static RAM when no delay stages are in use (common case).
static float *_delayLine[2][DSP_MAX_DELAY_SLOTS];  // Heap-allocated on demand
static bool _delaySlotUsed[DSP_MAX_DELAY_SLOTS];

// ===== Multi-Band Compressor Pool =====
#define DSP_MULTIBAND_MAX_SLOTS 1
#define DSP_MULTIBAND_MAX_BANDS 4

struct DspMultibandBand {
    float thresholdDb;
    float attackMs;
    float releaseMs;
    float ratio;
    float kneeDb;
    float makeupGainDb;
    float makeupLinear;
    float envelope;       // runtime
    float gainReduction;  // runtime
};

struct DspMultibandSlot {
    float crossoverFreqs[3];  // Up to 3 crossover boundaries for 4 bands
    DspMultibandBand bands[DSP_MULTIBAND_MAX_BANDS];
    float xoverCoeffs[3][2][5]; // [boundary][lpf/hpf][coeffs]
    float xoverDelay[3][2][2];  // [boundary][lpf/hpf][delay]
    float bandBuf[DSP_MULTIBAND_MAX_BANDS][256]; // Per-band processing buffers
};

#ifdef NATIVE_TEST
static DspMultibandSlot _mbSlots[DSP_MULTIBAND_MAX_SLOTS];
#else
static DspMultibandSlot *_mbSlots = nullptr;
#endif
static bool _mbSlotUsed[DSP_MULTIBAND_MAX_SLOTS];

int dsp_mb_alloc_slot() {
    for (int i = 0; i < DSP_MULTIBAND_MAX_SLOTS; i++) {
        if (!_mbSlotUsed[i]) {
            _mbSlotUsed[i] = true;
#ifdef NATIVE_TEST
            memset(&_mbSlots[i], 0, sizeof(DspMultibandSlot));
#else
            if (_mbSlots) memset(&_mbSlots[i], 0, sizeof(DspMultibandSlot));
#endif
            // Set default band params
            DspMultibandSlot *slot;
#ifdef NATIVE_TEST
            slot = &_mbSlots[i];
#else
            slot = _mbSlots ? &_mbSlots[i] : nullptr;
#endif
            if (slot) {
                slot->crossoverFreqs[0] = 200.0f;
                slot->crossoverFreqs[1] = 2000.0f;
                slot->crossoverFreqs[2] = 8000.0f;
                for (int b = 0; b < DSP_MULTIBAND_MAX_BANDS; b++) {
                    slot->bands[b].thresholdDb = -12.0f;
                    slot->bands[b].attackMs = 10.0f;
                    slot->bands[b].releaseMs = 100.0f;
                    slot->bands[b].ratio = 4.0f;
                    slot->bands[b].kneeDb = 6.0f;
                    slot->bands[b].makeupGainDb = 0.0f;
                    slot->bands[b].makeupLinear = 1.0f;
                    slot->bands[b].envelope = 0.0f;
                    slot->bands[b].gainReduction = 0.0f;
                }
            }
            return i;
        }
    }
    return -1;
}

void dsp_mb_free_slot(int slot) {
    if (slot >= 0 && slot < DSP_MULTIBAND_MAX_SLOTS) {
        _mbSlotUsed[slot] = false;
    }
}

// ===== Conversion Buffers (PSRAM on ESP32, static on native) =====
#ifdef NATIVE_TEST
static float _dspBufL[256];
static float _dspBufR[256];
static float _gainBuf[256];
#else
static float *_dspBufL = nullptr;
static float *_dspBufR = nullptr;
static float *_gainBuf = nullptr;
#endif

// Post-DSP float channel storage for routing matrix
#ifdef NATIVE_TEST
static float _postDspChannels[DSP_MAX_CHANNELS][256];  // static on native (no PSRAM)
#else
static float *_postDspChannels[DSP_MAX_CHANNELS] = {};  // PSRAM pointers on ESP32
#endif
static int _postDspFrames = 0;  // Number of valid frames in _postDspChannels

// ===== Forward Declarations =====
static int  dsp_process_channel(float *buf, int len, DspChannelConfig &ch, int stateIdx);
static void dsp_limiter_process(DspLimiterParams &lim, float *buf, int len, uint32_t sampleRate);
static void dsp_gain_process(DspGainParams &gain, float *buf, int len, uint32_t sampleRate);
static void dsp_fir_process(DspFirParams &fir, float *buf, int len, int stateIdx);
static void dsp_delay_process(DspDelayParams &dly, float *buf, int len, int stateIdx);
static void dsp_polarity_process(float *buf, int len);
static void dsp_mute_process(float *buf, int len);
static void dsp_compressor_process(DspCompressorParams &comp, float *buf, int len, uint32_t sampleRate);
static int  dsp_decimator_process(DspDecimatorParams &dec, float *buf, int len, int stateIdx);
static void dsp_noise_gate_process(DspNoiseGateParams &gate, float *buf, int len, uint32_t sampleRate);
static void dsp_tone_ctrl_process(DspToneCtrlParams &tc, float *buf, int len);
static void dsp_speaker_prot_process(DspSpeakerProtParams &sp, float *buf, int len, uint32_t sampleRate);
static void dsp_loudness_process(DspLoudnessParams &ld, float *buf, int len);
static void dsp_bass_enhance_process(DspBassEnhanceParams &be, float *buf, int len);
static void dsp_multiband_comp_process(DspMultibandCompParams &mb, float *buf, int len, uint32_t sampleRate);

// ===== FIR Pool Management =====

#ifndef NATIVE_TEST
// Flat pool index helpers for PSRAM-allocated FIR data
static inline float* _fir_taps_at(int stateIndex, int firSlot) {
    return _firTapsPool + (stateIndex * DSP_MAX_FIR_SLOTS + firSlot) * DSP_MAX_FIR_TAPS;
}
static inline float* _fir_delay_at(int stateIndex, int firSlot) {
    return _firDelayPool + (stateIndex * DSP_MAX_FIR_SLOTS + firSlot) * (DSP_MAX_FIR_TAPS + 8);
}
#endif

int dsp_fir_alloc_slot() {
    for (int i = 0; i < DSP_MAX_FIR_SLOTS; i++) {
        if (!_firSlotUsed[i]) {
            _firSlotUsed[i] = true;
            // Zero both states' data for this slot
            for (int s = 0; s < 2; s++) {
                float *taps = dsp_fir_get_taps(s, i);
                float *delay = dsp_fir_get_delay(s, i);
                if (taps) memset(taps, 0, sizeof(float) * DSP_MAX_FIR_TAPS);
                if (delay) memset(delay, 0, sizeof(float) * (DSP_MAX_FIR_TAPS + 8));
            }
            return i;
        }
    }
    return -1; // All slots in use
}

void dsp_fir_free_slot(int slot) {
    if (slot >= 0 && slot < DSP_MAX_FIR_SLOTS) {
        _firSlotUsed[slot] = false;
    }
}

float* dsp_fir_get_taps(int stateIndex, int firSlot) {
    if (stateIndex < 0 || stateIndex > 1 || firSlot < 0 || firSlot >= DSP_MAX_FIR_SLOTS)
        return nullptr;
#ifdef NATIVE_TEST
    return _firTaps[stateIndex][firSlot];
#else
    if (!_firTapsPool) return nullptr;
    return _fir_taps_at(stateIndex, firSlot);
#endif
}

float* dsp_fir_get_delay(int stateIndex, int firSlot) {
    if (stateIndex < 0 || stateIndex > 1 || firSlot < 0 || firSlot >= DSP_MAX_FIR_SLOTS)
        return nullptr;
#ifdef NATIVE_TEST
    return _firDelay[stateIndex][firSlot];
#else
    if (!_firDelayPool) return nullptr;
    return _fir_delay_at(stateIndex, firSlot);
#endif
}

// ===== Delay Pool Management =====

int dsp_delay_alloc_slot() {
#ifndef NATIVE_TEST
    // Pre-flight heap check when PSRAM is not available
    if (ESP.getPsramSize() == 0) {
        uint32_t needed = DSP_MAX_DELAY_SAMPLES * sizeof(float) * 2; // Both state pools
        uint32_t available = ESP.getMaxAllocHeap();
        if (available < needed + HEAP_WIFI_RESERVE_BYTES) { // Keep 40KB reserve for WiFi/MQTT/HTTP
            LOG_E("[DSP] Delay alloc blocked: need %lu + 40KB reserve, only %lu available",
                  (unsigned long)needed, (unsigned long)available);
            return -1;
        }
    }
#endif
    for (int i = 0; i < DSP_MAX_DELAY_SLOTS; i++) {
        if (!_delaySlotUsed[i]) {
            // Dynamically allocate delay lines for both state pools
            for (int s = 0; s < 2; s++) {
                if (!_delayLine[s][i]) {
#ifndef NATIVE_TEST
                    // Use PSRAM when available, fall back to internal heap
                    if (ESP.getPsramSize() > 0) {
                        _delayLine[s][i] = (float *)ps_calloc(DSP_MAX_DELAY_SAMPLES, sizeof(float));
                    } else {
                        _delayLine[s][i] = (float *)calloc(DSP_MAX_DELAY_SAMPLES, sizeof(float));
                    }
#else
                    _delayLine[s][i] = (float *)calloc(DSP_MAX_DELAY_SAMPLES, sizeof(float));
#endif
                    if (!_delayLine[s][i]) {
                        // Free the other pool if first succeeded
                        if (s == 1 && _delayLine[0][i]) {
                            free(_delayLine[0][i]);
                            _delayLine[0][i] = nullptr;
                        }
                        LOG_E("[DSP] Delay slot %d alloc failed (need %d bytes)",
                              i, (int)(DSP_MAX_DELAY_SAMPLES * sizeof(float)));
                        return -1;
                    }
                } else {
                    memset(_delayLine[s][i], 0, sizeof(float) * DSP_MAX_DELAY_SAMPLES);
                }
            }
            _delaySlotUsed[i] = true;
            return i;
        }
    }
    return -1;
}

void dsp_delay_free_slot(int slot) {
    if (slot >= 0 && slot < DSP_MAX_DELAY_SLOTS) {
        _delaySlotUsed[slot] = false;
        // Free heap memory when slot is released
        for (int s = 0; s < 2; s++) {
            if (_delayLine[s][slot]) {
                free(_delayLine[s][slot]);
                _delayLine[s][slot] = nullptr;
            }
        }
    }
}

float* dsp_delay_get_line(int stateIndex, int delaySlot) {
    if (stateIndex < 0 || stateIndex > 1 || delaySlot < 0 || delaySlot >= DSP_MAX_DELAY_SLOTS)
        return nullptr;
    return _delayLine[stateIndex][delaySlot];
}

// ===== Initialization =====

void dsp_init() {
#ifndef NATIVE_TEST
    // Allocate large DSP buffers from PSRAM (one-time)
    if (!_states) {
        _states = (DspState *)heap_caps_calloc(2, sizeof(DspState), MALLOC_CAP_SPIRAM);
        if (!_states) _states = (DspState *)calloc(2, sizeof(DspState));
    }
    if (!_firTapsPool) {
        size_t tapsSize = 2 * DSP_MAX_FIR_SLOTS * DSP_MAX_FIR_TAPS;
        size_t delaySize = 2 * DSP_MAX_FIR_SLOTS * (DSP_MAX_FIR_TAPS + 8);
        _firTapsPool  = (float *)heap_caps_calloc(tapsSize, sizeof(float), MALLOC_CAP_SPIRAM);
        _firDelayPool = (float *)heap_caps_calloc(delaySize, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!_firTapsPool)  _firTapsPool  = (float *)calloc(tapsSize, sizeof(float));
        if (!_firDelayPool) _firDelayPool = (float *)calloc(delaySize, sizeof(float));
    }
    if (!_dspBufL) {
        _dspBufL = (float *)heap_caps_calloc(256, sizeof(float), MALLOC_CAP_SPIRAM);
        _dspBufR = (float *)heap_caps_calloc(256, sizeof(float), MALLOC_CAP_SPIRAM);
        _gainBuf = (float *)heap_caps_calloc(256, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!_dspBufL) _dspBufL = (float *)calloc(256, sizeof(float));
        if (!_dspBufR) _dspBufR = (float *)calloc(256, sizeof(float));
        if (!_gainBuf) _gainBuf = (float *)calloc(256, sizeof(float));
    }
    if (!_postDspChannels[0]) {
        for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
            _postDspChannels[i] = (float *)heap_caps_calloc(256, sizeof(float), MALLOC_CAP_SPIRAM);
            if (!_postDspChannels[i]) _postDspChannels[i] = (float *)calloc(256, sizeof(float));
        }
    }
#endif

    dsp_init_state(_states[0]);
    dsp_init_state(_states[1]);
    dsp_init_metrics(_metrics);
    _activeIndex = 0;
    memset(&_emergencyLimiter, 0, sizeof(_emergencyLimiter));
    _emergencyLimiter.samplesSinceTrigger = UINT32_MAX / 2; // Not recently triggered

    // Clear FIR pool
#ifdef NATIVE_TEST
    memset(_firTaps, 0, sizeof(_firTaps));
    memset(_firDelay, 0, sizeof(_firDelay));
#else
    if (_firTapsPool) {
        size_t tapsBytes = 2 * DSP_MAX_FIR_SLOTS * DSP_MAX_FIR_TAPS * sizeof(float);
        size_t delayBytes = 2 * DSP_MAX_FIR_SLOTS * (DSP_MAX_FIR_TAPS + 8) * sizeof(float);
        memset(_firTapsPool, 0, tapsBytes);
        memset(_firDelayPool, 0, delayBytes);
    }
#endif
    memset(_firSlotUsed, 0, sizeof(_firSlotUsed));

    // Clear delay pool (pointers only — actual memory is heap-allocated on demand)
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < DSP_MAX_DELAY_SLOTS; i++)
            _delayLine[s][i] = nullptr;
    memset(_delaySlotUsed, 0, sizeof(_delaySlotUsed));

    // Clear multiband compressor pool
#ifndef NATIVE_TEST
    if (!_mbSlots) {
        _mbSlots = (DspMultibandSlot *)heap_caps_calloc(DSP_MULTIBAND_MAX_SLOTS, sizeof(DspMultibandSlot), MALLOC_CAP_SPIRAM);
        if (!_mbSlots) _mbSlots = (DspMultibandSlot *)calloc(DSP_MULTIBAND_MAX_SLOTS, sizeof(DspMultibandSlot));
    }
#endif
    memset(_mbSlotUsed, 0, sizeof(_mbSlotUsed));

    // Initialize swap synchronization mutex
#ifndef NATIVE_TEST
    if (!_swapMutex) {
        _swapMutex = xSemaphoreCreateMutex();
    }
#endif
    _swapRequested = false;

    LOG_I("[DSP] Pipeline initialized (double-buffered, %d channels, max %d stages/ch)",
          DSP_MAX_CHANNELS, DSP_MAX_STAGES);
}

// ===== Config Access =====

DspState *dsp_get_active_config() {
    return &_states[_activeIndex];
}

DspState *dsp_get_inactive_config() {
    return &_states[1 - _activeIndex];
}

void dsp_copy_active_to_inactive() {
    int activeIdx = _activeIndex;
    int inactiveIdx = 1 - activeIdx;

    // Copy struct data (DspFirParams only has slot index, no large arrays)
    _states[inactiveIdx] = _states[activeIdx];

    // Copy FIR pool data for used slots
    for (int s = 0; s < DSP_MAX_FIR_SLOTS; s++) {
        if (_firSlotUsed[s]) {
            float *srcTaps = dsp_fir_get_taps(activeIdx, s);
            float *dstTaps = dsp_fir_get_taps(inactiveIdx, s);
            float *srcDelay = dsp_fir_get_delay(activeIdx, s);
            float *dstDelay = dsp_fir_get_delay(inactiveIdx, s);
            if (srcTaps && dstTaps)
                memcpy(dstTaps, srcTaps, sizeof(float) * DSP_MAX_FIR_TAPS);
            if (srcDelay && dstDelay)
                memcpy(dstDelay, srcDelay, sizeof(float) * (DSP_MAX_FIR_TAPS + 8));
        }
    }

    // Copy delay pool data for used slots (both pointers must be valid)
    for (int s = 0; s < DSP_MAX_DELAY_SLOTS; s++) {
        if (_delaySlotUsed[s] && _delayLine[activeIdx][s] && _delayLine[inactiveIdx][s]) {
            memcpy(_delayLine[inactiveIdx][s], _delayLine[activeIdx][s],
                   sizeof(float) * DSP_MAX_DELAY_SAMPLES);
        }
    }
}

// Pure testable swap decision function — no FreeRTOS dependencies.
// Returns: 0=success (safe to swap), 1=mutex busy, 2=processing timeout, -1=still waiting.
// This mirrors the decision logic inside dsp_swap_config() so it can be unit-tested natively.
int dsp_swap_check_state(bool mutex_acquired, bool processing_active, int wait_iterations_remaining) {
    if (!mutex_acquired) return 1;                                 // mutex busy
    if (processing_active && wait_iterations_remaining <= 0) return 2; // processing timeout
    if (processing_active) return -1;                             // still waiting (loop again)
    return 0;                                                      // success — safe to swap
}

bool dsp_swap_config() {
    // Try to acquire mutex (5ms timeout) to prevent concurrent swaps
#ifndef NATIVE_TEST
    if (_swapMutex && xSemaphoreTake(_swapMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        LOG_W("[DSP] Swap failed: mutex busy");
        AppState::getInstance().dspSwapFailures++;
        AppState::getInstance().lastDspSwapFailure = millis();
        return false;
    }
#endif

    int oldActive = _activeIndex;
    int newActive = 1 - oldActive;

    // Set swap request flag for multi-ADC synchronization
    _swapRequested = true;

    // Wait for processing to finish (increased timeout: 100ms)
    int waitCount = 0;
    while (_processingActive && waitCount < 100) {
#ifndef NATIVE_TEST
        vTaskDelay(1);  // yield ~1ms
#endif
        waitCount++;
    }

    // Check for timeout
    if (_processingActive) {
        LOG_E("[DSP] Swap timeout after 100ms (audio task busy)");
        _swapRequested = false;
#ifndef NATIVE_TEST
        if (_swapMutex) xSemaphoreGive(_swapMutex);
#endif
        AppState::getInstance().dspSwapFailures++;
        AppState::getInstance().lastDspSwapFailure = millis();
        return false;
    }

    // Copy delay lines from old active → new active to avoid audio discontinuity
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        DspChannelConfig &oldCh = _states[oldActive].channels[ch];
        DspChannelConfig &newCh = _states[newActive].channels[ch];

        int minStages = oldCh.stageCount < newCh.stageCount ? oldCh.stageCount : newCh.stageCount;
        for (int s = 0; s < minStages; s++) {
            DspStage &oldS = oldCh.stages[s];
            DspStage &newS = newCh.stages[s];

            if (oldS.type == newS.type) {
                if (dsp_is_biquad_type(newS.type)) {
                    newS.biquad.delay[0] = oldS.biquad.delay[0];
                    newS.biquad.delay[1] = oldS.biquad.delay[1];
                    // Detect coefficient changes — initiate morphing to avoid pops
                    bool coeffChanged = false;
                    for (int c = 0; c < 5; c++) {
                        if (newS.biquad.coeffs[c] != oldS.biquad.coeffs[c]) {
                            coeffChanged = true;
                            break;
                        }
                    }
                    if (coeffChanged) {
                        // Store new coefficients as target, start from old coefficients
                        for (int c = 0; c < 5; c++) {
                            newS.biquad.targetCoeffs[c] = newS.biquad.coeffs[c];
                            newS.biquad.coeffs[c] = oldS.biquad.coeffs[c];
                        }
                        newS.biquad.morphRemaining = 64; // ~1.3ms at 48kHz
                    } else {
                        newS.biquad.morphRemaining = 0;
                    }
                } else if (newS.type == DSP_FIR && oldS.fir.firSlot >= 0 && newS.fir.firSlot >= 0) {
                    float *srcD = dsp_fir_get_delay(oldActive, oldS.fir.firSlot);
                    float *dstD = dsp_fir_get_delay(newActive, newS.fir.firSlot);
                    if (srcD && dstD)
                        memcpy(dstD, srcD, sizeof(float) * (DSP_MAX_FIR_TAPS + 8));
                    newS.fir.delayPos = oldS.fir.delayPos;
                } else if (newS.type == DSP_LIMITER) {
                    newS.limiter.envelope = oldS.limiter.envelope;
                    newS.limiter.gainReduction = oldS.limiter.gainReduction;
                } else if (newS.type == DSP_DELAY && oldS.delay.delaySlot >= 0 && newS.delay.delaySlot >= 0
                           && _delayLine[newActive][newS.delay.delaySlot]
                           && _delayLine[oldActive][oldS.delay.delaySlot]) {
                    memcpy(_delayLine[newActive][newS.delay.delaySlot],
                           _delayLine[oldActive][oldS.delay.delaySlot],
                           sizeof(float) * DSP_MAX_DELAY_SAMPLES);
                    newS.delay.writePos = oldS.delay.writePos;
                } else if (newS.type == DSP_GAIN) {
                    newS.gain.currentLinear = oldS.gain.currentLinear;
                } else if (newS.type == DSP_COMPRESSOR) {
                    newS.compressor.envelope = oldS.compressor.envelope;
                    newS.compressor.gainReduction = oldS.compressor.gainReduction;
                } else if (newS.type == DSP_DECIMATOR && oldS.decimator.firSlot >= 0 && newS.decimator.firSlot >= 0) {
                    float *srcD = dsp_fir_get_delay(oldActive, oldS.decimator.firSlot);
                    float *dstD = dsp_fir_get_delay(newActive, newS.decimator.firSlot);
                    if (srcD && dstD)
                        memcpy(dstD, srcD, sizeof(float) * (DSP_MAX_FIR_TAPS + 8));
                    newS.decimator.delayPos = oldS.decimator.delayPos;
                } else if (newS.type == DSP_NOISE_GATE) {
                    newS.noiseGate.envelope = oldS.noiseGate.envelope;
                    newS.noiseGate.gainReduction = oldS.noiseGate.gainReduction;
                    newS.noiseGate.holdCounter = oldS.noiseGate.holdCounter;
                } else if (newS.type == DSP_TONE_CTRL) {
                    memcpy(newS.toneCtrl.bassDelay, oldS.toneCtrl.bassDelay, sizeof(float) * 2);
                    memcpy(newS.toneCtrl.midDelay, oldS.toneCtrl.midDelay, sizeof(float) * 2);
                    memcpy(newS.toneCtrl.trebleDelay, oldS.toneCtrl.trebleDelay, sizeof(float) * 2);
                } else if (newS.type == DSP_SPEAKER_PROT) {
                    newS.speakerProt.currentTempC = oldS.speakerProt.currentTempC;
                    newS.speakerProt.envelope = oldS.speakerProt.envelope;
                    newS.speakerProt.gainReduction = oldS.speakerProt.gainReduction;
                } else if (newS.type == DSP_LOUDNESS) {
                    memcpy(newS.loudness.bassDelay, oldS.loudness.bassDelay, sizeof(float) * 2);
                    memcpy(newS.loudness.trebleDelay, oldS.loudness.trebleDelay, sizeof(float) * 2);
                } else if (newS.type == DSP_BASS_ENHANCE) {
                    memcpy(newS.bassEnhance.hpfDelay, oldS.bassEnhance.hpfDelay, sizeof(float) * 2);
                    memcpy(newS.bassEnhance.bpfDelay, oldS.bassEnhance.bpfDelay, sizeof(float) * 2);
                }
            }
        }
    }

    // Atomic swap
    _activeIndex = newActive;

    // Clear swap request flag
    _swapRequested = false;

    // Release mutex
#ifndef NATIVE_TEST
    if (_swapMutex) xSemaphoreGive(_swapMutex);
#endif

    // Update success counter
    AppState::getInstance().dspSwapSuccesses++;

    // Mark DSP swap event for audio quality correlation (Phase 3)
#ifndef NATIVE_TEST
    audio_quality_mark_event("dsp_swap");
#endif

    LOG_I("[DSP] Config swapped (active=%d)", newActive);
    return true;
}

// ===== Metrics =====

DspMetrics dsp_get_metrics() {
    return _metrics;
}

void dsp_reset_max_metrics() {
    _metrics.maxProcessTimeUs = 0;
}

void dsp_clear_cpu_load() {
    _metrics.processTimeUs = 0;
    _metrics.cpuLoadPercent = 0.0f;
}

// ===== Emergency Safety Limiter =====
// Brick-wall limiter at DSP output stage for speaker protection.
// Uses 8-sample lookahead to prevent overshoot, fast attack (0.1ms), moderate release (100ms).
static void dsp_emergency_limiter_process(float *bufL, float *bufR, int frames, float thresholdDb, uint32_t sampleRate) {
    const float thresholdLinear = powf(10.0f, thresholdDb / 20.0f);
    const float attackCoeff = expf(-1.0f / (0.1f * 0.001f * (float)sampleRate));   // 0.1ms attack
    const float releaseCoeff = expf(-1.0f / (100.0f * 0.001f * (float)sampleRate)); // 100ms release
    const int lookaheadSamples = 8;

    bool wasActive = false;
    float maxPeak = 0.0f;

    for (int f = 0; f < frames; f++) {
        // Write current samples to lookahead buffer
        int writePos = _emergencyLimiter.lookaheadPos;
        _emergencyLimiter.lookahead[0][writePos] = bufL[f];
        _emergencyLimiter.lookahead[1][writePos] = bufR[f];
        _emergencyLimiter.lookaheadPos = (writePos + 1) % lookaheadSamples;

        // Find peak in lookahead window (all 8 samples)
        float peakL = 0.0f, peakR = 0.0f;
        for (int i = 0; i < lookaheadSamples; i++) {
            float absL = fabsf(_emergencyLimiter.lookahead[0][i]);
            float absR = fabsf(_emergencyLimiter.lookahead[1][i]);
            if (absL > peakL) peakL = absL;
            if (absR > peakR) peakR = absR;
        }
        float peak = (peakL > peakR) ? peakL : peakR;
        if (peak > maxPeak) maxPeak = peak;

        // Envelope follower: instant attack, slow release
        if (peak > _emergencyLimiter.envelope) {
            _emergencyLimiter.envelope = peak; // Instant attack
        } else {
            _emergencyLimiter.envelope = releaseCoeff * _emergencyLimiter.envelope + (1.0f - releaseCoeff) * peak;
        }

        // Compute gain reduction (infinite ratio = hard ceiling)
        float gain = 1.0f;
        if (_emergencyLimiter.envelope > thresholdLinear) {
            gain = thresholdLinear / _emergencyLimiter.envelope;
            wasActive = true;
            _emergencyLimiter.samplesSinceTrigger = 0;
        } else {
            _emergencyLimiter.samplesSinceTrigger++;
        }

        // Read delayed samples from lookahead (lookahead delay)
        int readPos = (_emergencyLimiter.lookaheadPos + lookaheadSamples - lookaheadSamples) % lookaheadSamples;
        bufL[f] = _emergencyLimiter.lookahead[0][readPos] * gain;
        bufR[f] = _emergencyLimiter.lookahead[1][readPos] * gain;

        // Update gain reduction (convert to dB)
        if (gain < 1.0f) {
            _emergencyLimiter.gainReduction = 20.0f * log10f(gain);
        } else {
            _emergencyLimiter.gainReduction = 0.0f;
        }
    }

    // Update metrics
    _metrics.emergencyLimiterGrDb = _emergencyLimiter.gainReduction;
    _metrics.emergencyLimiterActive = (wasActive || _emergencyLimiter.samplesSinceTrigger < sampleRate / 10); // Active if triggered in last 100ms

    // Increment trigger counter if this buffer crossed threshold
    if (wasActive && _emergencyLimiter.samplesSinceTrigger == 0) {
        _emergencyLimiter.triggerCount++;
        _metrics.emergencyLimiterTriggers = _emergencyLimiter.triggerCount;
    }
}

// ===== Main Processing Entry Point =====

void dsp_process_buffer(int32_t *buffer, int stereoFrames, int adcIndex) {
    if (!buffer || stereoFrames <= 0 || stereoFrames > 256) return;

#ifndef NATIVE_TEST
    unsigned long startUs = (unsigned long)esp_timer_get_time();
#else
    unsigned long startUs = esp_timer_get_time();
#endif

    // Check for pending swap request BEFORE processing (allows swap between ADC buffers)
    if (_swapRequested && adcIndex == 0) {
        // Skip this ADC0 buffer to allow swap to complete between ADC0 and ADC1
        return;
    }

    _processingActive = true;
    int stateIdx = _activeIndex;
    DspState *cfg = &_states[stateIdx];
    if (cfg->globalBypass) {
        _metrics.processTimeUs = 0;
        _metrics.cpuLoadPercent = 0.0f;
        _processingActive = false;
        return;
    }

    // Map ADC index to channel pair: ADC0 → ch0(L), ch1(R); ADC1 → ch2(L), ch3(R)
    int chL = adcIndex * 2;
    int chR = adcIndex * 2 + 1;
    if (chL >= DSP_MAX_CHANNELS || chR >= DSP_MAX_CHANNELS) {
        _processingActive = false;
        return;
    }

    // Deinterleave int32 stereo → float mono buffers
    for (int f = 0; f < stereoFrames; f++) {
        _dspBufL[f] = (float)buffer[f * 2] / MAX_24BIT_F;
        _dspBufR[f] = (float)buffer[f * 2 + 1] / MAX_24BIT_F;
    }

    // Process each channel
    dsp_process_channel(_dspBufL, stereoFrames, cfg->channels[chL], stateIdx);
    dsp_process_channel(_dspBufR, stereoFrames, cfg->channels[chR], stateIdx);

    // Apply stereo width (mid-side processing) — operates on L+R pair, placed on L channel
    DspChannelConfig &chLeft = cfg->channels[chL];
    for (int i = 0; i < chLeft.stageCount; i++) {
        DspStage &s = chLeft.stages[i];
        if (s.enabled && s.type == DSP_STEREO_WIDTH) {
            float widthScale = s.stereoWidth.width / 100.0f;
            float centerGain = s.stereoWidth.centerGainLin;
            for (int f = 0; f < stereoFrames; f++) {
                float mid  = (_dspBufL[f] + _dspBufR[f]) * 0.5f * centerGain;
                float side = (_dspBufL[f] - _dspBufR[f]) * 0.5f * widthScale;
                _dspBufL[f] = mid + side;
                _dspBufR[f] = mid - side;
            }
            break; // Only one stereo width stage per pair
        }
    }

    // Apply emergency safety limiter (non-bypassable brick-wall protection)
    if (AppState::getInstance().emergencyLimiterEnabled) {
        float threshold = AppState::getInstance().emergencyLimiterThresholdDb;
        dsp_emergency_limiter_process(_dspBufL, _dspBufR, stereoFrames, threshold, cfg->sampleRate);
    }

    // Store post-DSP float channels for routing matrix
    memcpy(_postDspChannels[chL], _dspBufL, stereoFrames * sizeof(float));
    memcpy(_postDspChannels[chR], _dspBufR, stereoFrames * sizeof(float));
    _postDspFrames = stereoFrames;

    // Re-interleave float → int32 with clamp
    for (int f = 0; f < stereoFrames; f++) {
        float sL = _dspBufL[f];
        float sR = _dspBufR[f];
        if (sL > 1.0f) sL = 1.0f; else if (sL < -1.0f) sL = -1.0f;
        if (sR > 1.0f) sR = 1.0f; else if (sR < -1.0f) sR = -1.0f;
        buffer[f * 2] = (int32_t)(sL * MAX_24BIT_F);
        buffer[f * 2 + 1] = (int32_t)(sR * MAX_24BIT_F);
    }

    // Update timing metrics
#ifndef NATIVE_TEST
    unsigned long endUs = (unsigned long)esp_timer_get_time();
#else
    unsigned long endUs = esp_timer_get_time();
#endif
    uint32_t elapsed = (uint32_t)(endUs - startUs);
    _metrics.processTimeUs = elapsed;
    if (elapsed > _metrics.maxProcessTimeUs) _metrics.maxProcessTimeUs = elapsed;

    // CPU load = processTime / bufferPeriod * 100
    float bufferPeriodUs = (float)stereoFrames / (float)cfg->sampleRate * 1000000.0f;
    if (bufferPeriodUs > 0.0f) {
        _metrics.cpuLoadPercent = (float)elapsed / bufferPeriodUs * 100.0f;
    }

    // Collect limiter/compressor/gate GR from active channels (worst = most reduction)
    for (int c = chL; c <= chR; c++) {
        _metrics.limiterGrDb[c] = 0.0f;
        DspChannelConfig &ch = cfg->channels[c];
        for (int s = 0; s < ch.stageCount; s++) {
            if (ch.stages[s].enabled) {
                float gr = 0.0f;
                if (ch.stages[s].type == DSP_LIMITER) gr = ch.stages[s].limiter.gainReduction;
                else if (ch.stages[s].type == DSP_COMPRESSOR) gr = ch.stages[s].compressor.gainReduction;
                else if (ch.stages[s].type == DSP_NOISE_GATE) gr = ch.stages[s].noiseGate.gainReduction;
                else if (ch.stages[s].type == DSP_SPEAKER_PROT) gr = ch.stages[s].speakerProt.gainReduction;
                if (gr < _metrics.limiterGrDb[c]) _metrics.limiterGrDb[c] = gr;
            }
        }
    }

    _processingActive = false;
}

// ===== Per-Channel Processing =====

static int dsp_process_channel(float *buf, int len, DspChannelConfig &ch, int stateIdx) {
    if (ch.bypass) return len;

    DspState *cfg = &_states[stateIdx];
    int curLen = len;

    for (int i = 0; i < ch.stageCount; i++) {
        DspStage &s = ch.stages[i];
        if (!s.enabled) continue;

        if (dsp_is_biquad_type(s.type)) {
            if (s.biquad.morphRemaining > 0) {
                // Coefficient morphing: interpolate coeffs over remaining samples
                int remaining = s.biquad.morphRemaining;
                int processed = 0;
                while (processed < curLen && remaining > 0) {
                    // Interpolate coefficients
                    float t = 1.0f - (float)remaining / 64.0f;
                    float interpCoeffs[5];
                    for (int c = 0; c < 5; c++) {
                        interpCoeffs[c] = s.biquad.coeffs[c] + t * (s.biquad.targetCoeffs[c] - s.biquad.coeffs[c]);
                    }
                    // Process 8 samples at a time for efficiency
                    int chunk = curLen - processed;
                    if (chunk > 8) chunk = 8;
                    if (chunk > remaining) chunk = remaining;
                    dsps_biquad_f32(buf + processed, buf + processed, chunk, interpCoeffs, s.biquad.delay);
                    processed += chunk;
                    remaining -= chunk;
                }
                if (remaining <= 0) {
                    // Morph complete — snap to target coefficients
                    for (int c = 0; c < 5; c++) {
                        s.biquad.coeffs[c] = s.biquad.targetCoeffs[c];
                    }
                    s.biquad.morphRemaining = 0;
                    // Process any remaining samples with final coefficients
                    if (processed < curLen) {
                        dsps_biquad_f32(buf + processed, buf + processed, curLen - processed, s.biquad.coeffs, s.biquad.delay);
                    }
                } else {
                    s.biquad.morphRemaining = (uint16_t)remaining;
                }
            } else {
                dsps_biquad_f32(buf, buf, curLen, s.biquad.coeffs, s.biquad.delay);
            }
            continue;
        }

        switch (s.type) {
            case DSP_LIMITER:
                dsp_limiter_process(s.limiter, buf, curLen, cfg->sampleRate);
                break;
            case DSP_FIR:
                dsp_fir_process(s.fir, buf, curLen, stateIdx);
                break;
            case DSP_GAIN:
                dsp_gain_process(s.gain, buf, curLen, cfg->sampleRate);
                break;
            case DSP_DELAY:
                dsp_delay_process(s.delay, buf, curLen, stateIdx);
                break;
            case DSP_POLARITY:
                if (s.polarity.inverted) dsp_polarity_process(buf, curLen);
                break;
            case DSP_MUTE:
                if (s.mute.muted) dsp_mute_process(buf, curLen);
                break;
            case DSP_COMPRESSOR:
                dsp_compressor_process(s.compressor, buf, curLen, cfg->sampleRate);
                break;
            case DSP_DECIMATOR: {
                int newLen = dsp_decimator_process(s.decimator, buf, curLen, stateIdx);
                if (newLen > 0) curLen = newLen;
                break;
            }
            case DSP_CONVOLUTION:
                if (s.convolution.convSlot >= 0) {
                    dsp_conv_process(s.convolution.convSlot, buf, curLen);
                }
                break;
            case DSP_NOISE_GATE:
                dsp_noise_gate_process(s.noiseGate, buf, curLen, cfg->sampleRate);
                break;
            case DSP_TONE_CTRL:
                dsp_tone_ctrl_process(s.toneCtrl, buf, curLen);
                break;
            case DSP_SPEAKER_PROT:
                dsp_speaker_prot_process(s.speakerProt, buf, curLen, cfg->sampleRate);
                break;
            case DSP_STEREO_WIDTH:
                // Stereo width is handled post-channel in dsp_process_buffer()
                break;
            case DSP_LOUDNESS:
                dsp_loudness_process(s.loudness, buf, curLen);
                break;
            case DSP_BASS_ENHANCE:
                dsp_bass_enhance_process(s.bassEnhance, buf, curLen);
                break;
            case DSP_MULTIBAND_COMP:
                if (s.multibandComp.mbSlot >= 0) {
                    dsp_multiband_comp_process(s.multibandComp, buf, curLen, cfg->sampleRate);
                }
                break;
            default:
                break;
        }
    }
    return curLen;
}

// ===== Limiter =====

static void dsp_limiter_process(DspLimiterParams &lim, float *buf, int len, uint32_t sampleRate) {
    if (len <= 0 || sampleRate == 0) return;

    float threshLin = dsp_db_to_linear(lim.thresholdDb);
    float attackCoeff = dsp_time_coeff(lim.attackMs, (float)sampleRate);
    float releaseCoeff = dsp_time_coeff(lim.releaseMs, (float)sampleRate);

    float env = lim.envelope;
    float maxGr = 0.0f;

    // Pass 1: Envelope detection → gain buffer
    for (int i = 0; i < len; i++) {
        float absSample = fabsf(buf[i]);

        if (absSample > env) {
            env = attackCoeff * env + (1.0f - attackCoeff) * absSample;
        } else {
            env = releaseCoeff * env + (1.0f - releaseCoeff) * absSample;
        }

        float gainLin = 1.0f;
        if (env > threshLin && env > 0.0f) {
            float envDb = 20.0f * log10f(env);
            float overDb = envDb - lim.thresholdDb;
            float grDb = overDb * (1.0f - 1.0f / lim.ratio);
            gainLin = dsp_db_to_linear(-grDb);
            if (grDb > maxGr) maxGr = grDb;
        }

        _gainBuf[i] = gainLin;
    }

    // Pass 2: Apply gain via SIMD element-wise multiply
    dsps_mul_f32(buf, _gainBuf, buf, len, 1, 1, 1);

    lim.envelope = env;
    lim.gainReduction = -maxGr;
}

// ===== FIR =====

static void dsp_fir_process(DspFirParams &fir, float *buf, int len, int stateIdx) {
    if (fir.numTaps == 0 || fir.firSlot < 0 || fir.firSlot >= DSP_MAX_FIR_SLOTS) return;

    float *taps = dsp_fir_get_taps(stateIdx, fir.firSlot);
    float *delay = dsp_fir_get_delay(stateIdx, fir.firSlot);
    if (!taps || !delay) return;

    fir_f32_t firState;
    memset(&firState, 0, sizeof(firState)); // Zero extra fields (decim, use_delay on ESP32)
    firState.coeffs = taps;
    firState.delay = delay;
    firState.N = fir.numTaps;
    firState.pos = fir.delayPos;

    dsps_fir_f32(&firState, buf, buf, len);

    fir.delayPos = (uint16_t)firState.pos;
}

// ===== Gain =====

static void dsp_gain_process(DspGainParams &gain, float *buf, int len, uint32_t sampleRate) {
    float target = gain.gainLinear;
    float current = gain.currentLinear;

    // If already at target (within ~0.001 dB), use fast SIMD path
    float diff = fabsf(current - target);
    if (diff < 1e-6f) {
        gain.currentLinear = target; // snap
        dsps_mulc_f32(buf, buf, len, target, 1, 1);
        return;
    }

    // Exponential ramp: ~5ms time constant (240 samples @ 48kHz)
    float tau = 5.0f; // ms
    float coeff = dsp_time_coeff(tau, (float)sampleRate);
    float oneMinusCoeff = 1.0f - coeff;

    for (int i = 0; i < len; i++) {
        current = coeff * current + oneMinusCoeff * target;
        buf[i] *= current;
    }
    gain.currentLinear = current;
}

// ===== Delay =====

static void dsp_delay_process(DspDelayParams &dly, float *buf, int len, int stateIdx) {
    if (dly.delaySamples == 0 || dly.delaySlot < 0 || dly.delaySlot >= DSP_MAX_DELAY_SLOTS) return;

    float *line = _delayLine[stateIdx][dly.delaySlot];
    if (!line) return;  // Slot not allocated
    uint16_t delaySamples = dly.delaySamples;
    if (delaySamples > DSP_MAX_DELAY_SAMPLES) delaySamples = DSP_MAX_DELAY_SAMPLES;
    uint16_t wp = dly.writePos;

    for (int i = 0; i < len; i++) {
        // Write current sample into delay line
        line[wp] = buf[i];
        // Read from delay position behind write
        uint16_t readPos = (wp + DSP_MAX_DELAY_SAMPLES - delaySamples) % DSP_MAX_DELAY_SAMPLES;
        buf[i] = line[readPos];
        wp = (wp + 1) % DSP_MAX_DELAY_SAMPLES;
    }

    dly.writePos = wp;
}

// ===== Polarity =====

static void dsp_polarity_process(float *buf, int len) {
    dsps_mulc_f32(buf, buf, len, -1.0f, 1, 1);
}

// ===== Mute =====

static void dsp_mute_process(float *buf, int len) {
    memset(buf, 0, len * sizeof(float));
}

// ===== Compressor =====

static void dsp_compressor_process(DspCompressorParams &comp, float *buf, int len, uint32_t sampleRate) {
    if (len <= 0 || sampleRate == 0) return;

    float threshLin = dsp_db_to_linear(comp.thresholdDb);
    float attackCoeff = dsp_time_coeff(comp.attackMs, (float)sampleRate);
    float releaseCoeff = dsp_time_coeff(comp.releaseMs, (float)sampleRate);
    float makeupLin = comp.makeupLinear;

    float env = comp.envelope;
    float maxGr = 0.0f;

    // Pass 1: Envelope detection → gain buffer (includes makeup gain)
    for (int i = 0; i < len; i++) {
        float absSample = fabsf(buf[i]);

        if (absSample > env) {
            env = attackCoeff * env + (1.0f - attackCoeff) * absSample;
        } else {
            env = releaseCoeff * env + (1.0f - releaseCoeff) * absSample;
        }

        float gainLin = 1.0f;
        if (env > 0.0f) {
            float envDb = 20.0f * log10f(env);
            float overDb = envDb - comp.thresholdDb;

            float grDb = 0.0f;
            if (comp.kneeDb > 0.0f && overDb > -comp.kneeDb / 2.0f && overDb < comp.kneeDb / 2.0f) {
                float x = overDb + comp.kneeDb / 2.0f;
                grDb = (1.0f - 1.0f / comp.ratio) * x * x / (2.0f * comp.kneeDb);
            } else if (overDb >= comp.kneeDb / 2.0f) {
                grDb = overDb * (1.0f - 1.0f / comp.ratio);
            }

            if (grDb > 0.0f) {
                gainLin = dsp_db_to_linear(-grDb);
                if (grDb > maxGr) maxGr = grDb;
            }
        }

        _gainBuf[i] = gainLin * makeupLin;
    }

    // Pass 2: Apply gain via SIMD element-wise multiply
    dsps_mul_f32(buf, _gainBuf, buf, len, 1, 1, 1);

    comp.envelope = env;
    comp.gainReduction = -maxGr;
}

// ===== Decimator =====

static int dsp_decimator_process(DspDecimatorParams &dec, float *buf, int len, int stateIdx) {
    if (dec.factor <= 1 || dec.firSlot < 0 || dec.firSlot >= DSP_MAX_FIR_SLOTS) return len;

    float *taps = dsp_fir_get_taps(stateIdx, dec.firSlot);
    float *delay = dsp_fir_get_delay(stateIdx, dec.firSlot);
    if (!taps || !delay || dec.numTaps == 0) return len;

    fir_f32_t fird;
    memset(&fird, 0, sizeof(fird));
    fird.coeffs = taps;
    fird.delay = delay;
    fird.N = dec.numTaps;
    fird.pos = dec.delayPos;
    fird.decim = dec.factor;

    // Decimate in-place: output goes into the same buffer (fewer samples)
    int outLen = len / dec.factor;
    dsps_fird_f32(&fird, buf, buf, len);

    dec.delayPos = (uint16_t)fird.pos;
    return outLen;
}

// ===== Noise Gate =====

static void dsp_noise_gate_process(DspNoiseGateParams &gate, float *buf, int len, uint32_t sampleRate) {
    if (len <= 0 || sampleRate == 0) return;

    float threshLin = dsp_db_to_linear(gate.thresholdDb);
    float attackCoeff = dsp_time_coeff(gate.attackMs, (float)sampleRate);
    float releaseCoeff = dsp_time_coeff(gate.releaseMs, (float)sampleRate);
    float holdSamples = gate.holdMs * 0.001f * (float)sampleRate;
    float rangeLin = dsp_db_to_linear(gate.rangeDb);

    float env = gate.envelope;
    float holdCnt = gate.holdCounter;
    float maxGr = 0.0f;

    // Pass 1: Envelope → gain buffer
    for (int i = 0; i < len; i++) {
        float absSample = fabsf(buf[i]);

        if (absSample > env) {
            env = attackCoeff * env + (1.0f - attackCoeff) * absSample;
        } else {
            env = releaseCoeff * env + (1.0f - releaseCoeff) * absSample;
        }

        float gainLin = 1.0f;
        if (env < threshLin) {
            // Signal below threshold — apply gate/expansion
            if (holdCnt > 0.0f) {
                holdCnt -= 1.0f;
                // During hold: no attenuation
            } else {
                // Compute gate attenuation
                if (gate.ratio <= 1.0f) {
                    // Hard gate: full range attenuation
                    gainLin = rangeLin;
                } else {
                    // Expander: ratio-based attenuation
                    float envDb = (env > 1e-10f) ? 20.0f * log10f(env) : -100.0f;
                    float underDb = gate.thresholdDb - envDb;
                    if (underDb > 0.0f) {
                        float grDb = underDb * (1.0f - 1.0f / gate.ratio);
                        gainLin = dsp_db_to_linear(-grDb);
                        // Clamp to range
                        if (gainLin < rangeLin) gainLin = rangeLin;
                        if (grDb > maxGr) maxGr = grDb;
                    }
                }
                float grDb = -20.0f * log10f(gainLin > 1e-10f ? gainLin : 1e-10f);
                if (grDb > maxGr) maxGr = grDb;
            }
        } else {
            // Signal above threshold — reset hold timer
            holdCnt = holdSamples;
        }

        _gainBuf[i] = gainLin;
    }

    // Pass 2: Apply gain via SIMD
    dsps_mul_f32(buf, _gainBuf, buf, len, 1, 1, 1);

    gate.envelope = env;
    gate.holdCounter = holdCnt;
    gate.gainReduction = -maxGr;
}

// ===== Tone Control =====

static void dsp_tone_ctrl_process(DspToneCtrlParams &tc, float *buf, int len) {
    // Cascade 3 biquads: bass shelf → mid peak → treble shelf
    dsps_biquad_f32(buf, buf, len, tc.bassCoeffs, tc.bassDelay);
    dsps_biquad_f32(buf, buf, len, tc.midCoeffs, tc.midDelay);
    dsps_biquad_f32(buf, buf, len, tc.trebleCoeffs, tc.trebleDelay);
}

// ===== Speaker Protection =====

static void dsp_speaker_prot_process(DspSpeakerProtParams &sp, float *buf, int len, uint32_t sampleRate) {
    if (len <= 0 || sampleRate == 0) return;

    float dt = 1.0f / (float)sampleRate;
    float thermalTau = sp.thermalTauMs * 0.001f;
    float thermalLimit = sp.maxTempC * 0.7f;  // Start limiting at 70%
    float excursionLimit = sp.excursionLimitMm * 0.7f;

    float temp = sp.currentTempC;
    float env = sp.envelope;
    float maxGr = 0.0f;

    // Thermal mass: power to heat with time constant
    float thermalMass = thermalTau > 0.0f ? thermalTau : 2.0f;

    for (int i = 0; i < len; i++) {
        float sample = buf[i];
        float v2 = sample * sample;
        float power = v2 / sp.impedanceOhms;  // Normalized power

        // Smooth power envelope
        float alphaUp = expf(-dt / 0.010f);   // 10ms attack
        float alphaDn = expf(-dt / 0.050f);    // 50ms release
        if (power > env) env = alphaUp * env + (1.0f - alphaUp) * power;
        else             env = alphaDn * env + (1.0f - alphaDn) * power;

        // Thermal model: heat up based on power, cool down toward ambient (25C)
        temp += (env * sp.powerRatingW) * dt / thermalMass - (temp - 25.0f) * dt / thermalMass;
        if (temp < 25.0f) temp = 25.0f;

        // Thermal gain reduction (soft knee at 70% of max)
        float thermalGain = 1.0f;
        if (temp > thermalLimit && thermalLimit > 25.0f) {
            float over = (temp - thermalLimit) / (sp.maxTempC - thermalLimit);
            if (over > 1.0f) over = 1.0f;
            thermalGain = 1.0f - over * 0.9f;  // Max 90% reduction
        }

        // Excursion estimation (amplitude-based, scaled by driver area)
        float amplitude = fabsf(sample);
        float driverArea = sp.driverDiameterMm * sp.driverDiameterMm * 0.7854f; // pi/4 * d^2
        float estimatedExcursion = amplitude * 10.0f * 1000.0f / (driverArea > 0.0f ? driverArea : 1.0f);
        float excursionGain = 1.0f;
        if (estimatedExcursion > excursionLimit && excursionLimit > 0.0f) {
            excursionGain = excursionLimit / estimatedExcursion;
        }

        float gain = thermalGain < excursionGain ? thermalGain : excursionGain;
        if (gain < 0.01f) gain = 0.01f;

        float grDb = -20.0f * log10f(gain);
        if (grDb > maxGr) maxGr = grDb;

        _gainBuf[i] = gain;
    }

    dsps_mul_f32(buf, _gainBuf, buf, len, 1, 1, 1);

    sp.currentTempC = temp;
    sp.envelope = env;
    sp.gainReduction = -maxGr;
}

// ===== Loudness Compensation =====

static void dsp_loudness_process(DspLoudnessParams &ld, float *buf, int len) {
    // Cascade 2 biquads: bass shelf → treble shelf
    dsps_biquad_f32(buf, buf, len, ld.bassCoeffs, ld.bassDelay);
    dsps_biquad_f32(buf, buf, len, ld.trebleCoeffs, ld.trebleDelay);
}

// ===== Bass Enhancement =====

static void dsp_bass_enhance_process(DspBassEnhanceParams &be, float *buf, int len) {
    if (be.mix <= 0.0f) return;

    float mixScale = be.mix / 100.0f * be.harmonicGainLin;

    // Copy buf → _gainBuf (scratch), apply HPF to get high-freq content
    memcpy(_gainBuf, buf, len * sizeof(float));
    dsps_biquad_f32(_gainBuf, _gainBuf, len, be.hpfCoeffs, be.hpfDelay);

    // Subtract HPF from original to get low-freq content (in _gainBuf temporarily)
    // Actually: sub-bass = buf - HPF(buf)
    for (int i = 0; i < len; i++) {
        _gainBuf[i] = buf[i] - _gainBuf[i];  // LPF content (sub-bass)
    }

    // Generate harmonics from sub-bass
    for (int i = 0; i < len; i++) {
        float x = _gainBuf[i];
        float harmonic = 0.0f;
        if (be.order == 0 || be.order == 2) {
            harmonic += x * x;  // 2nd harmonic (sign-preserving would need abs, but x^2 generates 2f0)
        }
        if (be.order == 1 || be.order == 2) {
            harmonic += x * x * x;  // 3rd harmonic
        }
        _gainBuf[i] = harmonic;
    }

    // BPF to limit harmonic range
    dsps_biquad_f32(_gainBuf, _gainBuf, len, be.bpfCoeffs, be.bpfDelay);

    // Mix back (SIMD-accelerated: scale harmonics then add to dry signal)
    dsps_mulc_f32(_gainBuf, _gainBuf, len, mixScale, 1, 1);
    dsps_add_f32(buf, _gainBuf, buf, len, 1, 1, 1);
}

// ===== Multi-Band Compressor =====

static void dsp_multiband_comp_process(DspMultibandCompParams &mb, float *buf, int len, uint32_t sampleRate) {
    if (mb.mbSlot < 0 || mb.mbSlot >= DSP_MULTIBAND_MAX_SLOTS) return;
#ifdef NATIVE_TEST
    DspMultibandSlot &slot = _mbSlots[mb.mbSlot];
#else
    if (!_mbSlots) return;
    DspMultibandSlot &slot = _mbSlots[mb.mbSlot];
#endif

    int numBands = mb.numBands;
    if (numBands < 2) numBands = 2;
    if (numBands > DSP_MULTIBAND_MAX_BANDS) numBands = DSP_MULTIBAND_MAX_BANDS;

    // Split into bands using crossover filters (LR2 = single biquad per boundary)
    // Band 0: LPF at freq[0]
    // Band N-1: HPF at freq[N-2]
    // Middle bands: BPF between freq[i-1] and freq[i]
    int n = len > 256 ? 256 : len;

    // Copy input to all band buffers initially
    for (int b = 0; b < numBands; b++) {
        memcpy(slot.bandBuf[b], buf, n * sizeof(float));
    }

    // Apply crossover filters
    for (int boundary = 0; boundary < numBands - 1; boundary++) {
        // LPF for lower band
        dsps_biquad_f32(slot.bandBuf[boundary], slot.bandBuf[boundary], n,
                        slot.xoverCoeffs[boundary][0], slot.xoverDelay[boundary][0]);
        // HPF for upper band
        dsps_biquad_f32(slot.bandBuf[boundary + 1], slot.bandBuf[boundary + 1], n,
                        slot.xoverCoeffs[boundary][1], slot.xoverDelay[boundary][1]);
    }

    // Compress each band
    for (int b = 0; b < numBands; b++) {
        DspMultibandBand &band = slot.bands[b];
        float threshLin = dsp_db_to_linear(band.thresholdDb);
        float attackCoeff = dsp_time_coeff(band.attackMs, (float)sampleRate);
        float releaseCoeff = dsp_time_coeff(band.releaseMs, (float)sampleRate);

        float env = band.envelope;
        float maxGr = 0.0f;

        for (int i = 0; i < n; i++) {
            float absSample = fabsf(slot.bandBuf[b][i]);
            if (absSample > env)
                env = attackCoeff * env + (1.0f - attackCoeff) * absSample;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * absSample;

            float gainLin = band.makeupLinear;
            if (env > 0.0f && env > threshLin) {
                float envDb = 20.0f * log10f(env);
                float overDb = envDb - band.thresholdDb;
                float grDb = 0.0f;
                if (band.kneeDb > 0.0f && overDb > -band.kneeDb / 2.0f && overDb < band.kneeDb / 2.0f) {
                    float x = overDb + band.kneeDb / 2.0f;
                    grDb = (1.0f - 1.0f / band.ratio) * x * x / (2.0f * band.kneeDb);
                } else if (overDb >= band.kneeDb / 2.0f) {
                    grDb = overDb * (1.0f - 1.0f / band.ratio);
                }
                if (grDb > 0.0f) {
                    gainLin *= dsp_db_to_linear(-grDb);
                    if (grDb > maxGr) maxGr = grDb;
                }
            }
            slot.bandBuf[b][i] *= gainLin;
        }
        band.envelope = env;
        band.gainReduction = -maxGr;
    }

    // Sum bands back (SIMD-accelerated)
    memcpy(buf, slot.bandBuf[0], n * sizeof(float));
    for (int b = 1; b < numBands; b++) {
        dsps_add_f32(buf, slot.bandBuf[b], buf, n, 1, 1, 1);
    }
}

// ===== Stage CRUD =====

int dsp_add_stage(int channel, DspStageType type, int position) {
    if (channel < 0 || channel >= DSP_MAX_CHANNELS) return -1;
#ifndef NATIVE_TEST
    if (AppState::getInstance().heapCritical) {
        LOG_W("[DSP] Heap critical — refusing to add stage");
        return -1;
    }
#endif
    DspState *cfg = dsp_get_inactive_config();
    DspChannelConfig &ch = cfg->channels[channel];
    if (ch.stageCount >= DSP_MAX_STAGES) return -1;
    if (type >= DSP_STAGE_TYPE_COUNT) return -1;

    int pos = position;
    if (pos < 0 || pos > ch.stageCount) pos = ch.stageCount; // Append

    // Shift stages to make room
    for (int i = ch.stageCount; i > pos; i--) {
        ch.stages[i] = ch.stages[i - 1];
    }

    dsp_init_stage(ch.stages[pos], type);

    // Allocate pool slots if needed — fail and rollback if exhausted
    if (type == DSP_FIR) {
        int slot = dsp_fir_alloc_slot();
        if (slot < 0) {
            LOG_W("[DSP] No FIR slots available (max %d)", DSP_MAX_FIR_SLOTS);
            // Undo: shift stages back down
            for (int i = pos; i < ch.stageCount; i++) ch.stages[i] = ch.stages[i + 1];
            return -1;
        }
        ch.stages[pos].fir.firSlot = (int8_t)slot;
    } else if (type == DSP_DELAY) {
        int slot = dsp_delay_alloc_slot();
        if (slot < 0) {
            LOG_W("[DSP] No delay slots available (max %d)", DSP_MAX_DELAY_SLOTS);
            // Undo: shift stages back down
            for (int i = pos; i < ch.stageCount; i++) ch.stages[i] = ch.stages[i + 1];
            return -1;
        }
        ch.stages[pos].delay.delaySlot = (int8_t)slot;
    } else if (type == DSP_DECIMATOR) {
        int slot = dsp_fir_alloc_slot();
        if (slot < 0) {
            LOG_W("[DSP] No FIR slots for decimator (max %d)", DSP_MAX_FIR_SLOTS);
            for (int i = pos; i < ch.stageCount; i++) ch.stages[i] = ch.stages[i + 1];
            return -1;
        }
        ch.stages[pos].decimator.firSlot = (int8_t)slot;
        ch.stages[pos].decimator.factor = 2;
        // Design anti-aliasing filter (use half the max taps for efficiency)
        int numTaps = DSP_MAX_FIR_TAPS / 2;
        if (numTaps > DSP_MAX_FIR_TAPS) numTaps = DSP_MAX_FIR_TAPS;
        ch.stages[pos].decimator.numTaps = (uint16_t)numTaps;
        int inactiveIdx = 1 - _activeIndex;
        float *taps = dsp_fir_get_taps(inactiveIdx, slot);
        if (taps) {
            dsp_compute_decimation_filter(taps, numTaps, 2, (float)cfg->sampleRate);
        }
    } else if (type == DSP_CONVOLUTION) {
        // Convolution slots are initialized separately via dsp_conv_init_slot()
        // Just mark as unassigned; user loads IR via API
        ch.stages[pos].convolution.convSlot = -1;
        ch.stages[pos].convolution.irLength = 0;
        ch.stages[pos].convolution.irFilename[0] = '\0';
    } else if (type == DSP_MULTIBAND_COMP) {
        int slot = dsp_mb_alloc_slot();
        if (slot < 0) {
            LOG_W("[DSP] No multiband comp slots available (max %d)", DSP_MULTIBAND_MAX_SLOTS);
            for (int i = pos; i < ch.stageCount; i++) ch.stages[i] = ch.stages[i + 1];
            return -1;
        }
        ch.stages[pos].multibandComp.mbSlot = (int8_t)slot;
    }

    ch.stageCount++;

    // Compute coefficients for new stage
    if (dsp_is_biquad_type(type)) {
        dsp_compute_biquad_coeffs(ch.stages[pos].biquad, type, cfg->sampleRate);
    } else if (type == DSP_GAIN) {
        dsp_compute_gain_linear(ch.stages[pos].gain);
    } else if (type == DSP_COMPRESSOR) {
        dsp_compute_compressor_makeup(ch.stages[pos].compressor);
    } else if (type == DSP_TONE_CTRL) {
        dsp_compute_tone_ctrl_coeffs(ch.stages[pos].toneCtrl, cfg->sampleRate);
    } else if (type == DSP_LOUDNESS) {
        dsp_compute_loudness_coeffs(ch.stages[pos].loudness, cfg->sampleRate);
    } else if (type == DSP_BASS_ENHANCE) {
        dsp_compute_bass_enhance_coeffs(ch.stages[pos].bassEnhance, cfg->sampleRate);
    } else if (type == DSP_STEREO_WIDTH) {
        dsp_compute_stereo_width(ch.stages[pos].stereoWidth);
    }

    return pos;
}

bool dsp_remove_stage(int channel, int stageIndex) {
    if (channel < 0 || channel >= DSP_MAX_CHANNELS) return false;
    DspState *cfg = dsp_get_inactive_config();
    DspChannelConfig &ch = cfg->channels[channel];
    if (stageIndex < 0 || stageIndex >= ch.stageCount) return false;

    // Free pool slots if removing a pooled stage
    if (ch.stages[stageIndex].type == DSP_FIR) {
        dsp_fir_free_slot(ch.stages[stageIndex].fir.firSlot);
    } else if (ch.stages[stageIndex].type == DSP_DELAY) {
        dsp_delay_free_slot(ch.stages[stageIndex].delay.delaySlot);
    } else if (ch.stages[stageIndex].type == DSP_DECIMATOR) {
        dsp_fir_free_slot(ch.stages[stageIndex].decimator.firSlot);
    } else if (ch.stages[stageIndex].type == DSP_CONVOLUTION) {
        if (ch.stages[stageIndex].convolution.convSlot >= 0) {
            dsp_conv_free_slot(ch.stages[stageIndex].convolution.convSlot);
        }
    } else if (ch.stages[stageIndex].type == DSP_MULTIBAND_COMP) {
        dsp_mb_free_slot(ch.stages[stageIndex].multibandComp.mbSlot);
    }

    // Shift stages down
    for (int i = stageIndex; i < ch.stageCount - 1; i++) {
        ch.stages[i] = ch.stages[i + 1];
    }
    ch.stageCount--;
    return true;
}

bool dsp_reorder_stages(int channel, const int *newOrder, int count) {
    if (channel < 0 || channel >= DSP_MAX_CHANNELS) return false;
    DspState *cfg = dsp_get_inactive_config();
    DspChannelConfig &ch = cfg->channels[channel];
    if (count != ch.stageCount) return false;

    // Validate: each index appears exactly once
    bool used[DSP_MAX_STAGES] = {};
    for (int i = 0; i < count; i++) {
        if (newOrder[i] < 0 || newOrder[i] >= count || used[newOrder[i]]) return false;
        used[newOrder[i]] = true;
    }

    // Apply reorder using temp buffer
    DspStage temp[DSP_MAX_STAGES];
    for (int i = 0; i < count; i++) {
        temp[i] = ch.stages[newOrder[i]];
    }
    for (int i = 0; i < count; i++) {
        ch.stages[i] = temp[i];
    }
    return true;
}

bool dsp_set_stage_enabled(int channel, int stageIndex, bool enabled) {
    if (channel < 0 || channel >= DSP_MAX_CHANNELS) return false;
    DspState *cfg = dsp_get_inactive_config();
    DspChannelConfig &ch = cfg->channels[channel];
    if (stageIndex < 0 || stageIndex >= ch.stageCount) return false;

    ch.stages[stageIndex].enabled = enabled;
    return true;
}

// ===== Chain Stage Wrappers (PEQ-aware) =====

int dsp_add_chain_stage(int channel, DspStageType type, int chainPosition) {
    // Convert chain-relative position to absolute position
    int absPos;
    if (chainPosition < 0) {
        absPos = -1;  // Append (dsp_add_stage handles -1 as append)
    } else {
        absPos = DSP_PEQ_BANDS + chainPosition;
    }
    return dsp_add_stage(channel, type, absPos);
}

bool dsp_remove_chain_stage(int channel, int chainIndex) {
    int absIndex = DSP_PEQ_BANDS + chainIndex;
    // Prevent removing PEQ bands
    if (absIndex < DSP_PEQ_BANDS) return false;
    return dsp_remove_stage(channel, absIndex);
}

void dsp_ensure_peq_bands(DspState *cfg) {
    if (!cfg) return;
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        if (!dsp_has_peq_bands(cfg->channels[ch])) {
            dsp_init_peq_bands(cfg->channels[ch]);
        }
    }
}

void dsp_copy_peq_bands(int srcChannel, int dstChannel) {
    if (srcChannel < 0 || srcChannel >= DSP_MAX_CHANNELS) return;
    if (dstChannel < 0 || dstChannel >= DSP_MAX_CHANNELS) return;
    if (srcChannel == dstChannel) return;

    DspState *cfg = dsp_get_inactive_config();
    DspChannelConfig &src = cfg->channels[srcChannel];
    DspChannelConfig &dst = cfg->channels[dstChannel];

    // Copy PEQ bands (stages 0 through DSP_PEQ_BANDS-1)
    int peqCount = src.stageCount < DSP_PEQ_BANDS ? src.stageCount : DSP_PEQ_BANDS;
    for (int i = 0; i < peqCount; i++) {
        dst.stages[i] = src.stages[i];
    }
}

void dsp_copy_chain_stages(int srcChannel, int dstChannel) {
    if (srcChannel < 0 || srcChannel >= DSP_MAX_CHANNELS) return;
    if (dstChannel < 0 || dstChannel >= DSP_MAX_CHANNELS) return;
    if (srcChannel == dstChannel) return;

    DspState *cfg = dsp_get_inactive_config();
    DspChannelConfig &src = cfg->channels[srcChannel];
    DspChannelConfig &dst = cfg->channels[dstChannel];

    // Copy chain stages (DSP_PEQ_BANDS and above)
    int srcChainCount = src.stageCount > DSP_PEQ_BANDS ? src.stageCount - DSP_PEQ_BANDS : 0;
    int maxChain = DSP_MAX_STAGES - DSP_PEQ_BANDS;
    if (srcChainCount > maxChain) srcChainCount = maxChain;

    for (int i = 0; i < srcChainCount; i++) {
        dst.stages[DSP_PEQ_BANDS + i] = src.stages[DSP_PEQ_BANDS + i];
    }

    // Update dst stageCount: keep PEQ bands, replace chain count
    int dstPeq = dst.stageCount < DSP_PEQ_BANDS ? dst.stageCount : DSP_PEQ_BANDS;
    dst.stageCount = dstPeq + srcChainCount;
}

// ===== Stereo Link =====

int dsp_get_linked_partner(int channel) {
    if (channel < 0 || channel >= DSP_MAX_CHANNELS) return -1;
    DspState *cfg = dsp_get_inactive_config();
    if (!cfg->channels[channel].stereoLink) return -1;
    // Pairs: 0↔1, 2↔3
    return (channel % 2 == 0) ? channel + 1 : channel - 1;
}

void dsp_mirror_channel_config(int srcCh, int dstCh) {
    if (srcCh < 0 || srcCh >= DSP_MAX_CHANNELS) return;
    if (dstCh < 0 || dstCh >= DSP_MAX_CHANNELS) return;
    if (srcCh == dstCh) return;

    DspState *cfg = dsp_get_inactive_config();
    DspChannelConfig &src = cfg->channels[srcCh];
    DspChannelConfig &dst = cfg->channels[dstCh];

    // Free existing pool slots on destination
    for (int i = 0; i < dst.stageCount; i++) {
        if (dst.stages[i].type == DSP_FIR) dsp_fir_free_slot(dst.stages[i].fir.firSlot);
        else if (dst.stages[i].type == DSP_DELAY) dsp_delay_free_slot(dst.stages[i].delay.delaySlot);
        else if (dst.stages[i].type == DSP_DECIMATOR) dsp_fir_free_slot(dst.stages[i].decimator.firSlot);
        else if (dst.stages[i].type == DSP_CONVOLUTION && dst.stages[i].convolution.convSlot >= 0)
            dsp_conv_free_slot(dst.stages[i].convolution.convSlot);
        else if (dst.stages[i].type == DSP_MULTIBAND_COMP)
            dsp_mb_free_slot(dst.stages[i].multibandComp.mbSlot);
    }

    // Copy config params (bypass, stageCount, all stage params)
    dst.bypass = src.bypass;
    dst.stageCount = src.stageCount;

    for (int i = 0; i < src.stageCount; i++) {
        dst.stages[i] = src.stages[i];

        // Reset runtime state on destination
        if (dsp_is_biquad_type(dst.stages[i].type)) {
            dst.stages[i].biquad.delay[0] = 0.0f;
            dst.stages[i].biquad.delay[1] = 0.0f;
        } else if (dst.stages[i].type == DSP_LIMITER) {
            dst.stages[i].limiter.envelope = 0.0f;
            dst.stages[i].limiter.gainReduction = 0.0f;
        } else if (dst.stages[i].type == DSP_COMPRESSOR) {
            dst.stages[i].compressor.envelope = 0.0f;
            dst.stages[i].compressor.gainReduction = 0.0f;
        } else if (dst.stages[i].type == DSP_GAIN) {
            dst.stages[i].gain.currentLinear = dst.stages[i].gain.gainLinear;
        } else if (dst.stages[i].type == DSP_FIR) {
            // Allocate new FIR slot for destination, copy taps
            int newSlot = dsp_fir_alloc_slot();
            if (newSlot >= 0) {
                dst.stages[i].fir.firSlot = (int8_t)newSlot;
                dst.stages[i].fir.delayPos = 0;
                int inactiveIdx = 1 - _activeIndex;
                float *srcTaps = dsp_fir_get_taps(inactiveIdx, src.stages[i].fir.firSlot);
                float *dstTaps = dsp_fir_get_taps(inactiveIdx, newSlot);
                if (srcTaps && dstTaps) memcpy(dstTaps, srcTaps, sizeof(float) * DSP_MAX_FIR_TAPS);
            } else {
                dst.stages[i].fir.firSlot = -1;
            }
        } else if (dst.stages[i].type == DSP_DELAY) {
            // Allocate new delay slot for destination
            int newSlot = dsp_delay_alloc_slot();
            if (newSlot >= 0) {
                dst.stages[i].delay.delaySlot = (int8_t)newSlot;
                dst.stages[i].delay.writePos = 0;
            } else {
                dst.stages[i].delay.delaySlot = -1;
            }
        } else if (dst.stages[i].type == DSP_DECIMATOR) {
            int newSlot = dsp_fir_alloc_slot();
            if (newSlot >= 0) {
                dst.stages[i].decimator.firSlot = (int8_t)newSlot;
                dst.stages[i].decimator.delayPos = 0;
                int inactiveIdx = 1 - _activeIndex;
                float *srcTaps = dsp_fir_get_taps(inactiveIdx, src.stages[i].decimator.firSlot);
                float *dstTaps = dsp_fir_get_taps(inactiveIdx, newSlot);
                if (srcTaps && dstTaps) memcpy(dstTaps, srcTaps, sizeof(float) * DSP_MAX_FIR_TAPS);
            } else {
                dst.stages[i].decimator.firSlot = -1;
            }
        } else if (dst.stages[i].type == DSP_CONVOLUTION) {
            // Convolution slots are shared resources loaded from IR files.
            // For mirror, mark as unassigned — user must load IR separately.
            dst.stages[i].convolution.convSlot = -1;
        } else if (dst.stages[i].type == DSP_NOISE_GATE) {
            dst.stages[i].noiseGate.envelope = 0.0f;
            dst.stages[i].noiseGate.gainReduction = 0.0f;
            dst.stages[i].noiseGate.holdCounter = 0.0f;
        } else if (dst.stages[i].type == DSP_TONE_CTRL) {
            memset(dst.stages[i].toneCtrl.bassDelay, 0, sizeof(float) * 2);
            memset(dst.stages[i].toneCtrl.midDelay, 0, sizeof(float) * 2);
            memset(dst.stages[i].toneCtrl.trebleDelay, 0, sizeof(float) * 2);
        } else if (dst.stages[i].type == DSP_SPEAKER_PROT) {
            dst.stages[i].speakerProt.currentTempC = 25.0f;
            dst.stages[i].speakerProt.envelope = 0.0f;
            dst.stages[i].speakerProt.gainReduction = 0.0f;
        } else if (dst.stages[i].type == DSP_LOUDNESS) {
            memset(dst.stages[i].loudness.bassDelay, 0, sizeof(float) * 2);
            memset(dst.stages[i].loudness.trebleDelay, 0, sizeof(float) * 2);
        } else if (dst.stages[i].type == DSP_BASS_ENHANCE) {
            memset(dst.stages[i].bassEnhance.hpfDelay, 0, sizeof(float) * 2);
            memset(dst.stages[i].bassEnhance.bpfDelay, 0, sizeof(float) * 2);
        } else if (dst.stages[i].type == DSP_MULTIBAND_COMP) {
            // Multiband comp slots are scarce — allocate new for destination
            int newSlot = dsp_mb_alloc_slot();
            dst.stages[i].multibandComp.mbSlot = (int8_t)newSlot;
        }
    }
}

// ===== JSON Serialization =====

#ifndef NATIVE_TEST
#include <ArduinoJson.h>
#endif

// DC block functions removed in v1.8.3 - users should add a highpass filter stage instead

const char *stage_type_name(DspStageType t) {
    switch (t) {
        case DSP_BIQUAD_LPF:        return "LPF";
        case DSP_BIQUAD_HPF:        return "HPF";
        case DSP_BIQUAD_BPF:        return "BPF";
        case DSP_BIQUAD_NOTCH:      return "NOTCH";
        case DSP_BIQUAD_PEQ:        return "PEQ";
        case DSP_BIQUAD_LOW_SHELF:  return "LOW_SHELF";
        case DSP_BIQUAD_HIGH_SHELF: return "HIGH_SHELF";
        case DSP_BIQUAD_ALLPASS:    return "ALLPASS";
        case DSP_BIQUAD_ALLPASS_360: return "ALLPASS_360";
        case DSP_BIQUAD_ALLPASS_180: return "ALLPASS_180";
        case DSP_BIQUAD_BPF_0DB:   return "BPF_0DB";
        case DSP_BIQUAD_CUSTOM:     return "CUSTOM";
        case DSP_LIMITER:           return "LIMITER";
        case DSP_FIR:               return "FIR";
        case DSP_GAIN:              return "GAIN";
        case DSP_DELAY:             return "DELAY";
        case DSP_POLARITY:          return "POLARITY";
        case DSP_MUTE:              return "MUTE";
        case DSP_COMPRESSOR:        return "COMPRESSOR";
        case DSP_BIQUAD_LPF_1ST:   return "LPF_1ST";
        case DSP_BIQUAD_HPF_1ST:   return "HPF_1ST";
        case DSP_BIQUAD_LINKWITZ:  return "LINKWITZ";
        case DSP_DECIMATOR:        return "DECIMATOR";
        case DSP_CONVOLUTION:      return "CONVOLUTION";
        case DSP_NOISE_GATE:       return "NOISE_GATE";
        case DSP_TONE_CTRL:        return "TONE_CTRL";
        case DSP_SPEAKER_PROT:     return "SPEAKER_PROT";
        case DSP_STEREO_WIDTH:     return "STEREO_WIDTH";
        case DSP_LOUDNESS:         return "LOUDNESS";
        case DSP_BASS_ENHANCE:     return "BASS_ENHANCE";
        case DSP_MULTIBAND_COMP:   return "MULTIBAND_COMP";
        default: return "UNKNOWN";
    }
}

static DspStageType stage_type_from_name(const char *name) {
    if (!name) return DSP_BIQUAD_PEQ;
    if (strcmp(name, "LPF") == 0) return DSP_BIQUAD_LPF;
    if (strcmp(name, "HPF") == 0) return DSP_BIQUAD_HPF;
    if (strcmp(name, "BPF") == 0) return DSP_BIQUAD_BPF;
    if (strcmp(name, "NOTCH") == 0) return DSP_BIQUAD_NOTCH;
    if (strcmp(name, "PEQ") == 0) return DSP_BIQUAD_PEQ;
    if (strcmp(name, "LOW_SHELF") == 0) return DSP_BIQUAD_LOW_SHELF;
    if (strcmp(name, "HIGH_SHELF") == 0) return DSP_BIQUAD_HIGH_SHELF;
    if (strcmp(name, "ALLPASS") == 0) return DSP_BIQUAD_ALLPASS;
    if (strcmp(name, "ALLPASS_360") == 0) return DSP_BIQUAD_ALLPASS_360;
    if (strcmp(name, "ALLPASS_180") == 0) return DSP_BIQUAD_ALLPASS_180;
    if (strcmp(name, "BPF_0DB") == 0) return DSP_BIQUAD_BPF_0DB;
    if (strcmp(name, "CUSTOM") == 0) return DSP_BIQUAD_CUSTOM;
    if (strcmp(name, "LIMITER") == 0) return DSP_LIMITER;
    if (strcmp(name, "FIR") == 0) return DSP_FIR;
    if (strcmp(name, "GAIN") == 0) return DSP_GAIN;
    if (strcmp(name, "DELAY") == 0) return DSP_DELAY;
    if (strcmp(name, "POLARITY") == 0) return DSP_POLARITY;
    if (strcmp(name, "MUTE") == 0) return DSP_MUTE;
    if (strcmp(name, "COMPRESSOR") == 0) return DSP_COMPRESSOR;
    if (strcmp(name, "LPF_1ST") == 0) return DSP_BIQUAD_LPF_1ST;
    if (strcmp(name, "HPF_1ST") == 0) return DSP_BIQUAD_HPF_1ST;
    if (strcmp(name, "LINKWITZ") == 0) return DSP_BIQUAD_LINKWITZ;
    if (strcmp(name, "DECIMATOR") == 0) return DSP_DECIMATOR;
    if (strcmp(name, "CONVOLUTION") == 0) return DSP_CONVOLUTION;
    if (strcmp(name, "NOISE_GATE") == 0) return DSP_NOISE_GATE;
    if (strcmp(name, "TONE_CTRL") == 0) return DSP_TONE_CTRL;
    if (strcmp(name, "SPEAKER_PROT") == 0) return DSP_SPEAKER_PROT;
    if (strcmp(name, "STEREO_WIDTH") == 0) return DSP_STEREO_WIDTH;
    if (strcmp(name, "LOUDNESS") == 0) return DSP_LOUDNESS;
    if (strcmp(name, "BASS_ENHANCE") == 0) return DSP_BASS_ENHANCE;
    if (strcmp(name, "MULTIBAND_COMP") == 0) return DSP_MULTIBAND_COMP;
    return DSP_BIQUAD_PEQ;
}

#ifndef NATIVE_TEST

void dsp_export_config_to_json(int channel, char *buf, int bufSize) {
    if (channel < 0 || channel >= DSP_MAX_CHANNELS || !buf) return;
    DspState *cfg = dsp_get_active_config();
    DspChannelConfig &ch = cfg->channels[channel];

    JsonDocument doc;
    doc["bypass"] = ch.bypass;
    doc["stereoLink"] = ch.stereoLink;
    JsonArray stages = doc["stages"].to<JsonArray>();

    for (int i = 0; i < ch.stageCount; i++) {
        DspStage &s = ch.stages[i];
        JsonObject stageObj = stages.add<JsonObject>();
        stageObj["enabled"] = s.enabled;
        stageObj["type"] = stage_type_name(s.type);
        if (s.label[0]) stageObj["label"] = s.label;

        if (dsp_is_biquad_type(s.type)) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["frequency"] = s.biquad.frequency;
            params["gain"] = s.biquad.gain;
            params["Q"] = s.biquad.Q;
            if (s.type == DSP_BIQUAD_LINKWITZ) {
                params["Q2"] = s.biquad.Q2;
            }
            if (s.type == DSP_BIQUAD_CUSTOM) {
                JsonArray c = params["coeffs"].to<JsonArray>();
                for (int j = 0; j < 5; j++) c.add(s.biquad.coeffs[j]);
            }
        } else if (s.type == DSP_LIMITER) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["thresholdDb"] = s.limiter.thresholdDb;
            params["attackMs"] = s.limiter.attackMs;
            params["releaseMs"] = s.limiter.releaseMs;
            params["ratio"] = s.limiter.ratio;
        } else if (s.type == DSP_GAIN) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["gainDb"] = s.gain.gainDb;
        } else if (s.type == DSP_FIR) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["numTaps"] = s.fir.numTaps;
            params["firSlot"] = s.fir.firSlot;
        } else if (s.type == DSP_DELAY) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["delaySamples"] = s.delay.delaySamples;
        } else if (s.type == DSP_POLARITY) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["inverted"] = s.polarity.inverted;
        } else if (s.type == DSP_MUTE) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["muted"] = s.mute.muted;
        } else if (s.type == DSP_COMPRESSOR) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["thresholdDb"] = s.compressor.thresholdDb;
            params["attackMs"] = s.compressor.attackMs;
            params["releaseMs"] = s.compressor.releaseMs;
            params["ratio"] = s.compressor.ratio;
            params["kneeDb"] = s.compressor.kneeDb;
            params["makeupGainDb"] = s.compressor.makeupGainDb;
        } else if (s.type == DSP_CONVOLUTION) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["convSlot"] = s.convolution.convSlot;
            params["irLength"] = s.convolution.irLength;
            if (s.convolution.irFilename[0])
                params["irFilename"] = s.convolution.irFilename;
        } else if (s.type == DSP_NOISE_GATE) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["thresholdDb"] = s.noiseGate.thresholdDb;
            params["attackMs"] = s.noiseGate.attackMs;
            params["holdMs"] = s.noiseGate.holdMs;
            params["releaseMs"] = s.noiseGate.releaseMs;
            params["ratio"] = s.noiseGate.ratio;
            params["rangeDb"] = s.noiseGate.rangeDb;
        } else if (s.type == DSP_TONE_CTRL) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["bassGain"] = s.toneCtrl.bassGain;
            params["midGain"] = s.toneCtrl.midGain;
            params["trebleGain"] = s.toneCtrl.trebleGain;
        } else if (s.type == DSP_SPEAKER_PROT) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["powerRatingW"] = s.speakerProt.powerRatingW;
            params["impedanceOhms"] = s.speakerProt.impedanceOhms;
            params["thermalTauMs"] = s.speakerProt.thermalTauMs;
            params["excursionLimitMm"] = s.speakerProt.excursionLimitMm;
            params["driverDiameterMm"] = s.speakerProt.driverDiameterMm;
            params["maxTempC"] = s.speakerProt.maxTempC;
        } else if (s.type == DSP_STEREO_WIDTH) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["width"] = s.stereoWidth.width;
            params["centerGainDb"] = s.stereoWidth.centerGainDb;
        } else if (s.type == DSP_LOUDNESS) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["referenceLevelDb"] = s.loudness.referenceLevelDb;
            params["currentLevelDb"] = s.loudness.currentLevelDb;
            params["amount"] = s.loudness.amount;
        } else if (s.type == DSP_BASS_ENHANCE) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["frequency"] = s.bassEnhance.frequency;
            params["harmonicGainDb"] = s.bassEnhance.harmonicGainDb;
            params["mix"] = s.bassEnhance.mix;
            params["order"] = s.bassEnhance.order;
        } else if (s.type == DSP_MULTIBAND_COMP) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["numBands"] = s.multibandComp.numBands;
        }
    }

    serializeJson(doc, buf, bufSize);
}

void dsp_load_config_from_json(const char *json, int channel) {
    if (!json || channel < 0 || channel >= DSP_MAX_CHANNELS) return;
    DspState *cfg = dsp_get_inactive_config();
    int inactiveIdx = 1 - _activeIndex;
    DspChannelConfig &ch = cfg->channels[channel];

    // Free any existing pool slots for this channel
    for (int i = 0; i < ch.stageCount; i++) {
        if (ch.stages[i].type == DSP_FIR) {
            dsp_fir_free_slot(ch.stages[i].fir.firSlot);
        } else if (ch.stages[i].type == DSP_DELAY) {
            dsp_delay_free_slot(ch.stages[i].delay.delaySlot);
        } else if (ch.stages[i].type == DSP_DECIMATOR) {
            dsp_fir_free_slot(ch.stages[i].decimator.firSlot);
        } else if (ch.stages[i].type == DSP_CONVOLUTION && ch.stages[i].convolution.convSlot >= 0) {
            dsp_conv_free_slot(ch.stages[i].convolution.convSlot);
        }
    }

    JsonDocument doc;
    if (deserializeJson(doc, json)) return;

    if (doc["bypass"].is<bool>()) ch.bypass = doc["bypass"].as<bool>();
    if (doc["stereoLink"].is<bool>()) ch.stereoLink = doc["stereoLink"].as<bool>();

    if (doc["stages"].is<JsonArray>()) {
        ch.stageCount = 0;
        JsonArray stages = doc["stages"].as<JsonArray>();

        // Check if this is a legacy config (no PEQ bands) or PEQ-aware
        bool hasPeqBands = false;
        if (stages.size() >= (unsigned)DSP_PEQ_BANDS) {
            JsonObject first = stages[0].as<JsonObject>();
            const char *lbl = first["label"] | "";
            hasPeqBands = (lbl[0] == 'P' && lbl[1] == 'E' && lbl[2] == 'Q');
        }

        if (!hasPeqBands) {
            // Legacy config: init PEQ bands first, then load stages into chain region
            dsp_init_peq_bands(ch);
            // Recompute PEQ band coefficients
            for (int b = 0; b < DSP_PEQ_BANDS; b++) {
                dsp_compute_biquad_coeffs(ch.stages[b].biquad, ch.stages[b].type, cfg->sampleRate);
            }
        }

        int loadIdx = hasPeqBands ? 0 : DSP_PEQ_BANDS;
        if (hasPeqBands) ch.stageCount = 0;

        for (JsonObject stageObj : stages) {
            if (loadIdx >= DSP_MAX_STAGES) break;
            DspStage &s = ch.stages[loadIdx];
            DspStageType type = stage_type_from_name(stageObj["type"].as<const char *>());
            dsp_init_stage(s, type);
            if (stageObj["enabled"].is<bool>()) s.enabled = stageObj["enabled"].as<bool>();
            if (stageObj["label"].is<const char *>()) {
                strncpy(s.label, stageObj["label"].as<const char *>(), sizeof(s.label) - 1);
                s.label[sizeof(s.label) - 1] = '\0';
            }

            JsonObject params = stageObj["params"];
            if (dsp_is_biquad_type(type)) {
                if (params["frequency"].is<float>()) s.biquad.frequency = params["frequency"].as<float>();
                if (params["gain"].is<float>()) s.biquad.gain = params["gain"].as<float>();
                if (params["Q"].is<float>()) s.biquad.Q = params["Q"].as<float>();
                if (params["Q2"].is<float>()) s.biquad.Q2 = params["Q2"].as<float>();
                if (type == DSP_BIQUAD_CUSTOM && params["coeffs"].is<JsonArray>()) {
                    JsonArray c = params["coeffs"].as<JsonArray>();
                    for (int j = 0; j < 5 && j < (int)c.size(); j++) {
                        s.biquad.coeffs[j] = c[j].as<float>();
                    }
                } else {
                    dsp_compute_biquad_coeffs(s.biquad, type, cfg->sampleRate);
                }
            } else if (type == DSP_LIMITER) {
                if (params["thresholdDb"].is<float>()) s.limiter.thresholdDb = params["thresholdDb"].as<float>();
                if (params["attackMs"].is<float>()) s.limiter.attackMs = params["attackMs"].as<float>();
                if (params["releaseMs"].is<float>()) s.limiter.releaseMs = params["releaseMs"].as<float>();
                if (params["ratio"].is<float>()) s.limiter.ratio = params["ratio"].as<float>();
            } else if (type == DSP_GAIN) {
                if (params["gainDb"].is<float>()) s.gain.gainDb = params["gainDb"].as<float>();
                dsp_compute_gain_linear(s.gain);
            } else if (type == DSP_FIR) {
                int slot = dsp_fir_alloc_slot();
                if (slot < 0) { LOG_W("[DSP] Import: FIR slot alloc failed, skipping stage"); continue; }
                s.fir.firSlot = (int8_t)slot;
                if (params["numTaps"].is<int>()) s.fir.numTaps = params["numTaps"].as<uint16_t>();
            } else if (type == DSP_DELAY) {
                int slot = dsp_delay_alloc_slot();
                if (slot < 0) { LOG_W("[DSP] Import: delay slot alloc failed, skipping stage"); continue; }
                s.delay.delaySlot = (int8_t)slot;
                if (params["delaySamples"].is<int>()) {
                    uint16_t ds = params["delaySamples"].as<uint16_t>();
                    s.delay.delaySamples = ds > DSP_MAX_DELAY_SAMPLES ? DSP_MAX_DELAY_SAMPLES : ds;
                }
            } else if (type == DSP_POLARITY) {
                if (params["inverted"].is<bool>()) s.polarity.inverted = params["inverted"].as<bool>();
            } else if (type == DSP_MUTE) {
                if (params["muted"].is<bool>()) s.mute.muted = params["muted"].as<bool>();
            } else if (type == DSP_COMPRESSOR) {
                if (params["thresholdDb"].is<float>()) s.compressor.thresholdDb = params["thresholdDb"].as<float>();
                if (params["attackMs"].is<float>()) s.compressor.attackMs = params["attackMs"].as<float>();
                if (params["releaseMs"].is<float>()) s.compressor.releaseMs = params["releaseMs"].as<float>();
                if (params["ratio"].is<float>()) s.compressor.ratio = params["ratio"].as<float>();
                if (params["kneeDb"].is<float>()) s.compressor.kneeDb = params["kneeDb"].as<float>();
                if (params["makeupGainDb"].is<float>()) s.compressor.makeupGainDb = params["makeupGainDb"].as<float>();
                dsp_compute_compressor_makeup(s.compressor);
            } else if (type == DSP_CONVOLUTION) {
                // Convolution slot must be loaded separately via IR upload API
                s.convolution.convSlot = -1;
                if (params["irLength"].is<int>()) s.convolution.irLength = params["irLength"].as<uint16_t>();
                if (params["irFilename"].is<const char *>()) {
                    strncpy(s.convolution.irFilename, params["irFilename"].as<const char *>(), sizeof(s.convolution.irFilename) - 1);
                    s.convolution.irFilename[sizeof(s.convolution.irFilename) - 1] = '\0';
                }
            } else if (type == DSP_NOISE_GATE) {
                if (params["thresholdDb"].is<float>()) s.noiseGate.thresholdDb = params["thresholdDb"].as<float>();
                if (params["attackMs"].is<float>()) s.noiseGate.attackMs = params["attackMs"].as<float>();
                if (params["holdMs"].is<float>()) s.noiseGate.holdMs = params["holdMs"].as<float>();
                if (params["releaseMs"].is<float>()) s.noiseGate.releaseMs = params["releaseMs"].as<float>();
                if (params["ratio"].is<float>()) s.noiseGate.ratio = params["ratio"].as<float>();
                if (params["rangeDb"].is<float>()) s.noiseGate.rangeDb = params["rangeDb"].as<float>();
            } else if (type == DSP_TONE_CTRL) {
                if (params["bassGain"].is<float>()) s.toneCtrl.bassGain = params["bassGain"].as<float>();
                if (params["midGain"].is<float>()) s.toneCtrl.midGain = params["midGain"].as<float>();
                if (params["trebleGain"].is<float>()) s.toneCtrl.trebleGain = params["trebleGain"].as<float>();
                dsp_compute_tone_ctrl_coeffs(s.toneCtrl, cfg->sampleRate);
            } else if (type == DSP_SPEAKER_PROT) {
                if (params["powerRatingW"].is<float>()) s.speakerProt.powerRatingW = params["powerRatingW"].as<float>();
                if (params["impedanceOhms"].is<float>()) s.speakerProt.impedanceOhms = params["impedanceOhms"].as<float>();
                if (params["thermalTauMs"].is<float>()) s.speakerProt.thermalTauMs = params["thermalTauMs"].as<float>();
                if (params["excursionLimitMm"].is<float>()) s.speakerProt.excursionLimitMm = params["excursionLimitMm"].as<float>();
                if (params["driverDiameterMm"].is<float>()) s.speakerProt.driverDiameterMm = params["driverDiameterMm"].as<float>();
                if (params["maxTempC"].is<float>()) s.speakerProt.maxTempC = params["maxTempC"].as<float>();
                dsp_compute_speaker_prot(s.speakerProt);
            } else if (type == DSP_STEREO_WIDTH) {
                if (params["width"].is<float>()) s.stereoWidth.width = params["width"].as<float>();
                if (params["centerGainDb"].is<float>()) s.stereoWidth.centerGainDb = params["centerGainDb"].as<float>();
                dsp_compute_stereo_width(s.stereoWidth);
            } else if (type == DSP_LOUDNESS) {
                if (params["referenceLevelDb"].is<float>()) s.loudness.referenceLevelDb = params["referenceLevelDb"].as<float>();
                if (params["currentLevelDb"].is<float>()) s.loudness.currentLevelDb = params["currentLevelDb"].as<float>();
                if (params["amount"].is<float>()) s.loudness.amount = params["amount"].as<float>();
                dsp_compute_loudness_coeffs(s.loudness, cfg->sampleRate);
            } else if (type == DSP_BASS_ENHANCE) {
                if (params["frequency"].is<float>()) s.bassEnhance.frequency = params["frequency"].as<float>();
                if (params["harmonicGainDb"].is<float>()) s.bassEnhance.harmonicGainDb = params["harmonicGainDb"].as<float>();
                if (params["mix"].is<float>()) s.bassEnhance.mix = params["mix"].as<float>();
                if (params["order"].is<int>()) s.bassEnhance.order = params["order"].as<uint8_t>();
                dsp_compute_bass_enhance_coeffs(s.bassEnhance, cfg->sampleRate);
            } else if (type == DSP_MULTIBAND_COMP) {
                if (params["numBands"].is<int>()) s.multibandComp.numBands = params["numBands"].as<uint8_t>();
                int slot = dsp_mb_alloc_slot();
                if (slot < 0) { LOG_W("[DSP] Import: multiband slot alloc failed, skipping"); continue; }
                s.multibandComp.mbSlot = (int8_t)slot;
            }
            loadIdx++;
            ch.stageCount = loadIdx;
        }
    }
}

void dsp_export_full_config_json(char *buf, int bufSize) {
    if (!buf) return;
    DspState *cfg = dsp_get_active_config();

    JsonDocument doc;
    doc["globalBypass"] = cfg->globalBypass;
    doc["sampleRate"] = cfg->sampleRate;
    JsonArray channels = doc["channels"].to<JsonArray>();

    for (int c = 0; c < DSP_MAX_CHANNELS; c++) {
        JsonObject chObj = channels.add<JsonObject>();
        DspChannelConfig &ch = cfg->channels[c];
        chObj["bypass"] = ch.bypass;
        chObj["stereoLink"] = ch.stereoLink;
        JsonArray stages = chObj["stages"].to<JsonArray>();

        for (int i = 0; i < ch.stageCount; i++) {
            DspStage &s = ch.stages[i];
            JsonObject stageObj = stages.add<JsonObject>();
            stageObj["enabled"] = s.enabled;
            stageObj["type"] = stage_type_name(s.type);
            if (s.label[0]) stageObj["label"] = s.label;

            if (dsp_is_biquad_type(s.type)) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["frequency"] = s.biquad.frequency;
                params["gain"] = s.biquad.gain;
                params["Q"] = s.biquad.Q;
                if (s.type == DSP_BIQUAD_LINKWITZ) {
                    params["Q2"] = s.biquad.Q2;
                }
                if (s.type == DSP_BIQUAD_CUSTOM) {
                    JsonArray co = params["coeffs"].to<JsonArray>();
                    for (int j = 0; j < 5; j++) co.add(s.biquad.coeffs[j]);
                }
            } else if (s.type == DSP_LIMITER) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["thresholdDb"] = s.limiter.thresholdDb;
                params["attackMs"] = s.limiter.attackMs;
                params["releaseMs"] = s.limiter.releaseMs;
                params["ratio"] = s.limiter.ratio;
            } else if (s.type == DSP_GAIN) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["gainDb"] = s.gain.gainDb;
            } else if (s.type == DSP_FIR) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["numTaps"] = s.fir.numTaps;
                params["firSlot"] = s.fir.firSlot;
            } else if (s.type == DSP_DELAY) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["delaySamples"] = s.delay.delaySamples;
            } else if (s.type == DSP_POLARITY) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["inverted"] = s.polarity.inverted;
            } else if (s.type == DSP_MUTE) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["muted"] = s.mute.muted;
            } else if (s.type == DSP_COMPRESSOR) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["thresholdDb"] = s.compressor.thresholdDb;
                params["attackMs"] = s.compressor.attackMs;
                params["releaseMs"] = s.compressor.releaseMs;
                params["ratio"] = s.compressor.ratio;
                params["kneeDb"] = s.compressor.kneeDb;
                params["makeupGainDb"] = s.compressor.makeupGainDb;
            } else if (s.type == DSP_CONVOLUTION) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["convSlot"] = s.convolution.convSlot;
                params["irLength"] = s.convolution.irLength;
                if (s.convolution.irFilename[0])
                    params["irFilename"] = s.convolution.irFilename;
            } else if (s.type == DSP_NOISE_GATE) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["thresholdDb"] = s.noiseGate.thresholdDb;
                params["attackMs"] = s.noiseGate.attackMs;
                params["holdMs"] = s.noiseGate.holdMs;
                params["releaseMs"] = s.noiseGate.releaseMs;
                params["ratio"] = s.noiseGate.ratio;
                params["rangeDb"] = s.noiseGate.rangeDb;
            } else if (s.type == DSP_TONE_CTRL) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["bassGain"] = s.toneCtrl.bassGain;
                params["midGain"] = s.toneCtrl.midGain;
                params["trebleGain"] = s.toneCtrl.trebleGain;
            } else if (s.type == DSP_SPEAKER_PROT) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["powerRatingW"] = s.speakerProt.powerRatingW;
                params["impedanceOhms"] = s.speakerProt.impedanceOhms;
                params["thermalTauMs"] = s.speakerProt.thermalTauMs;
                params["excursionLimitMm"] = s.speakerProt.excursionLimitMm;
                params["driverDiameterMm"] = s.speakerProt.driverDiameterMm;
                params["maxTempC"] = s.speakerProt.maxTempC;
            } else if (s.type == DSP_STEREO_WIDTH) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["width"] = s.stereoWidth.width;
                params["centerGainDb"] = s.stereoWidth.centerGainDb;
            } else if (s.type == DSP_LOUDNESS) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["referenceLevelDb"] = s.loudness.referenceLevelDb;
                params["currentLevelDb"] = s.loudness.currentLevelDb;
                params["amount"] = s.loudness.amount;
            } else if (s.type == DSP_BASS_ENHANCE) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["frequency"] = s.bassEnhance.frequency;
                params["harmonicGainDb"] = s.bassEnhance.harmonicGainDb;
                params["mix"] = s.bassEnhance.mix;
                params["order"] = s.bassEnhance.order;
            } else if (s.type == DSP_MULTIBAND_COMP) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["numBands"] = s.multibandComp.numBands;
            }
        }
    }

    serializeJson(doc, buf, bufSize);
}

void dsp_import_full_config_json(const char *json) {
    if (!json) return;
    DspState *cfg = dsp_get_inactive_config();

    // Free all existing pool slots
    for (int c = 0; c < DSP_MAX_CHANNELS; c++) {
        for (int i = 0; i < cfg->channels[c].stageCount; i++) {
            if (cfg->channels[c].stages[i].type == DSP_FIR) {
                dsp_fir_free_slot(cfg->channels[c].stages[i].fir.firSlot);
            } else if (cfg->channels[c].stages[i].type == DSP_DELAY) {
                dsp_delay_free_slot(cfg->channels[c].stages[i].delay.delaySlot);
            } else if (cfg->channels[c].stages[i].type == DSP_DECIMATOR) {
                dsp_fir_free_slot(cfg->channels[c].stages[i].decimator.firSlot);
            } else if (cfg->channels[c].stages[i].type == DSP_CONVOLUTION && cfg->channels[c].stages[i].convolution.convSlot >= 0) {
                dsp_conv_free_slot(cfg->channels[c].stages[i].convolution.convSlot);
            } else if (cfg->channels[c].stages[i].type == DSP_MULTIBAND_COMP) {
                dsp_mb_free_slot(cfg->channels[c].stages[i].multibandComp.mbSlot);
            }
        }
    }

    JsonDocument doc;
    if (deserializeJson(doc, json)) return;

    if (doc["globalBypass"].is<bool>()) cfg->globalBypass = doc["globalBypass"].as<bool>();
    if (doc["sampleRate"].is<unsigned int>()) cfg->sampleRate = doc["sampleRate"].as<uint32_t>();

    if (doc["channels"].is<JsonArray>()) {
        JsonArray channels = doc["channels"].as<JsonArray>();
        int c = 0;
        for (JsonObject chObj : channels) {
            if (c >= DSP_MAX_CHANNELS) break;
            DspChannelConfig &ch = cfg->channels[c];
            if (chObj["bypass"].is<bool>()) ch.bypass = chObj["bypass"].as<bool>();
            if (chObj["stereoLink"].is<bool>()) ch.stereoLink = chObj["stereoLink"].as<bool>();
            ch.stageCount = 0;

            if (chObj["stages"].is<JsonArray>()) {
                JsonArray stages = chObj["stages"].as<JsonArray>();

                // Check if this channel has PEQ bands
                bool hasPeqBands = false;
                if (stages.size() >= (unsigned)DSP_PEQ_BANDS) {
                    JsonObject first = stages[0].as<JsonObject>();
                    const char *lbl = first["label"] | "";
                    hasPeqBands = (lbl[0] == 'P' && lbl[1] == 'E' && lbl[2] == 'Q');
                }

                if (!hasPeqBands) {
                    dsp_init_peq_bands(ch);
                    for (int b = 0; b < DSP_PEQ_BANDS; b++) {
                        dsp_compute_biquad_coeffs(ch.stages[b].biquad, ch.stages[b].type, cfg->sampleRate);
                    }
                }

                int loadIdx = hasPeqBands ? 0 : DSP_PEQ_BANDS;
                if (hasPeqBands) ch.stageCount = 0;

                for (JsonObject stageObj : stages) {
                    if (loadIdx >= DSP_MAX_STAGES) break;
                    DspStage &s = ch.stages[loadIdx];
                    DspStageType type = stage_type_from_name(stageObj["type"].as<const char *>());
                    dsp_init_stage(s, type);
                    if (stageObj["enabled"].is<bool>()) s.enabled = stageObj["enabled"].as<bool>();
                    if (stageObj["label"].is<const char *>()) {
                        strncpy(s.label, stageObj["label"].as<const char *>(), sizeof(s.label) - 1);
                        s.label[sizeof(s.label) - 1] = '\0';
                    }

                    JsonObject params = stageObj["params"];
                    if (dsp_is_biquad_type(type)) {
                        if (params["frequency"].is<float>()) s.biquad.frequency = params["frequency"].as<float>();
                        if (params["gain"].is<float>()) s.biquad.gain = params["gain"].as<float>();
                        if (params["Q"].is<float>()) s.biquad.Q = params["Q"].as<float>();
                        if (type == DSP_BIQUAD_CUSTOM && params["coeffs"].is<JsonArray>()) {
                            JsonArray co = params["coeffs"].as<JsonArray>();
                            for (int j = 0; j < 5 && j < (int)co.size(); j++)
                                s.biquad.coeffs[j] = co[j].as<float>();
                        } else {
                            dsp_compute_biquad_coeffs(s.biquad, type, cfg->sampleRate);
                        }
                    } else if (type == DSP_LIMITER) {
                        if (params["thresholdDb"].is<float>()) s.limiter.thresholdDb = params["thresholdDb"].as<float>();
                        if (params["attackMs"].is<float>()) s.limiter.attackMs = params["attackMs"].as<float>();
                        if (params["releaseMs"].is<float>()) s.limiter.releaseMs = params["releaseMs"].as<float>();
                        if (params["ratio"].is<float>()) s.limiter.ratio = params["ratio"].as<float>();
                    } else if (type == DSP_GAIN) {
                        if (params["gainDb"].is<float>()) s.gain.gainDb = params["gainDb"].as<float>();
                        dsp_compute_gain_linear(s.gain);
                    } else if (type == DSP_FIR) {
                        int slot = dsp_fir_alloc_slot();
                        if (slot < 0) { LOG_W("[DSP] Import: FIR slot alloc failed, skipping stage"); continue; }
                        s.fir.firSlot = (int8_t)slot;
                        if (params["numTaps"].is<int>()) s.fir.numTaps = params["numTaps"].as<uint16_t>();
                    } else if (type == DSP_DELAY) {
                        int slot = dsp_delay_alloc_slot();
                        if (slot < 0) { LOG_W("[DSP] Import: delay slot alloc failed, skipping stage"); continue; }
                        s.delay.delaySlot = (int8_t)slot;
                        if (params["delaySamples"].is<int>()) {
                            uint16_t ds = params["delaySamples"].as<uint16_t>();
                            s.delay.delaySamples = ds > DSP_MAX_DELAY_SAMPLES ? DSP_MAX_DELAY_SAMPLES : ds;
                        }
                    } else if (type == DSP_POLARITY) {
                        if (params["inverted"].is<bool>()) s.polarity.inverted = params["inverted"].as<bool>();
                    } else if (type == DSP_MUTE) {
                        if (params["muted"].is<bool>()) s.mute.muted = params["muted"].as<bool>();
                    } else if (type == DSP_COMPRESSOR) {
                        if (params["thresholdDb"].is<float>()) s.compressor.thresholdDb = params["thresholdDb"].as<float>();
                        if (params["attackMs"].is<float>()) s.compressor.attackMs = params["attackMs"].as<float>();
                        if (params["releaseMs"].is<float>()) s.compressor.releaseMs = params["releaseMs"].as<float>();
                        if (params["ratio"].is<float>()) s.compressor.ratio = params["ratio"].as<float>();
                        if (params["kneeDb"].is<float>()) s.compressor.kneeDb = params["kneeDb"].as<float>();
                        if (params["makeupGainDb"].is<float>()) s.compressor.makeupGainDb = params["makeupGainDb"].as<float>();
                        dsp_compute_compressor_makeup(s.compressor);
                    } else if (type == DSP_CONVOLUTION) {
                        s.convolution.convSlot = -1;
                        if (params["irLength"].is<int>()) s.convolution.irLength = params["irLength"].as<uint16_t>();
                        if (params["irFilename"].is<const char *>()) {
                            strncpy(s.convolution.irFilename, params["irFilename"].as<const char *>(), sizeof(s.convolution.irFilename) - 1);
                            s.convolution.irFilename[sizeof(s.convolution.irFilename) - 1] = '\0';
                        }
                    } else if (type == DSP_NOISE_GATE) {
                        if (params["thresholdDb"].is<float>()) s.noiseGate.thresholdDb = params["thresholdDb"].as<float>();
                        if (params["attackMs"].is<float>()) s.noiseGate.attackMs = params["attackMs"].as<float>();
                        if (params["holdMs"].is<float>()) s.noiseGate.holdMs = params["holdMs"].as<float>();
                        if (params["releaseMs"].is<float>()) s.noiseGate.releaseMs = params["releaseMs"].as<float>();
                        if (params["ratio"].is<float>()) s.noiseGate.ratio = params["ratio"].as<float>();
                        if (params["rangeDb"].is<float>()) s.noiseGate.rangeDb = params["rangeDb"].as<float>();
                    } else if (type == DSP_TONE_CTRL) {
                        if (params["bassGain"].is<float>()) s.toneCtrl.bassGain = params["bassGain"].as<float>();
                        if (params["midGain"].is<float>()) s.toneCtrl.midGain = params["midGain"].as<float>();
                        if (params["trebleGain"].is<float>()) s.toneCtrl.trebleGain = params["trebleGain"].as<float>();
                        dsp_compute_tone_ctrl_coeffs(s.toneCtrl, cfg->sampleRate);
                    } else if (type == DSP_SPEAKER_PROT) {
                        if (params["powerRatingW"].is<float>()) s.speakerProt.powerRatingW = params["powerRatingW"].as<float>();
                        if (params["impedanceOhms"].is<float>()) s.speakerProt.impedanceOhms = params["impedanceOhms"].as<float>();
                        if (params["thermalTauMs"].is<float>()) s.speakerProt.thermalTauMs = params["thermalTauMs"].as<float>();
                        if (params["excursionLimitMm"].is<float>()) s.speakerProt.excursionLimitMm = params["excursionLimitMm"].as<float>();
                        if (params["driverDiameterMm"].is<float>()) s.speakerProt.driverDiameterMm = params["driverDiameterMm"].as<float>();
                        if (params["maxTempC"].is<float>()) s.speakerProt.maxTempC = params["maxTempC"].as<float>();
                        dsp_compute_speaker_prot(s.speakerProt);
                    } else if (type == DSP_STEREO_WIDTH) {
                        if (params["width"].is<float>()) s.stereoWidth.width = params["width"].as<float>();
                        if (params["centerGainDb"].is<float>()) s.stereoWidth.centerGainDb = params["centerGainDb"].as<float>();
                        dsp_compute_stereo_width(s.stereoWidth);
                    } else if (type == DSP_LOUDNESS) {
                        if (params["referenceLevelDb"].is<float>()) s.loudness.referenceLevelDb = params["referenceLevelDb"].as<float>();
                        if (params["currentLevelDb"].is<float>()) s.loudness.currentLevelDb = params["currentLevelDb"].as<float>();
                        if (params["amount"].is<float>()) s.loudness.amount = params["amount"].as<float>();
                        dsp_compute_loudness_coeffs(s.loudness, cfg->sampleRate);
                    } else if (type == DSP_BASS_ENHANCE) {
                        if (params["frequency"].is<float>()) s.bassEnhance.frequency = params["frequency"].as<float>();
                        if (params["harmonicGainDb"].is<float>()) s.bassEnhance.harmonicGainDb = params["harmonicGainDb"].as<float>();
                        if (params["mix"].is<float>()) s.bassEnhance.mix = params["mix"].as<float>();
                        if (params["order"].is<int>()) s.bassEnhance.order = params["order"].as<uint8_t>();
                        dsp_compute_bass_enhance_coeffs(s.bassEnhance, cfg->sampleRate);
                    } else if (type == DSP_MULTIBAND_COMP) {
                        if (params["numBands"].is<int>()) s.multibandComp.numBands = params["numBands"].as<uint8_t>();
                        int slot = dsp_mb_alloc_slot();
                        if (slot < 0) { LOG_W("[DSP] Import: multiband slot alloc failed, skipping"); continue; }
                        s.multibandComp.mbSlot = (int8_t)slot;
                    }
                    loadIdx++;
                    ch.stageCount = loadIdx;
                }
            }
            c++;
        }
    }
}

#else
// Native test stubs for JSON functions
void dsp_export_config_to_json(int, char *, int) {}
void dsp_load_config_from_json(const char *, int) {}
void dsp_export_full_config_json(char *, int) {}
void dsp_import_full_config_json(const char *) {}
#endif // NATIVE_TEST

// ===== Zero Inactive Channels =====
// Clears post-DSP float buffers for an inactive input so the routing matrix
// doesn't multiply stale sample data into the DAC output.
void dsp_zero_channels(int adcIndex) {
    int chL = adcIndex * 2;
    int chR = adcIndex * 2 + 1;
#ifdef NATIVE_TEST
    if (chL < DSP_MAX_CHANNELS)
        memset(_postDspChannels[chL], 0, 256 * sizeof(float));
    if (chR < DSP_MAX_CHANNELS)
        memset(_postDspChannels[chR], 0, 256 * sizeof(float));
#else
    if (chL < DSP_MAX_CHANNELS && _postDspChannels[chL])
        memset(_postDspChannels[chL], 0, 256 * sizeof(float));
    if (chR < DSP_MAX_CHANNELS && _postDspChannels[chR])
        memset(_postDspChannels[chR], 0, 256 * sizeof(float));
#endif
}

// ===== Routing Matrix Execution =====
// Called from audio_capture_task after all inputs are processed through DSP.
// Applies the 6x6 routing matrix to post-DSP float channels,
// then re-interleaves output ch0/ch1 into dacBuf for DAC write.
void dsp_routing_execute(int32_t *dacBuf, int frames) {
    if (!dacBuf || frames <= 0 || _postDspFrames <= 0) return;
    int n = (frames < _postDspFrames) ? frames : _postDspFrames;

    DspRoutingMatrix *rm = dsp_get_routing_matrix();
    if (!rm) return;

    // Build channel pointer array
    float *channelPtrs[DSP_MAX_CHANNELS];
    for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
        channelPtrs[i] = _postDspChannels[i];
    }

    // Apply routing: 6x6 multiply-accumulate (modifies _postDspChannels in-place)
    dsp_routing_apply(*rm, channelPtrs, DSP_MAX_CHANNELS, n);

    // Re-interleave routed output ch0/ch1 → DAC buffer (left-justified int32)
    const float MAX_24BIT = 8388607.0f;
    for (int f = 0; f < n; f++) {
        float sL = _postDspChannels[0][f];
        float sR = _postDspChannels[1][f];
        if (sL > 1.0f) sL = 1.0f; else if (sL < -1.0f) sL = -1.0f;
        if (sR > 1.0f) sR = 1.0f; else if (sR < -1.0f) sR = -1.0f;
        dacBuf[f * 2] = (int32_t)(sL * MAX_24BIT) << 8;
        dacBuf[f * 2 + 1] = (int32_t)(sR * MAX_24BIT) << 8;
    }
}

#endif // DSP_ENABLED
