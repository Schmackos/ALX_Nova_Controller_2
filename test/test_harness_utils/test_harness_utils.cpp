/*
 * test_harness_utils.cpp
 *
 * Unity tests for src/test_harness_utils.h — covers all three inline utility
 * functions: test_harness_clamp, test_harness_is_valid_percentage, and
 * test_harness_map_range.
 *
 * Run with: pio test -e native -f test_harness_utils
 */

#include <unity.h>
#include <math.h>
#include "../../src/test_harness_utils.h"

void setUp(void) {}
void tearDown(void) {}

/* -------------------------------------------------------------------------
 * test_harness_clamp
 * ---------------------------------------------------------------------- */

void test_clamp_value_within_range_returns_unchanged(void) {
    double result = test_harness_clamp(5.0, 0.0, 10.0);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, (float)result);
}

void test_clamp_value_below_min_returns_min(void) {
    double result = test_harness_clamp(-3.0, 0.0, 10.0);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, (float)result);
}

void test_clamp_value_above_max_returns_max(void) {
    double result = test_harness_clamp(15.0, 0.0, 10.0);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, (float)result);
}

void test_clamp_value_exactly_at_min_returns_min(void) {
    double result = test_harness_clamp(0.0, 0.0, 10.0);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, (float)result);
}

void test_clamp_value_exactly_at_max_returns_max(void) {
    double result = test_harness_clamp(10.0, 0.0, 10.0);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, (float)result);
}

void test_clamp_negative_range(void) {
    /* Clamp -7.0 to [-10, -5] — should return -7.0 unchanged */
    double result = test_harness_clamp(-7.0, -10.0, -5.0);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -7.0f, (float)result);
}

void test_clamp_value_below_negative_min_returns_min(void) {
    /* Clamp -15.0 to [-10, -5] — should return -10.0 */
    double result = test_harness_clamp(-15.0, -10.0, -5.0);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -10.0f, (float)result);
}

void test_clamp_nan_propagates(void) {
    /* NaN comparisons are always false, so NaN must pass through unchanged */
    double result = test_harness_clamp(NAN, 0.0, 10.0);
    TEST_ASSERT_TRUE(isnan(result));
}

/* -------------------------------------------------------------------------
 * test_harness_is_valid_percentage
 * ---------------------------------------------------------------------- */

void test_is_valid_percentage_zero_is_valid(void) {
    TEST_ASSERT_EQUAL_INT(1, test_harness_is_valid_percentage(0.0));
}

void test_is_valid_percentage_one_hundred_is_valid(void) {
    TEST_ASSERT_EQUAL_INT(1, test_harness_is_valid_percentage(100.0));
}

void test_is_valid_percentage_midpoint_is_valid(void) {
    TEST_ASSERT_EQUAL_INT(1, test_harness_is_valid_percentage(50.0));
}

void test_is_valid_percentage_negative_tiny_is_invalid(void) {
    TEST_ASSERT_EQUAL_INT(0, test_harness_is_valid_percentage(-0.001));
}

void test_is_valid_percentage_slightly_over_100_is_invalid(void) {
    TEST_ASSERT_EQUAL_INT(0, test_harness_is_valid_percentage(100.001));
}

void test_is_valid_percentage_nan_is_invalid(void) {
    TEST_ASSERT_EQUAL_INT(0, test_harness_is_valid_percentage(NAN));
}

void test_is_valid_percentage_positive_infinity_is_invalid(void) {
    TEST_ASSERT_EQUAL_INT(0, test_harness_is_valid_percentage(INFINITY));
}

void test_is_valid_percentage_negative_infinity_is_invalid(void) {
    TEST_ASSERT_EQUAL_INT(0, test_harness_is_valid_percentage(-INFINITY));
}

/* -------------------------------------------------------------------------
 * test_harness_map_range
 * ---------------------------------------------------------------------- */

void test_map_range_midpoint(void) {
    /* 0.5 in [0,1] → [0,100] should give 50.0 */
    double result = test_harness_map_range(0.5, 0.0, 1.0, 0.0, 100.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, (float)result);
}

void test_map_range_at_in_min_returns_out_min(void) {
    double result = test_harness_map_range(0.0, 0.0, 1.0, 10.0, 20.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, (float)result);
}

void test_map_range_at_in_max_returns_out_max(void) {
    double result = test_harness_map_range(1.0, 0.0, 1.0, 10.0, 20.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, (float)result);
}

void test_map_range_reverse_output(void) {
    /* Reverse mapping: value at in_min should give out_min (which is 100.0) */
    double result = test_harness_map_range(0.0, 0.0, 1.0, 100.0, 0.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, (float)result);
}

void test_map_range_reverse_output_midpoint(void) {
    double result = test_harness_map_range(0.5, 0.0, 1.0, 100.0, 0.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, (float)result);
}

void test_map_range_zero_width_input_returns_out_min(void) {
    /* in_min == in_max — must not divide by zero; returns out_min */
    double result = test_harness_map_range(5.0, 5.0, 5.0, 42.0, 99.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, (float)result);
}

void test_map_range_negative_ranges(void) {
    /* Map -5 from [-10, 0] to [-100, 0] → 50% through → -50 */
    double result = test_harness_map_range(-5.0, -10.0, 0.0, -100.0, 0.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -50.0f, (float)result);
}

void test_map_range_extrapolation_below(void) {
    /* Value outside input range — linear extrapolation */
    /* -1 in [0,1] → [0,100] should extrapolate to -100.0 */
    double result = test_harness_map_range(-1.0, 0.0, 1.0, 0.0, 100.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -100.0f, (float)result);
}

void test_map_range_extrapolation_above(void) {
    /* 2 in [0,1] → [0,100] should extrapolate to 200.0 */
    double result = test_harness_map_range(2.0, 0.0, 1.0, 0.0, 100.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, (float)result);
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

int main(int argc, char **argv) {
    UNITY_BEGIN();

    /* test_harness_clamp */
    RUN_TEST(test_clamp_value_within_range_returns_unchanged);
    RUN_TEST(test_clamp_value_below_min_returns_min);
    RUN_TEST(test_clamp_value_above_max_returns_max);
    RUN_TEST(test_clamp_value_exactly_at_min_returns_min);
    RUN_TEST(test_clamp_value_exactly_at_max_returns_max);
    RUN_TEST(test_clamp_negative_range);
    RUN_TEST(test_clamp_value_below_negative_min_returns_min);
    RUN_TEST(test_clamp_nan_propagates);

    /* test_harness_is_valid_percentage */
    RUN_TEST(test_is_valid_percentage_zero_is_valid);
    RUN_TEST(test_is_valid_percentage_one_hundred_is_valid);
    RUN_TEST(test_is_valid_percentage_midpoint_is_valid);
    RUN_TEST(test_is_valid_percentage_negative_tiny_is_invalid);
    RUN_TEST(test_is_valid_percentage_slightly_over_100_is_invalid);
    RUN_TEST(test_is_valid_percentage_nan_is_invalid);
    RUN_TEST(test_is_valid_percentage_positive_infinity_is_invalid);
    RUN_TEST(test_is_valid_percentage_negative_infinity_is_invalid);

    /* test_harness_map_range */
    RUN_TEST(test_map_range_midpoint);
    RUN_TEST(test_map_range_at_in_min_returns_out_min);
    RUN_TEST(test_map_range_at_in_max_returns_out_max);
    RUN_TEST(test_map_range_reverse_output);
    RUN_TEST(test_map_range_reverse_output_midpoint);
    RUN_TEST(test_map_range_zero_width_input_returns_out_min);
    RUN_TEST(test_map_range_negative_ranges);
    RUN_TEST(test_map_range_extrapolation_below);
    RUN_TEST(test_map_range_extrapolation_above);

    return UNITY_END();
}
