// ESP-DSP Lite — dot product (ANSI C fallback for native tests)
#ifndef DSPS_DOTPROD_H
#define DSPS_DOTPROD_H

#include "dsp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Accumulates dot product into *dest (does NOT reset *dest before accumulating)
esp_err_t dsps_dotprod_f32(const float *src1, const float *src2, float *dest, int len);

#ifdef __cplusplus
}
#endif

#endif // DSPS_DOTPROD_H
