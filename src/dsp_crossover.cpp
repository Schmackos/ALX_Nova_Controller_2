#ifdef DSP_ENABLED

#include "dsp_crossover.h"
#include "dsp_coefficients.h"
#include "hal/hal_types.h"  // hal_safe_strcpy
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
        if (label) hal_safe_strcpy(s.label, sizeof(s.label), label);
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
        if (label) hal_safe_strcpy(s.label, sizeof(s.label), label);
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
        hal_safe_strcpy(s.label, sizeof(s.label), label);
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
        hal_safe_strcpy(s.label, sizeof(s.label), label);
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

#endif // DSP_ENABLED
