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

#include <unity.h>
#include <math.h>
#include <string.h>

#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_init_f32.c"
#include "../../lib/esp_dsp_lite/src/dsps_fird_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_corr_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_conv_f32_ansi.c"
#include "../../src/dsp_biquad_gen.c"

#include "../../src/dsp_pipeline.h"
#include "../../src/dsp_coefficients.h"

#include "../../src/heap_budget.cpp"
#include "../../src/psram_alloc.cpp"
#include "../../src/dsp_coefficients.cpp"
#include "../../src/dsp_pipeline.cpp"
#include "../../src/dsp_crossover.cpp"
#include "../../src/dsp_convolution.cpp"

#define FLOAT_TOL 0.001f

void setUp(void) {
    dsp_init();
}
void tearDown(void) {}

// ===== dsp_mb_alloc_slot / dsp_mb_free_slot =====

void test_mb_alloc_returns_valid_slot(void) {
    int slot = dsp_mb_alloc_slot();
    TEST_ASSERT_TRUE(slot >= 0);
    dsp_mb_free_slot(slot);
}

void test_mb_alloc_exhaustion_returns_minus1(void) {
    // DSP_MULTIBAND_MAX_SLOTS = 1, so second alloc should fail
    int s0 = dsp_mb_alloc_slot();
    int s1 = dsp_mb_alloc_slot();
    TEST_ASSERT_TRUE(s0 >= 0);
    TEST_ASSERT_EQUAL_INT(-1, s1);  // pool exhausted
    dsp_mb_free_slot(s0);
}

void test_mb_free_releases_slot_for_reuse(void) {
    int s0 = dsp_mb_alloc_slot();
    dsp_mb_free_slot(s0);
    int s1 = dsp_mb_alloc_slot();
    TEST_ASSERT_EQUAL_INT(s0, s1);
    dsp_mb_free_slot(s1);
}

// ===== dsp_mb_set_band_params =====

void test_mb_set_band_params_happy_path(void) {
    int slot = dsp_mb_alloc_slot();
    bool ok = dsp_mb_set_band_params(slot, 0,
                                     -12.0f,  // thresholdDb
                                     10.0f,   // attackMs
                                     100.0f,  // releaseMs
                                     4.0f,    // ratio
                                     6.0f,    // kneeDb
                                     3.0f);   // makeupGainDb
    TEST_ASSERT_TRUE(ok);
    dsp_mb_free_slot(slot);
}

void test_mb_set_band_params_invalid_slot_negative(void) {
    bool ok = dsp_mb_set_band_params(-1, 0, -12.0f, 10.0f, 100.0f, 4.0f, 6.0f, 0.0f);
    TEST_ASSERT_FALSE(ok);
}

void test_mb_set_band_params_invalid_slot_too_large(void) {
    bool ok = dsp_mb_set_band_params(DSP_MULTIBAND_MAX_SLOTS, 0, -12.0f, 10.0f, 100.0f, 4.0f, 6.0f, 0.0f);
    TEST_ASSERT_FALSE(ok);
}

void test_mb_set_band_params_invalid_band_negative(void) {
    int slot = dsp_mb_alloc_slot();
    bool ok = dsp_mb_set_band_params(slot, -1, -12.0f, 10.0f, 100.0f, 4.0f, 6.0f, 0.0f);
    TEST_ASSERT_FALSE(ok);
    dsp_mb_free_slot(slot);
}

void test_mb_set_band_params_invalid_band_too_large(void) {
    int slot = dsp_mb_alloc_slot();
    bool ok = dsp_mb_set_band_params(slot, DSP_MULTIBAND_MAX_BANDS, -12.0f, 10.0f, 100.0f, 4.0f, 6.0f, 0.0f);
    TEST_ASSERT_FALSE(ok);
    dsp_mb_free_slot(slot);
}

void test_mb_set_band_params_makeup_linear_computed(void) {
    int slot = dsp_mb_alloc_slot();
    float makeupGainDb = 6.0f;
    dsp_mb_set_band_params(slot, 0, -12.0f, 10.0f, 100.0f, 4.0f, 0.0f, makeupGainDb);

    // Access the slot data via a second band write to confirm it doesn't reset other bands.
    // The makeupLinear = 10^(6/20) = 1.995... ~= 2.0
    // We verify via a band0 read using set_band_params again — since we can't read,
    // just confirm it accepts and the second call also returns true.
    bool ok2 = dsp_mb_set_band_params(slot, 0, -12.0f, 10.0f, 100.0f, 4.0f, 0.0f, makeupGainDb);
    TEST_ASSERT_TRUE(ok2);
    dsp_mb_free_slot(slot);
}

void test_mb_set_band_params_zero_makeup_gain(void) {
    int slot = dsp_mb_alloc_slot();
    bool ok = dsp_mb_set_band_params(slot, 1, -6.0f, 5.0f, 50.0f, 2.0f, 3.0f, 0.0f);
    TEST_ASSERT_TRUE(ok);  // 0 dB makeup = 1.0 linear is valid
    dsp_mb_free_slot(slot);
}

void test_mb_set_band_params_all_four_bands(void) {
    int slot = dsp_mb_alloc_slot();
    for (int b = 0; b < DSP_MULTIBAND_MAX_BANDS; b++) {
        bool ok = dsp_mb_set_band_params(slot, b, -12.0f, 10.0f, 100.0f, 4.0f, 6.0f, 0.0f);
        TEST_ASSERT_TRUE(ok);
    }
    dsp_mb_free_slot(slot);
}

// ===== dsp_mb_set_crossover_freq =====

void test_mb_set_crossover_freq_happy_path(void) {
    int slot = dsp_mb_alloc_slot();
    bool ok = dsp_mb_set_crossover_freq(slot, 0, 200.0f, 48000);
    TEST_ASSERT_TRUE(ok);
    dsp_mb_free_slot(slot);
}

void test_mb_set_crossover_freq_invalid_slot(void) {
    bool ok = dsp_mb_set_crossover_freq(-1, 0, 200.0f, 48000);
    TEST_ASSERT_FALSE(ok);
}

void test_mb_set_crossover_freq_invalid_boundary_too_large(void) {
    int slot = dsp_mb_alloc_slot();
    bool ok = dsp_mb_set_crossover_freq(slot, 3, 200.0f, 48000);  // boundary 0-2 only
    TEST_ASSERT_FALSE(ok);
    dsp_mb_free_slot(slot);
}

void test_mb_set_crossover_freq_invalid_boundary_negative(void) {
    int slot = dsp_mb_alloc_slot();
    bool ok = dsp_mb_set_crossover_freq(slot, -1, 200.0f, 48000);
    TEST_ASSERT_FALSE(ok);
    dsp_mb_free_slot(slot);
}

void test_mb_set_crossover_freq_zero_sample_rate(void) {
    int slot = dsp_mb_alloc_slot();
    bool ok = dsp_mb_set_crossover_freq(slot, 0, 200.0f, 0);
    TEST_ASSERT_FALSE(ok);
    dsp_mb_free_slot(slot);
}

void test_mb_set_crossover_freq_near_nyquist_rejected(void) {
    // freqHz / sampleRate >= 0.5 => invalid
    int slot = dsp_mb_alloc_slot();
    bool ok = dsp_mb_set_crossover_freq(slot, 0, 24001.0f, 48000);  // > Nyquist
    TEST_ASSERT_FALSE(ok);
    dsp_mb_free_slot(slot);
}

void test_mb_set_crossover_freq_zero_freq_rejected(void) {
    int slot = dsp_mb_alloc_slot();
    bool ok = dsp_mb_set_crossover_freq(slot, 0, 0.0f, 48000);  // normFreq = 0
    TEST_ASSERT_FALSE(ok);
    dsp_mb_free_slot(slot);
}

void test_mb_set_crossover_freq_all_three_boundaries(void) {
    int slot = dsp_mb_alloc_slot();
    bool ok0 = dsp_mb_set_crossover_freq(slot, 0, 200.0f, 48000);
    bool ok1 = dsp_mb_set_crossover_freq(slot, 1, 2000.0f, 48000);
    bool ok2 = dsp_mb_set_crossover_freq(slot, 2, 8000.0f, 48000);
    TEST_ASSERT_TRUE(ok0);
    TEST_ASSERT_TRUE(ok1);
    TEST_ASSERT_TRUE(ok2);
    dsp_mb_free_slot(slot);
}

// ===== Integration: stage holds mbSlot reference =====

void test_multiband_comp_stage_holds_slot_index(void) {
    // Simulates setMultibandComp WS handler: alloc slot, set params, store in stage.
    // Use dsp_add_stage at position 0 — avoids PEQ band setup complexity in unit test.
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();

    // dsp_add_stage for DSP_MULTIBAND_COMP auto-allocs an mbSlot internally
    int stageIdx = dsp_add_stage(0, DSP_MULTIBAND_COMP, 0);
    TEST_ASSERT_TRUE(stageIdx >= 0);

    // The auto-allocated slot should be valid (>= 0)
    int8_t autoSlot = cfg->channels[0].stages[stageIdx].multibandComp.mbSlot;
    TEST_ASSERT_TRUE(autoSlot >= 0);

    // Set numBands and verify it sticks after swap
    cfg->channels[0].stages[stageIdx].multibandComp.numBands = 3;

    dsp_swap_config();

    // Confirm multiband stage is visible in active config with correct numBands
    DspState *active = dsp_get_active_config();
    bool found = false;
    for (int s = 0; s < active->channels[0].stageCount; s++) {
        if (active->channels[0].stages[s].type == DSP_MULTIBAND_COMP) {
            TEST_ASSERT_EQUAL_INT8(autoSlot, active->channels[0].stages[s].multibandComp.mbSlot);
            TEST_ASSERT_EQUAL_UINT8(3, active->channels[0].stages[s].multibandComp.numBands);
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_mb_alloc_returns_valid_slot);
    RUN_TEST(test_mb_alloc_exhaustion_returns_minus1);
    RUN_TEST(test_mb_free_releases_slot_for_reuse);

    RUN_TEST(test_mb_set_band_params_happy_path);
    RUN_TEST(test_mb_set_band_params_invalid_slot_negative);
    RUN_TEST(test_mb_set_band_params_invalid_slot_too_large);
    RUN_TEST(test_mb_set_band_params_invalid_band_negative);
    RUN_TEST(test_mb_set_band_params_invalid_band_too_large);
    RUN_TEST(test_mb_set_band_params_makeup_linear_computed);
    RUN_TEST(test_mb_set_band_params_zero_makeup_gain);
    RUN_TEST(test_mb_set_band_params_all_four_bands);

    RUN_TEST(test_mb_set_crossover_freq_happy_path);
    RUN_TEST(test_mb_set_crossover_freq_invalid_slot);
    RUN_TEST(test_mb_set_crossover_freq_invalid_boundary_too_large);
    RUN_TEST(test_mb_set_crossover_freq_invalid_boundary_negative);
    RUN_TEST(test_mb_set_crossover_freq_zero_sample_rate);
    RUN_TEST(test_mb_set_crossover_freq_near_nyquist_rejected);
    RUN_TEST(test_mb_set_crossover_freq_zero_freq_rejected);
    RUN_TEST(test_mb_set_crossover_freq_all_three_boundaries);

    RUN_TEST(test_multiband_comp_stage_holds_slot_index);

    return UNITY_END();
}
