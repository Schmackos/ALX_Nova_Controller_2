// ESP-DSP Lite — Direct convolution (ANSI C fallback for native tests)

#include "dsps_conv.h"

esp_err_t dsps_conv_f32_ansi(const float *Signal, int siglen,
                              const float *Pattern, int patlen,
                              float *dest) {
    if (!Signal || !Pattern || !dest || siglen <= 0 || patlen <= 0)
        return ESP_ERR_DSP_INVALID_PARAM;

    int outLen = siglen + patlen - 1;
    for (int n = 0; n < outLen; n++) {
        float sum = 0.0f;
        int kStart = (n - patlen + 1) > 0 ? (n - patlen + 1) : 0;
        int kEnd = n < (siglen - 1) ? n : (siglen - 1);
        for (int k = kStart; k <= kEnd; k++) {
            sum += Signal[k] * Pattern[n - k];
        }
        dest[n] = sum;
    }
    return ESP_OK;
}
