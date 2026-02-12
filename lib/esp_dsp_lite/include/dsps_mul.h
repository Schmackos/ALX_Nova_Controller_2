// ESP-DSP Lite — element-wise multiply (ANSI C fallback for native tests)
#ifndef DSPS_MUL_H
#define DSPS_MUL_H

#include "dsp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t dsps_mul_f32(const float *input1, const float *input2, float *output, int len, int step1, int step2, int step_out);

#ifdef __cplusplus
}
#endif

#endif // DSPS_MUL_H
