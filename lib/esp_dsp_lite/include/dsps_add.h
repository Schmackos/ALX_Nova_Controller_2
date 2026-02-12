// ESP-DSP Lite — element-wise addition (ANSI C fallback for native tests)
#ifndef DSPS_ADD_H
#define DSPS_ADD_H

#include "dsp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t dsps_add_f32(const float *input1, const float *input2, float *output, int len, int step1, int step2, int step_out);

#ifdef __cplusplus
}
#endif

#endif // DSPS_ADD_H
