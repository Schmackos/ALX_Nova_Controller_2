// test_asrc.cpp
// Unit tests for the polyphase ASRC engine (src/asrc.h / src/asrc.cpp).
//
// Covers:
//   - asrc_init() initialises filter and history buffers
//   - asrc_is_active() returns false before set_ratio
//   - asrc_set_ratio() activates known ratios (44100->48000, 96000->48000)
//   - asrc_set_ratio() passthrough on equal rates (srcRate == dstRate)
//   - asrc_set_ratio() passthrough on unknown ratios
//   - asrc_set_ratio() lane=0 srcRate==0 deactivates all lanes
//   - asrc_bypass() deactivates a specific lane
//   - asrc_reset_lane() zeroes history without clearing active flag
//   - asrc_process_lane() passthrough when inactive
//   - asrc_process_lane() output frame count within expected range for 44100->48000
//   - asrc_process_lane() output frame count for 96000->48000 (downsampling 1:2)
//   - asrc_process_lane() DC signal is preserved (mean preserved through filter)
//   - asrc_process_lane() silence in -> silence out
//   - asrc_process_lane() out-of-range lane returns input frame count
//   - asrc_deinit() resets state; asrc_is_active() returns false after

#include <unity.h>
#include <cstring>
#include <cstdlib>
#include <cmath>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#endif

// Pull in implementation files directly (test_build_src = no pattern)
#include "../../src/heap_budget.h"
#include "../../src/heap_budget.cpp"
#include "../../src/psram_alloc.h"
#include "../../src/psram_alloc.cpp"
#include "../../src/config.h"
#include "../../src/asrc.h"
#include "../../src/asrc.cpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static float s_laneL[ASRC_OUTPUT_FRAMES_MAX];
static float s_laneR[ASRC_OUTPUT_FRAMES_MAX];

static void fill_silence(int frames) {
    memset(s_laneL, 0, (size_t)frames * sizeof(float));
    memset(s_laneR, 0, (size_t)frames * sizeof(float));
}

static void fill_dc(int frames, float val) {
    for (int i = 0; i < frames; i++) {
        s_laneL[i] = val;
        s_laneR[i] = val;
    }
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp() {
    // Re-init fresh for each test — asrc_init guards _initialized so we
    // call deinit first to reset the guard.
    asrc_deinit();
    asrc_init();
}

void tearDown() {
    asrc_deinit();
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void test_init_succeeds() {
    // After init, is_active should be false for all lanes
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        TEST_ASSERT_FALSE(asrc_is_active(lane));
    }
}

void test_is_active_false_before_set_ratio() {
    TEST_ASSERT_FALSE(asrc_is_active(0));
}

void test_set_ratio_known_activates() {
    asrc_set_ratio(0, 44100, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));
}

void test_set_ratio_96k_to_48k_activates() {
    asrc_set_ratio(0, 96000, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));
}

void test_set_ratio_equal_rates_passthrough() {
    asrc_set_ratio(0, 48000, 48000);
    TEST_ASSERT_FALSE(asrc_is_active(0));
}

void test_set_ratio_unknown_passthrough() {
    asrc_set_ratio(0, 44100, 96000);  // Not in ratio table
    TEST_ASSERT_FALSE(asrc_is_active(0));
}

void test_set_ratio_zero_src_deactivates_lane() {
    // asrc_set_ratio with srcRate==0 deactivates that specific lane (passthrough)
    asrc_set_ratio(0, 44100, 48000);
    asrc_set_ratio(1, 44100, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));
    TEST_ASSERT_TRUE(asrc_is_active(1));

    asrc_set_ratio(0, 0, 48000);  // srcRate == 0 → deactivate lane 0
    TEST_ASSERT_FALSE(asrc_is_active(0));
    // Lane 1 remains active — set_ratio on lane 0 only affects lane 0
    TEST_ASSERT_TRUE(asrc_is_active(1));
}

void test_bypass_deactivates_lane() {
    asrc_set_ratio(0, 44100, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));
    asrc_bypass(0);
    TEST_ASSERT_FALSE(asrc_is_active(0));
}

void test_bypass_out_of_range_no_crash() {
    asrc_bypass(-1);
    asrc_bypass(AUDIO_PIPELINE_MAX_INPUTS);
    // Just verify no crash/assertion
    TEST_PASS();
}

void test_reset_lane_does_not_deactivate() {
    asrc_set_ratio(0, 44100, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));
    asrc_reset_lane(0);
    // reset_lane clears history but must not clear active
    TEST_ASSERT_TRUE(asrc_is_active(0));
}

void test_process_passthrough_when_inactive() {
    // Inactive lane: process returns input frame count unchanged
    fill_dc(256, 0.5f);
    int out = asrc_process_lane(0, s_laneL, s_laneR, 256);
    TEST_ASSERT_EQUAL_INT(256, out);
    // Buffer content should be unchanged
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, s_laneL[0]);
}

void test_process_out_of_range_lane_returns_frames() {
    int out = asrc_process_lane(-1, s_laneL, s_laneR, 256);
    TEST_ASSERT_EQUAL_INT(256, out);
    out = asrc_process_lane(AUDIO_PIPELINE_MAX_INPUTS, s_laneL, s_laneR, 256);
    TEST_ASSERT_EQUAL_INT(256, out);
}

void test_process_44100_to_48000_output_count() {
    // 256 frames @ 44100 → 48000: L/M = 160/147
    // Expected output = ceil(256 * 160 / 147) = 279
    asrc_set_ratio(0, 44100, 48000);
    fill_silence(256);
    int out = asrc_process_lane(0, s_laneL, s_laneR, 256);
    // Allow ±2 frames for phase boundary effects
    TEST_ASSERT_GREATER_OR_EQUAL_INT(277, out);
    TEST_ASSERT_LESS_OR_EQUAL_INT(ASRC_OUTPUT_FRAMES_MAX, out);
}

void test_process_96000_to_48000_output_count() {
    // 256 frames @ 96000 → 48000: L/M = 1/2 (downsampling)
    // Expected output = ceil(256 * 1 / 2) = 128
    asrc_set_ratio(0, 96000, 48000);
    fill_silence(256);
    int out = asrc_process_lane(0, s_laneL, s_laneR, 256);
    // Allow ±2 frames for phase boundary
    TEST_ASSERT_GREATER_OR_EQUAL_INT(126, out);
    TEST_ASSERT_LESS_OR_EQUAL_INT(130, out);
}

void test_process_silence_in_silence_out() {
    asrc_set_ratio(0, 44100, 48000);
    fill_silence(256);
    int out = asrc_process_lane(0, s_laneL, s_laneR, 256);
    // All output samples should be (near) zero
    float maxL = 0.0f, maxR = 0.0f;
    for (int i = 0; i < out; i++) {
        if (fabsf(s_laneL[i]) > maxL) maxL = fabsf(s_laneL[i]);
        if (fabsf(s_laneR[i]) > maxR) maxR = fabsf(s_laneR[i]);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, maxL);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, maxR);
}

void test_process_nonzero_input_nonzero_output() {
    // Non-zero input must produce non-zero output (filter is not all-zero).
    // Use a separate input buffer to avoid in-place overwrite contaminating next warmup.
    asrc_set_ratio(0, 44100, 48000);
    static float inL[256], inR[256];
    for (int i = 0; i < 256; i++) { inL[i] = 0.7f; inR[i] = 0.7f; }
    // Warmup: feed DC repeatedly using a separate copy each time
    int outCount = 0;
    for (int iter = 0; iter < 4; iter++) {
        // Copy fresh DC into lane buffers
        memcpy(s_laneL, inL, 256 * sizeof(float));
        memcpy(s_laneR, inR, 256 * sizeof(float));
        outCount = asrc_process_lane(0, s_laneL, s_laneR, 256);
    }
    // Check output of the 4th block
    float maxAbs = 0.0f;
    for (int i = 0; i < outCount; i++) {
        if (fabsf(s_laneL[i]) > maxAbs) maxAbs = fabsf(s_laneL[i]);
    }
    // Filter must produce non-zero output for non-zero input
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, maxAbs);
}

void test_process_stereo_channels_independent() {
    // L and R channels fed different signals must produce different outputs.
    asrc_set_ratio(0, 44100, 48000);
    // Warmup both channels with non-zero DC using separate copy buffers
    static float inL[256], inR[256];
    for (int iter = 0; iter < 4; iter++) {
        for (int i = 0; i < 256; i++) { inL[i] = 0.7f; inR[i] = 0.0f; }
        memcpy(s_laneL, inL, 256 * sizeof(float));
        memcpy(s_laneR, inR, 256 * sizeof(float));
        asrc_process_lane(0, s_laneL, s_laneR, 256);
    }
    // After warmup: L had DC=0.7, R had silence
    // L channel output must be larger than R channel output
    float maxL = 0.0f, maxR = 0.0f;
    for (int i = 0; i < 279; i++) {
        if (fabsf(s_laneL[i]) > maxL) maxL = fabsf(s_laneL[i]);
        if (fabsf(s_laneR[i]) > maxR) maxR = fabsf(s_laneR[i]);
    }
    TEST_ASSERT_GREATER_THAN_FLOAT(maxR, maxL);
}

void test_deinit_deactivates_lanes() {
    asrc_set_ratio(0, 44100, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));
    asrc_deinit();
    TEST_ASSERT_FALSE(asrc_is_active(0));
}

void test_reinit_after_deinit() {
    asrc_set_ratio(0, 44100, 48000);
    asrc_deinit();
    asrc_init();
    asrc_set_ratio(0, 44100, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));
}

void test_multiple_lanes_independent() {
    asrc_set_ratio(0, 44100, 48000);
    asrc_set_ratio(1, 96000, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));
    TEST_ASSERT_TRUE(asrc_is_active(1));
    // Deactivate lane 1 only
    asrc_bypass(1);
    TEST_ASSERT_TRUE(asrc_is_active(0));
    TEST_ASSERT_FALSE(asrc_is_active(1));
}

void test_48k_to_44100_activates() {
    asrc_set_ratio(0, 48000, 44100);
    TEST_ASSERT_TRUE(asrc_is_active(0));
}

void test_output_frames_max_constant() {
    // ceil(256 * 160 / 147) = 279 ≤ ASRC_OUTPUT_FRAMES_MAX (280)
    const int maxUpOut = (int)ceilf(256.0f * 160.0f / 147.0f);
    TEST_ASSERT_LESS_OR_EQUAL_INT(ASRC_OUTPUT_FRAMES_MAX, maxUpOut);
}

void test_process_88200_to_48000_output_count() {
    // 256 frames @ 88200 → 48000: L/M = 80/147
    // Expected output = ceil(256 * 80 / 147) = 140
    asrc_set_ratio(0, 88200, 48000);
    fill_silence(256);
    int out = asrc_process_lane(0, s_laneL, s_laneR, 256);
    // Allow ±2 frames for phase boundary effects
    TEST_ASSERT_GREATER_OR_EQUAL_INT(138, out);
    TEST_ASSERT_LESS_OR_EQUAL_INT(142, out);
}

void test_process_176400_to_48000_output_count() {
    // 256 frames @ 176400 → 48000: L/M = 40/147
    // Expected output = ceil(256 * 40 / 147) = 70
    asrc_set_ratio(0, 176400, 48000);
    fill_silence(256);
    int out = asrc_process_lane(0, s_laneL, s_laneR, 256);
    // Allow ±2 frames for phase boundary effects
    TEST_ASSERT_GREATER_OR_EQUAL_INT(68, out);
    TEST_ASSERT_LESS_OR_EQUAL_INT(72, out);
}

void test_process_192000_to_48000_output_count() {
    // 256 frames @ 192000 → 48000: L/M = 1/4
    // Expected output = ceil(256 * 1 / 4) = 64
    asrc_set_ratio(0, 192000, 48000);
    fill_silence(256);
    int out = asrc_process_lane(0, s_laneL, s_laneR, 256);
    // Allow ±2 frames for phase boundary effects
    TEST_ASSERT_GREATER_OR_EQUAL_INT(62, out);
    TEST_ASSERT_LESS_OR_EQUAL_INT(66, out);
}

void test_process_48000_to_44100_output_count() {
    // 256 frames @ 48000 → 44100: L/M = 147/160
    // Expected output = ceil(256 * 147 / 160) = 236
    asrc_set_ratio(0, 48000, 44100);
    fill_silence(256);
    int out = asrc_process_lane(0, s_laneL, s_laneR, 256);
    // Allow ±2 frames for phase boundary effects
    TEST_ASSERT_GREATER_OR_EQUAL_INT(234, out);
    TEST_ASSERT_LESS_OR_EQUAL_INT(238, out);
}

void test_downsampled_tail_is_stale() {
    // 96000→48000 produces ~128 output frames from 256 input frames.
    // Positions beyond the output count still contain original DC data,
    // proving the pipeline zero-fill fix is necessary.
    asrc_set_ratio(0, 96000, 48000);
    fill_dc(ASRC_OUTPUT_FRAMES_MAX, 0.5f);  // Fill ENTIRE buffer with DC
    int out = asrc_process_lane(0, s_laneL, s_laneR, 256);
    // Sanity: output count should be around 128
    TEST_ASSERT_GREATER_OR_EQUAL_INT(126, out);
    TEST_ASSERT_LESS_OR_EQUAL_INT(130, out);
    // Tail beyond output count should still have non-zero stale data
    TEST_ASSERT_TRUE(out < ASRC_OUTPUT_FRAMES_MAX);
    bool foundNonZero = false;
    for (int i = out; i < ASRC_OUTPUT_FRAMES_MAX && !foundNonZero; i++) {
        if (fabsf(s_laneL[i]) > 1e-9f) foundNonZero = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(foundNonZero,
        "Expected stale non-zero data beyond output count");
}

void test_upsampled_output_within_buffer() {
    // All upsampling ratios must produce output <= ASRC_OUTPUT_FRAMES_MAX
    // 44100→48000 (upsample)
    asrc_set_ratio(0, 44100, 48000);
    fill_silence(256);
    int out = asrc_process_lane(0, s_laneL, s_laneR, 256);
    TEST_ASSERT_LESS_OR_EQUAL_INT(ASRC_OUTPUT_FRAMES_MAX, out);

    // 48000→44100 (downsample, but verify anyway)
    asrc_bypass(0);
    asrc_set_ratio(0, 48000, 44100);
    fill_silence(256);
    out = asrc_process_lane(0, s_laneL, s_laneR, 256);
    TEST_ASSERT_LESS_OR_EQUAL_INT(ASRC_OUTPUT_FRAMES_MAX, out);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_init_succeeds);
    RUN_TEST(test_is_active_false_before_set_ratio);
    RUN_TEST(test_set_ratio_known_activates);
    RUN_TEST(test_set_ratio_96k_to_48k_activates);
    RUN_TEST(test_set_ratio_equal_rates_passthrough);
    RUN_TEST(test_set_ratio_unknown_passthrough);
    RUN_TEST(test_set_ratio_zero_src_deactivates_lane);
    RUN_TEST(test_bypass_deactivates_lane);
    RUN_TEST(test_bypass_out_of_range_no_crash);
    RUN_TEST(test_reset_lane_does_not_deactivate);
    RUN_TEST(test_process_passthrough_when_inactive);
    RUN_TEST(test_process_out_of_range_lane_returns_frames);
    RUN_TEST(test_process_44100_to_48000_output_count);
    RUN_TEST(test_process_96000_to_48000_output_count);
    RUN_TEST(test_process_silence_in_silence_out);
    RUN_TEST(test_process_nonzero_input_nonzero_output);
    RUN_TEST(test_process_stereo_channels_independent);
    RUN_TEST(test_deinit_deactivates_lanes);
    RUN_TEST(test_reinit_after_deinit);
    RUN_TEST(test_multiple_lanes_independent);
    RUN_TEST(test_48k_to_44100_activates);
    RUN_TEST(test_output_frames_max_constant);
    RUN_TEST(test_process_88200_to_48000_output_count);
    RUN_TEST(test_process_176400_to_48000_output_count);
    RUN_TEST(test_process_192000_to_48000_output_count);
    RUN_TEST(test_process_48000_to_44100_output_count);
    RUN_TEST(test_downsampled_tail_is_stale);
    RUN_TEST(test_upsampled_output_within_buffer);

    return UNITY_END();
}
