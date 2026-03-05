#ifndef OUTPUT_DSP_H
#define OUTPUT_DSP_H

#ifdef DSP_ENABLED

#include "dsp_pipeline.h"
#include <stdint.h>
#include <stdbool.h>

// Per-output mono DSP engine — runs post-matrix, pre-sink in audio pipeline.
// Separate from the pre-matrix stereo DSP engine (dsp_pipeline.h) because:
// 1. Operates on mono float channels (not stereo int32 pairs)
// 2. Post-matrix processing (crossover, per-output EQ, limiting)
// 3. Lightweight: no FIR/delay pools, no PEQ band convention

#ifndef OUTPUT_DSP_MAX_CHANNELS
#define OUTPUT_DSP_MAX_CHANNELS 8   // One per mono output in 8x8 matrix
#endif

#ifndef OUTPUT_DSP_MAX_STAGES
#define OUTPUT_DSP_MAX_STAGES 12    // Crossover + EQ + limiter + gain per channel
#endif

// ===== Output DSP Stage (subset of DspStage — biquad, gain, limiter, mute, polarity) =====
struct OutputDspStage {
    bool enabled;
    DspStageType type;
    char label[16];

    union {
        DspBiquadParams biquad;
        DspLimiterParams limiter;
        DspGainParams gain;
        DspPolarityParams polarity;
        DspMuteParams mute;
        DspCompressorParams compressor;
    };
};

// ===== Per-Channel Configuration =====
struct OutputDspChannelConfig {
    bool bypass;
    uint8_t stageCount;
    OutputDspStage stages[OUTPUT_DSP_MAX_STAGES];
};

// ===== Full Output DSP State =====
struct OutputDspState {
    bool globalBypass;
    uint32_t sampleRate;
    OutputDspChannelConfig channels[OUTPUT_DSP_MAX_CHANNELS];
};

// ===== Initialization helpers =====
inline void output_dsp_init_stage(OutputDspStage &s, DspStageType t = DSP_BIQUAD_PEQ) {
    s.enabled = true;
    s.type = t;
    s.label[0] = '\0';
    if (t == DSP_LIMITER) {
        dsp_init_limiter_params(s.limiter);
    } else if (t == DSP_GAIN) {
        dsp_init_gain_params(s.gain);
    } else if (t == DSP_POLARITY) {
        dsp_init_polarity_params(s.polarity);
    } else if (t == DSP_MUTE) {
        dsp_init_mute_params(s.mute);
    } else if (t == DSP_COMPRESSOR) {
        dsp_init_compressor_params(s.compressor);
    } else if (dsp_is_biquad_type(t)) {
        dsp_init_biquad_params(s.biquad);
    } else {
        dsp_init_biquad_params(s.biquad);
    }
}

inline void output_dsp_init_channel(OutputDspChannelConfig &ch) {
    ch.bypass = true;  // Bypass by default — no processing until user configures
    ch.stageCount = 0;
}

inline void output_dsp_init_state(OutputDspState &st) {
    st.globalBypass = false;
    st.sampleRate = 48000;
    for (int i = 0; i < OUTPUT_DSP_MAX_CHANNELS; i++) {
        output_dsp_init_channel(st.channels[i]);
    }
}

// ===== Public API =====
void output_dsp_init();

// Process a mono float buffer in-place for the given output channel.
// buf: mono float [-1.0, +1.0], frames: number of samples.
void output_dsp_process(int ch, float *buf, int frames);

// Double-buffered config access (same pattern as dsp_pipeline.h)
OutputDspState* output_dsp_get_active_config();
OutputDspState* output_dsp_get_inactive_config();
bool output_dsp_swap_config();

// Deep copy active → inactive
void output_dsp_copy_active_to_inactive();

// Stage CRUD (operates on inactive config)
int  output_dsp_add_stage(int channel, DspStageType type, int position = -1);
bool output_dsp_remove_stage(int channel, int stageIndex);
bool output_dsp_set_stage_enabled(int channel, int stageIndex, bool enabled);

// Crossover convenience: inserts LPF on subCh, HPF on mainCh at given frequency
int output_dsp_setup_crossover(int subCh, int mainCh, float freqHz, int order);

// Persistence
void output_dsp_save_channel(int ch);
void output_dsp_load_channel(int ch);
void output_dsp_save_all();
void output_dsp_load_all();

#endif // DSP_ENABLED
#endif // OUTPUT_DSP_H
