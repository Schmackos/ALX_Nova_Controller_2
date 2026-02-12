// ESP-DSP Lite — Vector math operations (ANSI C fallback for native tests)

#include "dsps_mulc.h"
#include "dsps_mul.h"
#include "dsps_add.h"

esp_err_t dsps_mulc_f32(const float *input, float *output, int len, float C, int step_in, int step_out) {
    if (!input || !output || len <= 0) return ESP_ERR_DSP_INVALID_PARAM;
    for (int i = 0; i < len; i++) {
        output[i * step_out] = input[i * step_in] * C;
    }
    return ESP_OK;
}

esp_err_t dsps_mul_f32(const float *input1, const float *input2, float *output, int len, int step1, int step2, int step_out) {
    if (!input1 || !input2 || !output || len <= 0) return ESP_ERR_DSP_INVALID_PARAM;
    for (int i = 0; i < len; i++) {
        output[i * step_out] = input1[i * step1] * input2[i * step2];
    }
    return ESP_OK;
}

esp_err_t dsps_add_f32(const float *input1, const float *input2, float *output, int len, int step1, int step2, int step_out) {
    if (!input1 || !input2 || !output || len <= 0) return ESP_ERR_DSP_INVALID_PARAM;
    for (int i = 0; i < len; i++) {
        output[i * step_out] = input1[i * step1] + input2[i * step2];
    }
    return ESP_OK;
}
