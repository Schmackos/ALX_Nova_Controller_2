/**
 * test_input_gain.cpp
 *
 * Unit tests for input gain application on AudioInputSource.
 *
 * Covers:
 *   - dB-to-linear conversion used by setInputGain WS handler
 *   - gainLinear field is written correctly for common dB values
 *   - Boundary: 0 dB -> linear 1.0, +6 dB -> ~2.0, -6 dB -> ~0.5
 *   - Out-of-range lane no-ops (function guard logic)
 *   - Lane with no source (read=NULL) is a no-op
 *   - Negative dB produces gainLinear < 1.0
 *   - gainLinear field is independent per lane (no cross-contamination)
 *
 * The actual implementation in audio_pipeline_set_source_gain() is:
 *   if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return;
 *   if (!_sources[lane].read) return;
 *   _sources[lane].gainLinear = gainLinear;
 *
 * We replicate this logic inline so the test does not pull in the full
 * audio_pipeline.cpp dependency chain (I2S, AppState, HAL, DSP, etc.).
 * This mirrors the pattern used in test_audio_pipeline.cpp.
 */

#include <unity.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#endif

#include "../../src/audio_input_source.h"

// ===== Constants =====

#ifndef AUDIO_PIPELINE_MAX_INPUTS
#define AUDIO_PIPELINE_MAX_INPUTS 8
#endif

// ===== Minimal pipeline source table (mirrors audio_pipeline.cpp internals) =====

static AudioInputSource _sources[AUDIO_PIPELINE_MAX_INPUTS];

// Minimal stub read callback — allows source to be "registered" (non-NULL read ptr)
static uint32_t stub_read(int32_t *dst, uint32_t frames) {
    (void)dst; (void)frames;
    return frames;
}

// ===== Function under test (replicated logic) =====

static void pipeline_set_source_gain(int lane, float gainLinear) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return;
    if (!_sources[lane].read) return;
    _sources[lane].gainLinear = gainLinear;
}

// ===== dB-to-linear helper (mirrors websocket_command.cpp handler, with clamp) =====

static float db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

// Mirrors the clamp applied in the setInputGain WS handler before powf
static float db_clamp(float db) {
    if (db < -60.0f) db = -60.0f;
    if (db > 12.0f)  db = 12.0f;
    return db;
}

// ===== setUp / tearDown =====

void setUp(void) {
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        AudioInputSource s = AUDIO_INPUT_SOURCE_INIT;
        _sources[i] = s;
    }
}

void tearDown(void) {}

// ===== Tests =====

// 1. 0 dB -> linear 1.0 (unity gain)
void test_zero_db_is_unity_linear(void) {
    _sources[0].read = stub_read;
    float linear = db_to_linear(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, linear);
    pipeline_set_source_gain(0, linear);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, _sources[0].gainLinear);
}

// 2. +6.02 dB -> linear ~2.0 (doubles amplitude)
void test_plus_6db_doubles_gain(void) {
    _sources[0].read = stub_read;
    float linear = db_to_linear(6.02f);
    pipeline_set_source_gain(0, linear);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.0f, _sources[0].gainLinear);
}

// 3. -6.02 dB -> linear ~0.5 (halves amplitude)
void test_minus_6db_halves_gain(void) {
    _sources[0].read = stub_read;
    float linear = db_to_linear(-6.02f);
    pipeline_set_source_gain(0, linear);
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 0.5f, _sources[0].gainLinear);
}

// 4. -20 dB -> linear 0.1
void test_minus_20db_is_tenth(void) {
    _sources[0].read = stub_read;
    float linear = db_to_linear(-20.0f);
    pipeline_set_source_gain(0, linear);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.1f, _sources[0].gainLinear);
}

// 5. Out-of-range lane (negative) is a no-op — source untouched
void test_negative_lane_is_noop(void) {
    _sources[0].read = stub_read;
    _sources[0].gainLinear = 1.0f;
    pipeline_set_source_gain(-1, db_to_linear(6.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, _sources[0].gainLinear);
}

// 6. Out-of-range lane (>= MAX_INPUTS) is a no-op
void test_overlimit_lane_is_noop(void) {
    _sources[0].read = stub_read;
    _sources[0].gainLinear = 1.0f;
    pipeline_set_source_gain(AUDIO_PIPELINE_MAX_INPUTS, db_to_linear(6.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, _sources[0].gainLinear);
}

// 7. Unregistered lane (read=NULL) is a no-op — gainLinear stays at init value 1.0
void test_unregistered_lane_is_noop(void) {
    // _sources[0].read = NULL (default from setUp)
    float before = _sources[0].gainLinear;
    pipeline_set_source_gain(0, db_to_linear(12.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, before, _sources[0].gainLinear);
}

// 8. Gain written to correct lane, other lanes unaffected
void test_gain_written_to_correct_lane_only(void) {
    _sources[0].read = stub_read;
    _sources[1].read = stub_read;
    _sources[0].gainLinear = 1.0f;
    _sources[1].gainLinear = 1.0f;

    pipeline_set_source_gain(1, db_to_linear(12.0f));

    // Lane 0 untouched
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, _sources[0].gainLinear);
    // Lane 1 updated (~4.0x for +12dB)
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 4.0f, _sources[1].gainLinear);
}

// 9. All 8 lanes can hold independent gains
void test_all_lanes_independent(void) {
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _sources[i].read = stub_read;
        pipeline_set_source_gain(i, db_to_linear((float)i * 3.0f));
    }
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        float expected = db_to_linear((float)i * 3.0f);
        TEST_ASSERT_FLOAT_WITHIN(expected * 0.02f + 0.001f, expected, _sources[i].gainLinear);
    }
}

// 10. Negative dB produces gainLinear < 1.0
void test_negative_db_produces_attenuation(void) {
    _sources[0].read = stub_read;
    pipeline_set_source_gain(0, db_to_linear(-12.0f));
    TEST_ASSERT_TRUE(_sources[0].gainLinear < 1.0f);
    TEST_ASSERT_TRUE(_sources[0].gainLinear > 0.0f);
}

// 11. Large positive gain (e.g. +20 dB) stays finite (no NaN/Inf)
void test_large_positive_db_stays_finite(void) {
    _sources[0].read = stub_read;
    float linear = db_to_linear(20.0f);
    pipeline_set_source_gain(0, linear);
    TEST_ASSERT_FALSE(isinf(_sources[0].gainLinear));
    TEST_ASSERT_FALSE(isnan(_sources[0].gainLinear));
    // +20 dB = 10x linear
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 10.0f, _sources[0].gainLinear);
}

// 12. Gain can be overwritten (second write wins)
void test_gain_can_be_overwritten(void) {
    _sources[0].read = stub_read;
    pipeline_set_source_gain(0, db_to_linear(6.0f));
    pipeline_set_source_gain(0, db_to_linear(-3.0f));
    float expected = db_to_linear(-3.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expected, _sources[0].gainLinear);
}

// 13. Handler clamp: db > 12.0 is clamped to 12.0 (prevents destructive gain)
void test_db_clamped_at_max(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, db_clamp(9999.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, db_clamp(13.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, db_clamp(12.0f));
}

// 14. Handler clamp: db < -60.0 is clamped to -60.0 (floor at -60 dB)
void test_db_clamped_at_min(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -60.0f, db_clamp(-9999.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -60.0f, db_clamp(-61.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -60.0f, db_clamp(-60.0f));
}

// ===== Main =====

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_db_is_unity_linear);
    RUN_TEST(test_plus_6db_doubles_gain);
    RUN_TEST(test_minus_6db_halves_gain);
    RUN_TEST(test_minus_20db_is_tenth);
    RUN_TEST(test_negative_lane_is_noop);
    RUN_TEST(test_overlimit_lane_is_noop);
    RUN_TEST(test_unregistered_lane_is_noop);
    RUN_TEST(test_gain_written_to_correct_lane_only);
    RUN_TEST(test_all_lanes_independent);
    RUN_TEST(test_negative_db_produces_attenuation);
    RUN_TEST(test_large_positive_db_stays_finite);
    RUN_TEST(test_gain_can_be_overwritten);
    RUN_TEST(test_db_clamped_at_max);
    RUN_TEST(test_db_clamped_at_min);
    return UNITY_END();
}
