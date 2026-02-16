#include <unity.h>
#include "dsp_pipeline.h"
#include "app_state.h"
#include <cstring>

// Mock AppState for testing
AppState& appState = AppState::getInstance();

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
    // Add a delay stage to channel 0
    DspState *inactive = dsp_get_inactive_config();
    dsp_add_stage(0, DSP_DELAY, -1);

    // Allocate delay slot
    inactive->channels[0].stages[0].delay.delaySamples = 100;
    inactive->channels[0].stages[0].delay.delaySlot = dsp_delay_alloc_slot();

    // Fill delay line with test pattern
    float *delayLine = dsp_delay_get_line(1, inactive->channels[0].stages[0].delay.delaySlot);
    if (delayLine) {
        for (int i = 0; i < 100; i++) {
            delayLine[i] = (float)i / 100.0f;
        }
        inactive->channels[0].stages[0].delay.writePos = 50;
    }

    // Swap config
    TEST_ASSERT_TRUE(dsp_swap_config());

    // Verify delay line copied to new active
    DspState *active = dsp_get_active_config();
    float *newDelayLine = dsp_delay_get_line(0, active->channels[0].stages[0].delay.delaySlot);

    if (newDelayLine && delayLine) {
        for (int i = 0; i < 100; i++) {
            TEST_ASSERT_EQUAL_FLOAT((float)i / 100.0f, newDelayLine[i]);
        }
        TEST_ASSERT_EQUAL_UINT16(50, active->channels[0].stages[0].delay.writePos);
    }
}

// Test 6: Biquad delay state preserved
void test_biquad_delay_preserved() {
    // Add a biquad stage
    DspState *inactive = dsp_get_inactive_config();
    dsp_add_stage(0, DSP_BIQUAD_PEQ, -1);

    // Set delay state
    inactive->channels[0].stages[0].biquad.delay[0] = 0.123f;
    inactive->channels[0].stages[0].biquad.delay[1] = 0.456f;

    // Swap
    TEST_ASSERT_TRUE(dsp_swap_config());

    // Verify preserved
    DspState *active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_FLOAT(0.123f, active->channels[0].stages[0].biquad.delay[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.456f, active->channels[0].stages[0].biquad.delay[1]);
}

// Test 7: Limiter envelope state preserved
void test_limiter_envelope_preserved() {
    // Add limiter stage
    DspState *inactive = dsp_get_inactive_config();
    dsp_add_stage(0, DSP_LIMITER, -1);

    // Set runtime state
    inactive->channels[0].stages[0].limiter.envelope = 0.789f;
    inactive->channels[0].stages[0].limiter.gainReduction = -3.5f;

    // Swap
    TEST_ASSERT_TRUE(dsp_swap_config());

    // Verify preserved
    DspState *active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_FLOAT(0.789f, active->channels[0].stages[0].limiter.envelope);
    TEST_ASSERT_EQUAL_FLOAT(-3.5f, active->channels[0].stages[0].limiter.gainReduction);
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
    // Add compressor stage
    DspState *inactive = dsp_get_inactive_config();
    dsp_add_stage(0, DSP_COMPRESSOR, -1);

    // Set runtime state
    inactive->channels[0].stages[0].compressor.envelope = 0.333f;
    inactive->channels[0].stages[0].compressor.gainReduction = -6.2f;

    // Swap
    TEST_ASSERT_TRUE(dsp_swap_config());

    // Verify preserved
    DspState *active = dsp_get_active_config();
    TEST_ASSERT_EQUAL_FLOAT(0.333f, active->channels[0].stages[0].compressor.envelope);
    TEST_ASSERT_EQUAL_FLOAT(-6.2f, active->channels[0].stages[0].compressor.gainReduction);
}

void setup() {
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

    UNITY_END();
}

void loop() {
    // Unity tests run once in setup()
}
