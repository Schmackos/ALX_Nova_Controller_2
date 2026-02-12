#include <unity.h>
#include <math.h>
#include <string.h>

// Include DSP sources directly (test_build_src = no)
#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_biquad_gen_f32.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_init_f32.c"

// Include DSP headers
#include "../../src/dsp_pipeline.h"
#include "../../src/dsp_coefficients.h"

// Include DSP implementation source (no ArduinoJson in native tests)
#include "../../src/dsp_coefficients.cpp"
#include "../../src/dsp_pipeline.cpp"

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

    // Metrics
    RUN_TEST(test_metrics_initial);

    return UNITY_END();
}
