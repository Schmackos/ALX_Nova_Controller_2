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
    appState.dsp.swapFailures = 0;
    appState.dsp.swapSuccesses = 0;
    appState.dsp.lastSwapFailure = 0;
}

void tearDown(void) {
    // Ensure _processingActive is cleared so a failed test cannot poison subsequent tests
    _processingActive = false;
}

// Test 1: Swap returns true on success
void test_swap_returns_true_on_success() {
    DspState *inactive = dsp_get_inactive_config();
    inactive->sampleRate = 96000; // Change something

    bool result = dsp_swap_config();

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(1, appState.dsp.swapSuccesses);
    TEST_ASSERT_EQUAL_UINT32(0, appState.dsp.swapFailures);
}

// Test 2: Swap returns false on timeout (simulated busy state)
void test_swap_returns_false_on_timeout() {
    // Hold _processingActive true so the swap wait loop spins to timeout.
    // In native builds, vTaskDelay is compiled out, so the 100-iteration loop
    // completes instantly and hits the timeout path.
    _processingActive = true;

    // Set mock time so lastSwapFailure gets a non-zero timestamp
    ArduinoMock::mockMillis = 5000;

    uint32_t failuresBefore = appState.dsp.swapFailures;
    uint32_t successesBefore = appState.dsp.swapSuccesses;

    bool result = dsp_swap_config();

    TEST_ASSERT_FALSE_MESSAGE(result, "swap should return false when processing is stuck active");
    TEST_ASSERT_EQUAL_UINT32(failuresBefore + 1, appState.dsp.swapFailures);
    TEST_ASSERT_EQUAL_UINT32(successesBefore, appState.dsp.swapSuccesses);
    TEST_ASSERT_EQUAL_UINT32(5000, appState.dsp.lastSwapFailure);

    // Cleanup: release the processing lock so subsequent tests are unaffected
    _processingActive = false;
}

// Test 3: Success counter increments correctly
void test_success_counter_increments() {
    uint32_t initial = appState.dsp.swapSuccesses;

    TEST_ASSERT_TRUE(dsp_swap_config());
    TEST_ASSERT_EQUAL_UINT32(initial + 1, appState.dsp.swapSuccesses);

    TEST_ASSERT_TRUE(dsp_swap_config());
    TEST_ASSERT_EQUAL_UINT32(initial + 2, appState.dsp.swapSuccesses);
}

// Test 4: Multiple consecutive swaps succeed
void test_multiple_swaps_succeed() {
    for (int i = 0; i < 10; i++) {
        bool result = dsp_swap_config();
        TEST_ASSERT_TRUE(result);
    }

    TEST_ASSERT_EQUAL_UINT32(10, appState.dsp.swapSuccesses);
    TEST_ASSERT_EQUAL_UINT32(0, appState.dsp.swapFailures);
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

// Test 10: Successful swap does not increment failure counter
void test_swap_success_does_not_increment_failures() {
    appState.dsp.swapFailures = 0;
    appState.dsp.swapSuccesses = 0;

    dsp_swap_config();
    dsp_swap_config();
    dsp_swap_config();

    TEST_ASSERT_EQUAL_UINT32(3, appState.dsp.swapSuccesses);
    TEST_ASSERT_EQUAL_UINT32(0, appState.dsp.swapFailures);
}

// Test 11: dsp_log_swap_failure helper does not touch counters
void test_log_swap_failure_does_not_increment_counter() {
    appState.dsp.swapFailures = 0;
    appState.dsp.lastSwapFailure = 0;

    dsp_log_swap_failure("Test");

    TEST_ASSERT_EQUAL_UINT32(0, appState.dsp.swapFailures);
    TEST_ASSERT_EQUAL_UINT32(0, appState.dsp.lastSwapFailure);
}

// Test 12: Coefficient morphing state is set when biquad coefficients change across swap
void test_swap_coeff_morphing_state() {
    // Use PEQ band 0 (already exists on both states after dsp_init) to test morphing.
    // After dsp_init, both states have 10 PEQ bands with default coefficients [1,0,0,0,0].
    // We set different coefficients on each state via direct assignment and verify
    // that the swap detects the difference and initiates coefficient morphing.

    const int band = 0; // PEQ band index

    // Known coefficient sets (arbitrary but distinctly different)
    const float coeffsA[5] = { 1.05f, -1.90f, 0.86f, -1.90f, 0.91f };
    const float coeffsB[5] = { 1.12f, -1.75f, 0.70f, -1.75f, 0.82f };

    // --- Step 1: Set coefficients A on the inactive config's band 0 ---
    DspState *inactive1 = dsp_get_inactive_config();
    for (int c = 0; c < 5; c++) {
        inactive1->channels[0].stages[band].biquad.coeffs[c] = coeffsA[c];
    }

    // Swap: the swap compares old active (state 0, default [1,0,0,0,0]) vs
    // new active (state 1, coeffsA). Coefficients differ -> morphing triggered.
    TEST_ASSERT_TRUE(dsp_swap_config());
    uint32_t swapCount1 = appState.dsp.swapSuccesses;

    // After first swap: morphing should be active on the now-active config
    DspState *activeAfterFirst = dsp_get_active_config();
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(
        64, activeAfterFirst->channels[0].stages[band].biquad.morphRemaining,
        "First swap should trigger morphing (default -> coeffsA)");

    // The active coeffs should be the OLD default coefficients (morph starts from old)
    TEST_ASSERT_EQUAL_FLOAT(1.0f, activeAfterFirst->channels[0].stages[band].biquad.coeffs[0]);
    // The target should be coeffsA
    for (int c = 0; c < 5; c++) {
        TEST_ASSERT_EQUAL_FLOAT(coeffsA[c], activeAfterFirst->channels[0].stages[band].biquad.targetCoeffs[c]);
    }

    // --- Step 2: Set coefficients B on the (new) inactive config's band 0 ---
    DspState *inactive2 = dsp_get_inactive_config();
    for (int c = 0; c < 5; c++) {
        inactive2->channels[0].stages[band].biquad.coeffs[c] = coeffsB[c];
    }

    // Second swap: old active has coeffs starting as default [1,0,0,0,0] (morph was
    // in progress but coeffs[] still holds the "from" values). New active has coeffsB.
    TEST_ASSERT_TRUE(dsp_swap_config());
    TEST_ASSERT_EQUAL_UINT32(swapCount1 + 1, appState.dsp.swapSuccesses);

    // After second swap: morphing should be active (coeffsA-from-old -> coeffsB)
    DspState *active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(
        64, active->channels[0].stages[band].biquad.morphRemaining,
        "Second swap should trigger morphing (old active coeffs -> coeffsB)");

    // The target coefficients should be coeffsB
    for (int c = 0; c < 5; c++) {
        TEST_ASSERT_EQUAL_FLOAT(coeffsB[c], active->channels[0].stages[band].biquad.targetCoeffs[c]);
    }
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
    RUN_TEST(test_swap_success_does_not_increment_failures);
    RUN_TEST(test_log_swap_failure_does_not_increment_counter);
    RUN_TEST(test_swap_coeff_morphing_state);

    return UNITY_END();
}
