// ESP-DSP Lite — FIR filter processing (ANSI C)
// Based on ESP-DSP by Espressif Systems (Apache 2.0 License)
#ifndef DSPS_FIR_H
#define DSPS_FIR_H

#include "dsp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FIR filter state structure (compatible with official ESP-DSP fir_f32_t)
 */
typedef struct {
    float *coeffs;      // Pointer to coefficient array
    float *delay;       // Pointer to delay line (caller-allocated)
    int    numTaps;     // Number of FIR taps (named 'N' in official ESP-DSP)
    int    pos;         // Current position in circular delay line
    int    decim;       // Decimation factor (used by dsps_fird_*)
} fir_f32_t;

/**
 * @brief Initialize FIR filter structure.
 */
esp_err_t dsps_fir_init_f32(fir_f32_t *fir, float *coeffs, float *delay, int numTaps);

/**
 * @brief Process samples through FIR filter.
 */
esp_err_t dsps_fir_f32_ansi(fir_f32_t *fir, const float *input, float *output, int len);

/**
 * @brief Initialize decimation FIR filter structure.
 */
esp_err_t dsps_fird_init_f32(fir_f32_t *fir, float *coeffs, float *delay, int numTaps, int decim);

/**
 * @brief Process samples through decimation FIR filter.
 * Returns the number of output samples produced.
 */
int dsps_fird_f32_ansi(fir_f32_t *fir, const float *input, float *output, int len);

#define dsps_fir_f32 dsps_fir_f32_ansi
#define dsps_fird_f32 dsps_fird_f32_ansi

#ifdef __cplusplus
}
#endif

#endif // DSPS_FIR_H
