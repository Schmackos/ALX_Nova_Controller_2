/**
 * test_input_phase.cpp
 *
 * Unit tests for the setInputPhase WS handler logic:
 * find-or-create DSP_POLARITY stage in the chain region of the input DSP.
 *
 * Covers:
 *   - Adding a new DSP_POLARITY stage when none exists (inverted=true)
 *   - Adding DSP_POLARITY with inverted=false (polarity restored)
 *   - Updating an existing DSP_POLARITY stage (no duplicate created)
 *   - Stage is placed in chain region (index >= DSP_PEQ_BANDS)
 *   - Stereo lane: both L and R channels get polarity stage
 *   - Independent polarity per lane
 *
 * The handler logic (from websocket_command.cpp setInputPhase):
 *   1. dsp_copy_active_to_inactive()
 *   2. For each channel in [lane*2, lane*2+1]:
 *      a. Scan chain region (stages[DSP_PEQ_BANDS..stageCount]) for DSP_POLARITY
 *      b. If found: update inverted flag + enable
 *      c. If not found: dsp_add_chain_stage(ch, DSP_POLARITY), then set inverted
 *   3. dsp_swap_config()
 *
 * Uses the same include pattern as test_dsp.cpp.
 */

#include <unity.h>
#include <math.h>
#include <string.h>

// ESP-DSP lite fallbacks (ANSI C implementations for native tests)
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
#include "../../src/thd_measurement.cpp"

// ===== Reimplemented find-or-create logic (mirrors websocket_command.cpp setInputPhase) =====

static void apply_input_phase(int lane, bool inverted) {
    int chL = lane * 2;
    int chR = lane * 2 + 1;
    if (lane < 0 || chR >= DSP_MAX_CHANNELS) return;

    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();

    for (int ch = chL; ch <= chR; ch++) {
        DspChannelConfig &chCfg = cfg->channels[ch];
        bool found = false;
        for (int s = DSP_PEQ_BANDS; s < chCfg.stageCount; s++) {
            if (chCfg.stages[s].type == DSP_POLARITY) {
                chCfg.stages[s].polarity.inverted = inverted;
                chCfg.stages[s].enabled = true;
                found = true;
                break;
            }
        }
        if (!found) {
            int idx = dsp_add_chain_stage(ch, DSP_POLARITY);
            if (idx >= 0) {
                cfg->channels[ch].stages[idx].polarity.inverted = inverted;
                cfg->channels[ch].stages[idx].enabled = true;
            }
        }
    }
    dsp_swap_config();
}

// ===== setUp / tearDown =====

void setUp(void) {
    dsp_init();
}

void tearDown(void) {}

// ===== Tests =====

// 1. No existing polarity stage: applying inverted=true creates DSP_POLARITY in chain region
void test_set_phase_creates_polarity_stage(void) {
    apply_input_phase(0, true);

    DspState *active = dsp_get_active_config();
    DspChannelConfig &chL = active->channels[0];

    // Find a DSP_POLARITY stage in the chain region
    bool found = false;
    for (int s = DSP_PEQ_BANDS; s < chL.stageCount; s++) {
        if (chL.stages[s].type == DSP_POLARITY) {
            TEST_ASSERT_TRUE(chL.stages[s].polarity.inverted);
            TEST_ASSERT_TRUE(chL.stages[s].enabled);
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

// 2. Polarity stage is in chain region (index >= DSP_PEQ_BANDS)
void test_polarity_stage_in_chain_region(void) {
    apply_input_phase(0, true);

    DspState *active = dsp_get_active_config();
    DspChannelConfig &chL = active->channels[0];

    for (int s = DSP_PEQ_BANDS; s < chL.stageCount; s++) {
        if (chL.stages[s].type == DSP_POLARITY) {
            TEST_ASSERT_GREATER_OR_EQUAL(DSP_PEQ_BANDS, s);
            return;
        }
    }
    TEST_FAIL_MESSAGE("DSP_POLARITY stage not found in chain region");
}

// 3. Stereo lane: both L (ch0) and R (ch1) get a DSP_POLARITY stage
void test_stereo_lane_both_channels_get_polarity(void) {
    apply_input_phase(0, true);

    DspState *active = dsp_get_active_config();

    for (int ch = 0; ch <= 1; ch++) {
        DspChannelConfig &chCfg = active->channels[ch];
        bool found = false;
        for (int s = DSP_PEQ_BANDS; s < chCfg.stageCount; s++) {
            if (chCfg.stages[s].type == DSP_POLARITY) {
                found = true;
                break;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(found, "DSP_POLARITY not found on channel");
    }
}

// 4. Applying inverted=false creates a polarity stage with inverted=false
void test_set_phase_inverted_false(void) {
    apply_input_phase(0, false);

    DspState *active = dsp_get_active_config();
    DspChannelConfig &chL = active->channels[0];

    for (int s = DSP_PEQ_BANDS; s < chL.stageCount; s++) {
        if (chL.stages[s].type == DSP_POLARITY) {
            TEST_ASSERT_FALSE(chL.stages[s].polarity.inverted);
            return;
        }
    }
    TEST_FAIL_MESSAGE("DSP_POLARITY stage not found");
}

// 5. Applying phase twice does not create a duplicate — stage count stays the same
void test_apply_phase_twice_no_duplicate(void) {
    apply_input_phase(0, true);

    DspState *active = dsp_get_active_config();
    int countAfterFirst = 0;
    for (int s = DSP_PEQ_BANDS; s < active->channels[0].stageCount; s++) {
        if (active->channels[0].stages[s].type == DSP_POLARITY) countAfterFirst++;
    }
    TEST_ASSERT_EQUAL(1, countAfterFirst);

    // Apply again with different polarity
    apply_input_phase(0, false);

    active = dsp_get_active_config();
    int countAfterSecond = 0;
    for (int s = DSP_PEQ_BANDS; s < active->channels[0].stageCount; s++) {
        if (active->channels[0].stages[s].type == DSP_POLARITY) countAfterSecond++;
    }
    TEST_ASSERT_EQUAL(1, countAfterSecond);

    // And the value was updated
    for (int s = DSP_PEQ_BANDS; s < active->channels[0].stageCount; s++) {
        if (active->channels[0].stages[s].type == DSP_POLARITY) {
            TEST_ASSERT_FALSE(active->channels[0].stages[s].polarity.inverted);
            return;
        }
    }
}

// 6. Lane 0 polarity does not affect lane 1 channels
void test_lane0_phase_independent_of_lane1(void) {
    apply_input_phase(0, true);

    DspState *active = dsp_get_active_config();

    // Lane 1 channels (ch2, ch3) should have no DSP_POLARITY stage
    for (int ch = 2; ch <= 3; ch++) {
        if (ch >= DSP_MAX_CHANNELS) break;
        DspChannelConfig &chCfg = active->channels[ch];
        for (int s = DSP_PEQ_BANDS; s < chCfg.stageCount; s++) {
            if (chCfg.stages[s].type == DSP_POLARITY) {
                TEST_FAIL_MESSAGE("DSP_POLARITY found on unrelated lane channel");
            }
        }
    }
}

// 7. Toggling: set inverted=true then inverted=false updates the same stage
void test_toggle_polarity_updates_existing_stage(void) {
    apply_input_phase(0, true);
    apply_input_phase(0, false);

    DspState *active = dsp_get_active_config();
    DspChannelConfig &chL = active->channels[0];

    int polarityCount = 0;
    bool lastInverted = true;
    for (int s = DSP_PEQ_BANDS; s < chL.stageCount; s++) {
        if (chL.stages[s].type == DSP_POLARITY) {
            polarityCount++;
            lastInverted = chL.stages[s].polarity.inverted;
        }
    }
    TEST_ASSERT_EQUAL(1, polarityCount);
    TEST_ASSERT_FALSE(lastInverted);
}

// 8. Out-of-range lane is a no-op (chR >= DSP_MAX_CHANNELS)
void test_out_of_range_lane_is_noop(void) {
    // Lane such that chR = lane*2+1 >= DSP_MAX_CHANNELS
    int badLane = DSP_MAX_CHANNELS;  // lane*2+1 would be 2*DSP_MAX_CHANNELS+1
    apply_input_phase(badLane, true);

    // No stages should have been added anywhere
    DspState *active = dsp_get_active_config();
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        for (int s = DSP_PEQ_BANDS; s < active->channels[ch].stageCount; s++) {
            if (active->channels[ch].stages[s].type == DSP_POLARITY) {
                TEST_FAIL_MESSAGE("DSP_POLARITY unexpectedly added by out-of-range lane");
            }
        }
    }
}

// ===== Main =====

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_set_phase_creates_polarity_stage);
    RUN_TEST(test_polarity_stage_in_chain_region);
    RUN_TEST(test_stereo_lane_both_channels_get_polarity);
    RUN_TEST(test_set_phase_inverted_false);
    RUN_TEST(test_apply_phase_twice_no_duplicate);
    RUN_TEST(test_lane0_phase_independent_of_lane1);
    RUN_TEST(test_toggle_polarity_updates_existing_stage);
    RUN_TEST(test_out_of_range_lane_is_noop);
    return UNITY_END();
}
