// Biquad coefficient generators (Robert Bristow-Johnson Audio EQ Cookbook)
// Renamed from dsps_biquad_gen_* to dsp_gen_* to avoid symbol conflicts
// with the pre-built ESP-DSP library (which has different signatures).
// Based on ESP-DSP by Espressif Systems (Apache 2.0 License)
#ifndef DSP_BIQUAD_GEN_H
#define DSP_BIQUAD_GEN_H

#ifdef __cplusplus
extern "C" {
#endif

// All generators write 5 coefficients into coeffs[5] = {b0, b1, b2, a1, a2}.
// freq: normalized frequency = f_Hz / f_sample (must be 0 < freq < 0.5)
// qFactor: quality factor (Q > 0)
// gain: gain in dB (for peaking EQ and shelf filters)
// Returns 0 on success, non-zero on invalid parameters.

int dsp_gen_lpf_f32(float *coeffs, float freq, float qFactor);
int dsp_gen_hpf_f32(float *coeffs, float freq, float qFactor);
int dsp_gen_bpf_f32(float *coeffs, float freq, float qFactor);
int dsp_gen_notch_f32(float *coeffs, float freq, float qFactor);
int dsp_gen_allpass_f32(float *coeffs, float freq, float qFactor);
int dsp_gen_allpass360_f32(float *coeffs, float freq, float qFactor);
int dsp_gen_allpass180_f32(float *coeffs, float freq, float qFactor);
int dsp_gen_bpf0db_f32(float *coeffs, float freq, float qFactor);
int dsp_gen_peakingEQ_f32(float *coeffs, float freq, float gain, float qFactor);
int dsp_gen_lowShelf_f32(float *coeffs, float freq, float gain, float qFactor);
int dsp_gen_highShelf_f32(float *coeffs, float freq, float gain, float qFactor);

#ifdef __cplusplus
}
#endif

#endif // DSP_BIQUAD_GEN_H
