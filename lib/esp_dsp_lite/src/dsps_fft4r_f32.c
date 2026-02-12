// ESP-DSP Lite — Radix-4 FFT stubs (ANSI C fallback for native tests)
// These are minimal stubs; native tests use arduinoFFT for actual FFT computation.
// These exist so that code conditionally calling dsps_fft4r_* links on native.

#include "dsps_fft4r.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

esp_err_t dsps_fft4r_init_fc32(float *fft_table_buff, int max_fft_size) {
    (void)fft_table_buff;
    (void)max_fft_size;
    return ESP_OK;
}

// Radix-2 Cooley-Tukey in-place FFT (fallback for native tests)
// Input: interleaved complex [Re0, Im0, Re1, Im1, ...], length = N complex pairs
esp_err_t dsps_fft4r_fc32(float *data, int N) {
    if (!data || N <= 0) return ESP_ERR_DSP_INVALID_PARAM;

    // Bit-reversal permutation
    int j = 0;
    for (int i = 0; i < N; i++) {
        if (i < j) {
            float tmpR = data[i * 2];
            float tmpI = data[i * 2 + 1];
            data[i * 2] = data[j * 2];
            data[i * 2 + 1] = data[j * 2 + 1];
            data[j * 2] = tmpR;
            data[j * 2 + 1] = tmpI;
        }
        int m = N >> 1;
        while (m >= 1 && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }

    // Cooley-Tukey butterfly
    for (int step = 1; step < N; step <<= 1) {
        float angle = -(float)M_PI / step;
        float wr = cosf(angle);
        float wi = sinf(angle);
        for (int group = 0; group < N; group += step << 1) {
            float tr = 1.0f, ti = 0.0f;
            for (int pair = 0; pair < step; pair++) {
                int i1 = (group + pair) * 2;
                int i2 = (group + pair + step) * 2;
                float ur = data[i2] * tr - data[i2 + 1] * ti;
                float ui = data[i2] * ti + data[i2 + 1] * tr;
                data[i2] = data[i1] - ur;
                data[i2 + 1] = data[i1 + 1] - ui;
                data[i1] += ur;
                data[i1 + 1] += ui;
                float newTr = tr * wr - ti * wi;
                ti = tr * wi + ti * wr;
                tr = newTr;
            }
        }
    }

    return ESP_OK;
}

esp_err_t dsps_bit_rev4r_fc32(float *data, int N) {
    // Bit reversal already done inside dsps_fft4r_fc32
    (void)data;
    (void)N;
    return ESP_OK;
}

esp_err_t dsps_cplx2real_fc32(float *data, int N) {
    // No-op for native fallback — the complex data is already usable
    (void)data;
    (void)N;
    return ESP_OK;
}
