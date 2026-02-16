#include <unity.h>
#include "dsp_pipeline.h"
#include "app_state.h"
#include <cmath>
#include <cstring>

#define SAMPLE_RATE 48000
#define TEST_FRAMES 256

// Mock AppState for testing
AppState& appState = AppState::getInstance();

void setUp(void) {
    // Reset DSP state before each test
    dsp_init();

    // Reset emergency limiter settings to defaults
    appState.emergencyLimiterEnabled = true;
    appState.emergencyLimiterThresholdDb = -0.5f;
}

void tearDown(void) {
    // Cleanup after each test
}

// Test 1: Limiter disabled → passthrough (no processing)
void test_limiter_disabled_passthrough() {
    appState.emergencyLimiterEnabled = false;

    // Create test signal at 0 dBFS (max amplitude)
    int32_t buffer[TEST_FRAMES * 2]; // Stereo
    for (int i = 0; i < TEST_FRAMES; i++) {
        buffer[i * 2] = 8388607;     // Max positive (L)
        buffer[i * 2 + 1] = -8388607; // Max negative (R)
    }

    // Process buffer
    dsp_process_buffer(buffer, TEST_FRAMES, 0);

    // Verify no limiting occurred (passthrough)
    DspMetrics metrics = dsp_get_metrics();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, metrics.emergencyLimiterGrDb);
    TEST_ASSERT_FALSE(metrics.emergencyLimiterActive);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.emergencyLimiterTriggers);
}

// Test 2: Signal below threshold → no gain reduction
void test_signal_below_threshold_no_gr() {
    appState.emergencyLimiterEnabled = true;
    appState.emergencyLimiterThresholdDb = -3.0f;

    // Create test signal at -6 dBFS (well below threshold)
    float amplitude = 8388607.0f * powf(10.0f, -6.0f / 20.0f); // -6 dBFS
    int32_t buffer[TEST_FRAMES * 2];
    for (int i = 0; i < TEST_FRAMES; i++) {
        buffer[i * 2] = (int32_t)amplitude;
        buffer[i * 2 + 1] = (int32_t)amplitude;
    }

    // Process buffer
    dsp_process_buffer(buffer, TEST_FRAMES, 0);

    // Verify no limiting (signal is below threshold)
    DspMetrics metrics = dsp_get_metrics();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, metrics.emergencyLimiterGrDb);
    TEST_ASSERT_FALSE(metrics.emergencyLimiterActive);
}

// Test 3: Signal above threshold → gain reduction applied
void test_signal_above_threshold_gr_applied() {
    appState.emergencyLimiterEnabled = true;
    appState.emergencyLimiterThresholdDb = -3.0f;

    // Create test signal at 0 dBFS (above -3 dBFS threshold)
    int32_t buffer[TEST_FRAMES * 2];
    for (int i = 0; i < TEST_FRAMES; i++) {
        buffer[i * 2] = 8388607;     // Max amplitude
        buffer[i * 2 + 1] = 8388607;
    }

    // Process buffer
    dsp_process_buffer(buffer, TEST_FRAMES, 0);

    // Verify limiting occurred (GR should be negative)
    DspMetrics metrics = dsp_get_metrics();
    TEST_ASSERT_TRUE(metrics.emergencyLimiterGrDb < -0.5f); // Significant GR
    TEST_ASSERT_TRUE(metrics.emergencyLimiterActive);
    TEST_ASSERT_GREATER_THAN(0, metrics.emergencyLimiterTriggers);
}

// Test 4: Lookahead buffer prevents overshoot
void test_lookahead_prevents_overshoot() {
    appState.emergencyLimiterEnabled = true;
    appState.emergencyLimiterThresholdDb = -0.5f;

    float threshold_linear = powf(10.0f, -0.5f / 20.0f);
    int32_t max_sample = (int32_t)(threshold_linear * 8388607.0f);

    // Create sudden peak signal
    int32_t buffer[TEST_FRAMES * 2];
    memset(buffer, 0, sizeof(buffer));
    buffer[100 * 2] = 8388607;     // Sudden peak at frame 100
    buffer[100 * 2 + 1] = 8388607;

    // Process buffer
    dsp_process_buffer(buffer, TEST_FRAMES, 0);

    // Check that output doesn't exceed threshold
    for (int i = 0; i < TEST_FRAMES * 2; i++) {
        TEST_ASSERT_TRUE(abs(buffer[i]) <= max_sample);
    }
}

// Test 5: Fast attack time (< 0.2ms)
void test_fast_attack_time() {
    appState.emergencyLimiterEnabled = true;
    appState.emergencyLimiterThresholdDb = -6.0f;

    // Process multiple buffers with constant peak signal
    int32_t buffer[TEST_FRAMES * 2];
    for (int i = 0; i < TEST_FRAMES; i++) {
        buffer[i * 2] = 8388607;
        buffer[i * 2 + 1] = 8388607;
    }

    // First buffer should already show GR (attack < 1 buffer period)
    dsp_process_buffer(buffer, TEST_FRAMES, 0);
    DspMetrics metrics1 = dsp_get_metrics();
    TEST_ASSERT_TRUE(metrics1.emergencyLimiterGrDb < -0.5f);

    // Second buffer should show similar or more GR
    dsp_process_buffer(buffer, TEST_FRAMES, 0);
    DspMetrics metrics2 = dsp_get_metrics();
    TEST_ASSERT_TRUE(metrics2.emergencyLimiterGrDb <= metrics1.emergencyLimiterGrDb);
}

// Test 6: Release time ~100ms
void test_release_time() {
    appState.emergencyLimiterEnabled = true;
    appState.emergencyLimiterThresholdDb = -3.0f;

    // Process buffer with peak
    int32_t buffer[TEST_FRAMES * 2];
    for (int i = 0; i < TEST_FRAMES; i++) {
        buffer[i * 2] = 8388607;
        buffer[i * 2 + 1] = 8388607;
    }
    dsp_process_buffer(buffer, TEST_FRAMES, 0);
    DspMetrics metrics_peak = dsp_get_metrics();
    TEST_ASSERT_TRUE(metrics_peak.emergencyLimiterGrDb < -1.0f);

    // Process buffers with silence (release should occur)
    memset(buffer, 0, sizeof(buffer));
    for (int i = 0; i < 10; i++) { // ~53ms worth of buffers
        dsp_process_buffer(buffer, TEST_FRAMES, 0);
    }
    DspMetrics metrics_mid = dsp_get_metrics();

    // GR should be recovering (less negative) but not fully released
    TEST_ASSERT_TRUE(metrics_mid.emergencyLimiterGrDb > metrics_peak.emergencyLimiterGrDb);
    TEST_ASSERT_TRUE(metrics_mid.emergencyLimiterGrDb < -0.1f);

    // Process more buffers for full release (~200ms total)
    for (int i = 0; i < 30; i++) {
        dsp_process_buffer(buffer, TEST_FRAMES, 0);
    }
    DspMetrics metrics_final = dsp_get_metrics();

    // GR should be near zero
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 0.0f, metrics_final.emergencyLimiterGrDb);
    TEST_ASSERT_FALSE(metrics_final.emergencyLimiterActive);
}

// Test 7: Trigger counter increments correctly
void test_trigger_counter_increments() {
    appState.emergencyLimiterEnabled = true;
    appState.emergencyLimiterThresholdDb = -3.0f;

    DspMetrics metrics_initial = dsp_get_metrics();
    uint32_t initial_count = metrics_initial.emergencyLimiterTriggers;

    // Process buffer with peak
    int32_t buffer[TEST_FRAMES * 2];
    for (int i = 0; i < TEST_FRAMES; i++) {
        buffer[i * 2] = 8388607;
        buffer[i * 2 + 1] = 8388607;
    }
    dsp_process_buffer(buffer, TEST_FRAMES, 0);

    DspMetrics metrics_after = dsp_get_metrics();
    TEST_ASSERT_EQUAL_UINT32(initial_count + 1, metrics_after.emergencyLimiterTriggers);
}

// Test 8: Metrics updated correctly
void test_metrics_updated() {
    appState.emergencyLimiterEnabled = true;
    appState.emergencyLimiterThresholdDb = -1.0f;

    // Process buffer with signal above threshold
    int32_t buffer[TEST_FRAMES * 2];
    for (int i = 0; i < TEST_FRAMES; i++) {
        buffer[i * 2] = 8388607;
        buffer[i * 2 + 1] = 8388607;
    }
    dsp_process_buffer(buffer, TEST_FRAMES, 0);

    DspMetrics metrics = dsp_get_metrics();

    // Verify all metrics are populated
    TEST_ASSERT_TRUE(metrics.emergencyLimiterGrDb < 0.0f); // Negative GR
    TEST_ASSERT_TRUE(metrics.emergencyLimiterActive);
    TEST_ASSERT_GREATER_THAN(0, metrics.emergencyLimiterTriggers);
}

// Test 9: Threshold edge cases
void test_threshold_edge_cases() {
    // Test at -6 dBFS (minimum allowed)
    appState.emergencyLimiterThresholdDb = -6.0f;
    int32_t buffer[TEST_FRAMES * 2];
    for (int i = 0; i < TEST_FRAMES; i++) {
        buffer[i * 2] = 8388607;
        buffer[i * 2 + 1] = 8388607;
    }
    dsp_process_buffer(buffer, TEST_FRAMES, 0);
    DspMetrics metrics1 = dsp_get_metrics();
    TEST_ASSERT_TRUE(metrics1.emergencyLimiterGrDb < 0.0f);

    // Test at 0 dBFS (maximum allowed)
    dsp_init(); // Reset state
    appState.emergencyLimiterThresholdDb = 0.0f;
    dsp_process_buffer(buffer, TEST_FRAMES, 0);
    DspMetrics metrics2 = dsp_get_metrics();
    // At 0 dBFS threshold, max signal should not trigger (or minimal GR)
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, metrics2.emergencyLimiterGrDb);
}

// Test 10: Multi-channel independence (stereo)
void test_multichannel_independence() {
    appState.emergencyLimiterEnabled = true;
    appState.emergencyLimiterThresholdDb = -3.0f;

    // Create asymmetric signal (L channel above threshold, R channel below)
    int32_t buffer[TEST_FRAMES * 2];
    float below_threshold = 8388607.0f * powf(10.0f, -6.0f / 20.0f);
    for (int i = 0; i < TEST_FRAMES; i++) {
        buffer[i * 2] = 8388607;                    // L: 0 dBFS (above -3)
        buffer[i * 2 + 1] = (int32_t)below_threshold; // R: -6 dBFS (below -3)
    }

    dsp_process_buffer(buffer, TEST_FRAMES, 0);

    // Verify limiting occurred (peak detection is max of both channels)
    DspMetrics metrics = dsp_get_metrics();
    TEST_ASSERT_TRUE(metrics.emergencyLimiterActive);
    TEST_ASSERT_TRUE(metrics.emergencyLimiterGrDb < -1.0f);

    // Verify both channels are limited together (lookahead buffer is per-channel but GR is shared)
    float max_allowed = 8388607.0f * powf(10.0f, -3.0f / 20.0f);
    for (int i = 0; i < TEST_FRAMES * 2; i++) {
        TEST_ASSERT_TRUE(abs(buffer[i]) <= (int32_t)(max_allowed * 1.1f)); // 10% tolerance for envelope follower
    }
}

void setup() {
    UNITY_BEGIN();

    RUN_TEST(test_limiter_disabled_passthrough);
    RUN_TEST(test_signal_below_threshold_no_gr);
    RUN_TEST(test_signal_above_threshold_gr_applied);
    RUN_TEST(test_lookahead_prevents_overshoot);
    RUN_TEST(test_fast_attack_time);
    RUN_TEST(test_release_time);
    RUN_TEST(test_trigger_counter_increments);
    RUN_TEST(test_metrics_updated);
    RUN_TEST(test_threshold_edge_cases);
    RUN_TEST(test_multichannel_independence);

    UNITY_END();
}

void loop() {
    // Unity tests run once in setup()
}
