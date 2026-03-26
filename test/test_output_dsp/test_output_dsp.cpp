#define UNIT_TEST
#define NATIVE_TEST
#define DSP_ENABLED
#define DSP_MAX_STAGES 24
#define DSP_PEQ_BANDS 10
#define DSP_MAX_FIR_TAPS 256
#define DSP_MAX_FIR_SLOTS 2
#define DSP_MAX_CHANNELS 4
#define DSP_MAX_DELAY_SLOTS 2
#define DSP_MAX_DELAY_SAMPLES 4800
#define DSP_DEFAULT_Q 0.707f
#define DSP_CPU_WARN_PERCENT 80.0f
#define DSP_PRESET_MAX_SLOTS 32
#define OUTPUT_DSP_MAX_CHANNELS 8
#define OUTPUT_DSP_MAX_STAGES 12
#define OUTPUT_DSP_MAX_DELAY_SAMPLES 4800

#include <unity.h>
#include <math.h>
#include <string.h>

// ESP-DSP lite fallbacks (ANSI C implementations for native tests)
#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_math_f32.c"

// Biquad coefficient generators
#include "../../src/dsp_biquad_gen.c"

// Include output_dsp.h first to get DspStageType and all type declarations.
// The include guard will prevent double-inclusion when output_dsp.cpp is included below.
#include "../../src/output_dsp.h"

// Stub: stage_type_name is declared in dsp_pipeline.h but defined in dsp_pipeline.cpp.
// We provide a minimal definition here since we cannot include the full dsp_pipeline.cpp.
const char* stage_type_name(DspStageType t) {
    switch (t) {
        case DSP_BIQUAD_LPF: return "LPF";
        case DSP_BIQUAD_HPF: return "HPF";
        case DSP_BIQUAD_BPF: return "BPF";
        case DSP_BIQUAD_PEQ: return "PEQ";
        case DSP_BIQUAD_NOTCH: return "NOTCH";
        case DSP_BIQUAD_LOW_SHELF: return "LOW_SHELF";
        case DSP_BIQUAD_HIGH_SHELF: return "HIGH_SHELF";
        case DSP_BIQUAD_ALLPASS: return "ALLPASS";
        case DSP_BIQUAD_ALLPASS_360: return "ALLPASS_360";
        case DSP_BIQUAD_ALLPASS_180: return "ALLPASS_180";
        case DSP_BIQUAD_BPF_0DB: return "BPF_0DB";
        case DSP_BIQUAD_CUSTOM: return "CUSTOM";
        case DSP_LIMITER: return "LIMITER";
        case DSP_GAIN: return "GAIN";
        case DSP_POLARITY: return "POLARITY";
        case DSP_MUTE: return "MUTE";
        case DSP_COMPRESSOR: return "COMPRESSOR";
        case DSP_BIQUAD_LPF_1ST: return "LPF_1ST";
        case DSP_BIQUAD_HPF_1ST: return "HPF_1ST";
        case DSP_BIQUAD_LINKWITZ: return "LINKWITZ";
        case DSP_DELAY: return "DELAY";
        default: return "UNKNOWN";
    }
}

// Module under test — psram_alloc/heap_budget needed since output_dsp uses psram_alloc()
#include "../../src/heap_budget.cpp"
#include "../../src/psram_alloc.cpp"
#include "../../src/output_dsp.cpp"

// ===== Helpers =====

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void generate_sine(float *buf, int frames, float freq, float sampleRate) {
    for (int i = 0; i < frames; i++) {
        buf[i] = sinf(2.0f * (float)M_PI * freq * (float)i / sampleRate);
    }
}

static float compute_rms(const float *buf, int frames) {
    float sum = 0;
    for (int i = 0; i < frames; i++) sum += buf[i] * buf[i];
    return sqrtf(sum / (float)frames);
}

// ===== setUp / tearDown =====

void setUp() {
    output_dsp_init();
}

void tearDown() {}

// ===== Test 1: Init creates default state =====

void test_init_creates_default_state() {
    OutputDspState *cfg = output_dsp_get_active_config();
    TEST_ASSERT_FALSE(cfg->globalBypass);
    TEST_ASSERT_EQUAL_UINT32(48000, cfg->sampleRate);
    for (int ch = 0; ch < OUTPUT_DSP_MAX_CHANNELS; ch++) {
        TEST_ASSERT_TRUE(cfg->channels[ch].bypass);
        TEST_ASSERT_EQUAL_UINT8(0, cfg->channels[ch].stageCount);
    }
}

// ===== Test 2: Bypass leaves buffer unchanged =====

void test_bypass_leaves_buffer_unchanged() {
    float buf[256];
    float ref[256];
    generate_sine(buf, 256, 1000.0f, 48000.0f);
    memcpy(ref, buf, sizeof(buf));

    // Channel 0 is bypass=true by default after init
    output_dsp_process(0, buf, 256);

    TEST_ASSERT_EQUAL_FLOAT_ARRAY(ref, buf, 256);
}

// ===== Test 3: Gain stage doubles amplitude =====

void test_gain_stage_doubles_amplitude() {
    // Set channel 0 bypass=false on inactive config, add gain stage
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    int pos = output_dsp_add_stage(0, DSP_GAIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pos);

    // Set gain to +6.02 dB (~2x linear)
    inactive->channels[0].stages[pos].gain.gainDb = 6.02f;
    inactive->channels[0].stages[pos].gain.gainLinear = powf(10.0f, 6.02f / 20.0f);
    inactive->channels[0].stages[pos].gain.currentLinear = inactive->channels[0].stages[pos].gain.gainLinear;

    output_dsp_swap_config();

    float buf[256];
    generate_sine(buf, 256, 1000.0f, 48000.0f);

    // Scale input to 0.25 amplitude
    for (int i = 0; i < 256; i++) buf[i] *= 0.25f;

    float inputRms = compute_rms(buf, 256);
    output_dsp_process(0, buf, 256);
    float outputRms = compute_rms(buf, 256);

    float ratio = outputRms / inputRms;
    // Should be approximately 2.0 (within 5%)
    TEST_ASSERT_FLOAT_WITHIN(0.10f, 2.0f, ratio);
}

// ===== Test 4: Mute stage zeros buffer =====

void test_mute_stage_zeros_buffer() {
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    int pos = output_dsp_add_stage(0, DSP_MUTE);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pos);

    output_dsp_swap_config();

    float buf[256];
    generate_sine(buf, 256, 1000.0f, 48000.0f);

    output_dsp_process(0, buf, 256);

    for (int i = 0; i < 256; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.0f, buf[i]);
    }
}

// ===== Test 5: Polarity negates samples =====

void test_polarity_negates_samples() {
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    int pos = output_dsp_add_stage(0, DSP_POLARITY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pos);

    output_dsp_swap_config();

    float buf[256];
    float ref[256];
    generate_sine(buf, 256, 1000.0f, 48000.0f);
    memcpy(ref, buf, sizeof(buf));

    output_dsp_process(0, buf, 256);

    for (int i = 0; i < 256; i++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, -ref[i], buf[i]);
    }
}

// ===== Test 6: LPF attenuates high frequencies =====

void test_biquad_lpf_attenuates_high() {
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    int pos = output_dsp_add_stage(0, DSP_BIQUAD_LPF);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pos);

    // Set LPF at 500Hz, Q=0.707
    inactive->channels[0].stages[pos].biquad.frequency = 500.0f;
    inactive->channels[0].stages[pos].biquad.Q = 0.707f;

    // Recompute coefficients
    float normFreq = 500.0f / 48000.0f;
    dsp_gen_lpf_f32(inactive->channels[0].stages[pos].biquad.coeffs, normFreq, 0.707f);
    inactive->channels[0].stages[pos].biquad.morphRemaining = 0;

    output_dsp_swap_config();

    // Measure 100Hz (should pass through)
    float buf[256];
    float rms100 = 0.0f;
    for (int iter = 0; iter < 20; iter++) {
        generate_sine(buf, 256, 100.0f, 48000.0f);
        output_dsp_process(0, buf, 256);
        rms100 = compute_rms(buf, 256);
    }

    // Re-init to clear filter state, re-add filter
    output_dsp_init();
    inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    pos = output_dsp_add_stage(0, DSP_BIQUAD_LPF);
    inactive->channels[0].stages[pos].biquad.frequency = 500.0f;
    inactive->channels[0].stages[pos].biquad.Q = 0.707f;
    dsp_gen_lpf_f32(inactive->channels[0].stages[pos].biquad.coeffs, normFreq, 0.707f);
    inactive->channels[0].stages[pos].biquad.morphRemaining = 0;

    output_dsp_swap_config();

    // Measure 5000Hz (should be attenuated)
    float rms5000 = 0.0f;
    for (int iter = 0; iter < 20; iter++) {
        generate_sine(buf, 256, 5000.0f, 48000.0f);
        output_dsp_process(0, buf, 256);
        rms5000 = compute_rms(buf, 256);
    }

    // 5000Hz should be >20dB below 100Hz
    float attenuation_dB = 20.0f * log10f(rms5000 / rms100);
    TEST_ASSERT_LESS_THAN(-20.0f, attenuation_dB);
}

// ===== Test 7: HPF attenuates low frequencies =====

void test_biquad_hpf_attenuates_low() {
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    int pos = output_dsp_add_stage(0, DSP_BIQUAD_HPF);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pos);

    inactive->channels[0].stages[pos].biquad.frequency = 500.0f;
    inactive->channels[0].stages[pos].biquad.Q = 0.707f;

    float normFreq = 500.0f / 48000.0f;
    dsp_gen_hpf_f32(inactive->channels[0].stages[pos].biquad.coeffs, normFreq, 0.707f);
    inactive->channels[0].stages[pos].biquad.morphRemaining = 0;

    output_dsp_swap_config();

    // Measure 5000Hz (should pass through)
    float buf[256];
    float rms5000 = 0.0f;
    for (int iter = 0; iter < 20; iter++) {
        generate_sine(buf, 256, 5000.0f, 48000.0f);
        output_dsp_process(0, buf, 256);
        rms5000 = compute_rms(buf, 256);
    }

    // Re-init to clear filter state, re-add filter
    output_dsp_init();
    inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    pos = output_dsp_add_stage(0, DSP_BIQUAD_HPF);
    inactive->channels[0].stages[pos].biquad.frequency = 500.0f;
    inactive->channels[0].stages[pos].biquad.Q = 0.707f;
    dsp_gen_hpf_f32(inactive->channels[0].stages[pos].biquad.coeffs, normFreq, 0.707f);
    inactive->channels[0].stages[pos].biquad.morphRemaining = 0;

    output_dsp_swap_config();

    // Measure 100Hz (should be attenuated)
    float rms100 = 0.0f;
    for (int iter = 0; iter < 20; iter++) {
        generate_sine(buf, 256, 100.0f, 48000.0f);
        output_dsp_process(0, buf, 256);
        rms100 = compute_rms(buf, 256);
    }

    // 100Hz should be >18dB below 5000Hz (2nd-order HPF gives ~20dB/decade)
    float attenuation_dB = 20.0f * log10f(rms100 / rms5000);
    TEST_ASSERT_LESS_THAN(-18.0f, attenuation_dB);
}

// ===== Test 8: Crossover setup =====

void test_crossover_setup() {
    int stagesAdded = output_dsp_setup_crossover(2, 0, 80.0f, 4);
    // LR4 = 2 LPF + 2 HPF = 4 stages total
    TEST_ASSERT_EQUAL(4, stagesAdded);

    OutputDspState *inactive = output_dsp_get_inactive_config();

    // Sub channel 2: should have 2 LPF stages with "XO" labels
    OutputDspChannelConfig &subCh = inactive->channels[2];
    TEST_ASSERT_EQUAL_UINT8(2, subCh.stageCount);
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, subCh.stages[i].type);
        TEST_ASSERT_EQUAL('X', subCh.stages[i].label[0]);
        TEST_ASSERT_EQUAL('O', subCh.stages[i].label[1]);
    }

    // Main channel 0: should have 2 HPF stages with "XO" labels
    OutputDspChannelConfig &mainCh = inactive->channels[0];
    TEST_ASSERT_EQUAL_UINT8(2, mainCh.stageCount);
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, mainCh.stages[i].type);
        TEST_ASSERT_EQUAL('X', mainCh.stages[i].label[0]);
        TEST_ASSERT_EQUAL('O', mainCh.stages[i].label[1]);
    }
}

// ===== Test 9: Add and remove stages =====

void test_add_remove_stages() {
    // Add 3 gain stages to channel 0 on inactive config
    int pos0 = output_dsp_add_stage(0, DSP_GAIN);
    int pos1 = output_dsp_add_stage(0, DSP_GAIN);
    int pos2 = output_dsp_add_stage(0, DSP_GAIN);

    TEST_ASSERT_EQUAL(0, pos0);
    TEST_ASSERT_EQUAL(1, pos1);
    TEST_ASSERT_EQUAL(2, pos2);

    OutputDspState *inactive = output_dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_UINT8(3, inactive->channels[0].stageCount);

    // Set distinguishing gain values
    inactive->channels[0].stages[0].gain.gainDb = 1.0f;
    inactive->channels[0].stages[1].gain.gainDb = 2.0f;
    inactive->channels[0].stages[2].gain.gainDb = 3.0f;

    // Remove stage at index 1
    bool removed = output_dsp_remove_stage(0, 1);
    TEST_ASSERT_TRUE(removed);
    TEST_ASSERT_EQUAL_UINT8(2, inactive->channels[0].stageCount);

    // Remaining stages should be the first and third (gainDb 1.0 and 3.0)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, inactive->channels[0].stages[0].gain.gainDb);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, inactive->channels[0].stages[1].gain.gainDb);
}

// ===== Test 10: Max stages limit =====

void test_max_stages_limit() {
    // Add OUTPUT_DSP_MAX_STAGES (12) gain stages
    for (int i = 0; i < OUTPUT_DSP_MAX_STAGES; i++) {
        int pos = output_dsp_add_stage(0, DSP_GAIN);
        TEST_ASSERT_GREATER_OR_EQUAL(0, pos);
    }

    OutputDspState *inactive = output_dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_UINT8(OUTPUT_DSP_MAX_STAGES, inactive->channels[0].stageCount);

    // Adding one more should fail
    int pos = output_dsp_add_stage(0, DSP_GAIN);
    TEST_ASSERT_EQUAL(-1, pos);
    TEST_ASSERT_EQUAL_UINT8(OUTPUT_DSP_MAX_STAGES, inactive->channels[0].stageCount);
}

// ===== Test 11: Config swap =====

void test_config_swap() {
    // Modify inactive config: set ch0 bypass=false
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    // Active config should still have ch0 bypass=true
    OutputDspState *active = output_dsp_get_active_config();
    TEST_ASSERT_TRUE(active->channels[0].bypass);

    // Swap
    output_dsp_swap_config();

    // Now active should have ch0 bypass=false
    active = output_dsp_get_active_config();
    TEST_ASSERT_FALSE(active->channels[0].bypass);
}

// ===== Test 12: Stage enable/disable =====

void test_stage_enable_disable() {
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    // Add a gain stage at +6dB
    int pos = output_dsp_add_stage(0, DSP_GAIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pos);
    inactive->channels[0].stages[pos].gain.gainDb = 6.0f;
    inactive->channels[0].stages[pos].gain.gainLinear = powf(10.0f, 6.0f / 20.0f);
    inactive->channels[0].stages[pos].gain.currentLinear = inactive->channels[0].stages[pos].gain.gainLinear;

    // Disable the stage
    bool ok = output_dsp_set_stage_enabled(0, pos, false);
    TEST_ASSERT_TRUE(ok);

    output_dsp_swap_config();

    // Process a sine -- RMS should be unchanged since stage is disabled
    float buf[256];
    generate_sine(buf, 256, 1000.0f, 48000.0f);
    float inputRms = compute_rms(buf, 256);

    output_dsp_process(0, buf, 256);
    float outputRms = compute_rms(buf, 256);

    float ratio = outputRms / inputRms;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ratio);
}

// ===== Test 13: Multi-stage chain (LPF + Gain) =====

void test_multi_stage_chain() {
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    // Add LPF at 500Hz
    int lpfPos = output_dsp_add_stage(0, DSP_BIQUAD_LPF);
    TEST_ASSERT_GREATER_OR_EQUAL(0, lpfPos);
    inactive->channels[0].stages[lpfPos].biquad.frequency = 500.0f;
    inactive->channels[0].stages[lpfPos].biquad.Q = 0.707f;
    float normFreq = 500.0f / 48000.0f;
    dsp_gen_lpf_f32(inactive->channels[0].stages[lpfPos].biquad.coeffs, normFreq, 0.707f);
    inactive->channels[0].stages[lpfPos].biquad.morphRemaining = 0;

    // Add Gain at +6dB
    int gainPos = output_dsp_add_stage(0, DSP_GAIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, gainPos);
    inactive->channels[0].stages[gainPos].gain.gainDb = 6.02f;
    inactive->channels[0].stages[gainPos].gain.gainLinear = powf(10.0f, 6.02f / 20.0f);
    inactive->channels[0].stages[gainPos].gain.currentLinear = inactive->channels[0].stages[gainPos].gain.gainLinear;

    output_dsp_swap_config();

    // Generate 100Hz sine (well below LPF cutoff, should pass through)
    float buf[256];
    float inputRms = 0.0f;

    // Let filter settle
    for (int iter = 0; iter < 20; iter++) {
        generate_sine(buf, 256, 100.0f, 48000.0f);
        inputRms = compute_rms(buf, 256);
        output_dsp_process(0, buf, 256);
    }

    float outputRms = compute_rms(buf, 256);
    float ratio = outputRms / inputRms;

    // LPF passes 100Hz, gain doubles it. Expect ratio ~2.0 (within 15% to account for filter)
    TEST_ASSERT_FLOAT_WITHIN(0.30f, 2.0f, ratio);
}

// ===== Test 14: Mono processing no crash =====

void test_mono_processing_no_crash() {
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;
    output_dsp_swap_config();

    float buf[256];
    memset(buf, 0, sizeof(buf));

    output_dsp_process(0, buf, 256);

    // All samples should be 0 or very close
    for (int i = 0; i < 256; i++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-10f, 0.0f, buf[i]);
    }
}

// ===== Test 15: DSP_DELAY stage can be added =====

void test_delay_stage_can_be_added() {
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    int pos = output_dsp_add_stage(0, DSP_DELAY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pos);

    OutputDspState *cfg = output_dsp_get_inactive_config();
    TEST_ASSERT_EQUAL(DSP_DELAY, cfg->channels[0].stages[pos].type);
    TEST_ASSERT_EQUAL_UINT8(1, cfg->channels[0].stageCount);
}

// ===== Test 16: DSP_DELAY with 0 samples is a pass-through =====

void test_delay_zero_samples_passthrough() {
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    int pos = output_dsp_add_stage(0, DSP_DELAY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pos);
    inactive->channels[0].stages[pos].delay.delaySamples = 0;
    inactive->channels[0].stages[pos].delay.writePos = 0;

    output_dsp_swap_config();

    float buf[64];
    float ref[64];
    generate_sine(buf, 64, 1000.0f, 48000.0f);
    memcpy(ref, buf, sizeof(buf));

    output_dsp_process(0, buf, 64);

    TEST_ASSERT_EQUAL_FLOAT_ARRAY(ref, buf, 64);
}

// ===== Test 17: DSP_DELAY with N samples delays signal by N samples =====

void test_delay_n_samples_delays_signal() {
    const int DELAY_SAMPLES = 10;
    const int BUF_SIZE = 128;

    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    int pos = output_dsp_add_stage(0, DSP_DELAY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pos);
    inactive->channels[0].stages[pos].delay.delaySamples = DELAY_SAMPLES;
    inactive->channels[0].stages[pos].delay.writePos = 0;

    output_dsp_swap_config();

    // Input: impulse at position 0 (1.0 followed by zeros)
    float buf[BUF_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = 1.0f;

    output_dsp_process(0, buf, BUF_SIZE);

    // First DELAY_SAMPLES samples should be silence (reading from zeroed delay buffer)
    for (int i = 0; i < DELAY_SAMPLES; i++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, buf[i]);
    }
    // The impulse should appear at position DELAY_SAMPLES
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, buf[DELAY_SAMPLES]);
}

// ===== Test 18: DSP_DELAY ms-to-samples conversion (handler logic) =====

void test_delay_ms_to_samples_conversion() {
    // At 48kHz: 10ms = 480 samples
    float ms = 10.0f;
    uint32_t sampleRate = 48000;
    uint16_t delaySamples = (uint16_t)((ms * sampleRate) / 1000.0f);
    TEST_ASSERT_EQUAL_UINT16(480, delaySamples);

    // At 48kHz: 100ms = 4800 samples (== OUTPUT_DSP_MAX_DELAY_SAMPLES)
    ms = 100.0f;
    delaySamples = (uint16_t)((ms * sampleRate) / 1000.0f);
    TEST_ASSERT_EQUAL_UINT16(4800, delaySamples);

    // Clamping: 101ms at 48kHz = 4848 → clamps to 4800
    ms = 101.0f;
    delaySamples = (uint16_t)((ms * sampleRate) / 1000.0f);
    if (delaySamples > OUTPUT_DSP_MAX_DELAY_SAMPLES) delaySamples = OUTPUT_DSP_MAX_DELAY_SAMPLES;
    TEST_ASSERT_EQUAL_UINT16(OUTPUT_DSP_MAX_DELAY_SAMPLES, delaySamples);
}

// ===== Test 19: DSP_DELAY find-or-create: second add updates existing stage =====

void test_delay_find_or_create_updates_existing() {
    OutputDspState *inactive = output_dsp_get_inactive_config();
    inactive->channels[0].bypass = false;

    // Add delay stage with 100 samples
    int pos = output_dsp_add_stage(0, DSP_DELAY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pos);
    inactive->channels[0].stages[pos].delay.delaySamples = 100;
    output_dsp_swap_config();

    int countAfterFirst = 0;
    OutputDspState *active = output_dsp_get_active_config();
    for (int s = 0; s < active->channels[0].stageCount; s++) {
        if (active->channels[0].stages[s].type == DSP_DELAY) countAfterFirst++;
    }
    TEST_ASSERT_EQUAL(1, countAfterFirst);

    // Simulate find-or-create: update existing stage (don't add another)
    output_dsp_copy_active_to_inactive();
    inactive = output_dsp_get_inactive_config();
    bool found = false;
    for (int s = 0; s < inactive->channels[0].stageCount; s++) {
        if (inactive->channels[0].stages[s].type == DSP_DELAY) {
            inactive->channels[0].stages[s].delay.delaySamples = 200;
            found = true;
            break;
        }
    }
    if (!found) {
        int idx = output_dsp_add_stage(0, DSP_DELAY);
        if (idx >= 0) inactive->channels[0].stages[idx].delay.delaySamples = 200;
    }
    output_dsp_swap_config();

    active = output_dsp_get_active_config();
    int countAfterSecond = 0;
    uint16_t finalSamples = 0;
    for (int s = 0; s < active->channels[0].stageCount; s++) {
        if (active->channels[0].stages[s].type == DSP_DELAY) {
            countAfterSecond++;
            finalSamples = active->channels[0].stages[s].delay.delaySamples;
        }
    }
    TEST_ASSERT_EQUAL(1, countAfterSecond);
    TEST_ASSERT_EQUAL_UINT16(200, finalSamples);
}

// ===== Test: Crossover swap-to-active (setOutputCrossover WS path) =====

void test_crossover_swap_to_active() {
    // Simulate the setOutputCrossover WS handler pattern:
    // copy → setup → swap → confirm stages visible in active config
    output_dsp_copy_active_to_inactive();
    int stagesAdded = output_dsp_setup_crossover(1, 0, 120.0f, 4);
    TEST_ASSERT_EQUAL(4, stagesAdded);

    output_dsp_swap_config();

    // Verify the crossover stages are now in the active config
    OutputDspState *active = output_dsp_get_active_config();

    // Sub ch 1: 2 LPF with "XO" label
    OutputDspChannelConfig &subCh = active->channels[1];
    TEST_ASSERT_EQUAL_UINT8(2, subCh.stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, subCh.stages[0].type);
    TEST_ASSERT_EQUAL('X', subCh.stages[0].label[0]);

    // Main ch 0: 2 HPF with "XO" label
    OutputDspChannelConfig &mainCh = active->channels[0];
    TEST_ASSERT_EQUAL_UINT8(2, mainCh.stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, mainCh.stages[0].type);
}

// ===== Main =====

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_init_creates_default_state);
    RUN_TEST(test_bypass_leaves_buffer_unchanged);
    RUN_TEST(test_gain_stage_doubles_amplitude);
    RUN_TEST(test_mute_stage_zeros_buffer);
    RUN_TEST(test_polarity_negates_samples);
    RUN_TEST(test_biquad_lpf_attenuates_high);
    RUN_TEST(test_biquad_hpf_attenuates_low);
    RUN_TEST(test_crossover_setup);
    RUN_TEST(test_add_remove_stages);
    RUN_TEST(test_max_stages_limit);
    RUN_TEST(test_config_swap);
    RUN_TEST(test_stage_enable_disable);
    RUN_TEST(test_multi_stage_chain);
    RUN_TEST(test_mono_processing_no_crash);
    RUN_TEST(test_delay_stage_can_be_added);
    RUN_TEST(test_delay_zero_samples_passthrough);
    RUN_TEST(test_delay_n_samples_delays_signal);
    RUN_TEST(test_delay_ms_to_samples_conversion);
    RUN_TEST(test_delay_find_or_create_updates_existing);
    RUN_TEST(test_crossover_swap_to_active);
    return UNITY_END();
}
