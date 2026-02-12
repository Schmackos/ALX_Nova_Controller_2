// ESP-DSP Lite — SFDR analysis (ANSI C fallback for native tests)
#ifndef DSPS_SFDR_H
#define DSPS_SFDR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Calculate Spurious-Free Dynamic Range of a spectrum
// input: FFT magnitude spectrum (half-size, real)
// len: number of bins
// use_dc: if 1, include DC bin in search
// Returns SFDR in dB (difference between strongest and second-strongest component)
float dsps_sfdr_f32(const float *input, int32_t len, int8_t use_dc);

#ifdef __cplusplus
}
#endif

#endif // DSPS_SFDR_H
