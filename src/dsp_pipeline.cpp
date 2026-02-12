#ifdef DSP_ENABLED

#include "dsp_pipeline.h"
#include "dsp_coefficients.h"
#include "dsps_biquad.h"
#include "dsps_fir.h"
#include <math.h>
#include <string.h>

#ifndef NATIVE_TEST
#include "app_state.h"
#include "debug_serial.h"
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

// ===== Double-buffered State =====
static DspState _states[2];
static volatile int _activeIndex = 0;
static DspMetrics _metrics;

// ===== FIR Data Pool (separate from DspStage union to save DRAM) =====
// Each slot: taps[256] + delay[256] = 2KB. 2 states × 2 slots × 2KB = 8KB total
static float _firTaps[2][DSP_MAX_FIR_SLOTS][DSP_MAX_FIR_TAPS];
static float _firDelay[2][DSP_MAX_FIR_SLOTS][DSP_MAX_FIR_TAPS];
static bool _firSlotUsed[DSP_MAX_FIR_SLOTS];

// ===== Conversion Buffers (static to avoid stack allocation) =====
static float _dspBufL[256];
static float _dspBufR[256];

// ===== Forward Declarations =====
static void dsp_process_channel(float *buf, int len, DspChannelConfig &ch, int stateIdx);
static void dsp_limiter_process(DspLimiterParams &lim, float *buf, int len, uint32_t sampleRate);
static void dsp_gain_process(DspGainParams &gain, float *buf, int len);
static void dsp_fir_process(DspFirParams &fir, float *buf, int len, int stateIdx);

// ===== FIR Pool Management =====

int dsp_fir_alloc_slot() {
    for (int i = 0; i < DSP_MAX_FIR_SLOTS; i++) {
        if (!_firSlotUsed[i]) {
            _firSlotUsed[i] = true;
            // Zero both states' data for this slot
            memset(_firTaps[0][i], 0, sizeof(float) * DSP_MAX_FIR_TAPS);
            memset(_firTaps[1][i], 0, sizeof(float) * DSP_MAX_FIR_TAPS);
            memset(_firDelay[0][i], 0, sizeof(float) * DSP_MAX_FIR_TAPS);
            memset(_firDelay[1][i], 0, sizeof(float) * DSP_MAX_FIR_TAPS);
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
    return _firTaps[stateIndex][firSlot];
}

float* dsp_fir_get_delay(int stateIndex, int firSlot) {
    if (stateIndex < 0 || stateIndex > 1 || firSlot < 0 || firSlot >= DSP_MAX_FIR_SLOTS)
        return nullptr;
    return _firDelay[stateIndex][firSlot];
}

// ===== Initialization =====

void dsp_init() {
    dsp_init_state(_states[0]);
    dsp_init_state(_states[1]);
    dsp_init_metrics(_metrics);
    _activeIndex = 0;

    // Clear FIR pool
    memset(_firTaps, 0, sizeof(_firTaps));
    memset(_firDelay, 0, sizeof(_firDelay));
    memset(_firSlotUsed, 0, sizeof(_firSlotUsed));

    LOG_I("[DSP] Pipeline initialized (double-buffered, %d channels, max %d stages/ch, %d FIR slots)",
          DSP_MAX_CHANNELS, DSP_MAX_STAGES, DSP_MAX_FIR_SLOTS);
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
            memcpy(_firTaps[inactiveIdx][s], _firTaps[activeIdx][s],
                   sizeof(float) * DSP_MAX_FIR_TAPS);
            memcpy(_firDelay[inactiveIdx][s], _firDelay[activeIdx][s],
                   sizeof(float) * DSP_MAX_FIR_TAPS);
        }
    }
}

void dsp_swap_config() {
    int oldActive = _activeIndex;
    int newActive = 1 - oldActive;

    // Copy delay lines from old active → new active to avoid audio discontinuity
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        DspChannelConfig &oldCh = _states[oldActive].channels[ch];
        DspChannelConfig &newCh = _states[newActive].channels[ch];

        int minStages = oldCh.stageCount < newCh.stageCount ? oldCh.stageCount : newCh.stageCount;
        for (int s = 0; s < minStages; s++) {
            DspStage &oldS = oldCh.stages[s];
            DspStage &newS = newCh.stages[s];

            if (oldS.type == newS.type) {
                if (newS.type <= DSP_BIQUAD_CUSTOM) {
                    newS.biquad.delay[0] = oldS.biquad.delay[0];
                    newS.biquad.delay[1] = oldS.biquad.delay[1];
                } else if (newS.type == DSP_FIR && oldS.fir.firSlot >= 0 && newS.fir.firSlot >= 0) {
                    // Copy FIR delay from old active's pool to new active's pool
                    memcpy(_firDelay[newActive][newS.fir.firSlot],
                           _firDelay[oldActive][oldS.fir.firSlot],
                           sizeof(float) * DSP_MAX_FIR_TAPS);
                    newS.fir.delayPos = oldS.fir.delayPos;
                } else if (newS.type == DSP_LIMITER) {
                    newS.limiter.envelope = oldS.limiter.envelope;
                    newS.limiter.gainReduction = oldS.limiter.gainReduction;
                }
            }
        }
    }

    // Atomic swap
    _activeIndex = newActive;
    LOG_I("[DSP] Config swapped (active=%d)", newActive);
}

// ===== Metrics =====

DspMetrics dsp_get_metrics() {
    return _metrics;
}

void dsp_reset_max_metrics() {
    _metrics.maxProcessTimeUs = 0;
}

// ===== Main Processing Entry Point =====

void dsp_process_buffer(int32_t *buffer, int stereoFrames, int adcIndex) {
    if (!buffer || stereoFrames <= 0 || stereoFrames > 256) return;

#ifndef NATIVE_TEST
    unsigned long startUs = (unsigned long)esp_timer_get_time();
#else
    unsigned long startUs = esp_timer_get_time();
#endif

    int stateIdx = _activeIndex;
    DspState *cfg = &_states[stateIdx];
    if (cfg->globalBypass) return;

    // Map ADC index to channel pair: ADC0 → ch0(L), ch1(R); ADC1 → ch2(L), ch3(R)
    int chL = adcIndex * 2;
    int chR = adcIndex * 2 + 1;
    if (chL >= DSP_MAX_CHANNELS || chR >= DSP_MAX_CHANNELS) return;

    // Deinterleave int32 stereo → float mono buffers
    for (int f = 0; f < stereoFrames; f++) {
        _dspBufL[f] = (float)buffer[f * 2] / MAX_24BIT_F;
        _dspBufR[f] = (float)buffer[f * 2 + 1] / MAX_24BIT_F;
    }

    // Process each channel
    dsp_process_channel(_dspBufL, stereoFrames, cfg->channels[chL], stateIdx);
    dsp_process_channel(_dspBufR, stereoFrames, cfg->channels[chR], stateIdx);

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

    // Collect limiter GR from active channels
    for (int c = chL; c <= chR; c++) {
        _metrics.limiterGrDb[c] = 0.0f;
        DspChannelConfig &ch = cfg->channels[c];
        for (int s = 0; s < ch.stageCount; s++) {
            if (ch.stages[s].type == DSP_LIMITER && ch.stages[s].enabled) {
                _metrics.limiterGrDb[c] = ch.stages[s].limiter.gainReduction;
                break;
            }
        }
    }
}

// ===== Per-Channel Processing =====

static void dsp_process_channel(float *buf, int len, DspChannelConfig &ch, int stateIdx) {
    if (ch.bypass) return;

    DspState *cfg = &_states[stateIdx];

    for (int i = 0; i < ch.stageCount; i++) {
        DspStage &s = ch.stages[i];
        if (!s.enabled) continue;

        switch (s.type) {
            case DSP_BIQUAD_LPF:
            case DSP_BIQUAD_HPF:
            case DSP_BIQUAD_BPF:
            case DSP_BIQUAD_NOTCH:
            case DSP_BIQUAD_PEQ:
            case DSP_BIQUAD_LOW_SHELF:
            case DSP_BIQUAD_HIGH_SHELF:
            case DSP_BIQUAD_ALLPASS:
            case DSP_BIQUAD_CUSTOM:
                dsps_biquad_f32(buf, buf, len, s.biquad.coeffs, s.biquad.delay);
                break;
            case DSP_LIMITER:
                dsp_limiter_process(s.limiter, buf, len, cfg->sampleRate);
                break;
            case DSP_FIR:
                dsp_fir_process(s.fir, buf, len, stateIdx);
                break;
            case DSP_GAIN:
                dsp_gain_process(s.gain, buf, len);
                break;
            default:
                break;
        }
    }
}

// ===== Limiter =====

static void dsp_limiter_process(DspLimiterParams &lim, float *buf, int len, uint32_t sampleRate) {
    if (len <= 0 || sampleRate == 0) return;

    float threshLin = powf(10.0f, lim.thresholdDb / 20.0f);
    float attackCoeff = expf(-1.0f / (lim.attackMs * 0.001f * (float)sampleRate));
    float releaseCoeff = expf(-1.0f / (lim.releaseMs * 0.001f * (float)sampleRate));

    float env = lim.envelope;
    float maxGr = 0.0f;

    for (int i = 0; i < len; i++) {
        float absSample = fabsf(buf[i]);

        // Envelope follower (peak detector)
        if (absSample > env) {
            env = attackCoeff * env + (1.0f - attackCoeff) * absSample;
        } else {
            env = releaseCoeff * env + (1.0f - releaseCoeff) * absSample;
        }

        // Gain computation
        float gainLin = 1.0f;
        if (env > threshLin && env > 0.0f) {
            float envDb = 20.0f * log10f(env);
            float overDb = envDb - lim.thresholdDb;
            float grDb = overDb * (1.0f - 1.0f / lim.ratio);
            gainLin = powf(10.0f, -grDb / 20.0f);
            if (grDb > maxGr) maxGr = grDb;
        }

        buf[i] *= gainLin;
    }

    lim.envelope = env;
    lim.gainReduction = -maxGr;
}

// ===== FIR =====

static void dsp_fir_process(DspFirParams &fir, float *buf, int len, int stateIdx) {
    if (fir.numTaps == 0 || fir.firSlot < 0 || fir.firSlot >= DSP_MAX_FIR_SLOTS) return;

    float *taps = _firTaps[stateIdx][fir.firSlot];
    float *delay = _firDelay[stateIdx][fir.firSlot];

    fir_f32_t firState;
    firState.coeffs = taps;
    firState.delay = delay;
    firState.numTaps = fir.numTaps;
    firState.pos = fir.delayPos;

    dsps_fir_f32(&firState, buf, buf, len);

    fir.delayPos = (uint16_t)firState.pos;
}

// ===== Gain =====

static void dsp_gain_process(DspGainParams &gain, float *buf, int len) {
    float g = gain.gainLinear;
    for (int i = 0; i < len; i++) {
        buf[i] *= g;
    }
}

// ===== Stage CRUD =====

int dsp_add_stage(int channel, DspStageType type, int position) {
    if (channel < 0 || channel >= DSP_MAX_CHANNELS) return -1;
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

    // Allocate FIR slot if needed
    if (type == DSP_FIR) {
        int slot = dsp_fir_alloc_slot();
        ch.stages[pos].fir.firSlot = (int8_t)slot;
        if (slot < 0) {
            LOG_W("[DSP] No FIR slots available (max %d)", DSP_MAX_FIR_SLOTS);
        }
    }

    ch.stageCount++;

    // Compute coefficients for new stage
    if (type <= DSP_BIQUAD_CUSTOM) {
        dsp_compute_biquad_coeffs(ch.stages[pos].biquad, type, cfg->sampleRate);
    } else if (type == DSP_GAIN) {
        dsp_compute_gain_linear(ch.stages[pos].gain);
    }

    return pos;
}

bool dsp_remove_stage(int channel, int stageIndex) {
    if (channel < 0 || channel >= DSP_MAX_CHANNELS) return false;
    DspState *cfg = dsp_get_inactive_config();
    DspChannelConfig &ch = cfg->channels[channel];
    if (stageIndex < 0 || stageIndex >= ch.stageCount) return false;

    // Free FIR slot if removing a FIR stage
    if (ch.stages[stageIndex].type == DSP_FIR) {
        dsp_fir_free_slot(ch.stages[stageIndex].fir.firSlot);
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

// ===== JSON Serialization =====

#ifndef NATIVE_TEST
#include <ArduinoJson.h>
#endif

static const char *stage_type_name(DspStageType t) {
    switch (t) {
        case DSP_BIQUAD_LPF:        return "LPF";
        case DSP_BIQUAD_HPF:        return "HPF";
        case DSP_BIQUAD_BPF:        return "BPF";
        case DSP_BIQUAD_NOTCH:      return "NOTCH";
        case DSP_BIQUAD_PEQ:        return "PEQ";
        case DSP_BIQUAD_LOW_SHELF:  return "LOW_SHELF";
        case DSP_BIQUAD_HIGH_SHELF: return "HIGH_SHELF";
        case DSP_BIQUAD_ALLPASS:    return "ALLPASS";
        case DSP_BIQUAD_CUSTOM:     return "CUSTOM";
        case DSP_LIMITER:           return "LIMITER";
        case DSP_FIR:               return "FIR";
        case DSP_GAIN:              return "GAIN";
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
    if (strcmp(name, "CUSTOM") == 0) return DSP_BIQUAD_CUSTOM;
    if (strcmp(name, "LIMITER") == 0) return DSP_LIMITER;
    if (strcmp(name, "FIR") == 0) return DSP_FIR;
    if (strcmp(name, "GAIN") == 0) return DSP_GAIN;
    return DSP_BIQUAD_PEQ;
}

#ifndef NATIVE_TEST

void dsp_export_config_to_json(int channel, char *buf, int bufSize) {
    if (channel < 0 || channel >= DSP_MAX_CHANNELS || !buf) return;
    DspState *cfg = dsp_get_active_config();
    DspChannelConfig &ch = cfg->channels[channel];

    JsonDocument doc;
    doc["bypass"] = ch.bypass;
    JsonArray stages = doc["stages"].to<JsonArray>();

    for (int i = 0; i < ch.stageCount; i++) {
        DspStage &s = ch.stages[i];
        JsonObject stageObj = stages.add<JsonObject>();
        stageObj["enabled"] = s.enabled;
        stageObj["type"] = stage_type_name(s.type);
        if (s.label[0]) stageObj["label"] = s.label;

        if (s.type <= DSP_BIQUAD_CUSTOM) {
            JsonObject params = stageObj["params"].to<JsonObject>();
            params["frequency"] = s.biquad.frequency;
            params["gain"] = s.biquad.gain;
            params["Q"] = s.biquad.Q;
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
        }
    }

    serializeJson(doc, buf, bufSize);
}

void dsp_load_config_from_json(const char *json, int channel) {
    if (!json || channel < 0 || channel >= DSP_MAX_CHANNELS) return;
    DspState *cfg = dsp_get_inactive_config();
    int inactiveIdx = 1 - _activeIndex;
    DspChannelConfig &ch = cfg->channels[channel];

    // Free any existing FIR slots for this channel
    for (int i = 0; i < ch.stageCount; i++) {
        if (ch.stages[i].type == DSP_FIR) {
            dsp_fir_free_slot(ch.stages[i].fir.firSlot);
        }
    }

    JsonDocument doc;
    if (deserializeJson(doc, json)) return;

    if (doc["bypass"].is<bool>()) ch.bypass = doc["bypass"].as<bool>();

    if (doc["stages"].is<JsonArray>()) {
        ch.stageCount = 0;
        JsonArray stages = doc["stages"].as<JsonArray>();
        for (JsonObject stageObj : stages) {
            if (ch.stageCount >= DSP_MAX_STAGES) break;
            DspStage &s = ch.stages[ch.stageCount];
            DspStageType type = stage_type_from_name(stageObj["type"].as<const char *>());
            dsp_init_stage(s, type);
            if (stageObj["enabled"].is<bool>()) s.enabled = stageObj["enabled"].as<bool>();
            if (stageObj["label"].is<const char *>()) {
                strncpy(s.label, stageObj["label"].as<const char *>(), sizeof(s.label) - 1);
                s.label[sizeof(s.label) - 1] = '\0';
            }

            JsonObject params = stageObj["params"];
            if (type <= DSP_BIQUAD_CUSTOM) {
                if (params["frequency"].is<float>()) s.biquad.frequency = params["frequency"].as<float>();
                if (params["gain"].is<float>()) s.biquad.gain = params["gain"].as<float>();
                if (params["Q"].is<float>()) s.biquad.Q = params["Q"].as<float>();
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
                s.fir.firSlot = (int8_t)slot;
                if (params["numTaps"].is<int>()) s.fir.numTaps = params["numTaps"].as<uint16_t>();
            }
            ch.stageCount++;
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
        JsonArray stages = chObj["stages"].to<JsonArray>();

        for (int i = 0; i < ch.stageCount; i++) {
            DspStage &s = ch.stages[i];
            JsonObject stageObj = stages.add<JsonObject>();
            stageObj["enabled"] = s.enabled;
            stageObj["type"] = stage_type_name(s.type);
            if (s.label[0]) stageObj["label"] = s.label;

            if (s.type <= DSP_BIQUAD_CUSTOM) {
                JsonObject params = stageObj["params"].to<JsonObject>();
                params["frequency"] = s.biquad.frequency;
                params["gain"] = s.biquad.gain;
                params["Q"] = s.biquad.Q;
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
            }
        }
    }

    serializeJson(doc, buf, bufSize);
}

void dsp_import_full_config_json(const char *json) {
    if (!json) return;
    DspState *cfg = dsp_get_inactive_config();

    // Free all existing FIR slots
    for (int c = 0; c < DSP_MAX_CHANNELS; c++) {
        for (int i = 0; i < cfg->channels[c].stageCount; i++) {
            if (cfg->channels[c].stages[i].type == DSP_FIR) {
                dsp_fir_free_slot(cfg->channels[c].stages[i].fir.firSlot);
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
            ch.stageCount = 0;

            if (chObj["stages"].is<JsonArray>()) {
                for (JsonObject stageObj : chObj["stages"].as<JsonArray>()) {
                    if (ch.stageCount >= DSP_MAX_STAGES) break;
                    DspStage &s = ch.stages[ch.stageCount];
                    DspStageType type = stage_type_from_name(stageObj["type"].as<const char *>());
                    dsp_init_stage(s, type);
                    if (stageObj["enabled"].is<bool>()) s.enabled = stageObj["enabled"].as<bool>();
                    if (stageObj["label"].is<const char *>()) {
                        strncpy(s.label, stageObj["label"].as<const char *>(), sizeof(s.label) - 1);
                        s.label[sizeof(s.label) - 1] = '\0';
                    }

                    JsonObject params = stageObj["params"];
                    if (type <= DSP_BIQUAD_CUSTOM) {
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
                        s.fir.firSlot = (int8_t)slot;
                        if (params["numTaps"].is<int>()) s.fir.numTaps = params["numTaps"].as<uint16_t>();
                    }
                    ch.stageCount++;
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

#endif // DSP_ENABLED
