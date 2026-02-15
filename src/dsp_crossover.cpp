#ifdef DSP_ENABLED

#include "dsp_crossover.h"
#include "dsp_coefficients.h"
#include "dsps_mulc.h"
#include "dsps_add.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef NATIVE_TEST
#include <esp_heap_caps.h>
#endif

// ===== Crossover Presets =====

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Compute Butterworth Q values for an Nth-order filter.
// Returns number of 2nd-order sections; sets hasFirstOrder=true if odd order.
// Q values are stored in ascending order (lowest Q first) for numerical stability.
static int butterworth_q_values(int order, float *qValues, bool &hasFirstOrder) {
    hasFirstOrder = (order % 2) != 0;
    int numSections = order / 2;
    for (int k = 0; k < numSections; k++) {
        qValues[k] = 1.0f / (2.0f * sinf((2 * k + 1) * (float)M_PI / (2.0f * order)));
    }
    // Sort ascending (lowest Q first) — bubble sort is fine for <= 12 elements
    for (int i = 0; i < numSections - 1; i++) {
        for (int j = 0; j < numSections - 1 - i; j++) {
            if (qValues[j] > qValues[j + 1]) {
                float tmp = qValues[j];
                qValues[j] = qValues[j + 1];
                qValues[j + 1] = tmp;
            }
        }
    }
    return numSections;
}

// Insert a Butterworth filter of given order with proper per-section Q values.
// label: if non-null, set on all inserted stages for UI grouping.
// Self-contained rollback: if insertion fails partway, all stages from this call are removed.
static int insert_butterworth_filter(int channel, float freq, int order,
                                     DspStageType type2nd, DspStageType type1st,
                                     const char *label = nullptr) {
    float qValues[12]; // max 12 sections for LR24 = BW12 x 2
    bool hasFirstOrder;
    int numSections = butterworth_q_values(order, qValues, hasFirstOrder);

    int firstIdx = -1;
    int localAdded = 0;

    // Insert first-order section if odd order
    if (hasFirstOrder) {
        int idx = dsp_add_stage(channel, type1st);
        if (idx < 0) return -1;
        firstIdx = idx;
        localAdded++;
        DspState *cfg = dsp_get_inactive_config();
        DspStage &s = cfg->channels[channel].stages[idx];
        s.biquad.frequency = freq;
        s.biquad.Q = 0.0f; // Not used for 1st-order
        if (label) strncpy(s.label, label, sizeof(s.label) - 1);
        dsp_compute_biquad_coeffs(s.biquad, type1st, cfg->sampleRate);
    }

    // Insert 2nd-order sections with correct Q values
    for (int i = 0; i < numSections; i++) {
        int idx = dsp_add_stage(channel, type2nd);
        if (idx < 0) {
            // Rollback all stages this call added
            for (int r = 0; r < localAdded; r++) {
                dsp_remove_stage(channel, firstIdx);
            }
            return -1;
        }
        if (firstIdx < 0) firstIdx = idx;
        localAdded++;
        DspState *cfg = dsp_get_inactive_config();
        DspStage &s = cfg->channels[channel].stages[idx];
        s.biquad.frequency = freq;
        s.biquad.Q = qValues[i];
        if (label) strncpy(s.label, label, sizeof(s.label) - 1);
        dsp_compute_biquad_coeffs(s.biquad, type2nd, cfg->sampleRate);
    }

    return firstIdx;
}

// ===== Bessel Crossover =====
// Pre-computed Q values for Bessel filters (from polynomial factorization).
// These produce maximally-flat group delay response.

static const float _besselQ2[] = { 0.5774f };                          // Order 2: 1 section
static const float _besselQ4[] = { 0.5219f, 0.8055f };                 // Order 4: 2 sections
static const float _besselQ6[] = { 0.5103f, 0.6112f, 1.0234f };       // Order 6: 3 sections
static const float _besselQ8[] = { 0.5060f, 0.5606f, 0.7109f, 1.2258f }; // Order 8: 4 sections

static const float *bessel_q_table(int order, int &numSections) {
    switch (order) {
        case 2: numSections = 1; return _besselQ2;
        case 4: numSections = 2; return _besselQ4;
        case 6: numSections = 3; return _besselQ6;
        case 8: numSections = 4; return _besselQ8;
        default: numSections = 0; return nullptr;
    }
}

int dsp_insert_crossover_bessel(int channel, float freq, int order, int role) {
    int numSections;
    const float *qValues = bessel_q_table(order, numSections);
    if (!qValues || numSections == 0) return -1;

    DspStageType type2nd = role == 0 ? DSP_BIQUAD_LPF : DSP_BIQUAD_HPF;
    char label[16];
    snprintf(label, sizeof(label), "BS%d %s", order, role == 0 ? "LPF" : "HPF");

    int firstIdx = -1;
    int localAdded = 0;

    for (int i = 0; i < numSections; i++) {
        int idx = dsp_add_stage(channel, type2nd);
        if (idx < 0) {
            // Rollback
            for (int r = 0; r < localAdded; r++) {
                dsp_remove_stage(channel, firstIdx);
            }
            return -1;
        }
        if (firstIdx < 0) firstIdx = idx;
        localAdded++;
        DspState *cfg = dsp_get_inactive_config();
        DspStage &s = cfg->channels[channel].stages[idx];
        s.biquad.frequency = freq;
        s.biquad.Q = qValues[i];
        strncpy(s.label, label, sizeof(s.label) - 1);
        dsp_compute_biquad_coeffs(s.biquad, type2nd, cfg->sampleRate);
    }

    return firstIdx;
}

// ===== Baffle Step Correction =====
BaffleStepResult dsp_baffle_step_correction(float baffleWidthMm) {
    BaffleStepResult result;
    if (baffleWidthMm <= 0.0f) {
        result.frequency = 500.0f;  // Safe default
        result.gainDb = 6.0f;
        return result;
    }
    // f = speed_of_sound / (pi * width)
    // speed_of_sound = 343000 mm/s
    result.frequency = 343000.0f / ((float)M_PI * baffleWidthMm);
    result.gainDb = 6.0f;  // Baffle step is always ~6 dB
    return result;
}

void dsp_clear_crossover_stages(int channel) {
    if (channel < 0 || channel >= DSP_MAX_CHANNELS) return;
    DspState *cfg = dsp_get_inactive_config();
    DspChannelConfig &ch = cfg->channels[channel];
    // Walk backwards through chain stages (>= DSP_PEQ_BANDS) and remove LPF/HPF types
    for (int i = ch.stageCount - 1; i >= DSP_PEQ_BANDS; i--) {
        DspStageType t = ch.stages[i].type;
        if (t == DSP_BIQUAD_LPF || t == DSP_BIQUAD_HPF ||
            t == DSP_BIQUAD_LPF_1ST || t == DSP_BIQUAD_HPF_1ST) {
            dsp_remove_stage(channel, i);
        }
    }
}

int dsp_insert_crossover_butterworth(int channel, float freq, int order, int role) {
    if (order < 1 || order > 12) return -1;
    DspStageType type2nd = role == 0 ? DSP_BIQUAD_LPF : DSP_BIQUAD_HPF;
    DspStageType type1st = role == 0 ? DSP_BIQUAD_LPF_1ST : DSP_BIQUAD_HPF_1ST;
    char label[16];
    snprintf(label, sizeof(label), "BW%d %s", order, role == 0 ? "LPF" : "HPF");
    return insert_butterworth_filter(channel, freq, order, type2nd, type1st, label);
}

int dsp_insert_crossover_lr(int channel, float freq, int order, int role) {
    // LR order must be even
    if (order < 2 || (order % 2) != 0) return -1;
    // LR must be achievable: LR(2M) = BW(M)^2, so halfOrder = order/2 must be 1..12
    int halfOrder = order / 2;
    if (halfOrder > 12) return -1;

    DspStageType type2nd = role == 0 ? DSP_BIQUAD_LPF : DSP_BIQUAD_HPF;
    DspStageType type1st = role == 0 ? DSP_BIQUAD_LPF_1ST : DSP_BIQUAD_HPF_1ST;
    char label[16];
    snprintf(label, sizeof(label), "LR%d %s", order, role == 0 ? "LPF" : "HPF");

    // Special case: LR2 = BW1^2 = single 2nd-order biquad with Q=0.5
    if (order == 2) {
        int idx = dsp_add_stage(channel, type2nd);
        if (idx < 0) return -1;
        DspState *cfg = dsp_get_inactive_config();
        DspStage &s = cfg->channels[channel].stages[idx];
        s.biquad.frequency = freq;
        s.biquad.Q = 0.5f;
        strncpy(s.label, label, sizeof(s.label) - 1);
        dsp_compute_biquad_coeffs(s.biquad, type2nd, cfg->sampleRate);
        return idx;
    }

    // Insert BW(halfOrder) twice — each call self-rollbacks on internal failure
    DspState *cfg = dsp_get_inactive_config();
    int countBefore = cfg->channels[channel].stageCount;

    int firstIdx = insert_butterworth_filter(channel, freq, halfOrder, type2nd, type1st, label);
    if (firstIdx < 0) return -1;

    // Count how many stages the first BW call added
    cfg = dsp_get_inactive_config();
    int firstBwStages = cfg->channels[channel].stageCount - countBefore;

    int secondIdx = insert_butterworth_filter(channel, freq, halfOrder, type2nd, type1st, label);
    if (secondIdx < 0) {
        // Second call self-rolled-back; now rollback first BW section too
        for (int i = 0; i < firstBwStages; i++) {
            dsp_remove_stage(channel, firstIdx);
        }
        return -1;
    }
    return firstIdx;
}

// Legacy convenience functions
int dsp_insert_crossover_lr2(int channel, float freq, int role) {
    return dsp_insert_crossover_lr(channel, freq, 2, role);
}

int dsp_insert_crossover_lr4(int channel, float freq, int role) {
    return dsp_insert_crossover_lr(channel, freq, 4, role);
}

int dsp_insert_crossover_lr8(int channel, float freq, int role) {
    return dsp_insert_crossover_lr(channel, freq, 8, role);
}

// ===== Bass Management =====

int dsp_setup_bass_management(int subChannel, const int *mainChannels, int numMains, float crossoverFreq) {
    if (subChannel < 0 || subChannel >= DSP_MAX_CHANNELS) return -1;
    if (!mainChannels || numMains <= 0) return -1;

    // LPF on sub channel (LR4 = 2 cascaded Butterworth)
    int result = dsp_insert_crossover_lr4(subChannel, crossoverFreq, 0);
    if (result < 0) return -1;

    // HPF on each main channel (LR4)
    for (int i = 0; i < numMains; i++) {
        if (mainChannels[i] < 0 || mainChannels[i] >= DSP_MAX_CHANNELS) continue;
        int r = dsp_insert_crossover_lr4(mainChannels[i], crossoverFreq, 1);
        if (r < 0) return -1;
    }

    return 0;
}

// ===== Routing Matrix =====

void dsp_routing_init(DspRoutingMatrix &rm) {
    dsp_routing_preset_identity(rm);
}

void dsp_routing_preset_identity(DspRoutingMatrix &rm) {
    memset(&rm, 0, sizeof(rm));
    for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
        rm.matrix[i][i] = 1.0f;
    }
}

void dsp_routing_preset_mono_sum(DspRoutingMatrix &rm) {
    float g = 1.0f / DSP_MAX_CHANNELS;
    for (int o = 0; o < DSP_MAX_CHANNELS; o++) {
        for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
            rm.matrix[o][i] = g;
        }
    }
}

void dsp_routing_preset_swap_lr(DspRoutingMatrix &rm) {
    memset(&rm, 0, sizeof(rm));
    // Swap L1↔R1, L2↔R2
    rm.matrix[0][1] = 1.0f; // output 0 (L1) = input 1 (R1)
    rm.matrix[1][0] = 1.0f; // output 1 (R1) = input 0 (L1)
    if (DSP_MAX_CHANNELS >= 4) {
        rm.matrix[2][3] = 1.0f; // output 2 (L2) = input 3 (R2)
        rm.matrix[3][2] = 1.0f; // output 3 (R2) = input 2 (L2)
    }
}

void dsp_routing_preset_sub_sum(DspRoutingMatrix &rm) {
    memset(&rm, 0, sizeof(rm));
    // Ch0 = 0.5 * (L1 + R1) — mono sub from first stereo pair
    rm.matrix[0][0] = 0.5f;
    rm.matrix[0][1] = 0.5f;
    // Other channels pass through
    rm.matrix[1][1] = 1.0f;
    if (DSP_MAX_CHANNELS >= 4) {
        rm.matrix[2][2] = 1.0f;
        rm.matrix[3][3] = 1.0f;
    }
}

void dsp_routing_set_gain_db(DspRoutingMatrix &rm, int output, int input, float gainDb) {
    if (output < 0 || output >= DSP_MAX_CHANNELS) return;
    if (input < 0 || input >= DSP_MAX_CHANNELS) return;
    if (gainDb <= -200.0f) {
        rm.matrix[output][input] = 0.0f;
    } else {
        rm.matrix[output][input] = powf(10.0f, gainDb / 20.0f);
    }
}

// Routing matrix using SIMD-accelerated vector ops (processes len samples at a time)
void dsp_routing_apply(const DspRoutingMatrix &rm, float *channels[], int numChannels, int len) {
    if (!channels || numChannels <= 0 || len <= 0) return;
    int nc = numChannels < DSP_MAX_CHANNELS ? numChannels : DSP_MAX_CHANNELS;

    // Copy input channels (routing reads all inputs before writing outputs)
    // Buffers allocated in PSRAM to save ~5KB internal SRAM
    static float *inputCopy = nullptr;  // [DSP_MAX_CHANNELS * 256]
    static float *temp = nullptr;       // [256]
    if (!inputCopy) {
#ifndef NATIVE_TEST
        inputCopy = (float *)heap_caps_calloc(DSP_MAX_CHANNELS * 256, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!inputCopy)
#endif
        inputCopy = (float *)calloc(DSP_MAX_CHANNELS * 256, sizeof(float));
    }
    if (!temp) {
#ifndef NATIVE_TEST
        temp = (float *)heap_caps_calloc(256, sizeof(float), MALLOC_CAP_SPIRAM);
        if (!temp)
#endif
        temp = (float *)calloc(256, sizeof(float));
    }
    if (!inputCopy || !temp) return;

    int n = len > 256 ? 256 : len;
    for (int i = 0; i < nc; i++) {
        memcpy(&inputCopy[i * 256], channels[i], n * sizeof(float));
    }

    // Compute each output channel using SIMD mulc + add
    for (int o = 0; o < nc; o++) {
        memset(channels[o], 0, n * sizeof(float));
        for (int i = 0; i < nc; i++) {
            float coeff = rm.matrix[o][i];
            if (coeff == 0.0f) continue;
            dsps_mulc_f32(&inputCopy[i * 256], temp, n, coeff, 1, 1);
            dsps_add_f32(channels[o], temp, channels[o], n, 1, 1, 1);
        }
    }
}

#endif // DSP_ENABLED
