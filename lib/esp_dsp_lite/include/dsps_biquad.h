// ESP-DSP Lite — Biquad IIR filter processing (ANSI C)
// Based on ESP-DSP by Espressif Systems (Apache 2.0 License)
#ifndef DSPS_BIQUAD_H
#define DSPS_BIQUAD_H

#include "dsp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Process samples through a second-order IIR (biquad) filter section.
 *
 * Direct Form II Transposed implementation.
 * Coefficients layout: coeffs[5] = {b0, b1, b2, a1, a2}
 * Delay line: delay[2] (caller-managed state, zero-init for first call)
 *
 * @param input   Input sample array
 * @param output  Output sample array (may alias input for in-place)
 * @param len     Number of samples to process
 * @param coeffs  Filter coefficients [b0, b1, b2, a1, a2]
 * @param delay   Two-element delay line (state preserved between calls)
 * @return ESP_OK on success
 */
esp_err_t dsps_biquad_f32_ansi(const float *input, float *output, int len,
                                float *coeffs, float *delay);

// Default name maps to ANSI implementation
#define dsps_biquad_f32 dsps_biquad_f32_ansi

#ifdef __cplusplus
}
#endif

#endif // DSPS_BIQUAD_H
