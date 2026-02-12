// ESP-DSP Lite — multiply-by-constant (ANSI C fallback for native tests)
#ifndef DSPS_MULC_H
#define DSPS_MULC_H

#include "dsp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t dsps_mulc_f32(const float *input, float *output, int len, float C, int step_in, int step_out);

#ifdef __cplusplus
}
#endif

#endif // DSPS_MULC_H
