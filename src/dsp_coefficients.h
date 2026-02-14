#ifndef DSP_COEFFICIENTS_H
#define DSP_COEFFICIENTS_H

#ifdef DSP_ENABLED

#include "dsp_pipeline.h"

// Compute biquad coefficients from parameters + stage type + sample rate.
// Writes to params.coeffs[5]. Pure function.
void dsp_compute_biquad_coeffs(DspBiquadParams &params, DspStageType type, uint32_t sampleRate);

// Load custom coefficients directly (for REW/miniDSP import).
void dsp_load_custom_coeffs(DspBiquadParams &params, float b0, float b1, float b2, float a1, float a2);

// Recompute all biquad-type stages in a channel.
void dsp_recompute_channel_coeffs(DspChannelConfig &ch, uint32_t sampleRate);

// Precompute linear gain from dB: gainLinear = 10^(gainDb/20).
void dsp_compute_gain_linear(DspGainParams &params);

// Precompute compressor makeup gain from dB.
void dsp_compute_compressor_makeup(DspCompressorParams &params);

// Design windowed-sinc anti-aliasing LPF for decimation.
// Writes numTaps floats to taps[]. Cutoff = sampleRate / (2 * decimFactor).
void dsp_compute_decimation_filter(float *taps, int numTaps, int decimFactor, float sampleRate);

#endif // DSP_ENABLED
#endif // DSP_COEFFICIENTS_H
