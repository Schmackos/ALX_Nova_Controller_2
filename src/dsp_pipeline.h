#ifndef DSP_PIPELINE_H
#define DSP_PIPELINE_H

#ifdef DSP_ENABLED

#ifndef NATIVE_TEST
#include "config.h"
#endif
#include <stdint.h>

// ===== Stage Types =====
enum DspStageType : uint8_t {
    DSP_BIQUAD_LPF = 0,
    DSP_BIQUAD_HPF,
    DSP_BIQUAD_BPF,
    DSP_BIQUAD_NOTCH,
    DSP_BIQUAD_PEQ,
    DSP_BIQUAD_LOW_SHELF,
    DSP_BIQUAD_HIGH_SHELF,
    DSP_BIQUAD_ALLPASS,     // allpass (alias for 360° phase shift)
    DSP_BIQUAD_ALLPASS_360, // allpass 360° (same as ALLPASS, explicit)
    DSP_BIQUAD_ALLPASS_180, // allpass 180° phase shift
    DSP_BIQUAD_BPF_0DB,    // bandpass 0dB peak gain
    DSP_BIQUAD_CUSTOM,
    DSP_LIMITER,
    DSP_FIR,
    DSP_GAIN,
    DSP_DELAY,
    DSP_POLARITY,
    DSP_MUTE,
    DSP_COMPRESSOR,
    DSP_BIQUAD_LPF_1ST = 19,   // First-order LPF (b2=0, a2=0)
    DSP_BIQUAD_HPF_1ST = 20,   // First-order HPF (b2=0, a2=0)
    DSP_STAGE_TYPE_COUNT
};

// Check if a stage type is a biquad-processed type (including first-order filters)
inline bool dsp_is_biquad_type(DspStageType type) {
    return type <= DSP_BIQUAD_CUSTOM || type == DSP_BIQUAD_LPF_1ST || type == DSP_BIQUAD_HPF_1ST;
}

// ===== Biquad Parameters =====
struct DspBiquadParams {
    float frequency;    // Center/corner frequency (Hz)
    float gain;         // Gain in dB (PEQ, shelf types)
    float Q;            // Quality factor
    float coeffs[5];    // [b0, b1, b2, a1, a2]
    float delay[2];     // Biquad delay line (state)
};

// ===== Limiter Parameters =====
struct DspLimiterParams {
    float thresholdDb;  // Threshold in dBFS
    float attackMs;     // Attack time (ms)
    float releaseMs;    // Release time (ms)
    float ratio;        // Compression ratio (20:1 ~ limiting)
    float envelope;     // Current envelope level (runtime)
    float gainReduction;// Current GR in dB (runtime, for metering)
};

// ===== FIR Parameters (taps/delay stored in external pool) =====
struct DspFirParams {
    uint16_t numTaps;
    uint16_t delayPos;
    int8_t firSlot;     // Index into static FIR pool (-1 = unassigned)
};

// ===== Gain Parameters =====
struct DspGainParams {
    float gainDb;       // Gain in dB
    float gainLinear;   // Pre-computed linear gain (10^(dB/20))
};

// ===== Delay Parameters (delay line stored in external pool) =====
struct DspDelayParams {
    uint16_t delaySamples;  // Delay in samples (max DSP_MAX_DELAY_SAMPLES)
    uint16_t writePos;      // Current write position in circular buffer
    int8_t delaySlot;       // Index into static delay pool (-1 = unassigned)
};

// ===== Polarity Parameters =====
struct DspPolarityParams {
    bool inverted;          // true = phase invert (multiply by -1)
};

// ===== Mute Parameters =====
struct DspMuteParams {
    bool muted;             // true = output silence
};

// ===== Compressor Parameters =====
struct DspCompressorParams {
    float thresholdDb;      // Threshold in dBFS
    float attackMs;         // Attack time (ms)
    float releaseMs;        // Release time (ms)
    float ratio;            // Compression ratio (1:1 to inf:1)
    float kneeDb;           // Soft knee width in dB (0 = hard knee)
    float makeupGainDb;     // Makeup gain in dB
    float makeupLinear;     // Pre-computed linear makeup gain
    float envelope;         // Current envelope level (runtime)
    float gainReduction;    // Current GR in dB (runtime, for metering)
};

// ===== Generic DSP Stage =====
struct DspStage {
    bool enabled;
    DspStageType type;
    char label[16];

    union {
        DspBiquadParams biquad;
        DspLimiterParams limiter;
        DspFirParams fir;
        DspGainParams gain;
        DspDelayParams delay;
        DspPolarityParams polarity;
        DspMuteParams mute;
        DspCompressorParams compressor;
    };
};

// ===== Channel Configuration =====
struct DspChannelConfig {
    bool bypass;
    uint8_t stageCount;
    DspStage stages[DSP_MAX_STAGES];
};

// ===== DSP Metrics =====
struct DspMetrics {
    uint32_t processTimeUs;     // Last buffer processing time (us)
    uint32_t maxProcessTimeUs;  // Peak processing time (us)
    float cpuLoadPercent;       // DSP CPU usage estimate (%)
    float limiterGrDb[DSP_MAX_CHANNELS]; // Per-channel limiter GR (dB)
};

// ===== Global DSP State =====
struct DspState {
    bool globalBypass;
    uint32_t sampleRate;
    DspChannelConfig channels[DSP_MAX_CHANNELS];
};

// ===== Initialization helpers =====
inline void dsp_init_biquad_params(DspBiquadParams &p) {
    p.frequency = 1000.0f;
    p.gain = 0.0f;
    p.Q = DSP_DEFAULT_Q;
    p.coeffs[0] = 1.0f; p.coeffs[1] = 0; p.coeffs[2] = 0;
    p.coeffs[3] = 0; p.coeffs[4] = 0;
    p.delay[0] = 0; p.delay[1] = 0;
}

inline void dsp_init_limiter_params(DspLimiterParams &p) {
    p.thresholdDb = 0.0f;
    p.attackMs = 5.0f;
    p.releaseMs = 50.0f;
    p.ratio = 20.0f;
    p.envelope = 0.0f;
    p.gainReduction = 0.0f;
}

inline void dsp_init_fir_params(DspFirParams &p) {
    p.numTaps = 0;
    p.delayPos = 0;
    p.firSlot = -1;
}

inline void dsp_init_gain_params(DspGainParams &p) {
    p.gainDb = 0.0f;
    p.gainLinear = 1.0f;
}

inline void dsp_init_delay_params(DspDelayParams &p) {
    p.delaySamples = 0;
    p.writePos = 0;
    p.delaySlot = -1;
}

inline void dsp_init_polarity_params(DspPolarityParams &p) {
    p.inverted = true;
}

inline void dsp_init_mute_params(DspMuteParams &p) {
    p.muted = true;
}

inline void dsp_init_compressor_params(DspCompressorParams &p) {
    p.thresholdDb = -12.0f;
    p.attackMs = 10.0f;
    p.releaseMs = 100.0f;
    p.ratio = 4.0f;
    p.kneeDb = 6.0f;
    p.makeupGainDb = 0.0f;
    p.makeupLinear = 1.0f;
    p.envelope = 0.0f;
    p.gainReduction = 0.0f;
}

inline void dsp_init_stage(DspStage &s, DspStageType t = DSP_BIQUAD_PEQ) {
    s.enabled = true;
    s.type = t;
    s.label[0] = '\0';
    if (t == DSP_LIMITER) {
        dsp_init_limiter_params(s.limiter);
    } else if (t == DSP_FIR) {
        dsp_init_fir_params(s.fir);
    } else if (t == DSP_GAIN) {
        dsp_init_gain_params(s.gain);
    } else if (t == DSP_DELAY) {
        dsp_init_delay_params(s.delay);
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

inline void dsp_init_channel(DspChannelConfig &ch) {
    ch.bypass = false;
    ch.stageCount = 0;
}

inline void dsp_init_state(DspState &st) {
    st.globalBypass = false;
    st.sampleRate = 48000;
    for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
        dsp_init_channel(st.channels[i]);
    }
}

inline void dsp_init_metrics(DspMetrics &m) {
    m.processTimeUs = 0;
    m.maxProcessTimeUs = 0;
    m.cpuLoadPercent = 0.0f;
    for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
        m.limiterGrDb[i] = 0.0f;
    }
}

// ===== Public API =====
void dsp_init();
void dsp_process_buffer(int32_t *buffer, int stereoFrames, int adcIndex);

// Config access (double-buffered: active = read by audio task, inactive = modified by API)
DspState *dsp_get_active_config();
DspState *dsp_get_inactive_config();
void dsp_swap_config();

// Deep copy active config to inactive (includes FIR pool data)
void dsp_copy_active_to_inactive();

// Metrics
DspMetrics dsp_get_metrics();
void dsp_reset_max_metrics();

// Stage CRUD (operates on inactive config)
int dsp_add_stage(int channel, DspStageType type, int position = -1);
bool dsp_remove_stage(int channel, int stageIndex);
bool dsp_reorder_stages(int channel, const int *newOrder, int count);
bool dsp_set_stage_enabled(int channel, int stageIndex, bool enabled);

// FIR pool access (taps/delay stored outside DspStage union to save DRAM)
int dsp_fir_alloc_slot();                              // Allocate slot, returns index or -1
void dsp_fir_free_slot(int slot);                      // Release slot
float* dsp_fir_get_taps(int stateIndex, int firSlot);  // Get taps array [DSP_MAX_FIR_TAPS]
float* dsp_fir_get_delay(int stateIndex, int firSlot); // Get delay array [DSP_MAX_FIR_TAPS]

// Delay pool access (delay lines stored outside DspStage union to save DRAM)
int dsp_delay_alloc_slot();                                   // Allocate slot, returns index or -1
void dsp_delay_free_slot(int slot);                           // Release slot
float* dsp_delay_get_line(int stateIndex, int delaySlot);     // Get delay line [DSP_MAX_DELAY_SAMPLES]

// Persistence helpers
void dsp_load_config_from_json(const char *json, int channel);
void dsp_export_config_to_json(int channel, char *buf, int bufSize);
void dsp_export_full_config_json(char *buf, int bufSize);
void dsp_import_full_config_json(const char *json);

#endif // DSP_ENABLED
#endif // DSP_PIPELINE_H
