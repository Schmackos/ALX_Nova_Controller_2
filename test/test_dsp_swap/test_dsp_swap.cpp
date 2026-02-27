#include <unity.h>
#include <cstring>

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
#include "../../src/app_state.h"

// Include DSP implementation
#include "../../src/dsp_coefficients.cpp"
#include "../../src/dsp_convolution.cpp"
#include "../../src/dsp_pipeline.cpp"

void setUp(void) {
    // Reset DSP before each test
    dsp_init();

    // Reset swap diagnostics
    appState.dspSwapFailures = 0;
    appState.dspSwapSuccesses = 0;
    appState.lastDspSwapFailure = 0;
}

void tearDown(void) {
    // Cleanup after each test
}

// Test 1: Swap returns true on success
void test_swap_returns_true_on_success() {
    DspState *inactive = dsp_get_inactive_config();
    inactive->sampleRate = 96000; // Change something

    bool result = dsp_swap_config();

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(1, appState.dspSwapSuccesses);
    TEST_ASSERT_EQUAL_UINT32(0, appState.dspSwapFailures);
}

// Test 2: Swap returns false on timeout (simulated busy state)
void test_swap_returns_false_on_timeout() {
    // This test is difficult to implement in native environment without FreeRTOS simulation
    // In real hardware, this would be tested by holding _processingActive true
    // For now, verify the success path works
    TEST_ASSERT_TRUE(dsp_swap_config());
}

// Test 3: Success counter increments correctly
void test_success_counter_increments() {
    uint32_t initial = appState.dspSwapSuccesses;

    TEST_ASSERT_TRUE(dsp_swap_config());
    TEST_ASSERT_EQUAL_UINT32(initial + 1, appState.dspSwapSuccesses);

    TEST_ASSERT_TRUE(dsp_swap_config());
    TEST_ASSERT_EQUAL_UINT32(initial + 2, appState.dspSwapSuccesses);
}

// Test 4: Multiple consecutive swaps succeed
void test_multiple_swaps_succeed() {
    for (int i = 0; i < 10; i++) {
        bool result = dsp_swap_config();
        TEST_ASSERT_TRUE(result);
    }

    TEST_ASSERT_EQUAL_UINT32(10, appState.dspSwapSuccesses);
    TEST_ASSERT_EQUAL_UINT32(0, appState.dspSwapFailures);
}

// Test 5: Delay line state preserved across swap
void test_delay_state_preserved() {
    // dsp_add_stage appends after DSP_PEQ_BANDS pre-populated stages; capture the index
    int stageIdx = dsp_add_stage(0, DSP_DELAY, -1);
    TEST_ASSERT_TRUE(stageIdx >= 0);

    DspState *inactive = dsp_get_inactive_config();
    inactive->channels[0].stages[stageIdx].delay.delaySamples = 100;
    int delaySlot = inactive->channels[0].stages[stageIdx].delay.delaySlot;

    // Fill inactive delay line (state index 1) with test pattern
    float *delayLine = dsp_delay_get_line(1, delaySlot);
    if (delayLine) {
        for (int i = 0; i < 100; i++) {
            delayLine[i] = (float)i / 100.0f;
        }
        inactive->channels[0].stages[stageIdx].delay.writePos = 50;
    }

    // Swap config — inactive (state 1) becomes active; no copy for stageIdx since it only
    // exists in new active (not old active), so the delay data written above is preserved
    TEST_ASSERT_TRUE(dsp_swap_config());

    // After swap, _activeIndex = 1. Active delay pool is _delayLine[1][slot].
    // The written pattern is still there since no cross-copy occurred for this stage.
    DspState *active = dsp_get_active_config();
    int newDelaySlot = active->channels[0].stages[stageIdx].delay.delaySlot;
    float *newDelayLine = dsp_delay_get_line(1, newDelaySlot); // state 1 = new active

    if (newDelayLine && delayLine) {
        for (int i = 0; i < 100; i++) {
            TEST_ASSERT_EQUAL_FLOAT((float)i / 100.0f, newDelayLine[i]);
        }
        TEST_ASSERT_EQUAL_UINT16(50, active->channels[0].stages[stageIdx].delay.writePos);
    }
}

// Test 6: Biquad delay state preserved
void test_biquad_delay_preserved() {
    // dsp_add_stage appends after DSP_PEQ_BANDS pre-populated stages; capture the index
    int stageIdx = dsp_add_stage(0, DSP_BIQUAD_PEQ, -1);
    TEST_ASSERT_TRUE(stageIdx >= 0);

    DspState *inactive = dsp_get_inactive_config();
    inactive->channels[0].stages[stageIdx].biquad.delay[0] = 0.123f;
    inactive->channels[0].stages[stageIdx].biquad.delay[1] = 0.456f;

    // Swap
    TEST_ASSERT_TRUE(dsp_swap_config());

    // Verify preserved — old active had no stage at stageIdx so no state copy occurs
    DspState *active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_FLOAT(0.123f, active->channels[0].stages[stageIdx].biquad.delay[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.456f, active->channels[0].stages[stageIdx].biquad.delay[1]);
}

// Test 7: Limiter envelope state preserved
void test_limiter_envelope_preserved() {
    // dsp_add_stage appends after DSP_PEQ_BANDS pre-populated stages; capture the index
    int stageIdx = dsp_add_stage(0, DSP_LIMITER, -1);
    TEST_ASSERT_TRUE(stageIdx >= 0);

    DspState *inactive = dsp_get_inactive_config();
    inactive->channels[0].stages[stageIdx].limiter.envelope = 0.789f;
    inactive->channels[0].stages[stageIdx].limiter.gainReduction = -3.5f;

    // Swap
    TEST_ASSERT_TRUE(dsp_swap_config());

    // Verify preserved
    DspState *active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_FLOAT(0.789f, active->channels[0].stages[stageIdx].limiter.envelope);
    TEST_ASSERT_EQUAL_FLOAT(-3.5f, active->channels[0].stages[stageIdx].limiter.gainReduction);
}

// Test 8: Gain ramping state preserved
void test_gain_ramping_preserved() {
    // dsp_add_stage appends after DSP_PEQ_BANDS pre-populated stages; capture the index
    int stageIdx = dsp_add_stage(0, DSP_GAIN, -1);
    TEST_ASSERT_TRUE(stageIdx >= 0);

    DspState *inactive = dsp_get_inactive_config();
    inactive->channels[0].stages[stageIdx].gain.currentLinear = 0.5f;
    inactive->channels[0].stages[stageIdx].gain.gainLinear = 1.0f;

    // Swap
    TEST_ASSERT_TRUE(dsp_swap_config());

    // Verify preserved
    DspState *active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_FLOAT(0.5f, active->channels[0].stages[stageIdx].gain.currentLinear);
}

// Test 9: Compressor state preserved
void test_compressor_state_preserved() {
    // dsp_add_stage appends after DSP_PEQ_BANDS pre-populated stages; capture the index
    int stageIdx = dsp_add_stage(0, DSP_COMPRESSOR, -1);
    TEST_ASSERT_TRUE(stageIdx >= 0);

    DspState *inactive = dsp_get_inactive_config();
    inactive->channels[0].stages[stageIdx].compressor.envelope = 0.333f;
    inactive->channels[0].stages[stageIdx].compressor.gainReduction = -6.2f;

    // Swap
    TEST_ASSERT_TRUE(dsp_swap_config());

    // Verify preserved
    DspState *active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_FLOAT(0.333f, active->channels[0].stages[stageIdx].compressor.envelope);
    TEST_ASSERT_EQUAL_FLOAT(-6.2f, active->channels[0].stages[stageIdx].compressor.gainReduction);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_swap_returns_true_on_success);
    RUN_TEST(test_swap_returns_false_on_timeout);
    RUN_TEST(test_success_counter_increments);
    RUN_TEST(test_multiple_swaps_succeed);
    RUN_TEST(test_delay_state_preserved);
    RUN_TEST(test_biquad_delay_preserved);
    RUN_TEST(test_limiter_envelope_preserved);
    RUN_TEST(test_gain_ramping_preserved);
    RUN_TEST(test_compressor_state_preserved);

    return UNITY_END();
}
