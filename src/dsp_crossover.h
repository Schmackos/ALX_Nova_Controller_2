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

// Insert Bessel filter of given order (2,4,6,8).
// Maximally-flat group delay crossover using pre-computed Q values.
int dsp_insert_crossover_bessel(int channel, float freq, int order, int role);

// Baffle step correction: returns frequency and gain for a high-shelf filter.
struct BaffleStepResult {
    float frequency;
    float gainDb;
};
BaffleStepResult dsp_baffle_step_correction(float baffleWidthMm);

// Remove existing LPF/HPF chain stages (>= DSP_PEQ_BANDS) on a channel
// Call before inserting new crossover to avoid duplication.
void dsp_clear_crossover_stages(int channel);

// Legacy convenience functions (delegate to generic implementations)
int dsp_insert_crossover_lr2(int channel, float freq, int role);
int dsp_insert_crossover_lr4(int channel, float freq, int role);
int dsp_insert_crossover_lr8(int channel, float freq, int role);

#endif // DSP_ENABLED
#endif // DSP_CROSSOVER_H
