#ifdef DSP_ENABLED

#include "dsp_crossover.h"
#include "dsp_coefficients.h"
#include "dsps_mulc.h"
#include "dsps_add.h"
#include <math.h>
#include <string.h>

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
static int insert_butterworth_filter(int channel, float freq, int order,
                                     DspStageType type2nd, DspStageType type1st) {
    float qValues[12]; // max 12 sections for LR24 = BW12 x 2
    bool hasFirstOrder;
    int numSections = butterworth_q_values(order, qValues, hasFirstOrder);

    int firstIdx = -1;

    // Insert first-order section if odd order
    if (hasFirstOrder) {
        int idx = dsp_add_stage(channel, type1st);
        if (idx < 0) return -1;
        firstIdx = idx;
        DspState *cfg = dsp_get_inactive_config();
        DspStage &s = cfg->channels[channel].stages[idx];
        s.biquad.frequency = freq;
        s.biquad.Q = 0.0f; // Not used for 1st-order
        dsp_compute_biquad_coeffs(s.biquad, type1st, cfg->sampleRate);
    }

    // Insert 2nd-order sections with correct Q values
    for (int i = 0; i < numSections; i++) {
        int idx = dsp_add_stage(channel, type2nd);
        if (idx < 0) return -1;
        if (firstIdx < 0) firstIdx = idx;
        DspState *cfg = dsp_get_inactive_config();
        DspStage &s = cfg->channels[channel].stages[idx];
        s.biquad.frequency = freq;
        s.biquad.Q = qValues[i];
        dsp_compute_biquad_coeffs(s.biquad, type2nd, cfg->sampleRate);
    }

    return firstIdx;
}

int dsp_insert_crossover_butterworth(int channel, float freq, int order, int role) {
    if (order < 1 || order > 8) return -1;
    DspStageType type2nd = role == 0 ? DSP_BIQUAD_LPF : DSP_BIQUAD_HPF;
    DspStageType type1st = role == 0 ? DSP_BIQUAD_LPF_1ST : DSP_BIQUAD_HPF_1ST;
    return insert_butterworth_filter(channel, freq, order, type2nd, type1st);
}

int dsp_insert_crossover_lr(int channel, float freq, int order, int role) {
    // LR order must be even
    if (order < 2 || (order % 2) != 0) return -1;
    // LR must be achievable: LR(2M) = BW(M)^2, so halfOrder = order/2 must be 1..12
    int halfOrder = order / 2;
    if (halfOrder > 12) return -1;

    DspStageType type2nd = role == 0 ? DSP_BIQUAD_LPF : DSP_BIQUAD_HPF;
    DspStageType type1st = role == 0 ? DSP_BIQUAD_LPF_1ST : DSP_BIQUAD_HPF_1ST;

    // Special case: LR2 = BW1^2 = single 2nd-order biquad with Q=0.5
    if (order == 2) {
        int idx = dsp_add_stage(channel, type2nd);
        if (idx < 0) return -1;
        DspState *cfg = dsp_get_inactive_config();
        DspStage &s = cfg->channels[channel].stages[idx];
        s.biquad.frequency = freq;
        s.biquad.Q = 0.5f;
        dsp_compute_biquad_coeffs(s.biquad, type2nd, cfg->sampleRate);
        return idx;
    }

    // Insert BW(halfOrder) twice
    int firstIdx = insert_butterworth_filter(channel, freq, halfOrder, type2nd, type1st);
    if (firstIdx < 0) return -1;
    int secondIdx = insert_butterworth_filter(channel, freq, halfOrder, type2nd, type1st);
    if (secondIdx < 0) return -1;
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
    static float inputCopy[DSP_MAX_CHANNELS][256];
    static float temp[256];
    int n = len > 256 ? 256 : len;
    for (int i = 0; i < nc; i++) {
        memcpy(inputCopy[i], channels[i], n * sizeof(float));
    }

    // Compute each output channel using SIMD mulc + add
    for (int o = 0; o < nc; o++) {
        memset(channels[o], 0, n * sizeof(float));
        for (int i = 0; i < nc; i++) {
            float coeff = rm.matrix[o][i];
            if (coeff == 0.0f) continue;
            dsps_mulc_f32(inputCopy[i], temp, n, coeff, 1, 1);
            dsps_add_f32(channels[o], temp, channels[o], n, 1, 1, 1);
        }
    }
}

#endif // DSP_ENABLED
