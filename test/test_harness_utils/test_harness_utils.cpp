#include <unity.h>
#include <cstring>
#include <cstdint>
#include <cmath>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/test_harness_utils.h"

void setUp(void) {}
void tearDown(void) {}

// ===== test_harness_clamp =====

void test_clamp_value_within_range(void) {
    TEST_ASSERT_EQUAL_INT32(50, test_harness_clamp(50, 0, 100));
}

void test_clamp_value_below_min(void) {
    TEST_ASSERT_EQUAL_INT32(0, test_harness_clamp(-10, 0, 100));
}

void test_clamp_value_above_max(void) {
    TEST_ASSERT_EQUAL_INT32(100, test_harness_clamp(200, 0, 100));
}

void test_clamp_value_equals_min_boundary(void) {
    TEST_ASSERT_EQUAL_INT32(0, test_harness_clamp(0, 0, 100));
}

void test_clamp_value_equals_max_boundary(void) {
    TEST_ASSERT_EQUAL_INT32(100, test_harness_clamp(100, 0, 100));
}

void test_clamp_negative_range(void) {
    // clamp(-5, -10, -1) should return -5 (within range)
    TEST_ASSERT_EQUAL_INT32(-5, test_harness_clamp(-5, -10, -1));
}

void test_clamp_negative_range_below_min(void) {
    // clamp(-15, -10, -1) should return -10 (below min)
    TEST_ASSERT_EQUAL_INT32(-10, test_harness_clamp(-15, -10, -1));
}

// ===== test_harness_is_valid_percentage =====

void test_percentage_zero_is_valid(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(0.0f));
}

void test_percentage_hundred_is_valid(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(100.0f));
}

void test_percentage_fifty_is_valid(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(50.0f));
}

void test_percentage_negative_is_invalid(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(-0.1f));
}

void test_percentage_above_hundred_is_invalid(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(100.1f));
}

void test_percentage_nan_is_invalid(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(NAN));
}

// ===== test_harness_map_range =====

void test_map_range_midpoint(void) {
    // 5.0 in [0,10] -> [0,100] = 50.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, test_harness_map_range(5.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

void test_map_range_at_in_min(void) {
    // 0.0 in [0,10] -> [0,100] = 0.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_harness_map_range(0.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

void test_map_range_at_in_max(void) {
    // 10.0 in [0,10] -> [0,100] = 100.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, test_harness_map_range(10.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

void test_map_range_reverse_output(void) {
    // 5.0 in [0,10] -> [100,0] = 50.0 (reversed output)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, test_harness_map_range(5.0f, 0.0f, 10.0f, 100.0f, 0.0f));
}

void test_map_range_degenerate_in_min_equals_in_max(void) {
    // When in_min == in_max, returns out_min
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_harness_map_range(5.0f, 5.0f, 5.0f, 0.0f, 100.0f));
}

void test_map_range_negative_ranges(void) {
    // -5.0 in [-10,0] -> [-100,0] = -50.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -50.0f, test_harness_map_range(-5.0f, -10.0f, 0.0f, -100.0f, 0.0f));
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_clamp_value_within_range);
    RUN_TEST(test_clamp_value_below_min);
    RUN_TEST(test_clamp_value_above_max);
    RUN_TEST(test_clamp_value_equals_min_boundary);
    RUN_TEST(test_clamp_value_equals_max_boundary);
    RUN_TEST(test_clamp_negative_range);
    RUN_TEST(test_clamp_negative_range_below_min);

    RUN_TEST(test_percentage_zero_is_valid);
    RUN_TEST(test_percentage_hundred_is_valid);
    RUN_TEST(test_percentage_fifty_is_valid);
    RUN_TEST(test_percentage_negative_is_invalid);
    RUN_TEST(test_percentage_above_hundred_is_invalid);
    RUN_TEST(test_percentage_nan_is_invalid);

    RUN_TEST(test_map_range_midpoint);
    RUN_TEST(test_map_range_at_in_min);
    RUN_TEST(test_map_range_at_in_max);
    RUN_TEST(test_map_range_reverse_output);
    RUN_TEST(test_map_range_degenerate_in_min_equals_in_max);
    RUN_TEST(test_map_range_negative_ranges);

    return UNITY_END();
}
