// ESP-DSP Lite — Dot product (ANSI C fallback for native tests)

#include "dsps_dotprod.h"

esp_err_t dsps_dotprod_f32(const float *src1, const float *src2, float *dest, int len) {
    if (!src1 || !src2 || !dest || len <= 0) return ESP_ERR_DSP_INVALID_PARAM;
    float sum = 0.0f;
    for (int i = 0; i < len; i++) {
        sum += src1[i] * src2[i];
    }
    *dest += sum;
    return ESP_OK;
}
