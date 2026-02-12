// ESP-DSP Lite — FIR filter initialization (ANSI C)
// Based on ESP-DSP by Espressif Systems (Apache 2.0 License)
#include "dsps_fir.h"

esp_err_t dsps_fir_init_f32(fir_f32_t *fir, float *coeffs, float *delay, int numTaps)
{
    if (!fir || !coeffs || !delay || numTaps <= 0) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }

    fir->coeffs = coeffs;
    fir->delay = delay;
    fir->numTaps = numTaps;
    fir->pos = 0;

    // Zero the delay line
    for (int i = 0; i < numTaps; i++) {
        delay[i] = 0.0f;
    }

    return ESP_OK;
}
