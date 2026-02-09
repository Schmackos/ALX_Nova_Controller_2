#include <cmath>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline re-implementation for native testing =====
// Tests don't compile src/ directly (test_build_src = no)

float audio_rms_to_vrms(float rms_linear, float vref) {
    if (rms_linear < 0.0f) rms_linear = 0.0f;
    if (rms_linear > 1.0f) rms_linear = 1.0f;
    return rms_linear * vref;
}

// ===== Tests =====

void setUp(void) {}
void tearDown(void) {}

void test_vrms_zero_rms_returns_zero(void) {
    float result = audio_rms_to_vrms(0.0f, 3.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, result);
}

void test_vrms_full_scale_returns_vref(void) {
    float result = audio_rms_to_vrms(1.0f, 3.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.3f, result);
}

void test_vrms_half_scale(void) {
    float result = audio_rms_to_vrms(0.5f, 3.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.65f, result);
}

void test_vrms_custom_vref_5v(void) {
    float result = audio_rms_to_vrms(0.5f, 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, result);
}

void test_vrms_negative_rms_clamped(void) {
    float result = audio_rms_to_vrms(-0.5f, 3.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, result);
}

void test_vrms_rms_above_one_clamped(void) {
    float result = audio_rms_to_vrms(1.5f, 3.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.3f, result);
}

void test_vrms_very_small_rms(void) {
    float result = audio_rms_to_vrms(0.001f, 3.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0033f, result);
}

void test_vrms_minimum_vref(void) {
    float result = audio_rms_to_vrms(0.5f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, result);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_vrms_zero_rms_returns_zero);
    RUN_TEST(test_vrms_full_scale_returns_vref);
    RUN_TEST(test_vrms_half_scale);
    RUN_TEST(test_vrms_custom_vref_5v);
    RUN_TEST(test_vrms_negative_rms_clamped);
    RUN_TEST(test_vrms_rms_above_one_clamped);
    RUN_TEST(test_vrms_very_small_rms);
    RUN_TEST(test_vrms_minimum_vref);
    return UNITY_END();
}
