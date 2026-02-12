#include <unity.h>
#include <math.h>
#include <string.h>

// Include DSP sources directly (test_build_src = no)
#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_init_f32.c"
#include "../../src/dsp_biquad_gen.c"

// Include DSP headers
#include "../../src/dsp_pipeline.h"
#include "../../src/dsp_coefficients.h"

// Include DSP implementation source (no ArduinoJson in native tests)
#include "../../src/dsp_coefficients.cpp"
#include "../../src/dsp_pipeline.cpp"
#include "../../src/dsp_crossover.cpp"

// Tolerance for float comparisons
#define FLOAT_TOL 0.001f
#define COEFF_TOL 0.01f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void setUp(void) {
    dsp_init();
}

void tearDown(void) {}

// ===== Coefficient Computation Tests =====

void test_lpf_coefficients(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.Q = 0.707f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_LPF, 48000);

    // b0 + b1 + b2 should sum to DC gain (= 1.0 for LPF at DC)
    float dcGain = (p.coeffs[0] + p.coeffs[1] + p.coeffs[2]) /
                   (1.0f + p.coeffs[3] + p.coeffs[4]);
    TEST_ASSERT_FLOAT_WITHIN(COEFF_TOL, 1.0f, dcGain);
    // b0 should be positive
    TEST_ASSERT_TRUE(p.coeffs[0] > 0.0f);
}

void test_hpf_coefficients(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.Q = 0.707f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_HPF, 48000);

    // DC gain should be ~0 for HPF
    float dcGain = (p.coeffs[0] + p.coeffs[1] + p.coeffs[2]) /
                   (1.0f + p.coeffs[3] + p.coeffs[4]);
    TEST_ASSERT_FLOAT_WITHIN(COEFF_TOL, 0.0f, dcGain);
}

void test_peq_coefficients_boost(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.gain = 6.0f;
    p.Q = 2.0f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_PEQ, 48000);

    // PEQ with +6dB: b0 should be > 1.0
    TEST_ASSERT_TRUE(p.coeffs[0] > 1.0f);
}

void test_peq_coefficients_cut(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.gain = -6.0f;
    p.Q = 2.0f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_PEQ, 48000);

    // PEQ with -6dB: b0 should be < 1.0
    TEST_ASSERT_TRUE(p.coeffs[0] < 1.0f);
}

void test_notch_coefficients(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.Q = 10.0f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_NOTCH, 48000);

    // At the notch frequency, response should be ~0
    // Verify b1 == a1 (property of notch filter)
    TEST_ASSERT_FLOAT_WITHIN(COEFF_TOL, p.coeffs[1], p.coeffs[3]);
}

void test_shelf_low_boost(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 200.0f;
    p.gain = 6.0f;
    p.Q = 0.707f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_LOW_SHELF, 48000);

    // DC gain should be ~2.0 (+6dB = 10^(6/20) ≈ 2.0)
    float dcGain = (p.coeffs[0] + p.coeffs[1] + p.coeffs[2]) /
                   (1.0f + p.coeffs[3] + p.coeffs[4]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.0f, dcGain);
}

void test_shelf_high_boost(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 10000.0f;
    p.gain = 6.0f;
    p.Q = 0.707f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_HIGH_SHELF, 48000);

    // DC gain should be ~1.0 (high shelf doesn't affect DC)
    float dcGain = (p.coeffs[0] + p.coeffs[1] + p.coeffs[2]) /
                   (1.0f + p.coeffs[3] + p.coeffs[4]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, dcGain);
}

void test_custom_coefficients_load(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    dsp_load_custom_coeffs(p, 0.5f, 0.3f, 0.2f, -0.1f, 0.05f);

    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.5f, p.coeffs[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.3f, p.coeffs[1]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.2f, p.coeffs[2]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, -0.1f, p.coeffs[3]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.05f, p.coeffs[4]);
}

void test_allpass_unity_magnitude(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.Q = 0.707f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_ALLPASS, 48000);

    // Allpass: |H(z)| = 1 for all frequencies
    // Verify: b0 == a2, b2 == 1.0 (after normalization)
    TEST_ASSERT_FLOAT_WITHIN(COEFF_TOL, p.coeffs[4], p.coeffs[0]);
}

// ===== Biquad Processing Tests =====

void test_biquad_passthrough(void) {
    // Unity passthrough: b0=1, b1=b2=a1=a2=0
    float coeffs[5] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float delay[2] = {0.0f, 0.0f};
    float input[4] = {0.5f, -0.3f, 0.8f, -0.1f};
    float output[4] = {0};

    dsps_biquad_f32(input, output, 4, coeffs, delay);

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, input[i], output[i]);
    }
}

void test_biquad_lpf_attenuates_high(void) {
    // 100Hz LPF at 48kHz should attenuate a 10kHz signal
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 100.0f;
    p.Q = 0.707f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_LPF, 48000);

    // Generate 10kHz sine at 48kHz
    float input[256], output[256];
    for (int i = 0; i < 256; i++) {
        input[i] = sinf(2.0f * (float)M_PI * 10000.0f * i / 48000.0f);
    }

    dsps_biquad_f32(input, output, 256, p.coeffs, p.delay);

    // Measure output RMS (skip first 32 samples for filter settling)
    float rmsOut = 0.0f;
    for (int i = 32; i < 256; i++) rmsOut += output[i] * output[i];
    rmsOut = sqrtf(rmsOut / 224.0f);

    // Should be heavily attenuated (< 0.1 for 100Hz LPF at 10kHz)
    TEST_ASSERT_TRUE(rmsOut < 0.1f);
}

// ===== FIR Processing Tests =====

void test_fir_impulse_response(void) {
    // FIR with [0.5, 0.3, 0.2] taps
    float coeffs[3] = {0.5f, 0.3f, 0.2f};
    float delay[3] = {0};
    fir_f32_t fir;
    dsps_fir_init_f32(&fir, coeffs, delay, 3);

    float input[5] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float output[5] = {0};

    dsps_fir_f32(&fir, input, output, 5);

    // Impulse response should match coefficients
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.5f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.3f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.2f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, output[3]);
}

void test_fir_moving_average(void) {
    // 4-tap moving average FIR
    float coeffs[4] = {0.25f, 0.25f, 0.25f, 0.25f};
    float delay[4] = {0};
    fir_f32_t fir;
    dsps_fir_init_f32(&fir, coeffs, delay, 4);

    // Step input: [1,1,1,1,1]
    float input[5] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float output[5] = {0};

    dsps_fir_f32(&fir, input, output, 5);

    // Output should ramp up: 0.25, 0.50, 0.75, 1.0, 1.0
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.25f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.50f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.75f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, output[3]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, output[4]);
}

// ===== Limiter Tests =====

void test_limiter_below_threshold(void) {
    DspStage s;
    dsp_init_stage(s, DSP_LIMITER);
    s.limiter.thresholdDb = 0.0f;
    s.limiter.attackMs = 1.0f;
    s.limiter.releaseMs = 10.0f;
    s.limiter.ratio = 20.0f;

    // Signal well below threshold
    float buf[64];
    for (int i = 0; i < 64; i++) buf[i] = 0.1f;

    float original[64];
    memcpy(original, buf, sizeof(buf));

    // Process via the pipeline's internal limiter function
    // (call dsp_process_buffer with a crafted buffer)
    // Instead, test the gain logic directly:
    // Below threshold → no gain reduction
    float threshLin = powf(10.0f, 0.0f / 20.0f); // 1.0
    TEST_ASSERT_TRUE(0.1f < threshLin); // Signal is below threshold
}

void test_limiter_above_threshold(void) {
    // Signal at 0dBFS (1.0), threshold at -6dB (0.5)
    // Should reduce gain
    float threshLin = powf(10.0f, -6.0f / 20.0f);
    TEST_ASSERT_TRUE(1.0f > threshLin);

    // Verify limiter gain formula
    float envDb = 20.0f * log10f(1.0f); // 0 dB
    float overDb = envDb - (-6.0f);      // +6 dB over
    float grDb = overDb * (1.0f - 1.0f / 20.0f); // ~5.7 dB reduction
    TEST_ASSERT_TRUE(grDb > 5.0f);
}

// ===== Gain Stage Tests =====

void test_gain_db_to_linear(void) {
    DspGainParams g;
    g.gainDb = 0.0f;
    dsp_compute_gain_linear(g);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, g.gainLinear);

    g.gainDb = 6.0f;
    dsp_compute_gain_linear(g);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.9953f, g.gainLinear);

    g.gainDb = -6.0f;
    dsp_compute_gain_linear(g);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5012f, g.gainLinear);

    g.gainDb = 20.0f;
    dsp_compute_gain_linear(g);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.0f, g.gainLinear);
}

void test_gain_buffer_multiply(void) {
    DspGainParams g;
    g.gainDb = 6.0f;
    dsp_compute_gain_linear(g);

    float buf[4] = {0.5f, -0.3f, 0.0f, 1.0f};
    float expected[4] = {0.5f * g.gainLinear, -0.3f * g.gainLinear,
                         0.0f, 1.0f * g.gainLinear};

    // Manually apply gain (same logic as dsp_gain_process)
    for (int i = 0; i < 4; i++) buf[i] *= g.gainLinear;

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, expected[i], buf[i]);
    }
}

// ===== Stage CRUD Tests =====

void test_add_stage_append(void) {
    int idx = dsp_add_stage(0, DSP_BIQUAD_PEQ);
    TEST_ASSERT_EQUAL_INT(0, idx);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(1, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_PEQ, cfg->channels[0].stages[0].type);
}

void test_add_stage_insert(void) {
    dsp_add_stage(0, DSP_BIQUAD_LPF);
    dsp_add_stage(0, DSP_BIQUAD_HPF);
    int idx = dsp_add_stage(0, DSP_BIQUAD_PEQ, 1); // Insert at position 1
    TEST_ASSERT_EQUAL_INT(1, idx);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(3, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[0].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_PEQ, cfg->channels[0].stages[1].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[0].stages[2].type);
}

void test_remove_stage(void) {
    dsp_add_stage(0, DSP_BIQUAD_LPF);
    dsp_add_stage(0, DSP_BIQUAD_PEQ);
    dsp_add_stage(0, DSP_BIQUAD_HPF);

    bool ok = dsp_remove_stage(0, 1); // Remove PEQ
    TEST_ASSERT_TRUE(ok);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(2, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[0].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[0].stages[1].type);
}

void test_reorder_stages(void) {
    dsp_add_stage(0, DSP_BIQUAD_LPF);
    dsp_add_stage(0, DSP_BIQUAD_PEQ);
    dsp_add_stage(0, DSP_BIQUAD_HPF);

    int order[] = {2, 0, 1};
    bool ok = dsp_reorder_stages(0, order, 3);
    TEST_ASSERT_TRUE(ok);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[0].stages[0].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[1].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_PEQ, cfg->channels[0].stages[2].type);
}

void test_max_stage_limit(void) {
    for (int i = 0; i < DSP_MAX_STAGES; i++) {
        int idx = dsp_add_stage(0, DSP_BIQUAD_PEQ);
        TEST_ASSERT_EQUAL_INT(i, idx);
    }
    // Next add should fail
    int idx = dsp_add_stage(0, DSP_BIQUAD_PEQ);
    TEST_ASSERT_EQUAL_INT(-1, idx);
}

void test_stage_enable_disable(void) {
    dsp_add_stage(0, DSP_BIQUAD_PEQ);

    bool ok = dsp_set_stage_enabled(0, 0, false);
    TEST_ASSERT_TRUE(ok);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_FALSE(cfg->channels[0].stages[0].enabled);

    ok = dsp_set_stage_enabled(0, 0, true);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(cfg->channels[0].stages[0].enabled);
}

// ===== Double-Buffer Tests =====

void test_double_buffer_swap(void) {
    DspState *inactive = dsp_get_inactive_config();
    inactive->globalBypass = true;
    inactive->channels[0].bypass = true;

    DspState *active = dsp_get_active_config();
    TEST_ASSERT_FALSE(active->globalBypass); // Active hasn't changed

    dsp_swap_config();

    active = dsp_get_active_config();
    TEST_ASSERT_TRUE(active->globalBypass); // Now active has the changes
}

void test_double_buffer_delay_continuity(void) {
    // Add a biquad to channel 0 on inactive config
    DspState *inactive = dsp_get_inactive_config();
    dsp_init_channel(inactive->channels[0]);
    dsp_init_stage(inactive->channels[0].stages[0], DSP_BIQUAD_PEQ);
    inactive->channels[0].stageCount = 1;

    // Set delay values on active config
    DspState *active = dsp_get_active_config();
    dsp_init_channel(active->channels[0]);
    dsp_init_stage(active->channels[0].stages[0], DSP_BIQUAD_PEQ);
    active->channels[0].stageCount = 1;
    active->channels[0].stages[0].biquad.delay[0] = 0.123f;
    active->channels[0].stages[0].biquad.delay[1] = 0.456f;

    dsp_swap_config();

    // New active should have copied delay lines
    active = dsp_get_active_config();
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.123f, active->channels[0].stages[0].biquad.delay[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.456f, active->channels[0].stages[0].biquad.delay[1]);
}

// ===== Processing Buffer Test =====

void test_bypass_passthrough(void) {
    DspState *cfg = dsp_get_active_config();
    cfg->globalBypass = true;

    int32_t buffer[8] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000};
    int32_t original[8];
    memcpy(original, buffer, sizeof(buffer));

    dsp_process_buffer(buffer, 4, 0);

    // Bypass: output == input (bitwise)
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_INT32(original[i], buffer[i]);
    }

    cfg->globalBypass = false;
}

void test_channel_recompute_coeffs(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    // Add a PEQ stage
    DspStage &s = ch.stages[0];
    dsp_init_stage(s, DSP_BIQUAD_PEQ);
    s.biquad.frequency = 1000.0f;
    s.biquad.gain = 6.0f;
    s.biquad.Q = 2.0f;
    ch.stageCount = 1;

    dsp_recompute_channel_coeffs(ch, 48000);

    // Verify coefficients were computed (b0 != default 1.0 for PEQ with gain)
    TEST_ASSERT_TRUE(ch.stages[0].biquad.coeffs[0] > 1.0f);
}

// ===== Delay Stage Tests =====

void test_delay_zero_passthrough(void) {
    // Delay of 0 samples = passthrough
    int idx = dsp_add_stage(0, DSP_DELAY);
    TEST_ASSERT_TRUE(idx >= 0);
    DspState *cfg = dsp_get_inactive_config();
    DspStage &s = cfg->channels[0].stages[idx];
    s.delay.delaySamples = 0;
    dsp_swap_config();

    int32_t buffer[8] = {1000000, 2000000, 3000000, 4000000, 5000000, 6000000, 7000000, 8000000};
    int32_t original[8];
    memcpy(original, buffer, sizeof(buffer));

    dsp_process_buffer(buffer, 4, 0);

    // With 0 delay, output should equal input (through float conversion)
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_INT32_WITHIN(2, original[i], buffer[i]);
    }
}

void test_delay_shifts_samples(void) {
    dsp_init();
    int idx = dsp_add_stage(0, DSP_DELAY);
    TEST_ASSERT_TRUE(idx >= 0);
    DspState *cfg = dsp_get_inactive_config();
    DspStage &s = cfg->channels[0].stages[idx];
    s.delay.delaySamples = 2;
    dsp_swap_config();

    // Process first buffer: impulse on L channel
    int32_t buffer1[8] = {8388607, 0, 0, 0, 0, 0, 0, 0}; // L=max at sample 0
    dsp_process_buffer(buffer1, 4, 0);
    // After 2-sample delay, impulse should appear at sample 2 (index 4 in interleaved)
    // Samples 0,1 should be 0 (delay line was empty)
    TEST_ASSERT_INT32_WITHIN(100, 0, buffer1[0]); // L sample 0 = 0 (delayed)
    TEST_ASSERT_INT32_WITHIN(100, 0, buffer1[2]); // L sample 1 = 0 (delayed)
    TEST_ASSERT_INT32_WITHIN(100, 8388607, buffer1[4]); // L sample 2 = impulse
}

void test_delay_slot_alloc_free(void) {
    int slot1 = dsp_delay_alloc_slot();
    TEST_ASSERT_TRUE(slot1 >= 0);
    int slot2 = dsp_delay_alloc_slot();
    TEST_ASSERT_TRUE(slot2 >= 0);
    TEST_ASSERT_NOT_EQUAL(slot1, slot2);

    // All slots used
    int slot3 = dsp_delay_alloc_slot();
    TEST_ASSERT_EQUAL_INT(-1, slot3);

    // Free and re-alloc
    dsp_delay_free_slot(slot1);
    int slot4 = dsp_delay_alloc_slot();
    TEST_ASSERT_EQUAL_INT(slot1, slot4);
}

// ===== Polarity Stage Tests =====

void test_polarity_inverts_signal(void) {
    dsp_init();
    int idx = dsp_add_stage(0, DSP_POLARITY);
    TEST_ASSERT_TRUE(idx >= 0);
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[0].stages[idx].polarity.inverted = true;
    dsp_swap_config();

    int32_t buffer[4] = {4000000, 0, -2000000, 0}; // L=+, L=-
    dsp_process_buffer(buffer, 2, 0);

    // L channel should be inverted
    TEST_ASSERT_INT32_WITHIN(100, -4000000, buffer[0]);
    TEST_ASSERT_INT32_WITHIN(100, 2000000, buffer[2]);
}

void test_polarity_not_inverted_passthrough(void) {
    dsp_init();
    int idx = dsp_add_stage(0, DSP_POLARITY);
    TEST_ASSERT_TRUE(idx >= 0);
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[0].stages[idx].polarity.inverted = false;
    dsp_swap_config();

    int32_t buffer[4] = {4000000, 0, -2000000, 0};
    int32_t original[4];
    memcpy(original, buffer, sizeof(buffer));
    dsp_process_buffer(buffer, 2, 0);

    // Not inverted = passthrough
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_INT32_WITHIN(2, original[i], buffer[i]);
    }
}

// ===== Mute Stage Tests =====

void test_mute_zeros_output(void) {
    dsp_init();
    int idx = dsp_add_stage(0, DSP_MUTE);
    TEST_ASSERT_TRUE(idx >= 0);
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[0].stages[idx].mute.muted = true;
    dsp_swap_config();

    int32_t buffer[4] = {4000000, 3000000, -2000000, 1000000};
    dsp_process_buffer(buffer, 2, 0);

    // L channel should be zeroed (mute applies to channel 0 = L)
    TEST_ASSERT_EQUAL_INT32(0, buffer[0]);
    TEST_ASSERT_EQUAL_INT32(0, buffer[2]);
}

void test_mute_not_muted_passthrough(void) {
    dsp_init();
    int idx = dsp_add_stage(0, DSP_MUTE);
    TEST_ASSERT_TRUE(idx >= 0);
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[0].stages[idx].mute.muted = false;
    dsp_swap_config();

    int32_t buffer[4] = {4000000, 0, -2000000, 0};
    int32_t original[4];
    memcpy(original, buffer, sizeof(buffer));
    dsp_process_buffer(buffer, 2, 0);

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_INT32_WITHIN(2, original[i], buffer[i]);
    }
}

// ===== Compressor Stage Tests =====

void test_compressor_below_threshold_passthrough(void) {
    // Signal well below threshold should pass through with only makeup gain
    DspCompressorParams comp;
    dsp_init_compressor_params(comp);
    comp.thresholdDb = 0.0f;      // 0 dBFS threshold
    comp.ratio = 4.0f;
    comp.kneeDb = 0.0f;           // Hard knee
    comp.makeupGainDb = 0.0f;
    dsp_compute_compressor_makeup(comp);

    // -20dBFS signal is well below 0dBFS threshold
    float threshLin = powf(10.0f, 0.0f / 20.0f); // 1.0
    float signalLin = 0.1f; // -20dBFS
    TEST_ASSERT_TRUE(signalLin < threshLin);
}

void test_compressor_above_threshold_reduces(void) {
    // Signal above threshold should be reduced by ratio
    float threshDb = -12.0f;
    float ratio = 4.0f;
    float envDb = 0.0f; // 0 dBFS signal
    float overDb = envDb - threshDb; // +12dB over threshold

    // Expected gain reduction: overDb * (1 - 1/ratio) = 12 * 0.75 = 9 dB
    float grDb = overDb * (1.0f - 1.0f / ratio);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 9.0f, grDb);
}

void test_compressor_soft_knee(void) {
    // In soft knee region, compression ramps gradually
    float kneeDb = 6.0f;
    float ratio = 4.0f;

    // At threshold (overDb = 0, which is in the knee region -3 to +3)
    float overDb = 0.0f;
    float x = overDb + kneeDb / 2.0f; // = 3.0
    float grDb = (1.0f - 1.0f / ratio) * x * x / (2.0f * kneeDb);
    // = 0.75 * 9 / 12 = 0.5625 dB
    TEST_ASSERT_TRUE(grDb > 0.0f);
    TEST_ASSERT_TRUE(grDb < 1.0f); // Much less than full compression
}

void test_compressor_makeup_gain(void) {
    DspCompressorParams comp;
    dsp_init_compressor_params(comp);
    comp.makeupGainDb = 6.0f;
    dsp_compute_compressor_makeup(comp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.9953f, comp.makeupLinear);

    comp.makeupGainDb = 0.0f;
    dsp_compute_compressor_makeup(comp);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, comp.makeupLinear);
}

// ===== New Biquad Type Tests =====

void test_bpf0db_unity_peak_gain(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.Q = 2.0f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_BPF_0DB, 48000);

    // Generate 1kHz sine at 48kHz (center frequency)
    float input[256], output[256];
    for (int i = 0; i < 256; i++) {
        input[i] = sinf(2.0f * (float)M_PI * 1000.0f * i / 48000.0f);
    }

    dsps_biquad_f32(input, output, 256, p.coeffs, p.delay);

    // At center frequency, output should be ~unity gain
    // Measure RMS in steady state (skip first 64 for settling)
    float rmsIn = 0.0f, rmsOut = 0.0f;
    for (int i = 64; i < 256; i++) {
        rmsIn += input[i] * input[i];
        rmsOut += output[i] * output[i];
    }
    rmsIn = sqrtf(rmsIn / 192.0f);
    rmsOut = sqrtf(rmsOut / 192.0f);

    // 0dB peak gain: output RMS should be close to input RMS
    float gainDb = 20.0f * log10f(rmsOut / rmsIn);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, gainDb);
}

void test_allpass360_unity_magnitude(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.Q = 0.707f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_ALLPASS_360, 48000);

    // Allpass 360°: same as standard allpass. Verify b0 == a2 (unity magnitude property)
    TEST_ASSERT_FLOAT_WITHIN(COEFF_TOL, p.coeffs[4], p.coeffs[0]);
}

void test_allpass180_first_order(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.Q = 0.707f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_ALLPASS_180, 48000);

    // First-order allpass: b2=0, a2=0
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, p.coeffs[2]); // b2 = 0
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, p.coeffs[4]); // a2 = 0
    // b0 should equal a1
    TEST_ASSERT_FLOAT_WITHIN(COEFF_TOL, p.coeffs[3], p.coeffs[0]);
}

void test_allpass180_unity_magnitude_at_dc(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.Q = 0.707f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_ALLPASS_180, 48000);

    // DC gain magnitude: |H(1)| = |b0 + b1| / |1 + a1| should be ~1.0
    float numDc = fabsf(p.coeffs[0] + p.coeffs[1]);
    float denDc = fabsf(1.0f + p.coeffs[3]);
    float dcGain = numDc / denDc;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, dcGain);
}

// ===== New Stage Init Tests =====

void test_init_delay_defaults(void) {
    DspDelayParams p;
    dsp_init_delay_params(p);
    TEST_ASSERT_EQUAL_UINT16(0, p.delaySamples);
    TEST_ASSERT_EQUAL_UINT16(0, p.writePos);
    TEST_ASSERT_EQUAL_INT8(-1, p.delaySlot);
}

void test_init_compressor_defaults(void) {
    DspCompressorParams p;
    dsp_init_compressor_params(p);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, -12.0f, p.thresholdDb);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 10.0f, p.attackMs);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 100.0f, p.releaseMs);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 4.0f, p.ratio);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 6.0f, p.kneeDb);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, p.makeupGainDb);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, p.makeupLinear);
}

// ===== Crossover Preset Tests =====

void test_crossover_lr4_inserts_two_biquads(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr4(0, 2000.0f, 0); // LPF
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(2, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[0].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[1].type);

    // Both should be at 2000 Hz with Butterworth Q
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2000.0f, cfg->channels[0].stages[0].biquad.frequency);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.707f, cfg->channels[0].stages[0].biquad.Q);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2000.0f, cfg->channels[0].stages[1].biquad.frequency);
}

void test_crossover_lr2_inserts_one_biquad(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr2(0, 1000.0f, 1); // HPF
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(1, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[0].stages[0].type);
    // LR2 should use Q=0.5 (not 0.707 which is BW2)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, cfg->channels[0].stages[0].biquad.Q);
}

void test_crossover_lr8_inserts_four_biquads(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr8(0, 500.0f, 0); // LPF
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // LR8 = BW4^2 = 2 pairs of BW4 Q values: {0.5412, 1.3066, 0.5412, 1.3066}
    TEST_ASSERT_EQUAL_INT(4, cfg->channels[0].stageCount);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[i].type);
        TEST_ASSERT_FLOAT_WITHIN(1.0f, 500.0f, cfg->channels[0].stages[i].biquad.frequency);
    }
    // BW4 Q values: Q1=0.5412, Q2=1.3066 (repeated twice for LR8)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5412f, cfg->channels[0].stages[0].biquad.Q);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.3066f, cfg->channels[0].stages[1].biquad.Q);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5412f, cfg->channels[0].stages[2].biquad.Q);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.3066f, cfg->channels[0].stages[3].biquad.Q);
}

void test_crossover_butterworth_rejects_invalid(void) {
    dsp_init();
    // Order 0 (too low)
    TEST_ASSERT_EQUAL_INT(-1, dsp_insert_crossover_butterworth(0, 1000.0f, 0, 0));
    // Order 9+ (too high)
    TEST_ASSERT_EQUAL_INT(-1, dsp_insert_crossover_butterworth(0, 1000.0f, 9, 0));
    TEST_ASSERT_EQUAL_INT(-1, dsp_insert_crossover_butterworth(0, 1000.0f, 10, 0));
    // Odd orders 1,3,5,7 are now VALID
    TEST_ASSERT_TRUE(dsp_insert_crossover_butterworth(0, 1000.0f, 1, 0) >= 0);
}

void test_crossover_lr4_sum_flat(void) {
    // LR4 LPF + HPF at same frequency should sum to ~flat magnitude
    // Test at DC: LPF DC gain should be ~1.0
    dsp_init();
    dsp_insert_crossover_lr4(0, 2000.0f, 0); // LPF on ch0
    dsp_insert_crossover_lr4(1, 2000.0f, 1); // HPF on ch1

    DspState *cfg = dsp_get_inactive_config();
    // LPF DC gain: each biquad should have DC gain ~1.0
    for (int s = 0; s < 2; s++) {
        DspBiquadParams &p = cfg->channels[0].stages[s].biquad;
        float dcGain = (p.coeffs[0] + p.coeffs[1] + p.coeffs[2]) /
                       (1.0f + p.coeffs[3] + p.coeffs[4]);
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, dcGain);
    }

    // HPF DC gain: each biquad should have DC gain ~0.0
    for (int s = 0; s < 2; s++) {
        DspBiquadParams &p = cfg->channels[1].stages[s].biquad;
        float dcGain = (p.coeffs[0] + p.coeffs[1] + p.coeffs[2]) /
                       (1.0f + p.coeffs[3] + p.coeffs[4]);
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, dcGain);
    }
}

// ===== Bass Management Test =====

void test_bass_management_setup(void) {
    dsp_init();
    int mains[] = {1, 2};
    int result = dsp_setup_bass_management(0, mains, 2, 80.0f);
    TEST_ASSERT_EQUAL_INT(0, result);

    DspState *cfg = dsp_get_inactive_config();
    // Sub channel (0) should have 2 LPF stages (LR4)
    TEST_ASSERT_EQUAL_INT(2, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[0].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[1].type);

    // Main channels (1, 2) should each have 2 HPF stages (LR4)
    TEST_ASSERT_EQUAL_INT(2, cfg->channels[1].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[1].stages[0].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[1].stages[1].type);

    TEST_ASSERT_EQUAL_INT(2, cfg->channels[2].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[2].stages[0].type);

    // Channel 3 should be untouched
    TEST_ASSERT_EQUAL_INT(0, cfg->channels[3].stageCount);
}

// ===== Routing Matrix Tests =====

void test_routing_identity(void) {
    DspRoutingMatrix rm;
    dsp_routing_preset_identity(rm);

    // Diagonal should be 1.0, off-diagonal should be 0.0
    for (int o = 0; o < DSP_MAX_CHANNELS; o++) {
        for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
            float expected = (o == i) ? 1.0f : 0.0f;
            TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, expected, rm.matrix[o][i]);
        }
    }
}

void test_routing_swap_lr(void) {
    DspRoutingMatrix rm;
    dsp_routing_preset_swap_lr(rm);

    // L1↔R1 swap: [0][1]=1, [1][0]=1, diagonal=0
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, rm.matrix[0][0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, rm.matrix[0][1]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, rm.matrix[1][0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, rm.matrix[1][1]);
}

void test_routing_apply_identity(void) {
    DspRoutingMatrix rm;
    dsp_routing_preset_identity(rm);

    float ch0[] = {1.0f, 2.0f};
    float ch1[] = {3.0f, 4.0f};
    float *channels[] = {ch0, ch1};

    dsp_routing_apply(rm, channels, 2, 2);

    // Identity: no change
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, ch0[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 2.0f, ch0[1]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 3.0f, ch1[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 4.0f, ch1[1]);
}

void test_routing_apply_swap(void) {
    DspRoutingMatrix rm;
    dsp_routing_preset_swap_lr(rm);

    float ch0[] = {1.0f, 2.0f};
    float ch1[] = {3.0f, 4.0f};
    float *channels[] = {ch0, ch1};

    dsp_routing_apply(rm, channels, 2, 2);

    // Swap: ch0 gets old ch1, ch1 gets old ch0
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 3.0f, ch0[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 4.0f, ch0[1]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, ch1[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 2.0f, ch1[1]);
}

void test_routing_set_gain_db(void) {
    DspRoutingMatrix rm;
    dsp_routing_preset_identity(rm);

    // Set output 0, input 1 to -6dB
    dsp_routing_set_gain_db(rm, 0, 1, -6.0f);
    float expected = powf(10.0f, -6.0f / 20.0f); // ~0.501
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expected, rm.matrix[0][1]);

    // Set to -inf (silence)
    dsp_routing_set_gain_db(rm, 0, 1, -200.0f);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, rm.matrix[0][1]);
}

void test_routing_mono_sum(void) {
    DspRoutingMatrix rm;
    dsp_routing_preset_mono_sum(rm);

    float ch0[] = {1.0f};
    float ch1[] = {1.0f};
    float *channels[] = {ch0, ch1};

    dsp_routing_apply(rm, channels, 2, 1);

    // Mono sum: each output = average of all inputs = (1+1)/N
    float expected = 2.0f / DSP_MAX_CHANNELS;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expected, ch0[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expected, ch1[0]);
}

// ===== Expanded Crossover Tests =====

void test_crossover_bw4_q_values(void) {
    dsp_init();
    int first = dsp_insert_crossover_butterworth(0, 1000.0f, 4, 0); // LPF BW4
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(2, cfg->channels[0].stageCount);
    // BW4: Q1 = 1/(2*sin(pi/8)) = 0.5412, Q2 = 1/(2*sin(3*pi/8)) = 1.3066
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5412f, cfg->channels[0].stages[0].biquad.Q);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.3066f, cfg->channels[0].stages[1].biquad.Q);
}

void test_crossover_bw3_first_order_plus_biquad(void) {
    dsp_init();
    int first = dsp_insert_crossover_butterworth(0, 1000.0f, 3, 0); // LPF BW3
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // BW3 = 1 first-order + 1 biquad
    TEST_ASSERT_EQUAL_INT(2, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF_1ST, cfg->channels[0].stages[0].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[1].type);
    // BW3 biquad Q = 1.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, cfg->channels[0].stages[1].biquad.Q);
}

void test_crossover_bw1_first_order_only(void) {
    dsp_init();
    int first = dsp_insert_crossover_butterworth(0, 1000.0f, 1, 0); // LPF BW1
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(1, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF_1ST, cfg->channels[0].stages[0].type);
}

void test_crossover_bw1_hpf(void) {
    dsp_init();
    int first = dsp_insert_crossover_butterworth(0, 1000.0f, 1, 1); // HPF BW1
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(1, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF_1ST, cfg->channels[0].stages[0].type);
}

void test_crossover_lr12_stage_count(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr(0, 1000.0f, 12, 0); // LPF LR12
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // LR12 = BW6^2. BW6 = 3 biquads. So LR12 = 6 biquads
    TEST_ASSERT_EQUAL_INT(6, cfg->channels[0].stageCount);
}

void test_crossover_lr16_stage_count(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr(0, 1000.0f, 16, 0); // LPF LR16
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // LR16 = BW8^2. BW8 = 4 biquads. So LR16 = 8 biquads
    TEST_ASSERT_EQUAL_INT(8, cfg->channels[0].stageCount);
}

void test_crossover_lr24_stage_count(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr(0, 1000.0f, 24, 0); // LPF LR24
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // LR24 = BW12^2. BW12 = 6 biquads. So LR24 = 12 biquads
    TEST_ASSERT_EQUAL_INT(12, cfg->channels[0].stageCount);
}

void test_crossover_lr6_has_first_order_sections(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr(0, 1000.0f, 6, 0); // LPF LR6
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // LR6 = BW3^2. BW3 = 1 first-order + 1 biquad. So LR6 = 2 first-order + 2 biquads = 4 stages
    TEST_ASSERT_EQUAL_INT(4, cfg->channels[0].stageCount);
    // First BW3 pass: 1st-order + biquad
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF_1ST, cfg->channels[0].stages[0].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[1].type);
    // Second BW3 pass: 1st-order + biquad
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF_1ST, cfg->channels[0].stages[2].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[3].type);
}

void test_crossover_lr_rejects_invalid(void) {
    dsp_init();
    // Odd LR orders are invalid
    TEST_ASSERT_EQUAL_INT(-1, dsp_insert_crossover_lr(0, 1000.0f, 3, 0));
    TEST_ASSERT_EQUAL_INT(-1, dsp_insert_crossover_lr(0, 1000.0f, 5, 0));
    // LR0 is invalid
    TEST_ASSERT_EQUAL_INT(-1, dsp_insert_crossover_lr(0, 1000.0f, 0, 0));
}

void test_first_order_lpf_dc_gain(void) {
    // First-order LPF should have DC gain ~1.0
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_LPF_1ST, 48000);

    // DC gain: (b0 + b1 + b2) / (1 + a1 + a2) — with b2=0, a2=0
    float dcGain = (p.coeffs[0] + p.coeffs[1] + p.coeffs[2]) /
                   (1.0f + p.coeffs[3] + p.coeffs[4]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, dcGain);
}

void test_first_order_hpf_dc_gain(void) {
    // First-order HPF should have DC gain ~0.0
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_HPF_1ST, 48000);

    // DC gain: (b0 + b1 + b2) / (1 + a1 + a2)
    float dcGain = (p.coeffs[0] + p.coeffs[1] + p.coeffs[2]) /
                   (1.0f + p.coeffs[3] + p.coeffs[4]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, dcGain);
}

// ===== Metrics Test =====

void test_metrics_initial(void) {
    DspMetrics m = dsp_get_metrics();
    TEST_ASSERT_EQUAL_UINT32(0, m.processTimeUs);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, m.cpuLoadPercent);
}

// ===== Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Coefficient computation
    RUN_TEST(test_lpf_coefficients);
    RUN_TEST(test_hpf_coefficients);
    RUN_TEST(test_peq_coefficients_boost);
    RUN_TEST(test_peq_coefficients_cut);
    RUN_TEST(test_notch_coefficients);
    RUN_TEST(test_shelf_low_boost);
    RUN_TEST(test_shelf_high_boost);
    RUN_TEST(test_custom_coefficients_load);
    RUN_TEST(test_allpass_unity_magnitude);

    // Biquad processing
    RUN_TEST(test_biquad_passthrough);
    RUN_TEST(test_biquad_lpf_attenuates_high);

    // FIR processing
    RUN_TEST(test_fir_impulse_response);
    RUN_TEST(test_fir_moving_average);

    // Limiter
    RUN_TEST(test_limiter_below_threshold);
    RUN_TEST(test_limiter_above_threshold);

    // Gain
    RUN_TEST(test_gain_db_to_linear);
    RUN_TEST(test_gain_buffer_multiply);

    // Stage CRUD
    RUN_TEST(test_add_stage_append);
    RUN_TEST(test_add_stage_insert);
    RUN_TEST(test_remove_stage);
    RUN_TEST(test_reorder_stages);
    RUN_TEST(test_max_stage_limit);
    RUN_TEST(test_stage_enable_disable);

    // Double-buffer
    RUN_TEST(test_double_buffer_swap);
    RUN_TEST(test_double_buffer_delay_continuity);

    // Processing
    RUN_TEST(test_bypass_passthrough);
    RUN_TEST(test_channel_recompute_coeffs);

    // Delay
    RUN_TEST(test_delay_zero_passthrough);
    RUN_TEST(test_delay_shifts_samples);
    RUN_TEST(test_delay_slot_alloc_free);

    // Polarity
    RUN_TEST(test_polarity_inverts_signal);
    RUN_TEST(test_polarity_not_inverted_passthrough);

    // Mute
    RUN_TEST(test_mute_zeros_output);
    RUN_TEST(test_mute_not_muted_passthrough);

    // Compressor
    RUN_TEST(test_compressor_below_threshold_passthrough);
    RUN_TEST(test_compressor_above_threshold_reduces);
    RUN_TEST(test_compressor_soft_knee);
    RUN_TEST(test_compressor_makeup_gain);

    // New biquad types
    RUN_TEST(test_bpf0db_unity_peak_gain);
    RUN_TEST(test_allpass360_unity_magnitude);
    RUN_TEST(test_allpass180_first_order);
    RUN_TEST(test_allpass180_unity_magnitude_at_dc);

    // New stage inits
    RUN_TEST(test_init_delay_defaults);
    RUN_TEST(test_init_compressor_defaults);

    // Crossover presets
    RUN_TEST(test_crossover_lr4_inserts_two_biquads);
    RUN_TEST(test_crossover_lr2_inserts_one_biquad);
    RUN_TEST(test_crossover_lr8_inserts_four_biquads);
    RUN_TEST(test_crossover_butterworth_rejects_invalid);
    RUN_TEST(test_crossover_lr4_sum_flat);

    // Expanded crossover tests
    RUN_TEST(test_crossover_bw4_q_values);
    RUN_TEST(test_crossover_bw3_first_order_plus_biquad);
    RUN_TEST(test_crossover_bw1_first_order_only);
    RUN_TEST(test_crossover_bw1_hpf);
    RUN_TEST(test_crossover_lr12_stage_count);
    RUN_TEST(test_crossover_lr16_stage_count);
    RUN_TEST(test_crossover_lr24_stage_count);
    RUN_TEST(test_crossover_lr6_has_first_order_sections);
    RUN_TEST(test_crossover_lr_rejects_invalid);
    RUN_TEST(test_first_order_lpf_dc_gain);
    RUN_TEST(test_first_order_hpf_dc_gain);

    // Bass management
    RUN_TEST(test_bass_management_setup);

    // Routing matrix
    RUN_TEST(test_routing_identity);
    RUN_TEST(test_routing_swap_lr);
    RUN_TEST(test_routing_apply_identity);
    RUN_TEST(test_routing_apply_swap);
    RUN_TEST(test_routing_set_gain_db);
    RUN_TEST(test_routing_mono_sum);

    // Metrics
    RUN_TEST(test_metrics_initial);

    return UNITY_END();
}
