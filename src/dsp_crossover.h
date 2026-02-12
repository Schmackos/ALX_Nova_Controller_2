#ifndef DSP_CROSSOVER_H
#define DSP_CROSSOVER_H

#ifdef DSP_ENABLED

#include "dsp_pipeline.h"

// ===== Crossover Presets =====
// These insert biquad stages to implement crossover filters.
// role: 0 = LPF, 1 = HPF

// Insert Butterworth filter of given order (1-8).
// Odd orders include a first-order section + biquad sections with correct per-section Q.
int dsp_insert_crossover_butterworth(int channel, float freq, int order, int role);

// Insert Linkwitz-Riley filter of given order (2,4,6,8,12,16,24).
// LR(2M) = BW(M) squared — each BW(M) section appears twice.
int dsp_insert_crossover_lr(int channel, float freq, int order, int role);

// Legacy convenience functions (delegate to generic implementations)
int dsp_insert_crossover_lr2(int channel, float freq, int role);
int dsp_insert_crossover_lr4(int channel, float freq, int role);
int dsp_insert_crossover_lr8(int channel, float freq, int role);

// ===== Bass Management =====
// Sets up sub + main crossover at given frequency with LR4 slopes.
// subChannel: channel index for subwoofer (gets LPF)
// mainChannels: array of channel indices for mains (get HPF)
// numMains: number of main channels
int dsp_setup_bass_management(int subChannel, const int *mainChannels, int numMains, float crossoverFreq);

// ===== Routing Matrix =====

struct DspRoutingMatrix {
    float matrix[DSP_MAX_CHANNELS][DSP_MAX_CHANNELS]; // [output][input] gain (linear)
};

void dsp_routing_init(DspRoutingMatrix &rm);
void dsp_routing_apply(const DspRoutingMatrix &rm, float *channels[], int numChannels, int len);

// Set a single matrix coefficient (gain in dB, -inf = -200 dB = silence)
void dsp_routing_set_gain_db(DspRoutingMatrix &rm, int output, int input, float gainDb);

// Preset: identity (1:1 mapping)
void dsp_routing_preset_identity(DspRoutingMatrix &rm);
// Preset: mono sum (all inputs → all outputs equally)
void dsp_routing_preset_mono_sum(DspRoutingMatrix &rm);
// Preset: swap L/R pairs
void dsp_routing_preset_swap_lr(DspRoutingMatrix &rm);
// Preset: sub sum (L1+R1 → Ch0, passthrough on others)
void dsp_routing_preset_sub_sum(DspRoutingMatrix &rm);

#endif // DSP_ENABLED
#endif // DSP_CROSSOVER_H
