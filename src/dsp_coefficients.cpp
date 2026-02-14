#ifdef DSP_ENABLED

#include "dsp_coefficients.h"
#include "dsp_biquad_gen.h"
#include <math.h>

static float clamp_freq(float freq, uint32_t sampleRate) {
    float normalized = freq / (float)sampleRate;
    if (normalized < 0.0001f) normalized = 0.0001f;
    if (normalized > 0.4999f) normalized = 0.4999f;
    return normalized;
}

void dsp_compute_biquad_coeffs(DspBiquadParams &params, DspStageType type, uint32_t sampleRate) {
    float freq = clamp_freq(params.frequency, sampleRate);
    float Q = params.Q;
    if (Q <= 0.0f) Q = DSP_DEFAULT_Q;

    switch (type) {
        case DSP_BIQUAD_LPF:
            dsp_gen_lpf_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_HPF:
            dsp_gen_hpf_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_BPF:
            dsp_gen_bpf_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_NOTCH:
            dsp_gen_notch_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_PEQ:
            dsp_gen_peakingEQ_f32(params.coeffs, freq, params.gain, Q);
            break;
        case DSP_BIQUAD_LOW_SHELF:
            dsp_gen_lowShelf_f32(params.coeffs, freq, params.gain, Q);
            break;
        case DSP_BIQUAD_HIGH_SHELF:
            dsp_gen_highShelf_f32(params.coeffs, freq, params.gain, Q);
            break;
        case DSP_BIQUAD_ALLPASS:
        case DSP_BIQUAD_ALLPASS_360:
            dsp_gen_allpass360_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_ALLPASS_180:
            dsp_gen_allpass180_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_BPF_0DB:
            dsp_gen_bpf0db_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_CUSTOM:
            // Custom coefficients already loaded — don't overwrite
            break;
        case DSP_BIQUAD_LPF_1ST:
            dsp_gen_lpf1_f32(params.coeffs, freq);
            break;
        case DSP_BIQUAD_HPF_1ST:
            dsp_gen_hpf1_f32(params.coeffs, freq);
            break;
        case DSP_BIQUAD_LINKWITZ: {
            float fp_norm = clamp_freq(params.gain, sampleRate); // gain field repurposed as Fp Hz
            float Q2 = params.Q2;
            if (Q2 <= 0.0f) Q2 = DSP_DEFAULT_Q;
            dsp_gen_linkwitz_f32(params.coeffs, freq, Q, fp_norm, Q2);
            break;
        }
        default:
            // Non-biquad types: set passthrough
            params.coeffs[0] = 1.0f;
            params.coeffs[1] = 0.0f;
            params.coeffs[2] = 0.0f;
            params.coeffs[3] = 0.0f;
            params.coeffs[4] = 0.0f;
            break;
    }
}

void dsp_load_custom_coeffs(DspBiquadParams &params, float b0, float b1, float b2, float a1, float a2) {
    params.coeffs[0] = b0;
    params.coeffs[1] = b1;
    params.coeffs[2] = b2;
    params.coeffs[3] = a1;
    params.coeffs[4] = a2;
}

void dsp_recompute_channel_coeffs(DspChannelConfig &ch, uint32_t sampleRate) {
    for (int i = 0; i < ch.stageCount; i++) {
        DspStage &s = ch.stages[i];
        if (dsp_is_biquad_type(s.type)) {
            dsp_compute_biquad_coeffs(s.biquad, s.type, sampleRate);
        } else if (s.type == DSP_GAIN) {
            dsp_compute_gain_linear(s.gain);
        } else if (s.type == DSP_COMPRESSOR) {
            dsp_compute_compressor_makeup(s.compressor);
        }
    }
}

void dsp_compute_gain_linear(DspGainParams &params) {
    params.gainLinear = powf(10.0f, params.gainDb / 20.0f);
    params.currentLinear = params.gainLinear; // No ramp on init/load
}

void dsp_compute_compressor_makeup(DspCompressorParams &params) {
    params.makeupLinear = powf(10.0f, params.makeupGainDb / 20.0f);
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void dsp_compute_decimation_filter(float *taps, int numTaps, int decimFactor, float sampleRate) {
    if (!taps || numTaps <= 0 || decimFactor <= 0 || sampleRate <= 0.0f) return;

    // Normalized cutoff frequency (0..1, where 1 = Fs/2)
    float fc = 1.0f / (float)decimFactor;  // Cutoff at Fs_new / 2
    int M = numTaps - 1;
    float sum = 0.0f;

    for (int i = 0; i < numTaps; i++) {
        float n = (float)i - (float)M / 2.0f;
        // Windowed-sinc: sinc(2*fc*n) * Blackman window
        float sinc;
        if (fabsf(n) < 1e-6f) {
            sinc = 2.0f * fc;
        } else {
            sinc = sinf(2.0f * (float)M_PI * fc * n) / ((float)M_PI * n);
        }
        // Blackman window
        float w = 0.42f - 0.5f * cosf(2.0f * (float)M_PI * (float)i / (float)M)
                  + 0.08f * cosf(4.0f * (float)M_PI * (float)i / (float)M);
        taps[i] = sinc * w;
        sum += taps[i];
    }

    // Normalize to unity DC gain
    if (fabsf(sum) > 1e-10f) {
        for (int i = 0; i < numTaps; i++) taps[i] /= sum;
    }
}

#endif // DSP_ENABLED
