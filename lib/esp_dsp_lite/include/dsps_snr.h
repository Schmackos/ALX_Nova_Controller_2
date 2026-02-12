// ESP-DSP Lite — SNR analysis (ANSI C fallback for native tests)
#ifndef DSPS_SNR_H
#define DSPS_SNR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Calculate Signal-to-Noise Ratio of a spectrum
// input: FFT magnitude spectrum (half-size, real)
// len: number of bins
// use_dc: if 1, include DC bin in noise calculation
// Returns SNR in dB
float dsps_snr_f32(const float *input, int32_t len, uint8_t use_dc);

#ifdef __cplusplus
}
#endif

#endif // DSPS_SNR_H
