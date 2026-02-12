// ESP-DSP Lite — Biquad IIR filter (Direct Form II Transposed, ANSI C)
// Based on ESP-DSP by Espressif Systems (Apache 2.0 License)
#include "dsps_biquad.h"

esp_err_t dsps_biquad_f32_ansi(const float *input, float *output, int len,
                                float *coeffs, float *delay)
{
    if (!input || !output || !coeffs || !delay || len <= 0) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }

    float b0 = coeffs[0];
    float b1 = coeffs[1];
    float b2 = coeffs[2];
    float a1 = coeffs[3];
    float a2 = coeffs[4];
    float d0 = delay[0];
    float d1 = delay[1];

    for (int i = 0; i < len; i++) {
        float x = input[i];
        float y = b0 * x + d0;
        d0 = b1 * x - a1 * y + d1;
        d1 = b2 * x - a2 * y;
        output[i] = y;
    }

    delay[0] = d0;
    delay[1] = d1;
    return ESP_OK;
}
