// ESP-DSP Lite — FIR filter processing (ANSI C)
// Based on ESP-DSP by Espressif Systems (Apache 2.0 License)
#ifndef DSPS_FIR_H
#define DSPS_FIR_H

#include "dsp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FIR filter state structure
 */
typedef struct {
    float *coeffs;      // Pointer to coefficient array (numTaps elements)
    float *delay;       // Pointer to delay line (numTaps elements, caller-allocated)
    int    numTaps;     // Number of FIR taps
    int    pos;         // Current position in circular delay line
} fir_f32_t;

/**
 * @brief Initialize FIR filter structure.
 *
 * @param fir       Pointer to FIR state structure
 * @param coeffs    Pointer to coefficient array (numTaps floats, caller-owned)
 * @param delay     Pointer to delay line (numTaps floats, caller-owned, zero-init)
 * @param numTaps   Number of filter taps
 * @return ESP_OK on success
 */
esp_err_t dsps_fir_init_f32(fir_f32_t *fir, float *coeffs, float *delay, int numTaps);

/**
 * @brief Process samples through FIR filter.
 *
 * @param fir       Pointer to initialized FIR state
 * @param input     Input sample array
 * @param output    Output sample array (may alias input)
 * @param len       Number of samples to process
 * @return ESP_OK on success
 */
esp_err_t dsps_fir_f32_ansi(fir_f32_t *fir, const float *input, float *output, int len);

#define dsps_fir_f32 dsps_fir_f32_ansi

#ifdef __cplusplus
}
#endif

#endif // DSPS_FIR_H
