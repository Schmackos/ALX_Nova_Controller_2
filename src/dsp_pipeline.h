#ifndef DSP_PIPELINE_H
#define DSP_PIPELINE_H

#ifdef DSP_ENABLED

#ifndef NATIVE_TEST
#include "config.h"
#endif
#include <stdint.h>
#include <math.h>

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
    DSP_BIQUAD_LINKWITZ = 21,  // Linkwitz Transform (F0, Q0, Fp, Qp)
    DSP_DECIMATOR = 22,        // Decimation FIR (integer downsampling)
    DSP_CONVOLUTION = 23,      // Partitioned convolution (room correction IR)
    DSP_NOISE_GATE = 24,       // Noise gate / expander
    DSP_TONE_CTRL = 25,        // 3-band bass/mid/treble tone controls
    DSP_SPEAKER_PROT = 26,     // Speaker protection (thermal + excursion)
    DSP_STEREO_WIDTH = 27,     // Stereo width / mid-side processing
    DSP_LOUDNESS = 28,         // Fletcher-Munson loudness compensation
    DSP_BASS_ENHANCE = 29,     // Psychoacoustic bass enhancement
    DSP_MULTIBAND_COMP = 30,   // Multi-band compressor (2-4 bands)
    DSP_STAGE_TYPE_COUNT
};

// Check if a stage type is a biquad-processed type (including first-order filters)
inline bool dsp_is_biquad_type(DspStageType type) {
    return type <= DSP_BIQUAD_CUSTOM || type == DSP_BIQUAD_LPF_1ST || type == DSP_BIQUAD_HPF_1ST
           || type == DSP_BIQUAD_LINKWITZ;
}

// ===== Biquad Parameters =====
struct DspBiquadParams {
    float frequency;    // Center/corner frequency (Hz); F0 for Linkwitz
    float gain;         // Gain in dB (PEQ, shelf); Fp Hz for Linkwitz
    float Q;            // Quality factor; Q0 for Linkwitz
    float Q2;           // Second Q factor (Qp for Linkwitz Transform)
    float coeffs[5];    // [b0, b1, b2, a1, a2]
    float delay[2];     // Biquad delay line (state)
    // Coefficient morphing for glitch-free PEQ updates
    float targetCoeffs[5]; // Target coefficients (when morphing)
    uint16_t morphRemaining; // Samples remaining in morph (0 = settled)
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
    float currentLinear;// Current ramping value (runtime state, not persisted)
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

// ===== Decimator Parameters =====
struct DspDecimatorParams {
    uint8_t  factor;    // Decimation factor (2, 4, or 8)
    int8_t   firSlot;   // Index into FIR pool (-1 = unassigned)
    uint16_t numTaps;   // Anti-aliasing filter tap count
    uint16_t delayPos;  // Current position in FIR delay line
};

// ===== Convolution Parameters =====
struct DspConvolutionParams {
    int8_t convSlot;    // Index into convolution slot pool (-1 = unassigned)
    uint16_t irLength;  // Original IR length in samples
    char irFilename[32]; // Source WAV filename
};

// ===== Noise Gate Parameters =====
struct DspNoiseGateParams {
    float thresholdDb;  // Threshold in dBFS (-80..0)
    float attackMs;     // Attack time (ms)
    float holdMs;       // Hold time after signal drops (ms)
    float releaseMs;    // Release time (ms)
    float ratio;        // Expansion ratio (1=hard gate, >1=expander)
    float rangeDb;      // Maximum attenuation (dB, -80..0)
    float envelope;     // Current envelope level (runtime)
    float gainReduction;// Current GR in dB (runtime)
    float holdCounter;  // Hold timer in samples (runtime)
};

// ===== Tone Control Parameters =====
struct DspToneCtrlParams {
    float bassGain;     // Bass gain in dB (-12..+12)
    float midGain;      // Mid gain in dB (-12..+12)
    float trebleGain;   // Treble gain in dB (-12..+12)
    float bassCoeffs[5];   float bassDelay[2];
    float midCoeffs[5];    float midDelay[2];
    float trebleCoeffs[5]; float trebleDelay[2];
};

// ===== Speaker Protection Parameters =====
struct DspSpeakerProtParams {
    float powerRatingW;      // Speaker power rating (W)
    float impedanceOhms;     // Speaker impedance (Ohms)
    float thermalTauMs;      // Thermal time constant (ms)
    float excursionLimitMm;  // Maximum excursion (mm)
    float driverDiameterMm;  // Driver diameter (mm)
    float maxTempC;          // Maximum voice coil temp (C)
    float currentTempC;      // Current estimated temp (runtime)
    float envelope;          // Power envelope (runtime)
    float gainReduction;     // Current GR in dB (runtime)
};

// ===== Stereo Width Parameters =====
struct DspStereoWidthParams {
    float width;           // 0=mono, 100=normal, 200=extra wide
    float centerGainDb;    // Center/mid channel gain (-12..+12 dB)
    float centerGainLin;   // Precomputed linear gain
};

// ===== Loudness Compensation Parameters =====
struct DspLoudnessParams {
    float referenceLevelDb;  // Reference level (dB SPL at max volume)
    float currentLevelDb;    // Current listening level (dB SPL)
    float amount;            // Amount 0-100%
    float bassBoostDb;       // Computed bass boost (runtime)
    float trebleBoostDb;     // Computed treble boost (runtime)
    float bassCoeffs[5];  float bassDelay[2];
    float trebleCoeffs[5]; float trebleDelay[2];
};

// ===== Bass Enhancement Parameters =====
struct DspBassEnhanceParams {
    float frequency;       // Crossover frequency (Hz)
    float harmonicGainDb;  // Harmonic gain (-12..+12 dB)
    float harmonicGainLin; // Precomputed linear gain
    float mix;             // Wet/dry mix 0-100%
    uint8_t order;         // 0=2nd, 1=3rd, 2=both
    float hpfCoeffs[5]; float hpfDelay[2];
    float bpfCoeffs[5]; float bpfDelay[2];
};

// ===== Multi-Band Compressor Parameters (slim — band data in pool) =====
struct DspMultibandCompParams {
    uint8_t numBands;    // 2-4 bands
    int8_t mbSlot;       // Pool slot index (-1 = unassigned)
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
        DspDecimatorParams decimator;
        DspConvolutionParams convolution;
        DspNoiseGateParams noiseGate;
        DspToneCtrlParams toneCtrl;
        DspSpeakerProtParams speakerProt;
        DspStereoWidthParams stereoWidth;
        DspLoudnessParams loudness;
        DspBassEnhanceParams bassEnhance;
        DspMultibandCompParams multibandComp;
    };
};

// ===== Channel Configuration =====
struct DspChannelConfig {
    bool bypass;
    bool stereoLink;    // When true, channels 0+1 and 2+3 are linked pairs
    uint8_t stageCount;
    DspStage stages[DSP_MAX_STAGES];
};

// ===== Emergency Safety Limiter State =====
struct EmergencyLimiterState {
    float envelope;                           // Current envelope level (runtime)
    float gainReduction;                      // Current GR in dB (runtime)
    uint32_t triggerCount;                    // Lifetime trigger counter
    uint32_t samplesSinceTrigger;             // Samples since last trigger
    float lookahead[DSP_MAX_CHANNELS][8];     // Lookahead buffer (8 samples per channel)
    uint8_t lookaheadPos;                     // Current write position in lookahead
};

// ===== DSP Metrics =====
struct DspMetrics {
    uint32_t processTimeUs;     // Last buffer processing time (us)
    uint32_t maxProcessTimeUs;  // Peak processing time (us)
    float cpuLoadPercent;       // DSP CPU usage estimate (%)
    float limiterGrDb[DSP_MAX_CHANNELS]; // Per-channel limiter GR (dB)
    // Emergency limiter metrics
    float emergencyLimiterGrDb;              // Emergency limiter GR (dB)
    uint32_t emergencyLimiterTriggers;       // Lifetime trigger count
    bool emergencyLimiterActive;             // Currently limiting flag
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
    p.Q2 = DSP_DEFAULT_Q;
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
    p.currentLinear = 1.0f;
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

inline void dsp_init_decimator_params(DspDecimatorParams &p) {
    p.factor = 2;
    p.firSlot = -1;
    p.numTaps = 0;
    p.delayPos = 0;
}

inline void dsp_init_convolution_params(DspConvolutionParams &p) {
    p.convSlot = -1;
    p.irLength = 0;
    p.irFilename[0] = '\0';
}

inline void dsp_init_noise_gate_params(DspNoiseGateParams &p) {
    p.thresholdDb = -40.0f;
    p.attackMs = 1.0f;
    p.holdMs = 50.0f;
    p.releaseMs = 100.0f;
    p.ratio = 1.0f;  // Hard gate
    p.rangeDb = -80.0f;
    p.envelope = 0.0f;
    p.gainReduction = 0.0f;
    p.holdCounter = 0.0f;
}

inline void dsp_init_tone_ctrl_params(DspToneCtrlParams &p) {
    p.bassGain = 0.0f;
    p.midGain = 0.0f;
    p.trebleGain = 0.0f;
    for (int i = 0; i < 5; i++) { p.bassCoeffs[i] = 0; p.midCoeffs[i] = 0; p.trebleCoeffs[i] = 0; }
    p.bassCoeffs[0] = 1.0f; p.midCoeffs[0] = 1.0f; p.trebleCoeffs[0] = 1.0f;
    p.bassDelay[0] = p.bassDelay[1] = 0;
    p.midDelay[0] = p.midDelay[1] = 0;
    p.trebleDelay[0] = p.trebleDelay[1] = 0;
}

inline void dsp_init_speaker_prot_params(DspSpeakerProtParams &p) {
    p.powerRatingW = 100.0f;
    p.impedanceOhms = 8.0f;
    p.thermalTauMs = 2000.0f;
    p.excursionLimitMm = 5.0f;
    p.driverDiameterMm = 165.0f;
    p.maxTempC = 180.0f;
    p.currentTempC = 25.0f;
    p.envelope = 0.0f;
    p.gainReduction = 0.0f;
}

inline void dsp_init_stereo_width_params(DspStereoWidthParams &p) {
    p.width = 100.0f;
    p.centerGainDb = 0.0f;
    p.centerGainLin = 1.0f;
}

inline void dsp_init_loudness_params(DspLoudnessParams &p) {
    p.referenceLevelDb = 85.0f;
    p.currentLevelDb = 75.0f;
    p.amount = 100.0f;
    p.bassBoostDb = 0.0f;
    p.trebleBoostDb = 0.0f;
    for (int i = 0; i < 5; i++) { p.bassCoeffs[i] = 0; p.trebleCoeffs[i] = 0; }
    p.bassCoeffs[0] = 1.0f; p.trebleCoeffs[0] = 1.0f;
    p.bassDelay[0] = p.bassDelay[1] = 0;
    p.trebleDelay[0] = p.trebleDelay[1] = 0;
}

inline void dsp_init_bass_enhance_params(DspBassEnhanceParams &p) {
    p.frequency = 80.0f;
    p.harmonicGainDb = 0.0f;
    p.harmonicGainLin = 1.0f;
    p.mix = 50.0f;
    p.order = 2;  // Both 2nd and 3rd
    for (int i = 0; i < 5; i++) { p.hpfCoeffs[i] = 0; p.bpfCoeffs[i] = 0; }
    p.hpfCoeffs[0] = 1.0f; p.bpfCoeffs[0] = 1.0f;
    p.hpfDelay[0] = p.hpfDelay[1] = 0;
    p.bpfDelay[0] = p.bpfDelay[1] = 0;
}

inline void dsp_init_multiband_comp_params(DspMultibandCompParams &p) {
    p.numBands = 3;
    p.mbSlot = -1;
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
    } else if (t == DSP_DECIMATOR) {
        dsp_init_decimator_params(s.decimator);
    } else if (t == DSP_CONVOLUTION) {
        dsp_init_convolution_params(s.convolution);
    } else if (t == DSP_NOISE_GATE) {
        dsp_init_noise_gate_params(s.noiseGate);
    } else if (t == DSP_TONE_CTRL) {
        dsp_init_tone_ctrl_params(s.toneCtrl);
    } else if (t == DSP_SPEAKER_PROT) {
        dsp_init_speaker_prot_params(s.speakerProt);
    } else if (t == DSP_STEREO_WIDTH) {
        dsp_init_stereo_width_params(s.stereoWidth);
    } else if (t == DSP_LOUDNESS) {
        dsp_init_loudness_params(s.loudness);
    } else if (t == DSP_BASS_ENHANCE) {
        dsp_init_bass_enhance_params(s.bassEnhance);
    } else if (t == DSP_MULTIBAND_COMP) {
        dsp_init_multiband_comp_params(s.multibandComp);
    } else if (dsp_is_biquad_type(t)) {
        dsp_init_biquad_params(s.biquad);
    } else {
        dsp_init_biquad_params(s.biquad);
    }
}

inline void dsp_init_channel(DspChannelConfig &ch) {
    ch.bypass = false;
    ch.stereoLink = true;
    ch.stageCount = 0;
}

// Initialize PEQ bands (stages 0 through DSP_PEQ_BANDS-1) as disabled PEQ filters.
// Existing stages (if any) are shifted to chain region (index >= DSP_PEQ_BANDS).
inline void dsp_init_peq_bands(DspChannelConfig &ch) {
    // Shift existing stages to chain region
    if (ch.stageCount > 0) {
        int chainCount = ch.stageCount;
        if (chainCount + DSP_PEQ_BANDS > DSP_MAX_STAGES)
            chainCount = DSP_MAX_STAGES - DSP_PEQ_BANDS;
        // Move from end to avoid overlap
        for (int i = chainCount - 1; i >= 0; i--) {
            ch.stages[DSP_PEQ_BANDS + i] = ch.stages[i];
        }
        ch.stageCount = DSP_PEQ_BANDS + chainCount;
    } else {
        ch.stageCount = DSP_PEQ_BANDS;
    }
    // Initialize PEQ bands as disabled, spread logarithmically across spectrum
    static const float peqDefaultFreqs[10] = {
        31.0f, 63.0f, 125.0f, 250.0f, 500.0f,
        1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
    };
    for (int i = 0; i < DSP_PEQ_BANDS; i++) {
        dsp_init_stage(ch.stages[i], DSP_BIQUAD_PEQ);
        ch.stages[i].enabled = false;
        ch.stages[i].biquad.frequency = (i < 10) ? peqDefaultFreqs[i] : 1000.0f;
        ch.stages[i].biquad.gain = 0.0f;
        ch.stages[i].biquad.Q = 1.0f;
        // Simple label construction (snprintf may not be available in all inline contexts)
        ch.stages[i].label[0] = 'P'; ch.stages[i].label[1] = 'E';
        ch.stages[i].label[2] = 'Q'; ch.stages[i].label[3] = ' ';
        if (i < 9) {
            ch.stages[i].label[4] = '1' + i;
            ch.stages[i].label[5] = '\0';
        } else {
            ch.stages[i].label[4] = '1';
            ch.stages[i].label[5] = '0';
            ch.stages[i].label[6] = '\0';
        }
    }
}

// Check if a stage index is in the PEQ region (0 through DSP_PEQ_BANDS-1).
inline bool dsp_is_peq_index(int stageIndex) {
    return stageIndex >= 0 && stageIndex < DSP_PEQ_BANDS;
}

// Count chain stages (index >= DSP_PEQ_BANDS).
inline int dsp_chain_stage_count(const DspChannelConfig &ch) {
    return ch.stageCount > DSP_PEQ_BANDS ? ch.stageCount - DSP_PEQ_BANDS : 0;
}

// Check if channel has PEQ bands initialized (stages 0-9 are biquad type with "PEQ" prefix labels).
inline bool dsp_has_peq_bands(const DspChannelConfig &ch) {
    if (ch.stageCount < DSP_PEQ_BANDS) return false;
    return ch.stages[0].label[0] == 'P' && ch.stages[0].label[1] == 'E' && ch.stages[0].label[2] == 'Q';
}

inline void dsp_init_state(DspState &st) {
    st.globalBypass = false;
    st.sampleRate = 48000;
    for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
        dsp_init_channel(st.channels[i]);
        dsp_init_peq_bands(st.channels[i]);
    }
}

inline void dsp_init_metrics(DspMetrics &m) {
    m.processTimeUs = 0;
    m.maxProcessTimeUs = 0;
    m.cpuLoadPercent = 0.0f;
    for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
        m.limiterGrDb[i] = 0.0f;
    }
    m.emergencyLimiterGrDb = 0.0f;
    m.emergencyLimiterTriggers = 0;
    m.emergencyLimiterActive = false;
}

// ===== Conversion Helpers =====

// Convert dB to linear gain: 10^(dB/20)
inline float dsp_db_to_linear(float dB) {
    return powf(10.0f, dB / 20.0f);
}

// Compute single-pole envelope time constant from milliseconds
inline float dsp_time_coeff(float ms, float sampleRate) {
    return expf(-1.0f / (ms * 0.001f * sampleRate));
}

// ===== Public API =====
void dsp_init();
void dsp_process_buffer(int32_t *buffer, int stereoFrames, int adcIndex);

// Config access (double-buffered: active = read by audio task, inactive = modified by API)
DspState *dsp_get_active_config();
DspState *dsp_get_inactive_config();
bool dsp_swap_config();  // Returns: true if swap successful, false if busy/timeout

// Pure testable swap decision function — no FreeRTOS dependencies.
// Returns: 0=success (safe to swap), 1=mutex busy, 2=processing timeout, -1=still waiting
// Parameters mirror the swap loop state so this function can be tested natively.
int dsp_swap_check_state(bool mutex_acquired, bool processing_active, int wait_iterations_remaining);

// Deep copy active config to inactive (includes FIR pool data)
void dsp_copy_active_to_inactive();

// Metrics
DspMetrics dsp_get_metrics();
void dsp_reset_max_metrics();
void dsp_clear_cpu_load();

// Stage CRUD (operates on inactive config)
int dsp_add_stage(int channel, DspStageType type, int position = -1);
bool dsp_remove_stage(int channel, int stageIndex);
bool dsp_reorder_stages(int channel, const int *newOrder, int count);
bool dsp_set_stage_enabled(int channel, int stageIndex, bool enabled);

// Chain stage wrappers (operate only on indices >= DSP_PEQ_BANDS)
int dsp_add_chain_stage(int channel, DspStageType type, int chainPosition = -1);
bool dsp_remove_chain_stage(int channel, int chainIndex);

// Stereo link helpers
void dsp_mirror_channel_config(int srcCh, int dstCh);  // Copy stage config (preserving runtime state)
int  dsp_get_linked_partner(int channel);               // Returns partner channel, or -1 if unlinked

// PEQ band helpers
void dsp_ensure_peq_bands(DspState *cfg);  // Ensure all channels have PEQ bands
void dsp_copy_peq_bands(int srcChannel, int dstChannel);  // Copy PEQ bands between channels
void dsp_copy_chain_stages(int srcChannel, int dstChannel);  // Copy chain stages (Additional Processing) between channels
const char *stage_type_name(DspStageType t);               // Stage type to string name

// DC block removed in v1.8.3 - use highpass filter stage instead (dsp_add_stage with DSP_HIGHPASS)

// Multi-band compressor pool
int dsp_mb_alloc_slot();
void dsp_mb_free_slot(int slot);

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

// Zero post-DSP channels for an inactive input (prevents stale data in routing matrix)
void dsp_zero_channels(int adcIndex);

// Routing matrix execution: apply 6x6 routing to post-DSP channels, output to DAC buffer
void dsp_routing_execute(int32_t *dacBuf, int frames);

#endif // DSP_ENABLED
#endif // DSP_PIPELINE_H
