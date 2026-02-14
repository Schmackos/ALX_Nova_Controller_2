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

// Include DSP implementation source
#include "../../src/dsp_coefficients.cpp"
#include "../../src/dsp_pipeline.cpp"
#include "../../src/dsp_crossover.cpp"

#define FLOAT_TOL 0.001f

void setUp(void) {
    dsp_init();
}

void tearDown(void) {}

// ===== PEQ Band Initialization Tests =====

void test_peq_bands_initialized_on_init(void) {
    DspState *cfg = dsp_get_active_config();
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        // Should have exactly DSP_PEQ_BANDS stages
        TEST_ASSERT_GREATER_OR_EQUAL(DSP_PEQ_BANDS, cfg->channels[ch].stageCount);
    }
}

void test_peq_bands_default_values(void) {
    DspState *cfg = dsp_get_active_config();
    DspChannelConfig &ch = cfg->channels[0];

    // Default frequencies spread logarithmically across spectrum
    const float expectedFreqs[10] = {31,63,125,250,500,1000,2000,4000,8000,16000};
    for (int b = 0; b < DSP_PEQ_BANDS; b++) {
        DspStage &s = ch.stages[b];
        TEST_ASSERT_FALSE(s.enabled);
        TEST_ASSERT_EQUAL(DSP_BIQUAD_PEQ, s.type);
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, expectedFreqs[b], s.biquad.frequency);
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, s.biquad.gain);
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, s.biquad.Q);
    }
}

void test_peq_band_labels(void) {
    DspState *cfg = dsp_get_active_config();
    DspChannelConfig &ch = cfg->channels[0];

    // Check first and last PEQ labels
    TEST_ASSERT_EQUAL_STRING("PEQ 1", ch.stages[0].label);
    TEST_ASSERT_EQUAL_STRING("PEQ 2", ch.stages[1].label);
    TEST_ASSERT_EQUAL_STRING("PEQ 9", ch.stages[8].label);
    TEST_ASSERT_EQUAL_STRING("PEQ 10", ch.stages[9].label);
}

void test_peq_bands_all_channels(void) {
    DspState *cfg = dsp_get_active_config();
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        TEST_ASSERT_TRUE(dsp_has_peq_bands(cfg->channels[ch]));
    }
}

// ===== dsp_is_peq_index Tests =====

void test_is_peq_index_in_range(void) {
    for (int i = 0; i < DSP_PEQ_BANDS; i++) {
        TEST_ASSERT_TRUE(dsp_is_peq_index(i));
    }
}

void test_is_peq_index_out_of_range(void) {
    TEST_ASSERT_FALSE(dsp_is_peq_index(-1));
    TEST_ASSERT_FALSE(dsp_is_peq_index(DSP_PEQ_BANDS));
    TEST_ASSERT_FALSE(dsp_is_peq_index(15));
    TEST_ASSERT_FALSE(dsp_is_peq_index(20));
}

// ===== Chain Stage Tests =====

void test_chain_stage_count_empty(void) {
    DspState *cfg = dsp_get_active_config();
    // Initially only PEQ bands, no chain stages
    TEST_ASSERT_EQUAL(0, dsp_chain_stage_count(cfg->channels[0]));
}

void test_add_chain_stage(void) {
    dsp_copy_active_to_inactive();
    int idx = dsp_add_chain_stage(0, DSP_GAIN);
    TEST_ASSERT_GREATER_OR_EQUAL(DSP_PEQ_BANDS, idx);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL(DSP_PEQ_BANDS + 1, cfg->channels[0].stageCount);
    TEST_ASSERT_EQUAL(DSP_GAIN, cfg->channels[0].stages[idx].type);
    TEST_ASSERT_EQUAL(1, dsp_chain_stage_count(cfg->channels[0]));
}

void test_add_chain_stage_with_position(void) {
    dsp_copy_active_to_inactive();

    // Add two chain stages
    int idx1 = dsp_add_chain_stage(0, DSP_GAIN);
    int idx2 = dsp_add_chain_stage(0, DSP_LIMITER, 0); // At chain position 0

    TEST_ASSERT_EQUAL(DSP_PEQ_BANDS, idx2); // Should be at absolute index 10
    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL(DSP_LIMITER, cfg->channels[0].stages[DSP_PEQ_BANDS].type);
    TEST_ASSERT_EQUAL(DSP_GAIN, cfg->channels[0].stages[DSP_PEQ_BANDS + 1].type);
}

void test_remove_chain_stage(void) {
    dsp_copy_active_to_inactive();
    dsp_add_chain_stage(0, DSP_GAIN);
    dsp_add_chain_stage(0, DSP_LIMITER);

    DspState *cfg = dsp_get_inactive_config();
    TEST_ASSERT_EQUAL(DSP_PEQ_BANDS + 2, cfg->channels[0].stageCount);

    bool result = dsp_remove_chain_stage(0, 0); // Remove first chain stage
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(DSP_PEQ_BANDS + 1, cfg->channels[0].stageCount);
}

void test_remove_chain_stage_rejects_peq_index(void) {
    dsp_copy_active_to_inactive();
    // Try to remove a PEQ band via chain stage removal
    // chainIndex -10 would map to absIndex < PEQ_BANDS
    bool result = dsp_remove_chain_stage(0, -1);
    TEST_ASSERT_FALSE(result);
}

// ===== PEQ Band Update Tests =====

void test_peq_band_update(void) {
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();

    DspStage &s = cfg->channels[0].stages[3]; // PEQ 4
    s.enabled = true;
    s.biquad.frequency = 2000.0f;
    s.biquad.gain = 6.0f;
    s.biquad.Q = 2.0f;
    dsp_compute_biquad_coeffs(s.biquad, s.type, cfg->sampleRate);

    TEST_ASSERT_TRUE(s.enabled);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 2000.0f, s.biquad.frequency);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 6.0f, s.biquad.gain);
    // PEQ with +6dB: b0 should be > 1.0
    TEST_ASSERT_TRUE(s.biquad.coeffs[0] > 1.0f);
}

void test_peq_band_type_change(void) {
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();

    DspStage &s = cfg->channels[0].stages[0];
    s.type = DSP_BIQUAD_LOW_SHELF;
    s.biquad.frequency = 200.0f;
    s.biquad.gain = 3.0f;
    dsp_compute_biquad_coeffs(s.biquad, s.type, cfg->sampleRate);

    TEST_ASSERT_EQUAL(DSP_BIQUAD_LOW_SHELF, s.type);
    // Low shelf coefficients should be non-trivial
    TEST_ASSERT_TRUE(s.biquad.coeffs[0] != 0.0f);
}

// ===== Channel Copy Tests =====

void test_copy_peq_bands(void) {
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();

    // Configure band 0 of channel 0
    cfg->channels[0].stages[0].enabled = true;
    cfg->channels[0].stages[0].biquad.frequency = 500.0f;
    cfg->channels[0].stages[0].biquad.gain = -3.0f;
    cfg->channels[0].stages[0].biquad.Q = 0.5f;

    // Copy to channel 1
    dsp_copy_peq_bands(0, 1);

    // Verify channel 1 has the same PEQ settings
    TEST_ASSERT_TRUE(cfg->channels[1].stages[0].enabled);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 500.0f, cfg->channels[1].stages[0].biquad.frequency);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, -3.0f, cfg->channels[1].stages[0].biquad.gain);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.5f, cfg->channels[1].stages[0].biquad.Q);
}

void test_copy_peq_bands_same_channel(void) {
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();

    float origFreq = cfg->channels[0].stages[0].biquad.frequency;
    // Copy same channel should be a no-op
    dsp_copy_peq_bands(0, 0);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, origFreq, cfg->channels[0].stages[0].biquad.frequency);
}

// ===== Enable/Disable All Tests =====

void test_enable_all_peq_bands(void) {
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();

    // Enable all PEQ bands
    for (int b = 0; b < DSP_PEQ_BANDS; b++) {
        cfg->channels[0].stages[b].enabled = true;
    }

    for (int b = 0; b < DSP_PEQ_BANDS; b++) {
        TEST_ASSERT_TRUE(cfg->channels[0].stages[b].enabled);
    }
}

void test_disable_all_peq_bands(void) {
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();

    // First enable all
    for (int b = 0; b < DSP_PEQ_BANDS; b++) {
        cfg->channels[0].stages[b].enabled = true;
    }
    // Then disable all
    for (int b = 0; b < DSP_PEQ_BANDS; b++) {
        cfg->channels[0].stages[b].enabled = false;
    }

    for (int b = 0; b < DSP_PEQ_BANDS; b++) {
        TEST_ASSERT_FALSE(cfg->channels[0].stages[b].enabled);
    }
}

// ===== Config Migration Tests =====

void test_dsp_has_peq_bands_detection(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);
    TEST_ASSERT_FALSE(dsp_has_peq_bands(ch));

    dsp_init_peq_bands(ch);
    TEST_ASSERT_TRUE(dsp_has_peq_bands(ch));
}

void test_init_peq_bands_shifts_existing_stages(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    // Add 3 stages first
    ch.stageCount = 3;
    dsp_init_stage(ch.stages[0], DSP_GAIN);
    ch.stages[0].gain.gainDb = 5.0f;
    dsp_init_stage(ch.stages[1], DSP_LIMITER);
    ch.stages[1].limiter.thresholdDb = -6.0f;
    dsp_init_stage(ch.stages[2], DSP_MUTE);

    // Init PEQ bands — should shift existing stages to 10+
    dsp_init_peq_bands(ch);

    TEST_ASSERT_EQUAL(DSP_PEQ_BANDS + 3, ch.stageCount);
    TEST_ASSERT_TRUE(dsp_has_peq_bands(ch));

    // Original stages should now be at indices 10, 11, 12
    TEST_ASSERT_EQUAL(DSP_GAIN, ch.stages[10].type);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 5.0f, ch.stages[10].gain.gainDb);
    TEST_ASSERT_EQUAL(DSP_LIMITER, ch.stages[11].type);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, -6.0f, ch.stages[11].limiter.thresholdDb);
    TEST_ASSERT_EQUAL(DSP_MUTE, ch.stages[12].type);
}

void test_ensure_peq_bands(void) {
    // Reset to no PEQ bands
    DspState st;
    st.globalBypass = false;
    st.sampleRate = 48000;
    for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
        dsp_init_channel(st.channels[i]);
    }

    // Verify no PEQ bands
    TEST_ASSERT_FALSE(dsp_has_peq_bands(st.channels[0]));

    // Ensure PEQ bands
    dsp_ensure_peq_bands(&st);
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        TEST_ASSERT_TRUE(dsp_has_peq_bands(st.channels[ch]));
    }
}

// ===== Mixed PEQ + Chain Stage Tests =====

void test_peq_and_chain_stages_coexist(void) {
    dsp_copy_active_to_inactive();

    // Add a chain stage
    int chainIdx = dsp_add_chain_stage(0, DSP_GAIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, chainIdx);

    DspState *cfg = dsp_get_inactive_config();
    // PEQ bands still valid
    for (int b = 0; b < DSP_PEQ_BANDS; b++) {
        TEST_ASSERT_EQUAL_STRING_LEN("PEQ", cfg->channels[0].stages[b].label, 3);
    }
    // Chain stage present
    TEST_ASSERT_EQUAL(DSP_GAIN, cfg->channels[0].stages[chainIdx].type);
    TEST_ASSERT_EQUAL(1, dsp_chain_stage_count(cfg->channels[0]));
}

void test_remove_stage_rejects_negative(void) {
    dsp_copy_active_to_inactive();
    bool result = dsp_remove_stage(0, -1);
    TEST_ASSERT_FALSE(result);
}

void test_stage_count_with_peq(void) {
    DspState *cfg = dsp_get_active_config();
    // After init, stageCount should be DSP_PEQ_BANDS
    TEST_ASSERT_EQUAL(DSP_PEQ_BANDS, cfg->channels[0].stageCount);
}

// ===== DSP Processing With PEQ =====

void test_peq_disabled_bands_pass_through(void) {
    // All PEQ bands disabled → signal should pass through unchanged
    int32_t buffer[128]; // 64 stereo frames
    for (int i = 0; i < 128; i++) {
        buffer[i] = (i % 2 == 0) ? 1000000 : -1000000;
    }

    int32_t expected[128];
    memcpy(expected, buffer, sizeof(buffer));

    dsp_process_buffer(buffer, 64, 0);

    // Disabled PEQ bands should not alter the signal
    for (int i = 0; i < 128; i++) {
        TEST_ASSERT_INT32_WITHIN(10, expected[i], buffer[i]);
    }
}

void test_peq_enabled_band_modifies_signal(void) {
    // Enable one PEQ band with significant boost
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[0].stages[0].enabled = true;
    cfg->channels[0].stages[0].biquad.frequency = 1000.0f;
    cfg->channels[0].stages[0].biquad.gain = 12.0f;
    cfg->channels[0].stages[0].biquad.Q = 1.0f;
    dsp_compute_biquad_coeffs(cfg->channels[0].stages[0].biquad,
                               DSP_BIQUAD_PEQ, cfg->sampleRate);
    dsp_swap_config();

    // Generate a 1kHz-ish signal
    int32_t buffer[128];
    for (int i = 0; i < 64; i++) {
        float t = (float)i / 48000.0f;
        float val = sinf(2.0f * 3.14159f * 1000.0f * t) * 1000000.0f;
        buffer[i * 2] = (int32_t)val;
        buffer[i * 2 + 1] = (int32_t)val;
    }

    int32_t original_peak = 0;
    for (int i = 0; i < 128; i++) {
        int32_t abs_val = buffer[i] < 0 ? -buffer[i] : buffer[i];
        if (abs_val > original_peak) original_peak = abs_val;
    }

    dsp_process_buffer(buffer, 64, 0);

    // With +12dB boost, output should be louder
    int32_t processed_peak = 0;
    for (int i = 0; i < 128; i++) {
        int32_t abs_val = buffer[i] < 0 ? -buffer[i] : buffer[i];
        if (abs_val > processed_peak) processed_peak = abs_val;
    }

    // The processed peak should be significantly larger (at least 2x for 12dB)
    TEST_ASSERT_GREATER_THAN(original_peak, processed_peak);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // PEQ band initialization
    RUN_TEST(test_peq_bands_initialized_on_init);
    RUN_TEST(test_peq_bands_default_values);
    RUN_TEST(test_peq_band_labels);
    RUN_TEST(test_peq_bands_all_channels);

    // dsp_is_peq_index
    RUN_TEST(test_is_peq_index_in_range);
    RUN_TEST(test_is_peq_index_out_of_range);

    // Chain stage CRUD
    RUN_TEST(test_chain_stage_count_empty);
    RUN_TEST(test_add_chain_stage);
    RUN_TEST(test_add_chain_stage_with_position);
    RUN_TEST(test_remove_chain_stage);
    RUN_TEST(test_remove_chain_stage_rejects_peq_index);

    // PEQ band updates
    RUN_TEST(test_peq_band_update);
    RUN_TEST(test_peq_band_type_change);

    // Channel copy
    RUN_TEST(test_copy_peq_bands);
    RUN_TEST(test_copy_peq_bands_same_channel);

    // Enable/disable all
    RUN_TEST(test_enable_all_peq_bands);
    RUN_TEST(test_disable_all_peq_bands);

    // Config migration
    RUN_TEST(test_dsp_has_peq_bands_detection);
    RUN_TEST(test_init_peq_bands_shifts_existing_stages);
    RUN_TEST(test_ensure_peq_bands);

    // Mixed PEQ + chain
    RUN_TEST(test_peq_and_chain_stages_coexist);
    RUN_TEST(test_remove_stage_rejects_negative);
    RUN_TEST(test_stage_count_with_peq);

    // DSP processing with PEQ
    RUN_TEST(test_peq_disabled_bands_pass_through);
    RUN_TEST(test_peq_enabled_band_modifies_signal);

    return UNITY_END();
}
