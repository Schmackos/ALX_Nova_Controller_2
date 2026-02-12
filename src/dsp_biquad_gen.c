// Biquad coefficient generators (Robert Bristow-Johnson Audio EQ Cookbook)
// Renamed from dsps_biquad_gen_* to dsp_gen_* to avoid symbol conflicts
// with the pre-built ESP-DSP library (which has different signatures).
// Based on ESP-DSP by Espressif Systems (Apache 2.0 License)
#include "dsp_biquad_gen.h"
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

int dsp_gen_lpf_f32(float *coeffs, float freq, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return -1;
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
    return 0;
}

int dsp_gen_hpf_f32(float *coeffs, float freq, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return -1;
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
    return 0;
}

int dsp_gen_bpf_f32(float *coeffs, float freq, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return -1;
    }
    float w0 = 2.0f * (float)M_PI * freq;
    float sinW0 = sinf(w0);
    float cosW0 = cosf(w0);
    float alpha = sinW0 / (2.0f * qFactor);

    float a0 = 1.0f + alpha;
    coeffs[0] = alpha;                   // b0
    coeffs[1] = 0.0f;                    // b1
    coeffs[2] = -alpha;                  // b2
    coeffs[3] = -2.0f * cosW0;          // a1
    coeffs[4] = 1.0f - alpha;           // a2

    normalize(coeffs, a0);
    return 0;
}

int dsp_gen_notch_f32(float *coeffs, float freq, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return -1;
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
    return 0;
}

int dsp_gen_allpass_f32(float *coeffs, float freq, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return -1;
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
    return 0;
}

// Allpass 360 -- same as standard second-order allpass
int dsp_gen_allpass360_f32(float *coeffs, float freq, float qFactor)
{
    return dsp_gen_allpass_f32(coeffs, freq, qFactor);
}

// Allpass 180 -- first-order allpass for 180 phase shift at target frequency
int dsp_gen_allpass180_f32(float *coeffs, float freq, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f) {
        return -1;
    }
    (void)qFactor; // Not used for first-order allpass
    float t = tanf((float)M_PI * freq);
    float a = (t - 1.0f) / (t + 1.0f);

    coeffs[0] = a;      // b0
    coeffs[1] = 1.0f;   // b1
    coeffs[2] = 0.0f;   // b2
    coeffs[3] = a;       // a1
    coeffs[4] = 0.0f;   // a2

    return 0;
}

// BPF with 0dB peak gain
int dsp_gen_bpf0db_f32(float *coeffs, float freq, float qFactor)
{
    return dsp_gen_bpf_f32(coeffs, freq, qFactor);
}

int dsp_gen_peakingEQ_f32(float *coeffs, float freq, float gain, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return -1;
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
    return 0;
}

int dsp_gen_lowShelf_f32(float *coeffs, float freq, float gain, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return -1;
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
    return 0;
}

int dsp_gen_highShelf_f32(float *coeffs, float freq, float gain, float qFactor)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f || qFactor <= 0.0f) {
        return -1;
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
    return 0;
}

// First-order LPF as biquad: H(z) = w/(1+w) * (1 + z^-1) / (1 - ((w-1)/(w+1)) * z^-1)
// where w = tan(pi * freq_normalized)
int dsp_gen_lpf1_f32(float *coeffs, float freq)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f) {
        return -1;
    }
    float w = tanf((float)M_PI * freq);
    float n = 1.0f / (1.0f + w);

    coeffs[0] = w * n;              // b0
    coeffs[1] = w * n;              // b1
    coeffs[2] = 0.0f;               // b2
    coeffs[3] = (w - 1.0f) * n;     // a1
    coeffs[4] = 0.0f;               // a2

    return 0;
}

// First-order HPF as biquad: H(z) = 1/(1+w) * (1 - z^-1) / (1 - ((w-1)/(w+1)) * z^-1)
// where w = tan(pi * freq_normalized)
int dsp_gen_hpf1_f32(float *coeffs, float freq)
{
    if (!coeffs || freq <= 0.0f || freq >= 0.5f) {
        return -1;
    }
    float w = tanf((float)M_PI * freq);
    float n = 1.0f / (1.0f + w);

    coeffs[0] = n;                   // b0
    coeffs[1] = -n;                  // b1
    coeffs[2] = 0.0f;               // b2
    coeffs[3] = (w - 1.0f) * n;     // a1
    coeffs[4] = 0.0f;               // a2

    return 0;
}
