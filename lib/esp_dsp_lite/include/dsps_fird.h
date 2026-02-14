// ESP-DSP Lite — Decimation FIR filter (ANSI C fallback for native tests)
#ifndef DSPS_FIRD_H
#define DSPS_FIRD_H

#include "dsp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decimation FIR filter state structure
 */
typedef struct {
    float *coeffs;      // Pointer to coefficient array (numTaps elements)
    float *delay;       // Pointer to delay line (numTaps elements, caller-allocated)
    int    numTaps;     // Number of FIR taps
    int    pos;         // Current position in circular delay line
    int    decim;       // Decimation factor
} fir_f32_d_t;

/**
 * @brief Initialize decimation FIR filter structure.
 */
esp_err_t dsps_fird_init_f32(fir_f32_d_t *fir, float *coeffs, float *delay, int numTaps, int decim);

/**
 * @brief Process samples through decimation FIR filter.
 * Output length = len / decim.
 */
esp_err_t dsps_fird_f32_ansi(fir_f32_d_t *fir, const float *input, float *output, int len);

#define dsps_fird_f32 dsps_fird_f32_ansi

#ifdef __cplusplus
}
#endif

#endif // DSPS_FIRD_H
