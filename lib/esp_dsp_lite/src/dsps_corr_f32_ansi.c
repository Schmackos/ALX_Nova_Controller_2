// ESP-DSP Lite — Cross-correlation (ANSI C fallback for native tests)

#include "dsps_corr.h"

esp_err_t dsps_corr_f32_ansi(const float *Signal, int siglen,
                              const float *Pattern, int patlen,
                              float *dest) {
    if (!Signal || !Pattern || !dest || siglen <= 0 || patlen <= 0 || patlen > siglen)
        return ESP_ERR_DSP_INVALID_PARAM;

    int outLen = siglen - patlen + 1;
    for (int n = 0; n < outLen; n++) {
        float sum = 0.0f;
        for (int m = 0; m < patlen; m++) {
            sum += Signal[n + m] * Pattern[m];
        }
        dest[n] = sum;
    }
    return ESP_OK;
}
