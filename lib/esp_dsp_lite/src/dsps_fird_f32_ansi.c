// ESP-DSP Lite — Decimation FIR filter (ANSI C fallback for native tests)

#include "dsps_fir.h"
#include <string.h>

esp_err_t dsps_fird_init_f32(fir_f32_t *fir, float *coeffs, float *delay, int numTaps, int decim) {
    if (!fir || !coeffs || !delay || numTaps <= 0 || decim <= 0)
        return ESP_ERR_DSP_INVALID_PARAM;
    fir->coeffs = coeffs;
    fir->delay = delay;
    fir->N = numTaps;
    fir->pos = 0;
    fir->decim = decim;
    memset(delay, 0, sizeof(float) * numTaps);
    return ESP_OK;
}

int dsps_fird_f32_ansi(fir_f32_t *fir, const float *input, float *output, int len) {
    if (!fir || !input || !output || len <= 0)
        return 0;

    int outIdx = 0;
    int decim = fir->decim;
    int numTaps = fir->N;
    int pos = fir->pos;

    for (int i = 0; i < len; i++) {
        // Insert sample into delay line
        fir->delay[pos] = input[i];
        pos = (pos + 1) % numTaps;

        // Only compute output every 'decim' samples
        if ((i + 1) % decim == 0) {
            float acc = 0.0f;
            int idx = pos;
            for (int t = 0; t < numTaps; t++) {
                idx--;
                if (idx < 0) idx = numTaps - 1;
                acc += fir->delay[idx] * fir->coeffs[t];
            }
            output[outIdx++] = acc;
        }
    }

    fir->pos = pos;
    return outIdx;
}
