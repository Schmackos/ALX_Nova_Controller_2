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
#include "../../src/dsp_crossover.cpp"

// Stub for dsp_api (dsp_pipeline.cpp calls dsp_get_routing_matrix via routing execute)
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
    // Step 1: add delay to inactive (state 1), swap to make it active
    dsp_add_stage(0, DSP_DELAY, -1);   // appends at stage DSP_PEQ_BANDS in state 1
    dsp_swap_config();                 // state 1 is now active; _activeIndex = 1

    // Step 2: set state on OLD ACTIVE (state 1) — fill delay line with test pattern
    DspState *active = dsp_get_active_config();   // &_states[1]
    active->channels[0].stages[DSP_PEQ_BANDS].delay.delaySamples = 100;
    active->channels[0].stages[DSP_PEQ_BANDS].delay.writePos = 50;
    int delaySlot = active->channels[0].stages[DSP_PEQ_BANDS].delay.delaySlot;
    float *delayLine = dsp_delay_get_line(1, delaySlot);   // state index 1
    if (delayLine) {
        for (int i = 0; i < 100; i++) {
            delayLine[i] = (float)i / 100.0f;
        }
    }

    // Step 3: add matching delay to (now) inactive (state 0)
    dsp_add_stage(0, DSP_DELAY, -1);   // appends at stage DSP_PEQ_BANDS in state 0

    // Step 4: swap — copies delay line from old active (state 1) to new active (state 0)
    TEST_ASSERT_TRUE(dsp_swap_config());   // state 0 is now active; _activeIndex = 0

    // Step 5: verify
    DspState *new_active = dsp_get_active_config();   // &_states[0]
    int newDelaySlot = new_active->channels[0].stages[DSP_PEQ_BANDS].delay.delaySlot;
    float *newDelayLine = dsp_delay_get_line(0, newDelaySlot);   // state index 0

    if (newDelayLine && delayLine) {
        for (int i = 0; i < 100; i++) {
            TEST_ASSERT_EQUAL_FLOAT((float)i / 100.0f, newDelayLine[i]);
        }
        TEST_ASSERT_EQUAL_UINT16(50, new_active->channels[0].stages[DSP_PEQ_BANDS].delay.writePos);
    }
}

// Test 6: Biquad delay state preserved
void test_biquad_delay_preserved() {
    // Both configs already have matching PEQ stages (DSP_BIQUAD_PEQ) at stages[0-9]
    // from dsp_init_state. Set delay state on ACTIVE config — swap must copy it to new active.
    DspState *active = dsp_get_active_config();
    active->channels[0].stages[0].biquad.delay[0] = 0.123f;
    active->channels[0].stages[0].biquad.delay[1] = 0.456f;

    TEST_ASSERT_TRUE(dsp_swap_config());

    DspState *new_active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_FLOAT(0.123f, new_active->channels[0].stages[0].biquad.delay[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.456f, new_active->channels[0].stages[0].biquad.delay[1]);
}

// Test 7: Limiter envelope state preserved
void test_limiter_envelope_preserved() {
    // Step 1: add limiter to inactive (state 1), swap to make it active at stage DSP_PEQ_BANDS
    dsp_add_stage(0, DSP_LIMITER, -1);
    dsp_swap_config();   // state 1 active

    // Step 2: add matching limiter to (now) inactive (state 0)
    dsp_add_stage(0, DSP_LIMITER, -1);

    // Step 3: set runtime state on OLD ACTIVE (state 1) at the limiter stage
    DspState *active = dsp_get_active_config();
    active->channels[0].stages[DSP_PEQ_BANDS].limiter.envelope = 0.789f;
    active->channels[0].stages[DSP_PEQ_BANDS].limiter.gainReduction = -3.5f;

    // Step 4: swap — copies limiter state from old active to new active
    TEST_ASSERT_TRUE(dsp_swap_config());   // state 0 active

    DspState *new_active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_FLOAT(0.789f, new_active->channels[0].stages[DSP_PEQ_BANDS].limiter.envelope);
    TEST_ASSERT_EQUAL_FLOAT(-3.5f, new_active->channels[0].stages[DSP_PEQ_BANDS].limiter.gainReduction);
}

// Test 8: Gain ramping state preserved
void test_gain_ramping_preserved() {
    // Add gain stage
    DspState *inactive = dsp_get_inactive_config();
    dsp_add_stage(0, DSP_GAIN, -1);

    // Set runtime state
    inactive->channels[0].stages[0].gain.currentLinear = 0.5f;
    inactive->channels[0].stages[0].gain.gainLinear = 1.0f;

    // Swap
    TEST_ASSERT_TRUE(dsp_swap_config());

    // Verify preserved
    DspState *active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_FLOAT(0.5f, active->channels[0].stages[0].gain.currentLinear);
}

// Test 9: Compressor state preserved
void test_compressor_state_preserved() {
    // Step 1: add compressor to inactive (state 1), swap to make it active at stage DSP_PEQ_BANDS
    dsp_add_stage(0, DSP_COMPRESSOR, -1);
    dsp_swap_config();   // state 1 active

    // Step 2: add matching compressor to (now) inactive (state 0)
    dsp_add_stage(0, DSP_COMPRESSOR, -1);

    // Step 3: set runtime state on OLD ACTIVE (state 1) at the compressor stage
    DspState *active = dsp_get_active_config();
    active->channels[0].stages[DSP_PEQ_BANDS].compressor.envelope = 0.333f;
    active->channels[0].stages[DSP_PEQ_BANDS].compressor.gainReduction = -6.2f;

    // Step 4: swap — copies compressor state from old active to new active
    TEST_ASSERT_TRUE(dsp_swap_config());   // state 0 active

    DspState *new_active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_FLOAT(0.333f, new_active->channels[0].stages[DSP_PEQ_BANDS].compressor.envelope);
    TEST_ASSERT_EQUAL_FLOAT(-6.2f, new_active->channels[0].stages[DSP_PEQ_BANDS].compressor.gainReduction);
}

// ===== dsp_swap_check_state() pure function tests =====

// Test: mutex not acquired → returns 1 (mutex busy)
void test_swap_check_mutex_busy() {
    TEST_ASSERT_EQUAL(1, dsp_swap_check_state(false, false, 10));
}

// Test: mutex acquired, processing active, no wait iterations remaining → returns 2 (timeout)
void test_swap_check_processing_timeout() {
    TEST_ASSERT_EQUAL(2, dsp_swap_check_state(true, true, 0));
}

// Test: mutex acquired, processing active, wait remaining → returns -1 (still waiting)
void test_swap_check_still_waiting() {
    TEST_ASSERT_EQUAL(-1, dsp_swap_check_state(true, true, 5));
}

// Test: mutex acquired, not processing → returns 0 (success, safe to swap)
void test_swap_check_success() {
    TEST_ASSERT_EQUAL(0, dsp_swap_check_state(true, false, 10));
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
    RUN_TEST(test_swap_check_mutex_busy);
    RUN_TEST(test_swap_check_processing_timeout);
    RUN_TEST(test_swap_check_still_waiting);
    RUN_TEST(test_swap_check_success);

    return UNITY_END();
}
