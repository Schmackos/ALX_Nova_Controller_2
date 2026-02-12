// ESP-DSP Lite — Biquad coefficient generators (Robert Bristow-Johnson Audio EQ Cookbook)
// Based on ESP-DSP by Espressif Systems (Apache 2.0 License)
#include "dsps_biquad_gen.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper: normalize coefficients so a0 = 1.0
static void normalize(float *coeffs, float a0)
{
    float inv_a0 = 1.0f / a0;
    coeffs[0] *= inv_a0; // b0
    coeffs[1] *= inv_a0; // b1
    coeffs[2] *= inv_a0; // b2
    coeffs[3] *= inv_a0; // a1
    coeffs[4] *= inv_a0; // a2
}

esp_err_t dsps_biquad_gen_lpf_f32(float *coeffs, float freq, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }
    float w0 = 2.0f * (float)M_PI * freq;
    float sinW0 = sinf(w0);
    float cosW0 = cosf(w0);
    float alpha = sinW0 / (2.0f * qFactor);

    float a0 = 1.0f + alpha;
    coeffs[0] = (1.0f - cosW0) / 2.0f; // b0
    coeffs[1] = 1.0f - cosW0;           // b1
    coeffs[2] = (1.0f - cosW0) / 2.0f;  // b2
    coeffs[3] = -2.0f * cosW0;          // a1
    coeffs[4] = 1.0f - alpha;           // a2

    normalize(coeffs, a0);
    return ESP_OK;
}

esp_err_t dsps_biquad_gen_hpf_f32(float *coeffs, float freq, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }
    float w0 = 2.0f * (float)M_PI * freq;
    float sinW0 = sinf(w0);
    float cosW0 = cosf(w0);
    float alpha = sinW0 / (2.0f * qFactor);

    float a0 = 1.0f + alpha;
    coeffs[0] = (1.0f + cosW0) / 2.0f;  // b0
    coeffs[1] = -(1.0f + cosW0);         // b1
    coeffs[2] = (1.0f + cosW0) / 2.0f;   // b2
    coeffs[3] = -2.0f * cosW0;           // a1
    coeffs[4] = 1.0f - alpha;            // a2

    normalize(coeffs, a0);
    return ESP_OK;
}

esp_err_t dsps_biquad_gen_bpf_f32(float *coeffs, float freq, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }
    float w0 = 2.0f * (float)M_PI * freq;
    float sinW0 = sinf(w0);
    float cosW0 = cosf(w0);
    float alpha = sinW0 / (2.0f * qFactor);

    float a0 = 1.0f + alpha;
    coeffs[0] = alpha;                   // b0 (constant-0-dB-peak-gain BPF)
    coeffs[1] = 0.0f;                    // b1
    coeffs[2] = -alpha;                  // b2
    coeffs[3] = -2.0f * cosW0;          // a1
    coeffs[4] = 1.0f - alpha;           // a2

    normalize(coeffs, a0);
    return ESP_OK;
}

esp_err_t dsps_biquad_gen_notch_f32(float *coeffs, float freq, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }
    float w0 = 2.0f * (float)M_PI * freq;
    float sinW0 = sinf(w0);
    float cosW0 = cosf(w0);
    float alpha = sinW0 / (2.0f * qFactor);

    float a0 = 1.0f + alpha;
    coeffs[0] = 1.0f;                   // b0
    coeffs[1] = -2.0f * cosW0;          // b1
    coeffs[2] = 1.0f;                   // b2
    coeffs[3] = -2.0f * cosW0;          // a1
    coeffs[4] = 1.0f - alpha;           // a2

    normalize(coeffs, a0);
    return ESP_OK;
}

esp_err_t dsps_biquad_gen_allpass_f32(float *coeffs, float freq, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }
    float w0 = 2.0f * (float)M_PI * freq;
    float sinW0 = sinf(w0);
    float cosW0 = cosf(w0);
    float alpha = sinW0 / (2.0f * qFactor);

    float a0 = 1.0f + alpha;
    coeffs[0] = 1.0f - alpha;           // b0
    coeffs[1] = -2.0f * cosW0;          // b1
    coeffs[2] = 1.0f + alpha;           // b2
    coeffs[3] = -2.0f * cosW0;          // a1
    coeffs[4] = 1.0f - alpha;           // a2

    normalize(coeffs, a0);
    return ESP_OK;
}

esp_err_t dsps_biquad_gen_peakingEQ_f32(float *coeffs, float freq, float gain, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }
    float A = powf(10.0f, gain / 40.0f); // sqrt(10^(dB/20))
    float w0 = 2.0f * (float)M_PI * freq;
    float sinW0 = sinf(w0);
    float cosW0 = cosf(w0);
    float alpha = sinW0 / (2.0f * qFactor);

    float a0 = 1.0f + alpha / A;
    coeffs[0] = 1.0f + alpha * A;       // b0
    coeffs[1] = -2.0f * cosW0;          // b1
    coeffs[2] = 1.0f - alpha * A;       // b2
    coeffs[3] = -2.0f * cosW0;          // a1
    coeffs[4] = 1.0f - alpha / A;       // a2

    normalize(coeffs, a0);
    return ESP_OK;
}

esp_err_t dsps_biquad_gen_lowShelf_f32(float *coeffs, float freq, float gain, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }
    float A = powf(10.0f, gain / 40.0f);
    float w0 = 2.0f * (float)M_PI * freq;
    float sinW0 = sinf(w0);
    float cosW0 = cosf(w0);
    float alpha = sinW0 / (2.0f * qFactor);
    float twoSqrtAalpha = 2.0f * sqrtf(A) * alpha;

    float a0 = (A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAalpha;
    coeffs[0] = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAalpha); // b0
    coeffs[1] = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0);           // b1
    coeffs[2] = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAalpha); // b2
    coeffs[3] = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0);              // a1
    coeffs[4] = (A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAalpha;       // a2

    normalize(coeffs, a0);
    return ESP_OK;
}

esp_err_t dsps_biquad_gen_highShelf_f32(float *coeffs, float freq, float gain, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return ESP_ERR_DSP_INVALID_PARAM;
    }
    float A = powf(10.0f, gain / 40.0f);
    float w0 = 2.0f * (float)M_PI * freq;
    float sinW0 = sinf(w0);
    float cosW0 = cosf(w0);
    float alpha = sinW0 / (2.0f * qFactor);
    float twoSqrtAalpha = 2.0f * sqrtf(A) * alpha;

    float a0 = (A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAalpha;
    coeffs[0] = A * ((A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAalpha); // b0
    coeffs[1] = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0);          // b1
    coeffs[2] = A * ((A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAalpha); // b2
    coeffs[3] = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0);               // a1
    coeffs[4] = (A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAalpha;       // a2

    normalize(coeffs, a0);
    return ESP_OK;
}
