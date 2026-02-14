// ESP-DSP Lite — Cross-correlation (ANSI C fallback for native tests)
#ifndef DSPS_CORR_H
#define DSPS_CORR_H

#include "dsp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Cross-correlation of Signal and Pattern.
 * dest[n] = sum(Signal[n+m] * Pattern[m]) for m=0..patlen-1
 * Output length = siglen - patlen + 1
 */
esp_err_t dsps_corr_f32_ansi(const float *Signal, int siglen,
                              const float *Pattern, int patlen,
                              float *dest);

#define dsps_corr_f32 dsps_corr_f32_ansi

#ifdef __cplusplus
}
#endif

#endif // DSPS_CORR_H
