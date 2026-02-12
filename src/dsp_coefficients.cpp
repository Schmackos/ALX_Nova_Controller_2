#ifdef DSP_ENABLED

#include "dsp_coefficients.h"
#include "dsps_biquad_gen.h"
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
            dsps_biquad_gen_lpf_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_HPF:
            dsps_biquad_gen_hpf_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_BPF:
            dsps_biquad_gen_bpf_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_NOTCH:
            dsps_biquad_gen_notch_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_PEQ:
            dsps_biquad_gen_peakingEQ_f32(params.coeffs, freq, params.gain, Q);
            break;
        case DSP_BIQUAD_LOW_SHELF:
            dsps_biquad_gen_lowShelf_f32(params.coeffs, freq, params.gain, Q);
            break;
        case DSP_BIQUAD_HIGH_SHELF:
            dsps_biquad_gen_highShelf_f32(params.coeffs, freq, params.gain, Q);
            break;
        case DSP_BIQUAD_ALLPASS:
            dsps_biquad_gen_allpass_f32(params.coeffs, freq, Q);
            break;
        case DSP_BIQUAD_CUSTOM:
            // Custom coefficients already loaded — don't overwrite
            break;
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
        if (s.type <= DSP_BIQUAD_CUSTOM) {
            dsp_compute_biquad_coeffs(s.biquad, s.type, sampleRate);
        } else if (s.type == DSP_GAIN) {
            dsp_compute_gain_linear(s.gain);
        }
    }
}

void dsp_compute_gain_linear(DspGainParams &params) {
    params.gainLinear = powf(10.0f, params.gainDb / 20.0f);
}

#endif // DSP_ENABLED
