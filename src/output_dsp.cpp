#ifdef DSP_ENABLED

#include "output_dsp.h"
#include "dsp_coefficients.h"
#include "dsp_biquad_gen.h"
#include "dsps_biquad.h"
#include "dsps_mulc.h"
#include "dsps_mul.h"
#include "audio_pipeline.h"
#include "app_state.h"
#include <math.h>
#include <string.h>

#ifndef NATIVE_TEST
#include "debug_serial.h"
#include "diag_journal.h"
#include "psram_alloc.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#else
#define LOG_I(...)
#define LOG_W(...)
#define LOG_E(...)
#endif

// ===== Double-buffered State (PSRAM on ESP32, static on native) =====
#ifdef NATIVE_TEST
static OutputDspState _states[2];
#else
static OutputDspState *_states = nullptr;
#endif
static volatile int _activeIndex = 0;

// ===== Swap Synchronization =====
#ifndef NATIVE_TEST
static SemaphoreHandle_t _swapMutex = NULL;
#endif
static volatile bool _swapRequested = false;

// ===== Gain buffer for 2-pass limiter/compressor (no dynamic alloc in process) =====
#ifdef NATIVE_TEST
static float _outGainBuf[256];
#else
static float *_outGainBuf = nullptr;
#endif

// ===== Per-channel delay circular buffers (PSRAM-allocated on demand) =====
// One buffer per output channel. Allocated when a DSP_DELAY stage is first created.
// writePos per stage is stored in DspDelayParams.writePos.
#ifdef NATIVE_TEST
static float _outDelayBuf[OUTPUT_DSP_MAX_CHANNELS][OUTPUT_DSP_MAX_DELAY_SAMPLES];
static bool _outDelayBufAlloc[OUTPUT_DSP_MAX_CHANNELS];
#else
static float *_outDelayBuf[OUTPUT_DSP_MAX_CHANNELS];
static bool _outDelayBufAlloc[OUTPUT_DSP_MAX_CHANNELS];
#endif

// ===== Forward Declarations =====
static void output_dsp_limiter_process(DspLimiterParams &lim, float *buf, int len, uint32_t sampleRate);
static void output_dsp_gain_process(DspGainParams &gain, float *buf, int len, uint32_t sampleRate);
static void output_dsp_compressor_process(DspCompressorParams &comp, float *buf, int len, uint32_t sampleRate);
static void output_dsp_delay_process(DspDelayParams &d, float *delayBuf, float *buf, int frames);

// ===== Local stage type helpers (reuse stage_type_name from dsp_pipeline.h for serialization) =====

static DspStageType output_dsp_type_from_name(const char *name) {
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
    if (strcmp(name, "GAIN") == 0) return DSP_GAIN;
    if (strcmp(name, "POLARITY") == 0) return DSP_POLARITY;
    if (strcmp(name, "MUTE") == 0) return DSP_MUTE;
    if (strcmp(name, "COMPRESSOR") == 0) return DSP_COMPRESSOR;
    if (strcmp(name, "DELAY") == 0) return DSP_DELAY;
    if (strcmp(name, "LPF_1ST") == 0) return DSP_BIQUAD_LPF_1ST;
    if (strcmp(name, "HPF_1ST") == 0) return DSP_BIQUAD_HPF_1ST;
    if (strcmp(name, "LINKWITZ") == 0) return DSP_BIQUAD_LINKWITZ;
    return DSP_BIQUAD_PEQ;
}

// ===== Coefficient computation helpers =====

static void output_dsp_compute_gain_linear(DspGainParams &g) {
    g.gainLinear = dsp_db_to_linear(g.gainDb);
}

static void output_dsp_compute_compressor_makeup(DspCompressorParams &c) {
    c.makeupLinear = dsp_db_to_linear(c.makeupGainDb);
}

// ===== Initialization =====

void output_dsp_init() {
#ifndef NATIVE_TEST
    // Allocate double-buffered state from PSRAM
    if (!_states) {
        _states = (OutputDspState *)psram_alloc(2, sizeof(OutputDspState), "outdsp_states");
    }
    // Allocate gain buffer for 2-pass limiter/compressor
    if (!_outGainBuf) {
        _outGainBuf = (float *)psram_alloc(256, sizeof(float), "outdsp_buf");
    }
    // Delay buffers are allocated on demand (output_dsp_alloc_delay_buf)
    memset(_outDelayBuf, 0, sizeof(_outDelayBuf));
#endif
    memset(_outDelayBufAlloc, 0, sizeof(_outDelayBufAlloc));

    output_dsp_init_state(_states[0]);
    output_dsp_init_state(_states[1]);
    _activeIndex = 0;

#ifndef NATIVE_TEST
    if (!_swapMutex) {
        _swapMutex = xSemaphoreCreateMutex();
    }
#endif
    _swapRequested = false;

    LOG_I("[OutputDSP] Initialized (double-buffered, %d channels, max %d stages/ch)",
          OUTPUT_DSP_MAX_CHANNELS, OUTPUT_DSP_MAX_STAGES);
}

// ===== Config Access =====

OutputDspState* output_dsp_get_active_config() {
    return &_states[_activeIndex];
}

OutputDspState* output_dsp_get_inactive_config() {
    return &_states[1 - _activeIndex];
}

void output_dsp_copy_active_to_inactive() {
    int activeIdx = _activeIndex;
    int inactiveIdx = 1 - activeIdx;
    _states[inactiveIdx] = _states[activeIdx];
}

// ===== Swap =====

bool output_dsp_swap_config() {
#ifndef NATIVE_TEST
    if (_swapMutex && xSemaphoreTake(_swapMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        LOG_W("[OutputDSP] Swap failed: mutex busy");
        return false;
    }
#endif

    int oldActive = _activeIndex;
    int newActive = 1 - oldActive;

    // Notify pipeline of impending swap (reuses same hold-buffer mechanism)
    audio_pipeline_notify_dsp_swap();

    _swapRequested = true;

    // Brief wait for audio task to see the flag (output DSP runs inside audio pipeline,
    // so we just need a few ms for the current iteration to finish)
#ifndef NATIVE_TEST
    int waitCount = 0;
    while (_swapRequested && waitCount < 50) {
        vTaskDelay(1);
        waitCount++;
    }
#endif

    // Copy runtime state (delay lines, envelopes) from old active to new active
    for (int ch = 0; ch < OUTPUT_DSP_MAX_CHANNELS; ch++) {
        OutputDspChannelConfig &oldCh = _states[oldActive].channels[ch];
        OutputDspChannelConfig &newCh = _states[newActive].channels[ch];

        int minStages = oldCh.stageCount < newCh.stageCount ? oldCh.stageCount : newCh.stageCount;
        for (int s = 0; s < minStages; s++) {
            OutputDspStage &oldS = oldCh.stages[s];
            OutputDspStage &newS = newCh.stages[s];

            if (oldS.type != newS.type) continue;

            if (dsp_is_biquad_type(newS.type)) {
                // Copy delay lines for continuity
                newS.biquad.delay[0] = oldS.biquad.delay[0];
                newS.biquad.delay[1] = oldS.biquad.delay[1];
                // Detect coefficient changes for glitch-free morphing
                bool coeffChanged = false;
                for (int c = 0; c < 5; c++) {
                    if (newS.biquad.coeffs[c] != oldS.biquad.coeffs[c]) {
                        coeffChanged = true;
                        break;
                    }
                }
                if (coeffChanged) {
                    for (int c = 0; c < 5; c++) {
                        newS.biquad.targetCoeffs[c] = newS.biquad.coeffs[c];
                        newS.biquad.coeffs[c] = oldS.biquad.coeffs[c];
                    }
                    newS.biquad.morphRemaining = 64; // ~1.3ms at 48kHz
                } else {
                    newS.biquad.morphRemaining = 0;
                }
            } else if (newS.type == DSP_LIMITER) {
                newS.limiter.envelope = oldS.limiter.envelope;
                newS.limiter.gainReduction = oldS.limiter.gainReduction;
            } else if (newS.type == DSP_GAIN) {
                newS.gain.currentLinear = oldS.gain.currentLinear;
            } else if (newS.type == DSP_COMPRESSOR) {
                newS.compressor.envelope = oldS.compressor.envelope;
                newS.compressor.gainReduction = oldS.compressor.gainReduction;
            }
            // DSP_POLARITY and DSP_MUTE have no runtime state to preserve
        }
    }

    // Atomic swap
    _activeIndex = newActive;
    _swapRequested = false;

#ifndef NATIVE_TEST
    if (_swapMutex) xSemaphoreGive(_swapMutex);
#endif

    LOG_I("[OutputDSP] Config swapped (active=%d)", newActive);
    return true;
}

// ===== Processing =====

void output_dsp_process(int ch, float *buf, int frames) {
    if (ch < 0 || ch >= OUTPUT_DSP_MAX_CHANNELS || !buf || frames <= 0 || frames > 256) return;

    OutputDspState *cfg = &_states[_activeIndex];

    // Global bypass — skip all processing
    if (cfg->globalBypass) return;

    OutputDspChannelConfig &channel = cfg->channels[ch];

    // Per-channel bypass
    if (channel.bypass) return;

    uint32_t sampleRate = cfg->sampleRate;

    for (int i = 0; i < channel.stageCount; i++) {
        OutputDspStage &s = channel.stages[i];
        if (!s.enabled) continue;

        if (dsp_is_biquad_type(s.type)) {
            if (s.biquad.morphRemaining > 0) {
                // Coefficient morphing for glitch-free transitions
                int remaining = s.biquad.morphRemaining;
                int processed = 0;
                while (processed < frames && remaining > 0) {
                    float t = 1.0f - (float)remaining / 64.0f;
                    float interpCoeffs[5];
                    for (int c = 0; c < 5; c++) {
                        interpCoeffs[c] = s.biquad.coeffs[c] + t * (s.biquad.targetCoeffs[c] - s.biquad.coeffs[c]);
                    }
                    int chunk = frames - processed;
                    if (chunk > 8) chunk = 8;
                    if (chunk > remaining) chunk = remaining;
                    dsps_biquad_f32(buf + processed, buf + processed, chunk, interpCoeffs, s.biquad.delay);
                    processed += chunk;
                    remaining -= chunk;
                }
                if (remaining <= 0) {
                    for (int c = 0; c < 5; c++) {
                        s.biquad.coeffs[c] = s.biquad.targetCoeffs[c];
                    }
                    s.biquad.morphRemaining = 0;
                    if (processed < frames) {
                        dsps_biquad_f32(buf + processed, buf + processed, frames - processed, s.biquad.coeffs, s.biquad.delay);
                    }
                } else {
                    s.biquad.morphRemaining = (uint16_t)remaining;
                }
            } else {
                dsps_biquad_f32(buf, buf, frames, s.biquad.coeffs, s.biquad.delay);
            }
            continue;
        }

        switch (s.type) {
            case DSP_LIMITER:
                output_dsp_limiter_process(s.limiter, buf, frames, sampleRate);
                break;
            case DSP_GAIN:
                output_dsp_gain_process(s.gain, buf, frames, sampleRate);
                break;
            case DSP_POLARITY:
                if (s.polarity.inverted) {
                    dsps_mulc_f32(buf, buf, frames, -1.0f, 1, 1);
                }
                break;
            case DSP_MUTE:
                if (s.mute.muted) {
                    memset(buf, 0, frames * sizeof(float));
                }
                break;
            case DSP_COMPRESSOR:
                output_dsp_compressor_process(s.compressor, buf, frames, sampleRate);
                break;
            case DSP_DELAY:
                if (_outDelayBufAlloc[ch]) {
                    output_dsp_delay_process(s.delay, _outDelayBuf[ch], buf, frames);
                }
                break;
            default:
                break;
        }
    }
}

// ===== Delay (circular buffer, per-channel PSRAM buffer) =====

static void output_dsp_delay_process(DspDelayParams &d, float *delayBuf, float *buf, int frames) {
    uint16_t delaySamples = d.delaySamples;
    if (delaySamples == 0) return;  // 0ms delay — pass through unchanged
    if (delaySamples > OUTPUT_DSP_MAX_DELAY_SAMPLES) delaySamples = OUTPUT_DSP_MAX_DELAY_SAMPLES;

    uint16_t wp = d.writePos;
    for (int i = 0; i < frames; i++) {
        // Read delayed sample from circular buffer
        uint16_t rp = (wp >= delaySamples) ? (uint16_t)(wp - delaySamples)
                                            : (uint16_t)(OUTPUT_DSP_MAX_DELAY_SAMPLES + wp - delaySamples);
        float delayed = delayBuf[rp];
        // Write current sample into buffer
        delayBuf[wp] = buf[i];
        buf[i] = delayed;
        if (++wp >= OUTPUT_DSP_MAX_DELAY_SAMPLES) wp = 0;
    }
    d.writePos = wp;
}

// ===== Limiter (2-pass: envelope detection + gain application) =====

static void output_dsp_limiter_process(DspLimiterParams &lim, float *buf, int len, uint32_t sampleRate) {
    if (len <= 0 || sampleRate == 0) return;

    float threshLin = dsp_db_to_linear(lim.thresholdDb);
    float attackCoeff = dsp_time_coeff(lim.attackMs, (float)sampleRate);
    float releaseCoeff = dsp_time_coeff(lim.releaseMs, (float)sampleRate);

    float env = lim.envelope;
    float maxGr = 0.0f;

    // Pass 1: Envelope detection -> gain buffer
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

        _outGainBuf[i] = gainLin;
    }

    // Pass 2: Apply gain via SIMD element-wise multiply
    dsps_mul_f32(buf, _outGainBuf, buf, len, 1, 1, 1);

    lim.envelope = env;
    lim.gainReduction = -maxGr;
}

// ===== Gain (with exponential ramp for glitch-free transitions) =====

static void output_dsp_gain_process(DspGainParams &gain, float *buf, int len, uint32_t sampleRate) {
    float target = gain.gainLinear;
    float current = gain.currentLinear;

    // If already at target, use fast SIMD path
    float diff = fabsf(current - target);
    if (diff < 1e-6f) {
        gain.currentLinear = target;
        dsps_mulc_f32(buf, buf, len, target, 1, 1);
        return;
    }

    // Exponential ramp: ~5ms time constant
    float tau = 5.0f; // ms
    float coeff = dsp_time_coeff(tau, (float)sampleRate);
    float oneMinusCoeff = 1.0f - coeff;

    for (int i = 0; i < len; i++) {
        current = coeff * current + oneMinusCoeff * target;
        buf[i] *= current;
    }
    gain.currentLinear = current;
}

// ===== Compressor (2-pass with soft knee) =====

static void output_dsp_compressor_process(DspCompressorParams &comp, float *buf, int len, uint32_t sampleRate) {
    if (len <= 0 || sampleRate == 0) return;

    float attackCoeff = dsp_time_coeff(comp.attackMs, (float)sampleRate);
    float releaseCoeff = dsp_time_coeff(comp.releaseMs, (float)sampleRate);
    float makeupLin = comp.makeupLinear;

    float env = comp.envelope;
    float maxGr = 0.0f;

    // Pass 1: Envelope detection -> gain buffer (includes makeup gain)
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
                // Soft knee region
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

        _outGainBuf[i] = gainLin * makeupLin;
    }

    // Pass 2: Apply gain via SIMD element-wise multiply
    dsps_mul_f32(buf, _outGainBuf, buf, len, 1, 1, 1);

    comp.envelope = env;
    comp.gainReduction = -maxGr;
}

// ===== Delay Buffer Management =====

// Allocate the per-channel delay circular buffer on demand.
// Returns true if the buffer is ready (already allocated or just allocated).
static bool output_dsp_alloc_delay_buf(int channel) {
    if (channel < 0 || channel >= OUTPUT_DSP_MAX_CHANNELS) return false;
    if (_outDelayBufAlloc[channel]) return true;
#ifdef NATIVE_TEST
    memset(_outDelayBuf[channel], 0, sizeof(_outDelayBuf[channel]));
#else
    _outDelayBuf[channel] = (float *)psram_alloc(OUTPUT_DSP_MAX_DELAY_SAMPLES, sizeof(float), "outdsp_delay");
    if (!_outDelayBuf[channel]) {
        LOG_W("[OutputDSP] Failed to allocate delay buffer for ch=%d", channel);
        return false;
    }
    memset(_outDelayBuf[channel], 0, OUTPUT_DSP_MAX_DELAY_SAMPLES * sizeof(float));
#endif
    _outDelayBufAlloc[channel] = true;
    return true;
}

// ===== Stage CRUD =====

int output_dsp_add_stage(int channel, DspStageType type, int position) {
    if (channel < 0 || channel >= OUTPUT_DSP_MAX_CHANNELS) return -1;

    OutputDspState *cfg = output_dsp_get_inactive_config();
    OutputDspChannelConfig &ch = cfg->channels[channel];
    if (ch.stageCount >= OUTPUT_DSP_MAX_STAGES) return -1;

    // Validate supported types for output DSP (no FIR, decimator, convolution, etc.)
    if (!dsp_is_biquad_type(type) &&
        type != DSP_LIMITER && type != DSP_GAIN && type != DSP_POLARITY &&
        type != DSP_MUTE && type != DSP_COMPRESSOR && type != DSP_DELAY) {
        LOG_W("[OutputDSP] Unsupported stage type %d for output DSP", (int)type);
        return -1;
    }
    // Delay requires a buffer — allocate on demand
    if (type == DSP_DELAY && !output_dsp_alloc_delay_buf(channel)) {
        LOG_W("[OutputDSP] Cannot add delay stage for ch=%d — buffer allocation failed", channel);
        return -1;
    }

    int pos = position;
    if (pos < 0 || pos > ch.stageCount) pos = ch.stageCount; // Append

    // Shift stages to make room
    for (int i = ch.stageCount; i > pos; i--) {
        ch.stages[i] = ch.stages[i - 1];
    }

    output_dsp_init_stage(ch.stages[pos], type);
    ch.stageCount++;

    // Compute coefficients for new stage
    if (dsp_is_biquad_type(type)) {
        dsp_compute_biquad_coeffs(ch.stages[pos].biquad, type, cfg->sampleRate);
    } else if (type == DSP_GAIN) {
        output_dsp_compute_gain_linear(ch.stages[pos].gain);
    } else if (type == DSP_COMPRESSOR) {
        output_dsp_compute_compressor_makeup(ch.stages[pos].compressor);
    }

    return pos;
}

bool output_dsp_remove_stage(int channel, int stageIndex) {
    if (channel < 0 || channel >= OUTPUT_DSP_MAX_CHANNELS) return false;

    OutputDspState *cfg = output_dsp_get_inactive_config();
    OutputDspChannelConfig &ch = cfg->channels[channel];
    if (stageIndex < 0 || stageIndex >= ch.stageCount) return false;

    // No pool slots to free (output DSP stages are self-contained)

    // Shift stages down
    for (int i = stageIndex; i < ch.stageCount - 1; i++) {
        ch.stages[i] = ch.stages[i + 1];
    }
    ch.stageCount--;
    return true;
}

bool output_dsp_set_stage_enabled(int channel, int stageIndex, bool enabled) {
    if (channel < 0 || channel >= OUTPUT_DSP_MAX_CHANNELS) return false;

    OutputDspState *cfg = output_dsp_get_inactive_config();
    OutputDspChannelConfig &ch = cfg->channels[channel];
    if (stageIndex < 0 || stageIndex >= ch.stageCount) return false;

    ch.stages[stageIndex].enabled = enabled;
    return true;
}

// ===== Crossover Convenience =====

// Remove all stages with labels starting with "XO " from a channel
static void output_dsp_clear_crossover(OutputDspChannelConfig &ch) {
    int i = 0;
    while (i < ch.stageCount) {
        if (ch.stages[i].label[0] == 'X' && ch.stages[i].label[1] == 'O' && ch.stages[i].label[2] == ' ') {
            // Shift stages down
            for (int j = i; j < ch.stageCount - 1; j++) {
                ch.stages[j] = ch.stages[j + 1];
            }
            ch.stageCount--;
        } else {
            i++;
        }
    }
}

int output_dsp_setup_crossover(int subCh, int mainCh, float freqHz, int order) {
    if (subCh < 0 || subCh >= OUTPUT_DSP_MAX_CHANNELS) return -1;
    if (mainCh < 0 || mainCh >= OUTPUT_DSP_MAX_CHANNELS) return -1;
    if (freqHz <= 0.0f) return -1;
    if (order != 2 && order != 4 && order != 8) return -1;

    OutputDspState *cfg = output_dsp_get_inactive_config();
    uint32_t sampleRate = cfg->sampleRate;
    if (sampleRate == 0) return -1;

    float normFreq = freqHz / (float)sampleRate;
    if (normFreq <= 0.0f || normFreq >= 0.5f) return -1;

    // Clear existing crossover stages on both channels
    output_dsp_clear_crossover(cfg->channels[subCh]);
    output_dsp_clear_crossover(cfg->channels[mainCh]);

    // Linkwitz-Riley crossover = Butterworth squared
    // LR2 = 1 biquad section (Q=0.5)
    // LR4 = 2 identical Butterworth biquad sections (Q=0.707)
    // LR8 = 4 identical Butterworth biquad sections (Q=0.707)
    int numSections;
    float Q;

    switch (order) {
        case 2:  numSections = 1; Q = 0.5f;     break;
        case 4:  numSections = 2; Q = 0.7071f;   break;
        case 8:  numSections = 4; Q = 0.7071f;   break;
        default: return -1;
    }

    // Check space on both channels
    OutputDspChannelConfig &subConfig = cfg->channels[subCh];
    OutputDspChannelConfig &mainConfig = cfg->channels[mainCh];
    if (subConfig.stageCount + numSections > OUTPUT_DSP_MAX_STAGES) return -1;
    if (mainConfig.stageCount + numSections > OUTPUT_DSP_MAX_STAGES) return -1;

    // Compute LPF coefficients for sub channel
    float lpfCoeffs[5];
    dsp_gen_lpf_f32(lpfCoeffs, normFreq, Q);

    // Compute HPF coefficients for main channel
    float hpfCoeffs[5];
    dsp_gen_hpf_f32(hpfCoeffs, normFreq, Q);

    int stagesAdded = 0;

    // Insert LPF biquads at the beginning of sub channel
    for (int s = 0; s < numSections; s++) {
        int pos = s; // Insert at beginning, after previously inserted sections

        // Shift existing stages to make room
        if (subConfig.stageCount + 1 > OUTPUT_DSP_MAX_STAGES) break;
        for (int i = subConfig.stageCount; i > pos; i--) {
            subConfig.stages[i] = subConfig.stages[i - 1];
        }

        OutputDspStage &stage = subConfig.stages[pos];
        output_dsp_init_stage(stage, DSP_BIQUAD_LPF);
        stage.biquad.frequency = freqHz;
        stage.biquad.Q = Q;
        memcpy(stage.biquad.coeffs, lpfCoeffs, sizeof(float) * 5);
        stage.biquad.delay[0] = 0.0f;
        stage.biquad.delay[1] = 0.0f;

        // Label: "XO LPF N"
        stage.label[0] = 'X'; stage.label[1] = 'O'; stage.label[2] = ' ';
        stage.label[3] = 'L'; stage.label[4] = 'P'; stage.label[5] = 'F';
        stage.label[6] = ' '; stage.label[7] = '1' + s; stage.label[8] = '\0';

        subConfig.stageCount++;
        stagesAdded++;
    }

    // Insert HPF biquads at the beginning of main channel
    for (int s = 0; s < numSections; s++) {
        int pos = s;

        if (mainConfig.stageCount + 1 > OUTPUT_DSP_MAX_STAGES) break;
        for (int i = mainConfig.stageCount; i > pos; i--) {
            mainConfig.stages[i] = mainConfig.stages[i - 1];
        }

        OutputDspStage &stage = mainConfig.stages[pos];
        output_dsp_init_stage(stage, DSP_BIQUAD_HPF);
        stage.biquad.frequency = freqHz;
        stage.biquad.Q = Q;
        memcpy(stage.biquad.coeffs, hpfCoeffs, sizeof(float) * 5);
        stage.biquad.delay[0] = 0.0f;
        stage.biquad.delay[1] = 0.0f;

        // Label: "XO HPF N"
        stage.label[0] = 'X'; stage.label[1] = 'O'; stage.label[2] = ' ';
        stage.label[3] = 'H'; stage.label[4] = 'P'; stage.label[5] = 'F';
        stage.label[6] = ' '; stage.label[7] = '1' + s; stage.label[8] = '\0';

        mainConfig.stageCount++;
        stagesAdded++;
    }

    LOG_I("[OutputDSP] Crossover: LR%d at %.0f Hz, sub=ch%d(%d LPF), main=ch%d(%d HPF)",
          order, freqHz, subCh, numSections, mainCh, numSections);

    return stagesAdded;
}

// ===== Persistence =====

#ifndef NATIVE_TEST

void output_dsp_save_channel(int ch) {
    if (ch < 0 || ch >= OUTPUT_DSP_MAX_CHANNELS) return;

    OutputDspState *cfg = output_dsp_get_active_config();
    OutputDspChannelConfig &channel = cfg->channels[ch];

    JsonDocument doc;
    doc["bypass"] = channel.bypass;
    JsonArray stages = doc["stages"].to<JsonArray>();

    for (int i = 0; i < channel.stageCount; i++) {
        OutputDspStage &s = channel.stages[i];
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
        } else if (s.type == DSP_DELAY) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["delaySamples"] = s.delay.delaySamples;
        }
    }

    char path[32];
    snprintf(path, sizeof(path), "/output_dsp_ch%d.json", ch);

    File f = LittleFS.open(path, "w");
    if (!f) {
        LOG_E("[OutputDSP] Failed to open %s for write", path);
        return;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[OutputDSP] Saved ch%d (%d stages) to %s", ch, channel.stageCount, path);
}

void output_dsp_load_channel(int ch) {
    if (ch < 0 || ch >= OUTPUT_DSP_MAX_CHANNELS) return;

    char path[32];
    snprintf(path, sizeof(path), "/output_dsp_ch%d.json", ch);

    File f = LittleFS.open(path, "r");
    if (!f) {
        LOG_I("[OutputDSP] No saved config for ch%d", ch);
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        LOG_E("[OutputDSP] JSON parse error for ch%d: %s", ch, err.c_str());
        return;
    }

    // Load into active config directly (called at startup before audio starts)
    OutputDspState *cfg = output_dsp_get_active_config();
    OutputDspChannelConfig &channel = cfg->channels[ch];

    channel.bypass = doc["bypass"] | true;
    channel.stageCount = 0;

    JsonArray stages = doc["stages"];
    if (!stages) return;

    for (JsonObject stageObj : stages) {
        if (channel.stageCount >= OUTPUT_DSP_MAX_STAGES) break;

        const char *typeName = stageObj["type"] | "PEQ";
        DspStageType type = output_dsp_type_from_name(typeName);

        // Validate type is supported
        if (!dsp_is_biquad_type(type) &&
            type != DSP_LIMITER && type != DSP_GAIN && type != DSP_POLARITY &&
            type != DSP_MUTE && type != DSP_COMPRESSOR && type != DSP_DELAY) {
            LOG_W("[OutputDSP] Skipping unsupported type '%s' in ch%d config", typeName, ch);
            continue;
        }
        // Delay stages need a buffer allocated before they can process
        if (type == DSP_DELAY && !output_dsp_alloc_delay_buf(ch)) {
            LOG_W("[OutputDSP] Skipping DSP_DELAY for ch%d — buffer allocation failed", ch);
            continue;
        }

        int idx = channel.stageCount;
        output_dsp_init_stage(channel.stages[idx], type);
        channel.stages[idx].enabled = stageObj["enabled"] | true;

        // Copy label
        const char *label = stageObj["label"] | "";
        strncpy(channel.stages[idx].label, label, sizeof(channel.stages[idx].label) - 1);
        channel.stages[idx].label[sizeof(channel.stages[idx].label) - 1] = '\0';

        JsonObject params = stageObj["params"];
        if (params) {
            OutputDspStage &s = channel.stages[idx];

            if (dsp_is_biquad_type(type)) {
                s.biquad.frequency = params["frequency"] | 1000.0f;
                s.biquad.gain = params["gain"] | 0.0f;
                s.biquad.Q = params["Q"] | 0.707f;
                if (type == DSP_BIQUAD_LINKWITZ) {
                    s.biquad.Q2 = params["Q2"] | 0.707f;
                }
                if (type == DSP_BIQUAD_CUSTOM) {
                    JsonArray coeffArr = params["coeffs"];
                    if (coeffArr && coeffArr.size() == 5) {
                        for (int j = 0; j < 5; j++) s.biquad.coeffs[j] = coeffArr[j];
                    }
                } else {
                    // Recompute coefficients from parameters
                    dsp_compute_biquad_coeffs(s.biquad, type, cfg->sampleRate);
                }
            } else if (type == DSP_LIMITER) {
                s.limiter.thresholdDb = params["thresholdDb"] | 0.0f;
                s.limiter.attackMs = params["attackMs"] | 5.0f;
                s.limiter.releaseMs = params["releaseMs"] | 50.0f;
                s.limiter.ratio = params["ratio"] | 20.0f;
                s.limiter.envelope = 0.0f;
                s.limiter.gainReduction = 0.0f;
            } else if (type == DSP_GAIN) {
                s.gain.gainDb = params["gainDb"] | 0.0f;
                output_dsp_compute_gain_linear(s.gain);
                s.gain.currentLinear = s.gain.gainLinear; // No ramp on load
            } else if (type == DSP_POLARITY) {
                s.polarity.inverted = params["inverted"] | true;
            } else if (type == DSP_MUTE) {
                s.mute.muted = params["muted"] | true;
            } else if (type == DSP_COMPRESSOR) {
                s.compressor.thresholdDb = params["thresholdDb"] | -12.0f;
                s.compressor.attackMs = params["attackMs"] | 10.0f;
                s.compressor.releaseMs = params["releaseMs"] | 100.0f;
                s.compressor.ratio = params["ratio"] | 4.0f;
                s.compressor.kneeDb = params["kneeDb"] | 6.0f;
                s.compressor.makeupGainDb = params["makeupGainDb"] | 0.0f;
                output_dsp_compute_compressor_makeup(s.compressor);
                s.compressor.envelope = 0.0f;
                s.compressor.gainReduction = 0.0f;
            } else if (type == DSP_DELAY) {
                s.delay.delaySamples = params["delaySamples"] | (uint16_t)0;
                if (s.delay.delaySamples > OUTPUT_DSP_MAX_DELAY_SAMPLES)
                    s.delay.delaySamples = OUTPUT_DSP_MAX_DELAY_SAMPLES;
                s.delay.writePos = 0;
                s.delay.delaySlot = -1;  // Not used in output DSP (inline buffer)
            }
        }

        channel.stageCount++;
    }

    LOG_I("[OutputDSP] Loaded ch%d: %d stages, bypass=%d", ch, channel.stageCount, channel.bypass);
}

void output_dsp_save_all() {
    for (int ch = 0; ch < OUTPUT_DSP_MAX_CHANNELS; ch++) {
        output_dsp_save_channel(ch);
    }
}

void output_dsp_load_all() {
    for (int ch = 0; ch < OUTPUT_DSP_MAX_CHANNELS; ch++) {
        output_dsp_load_channel(ch);
    }
}

#else
// Native test stubs — no filesystem
void output_dsp_save_channel(int ch) { (void)ch; }
void output_dsp_load_channel(int ch) { (void)ch; }
void output_dsp_save_all() {}
void output_dsp_load_all() {}
#endif // NATIVE_TEST

#endif // DSP_ENABLED
