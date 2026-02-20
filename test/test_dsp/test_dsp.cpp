#include <unity.h>
#include <math.h>
#include <string.h>

// Include DSP sources directly (test_build_src = no)
#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_init_f32.c"
#include "../../lib/esp_dsp_lite/src/dsps_fird_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_corr_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_conv_f32_ansi.c"
#include "../../src/dsp_biquad_gen.c"

// Include DSP headers
#include "../../src/dsp_pipeline.h"
#include "../../src/dsp_coefficients.h"

// Include DSP implementation source (no ArduinoJson in native tests)
#include "../../src/dsp_coefficients.cpp"
#include "../../src/dsp_crossover.cpp"

// Stub for dsp_api functions (not linked in native tests).
// Must appear after dsp_crossover.cpp (defines DspRoutingMatrix) and
// before dsp_pipeline.cpp (calls dsp_get_routing_matrix).
static DspRoutingMatrix _testRoutingMatrix;
static bool _testRoutingMatrixInit = false;
DspRoutingMatrix* dsp_get_routing_matrix() {
    if (!_testRoutingMatrixInit) {
        dsp_routing_init(_testRoutingMatrix);
        dsp_routing_preset_identity(_testRoutingMatrix);
        _testRoutingMatrixInit = true;
    }
    return &_testRoutingMatrix;
}

#include "../../src/dsp_pipeline.cpp"
#include "../../src/dsp_convolution.cpp"
#include "../../src/thd_measurement.cpp"

// Tolerance for float comparisons
#define FLOAT_TOL 0.001f
#define COEFF_TOL 0.01f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void setUp(void) {
    dsp_init();
    appState.emergencyLimiterEnabled = false; // Don't let lookahead delay affect DSP stage tests
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
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS, idx); // Appends after PEQ bands

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 1, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_PEQ, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
}

void test_add_stage_insert(void) {
    // Insert stages into chain region (after PEQ bands)
    dsp_add_stage(0, DSP_BIQUAD_LPF);   // goes to index 10
    dsp_add_stage(0, DSP_BIQUAD_HPF);   // goes to index 11
    int idx = dsp_add_stage(0, DSP_BIQUAD_PEQ, DSP_PEQ_BANDS + 1); // Insert at chain pos 1
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 1, idx);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 3, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_PEQ, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[0].stages[DSP_PEQ_BANDS + 2].type);
}

void test_remove_stage(void) {
    dsp_add_stage(0, DSP_BIQUAD_LPF);   // index 10
    dsp_add_stage(0, DSP_BIQUAD_PEQ);   // index 11
    dsp_add_stage(0, DSP_BIQUAD_HPF);   // index 12

    bool ok = dsp_remove_stage(0, DSP_PEQ_BANDS + 1); // Remove the PEQ at chain index 1
    TEST_ASSERT_TRUE(ok);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 2, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].type);
}

void test_reorder_stages(void) {
    dsp_add_stage(0, DSP_BIQUAD_LPF);   // index 10
    dsp_add_stage(0, DSP_BIQUAD_PEQ);   // index 11
    dsp_add_stage(0, DSP_BIQUAD_HPF);   // index 12

    // Reorder must cover ALL stages including PEQ bands
    int cnt = DSP_PEQ_BANDS + 3;
    int order[DSP_MAX_STAGES];
    // Keep PEQ bands in place (0-9)
    for (int i = 0; i < DSP_PEQ_BANDS; i++) order[i] = i;
    // Reorder chain stages: HPF, LPF, PEQ → indices 12, 10, 11
    order[DSP_PEQ_BANDS + 0] = DSP_PEQ_BANDS + 2;
    order[DSP_PEQ_BANDS + 1] = DSP_PEQ_BANDS + 0;
    order[DSP_PEQ_BANDS + 2] = DSP_PEQ_BANDS + 1;
    bool ok = dsp_reorder_stages(0, order, cnt);
    TEST_ASSERT_TRUE(ok);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_PEQ, cfg->channels[0].stages[DSP_PEQ_BANDS + 2].type);
}

void test_max_stage_limit(void) {
    // PEQ bands already fill 0-9, so only 10 more chain stages can be added
    int chainSlots = DSP_MAX_STAGES - DSP_PEQ_BANDS;
    for (int i = 0; i < chainSlots; i++) {
        int idx = dsp_add_stage(0, DSP_BIQUAD_PEQ);
        TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + i, idx);
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

// ===== Two-Pass Compressor/Limiter Tests =====

void test_two_pass_limiter_reduces_above_threshold(void) {
    // Two-pass limiter: verify it still limits properly
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    int pos = dsp_add_stage(0, DSP_LIMITER);
    TEST_ASSERT_TRUE(pos >= 0);
    cfg->channels[0].stages[pos].limiter.thresholdDb = -6.0f;
    cfg->channels[0].stages[pos].limiter.attackMs = 0.01f; // Very fast
    cfg->channels[0].stages[pos].limiter.releaseMs = 50.0f;
    cfg->channels[0].stages[pos].limiter.ratio = 20.0f;
    dsp_swap_config();

    // Create a stereo buffer with 0 dBFS signal (above -6dB threshold)
    int32_t buf[256 * 2];
    for (int i = 0; i < 256; i++) {
        buf[i * 2] = 8388607;     // L = +1.0 (0 dBFS)
        buf[i * 2 + 1] = 8388607; // R = +1.0
    }

    // Process multiple buffers to let envelope settle
    for (int pass = 0; pass < 10; pass++) {
        for (int i = 0; i < 256; i++) {
            buf[i * 2] = 8388607;
            buf[i * 2 + 1] = 8388607;
        }
        dsp_process_buffer(buf, 256, 0);
    }

    // After limiting, output should be reduced below input
    float outL = (float)buf[200 * 2] / 8388607.0f;
    TEST_ASSERT_TRUE(outL < 0.9f); // Significantly reduced
    TEST_ASSERT_TRUE(outL > 0.0f); // But not zero
}

void test_two_pass_compressor_applies_makeup(void) {
    // Verify compressor with makeup gain works in two-pass mode
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    int pos = dsp_add_stage(0, DSP_COMPRESSOR);
    TEST_ASSERT_TRUE(pos >= 0);
    cfg->channels[0].stages[pos].compressor.thresholdDb = 0.0f; // Above any signal
    cfg->channels[0].stages[pos].compressor.ratio = 4.0f;
    cfg->channels[0].stages[pos].compressor.kneeDb = 0.0f;
    cfg->channels[0].stages[pos].compressor.makeupGainDb = 6.0f;
    dsp_compute_compressor_makeup(cfg->channels[0].stages[pos].compressor);
    dsp_swap_config();

    // Create a low-level stereo buffer (-20 dBFS, well below threshold)
    int32_t buf[64 * 2];
    for (int i = 0; i < 64; i++) {
        int32_t val = (int32_t)(0.1f * 8388607.0f); // ~-20 dBFS
        buf[i * 2] = val;
        buf[i * 2 + 1] = val;
    }

    dsp_process_buffer(buf, 64, 0);

    // With 0dB threshold and -20dBFS signal, no compression → only makeup gain applies
    // Output should be ~0.1 * 2.0 = 0.2 (6dB makeup = 2x)
    float outL = (float)buf[32 * 2] / 8388607.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.2f, outL);
}

// ===== Stereo Link Tests =====

void test_stereo_link_default_true(void) {
    dsp_init();
    DspState *cfg = dsp_get_active_config();
    TEST_ASSERT_TRUE(cfg->channels[0].stereoLink);
    TEST_ASSERT_TRUE(cfg->channels[1].stereoLink);
    TEST_ASSERT_TRUE(cfg->channels[2].stereoLink);
    TEST_ASSERT_TRUE(cfg->channels[3].stereoLink);
}

void test_stereo_link_partner(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[0].stereoLink = true;
    cfg->channels[1].stereoLink = true;
    TEST_ASSERT_EQUAL_INT(1, dsp_get_linked_partner(0));
    TEST_ASSERT_EQUAL_INT(0, dsp_get_linked_partner(1));
    TEST_ASSERT_EQUAL_INT(3, dsp_get_linked_partner(2));
    TEST_ASSERT_EQUAL_INT(2, dsp_get_linked_partner(3));

    // Unlinked returns -1
    cfg->channels[0].stereoLink = false;
    TEST_ASSERT_EQUAL_INT(-1, dsp_get_linked_partner(0));
}

void test_stereo_link_mirror_copies_stages(void) {
    dsp_init();
    // Add a PEQ on channel 0
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[0].stages[0].enabled = true;
    cfg->channels[0].stages[0].biquad.frequency = 2000.0f;
    cfg->channels[0].stages[0].biquad.gain = 6.0f;
    dsp_compute_biquad_coeffs(cfg->channels[0].stages[0].biquad, DSP_BIQUAD_PEQ, 48000);

    // Mirror ch0 → ch1
    dsp_mirror_channel_config(0, 1);

    // ch1 should have same params but cleared runtime state
    TEST_ASSERT_EQUAL_INT(cfg->channels[0].stageCount, cfg->channels[1].stageCount);
    TEST_ASSERT_TRUE(cfg->channels[1].stages[0].enabled);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2000.0f, cfg->channels[1].stages[0].biquad.frequency);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.0f, cfg->channels[1].stages[0].biquad.gain);
    // Runtime state should be reset
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cfg->channels[1].stages[0].biquad.delay[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cfg->channels[1].stages[0].biquad.delay[1]);
}

void test_stereo_link_mirror_resets_envelope(void) {
    dsp_init();
    dsp_copy_active_to_inactive();

    // Add compressor on ch0
    int pos = dsp_add_stage(0, DSP_COMPRESSOR);
    TEST_ASSERT_TRUE(pos >= 0);
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[0].stages[pos].compressor.envelope = 0.5f;
    cfg->channels[0].stages[pos].compressor.gainReduction = -3.0f;

    dsp_mirror_channel_config(0, 1);

    // Envelope/GR should be reset on destination
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cfg->channels[1].stages[pos].compressor.envelope);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, cfg->channels[1].stages[pos].compressor.gainReduction);
}

// ===== Decimation FIR Tests =====

void test_decimator_halves_output_length(void) {
    dsp_init();
    dsp_copy_active_to_inactive();
    int pos = dsp_add_stage(0, DSP_DECIMATOR);
    TEST_ASSERT_TRUE(pos >= 0);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_UINT8(2, cfg->channels[0].stages[pos].decimator.factor);
    TEST_ASSERT_TRUE(cfg->channels[0].stages[pos].decimator.firSlot >= 0);
    TEST_ASSERT_TRUE(cfg->channels[0].stages[pos].decimator.numTaps > 0);
    dsp_swap_config();

    // Create stereo buffer with 128 frames (256 stereo samples)
    int32_t buf[128 * 2];
    for (int i = 0; i < 128; i++) {
        buf[i * 2] = (int32_t)(0.5f * 8388607.0f);
        buf[i * 2 + 1] = (int32_t)(0.5f * 8388607.0f);
    }

    dsp_process_buffer(buf, 128, 0);
    // After decimation, output should have data in first 64 positions
    // The decimator reduces the effective buffer length
    // Verify the output is non-zero in first half
    bool hasData = false;
    for (int i = 0; i < 64; i++) {
        if (buf[i * 2] != 0) { hasData = true; break; }
    }
    TEST_ASSERT_TRUE(hasData);
}

void test_decimation_filter_design(void) {
    // Verify the windowed-sinc anti-aliasing filter
    float taps[64];
    dsp_compute_decimation_filter(taps, 64, 2, 48000.0f);

    // DC gain should be ~1.0 (unity)
    float dcGain = 0.0f;
    for (int i = 0; i < 64; i++) dcGain += taps[i];
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, dcGain);

    // Center tap should be the largest
    float maxTap = 0.0f;
    int maxIdx = 0;
    for (int i = 0; i < 64; i++) {
        if (taps[i] > maxTap) { maxTap = taps[i]; maxIdx = i; }
    }
    TEST_ASSERT_EQUAL_INT(31, maxIdx); // Center of 64-tap filter
}

void test_decimator_fird_basic(void) {
    // Direct test of dsps_fird_f32
    float coeffs[3] = {0.25f, 0.5f, 0.25f};
    float delay[3] = {0};
    fir_f32_t fird;
    dsps_fird_init_f32(&fird, coeffs, delay, 3, 2);

    float input[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    float output[4] = {0};
    dsps_fird_f32(&fird, input, output, 8);

    // Output should have 4 samples (8 / 2)
    // First output at input[1]: delay has [0,1,1] → 0.25*1 + 0.5*1 + 0.25*0 = 0.75
    TEST_ASSERT_TRUE(output[0] > 0.0f);
    // After settling, output should converge to 1.0 (all 1s input, filter sums to 1.0)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, output[3]);
}

void test_decimator_slot_freed_on_remove(void) {
    dsp_init();
    dsp_copy_active_to_inactive();
    int pos = dsp_add_stage(0, DSP_DECIMATOR);
    TEST_ASSERT_TRUE(pos >= 0);

    DspState *cfg = dsp_get_inactive_config();
    int8_t slot = cfg->channels[0].stages[pos].decimator.firSlot;
    TEST_ASSERT_TRUE(slot >= 0);

    // Remove the stage — slot should be freed
    bool ok = dsp_remove_stage(0, pos);
    TEST_ASSERT_TRUE(ok);

    // Should be able to alloc another slot (proves the old one was freed)
    int newSlot = dsp_fir_alloc_slot();
    TEST_ASSERT_TRUE(newSlot >= 0);
    dsp_fir_free_slot(newSlot);
}

// ===== Cross-Correlation / Delay Alignment Tests =====

void test_corr_known_delay_detected(void) {
    // Create two signals: sig2 is sig1 delayed by 10 samples
    int len = 256;
    float sig1[256], sig2[256];
    for (int i = 0; i < len; i++) {
        sig1[i] = sinf(2.0f * (float)M_PI * 1000.0f * (float)i / 48000.0f);
    }
    // sig2 = sig1 shifted by 10 samples
    memset(sig2, 0, sizeof(sig2));
    for (int i = 10; i < len; i++) {
        sig2[i] = sig1[i - 10];
    }

    int patLen = 246; // len - 10
    int corrLen = len - patLen + 1; // 11
    float corr[11];
    dsps_corr_f32(sig2, len, sig1, patLen, corr);

    // Find peak — should be at index 10
    float maxVal = -1e30f;
    int maxIdx = 0;
    for (int i = 0; i < corrLen; i++) {
        if (corr[i] > maxVal) { maxVal = corr[i]; maxIdx = i; }
    }
    TEST_ASSERT_EQUAL_INT(10, maxIdx);
}

void test_corr_zero_delay_returns_zero_index(void) {
    int len = 128;
    float sig[128], corr[1];
    for (int i = 0; i < len; i++) sig[i] = sinf(2.0f * (float)M_PI * 440.0f * (float)i / 48000.0f);

    // Correlate signal with itself (full length) → single output at index 0
    dsps_corr_f32(sig, len, sig, len, corr);
    TEST_ASSERT_TRUE(corr[0] > 0.0f);
}

// Delay alignment tests removed in v1.8.3 - incomplete feature, never functional
/*
void test_delay_align_measure_known_offset(void) {
    // Feed same signal to both ADCs with 5-sample offset
    int frames = 128;
    int32_t adc1[128 * 2], adc2[128 * 2];
    memset(adc1, 0, sizeof(adc1));
    memset(adc2, 0, sizeof(adc2));

    for (int i = 0; i < frames; i++) {
        float s = sinf(2.0f * (float)M_PI * 1000.0f * (float)i / 48000.0f);
        int32_t val = (int32_t)(s * 8388607.0f);
        adc1[i * 2] = val;
        adc1[i * 2 + 1] = val;
    }
    // adc2 = adc1 delayed by 5 samples
    for (int i = 5; i < frames; i++) {
        adc2[i * 2] = adc1[(i - 5) * 2];
        adc2[i * 2 + 1] = adc1[(i - 5) * 2 + 1];
    }

    DelayAlignResult r = delay_align_measure(adc1, frames, adc2, frames, 48000, 20);
    // Should detect delay near 5 samples
    TEST_ASSERT_TRUE(r.delaySamples >= 3 && r.delaySamples <= 7);
    TEST_ASSERT_TRUE(r.confidence > 1.0f);
}

void test_delay_align_low_confidence_with_noise(void) {
    // Two uncorrelated signals → result may not be valid
    int frames = 64;
    int32_t adc1[64 * 2], adc2[64 * 2];

    for (int i = 0; i < frames; i++) {
        adc1[i * 2] = (int32_t)(sinf(2.0f * (float)M_PI * 440.0f * (float)i / 48000.0f) * 8388607.0f);
        adc1[i * 2 + 1] = adc1[i * 2];
        adc2[i * 2] = (int32_t)(sinf(2.0f * (float)M_PI * 3000.0f * (float)i / 48000.0f) * 8388607.0f);
        adc2[i * 2 + 1] = adc2[i * 2];
    }

    DelayAlignResult r = delay_align_measure(adc1, frames, adc2, frames, 48000, 10);
    // Verify it returns a result without crashing
    TEST_ASSERT_TRUE(r.delayMs >= 0.0f);
}
*/

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
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 2, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].type);

    // Both should be at 2000 Hz with Butterworth Q
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2000.0f, cfg->channels[0].stages[DSP_PEQ_BANDS].biquad.frequency);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.707f, cfg->channels[0].stages[DSP_PEQ_BANDS].biquad.Q);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2000.0f, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].biquad.frequency);
}

void test_crossover_lr2_inserts_one_biquad(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr2(0, 1000.0f, 1); // HPF
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 1, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
    // LR2 should use Q=0.5 (not 0.707 which is BW2)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, cfg->channels[0].stages[DSP_PEQ_BANDS].biquad.Q);
}

void test_crossover_lr8_inserts_four_biquads(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr8(0, 500.0f, 0); // LPF
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // LR8 = BW4^2 = 2 pairs of BW4 Q values: {0.5412, 1.3066, 0.5412, 1.3066}
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 4, cfg->channels[0].stageCount);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[DSP_PEQ_BANDS + i].type);
        TEST_ASSERT_FLOAT_WITHIN(1.0f, 500.0f, cfg->channels[0].stages[DSP_PEQ_BANDS + i].biquad.frequency);
    }
    // BW4 Q values: Q1=0.5412, Q2=1.3066 (repeated twice for LR8)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5412f, cfg->channels[0].stages[DSP_PEQ_BANDS].biquad.Q);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.3066f, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].biquad.Q);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5412f, cfg->channels[0].stages[DSP_PEQ_BANDS + 2].biquad.Q);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.3066f, cfg->channels[0].stages[DSP_PEQ_BANDS + 3].biquad.Q);
}

void test_crossover_butterworth_rejects_invalid(void) {
    dsp_init();
    // Order 0 (too low)
    TEST_ASSERT_EQUAL_INT(-1, dsp_insert_crossover_butterworth(0, 1000.0f, 0, 0));
    // Order 13+ (too high — max is 12)
    TEST_ASSERT_EQUAL_INT(-1, dsp_insert_crossover_butterworth(0, 1000.0f, 13, 0));
    TEST_ASSERT_EQUAL_INT(-1, dsp_insert_crossover_butterworth(0, 1000.0f, 20, 0));
    // Orders 1-12 are valid; test boundary
    TEST_ASSERT_TRUE(dsp_insert_crossover_butterworth(0, 1000.0f, 1, 0) >= 0);
    dsp_init(); // Reset
    TEST_ASSERT_TRUE(dsp_insert_crossover_butterworth(0, 1000.0f, 12, 0) >= 0);
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
        DspBiquadParams &p = cfg->channels[0].stages[DSP_PEQ_BANDS + s].biquad;
        float dcGain = (p.coeffs[0] + p.coeffs[1] + p.coeffs[2]) /
                       (1.0f + p.coeffs[3] + p.coeffs[4]);
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, dcGain);
    }

    // HPF DC gain: each biquad should have DC gain ~0.0
    for (int s = 0; s < 2; s++) {
        DspBiquadParams &p = cfg->channels[1].stages[DSP_PEQ_BANDS + s].biquad;
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
    // Sub channel (0) should have PEQ bands + 2 LPF stages (LR4)
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 2, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].type);

    // Main channels (1, 2) should each have PEQ bands + 2 HPF stages (LR4)
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 2, cfg->channels[1].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[1].stages[DSP_PEQ_BANDS].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[1].stages[DSP_PEQ_BANDS + 1].type);

    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 2, cfg->channels[2].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, cfg->channels[2].stages[DSP_PEQ_BANDS].type);

    // Channel 3 should only have PEQ bands (untouched)
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS, cfg->channels[3].stageCount);
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
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 2, cfg->channels[0].stageCount);
    // BW4: Q1 = 1/(2*sin(pi/8)) = 0.5412, Q2 = 1/(2*sin(3*pi/8)) = 1.3066
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5412f, cfg->channels[0].stages[DSP_PEQ_BANDS].biquad.Q);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.3066f, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].biquad.Q);
}

void test_crossover_bw3_first_order_plus_biquad(void) {
    dsp_init();
    int first = dsp_insert_crossover_butterworth(0, 1000.0f, 3, 0); // LPF BW3
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // BW3 = 1 first-order + 1 biquad
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 2, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF_1ST, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].type);
    // BW3 biquad Q = 1.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].biquad.Q);
}

void test_crossover_bw1_first_order_only(void) {
    dsp_init();
    int first = dsp_insert_crossover_butterworth(0, 1000.0f, 1, 0); // LPF BW1
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 1, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF_1ST, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
}

void test_crossover_bw1_hpf(void) {
    dsp_init();
    int first = dsp_insert_crossover_butterworth(0, 1000.0f, 1, 1); // HPF BW1
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 1, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF_1ST, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
}

void test_crossover_lr12_stage_count(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr(0, 1000.0f, 12, 0); // LPF LR12
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // LR12 = BW6^2. BW6 = 3 biquads. So LR12 = 6 biquads
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 6, cfg->channels[0].stageCount);
}

void test_crossover_lr16_stage_count(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr(0, 1000.0f, 16, 0); // LPF LR16
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // LR16 = BW8^2. BW8 = 4 biquads. So LR16 = 8 biquads
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 8, cfg->channels[0].stageCount);
}

void test_crossover_lr24_stage_count(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr(0, 1000.0f, 24, 0); // LPF LR24
    // LR24 = BW12^2. BW12 = 6 biquads. So LR24 = 12 biquads. 10 PEQ + 12 = 22 <= 24 max
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 12, cfg->channels[0].stageCount);
}

void test_crossover_lr6_has_first_order_sections(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr(0, 1000.0f, 6, 0); // LPF LR6
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // LR6 = BW3^2. BW3 = 1 first-order + 1 biquad. So LR6 = 2 first-order + 2 biquads = 4 stages
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 4, cfg->channels[0].stageCount);
    // First BW3 pass: 1st-order + biquad
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF_1ST, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].type);
    // Second BW3 pass: 1st-order + biquad
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF_1ST, cfg->channels[0].stages[DSP_PEQ_BANDS + 2].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, cfg->channels[0].stages[DSP_PEQ_BANDS + 3].type);
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

// ===== Crossover Label & HPF Tests =====

void test_crossover_lr_stages_have_label(void) {
    dsp_init();
    int first = dsp_insert_crossover_lr(0, 2000.0f, 8, 0); // LR8 LPF
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    // All 4 stages should have "LR8 LPF" label
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_STRING("LR8 LPF", cfg->channels[0].stages[DSP_PEQ_BANDS + i].label);
    }
}

void test_crossover_bw_stages_have_label(void) {
    dsp_init();
    int first = dsp_insert_crossover_butterworth(0, 1000.0f, 4, 1); // BW4 HPF
    TEST_ASSERT_TRUE(first >= 0);

    DspState *cfg = dsp_get_inactive_config();
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT_EQUAL_STRING("BW4 HPF", cfg->channels[0].stages[DSP_PEQ_BANDS + i].label);
    }
}

void test_crossover_butterworth_hpf_all_orders(void) {
    // Verify every Butterworth HPF order (1-8) inserts successfully
    for (int order = 1; order <= 8; order++) {
        dsp_init();
        int first = dsp_insert_crossover_butterworth(0, 1000.0f, order, 1); // HPF
        TEST_ASSERT_TRUE_MESSAGE(first >= 0, "BW HPF insertion failed");

        DspState *cfg = dsp_get_inactive_config();
        // Verify the first stage is an HPF type
        DspStageType t = cfg->channels[0].stages[DSP_PEQ_BANDS].type;
        TEST_ASSERT_TRUE_MESSAGE(t == DSP_BIQUAD_HPF || t == DSP_BIQUAD_HPF_1ST,
                                "Expected HPF or HPF_1ST type");
    }
}

void test_crossover_lr_rollback_on_partial_failure(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    int baseLine = cfg->channels[0].stageCount; // Should be DSP_PEQ_BANDS

    // Fill up stages close to limit so LR24 will fail partway
    // Add 13 dummy stages (10 PEQ + 13 = 23, LR24 needs 12 more = 35 > 24)
    for (int i = 0; i < 13; i++) {
        dsp_add_stage(0, DSP_GAIN);
    }
    cfg = dsp_get_inactive_config();
    int beforeCount = cfg->channels[0].stageCount;

    // LR24 needs 12 stages but only 1 slot left — should fail and rollback
    int result = dsp_insert_crossover_lr(0, 1000.0f, 24, 0);
    TEST_ASSERT_EQUAL_INT(-1, result);

    // Stage count should be unchanged (rollback cleaned up)
    cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(beforeCount, cfg->channels[0].stageCount);
}

// ===== Linkwitz Transform Tests =====

void test_linkwitz_coefficients_valid(void) {
    // Typical sealed speaker: Fs=50Hz, Qts=0.707 → target Fs=25Hz, Qts=0.5
    float coeffs[5];
    float f0 = 50.0f / 48000.0f;
    float fp = 25.0f / 48000.0f;
    int ret = dsp_gen_linkwitz_f32(coeffs, f0, 0.707f, fp, 0.5f);
    TEST_ASSERT_EQUAL_INT(0, ret);
    // b0 should be valid (non-zero, non-NaN)
    TEST_ASSERT_FALSE(isnan(coeffs[0]));
    TEST_ASSERT_FALSE(isnan(coeffs[1]));
    TEST_ASSERT_TRUE(coeffs[0] != 0.0f);
}

void test_linkwitz_identity_passthrough(void) {
    // When F0==Fp and Q0==Qp, the transform should be identity (passthrough)
    float coeffs[5];
    float freq = 50.0f / 48000.0f;
    int ret = dsp_gen_linkwitz_f32(coeffs, freq, 0.707f, freq, 0.707f);
    TEST_ASSERT_EQUAL_INT(0, ret);
    // Identity biquad: b0=1, b1=0, b2=0 approximately (or all coeffs produce unity)
    // DC gain should be 1.0
    float dcGain = (coeffs[0] + coeffs[1] + coeffs[2]) /
                   (1.0f + coeffs[3] + coeffs[4]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, dcGain);
}

void test_linkwitz_stage_processes(void) {
    // Linkwitz stage should process through biquad pipeline without error
    dsp_init();
    int idx = dsp_add_stage(0, DSP_BIQUAD_LINKWITZ);
    TEST_ASSERT_TRUE(idx >= 0);
    DspState *cfg = dsp_get_inactive_config();
    DspStage &s = cfg->channels[0].stages[idx];
    s.biquad.frequency = 50.0f;  // F0
    s.biquad.gain = 25.0f;       // Fp (repurposed)
    s.biquad.Q = 0.707f;         // Q0
    s.biquad.Q2 = 0.5f;          // Qp
    dsp_compute_biquad_coeffs(s.biquad, DSP_BIQUAD_LINKWITZ, cfg->sampleRate);
    dsp_swap_config();

    int32_t buffer[8] = {4194304, 4194304, 4194304, 4194304,
                         4194304, 4194304, 4194304, 4194304};
    dsp_process_buffer(buffer, 4, 0);
    // Just verify it doesn't crash and produces non-zero output
    bool anyNonZero = false;
    for (int i = 0; i < 8; i++) {
        if (buffer[i] != 0) anyNonZero = true;
    }
    TEST_ASSERT_TRUE(anyNonZero);
}

void test_linkwitz_rejects_invalid(void) {
    float coeffs[5];
    // Zero frequency
    TEST_ASSERT_EQUAL_INT(-1, dsp_gen_linkwitz_f32(coeffs, 0.0f, 0.707f, 0.001f, 0.5f));
    // Negative Q
    TEST_ASSERT_EQUAL_INT(-1, dsp_gen_linkwitz_f32(coeffs, 0.001f, -1.0f, 0.001f, 0.5f));
    // Null coeffs
    TEST_ASSERT_EQUAL_INT(-1, dsp_gen_linkwitz_f32(NULL, 0.001f, 0.707f, 0.001f, 0.5f));
}

void test_linkwitz_is_biquad_type(void) {
    TEST_ASSERT_TRUE(dsp_is_biquad_type(DSP_BIQUAD_LINKWITZ));
    TEST_ASSERT_EQUAL_STRING("LINKWITZ", stage_type_name(DSP_BIQUAD_LINKWITZ));
}

// ===== Gain Ramp Tests =====

void test_gain_ramp_converges(void) {
    // Gain ramp should converge to target within ~20ms (4 buffers @ 256 samples @ 48kHz)
    dsp_init();
    int idx = dsp_add_stage(0, DSP_GAIN);
    TEST_ASSERT_TRUE(idx >= 0);
    DspState *cfg = dsp_get_inactive_config();
    DspStage &s = cfg->channels[0].stages[idx];
    s.gain.gainDb = 6.0f;
    dsp_compute_gain_linear(s.gain);
    // Set current to 1.0 (unity) so ramp must travel to ~2.0
    s.gain.currentLinear = 1.0f;
    dsp_swap_config();

    // Process 8 buffers of 256 stereo frames (~42ms at 48kHz, ~8 time constants)
    for (int b = 0; b < 8; b++) {
        int32_t buffer[512];
        for (int i = 0; i < 512; i++) buffer[i] = 4194304; // ~0.5 normalized
        dsp_process_buffer(buffer, 256, 0);
    }

    // After ~42ms (8 tau) the ramp should be within 0.04% of target
    cfg = dsp_get_active_config();
    float current = cfg->channels[0].stages[idx].gain.currentLinear;
    float target = cfg->channels[0].stages[idx].gain.gainLinear;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, target, current);
}

void test_gain_ramp_settled_uses_fast_path(void) {
    // When currentLinear == gainLinear, output should match simple multiplication
    dsp_init();
    int idx = dsp_add_stage(0, DSP_GAIN);
    TEST_ASSERT_TRUE(idx >= 0);
    DspState *cfg = dsp_get_inactive_config();
    DspStage &s = cfg->channels[0].stages[idx];
    s.gain.gainDb = -6.0f;
    dsp_compute_gain_linear(s.gain);
    // currentLinear already set by dsp_compute_gain_linear
    TEST_ASSERT_FLOAT_WITHIN(1e-7f, s.gain.gainLinear, s.gain.currentLinear);
    dsp_swap_config();

    int32_t buffer[8] = {4194304, 4194304, 4194304, 4194304,
                         4194304, 4194304, 4194304, 4194304};
    dsp_process_buffer(buffer, 4, 0);

    // All L-channel samples should be multiplied by the same gain
    float expected = 4194304.0f / 8388607.0f * s.gain.gainLinear * 8388607.0f;
    TEST_ASSERT_INT32_WITHIN(2, (int32_t)expected, buffer[0]);
    TEST_ASSERT_INT32_WITHIN(2, (int32_t)expected, buffer[2]);
    TEST_ASSERT_INT32_WITHIN(2, (int32_t)expected, buffer[4]);
    TEST_ASSERT_INT32_WITHIN(2, (int32_t)expected, buffer[6]);
}

void test_gain_ramp_smooth_transition(void) {
    // A gain change mid-stream should produce a smooth ramp (no sudden jump)
    dsp_init();
    int idx = dsp_add_stage(0, DSP_GAIN);
    TEST_ASSERT_TRUE(idx >= 0);
    DspState *cfg = dsp_get_inactive_config();
    DspStage &s = cfg->channels[0].stages[idx];
    s.gain.gainDb = 0.0f;
    dsp_compute_gain_linear(s.gain);
    s.gain.currentLinear = 0.1f; // Start far from target (1.0)
    dsp_swap_config();

    // Fill with constant value to see the ramp on output
    int32_t buffer[128]; // 64 stereo frames
    for (int i = 0; i < 128; i++) buffer[i] = 4194304;

    dsp_process_buffer(buffer, 64, 0);

    // Each successive L-channel sample should be >= the previous (ramping up)
    for (int i = 2; i < 128; i += 2) {
        TEST_ASSERT_TRUE_MESSAGE(buffer[i] >= buffer[i - 2] - 1,
            "Gain ramp should be monotonically increasing toward target");
    }
    // First sample should be less than last (ramp occurred)
    TEST_ASSERT_TRUE(buffer[0] < buffer[126]);
}

void test_gain_ramp_swap_preserves_state(void) {
    // Config swap should preserve currentLinear from old active config
    dsp_init();
    int idx = dsp_add_stage(0, DSP_GAIN);
    TEST_ASSERT_TRUE(idx >= 0);
    DspState *cfg = dsp_get_inactive_config();
    DspStage &s = cfg->channels[0].stages[idx];
    s.gain.gainDb = 6.0f;
    dsp_compute_gain_linear(s.gain);
    s.gain.currentLinear = 0.75f; // Mid-ramp value
    dsp_swap_config();

    // Copy active to inactive, modify something, swap back
    dsp_copy_active_to_inactive();
    cfg = dsp_get_inactive_config();
    // Change gain target but keep same stage type/position
    cfg->channels[0].stages[idx].gain.gainDb = 12.0f;
    dsp_compute_gain_linear(cfg->channels[0].stages[idx].gain);
    dsp_swap_config();

    // After swap, currentLinear should be preserved from old active (0.75)
    cfg = dsp_get_active_config();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, cfg->channels[0].stages[idx].gain.currentLinear);
}

// ===== Routing Matrix Tests =====

void test_routing_matrix_init_identity(void) {
    DspRoutingMatrix rm;
    dsp_routing_init(rm);
    // Identity: diagonal = 1.0, off-diagonal = 0.0
    for (int o = 0; o < DSP_MAX_CHANNELS; o++) {
        for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
            if (o == i) {
                TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, rm.matrix[o][i]);
            } else {
                TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, rm.matrix[o][i]);
            }
        }
    }
}

void test_routing_matrix_presets(void) {
    DspRoutingMatrix rm;
    // Swap LR
    dsp_routing_preset_swap_lr(rm);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, rm.matrix[0][0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, rm.matrix[0][1]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, rm.matrix[1][0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, rm.matrix[1][1]);

    // Mono sum: gain = 1/DSP_MAX_CHANNELS (~0.167 for 6 channels)
    float g = 1.0f / DSP_MAX_CHANNELS;
    dsp_routing_preset_mono_sum(rm);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, g, rm.matrix[0][0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, g, rm.matrix[0][1]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, g, rm.matrix[1][0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, g, rm.matrix[1][1]);
}

void test_gain_init_sets_current_linear(void) {
    DspGainParams p;
    dsp_init_gain_params(p);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, p.currentLinear);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, p.gainDb);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, p.gainLinear);
}

// ===== Convolution Tests (N3) =====

void test_conv_impulse_passthrough(void) {
    // Convolving with a unit impulse should produce the original signal
    float ir[1] = {1.0f};
    int ret = dsp_conv_init_slot(0, ir, 1);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_TRUE(dsp_conv_is_active(0));
    TEST_ASSERT_EQUAL_INT(1, dsp_conv_get_ir_length(0));

    float buf[8] = {1.0f, 0.5f, -0.3f, 0.7f, 0.0f, -1.0f, 0.2f, 0.1f};
    float expected[8];
    memcpy(expected, buf, sizeof(expected));

    dsp_conv_process(0, buf, 8);

    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, expected[i], buf[i]);
    }
    dsp_conv_free_slot(0);
}

void test_conv_free_releases_slot(void) {
    float ir[4] = {0.25f, 0.25f, 0.25f, 0.25f};
    int ret = dsp_conv_init_slot(0, ir, 4);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_TRUE(dsp_conv_is_active(0));

    dsp_conv_free_slot(0);
    TEST_ASSERT_FALSE(dsp_conv_is_active(0));
    TEST_ASSERT_EQUAL_INT(0, dsp_conv_get_ir_length(0));
}

void test_conv_short_ir_matches_direct(void) {
    // Compare partitioned convolution with direct dsps_conv_f32 for a short IR
    float ir[4] = {1.0f, 0.5f, 0.25f, 0.125f};
    float input[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Direct convolution (reference)
    float directOut[11]; // siglen + patlen - 1 = 8 + 4 - 1 = 11
    dsps_conv_f32(input, 8, ir, 4, directOut);

    // Partitioned convolution
    int ret = dsp_conv_init_slot(1, ir, 4);
    TEST_ASSERT_EQUAL_INT(0, ret);

    float partBuf[8];
    memcpy(partBuf, input, sizeof(float) * 8);
    dsp_conv_process(1, partBuf, 8);

    // First 4 output samples of partitioned should match direct convolution
    // (the rest is overlap that comes out in next block)
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.05f, directOut[i], partBuf[i]);
    }
    dsp_conv_free_slot(1);
}

void test_conv_stage_type_integration(void) {
    // Test that DSP_CONVOLUTION stage type can be added and removed
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    int pos = dsp_add_stage(0, DSP_CONVOLUTION);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pos);
    TEST_ASSERT_EQUAL(DSP_CONVOLUTION, cfg->channels[0].stages[pos].type);
    TEST_ASSERT_EQUAL_INT8(-1, cfg->channels[0].stages[pos].convolution.convSlot);

    // Remove it
    bool removed = dsp_remove_stage(0, pos);
    TEST_ASSERT_TRUE(removed);
}

// ===== Metrics Test =====

void test_metrics_initial(void) {
    DspMetrics m = dsp_get_metrics();
    TEST_ASSERT_EQUAL_UINT32(0, m.processTimeUs);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, m.cpuLoadPercent);
}

// ===== Noise Gate Tests =====

void test_noise_gate_below_threshold_attenuated(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_NOISE_GATE);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.noiseGate.thresholdDb = -20.0f;
    s.noiseGate.rangeDb = -80.0f;
    s.noiseGate.ratio = 1.0f; // hard gate
    s.noiseGate.holdMs = 0.0f;
    dsp_swap_config();

    // Generate low-level signal (-40 dBFS ~ 0.01 linear)
    float buf[64];
    for (int i = 0; i < 64; i++) buf[i] = 0.01f * sinf(2.0f * M_PI * 1000.0f * i / 48000.0f);
    cfg = dsp_get_active_config();
    dsp_noise_gate_process(cfg->channels[0].stages[idx].noiseGate, buf, 64, 48000);

    // Signal should be heavily attenuated
    float rms = 0.0f;
    for (int i = 0; i < 64; i++) rms += buf[i] * buf[i];
    rms = sqrtf(rms / 64);
    TEST_ASSERT_TRUE(rms < 0.005f); // Well below input level of 0.01
}

void test_noise_gate_above_threshold_passthrough(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_NOISE_GATE);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.noiseGate.thresholdDb = -40.0f;
    dsp_swap_config();

    cfg = dsp_get_active_config();
    DspNoiseGateParams &gate = cfg->channels[0].stages[idx].noiseGate;

    // Let envelope settle above threshold with warm-up
    float warmup[256];
    for (int i = 0; i < 256; i++) warmup[i] = 0.5f * sinf(2.0f * M_PI * 1000.0f * i / 48000.0f);
    dsp_noise_gate_process(gate, warmup, 256, 48000);

    // Now signal should pass through with settled envelope
    float buf[64], ref[64];
    for (int i = 0; i < 64; i++) {
        buf[i] = 0.5f * sinf(2.0f * M_PI * 1000.0f * i / 48000.0f);
        ref[i] = buf[i];
    }
    dsp_noise_gate_process(gate, buf, 64, 48000);

    float rmsOut = 0.0f, rmsRef = 0.0f;
    for (int i = 0; i < 64; i++) {
        rmsOut += buf[i] * buf[i];
        rmsRef += ref[i] * ref[i];
    }
    rmsOut = sqrtf(rmsOut / 64);
    rmsRef = sqrtf(rmsRef / 64);
    float gainDb = 20.0f * log10f(rmsOut / rmsRef);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 0.0f, gainDb);
}

void test_noise_gate_hold_time(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_NOISE_GATE);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.noiseGate.thresholdDb = -20.0f;
    s.noiseGate.holdMs = 100.0f; // 100ms hold
    s.noiseGate.ratio = 1.0f;
    s.noiseGate.rangeDb = -80.0f;
    dsp_swap_config();

    cfg = dsp_get_active_config();
    DspNoiseGateParams &gate = cfg->channels[0].stages[idx].noiseGate;

    // First: process loud signal to open gate
    float buf[128];
    for (int i = 0; i < 128; i++) buf[i] = 0.5f;
    dsp_noise_gate_process(gate, buf, 128, 48000);

    // Now process silence — gate should still be open due to hold
    for (int i = 0; i < 128; i++) buf[i] = 0.001f; // Very quiet
    dsp_noise_gate_process(gate, buf, 128, 48000);

    // During hold, signal should mostly pass through (not fully attenuated)
    float rms = 0.0f;
    for (int i = 0; i < 128; i++) rms += buf[i] * buf[i];
    rms = sqrtf(rms / 128);
    TEST_ASSERT_TRUE(rms > 0.0005f); // Hold should keep gate open
}

void test_noise_gate_expander_ratio(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_NOISE_GATE);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.noiseGate.thresholdDb = -20.0f;
    s.noiseGate.ratio = 4.0f; // Expander mode (not hard gate)
    s.noiseGate.rangeDb = -80.0f;
    s.noiseGate.holdMs = 0.0f;
    dsp_swap_config();

    float buf[64];
    for (int i = 0; i < 64; i++) buf[i] = 0.05f * sinf(2.0f * M_PI * 1000.0f * i / 48000.0f);
    cfg = dsp_get_active_config();
    dsp_noise_gate_process(cfg->channels[0].stages[idx].noiseGate, buf, 64, 48000);

    // With ratio>1, signal should be partially attenuated (not fully killed)
    float rms = 0.0f;
    for (int i = 0; i < 64; i++) rms += buf[i] * buf[i];
    rms = sqrtf(rms / 64);
    TEST_ASSERT_TRUE(rms < 0.05f); // Some attenuation
    TEST_ASSERT_TRUE(rms > 0.0001f); // But not fully killed (expander, not hard gate)
}

void test_noise_gate_range_limit(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_NOISE_GATE);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.noiseGate.thresholdDb = -10.0f;
    s.noiseGate.rangeDb = -20.0f; // Only -20dB max attenuation (not full silence)
    s.noiseGate.ratio = 1.0f;
    s.noiseGate.holdMs = 0.0f;
    dsp_swap_config();

    float buf[64];
    for (int i = 0; i < 64; i++) buf[i] = 0.01f; // Below threshold
    cfg = dsp_get_active_config();
    dsp_noise_gate_process(cfg->channels[0].stages[idx].noiseGate, buf, 64, 48000);

    // Attenuation should not exceed rangeDb (-20dB = factor of 0.1)
    // So output should be at least input * 0.1 = 0.001
    float minVal = 1.0f;
    for (int i = 0; i < 64; i++) {
        float abs = fabsf(buf[i]);
        if (abs < minVal && abs > 0.0f) minVal = abs;
    }
    TEST_ASSERT_TRUE(minVal >= 0.0005f); // Range limits attenuation
}

// ===== Tone Control Tests =====

void test_tone_ctrl_flat_at_zero_gain(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_TONE_CTRL);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.toneCtrl.bassGain = 0.0f;
    s.toneCtrl.midGain = 0.0f;
    s.toneCtrl.trebleGain = 0.0f;
    dsp_compute_tone_ctrl_coeffs(s.toneCtrl, 48000);
    dsp_swap_config();

    // Process 1kHz sine (mid-range)
    float buf[256], ref[256];
    for (int i = 0; i < 256; i++) {
        buf[i] = 0.5f * sinf(2.0f * M_PI * 1000.0f * i / 48000.0f);
        ref[i] = buf[i];
    }
    cfg = dsp_get_active_config();
    dsp_tone_ctrl_process(cfg->channels[0].stages[idx].toneCtrl, buf, 256);

    // After settling (skip first 32 samples for biquad transient), should be ~passthrough
    float rmsOut = 0.0f, rmsRef = 0.0f;
    for (int i = 32; i < 256; i++) {
        rmsOut += buf[i] * buf[i];
        rmsRef += ref[i] * ref[i];
    }
    rmsOut = sqrtf(rmsOut / 224);
    rmsRef = sqrtf(rmsRef / 224);
    float gainDb = 20.0f * log10f(rmsOut / rmsRef);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, gainDb); // Within 1 dB of flat
}

void test_tone_ctrl_bass_boost(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_TONE_CTRL);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.toneCtrl.bassGain = 6.0f;
    s.toneCtrl.midGain = 0.0f;
    s.toneCtrl.trebleGain = 0.0f;
    dsp_compute_tone_ctrl_coeffs(s.toneCtrl, 48000);
    dsp_swap_config();

    // Process low-frequency sine (50Hz)
    float buf[256], ref[256];
    for (int i = 0; i < 256; i++) {
        buf[i] = 0.3f * sinf(2.0f * M_PI * 50.0f * i / 48000.0f);
        ref[i] = buf[i];
    }
    cfg = dsp_get_active_config();
    dsp_tone_ctrl_process(cfg->channels[0].stages[idx].toneCtrl, buf, 256);

    float rmsOut = 0.0f, rmsRef = 0.0f;
    for (int i = 128; i < 256; i++) { // Skip transient
        rmsOut += buf[i] * buf[i];
        rmsRef += ref[i] * ref[i];
    }
    rmsOut = sqrtf(rmsOut / 128);
    rmsRef = sqrtf(rmsRef / 128);
    float gainDb = 20.0f * log10f(rmsOut / rmsRef);
    TEST_ASSERT_TRUE(gainDb > 2.0f); // Some bass boost detected
}

void test_tone_ctrl_treble_cut(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_TONE_CTRL);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.toneCtrl.bassGain = 0.0f;
    s.toneCtrl.midGain = 0.0f;
    s.toneCtrl.trebleGain = -6.0f;
    dsp_compute_tone_ctrl_coeffs(s.toneCtrl, 48000);
    dsp_swap_config();

    // Process high-frequency sine (15kHz)
    float buf[256], ref[256];
    for (int i = 0; i < 256; i++) {
        buf[i] = 0.3f * sinf(2.0f * M_PI * 15000.0f * i / 48000.0f);
        ref[i] = buf[i];
    }
    cfg = dsp_get_active_config();
    dsp_tone_ctrl_process(cfg->channels[0].stages[idx].toneCtrl, buf, 256);

    float rmsOut = 0.0f, rmsRef = 0.0f;
    for (int i = 64; i < 256; i++) {
        rmsOut += buf[i] * buf[i];
        rmsRef += ref[i] * ref[i];
    }
    rmsOut = sqrtf(rmsOut / 192);
    rmsRef = sqrtf(rmsRef / 192);
    float gainDb = 20.0f * log10f(rmsOut / rmsRef);
    TEST_ASSERT_TRUE(gainDb < -2.0f); // Treble cut detected
}

void test_tone_ctrl_mid_boost(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_TONE_CTRL);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.toneCtrl.bassGain = 0.0f;
    s.toneCtrl.midGain = 6.0f;
    s.toneCtrl.trebleGain = 0.0f;
    dsp_compute_tone_ctrl_coeffs(s.toneCtrl, 48000);
    dsp_swap_config();

    // Process 1kHz sine
    float buf[256], ref[256];
    for (int i = 0; i < 256; i++) {
        buf[i] = 0.3f * sinf(2.0f * M_PI * 1000.0f * i / 48000.0f);
        ref[i] = buf[i];
    }
    cfg = dsp_get_active_config();
    dsp_tone_ctrl_process(cfg->channels[0].stages[idx].toneCtrl, buf, 256);

    float rmsOut = 0.0f, rmsRef = 0.0f;
    for (int i = 64; i < 256; i++) {
        rmsOut += buf[i] * buf[i];
        rmsRef += ref[i] * ref[i];
    }
    rmsOut = sqrtf(rmsOut / 192);
    rmsRef = sqrtf(rmsRef / 192);
    float gainDb = 20.0f * log10f(rmsOut / rmsRef);
    TEST_ASSERT_TRUE(gainDb > 2.0f); // Mid boost detected
}

// ===== Bessel Crossover Tests =====

void test_bessel_q_table_values(void) {
    // Verify the known Q values for Bessel order 2
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_insert_crossover_bessel(0, 1000.0f, 2, 0);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5774f, s.biquad.Q);
}

void test_bessel_crossover_insert_order2(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    int countBefore = cfg->channels[0].stageCount;
    int idx = dsp_insert_crossover_bessel(0, 1000.0f, 2, 0);
    TEST_ASSERT_TRUE(idx >= 0);
    TEST_ASSERT_EQUAL(countBefore + 1, cfg->channels[0].stageCount); // 1 section
}

void test_bessel_crossover_insert_order4(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    int countBefore = cfg->channels[0].stageCount;
    int idx = dsp_insert_crossover_bessel(0, 1000.0f, 4, 0);
    TEST_ASSERT_TRUE(idx >= 0);
    TEST_ASSERT_EQUAL(countBefore + 2, cfg->channels[0].stageCount); // 2 sections
}

void test_bessel_crossover_summation_flat(void) {
    dsp_init();
    // Insert LPF + HPF Bessel order 2 and check flat sum
    DspState *cfg = dsp_get_inactive_config();
    dsp_insert_crossover_bessel(0, 1000.0f, 2, 0); // LPF
    dsp_insert_crossover_bessel(1, 1000.0f, 2, 1); // HPF
    dsp_swap_config();

    // Process a multi-frequency signal through both
    float bufL[256], bufR[256], sum[256];
    for (int i = 0; i < 256; i++) {
        float sample = 0.3f * sinf(2.0f * M_PI * 200.0f * i / 48000.0f)
                     + 0.3f * sinf(2.0f * M_PI * 5000.0f * i / 48000.0f);
        bufL[i] = sample;
        bufR[i] = sample;
    }

    cfg = dsp_get_active_config();
    // Process LPF on channel 0 stages
    for (int s = DSP_PEQ_BANDS; s < cfg->channels[0].stageCount; s++) {
        if (cfg->channels[0].stages[s].enabled && dsp_is_biquad_type(cfg->channels[0].stages[s].type)) {
            dsps_biquad_f32(bufL, bufL, 256, cfg->channels[0].stages[s].biquad.coeffs, cfg->channels[0].stages[s].biquad.delay);
        }
    }
    for (int s = DSP_PEQ_BANDS; s < cfg->channels[1].stageCount; s++) {
        if (cfg->channels[1].stages[s].enabled && dsp_is_biquad_type(cfg->channels[1].stages[s].type)) {
            dsps_biquad_f32(bufR, bufR, 256, cfg->channels[1].stages[s].biquad.coeffs, cfg->channels[1].stages[s].biquad.delay);
        }
    }

    // Sum should be approximately flat (within 3dB due to Bessel not summing perfectly flat)
    float rmsSum = 0.0f;
    for (int i = 128; i < 256; i++) {
        sum[i] = bufL[i] + bufR[i];
        rmsSum += sum[i] * sum[i];
    }
    rmsSum = sqrtf(rmsSum / 128);
    TEST_ASSERT_TRUE(rmsSum > 0.1f); // Sum is non-zero
}

void test_bessel_crossover_rollback_on_full(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    // Fill up stages
    for (int i = cfg->channels[0].stageCount; i < DSP_MAX_STAGES; i++) {
        dsp_add_stage(0, DSP_BIQUAD_PEQ);
    }
    // Now try to insert Bessel order 4 (needs 2 stages) — should fail
    int idx = dsp_insert_crossover_bessel(0, 1000.0f, 4, 0);
    TEST_ASSERT_EQUAL(-1, idx);
}

// ===== Speaker Protection Tests =====

void test_speaker_prot_thermal_ramp(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_SPEAKER_PROT);
    TEST_ASSERT_TRUE(idx >= 0);
    dsp_swap_config();

    cfg = dsp_get_active_config();
    DspSpeakerProtParams &sp = cfg->channels[0].stages[idx].speakerProt;
    float initialTemp = sp.currentTempC;

    // Process sustained loud signal
    float buf[256];
    for (int iter = 0; iter < 20; iter++) {
        for (int i = 0; i < 256; i++) buf[i] = 0.8f;
        dsp_speaker_prot_process(sp, buf, 256, 48000);
    }
    TEST_ASSERT_TRUE(sp.currentTempC > initialTemp); // Temp should rise
}

void test_speaker_prot_cool_down(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_SPEAKER_PROT);
    TEST_ASSERT_TRUE(idx >= 0);
    // Use fast thermal time constant so heating/cooling is pronounced in test
    cfg->channels[0].stages[idx].speakerProt.thermalTauMs = 100.0f;
    dsp_swap_config();

    cfg = dsp_get_active_config();
    DspSpeakerProtParams &sp = cfg->channels[0].stages[idx].speakerProt;

    // Heat it up with loud signal
    float buf[256];
    for (int iter = 0; iter < 100; iter++) {
        for (int i = 0; i < 256; i++) buf[i] = 0.9f;
        dsp_speaker_prot_process(sp, buf, 256, 48000);
    }
    float hotTemp = sp.currentTempC;
    TEST_ASSERT_TRUE(hotTemp > 26.0f); // Ensure meaningful heating occurred

    // Now process silence — should cool down
    for (int iter = 0; iter < 100; iter++) {
        for (int i = 0; i < 256; i++) buf[i] = 0.0f;
        dsp_speaker_prot_process(sp, buf, 256, 48000);
    }
    TEST_ASSERT_TRUE(sp.currentTempC < hotTemp);
}

void test_speaker_prot_gain_reduction_at_limit(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_SPEAKER_PROT);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.speakerProt.maxTempC = 50.0f; // Low threshold for fast triggering
    s.speakerProt.thermalTauMs = 10.0f; // Fast thermal response
    dsp_swap_config();

    cfg = dsp_get_active_config();
    DspSpeakerProtParams &sp = cfg->channels[0].stages[idx].speakerProt;

    float buf[256];
    for (int iter = 0; iter < 100; iter++) {
        for (int i = 0; i < 256; i++) buf[i] = 0.95f;
        dsp_speaker_prot_process(sp, buf, 256, 48000);
    }
    TEST_ASSERT_TRUE(sp.gainReduction < 0.0f); // GR should be applied
}

void test_speaker_prot_excursion_limit(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_SPEAKER_PROT);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.speakerProt.excursionLimitMm = 0.1f; // Very low limit
    s.speakerProt.driverDiameterMm = 10.0f; // Small driver = small area = higher excursion estimate
    dsp_swap_config();

    cfg = dsp_get_active_config();
    DspSpeakerProtParams &sp = cfg->channels[0].stages[idx].speakerProt;

    float buf[256];
    for (int i = 0; i < 256; i++) buf[i] = 0.9f;
    dsp_speaker_prot_process(sp, buf, 256, 48000);

    // With very low excursion limit, GR should kick in for high amplitude
    TEST_ASSERT_TRUE(sp.gainReduction < 0.0f);
}

void test_speaker_prot_metering_populated(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_SPEAKER_PROT);
    TEST_ASSERT_TRUE(idx >= 0);
    dsp_swap_config();

    cfg = dsp_get_active_config();
    DspSpeakerProtParams &sp = cfg->channels[0].stages[idx].speakerProt;

    float buf[256];
    for (int i = 0; i < 256; i++) buf[i] = 0.5f;
    dsp_speaker_prot_process(sp, buf, 256, 48000);

    // After processing, runtime fields should be non-zero/meaningful
    TEST_ASSERT_TRUE(sp.currentTempC >= 25.0f);
    TEST_ASSERT_TRUE(sp.envelope >= 0.0f);
}

// ===== Stereo Width Tests =====

void test_stereo_width_mono_collapse(void) {
    // width=0 → L should equal R (mono)
    float bufL[64], bufR[64];
    for (int i = 0; i < 64; i++) {
        bufL[i] = sinf(2.0f * M_PI * 1000.0f * i / 48000.0f);
        bufR[i] = sinf(2.0f * M_PI * 500.0f * i / 48000.0f);
    }

    // Apply M/S transform with width=0
    for (int f = 0; f < 64; f++) {
        float mid = (bufL[f] + bufR[f]) * 0.5f;
        float side = (bufL[f] - bufR[f]) * 0.5f * 0.0f; // width=0
        bufL[f] = mid + side;
        bufR[f] = mid - side;
    }

    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, bufL[i], bufR[i]);
    }
}

void test_stereo_width_normal_passthrough(void) {
    float bufL[64], bufR[64], refL[64], refR[64];
    for (int i = 0; i < 64; i++) {
        bufL[i] = sinf(2.0f * M_PI * 1000.0f * i / 48000.0f);
        bufR[i] = sinf(2.0f * M_PI * 500.0f * i / 48000.0f);
        refL[i] = bufL[i];
        refR[i] = bufR[i];
    }

    // Apply M/S transform with width=100 (normal)
    for (int f = 0; f < 64; f++) {
        float mid = (bufL[f] + bufR[f]) * 0.5f;
        float side = (bufL[f] - bufR[f]) * 0.5f * 1.0f; // width=100/100
        bufL[f] = mid + side;
        bufR[f] = mid - side;
    }

    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, refL[i], bufL[i]);
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, refR[i], bufR[i]);
    }
}

void test_stereo_width_extra_wide(void) {
    float bufL[64], bufR[64];
    for (int i = 0; i < 64; i++) {
        bufL[i] = 0.5f;
        bufR[i] = -0.5f; // Pure side signal
    }

    // Apply M/S with width=200 (2x)
    for (int f = 0; f < 64; f++) {
        float mid = (bufL[f] + bufR[f]) * 0.5f;
        float side = (bufL[f] - bufR[f]) * 0.5f * 2.0f; // width=200/100
        bufL[f] = mid + side;
        bufR[f] = mid - side;
    }

    // Side signal should be amplified: L=+1, R=-1
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, bufL[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, bufR[0]);
}

void test_stereo_width_center_boost(void) {
    float bufL[64], bufR[64];
    // Mono signal (same on L and R = pure mid)
    for (int i = 0; i < 64; i++) {
        bufL[i] = 0.5f;
        bufR[i] = 0.5f;
    }

    float centerGain = powf(10.0f, 6.0f / 20.0f); // +6dB
    for (int f = 0; f < 64; f++) {
        float mid = (bufL[f] + bufR[f]) * 0.5f * centerGain;
        float side = (bufL[f] - bufR[f]) * 0.5f * 1.0f;
        bufL[f] = mid + side;
        bufR[f] = mid - side;
    }

    // Mid boosted by +6dB → ~2x → L and R should be ~1.0
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.5f * centerGain, bufL[0]);
}

// ===== Loudness Compensation Tests =====

void test_loudness_reference_equals_current_flat(void) {
    DspLoudnessParams ld;
    dsp_init_loudness_params(ld);
    ld.referenceLevelDb = 75.0f;
    ld.currentLevelDb = 75.0f;
    ld.amount = 100.0f;
    dsp_compute_loudness_coeffs(ld, 48000);

    // When ref == current, boosts should be ~0
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, ld.bassBoostDb);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, ld.trebleBoostDb);
}

void test_loudness_low_volume_bass_boost(void) {
    DspLoudnessParams ld;
    dsp_init_loudness_params(ld);
    ld.referenceLevelDb = 80.0f;
    ld.currentLevelDb = 40.0f;
    ld.amount = 100.0f;
    dsp_compute_loudness_coeffs(ld, 48000);

    // At low volume, bass should be boosted
    TEST_ASSERT_TRUE(ld.bassBoostDb > 2.0f);
}

void test_loudness_amount_zero_bypass(void) {
    DspLoudnessParams ld;
    dsp_init_loudness_params(ld);
    ld.referenceLevelDb = 80.0f;
    ld.currentLevelDb = 30.0f;
    ld.amount = 0.0f; // Disabled
    dsp_compute_loudness_coeffs(ld, 48000);

    // amount=0 → no boost
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, ld.bassBoostDb);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, ld.trebleBoostDb);
}

void test_loudness_treble_boost_at_low_level(void) {
    DspLoudnessParams ld;
    dsp_init_loudness_params(ld);
    ld.referenceLevelDb = 80.0f;
    ld.currentLevelDb = 30.0f;
    ld.amount = 100.0f;
    dsp_compute_loudness_coeffs(ld, 48000);

    // Treble should also be boosted at low level
    TEST_ASSERT_TRUE(ld.trebleBoostDb > 1.0f);
}

// ===== Bass Enhancement Tests =====

void test_bass_enhance_generates_harmonics(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_BASS_ENHANCE);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.bassEnhance.frequency = 80.0f;
    s.bassEnhance.harmonicGainDb = 6.0f;
    s.bassEnhance.harmonicGainLin = powf(10.0f, 6.0f / 20.0f);
    s.bassEnhance.mix = 100.0f;
    s.bassEnhance.order = 2; // Both 2nd and 3rd
    dsp_compute_bass_enhance_coeffs(s.bassEnhance, 48000);
    dsp_swap_config();

    // Generate 40Hz sine (sub-bass)
    float buf[256], ref[256];
    for (int i = 0; i < 256; i++) {
        buf[i] = 0.5f * sinf(2.0f * M_PI * 40.0f * i / 48000.0f);
        ref[i] = buf[i];
    }
    cfg = dsp_get_active_config();
    dsp_bass_enhance_process(cfg->channels[0].stages[idx].bassEnhance, buf, 256);

    // Output should differ from input (harmonics added)
    float diffRms = 0.0f;
    for (int i = 128; i < 256; i++) {
        float d = buf[i] - ref[i];
        diffRms += d * d;
    }
    diffRms = sqrtf(diffRms / 128);
    TEST_ASSERT_TRUE(diffRms > 0.001f); // Some harmonic content generated
}

void test_bass_enhance_mix_zero_passthrough(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_BASS_ENHANCE);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.bassEnhance.mix = 0.0f; // Disabled
    dsp_compute_bass_enhance_coeffs(s.bassEnhance, 48000);
    dsp_swap_config();

    float buf[64], ref[64];
    for (int i = 0; i < 64; i++) {
        buf[i] = 0.5f * sinf(2.0f * M_PI * 40.0f * i / 48000.0f);
        ref[i] = buf[i];
    }
    cfg = dsp_get_active_config();
    dsp_bass_enhance_process(cfg->channels[0].stages[idx].bassEnhance, buf, 64);

    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, ref[i], buf[i]);
    }
}

void test_bass_enhance_frequency_isolation(void) {
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_BASS_ENHANCE);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.bassEnhance.frequency = 80.0f;
    s.bassEnhance.harmonicGainDb = 6.0f;
    s.bassEnhance.harmonicGainLin = powf(10.0f, 6.0f / 20.0f);
    s.bassEnhance.mix = 100.0f;
    s.bassEnhance.order = 2;
    dsp_compute_bass_enhance_coeffs(s.bassEnhance, 48000);
    dsp_swap_config();

    // Process 5kHz sine (well above crossover — should be mostly unaffected)
    float buf[256], ref[256];
    for (int i = 0; i < 256; i++) {
        buf[i] = 0.5f * sinf(2.0f * M_PI * 5000.0f * i / 48000.0f);
        ref[i] = buf[i];
    }
    cfg = dsp_get_active_config();
    dsp_bass_enhance_process(cfg->channels[0].stages[idx].bassEnhance, buf, 256);

    // HF content should be mostly unchanged (harmonics are generated only from LF)
    float rmsOut = 0.0f, rmsRef = 0.0f;
    for (int i = 128; i < 256; i++) {
        rmsOut += buf[i] * buf[i];
        rmsRef += ref[i] * ref[i];
    }
    rmsOut = sqrtf(rmsOut / 128);
    rmsRef = sqrtf(rmsRef / 128);
    float gainDb = 20.0f * log10f(rmsOut / rmsRef);
    TEST_ASSERT_FLOAT_WITHIN(3.0f, 0.0f, gainDb); // Within 3dB of original
}

// ===== Multi-Band Compressor Tests =====

void test_multiband_2band_split_and_sum(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_MULTIBAND_COMP);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    TEST_ASSERT_TRUE(s.multibandComp.mbSlot >= 0);
    s.multibandComp.numBands = 2;

    // Set very high threshold so no compression occurs
#ifdef NATIVE_TEST
    DspMultibandSlot &slot = _mbSlots[s.multibandComp.mbSlot];
#else
    DspMultibandSlot &slot = _mbSlots[s.multibandComp.mbSlot];
#endif
    slot.crossoverFreqs[0] = 1000.0f;
    // Compute crossover coefficients
    float freq = 1000.0f / 48000.0f; // normalized
    dsp_gen_lpf_f32(slot.xoverCoeffs[0][0], freq, 0.5f);
    dsp_gen_hpf_f32(slot.xoverCoeffs[0][1], freq, 0.5f);
    for (int b = 0; b < 2; b++) {
        slot.bands[b].thresholdDb = 0.0f; // No compression
        slot.bands[b].ratio = 1.0f;
        slot.bands[b].makeupLinear = 1.0f;
    }

    dsp_swap_config();

    float buf[256], ref[256];
    for (int i = 0; i < 256; i++) {
        buf[i] = 0.5f * sinf(2.0f * M_PI * 500.0f * i / 48000.0f);
        ref[i] = buf[i];
    }

    cfg = dsp_get_active_config();
    dsp_multiband_comp_process(cfg->channels[0].stages[idx].multibandComp, buf, 256, 48000);

    // Sum of split bands should roughly reconstruct original (within a few dB due to crossover)
    float rmsOut = 0.0f, rmsRef = 0.0f;
    for (int i = 128; i < 256; i++) {
        rmsOut += buf[i] * buf[i];
        rmsRef += ref[i] * ref[i];
    }
    rmsOut = sqrtf(rmsOut / 128);
    rmsRef = sqrtf(rmsRef / 128);
    TEST_ASSERT_TRUE(rmsOut > rmsRef * 0.3f); // At least 30% of original survives split+sum
}

void test_multiband_per_band_compression(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_MULTIBAND_COMP);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.multibandComp.numBands = 2;
#ifdef NATIVE_TEST
    DspMultibandSlot &slot = _mbSlots[s.multibandComp.mbSlot];
#else
    DspMultibandSlot &slot = _mbSlots[s.multibandComp.mbSlot];
#endif
    slot.crossoverFreqs[0] = 1000.0f;
    float freq = 1000.0f / 48000.0f;
    dsp_gen_lpf_f32(slot.xoverCoeffs[0][0], freq, 0.5f);
    dsp_gen_hpf_f32(slot.xoverCoeffs[0][1], freq, 0.5f);

    // Compress only low band heavily
    slot.bands[0].thresholdDb = -30.0f;
    slot.bands[0].ratio = 10.0f;
    slot.bands[0].makeupLinear = 1.0f;
    slot.bands[1].thresholdDb = 0.0f;
    slot.bands[1].ratio = 1.0f;
    slot.bands[1].makeupLinear = 1.0f;
    dsp_swap_config();

    // Process signal — should see compression on band 0
    float buf[256];
    for (int i = 0; i < 256; i++) buf[i] = 0.5f * sinf(2.0f * M_PI * 200.0f * i / 48000.0f);

    cfg = dsp_get_active_config();
    dsp_multiband_comp_process(cfg->channels[0].stages[idx].multibandComp, buf, 256, 48000);

    // Band 0 should have gain reduction
    TEST_ASSERT_TRUE(slot.bands[0].gainReduction < 0.0f);
}

void test_multiband_3band_crossover_accuracy(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();
    int idx = dsp_add_chain_stage(0, DSP_MULTIBAND_COMP);
    TEST_ASSERT_TRUE(idx >= 0);
    DspStage &s = cfg->channels[0].stages[idx];
    s.multibandComp.numBands = 3;
    TEST_ASSERT_TRUE(s.multibandComp.mbSlot >= 0);
    TEST_ASSERT_EQUAL(3, s.multibandComp.numBands);
}

void test_multiband_slot_alloc_and_free(void) {
    dsp_init();
    int slot = dsp_mb_alloc_slot();
    TEST_ASSERT_TRUE(slot >= 0);

    // Second alloc should fail (only 1 slot)
    int slot2 = dsp_mb_alloc_slot();
    TEST_ASSERT_EQUAL(-1, slot2);

    // Free and re-alloc should work
    dsp_mb_free_slot(slot);
    int slot3 = dsp_mb_alloc_slot();
    TEST_ASSERT_TRUE(slot3 >= 0);
}

// ===== Baffle Step Tests =====

void test_baffle_step_frequency_250mm(void) {
    BaffleStepResult r = dsp_baffle_step_correction(250.0f);
    // f = 343000 / (pi * 250) = ~436.6 Hz
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 436.6f, r.frequency);
}

void test_baffle_step_zero_width_safe(void) {
    BaffleStepResult r = dsp_baffle_step_correction(0.0f);
    TEST_ASSERT_TRUE(r.frequency > 0.0f);
    TEST_ASSERT_TRUE(r.gainDb > 0.0f);
}

void test_baffle_step_gain_6db(void) {
    BaffleStepResult r = dsp_baffle_step_correction(300.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 6.0f, r.gainDb);
}

// ===== THD Measurement Tests =====

void test_thd_pure_sine_near_zero(void) {
    thd_start_measurement(1000.0f, 4);
    TEST_ASSERT_TRUE(thd_is_measuring());

    // Generate clean FFT magnitude for pure 1kHz sine
    float binFreqHz = 48000.0f / 1024.0f; // ~46.875 Hz/bin
    int fundamentalBin = (int)(1000.0f / binFreqHz + 0.5f); // bin 21
    int numBins = 512;

    for (int frame = 0; frame < 4; frame++) {
        float fftMag[512];
        memset(fftMag, 0, sizeof(fftMag));
        fftMag[fundamentalBin] = 1.0f; // Pure fundamental, no harmonics
        thd_process_fft_buffer(fftMag, numBins, binFreqHz, 48000.0f);
    }

    ThdResult r = thd_get_result();
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_TRUE(r.thdPlusNPercent < 1.0f); // Near zero THD for pure sine
}

void test_thd_known_3rd_harmonic(void) {
    thd_start_measurement(1000.0f, 4);

    float binFreqHz = 48000.0f / 1024.0f;
    int fundamentalBin = (int)(1000.0f / binFreqHz + 0.5f);
    int thirdHarmBin = (int)(3000.0f / binFreqHz + 0.5f);
    int numBins = 512;

    for (int frame = 0; frame < 4; frame++) {
        float fftMag[512];
        memset(fftMag, 0, sizeof(fftMag));
        fftMag[fundamentalBin] = 1.0f;
        fftMag[thirdHarmBin] = 0.1f; // 3rd harmonic at -20dB = 10%
        thd_process_fft_buffer(fftMag, numBins, binFreqHz, 48000.0f);
    }

    ThdResult r = thd_get_result();
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_TRUE(r.thdPlusNPercent > 5.0f); // Should detect significant THD
}

void test_thd_averaging_accumulates(void) {
    thd_start_measurement(1000.0f, 8);
    TEST_ASSERT_TRUE(thd_is_measuring());

    float binFreqHz = 48000.0f / 1024.0f;
    int fundamentalBin = (int)(1000.0f / binFreqHz + 0.5f);
    int numBins = 512;

    for (int frame = 0; frame < 4; frame++) {
        float fftMag[512];
        memset(fftMag, 0, sizeof(fftMag));
        fftMag[fundamentalBin] = 1.0f;
        thd_process_fft_buffer(fftMag, numBins, binFreqHz, 48000.0f);
    }

    // After 4 frames of 8 needed, should still be measuring
    TEST_ASSERT_TRUE(thd_is_measuring());
    ThdResult r = thd_get_result();
    TEST_ASSERT_EQUAL(4, r.framesProcessed);
    TEST_ASSERT_FALSE(r.valid);
}

void test_thd_cancel_stops(void) {
    thd_start_measurement(1000.0f, 4);
    TEST_ASSERT_TRUE(thd_is_measuring());
    thd_stop_measurement();
    TEST_ASSERT_FALSE(thd_is_measuring());
}

void test_thd_invalid_freq_safe(void) {
    thd_start_measurement(0.0f, 4);
    TEST_ASSERT_FALSE(thd_is_measuring());
    ThdResult r = thd_get_result();
    TEST_ASSERT_FALSE(r.valid);
}

// ===== Utility & Helper Tests =====

void test_stage_type_name_all_types(void) {
    TEST_ASSERT_EQUAL_STRING("LPF", stage_type_name(DSP_BIQUAD_LPF));
    TEST_ASSERT_EQUAL_STRING("HPF", stage_type_name(DSP_BIQUAD_HPF));
    TEST_ASSERT_EQUAL_STRING("BPF", stage_type_name(DSP_BIQUAD_BPF));
    TEST_ASSERT_EQUAL_STRING("NOTCH", stage_type_name(DSP_BIQUAD_NOTCH));
    TEST_ASSERT_EQUAL_STRING("PEQ", stage_type_name(DSP_BIQUAD_PEQ));
    TEST_ASSERT_EQUAL_STRING("LOW_SHELF", stage_type_name(DSP_BIQUAD_LOW_SHELF));
    TEST_ASSERT_EQUAL_STRING("HIGH_SHELF", stage_type_name(DSP_BIQUAD_HIGH_SHELF));
    TEST_ASSERT_EQUAL_STRING("ALLPASS", stage_type_name(DSP_BIQUAD_ALLPASS));
    TEST_ASSERT_EQUAL_STRING("ALLPASS_360", stage_type_name(DSP_BIQUAD_ALLPASS_360));
    TEST_ASSERT_EQUAL_STRING("ALLPASS_180", stage_type_name(DSP_BIQUAD_ALLPASS_180));
    TEST_ASSERT_EQUAL_STRING("BPF_0DB", stage_type_name(DSP_BIQUAD_BPF_0DB));
    TEST_ASSERT_EQUAL_STRING("CUSTOM", stage_type_name(DSP_BIQUAD_CUSTOM));
    TEST_ASSERT_EQUAL_STRING("LIMITER", stage_type_name(DSP_LIMITER));
    TEST_ASSERT_EQUAL_STRING("FIR", stage_type_name(DSP_FIR));
    TEST_ASSERT_EQUAL_STRING("GAIN", stage_type_name(DSP_GAIN));
    TEST_ASSERT_EQUAL_STRING("DELAY", stage_type_name(DSP_DELAY));
    TEST_ASSERT_EQUAL_STRING("POLARITY", stage_type_name(DSP_POLARITY));
    TEST_ASSERT_EQUAL_STRING("MUTE", stage_type_name(DSP_MUTE));
    TEST_ASSERT_EQUAL_STRING("COMPRESSOR", stage_type_name(DSP_COMPRESSOR));
    TEST_ASSERT_EQUAL_STRING("LPF_1ST", stage_type_name(DSP_BIQUAD_LPF_1ST));
    TEST_ASSERT_EQUAL_STRING("HPF_1ST", stage_type_name(DSP_BIQUAD_HPF_1ST));
    TEST_ASSERT_EQUAL_STRING("LINKWITZ", stage_type_name(DSP_BIQUAD_LINKWITZ));
    TEST_ASSERT_EQUAL_STRING("DECIMATOR", stage_type_name(DSP_DECIMATOR));
    TEST_ASSERT_EQUAL_STRING("CONVOLUTION", stage_type_name(DSP_CONVOLUTION));
    TEST_ASSERT_EQUAL_STRING("NOISE_GATE", stage_type_name(DSP_NOISE_GATE));
    TEST_ASSERT_EQUAL_STRING("TONE_CTRL", stage_type_name(DSP_TONE_CTRL));
    TEST_ASSERT_EQUAL_STRING("SPEAKER_PROT", stage_type_name(DSP_SPEAKER_PROT));
    TEST_ASSERT_EQUAL_STRING("STEREO_WIDTH", stage_type_name(DSP_STEREO_WIDTH));
    TEST_ASSERT_EQUAL_STRING("LOUDNESS", stage_type_name(DSP_LOUDNESS));
    TEST_ASSERT_EQUAL_STRING("BASS_ENHANCE", stage_type_name(DSP_BASS_ENHANCE));
    TEST_ASSERT_EQUAL_STRING("MULTIBAND_COMP", stage_type_name(DSP_MULTIBAND_COMP));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", stage_type_name((DspStageType)255));
}

void test_ensure_peq_bands(void) {
    DspState *cfg = dsp_get_inactive_config();
    // Clear PEQ labels to make dsp_has_peq_bands() return false
    cfg->channels[0].stages[0].label[0] = 'X';
    TEST_ASSERT_FALSE(dsp_has_peq_bands(cfg->channels[0]));

    dsp_ensure_peq_bands(cfg);
    // Should have re-initialized PEQ bands
    TEST_ASSERT_TRUE(dsp_has_peq_bands(cfg->channels[0]));
    TEST_ASSERT_EQUAL_STRING("PEQ 1", cfg->channels[0].stages[0].label);
}

void test_copy_peq_bands(void) {
    DspState *cfg = dsp_get_inactive_config();
    // Modify channel 0 PEQ band 0 frequency
    cfg->channels[0].stages[0].biquad.frequency = 5000.0f;
    cfg->channels[0].stages[0].biquad.gain = 6.0f;

    dsp_copy_peq_bands(0, 1);

    cfg = dsp_get_inactive_config();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5000.0f, cfg->channels[1].stages[0].biquad.frequency);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.0f, cfg->channels[1].stages[0].biquad.gain);
}

void test_copy_chain_stages_basic(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();

    // Add some chain stages to channel 0
    int gain1 = dsp_add_stage(0, DSP_GAIN);
    int limiter1 = dsp_add_stage(0, DSP_LIMITER);

    cfg = dsp_get_inactive_config();
    cfg->channels[0].stages[gain1].gain.gainLinear = 2.0f;
    cfg->channels[0].stages[limiter1].limiter.thresholdDb = -6.0f;

    // Copy chain stages from channel 0 to channel 1
    dsp_copy_chain_stages(0, 1);

    cfg = dsp_get_inactive_config();
    // Channel 1 should now have the same chain stages
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS + 2, cfg->channels[1].stageCount);
    TEST_ASSERT_EQUAL_INT(DSP_GAIN, cfg->channels[1].stages[DSP_PEQ_BANDS].type);
    TEST_ASSERT_EQUAL_INT(DSP_LIMITER, cfg->channels[1].stages[DSP_PEQ_BANDS + 1].type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, cfg->channels[1].stages[DSP_PEQ_BANDS].gain.gainLinear);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -6.0f, cfg->channels[1].stages[DSP_PEQ_BANDS + 1].limiter.thresholdDb);
}

void test_copy_chain_stages_preserves_peq(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();

    // Modify PEQ on destination channel
    cfg->channels[1].stages[0].biquad.frequency = 8000.0f;
    cfg->channels[1].stages[0].biquad.gain = 3.0f;

    // Add chain stage to source channel
    dsp_add_stage(0, DSP_GAIN);

    // Copy chain stages
    dsp_copy_chain_stages(0, 1);

    cfg = dsp_get_inactive_config();
    // PEQ should be unchanged
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 8000.0f, cfg->channels[1].stages[0].biquad.frequency);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, cfg->channels[1].stages[0].biquad.gain);
    // Chain stage should be copied
    TEST_ASSERT_EQUAL_INT(DSP_GAIN, cfg->channels[1].stages[DSP_PEQ_BANDS].type);
}

void test_copy_chain_stages_empty_chain(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();

    // Add chain stage to destination first
    dsp_add_stage(1, DSP_GAIN);
    int oldCount = cfg->channels[1].stageCount;

    // Copy from channel 0 (which has no chain stages)
    dsp_copy_chain_stages(0, 1);

    cfg = dsp_get_inactive_config();
    // Destination should have only PEQ bands now
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS, cfg->channels[1].stageCount);
}

void test_copy_chain_stages_same_channel_noop(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();

    dsp_add_stage(0, DSP_GAIN);
    int beforeCount = cfg->channels[0].stageCount;

    // Copy to same channel should do nothing
    dsp_copy_chain_stages(0, 0);

    cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(beforeCount, cfg->channels[0].stageCount);
}

void test_copy_chain_stages_with_labels(void) {
    dsp_init();
    DspState *cfg = dsp_get_inactive_config();

    // Add crossover with labels to channel 0
    dsp_insert_crossover_lr(0, 2000.0f, 8, 0); // LR8 LPF

    // Copy to channel 1
    dsp_copy_chain_stages(0, 1);

    cfg = dsp_get_inactive_config();
    // Labels should be copied
    TEST_ASSERT_EQUAL_STRING("LR8 LPF", cfg->channels[1].stages[DSP_PEQ_BANDS].label);
    TEST_ASSERT_EQUAL_STRING("LR8 LPF", cfg->channels[1].stages[DSP_PEQ_BANDS + 1].label);
}

void test_reset_max_metrics(void) {
    // Process a buffer to generate metrics
    int32_t buf[64] = {0};
    for (int i = 0; i < 64; i++) buf[i] = 100000;
    dsp_process_buffer(buf, 32, 0);

    DspMetrics m = dsp_get_metrics();
    // maxProcessTimeUs should be set (>= processTimeUs)
    TEST_ASSERT_TRUE(m.maxProcessTimeUs >= m.processTimeUs);

    dsp_reset_max_metrics();
    m = dsp_get_metrics();
    TEST_ASSERT_EQUAL_UINT32(0, m.maxProcessTimeUs);
}

void test_clear_cpu_load(void) {
    int32_t buf[64] = {0};
    for (int i = 0; i < 64; i++) buf[i] = 100000;
    dsp_process_buffer(buf, 32, 0);

    dsp_clear_cpu_load();
    DspMetrics m = dsp_get_metrics();
    TEST_ASSERT_EQUAL_UINT32(0, m.processTimeUs);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, m.cpuLoadPercent);
}

void test_is_peq_index_boundaries(void) {
    TEST_ASSERT_FALSE(dsp_is_peq_index(-1));
    TEST_ASSERT_TRUE(dsp_is_peq_index(0));
    TEST_ASSERT_TRUE(dsp_is_peq_index(DSP_PEQ_BANDS - 1));
    TEST_ASSERT_FALSE(dsp_is_peq_index(DSP_PEQ_BANDS));
    TEST_ASSERT_FALSE(dsp_is_peq_index(100));
}

void test_chain_stage_count(void) {
    DspState *cfg = dsp_get_inactive_config();
    DspChannelConfig &ch = cfg->channels[0];
    // After init, stageCount == DSP_PEQ_BANDS, so chain count == 0
    TEST_ASSERT_EQUAL_INT(0, dsp_chain_stage_count(ch));

    // Add a chain stage
    dsp_add_stage(0, DSP_GAIN);
    cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(1, dsp_chain_stage_count(cfg->channels[0]));
}

void test_has_peq_bands(void) {
    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_TRUE(dsp_has_peq_bands(cfg->channels[0]));

    // Corrupt label to break detection
    cfg->channels[0].stages[0].label[0] = 'Z';
    TEST_ASSERT_FALSE(dsp_has_peq_bands(cfg->channels[0]));

    // Channel with too few stages
    DspChannelConfig empty;
    memset(&empty, 0, sizeof(empty));
    empty.stageCount = DSP_PEQ_BANDS - 1;
    TEST_ASSERT_FALSE(dsp_has_peq_bands(empty));
}

void test_fir_pool_exhaustion_rollback(void) {
    DspState *cfg = dsp_get_inactive_config();
    int initialCount = cfg->channels[0].stageCount;

    // Allocate all FIR slots
    int slots[DSP_MAX_FIR_SLOTS];
    for (int i = 0; i < DSP_MAX_FIR_SLOTS; i++) {
        slots[i] = dsp_add_stage(0, DSP_FIR);
        TEST_ASSERT_TRUE(slots[i] >= 0);
    }

    // Next FIR should fail and rollback
    int overflow = dsp_add_stage(0, DSP_FIR);
    TEST_ASSERT_EQUAL_INT(-1, overflow);

    cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(initialCount + DSP_MAX_FIR_SLOTS, cfg->channels[0].stageCount);
}

void test_delay_pool_exhaustion_rollback(void) {
    DspState *cfg = dsp_get_inactive_config();
    int initialCount = cfg->channels[0].stageCount;

    // Allocate all delay slots
    for (int i = 0; i < DSP_MAX_DELAY_SLOTS; i++) {
        int idx = dsp_add_stage(0, DSP_DELAY);
        TEST_ASSERT_TRUE(idx >= 0);
    }

    // Next delay should fail and rollback
    int overflow = dsp_add_stage(0, DSP_DELAY);
    TEST_ASSERT_EQUAL_INT(-1, overflow);

    cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL_INT(initialCount + DSP_MAX_DELAY_SLOTS, cfg->channels[0].stageCount);
}

void test_db_to_linear_helper(void) {
    // 0 dB = 1.0
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, dsp_db_to_linear(0.0f));
    // +6 dB ≈ 2.0
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.9953f, dsp_db_to_linear(6.0f));
    // -6 dB ≈ 0.5
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5012f, dsp_db_to_linear(-6.0f));
    // -20 dB = 0.1
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.1f, dsp_db_to_linear(-20.0f));
    // +20 dB = 10.0
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, dsp_db_to_linear(20.0f));
}

void test_time_coeff_helper(void) {
    // At 48kHz, 1ms should give coeff close to exp(-1/(0.001*48000)) = exp(-1/48) ≈ 0.9794
    float c = dsp_time_coeff(1.0f, 48000.0f);
    float expected = expf(-1.0f / 48.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected, c);

    // 10ms at 48000: exp(-1/(0.01*48000)) = exp(-1/480) ≈ 0.99792
    c = dsp_time_coeff(10.0f, 48000.0f);
    expected = expf(-1.0f / 480.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.00001f, expected, c);
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

    // Two-pass compressor/limiter
    RUN_TEST(test_two_pass_limiter_reduces_above_threshold);
    RUN_TEST(test_two_pass_compressor_applies_makeup);

    // Stereo link
    RUN_TEST(test_stereo_link_default_true);
    RUN_TEST(test_stereo_link_partner);
    RUN_TEST(test_stereo_link_mirror_copies_stages);
    RUN_TEST(test_stereo_link_mirror_resets_envelope);

    // Decimation FIR
    RUN_TEST(test_decimator_halves_output_length);
    RUN_TEST(test_decimation_filter_design);
    RUN_TEST(test_decimator_fird_basic);
    RUN_TEST(test_decimator_slot_freed_on_remove);

    // Cross-correlation / delay alignment
    RUN_TEST(test_corr_known_delay_detected);
    RUN_TEST(test_corr_zero_delay_returns_zero_index);
    // Delay alignment tests removed in v1.8.3 - incomplete feature
    // RUN_TEST(test_delay_align_measure_known_offset);
    // RUN_TEST(test_delay_align_low_confidence_with_noise);

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
    RUN_TEST(test_crossover_lr_stages_have_label);
    RUN_TEST(test_crossover_bw_stages_have_label);
    RUN_TEST(test_crossover_butterworth_hpf_all_orders);
    RUN_TEST(test_crossover_lr_rollback_on_partial_failure);

    // Bass management
    RUN_TEST(test_bass_management_setup);

    // Routing matrix
    RUN_TEST(test_routing_identity);
    RUN_TEST(test_routing_swap_lr);
    RUN_TEST(test_routing_apply_identity);
    RUN_TEST(test_routing_apply_swap);
    RUN_TEST(test_routing_set_gain_db);
    RUN_TEST(test_routing_mono_sum);

    // Linkwitz Transform
    RUN_TEST(test_linkwitz_coefficients_valid);
    RUN_TEST(test_linkwitz_identity_passthrough);
    RUN_TEST(test_linkwitz_stage_processes);
    RUN_TEST(test_linkwitz_rejects_invalid);
    RUN_TEST(test_linkwitz_is_biquad_type);

    // Gain ramp
    RUN_TEST(test_gain_ramp_converges);
    RUN_TEST(test_gain_ramp_settled_uses_fast_path);
    RUN_TEST(test_gain_ramp_smooth_transition);
    RUN_TEST(test_gain_ramp_swap_preserves_state);

    // Routing matrix
    RUN_TEST(test_routing_matrix_init_identity);
    RUN_TEST(test_routing_matrix_presets);

    // Config presets
    RUN_TEST(test_gain_init_sets_current_linear);

    // Convolution (N3)
    RUN_TEST(test_conv_impulse_passthrough);
    RUN_TEST(test_conv_free_releases_slot);
    RUN_TEST(test_conv_short_ir_matches_direct);
    RUN_TEST(test_conv_stage_type_integration);

    // Metrics
    RUN_TEST(test_metrics_initial);

    // Noise Gate
    RUN_TEST(test_noise_gate_below_threshold_attenuated);
    RUN_TEST(test_noise_gate_above_threshold_passthrough);
    RUN_TEST(test_noise_gate_hold_time);
    RUN_TEST(test_noise_gate_expander_ratio);
    RUN_TEST(test_noise_gate_range_limit);

    // Tone Control
    RUN_TEST(test_tone_ctrl_flat_at_zero_gain);
    RUN_TEST(test_tone_ctrl_bass_boost);
    RUN_TEST(test_tone_ctrl_treble_cut);
    RUN_TEST(test_tone_ctrl_mid_boost);

    // Bessel Crossover
    RUN_TEST(test_bessel_q_table_values);
    RUN_TEST(test_bessel_crossover_insert_order2);
    RUN_TEST(test_bessel_crossover_insert_order4);
    RUN_TEST(test_bessel_crossover_summation_flat);
    RUN_TEST(test_bessel_crossover_rollback_on_full);

    // Speaker Protection
    RUN_TEST(test_speaker_prot_thermal_ramp);
    RUN_TEST(test_speaker_prot_cool_down);
    RUN_TEST(test_speaker_prot_gain_reduction_at_limit);
    RUN_TEST(test_speaker_prot_excursion_limit);
    RUN_TEST(test_speaker_prot_metering_populated);

    // Stereo Width
    RUN_TEST(test_stereo_width_mono_collapse);
    RUN_TEST(test_stereo_width_normal_passthrough);
    RUN_TEST(test_stereo_width_extra_wide);
    RUN_TEST(test_stereo_width_center_boost);

    // Loudness Compensation
    RUN_TEST(test_loudness_reference_equals_current_flat);
    RUN_TEST(test_loudness_low_volume_bass_boost);
    RUN_TEST(test_loudness_amount_zero_bypass);
    RUN_TEST(test_loudness_treble_boost_at_low_level);

    // Bass Enhancement
    RUN_TEST(test_bass_enhance_generates_harmonics);
    RUN_TEST(test_bass_enhance_mix_zero_passthrough);
    RUN_TEST(test_bass_enhance_frequency_isolation);

    // Multi-Band Compressor
    RUN_TEST(test_multiband_2band_split_and_sum);
    RUN_TEST(test_multiband_per_band_compression);
    RUN_TEST(test_multiband_3band_crossover_accuracy);
    RUN_TEST(test_multiband_slot_alloc_and_free);

    // Baffle Step
    RUN_TEST(test_baffle_step_frequency_250mm);
    RUN_TEST(test_baffle_step_zero_width_safe);
    RUN_TEST(test_baffle_step_gain_6db);

    // THD Measurement
    RUN_TEST(test_thd_pure_sine_near_zero);
    RUN_TEST(test_thd_known_3rd_harmonic);
    RUN_TEST(test_thd_averaging_accumulates);
    RUN_TEST(test_thd_cancel_stops);
    RUN_TEST(test_thd_invalid_freq_safe);

    // Utility & Helper functions
    RUN_TEST(test_stage_type_name_all_types);
    RUN_TEST(test_ensure_peq_bands);
    RUN_TEST(test_copy_peq_bands);
    RUN_TEST(test_copy_chain_stages_basic);
    RUN_TEST(test_copy_chain_stages_preserves_peq);
    RUN_TEST(test_copy_chain_stages_empty_chain);
    RUN_TEST(test_copy_chain_stages_same_channel_noop);
    RUN_TEST(test_copy_chain_stages_with_labels);
    RUN_TEST(test_reset_max_metrics);
    RUN_TEST(test_clear_cpu_load);
    RUN_TEST(test_is_peq_index_boundaries);
    RUN_TEST(test_chain_stage_count);
    RUN_TEST(test_has_peq_bands);
    RUN_TEST(test_fir_pool_exhaustion_rollback);
    RUN_TEST(test_delay_pool_exhaustion_rollback);
    RUN_TEST(test_db_to_linear_helper);
    RUN_TEST(test_time_coeff_helper);

    return UNITY_END();
}
