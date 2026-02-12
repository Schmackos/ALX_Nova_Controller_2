// ESP-DSP Lite — Radix-4 FFT (ANSI C fallback for native tests)
// Note: On native, arduinoFFT is used instead. This header provides stub
// declarations so code that conditionally includes dsps_fft4r.h compiles.
#ifndef DSPS_FFT4R_H
#define DSPS_FFT4R_H

#include "dsp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t dsps_fft4r_init_fc32(float *fft_table_buff, int max_fft_size);
esp_err_t dsps_fft4r_fc32(float *data, int N);
esp_err_t dsps_bit_rev4r_fc32(float *data, int N);
esp_err_t dsps_cplx2real_fc32(float *data, int N);

#ifdef __cplusplus
}
#endif

#endif // DSPS_FFT4R_H
