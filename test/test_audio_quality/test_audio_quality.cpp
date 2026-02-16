/**
 * @file test_audio_quality.cpp
 * @brief Unit tests for audio quality diagnostics module
 *
 * Tests cover:
 * - Initialization and state management
 * - Glitch detection (discontinuity, DC offset, dropout, overload)
 * - Timing histogram tracking
 * - Event correlation (DSP swap, WiFi, MQTT)
 * - Memory monitoring
 * - Statistics and reset behavior
 * - Integration scenarios
 *
 * Total: 33 tests
 */

#include <unity.h>
#include "audio_quality.h"
#include <cstring>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Mock millis() for native tests
static unsigned long mock_millis_value = 0;
extern "C" unsigned long millis() {
    return mock_millis_value;
}

void setUp(void) {
    // Reset mock time
    mock_millis_value = 1000;

    // Initialize audio quality module with defaults
    audio_quality_init();
    audio_quality_reset_stats();
}

void tearDown(void) {
    // Clean up after each test
}

// ============================================================================
// GROUP 1: Initialization & State (5 tests)
// ============================================================================

void test_init_sets_defaults(void) {
    audio_quality_init();

    TEST_ASSERT_FALSE(audio_quality_is_enabled());
    TEST_ASSERT_EQUAL_FLOAT(0.5f, audio_quality_get_threshold());
}

void test_enable_disable_transitions(void) {
    TEST_ASSERT_FALSE(audio_quality_is_enabled());

    audio_quality_enable(true);
    TEST_ASSERT_TRUE(audio_quality_is_enabled());

    audio_quality_enable(false);
    TEST_ASSERT_FALSE(audio_quality_is_enabled());
}

void test_threshold_validation_clamps(void) {
    // Below minimum (0.1)
    audio_quality_set_threshold(0.05f);
    TEST_ASSERT_EQUAL_FLOAT(0.1f, audio_quality_get_threshold());

    // Above maximum (1.0)
    audio_quality_set_threshold(1.5f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, audio_quality_get_threshold());

    // Negative
    audio_quality_set_threshold(-0.3f);
    TEST_ASSERT_EQUAL_FLOAT(0.1f, audio_quality_get_threshold());
}

void test_threshold_get_set_roundtrip(void) {
    audio_quality_set_threshold(0.3f);
    TEST_ASSERT_EQUAL_FLOAT(0.3f, audio_quality_get_threshold());

    audio_quality_set_threshold(0.75f);
    TEST_ASSERT_EQUAL_FLOAT(0.75f, audio_quality_get_threshold());

    audio_quality_set_threshold(1.0f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, audio_quality_get_threshold());
}

void test_multiple_init_calls_safe(void) {
    audio_quality_init();
    audio_quality_set_threshold(0.7f);
    audio_quality_enable(true);

    // Re-init should reset to defaults
    audio_quality_init();
    TEST_ASSERT_FALSE(audio_quality_is_enabled());
    TEST_ASSERT_EQUAL_FLOAT(0.5f, audio_quality_get_threshold());
}

// ============================================================================
// GROUP 2: Glitch Detection (8 tests)
// ============================================================================

void test_discontinuity_detection_large_jump(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.5f); // 50% full-scale jump threshold

    // Create buffer with large discontinuity
    int32_t buffer[256];
    for (int i = 0; i < 128; i++) {
        buffer[i] = 100000; // Low value
    }
    for (int i = 128; i < 256; i++) {
        buffer[i] = 0x7FFFFF00; // Jump to near full-scale (>50%)
    }

    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_GREATER_THAN(0, diag->glitchHistory.totalCount);
    TEST_ASSERT_EQUAL(GLITCH_TYPE_DISCONTINUITY, diag->glitchHistory.events[0].type);
    TEST_ASSERT_EQUAL(0, diag->glitchHistory.events[0].adcIndex);
}

void test_dc_offset_detection(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.5f);

    // Create buffer with sustained DC offset
    int32_t buffer[256];
    int32_t dc_offset = 0x40000000; // 50% of full-scale DC offset
    for (int i = 0; i < 256; i++) {
        buffer[i] = dc_offset + (i % 2 ? 1000 : -1000); // Small AC ripple on DC
    }

    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_GREATER_THAN(0, diag->glitchHistory.totalCount);
    TEST_ASSERT_EQUAL(GLITCH_TYPE_DC_OFFSET, diag->glitchHistory.events[0].type);
}

void test_dropout_detection_silent_samples(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.5f);

    // Create buffer with >50% near-zero samples (dropout)
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = (i < 200) ? 10 : 1000000; // 78% samples near zero
    }

    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_GREATER_THAN(0, diag->glitchHistory.totalCount);
    TEST_ASSERT_EQUAL(GLITCH_TYPE_DROPOUT, diag->glitchHistory.events[0].type);
}

void test_overload_detection_clipping(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.5f);

    // Create buffer with samples near full-scale (clipping)
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = 0x7FFFFF00; // >95% of full-scale
    }

    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_GREATER_THAN(0, diag->glitchHistory.totalCount);
    TEST_ASSERT_EQUAL(GLITCH_TYPE_OVERLOAD, diag->glitchHistory.events[0].type);
}

void test_below_threshold_no_false_positives(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.8f); // High threshold

    // Create buffer with small variations (below threshold)
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = 100000 + (i % 2 ? 5000 : -5000); // Small variation
    }

    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(0, diag->glitchHistory.totalCount);
}

void test_ring_buffer_wraps_after_32_events(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.3f);

    // Create buffer that will trigger glitches
    int32_t buffer[256];

    // Trigger 35 glitch events
    for (int event = 0; event < 35; event++) {
        for (int i = 0; i < 256; i++) {
            buffer[i] = (i == 0) ? 0x7FFFFF00 : 100000; // Discontinuity at start
        }
        audio_quality_scan_buffer(0, buffer, 128, 1000);
        mock_millis_value += 10;
    }

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(35, diag->glitchHistory.totalCount);
    // Ring buffer size is 32, should wrap
    TEST_ASSERT_EQUAL(3, diag->glitchHistory.writePos); // (35 % 32) = 3
}

void test_per_adc_and_per_channel_tracking(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.5f);

    // Create buffers with glitches on different ADCs/channels
    int32_t buffer1[256];
    int32_t buffer2[256];

    // ADC 0, discontinuity in left channel (even indices)
    for (int i = 0; i < 256; i++) {
        buffer1[i] = (i < 128 && i % 2 == 0) ? 100000 : 0x7FFFFF00;
    }

    // ADC 1, discontinuity in right channel (odd indices)
    for (int i = 0; i < 256; i++) {
        buffer2[i] = (i < 128 && i % 2 == 1) ? 100000 : 0x7FFFFF00;
    }

    audio_quality_scan_buffer(0, buffer1, 128, 1000);
    audio_quality_scan_buffer(1, buffer2, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_GREATER_OR_EQUAL(2, diag->glitchHistory.totalCount);

    // Check that ADC indices are tracked
    bool found_adc0 = false, found_adc1 = false;
    for (int i = 0; i < diag->glitchHistory.totalCount && i < 32; i++) {
        if (diag->glitchHistory.events[i].adcIndex == 0) found_adc0 = true;
        if (diag->glitchHistory.events[i].adcIndex == 1) found_adc1 = true;
    }
    TEST_ASSERT_TRUE(found_adc0);
    TEST_ASSERT_TRUE(found_adc1);
}

void test_glitch_type_enum_to_string(void) {
    TEST_ASSERT_EQUAL_STRING("NONE", audio_quality_glitch_type_to_string(GLITCH_TYPE_NONE));
    TEST_ASSERT_EQUAL_STRING("DISCONTINUITY", audio_quality_glitch_type_to_string(GLITCH_TYPE_DISCONTINUITY));
    TEST_ASSERT_EQUAL_STRING("DC_OFFSET", audio_quality_glitch_type_to_string(GLITCH_TYPE_DC_OFFSET));
    TEST_ASSERT_EQUAL_STRING("DROPOUT", audio_quality_glitch_type_to_string(GLITCH_TYPE_DROPOUT));
    TEST_ASSERT_EQUAL_STRING("OVERLOAD", audio_quality_glitch_type_to_string(GLITCH_TYPE_OVERLOAD));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", audio_quality_glitch_type_to_string((GlitchType)99));
}

// ============================================================================
// GROUP 3: Timing Histogram (5 tests)
// ============================================================================

void test_timing_buckets_increment_correctly(void) {
    audio_quality_enable(true);

    int32_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    // Scan with various latencies
    audio_quality_scan_buffer(0, buffer, 128, 500);   // 0.5ms → bucket 0
    audio_quality_scan_buffer(0, buffer, 128, 2500);  // 2.5ms → bucket 2
    audio_quality_scan_buffer(0, buffer, 128, 8000);  // 8.0ms → bucket 8
    audio_quality_scan_buffer(0, buffer, 128, 15000); // 15.0ms → bucket 15

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(1, diag->timingHistogram.buckets[0]);
    TEST_ASSERT_EQUAL(1, diag->timingHistogram.buckets[2]);
    TEST_ASSERT_EQUAL(1, diag->timingHistogram.buckets[8]);
    TEST_ASSERT_EQUAL(1, diag->timingHistogram.buckets[15]);
}

void test_timing_overflow_bucket_over_20ms(void) {
    audio_quality_enable(true);

    int32_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    // Scan with latency >20ms
    audio_quality_scan_buffer(0, buffer, 128, 25000); // 25ms
    audio_quality_scan_buffer(0, buffer, 128, 50000); // 50ms

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(2, diag->timingHistogram.overflowCount);
}

void test_timing_average_latency_calculation(void) {
    audio_quality_enable(true);

    int32_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    // Scan with known latencies
    audio_quality_scan_buffer(0, buffer, 128, 1000); // 1ms
    audio_quality_scan_buffer(0, buffer, 128, 3000); // 3ms
    audio_quality_scan_buffer(0, buffer, 128, 5000); // 5ms

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(3, diag->timingHistogram.sampleCount);
    // Average should be (1000 + 3000 + 5000) / 3 = 3000 µs
    TEST_ASSERT_EQUAL_UINT32(3000, diag->timingHistogram.avgLatencyUs);
}

void test_timing_max_latency_tracking(void) {
    audio_quality_enable(true);

    int32_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    audio_quality_scan_buffer(0, buffer, 128, 2000);  // 2ms
    audio_quality_scan_buffer(0, buffer, 128, 10000); // 10ms (new max)
    audio_quality_scan_buffer(0, buffer, 128, 5000);  // 5ms

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL_UINT32(10000, diag->timingHistogram.maxLatencyUs);
}

void test_timing_sample_count_increments(void) {
    audio_quality_enable(true);

    int32_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    for (int i = 0; i < 10; i++) {
        audio_quality_scan_buffer(0, buffer, 128, 1000);
    }

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(10, diag->timingHistogram.sampleCount);
}

// ============================================================================
// GROUP 4: Event Correlation (6 tests)
// ============================================================================

void test_dsp_swap_correlation_within_100ms(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.3f);

    // Mark DSP swap event
    mock_millis_value = 5000;
    audio_quality_mark_event("dsp_swap");

    // Create glitch within 100ms
    mock_millis_value = 5050; // 50ms after DSP swap
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = (i == 0) ? 0x7FFFFF00 : 100000; // Discontinuity
    }
    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_TRUE(diag->glitchHistory.events[0].correlation.dspSwap);
}

void test_wifi_event_correlation_within_100ms(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.3f);

    // Mark WiFi event
    mock_millis_value = 10000;
    audio_quality_mark_event("wifi_connected");

    // Create glitch within 100ms
    mock_millis_value = 10080; // 80ms after WiFi event
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = (i == 0) ? 0x7FFFFF00 : 100000;
    }
    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_TRUE(diag->glitchHistory.events[0].correlation.wifiEvent);
}

void test_mqtt_event_correlation_within_100ms(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.3f);

    // Mark MQTT event (use "wifi_disconnected" to trigger MQTT flag for testing)
    mock_millis_value = 15000;
    audio_quality_mark_event("mqtt_connected");

    // Create glitch within 100ms
    mock_millis_value = 15030; // 30ms after MQTT event
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = (i == 0) ? 0x7FFFFF00 : 100000;
    }
    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_TRUE(diag->glitchHistory.events[0].correlation.mqttEvent);
}

void test_event_over_100ms_no_correlation(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.3f);

    // Mark DSP swap event
    mock_millis_value = 20000;
    audio_quality_mark_event("dsp_swap");

    // Create glitch >100ms later
    mock_millis_value = 20150; // 150ms after DSP swap
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = (i == 0) ? 0x7FFFFF00 : 100000;
    }
    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_FALSE(diag->glitchHistory.events[0].correlation.dspSwap);
}

void test_multiple_events_correlate_correctly(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.3f);

    // Mark multiple events
    mock_millis_value = 25000;
    audio_quality_mark_event("dsp_swap");
    mock_millis_value = 25020;
    audio_quality_mark_event("wifi_connected");

    // Create glitch within 100ms of both
    mock_millis_value = 25050; // 50ms from DSP, 30ms from WiFi
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = (i == 0) ? 0x7FFFFF00 : 100000;
    }
    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_TRUE(diag->glitchHistory.events[0].correlation.dspSwap);
    TEST_ASSERT_TRUE(diag->glitchHistory.events[0].correlation.wifiEvent);
}

void test_correlation_flags_clear_when_no_recent_glitches(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.3f);

    // Mark event
    mock_millis_value = 30000;
    audio_quality_mark_event("dsp_swap");

    // Wait >100ms, then scan clean buffer
    mock_millis_value = 30200;
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = 100000; // Clean signal
    }
    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(0, diag->glitchHistory.totalCount);
}

// ============================================================================
// GROUP 5: Memory Monitoring (3 tests)
// ============================================================================

void test_memory_snapshots_ring_buffer(void) {
    audio_quality_enable(true);

    // Take 5 snapshots
    for (int i = 0; i < 5; i++) {
        audio_quality_update_memory();
        mock_millis_value += 1000;
    }

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(5, diag->memoryHistory.writePos);
}

void test_memory_write_position_wraps_correctly(void) {
    audio_quality_enable(true);

    // Take 65 snapshots (ring buffer size is 60)
    for (int i = 0; i < 65; i++) {
        audio_quality_update_memory();
        mock_millis_value += 1000;
    }

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(5, diag->memoryHistory.writePos); // 65 % 60 = 5
}

void test_memory_timestamps_increment(void) {
    audio_quality_enable(true);

    mock_millis_value = 1000;
    audio_quality_update_memory();
    const AudioQualityDiag* diag1 = audio_quality_get_diagnostics();
    unsigned long first_timestamp = diag1->memoryHistory.snapshots[0].timestamp;

    mock_millis_value = 2000;
    audio_quality_update_memory();
    const AudioQualityDiag* diag2 = audio_quality_get_diagnostics();
    unsigned long second_timestamp = diag2->memoryHistory.snapshots[1].timestamp;

    TEST_ASSERT_EQUAL_UINT32(1000, first_timestamp);
    TEST_ASSERT_EQUAL_UINT32(2000, second_timestamp);
}

// ============================================================================
// GROUP 6: Statistics & Reset (3 tests)
// ============================================================================

void test_reset_clears_all_counters(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.3f);

    // Generate some activity
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = (i == 0) ? 0x7FFFFF00 : 100000;
    }
    audio_quality_scan_buffer(0, buffer, 128, 1000);
    audio_quality_scan_buffer(0, buffer, 128, 5000);

    // Verify activity
    const AudioQualityDiag* diag1 = audio_quality_get_diagnostics();
    TEST_ASSERT_GREATER_THAN(0, diag1->glitchHistory.totalCount);
    TEST_ASSERT_GREATER_THAN(0, diag1->timingHistogram.sampleCount);

    // Reset and verify cleared
    audio_quality_reset_stats();
    const AudioQualityDiag* diag2 = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(0, diag2->glitchHistory.totalCount);
    TEST_ASSERT_EQUAL(0, diag2->glitchHistory.writePos);
    TEST_ASSERT_EQUAL(0, diag2->timingHistogram.sampleCount);
}

void test_reset_preserves_settings(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.7f);

    // Reset
    audio_quality_reset_stats();

    // Verify settings preserved
    TEST_ASSERT_TRUE(audio_quality_is_enabled());
    TEST_ASSERT_EQUAL_FLOAT(0.7f, audio_quality_get_threshold());
}

void test_last_minute_counter_decay(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.3f);

    // Generate glitch at T=0
    mock_millis_value = 1000;
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = (i == 0) ? 0x7FFFFF00 : 100000;
    }
    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag1 = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(1, diag1->glitchHistory.totalCount);
    TEST_ASSERT_EQUAL(1, diag1->glitchHistory.lastMinuteCount);

    // Wait >60 seconds, scan again
    mock_millis_value = 62000;
    memset(buffer, 0, sizeof(buffer));
    audio_quality_scan_buffer(0, buffer, 128, 1000);

    const AudioQualityDiag* diag2 = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(1, diag2->glitchHistory.totalCount); // Total unchanged
    TEST_ASSERT_EQUAL(0, diag2->glitchHistory.lastMinuteCount); // Last-minute decayed
}

// ============================================================================
// GROUP 7: Integration (3 tests)
// ============================================================================

void test_disabled_state_no_processing_overhead(void) {
    audio_quality_enable(false);

    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = (i == 0) ? 0x7FFFFF00 : 100000; // Would trigger glitch if enabled
    }

    // Scan multiple times while disabled
    for (int i = 0; i < 100; i++) {
        audio_quality_scan_buffer(0, buffer, 128, 1000);
    }

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    TEST_ASSERT_EQUAL(0, diag->glitchHistory.totalCount);
    TEST_ASSERT_EQUAL(0, diag->timingHistogram.sampleCount);
}

void test_real_audio_buffer_scan(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.5f);

    // Synthesize realistic audio data: 1kHz sine wave at -6dBFS
    int32_t buffer[512]; // 256 stereo frames
    const float amplitude = 0.5f * 0x7FFFFFFF; // -6dBFS
    const float freq = 1000.0f;
    const float sampleRate = 48000.0f;

    for (int i = 0; i < 512; i++) {
        float phase = 2.0f * M_PI * freq * (i / 2) / sampleRate;
        buffer[i] = (int32_t)(amplitude * sinf(phase));
    }

    // Scan buffer
    audio_quality_scan_buffer(0, buffer, 256, 1500);

    const AudioQualityDiag* diag = audio_quality_get_diagnostics();
    // Should not detect glitches in clean sine wave
    TEST_ASSERT_EQUAL(0, diag->glitchHistory.totalCount);
    TEST_ASSERT_EQUAL(1, diag->timingHistogram.sampleCount);
    TEST_ASSERT_EQUAL_UINT32(1500, diag->timingHistogram.avgLatencyUs);
}

void test_full_diagnostics_struct_retrieval(void) {
    audio_quality_enable(true);
    audio_quality_set_threshold(0.4f);

    // Generate some activity
    int32_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = 0x7FFFFF00; // Overload
    }
    audio_quality_scan_buffer(0, buffer, 128, 2000);
    audio_quality_mark_event("dsp_swap");
    audio_quality_update_memory();

    // Retrieve full diagnostics
    const AudioQualityDiag* diag = audio_quality_get_diagnostics();

    TEST_ASSERT_NOT_NULL(diag);
    TEST_ASSERT_GREATER_THAN(0, diag->glitchHistory.totalCount);
    TEST_ASSERT_EQUAL(1, diag->timingHistogram.sampleCount);
    TEST_ASSERT_GREATER_THAN(0, diag->memoryHistory.writePos);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Group 1: Initialization & State (5 tests)
    RUN_TEST(test_init_sets_defaults);
    RUN_TEST(test_enable_disable_transitions);
    RUN_TEST(test_threshold_validation_clamps);
    RUN_TEST(test_threshold_get_set_roundtrip);
    RUN_TEST(test_multiple_init_calls_safe);

    // Group 2: Glitch Detection (8 tests)
    RUN_TEST(test_discontinuity_detection_large_jump);
    RUN_TEST(test_dc_offset_detection);
    RUN_TEST(test_dropout_detection_silent_samples);
    RUN_TEST(test_overload_detection_clipping);
    RUN_TEST(test_below_threshold_no_false_positives);
    RUN_TEST(test_ring_buffer_wraps_after_32_events);
    RUN_TEST(test_per_adc_and_per_channel_tracking);
    RUN_TEST(test_glitch_type_enum_to_string);

    // Group 3: Timing Histogram (5 tests)
    RUN_TEST(test_timing_buckets_increment_correctly);
    RUN_TEST(test_timing_overflow_bucket_over_20ms);
    RUN_TEST(test_timing_average_latency_calculation);
    RUN_TEST(test_timing_max_latency_tracking);
    RUN_TEST(test_timing_sample_count_increments);

    // Group 4: Event Correlation (6 tests)
    RUN_TEST(test_dsp_swap_correlation_within_100ms);
    RUN_TEST(test_wifi_event_correlation_within_100ms);
    RUN_TEST(test_mqtt_event_correlation_within_100ms);
    RUN_TEST(test_event_over_100ms_no_correlation);
    RUN_TEST(test_multiple_events_correlate_correctly);
    RUN_TEST(test_correlation_flags_clear_when_no_recent_glitches);

    // Group 5: Memory Monitoring (3 tests)
    RUN_TEST(test_memory_snapshots_ring_buffer);
    RUN_TEST(test_memory_write_position_wraps_correctly);
    RUN_TEST(test_memory_timestamps_increment);

    // Group 6: Statistics & Reset (3 tests)
    RUN_TEST(test_reset_clears_all_counters);
    RUN_TEST(test_reset_preserves_settings);
    RUN_TEST(test_last_minute_counter_decay);

    // Group 7: Integration (3 tests)
    RUN_TEST(test_disabled_state_no_processing_overhead);
    RUN_TEST(test_real_audio_buffer_scan);
    RUN_TEST(test_full_diagnostics_struct_retrieval);

    return UNITY_END();
}
