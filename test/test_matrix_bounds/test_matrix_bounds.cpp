/**
 * test_matrix_bounds.cpp
 *
 * Unit tests for matrix routing bounds validation:
 *   - Compile-time dimension invariants
 *   - Matrix gain setter/getter boundary checks
 *   - firstChannel overflow in sink dispatch
 *   - set_sink / set_source registration bounds
 *   - inCh[] loop-based population correctness
 *   - Sink channel validation (firstChannel + channelCount <= MATRIX_SIZE)
 *
 * Tests run on the native platform (host machine, no hardware).
 * All logic under test is replicated inline (test_build_src = no).
 *
 * Build flags required (already in platformio.ini [env:native]):
 *   -D UNIT_TEST -D NATIVE_TEST -D DSP_ENABLED -D DAC_ENABLED
 */

#include <unity.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

// Pull in real config for authoritative constants
#include "../../src/config.h"
#include "../../src/audio_pipeline.h"
#include "../../src/audio_input_source.h"
#include "../../src/audio_output_sink.h"

// ===== Replicated logic from audio_pipeline.cpp =====

#define FRAMES 256

// Matrix gain storage (mirrors production static)
static float _matrixGain[AUDIO_PIPELINE_MATRIX_SIZE][AUDIO_PIPELINE_MATRIX_SIZE];

// Replicated set_matrix_gain with bounds check (matches production)
static void set_matrix_gain(int out_ch, int in_ch, float gain_linear) {
    if (out_ch < 0 || out_ch >= AUDIO_PIPELINE_MATRIX_SIZE) return;
    if (in_ch  < 0 || in_ch  >= AUDIO_PIPELINE_MATRIX_SIZE) return;
    _matrixGain[out_ch][in_ch] = gain_linear;
}

// Replicated get_matrix_gain with bounds check (matches production)
static float get_matrix_gain(int out_ch, int in_ch) {
    if (out_ch < 0 || out_ch >= AUDIO_PIPELINE_MATRIX_SIZE) return 0.0f;
    if (in_ch  < 0 || in_ch  >= AUDIO_PIPELINE_MATRIX_SIZE) return 0.0f;
    return _matrixGain[out_ch][in_ch];
}

// Replicated set_source validation (matches production)
static bool validate_set_source(int lane) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return false;
    if (lane * 2 + 1 >= AUDIO_PIPELINE_MATRIX_SIZE) return false;
    return true;
}

// Replicated set_sink validation (matches production)
static bool validate_set_sink(int slot, uint8_t firstChannel, uint8_t channelCount) {
    if (slot < 0 || slot >= AUDIO_OUT_MAX_SINKS) return false;
    if (firstChannel + channelCount > AUDIO_PIPELINE_MATRIX_SIZE) return false;
    return true;
}

// Replicated sink dispatch bounds logic (matches production pipeline_write_output)
static bool dispatch_would_skip(uint8_t firstChannel, uint8_t channelCount) {
    int chL = firstChannel;
    int chR = (channelCount >= 2) ? chL + 1 : chL;
    if (chL >= AUDIO_PIPELINE_MATRIX_SIZE) return true;   // skipped
    if (chR >= AUDIO_PIPELINE_MATRIX_SIZE) chR = chL;     // clamped, not skipped
    return false;
}

// Replicated inCh population loop (matches production pipeline_mix_matrix)
static void populate_inCh(const float **inCh, float *laneL[], float *laneR[]) {
    for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) inCh[i] = NULL;
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS && lane * 2 + 1 < AUDIO_PIPELINE_MATRIX_SIZE; lane++) {
        inCh[lane * 2]     = laneL[lane];
        inCh[lane * 2 + 1] = laneR[lane];
    }
}

// ===== setUp / tearDown =====

void setUp() {
    memset(_matrixGain, 0, sizeof(_matrixGain));
}

void tearDown() {}

// ============================================================
// Group 1: Dimension Invariants
// ============================================================

void test_max_inputs_stereo_fits_matrix() {
    TEST_ASSERT_TRUE_MESSAGE(
        AUDIO_PIPELINE_MAX_INPUTS * 2 <= AUDIO_PIPELINE_MATRIX_SIZE,
        "All stereo input channels must fit within matrix columns");
}

void test_max_sinks_stereo_fits_matrix() {
    TEST_ASSERT_TRUE_MESSAGE(
        AUDIO_OUT_MAX_SINKS * 2 <= AUDIO_PIPELINE_MATRIX_SIZE,
        "All stereo output channels must fit within matrix rows");
}

void test_input_source_max_inputs_matches() {
    // Verifies audio_input_source.h fallback matches audio_pipeline.h
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        8, AUDIO_PIPELINE_MAX_INPUTS,
        "AUDIO_PIPELINE_MAX_INPUTS must be 8 regardless of include order");
}

// ============================================================
// Group 2: Matrix Gain Bounds
// ============================================================

void test_gain_valid_corner_indices() {
    set_matrix_gain(0, 0, 1.0f);
    set_matrix_gain(7, 7, 0.5f);
    set_matrix_gain(15, 15, 0.25f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,  get_matrix_gain(0, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f,  get_matrix_gain(7, 7));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, get_matrix_gain(15, 15));
}

void test_gain_negative_out_rejected() {
    set_matrix_gain(0, 0, 1.0f);
    set_matrix_gain(-1, 0, 0.5f);  // Should be rejected
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, get_matrix_gain(0, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, get_matrix_gain(-1, 0));
}

void test_gain_out_at_boundary() {
    set_matrix_gain(15, 0, 0.75f);  // Last valid
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, get_matrix_gain(15, 0));

    set_matrix_gain(16, 0, 0.5f);   // Out of bounds — should be rejected
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, get_matrix_gain(16, 0));
}

void test_gain_negative_in_rejected() {
    set_matrix_gain(0, 0, 1.0f);
    set_matrix_gain(0, -1, 0.5f);  // Should be rejected
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, get_matrix_gain(0, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, get_matrix_gain(0, -1));
}

void test_gain_in_at_boundary() {
    set_matrix_gain(0, 15, 0.8f);  // Last valid
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, get_matrix_gain(0, 15));

    set_matrix_gain(0, 16, 0.5f);  // Out of bounds
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, get_matrix_gain(0, 16));
}

void test_gain_both_at_max() {
    set_matrix_gain(15, 15, 0.42f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.42f, get_matrix_gain(15, 15));
}

// ============================================================
// Group 3: firstChannel Overflow in Sink Dispatch
// ============================================================

void test_firstChannel_0_valid() {
    TEST_ASSERT_FALSE_MESSAGE(dispatch_would_skip(0, 2),
        "firstChannel=0, count=2 should not be skipped");
}

void test_firstChannel_14_valid() {
    TEST_ASSERT_FALSE_MESSAGE(dispatch_would_skip(14, 2),
        "firstChannel=14, count=2 (14+2=16=MATRIX_SIZE) should not be skipped");
}

void test_firstChannel_15_mono_valid() {
    TEST_ASSERT_FALSE_MESSAGE(dispatch_would_skip(15, 1),
        "firstChannel=15, count=1 (mono) should not be skipped");
}

void test_firstChannel_16_overflow_skipped() {
    TEST_ASSERT_TRUE_MESSAGE(dispatch_would_skip(16, 2),
        "firstChannel=16 exceeds MATRIX_SIZE and must be skipped");
}

// ============================================================
// Group 4: Registration Bounds
// ============================================================

void test_set_sink_negative_slot_rejected() {
    TEST_ASSERT_FALSE(validate_set_sink(-1, 0, 2));
}

void test_set_sink_at_max_rejected() {
    TEST_ASSERT_FALSE(validate_set_sink(AUDIO_OUT_MAX_SINKS, 0, 2));
}

void test_set_source_negative_lane_rejected() {
    TEST_ASSERT_FALSE(validate_set_source(-1));
}

void test_set_source_at_max_rejected() {
    TEST_ASSERT_FALSE(validate_set_source(AUDIO_PIPELINE_MAX_INPUTS));
}

// ============================================================
// Group 5: inCh Population
// ============================================================

void test_inCh_all_8_lanes_populated() {
    float bufsL[AUDIO_PIPELINE_MAX_INPUTS][1];
    float bufsR[AUDIO_PIPELINE_MAX_INPUTS][1];
    float *laneL[AUDIO_PIPELINE_MAX_INPUTS];
    float *laneR[AUDIO_PIPELINE_MAX_INPUTS];
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        bufsL[i][0] = (float)(i * 2);
        bufsR[i][0] = (float)(i * 2 + 1);
        laneL[i] = bufsL[i];
        laneR[i] = bufsR[i];
    }

    const float *inCh[AUDIO_PIPELINE_MATRIX_SIZE];
    populate_inCh(inCh, laneL, laneR);

    // All 8 lanes × 2 channels = 16 entries populated
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        TEST_ASSERT_NOT_NULL_MESSAGE(inCh[lane * 2],
            "Left channel of each lane must be populated");
        TEST_ASSERT_NOT_NULL_MESSAGE(inCh[lane * 2 + 1],
            "Right channel of each lane must be populated");
        TEST_ASSERT_EQUAL_FLOAT((float)(lane * 2), inCh[lane * 2][0]);
        TEST_ASSERT_EQUAL_FLOAT((float)(lane * 2 + 1), inCh[lane * 2 + 1][0]);
    }
}

void test_inCh_null_lane_stays_null() {
    float *laneL[AUDIO_PIPELINE_MAX_INPUTS] = {};
    float *laneR[AUDIO_PIPELINE_MAX_INPUTS] = {};
    // Only populate lane 0
    float bufL[1] = {1.0f};
    float bufR[1] = {2.0f};
    laneL[0] = bufL;
    laneR[0] = bufR;

    const float *inCh[AUDIO_PIPELINE_MATRIX_SIZE];
    populate_inCh(inCh, laneL, laneR);

    TEST_ASSERT_NOT_NULL(inCh[0]);
    TEST_ASSERT_NOT_NULL(inCh[1]);
    // Lanes 1-7 are null pointers -> inCh[2..15] should be null
    for (int i = 2; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
        TEST_ASSERT_NULL_MESSAGE(inCh[i],
            "Unregistered lanes must remain NULL in inCh array");
    }
}

void test_inCh_bounds_safe_with_max_inputs() {
    // Verify the loop guard: lane * 2 + 1 < AUDIO_PIPELINE_MATRIX_SIZE
    // With MAX_INPUTS=8 and MATRIX_SIZE=16: lane 7 -> index 15 -> 15 < 16 -> OK
    int maxValidLane = (AUDIO_PIPELINE_MATRIX_SIZE / 2) - 1;  // 7
    TEST_ASSERT_TRUE_MESSAGE(
        maxValidLane < AUDIO_PIPELINE_MAX_INPUTS,
        "Highest valid lane must be within MAX_INPUTS");
    TEST_ASSERT_TRUE_MESSAGE(
        maxValidLane * 2 + 1 < AUDIO_PIPELINE_MATRIX_SIZE,
        "Highest valid lane's R channel must be within matrix size");
}

// ============================================================
// Group 6: Sink Channel Validation
// ============================================================

void test_set_sink_firstChannel_within_matrix_accepted() {
    // sinkSlot 0, firstChannel=0, channelCount=2: 0+2=2 <= 16
    TEST_ASSERT_TRUE(validate_set_sink(0, 0, 2));
    // sinkSlot 7, firstChannel=14, channelCount=2: 14+2=16 <= 16
    TEST_ASSERT_TRUE(validate_set_sink(7, 14, 2));
}

void test_set_sink_firstChannel_exceeds_matrix_rejected() {
    // firstChannel=15, channelCount=2: 15+2=17 > 16
    TEST_ASSERT_FALSE(validate_set_sink(0, 15, 2));
    // firstChannel=16, channelCount=1: 16+1=17 > 16
    TEST_ASSERT_FALSE(validate_set_sink(0, 16, 1));
}

void test_set_sink_channelCount_overflow_rejected() {
    // firstChannel=14, channelCount=4: 14+4=18 > 16
    TEST_ASSERT_FALSE(validate_set_sink(0, 14, 4));
}

void test_dispatch_clamps_chR_when_at_boundary() {
    // firstChannel=15, channelCount=2 -> chR would be 16 -> clamped to chL (15)
    // But this should be caught by set_sink validation first.
    // In dispatch, chL=15 < 16 so not skipped
    TEST_ASSERT_FALSE_MESSAGE(dispatch_would_skip(15, 2),
        "Dispatch should not skip chL=15 (valid); chR gets clamped");
}

// ===== Main =====

int main() {
    UNITY_BEGIN();

    // Group 1: Dimension invariants
    RUN_TEST(test_max_inputs_stereo_fits_matrix);
    RUN_TEST(test_max_sinks_stereo_fits_matrix);
    RUN_TEST(test_input_source_max_inputs_matches);

    // Group 2: Matrix gain bounds
    RUN_TEST(test_gain_valid_corner_indices);
    RUN_TEST(test_gain_negative_out_rejected);
    RUN_TEST(test_gain_out_at_boundary);
    RUN_TEST(test_gain_negative_in_rejected);
    RUN_TEST(test_gain_in_at_boundary);
    RUN_TEST(test_gain_both_at_max);

    // Group 3: firstChannel overflow
    RUN_TEST(test_firstChannel_0_valid);
    RUN_TEST(test_firstChannel_14_valid);
    RUN_TEST(test_firstChannel_15_mono_valid);
    RUN_TEST(test_firstChannel_16_overflow_skipped);

    // Group 4: Registration bounds
    RUN_TEST(test_set_sink_negative_slot_rejected);
    RUN_TEST(test_set_sink_at_max_rejected);
    RUN_TEST(test_set_source_negative_lane_rejected);
    RUN_TEST(test_set_source_at_max_rejected);

    // Group 5: inCh population
    RUN_TEST(test_inCh_all_8_lanes_populated);
    RUN_TEST(test_inCh_null_lane_stays_null);
    RUN_TEST(test_inCh_bounds_safe_with_max_inputs);

    // Group 6: Sink channel validation
    RUN_TEST(test_set_sink_firstChannel_within_matrix_accepted);
    RUN_TEST(test_set_sink_firstChannel_exceeds_matrix_rejected);
    RUN_TEST(test_set_sink_channelCount_overflow_rejected);
    RUN_TEST(test_dispatch_clamps_chR_when_at_boundary);

    return UNITY_END();
}
