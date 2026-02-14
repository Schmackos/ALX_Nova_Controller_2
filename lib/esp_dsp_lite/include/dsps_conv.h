// ESP-DSP Lite — Direct convolution (ANSI C fallback for native tests)
#ifndef DSPS_CONV_H
#define DSPS_CONV_H

#include "dsp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Direct linear convolution.
 * Output length = siglen + patlen - 1
 */
esp_err_t dsps_conv_f32_ansi(const float *Signal, int siglen,
                              const float *Pattern, int patlen,
                              float *dest);

#define dsps_conv_f32 dsps_conv_f32_ansi

#ifdef __cplusplus
}
#endif

#endif // DSPS_CONV_H
