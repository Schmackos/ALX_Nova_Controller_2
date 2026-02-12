// ESP-DSP Lite — Window functions (ANSI C fallback for native tests)
// Formulas from: https://en.wikipedia.org/wiki/Window_function

#include "dsps_wind.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void dsps_wind_hann_f32(float *window, int len) {
    for (int i = 0; i < len; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (len - 1)));
    }
}

void dsps_wind_blackman_f32(float *window, int len) {
    // Exact Blackman (alpha = 0.16)
    const float a0 = 0.42f;
    const float a1 = 0.50f;
    const float a2 = 0.08f;
    for (int i = 0; i < len; i++) {
        float t = 2.0f * (float)M_PI * i / (len - 1);
        window[i] = a0 - a1 * cosf(t) + a2 * cosf(2.0f * t);
    }
}

void dsps_wind_blackman_harris_f32(float *window, int len) {
    const float a0 = 0.35875f;
    const float a1 = 0.48829f;
    const float a2 = 0.14128f;
    const float a3 = 0.01168f;
    for (int i = 0; i < len; i++) {
        float t = 2.0f * (float)M_PI * i / (len - 1);
        window[i] = a0 - a1 * cosf(t) + a2 * cosf(2.0f * t) - a3 * cosf(3.0f * t);
    }
}

void dsps_wind_blackman_nuttall_f32(float *window, int len) {
    const float a0 = 0.3635819f;
    const float a1 = 0.4891775f;
    const float a2 = 0.1365995f;
    const float a3 = 0.0106411f;
    for (int i = 0; i < len; i++) {
        float t = 2.0f * (float)M_PI * i / (len - 1);
        window[i] = a0 - a1 * cosf(t) + a2 * cosf(2.0f * t) - a3 * cosf(3.0f * t);
    }
}

void dsps_wind_nuttall_f32(float *window, int len) {
    const float a0 = 0.355768f;
    const float a1 = 0.487396f;
    const float a2 = 0.144232f;
    const float a3 = 0.012604f;
    for (int i = 0; i < len; i++) {
        float t = 2.0f * (float)M_PI * i / (len - 1);
        window[i] = a0 - a1 * cosf(t) + a2 * cosf(2.0f * t) - a3 * cosf(3.0f * t);
    }
}

void dsps_wind_flat_top_f32(float *window, int len) {
    const float a0 = 0.21557895f;
    const float a1 = 0.41663158f;
    const float a2 = 0.277263158f;
    const float a3 = 0.083578947f;
    const float a4 = 0.006947368f;
    for (int i = 0; i < len; i++) {
        float t = 2.0f * (float)M_PI * i / (len - 1);
        window[i] = a0 - a1 * cosf(t) + a2 * cosf(2.0f * t) - a3 * cosf(3.0f * t) + a4 * cosf(4.0f * t);
    }
}
