// ESP-DSP Lite — FIR filter processing (ANSI C)
// Based on ESP-DSP by Espressif Systems (Apache 2.0 License)
#include "dsps_fir.h"

esp_err_t dsps_fir_f32_ansi(fir_f32_t *fir, const float *input, float *output, int len)
{
    if (!fir || !input || !output || len <= 0) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }
    if (!fir->coeffs || !fir->delay || fir->N <= 0) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }

    float *coeffs = fir->coeffs;
    float *delay = fir->delay;
    int numTaps = fir->N;
    int pos = fir->pos;

    for (int i = 0; i < len; i++) {
        // Insert new sample into circular delay line
        delay[pos] = input[i];

        // Compute convolution sum
        float acc = 0.0f;
        int idx = pos;
        for (int t = 0; t < numTaps; t++) {
            acc += coeffs[t] * delay[idx];
            idx--;
            if (idx < 0) {
                idx = numTaps - 1;
            }
        }
        output[i] = acc;

        pos++;
        if (pos >= numTaps) {
            pos = 0;
        }
    }

    fir->pos = pos;
    return ESP_OK;
}
