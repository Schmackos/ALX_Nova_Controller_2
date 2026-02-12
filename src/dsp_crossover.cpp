#ifdef DSP_ENABLED

#include "dsp_crossover.h"
#include "dsp_coefficients.h"
#include <math.h>
#include <string.h>

// ===== Crossover Presets =====

// Insert N biquads of the same type at given frequency (Butterworth Q = 0.707)
static int insert_butterworth_biquads(int channel, float freq, int count, DspStageType type) {
    int firstIdx = -1;
    for (int i = 0; i < count; i++) {
        int idx = dsp_add_stage(channel, type);
        if (idx < 0) return -1;
        if (firstIdx < 0) firstIdx = idx;

        DspState *cfg = dsp_get_inactive_config();
        DspStage &s = cfg->channels[channel].stages[idx];
        s.biquad.frequency = freq;
        s.biquad.Q = 0.707f; // Butterworth Q
        dsp_compute_biquad_coeffs(s.biquad, type, cfg->sampleRate);
    }
    return firstIdx;
}

int dsp_insert_crossover_lr2(int channel, float freq, int role) {
    DspStageType type = role == 0 ? DSP_BIQUAD_LPF : DSP_BIQUAD_HPF;
    return insert_butterworth_biquads(channel, freq, 1, type);
}

int dsp_insert_crossover_lr4(int channel, float freq, int role) {
    DspStageType type = role == 0 ? DSP_BIQUAD_LPF : DSP_BIQUAD_HPF;
    return insert_butterworth_biquads(channel, freq, 2, type);
}

int dsp_insert_crossover_lr8(int channel, float freq, int role) {
    DspStageType type = role == 0 ? DSP_BIQUAD_LPF : DSP_BIQUAD_HPF;
    return insert_butterworth_biquads(channel, freq, 4, type);
}

int dsp_insert_crossover_butterworth(int channel, float freq, int order, int role) {
    if (order < 2 || order > 8 || (order % 2) != 0) return -1;
    DspStageType type = role == 0 ? DSP_BIQUAD_LPF : DSP_BIQUAD_HPF;
    return insert_butterworth_biquads(channel, freq, order / 2, type);
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

void dsp_routing_apply(const DspRoutingMatrix &rm, float *channels[], int numChannels, int len) {
    if (!channels || numChannels <= 0 || len <= 0) return;
    int nc = numChannels < DSP_MAX_CHANNELS ? numChannels : DSP_MAX_CHANNELS;

    // Temp buffer for one sample across all channels
    float temp[DSP_MAX_CHANNELS];

    for (int s = 0; s < len; s++) {
        // Compute output for each channel
        for (int o = 0; o < nc; o++) {
            float sum = 0.0f;
            for (int i = 0; i < nc; i++) {
                sum += rm.matrix[o][i] * channels[i][s];
            }
            temp[o] = sum;
        }
        // Write back
        for (int o = 0; o < nc; o++) {
            channels[o][s] = temp[o];
        }
    }
}

#endif // DSP_ENABLED
