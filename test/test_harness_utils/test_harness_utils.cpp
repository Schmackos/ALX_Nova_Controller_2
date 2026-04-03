/**
 * test_harness_utils.cpp
 *
 * Unity tests for src/test_harness_utils.h
 * Covers: test_harness_clamp, test_harness_is_valid_percentage, test_harness_map_range
 */

#include <unity.h>
#include <math.h>
#include "../../src/test_harness_utils.h"

void setUp(void) {}
void tearDown(void) {}

/* -------------------------------------------------------------------------
 * test_harness_clamp
 * ------------------------------------------------------------------------- */

void test_clamp_value_within_range_returned_unchanged(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, test_harness_clamp(5.0f, 0.0f, 10.0f));
}

void test_clamp_value_below_min_returns_min(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, test_harness_clamp(-3.0f, 0.0f, 10.0f));
}

void test_clamp_value_above_max_returns_max(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, test_harness_clamp(15.0f, 0.0f, 10.0f));
}

void test_clamp_value_exactly_at_min_returns_min(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, test_harness_clamp(0.0f, 0.0f, 10.0f));
}

void test_clamp_value_exactly_at_max_returns_max(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, test_harness_clamp(10.0f, 0.0f, 10.0f));
}

void test_clamp_nan_returns_min_val(void) {
    float result = test_harness_clamp(NAN, 0.0f, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, result);
}

void test_clamp_negative_range_value_within(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -5.0f, test_harness_clamp(-5.0f, -10.0f, -1.0f));
}

void test_clamp_negative_range_value_below_min(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -10.0f, test_harness_clamp(-15.0f, -10.0f, -1.0f));
}

void test_clamp_negative_range_value_above_max(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -1.0f, test_harness_clamp(0.0f, -10.0f, -1.0f));
}

/* -------------------------------------------------------------------------
 * test_harness_is_valid_percentage
 * ------------------------------------------------------------------------- */

void test_percentage_zero_is_valid(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(0.0f));
}

void test_percentage_hundred_is_valid(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(100.0f));
}

void test_percentage_midrange_is_valid(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(50.0f));
}

void test_percentage_just_below_zero_is_invalid(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(-0.001f));
}

void test_percentage_just_above_hundred_is_invalid(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(100.001f));
}

void test_percentage_negative_one_is_invalid(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(-1.0f));
}

void test_percentage_two_hundred_is_invalid(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(200.0f));
}

void test_percentage_nan_is_invalid(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(NAN));
}

/* -------------------------------------------------------------------------
 * test_harness_map_range
 * ------------------------------------------------------------------------- */

void test_map_range_midpoint(void) {
    /* map_range(5, 0, 10, 0, 100) -> 50 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, test_harness_map_range(5.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

void test_map_range_at_input_min(void) {
    /* map_range(0, 0, 10, 0, 100) -> 0 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_harness_map_range(0.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

void test_map_range_at_input_max(void) {
    /* map_range(10, 0, 10, 0, 100) -> 100 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, test_harness_map_range(10.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

void test_map_range_inverse_mapping(void) {
    /* map_range(0, 0, 10, 100, 0) -> 100 (output range inverted) */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, test_harness_map_range(0.0f, 0.0f, 10.0f, 100.0f, 0.0f));
}

void test_map_range_zero_width_input_returns_out_min(void) {
    /* in_min == in_max: guard against div-by-zero, returns out_min */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_harness_map_range(5.0f, 5.0f, 5.0f, 0.0f, 100.0f));
}

void test_map_range_extrapolation_above_max(void) {
    /* map_range(15, 0, 10, 0, 100) -> 150 (no clamping) */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 150.0f, test_harness_map_range(15.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

void test_map_range_negative_input_range(void) {
    /* map_range(-5, -10, 0, 0, 100) -> 50 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, test_harness_map_range(-5.0f, -10.0f, 0.0f, 0.0f, 100.0f));
}

void test_map_range_inverse_midpoint(void) {
    /* map_range(5, 0, 10, 100, 0) -> 50 (inverse, midpoint still 50) */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, test_harness_map_range(5.0f, 0.0f, 10.0f, 100.0f, 0.0f));
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */

int main(int argc, char **argv) {
    UNITY_BEGIN();

    /* test_harness_clamp */
    RUN_TEST(test_clamp_value_within_range_returned_unchanged);
    RUN_TEST(test_clamp_value_below_min_returns_min);
    RUN_TEST(test_clamp_value_above_max_returns_max);
    RUN_TEST(test_clamp_value_exactly_at_min_returns_min);
    RUN_TEST(test_clamp_value_exactly_at_max_returns_max);
    RUN_TEST(test_clamp_nan_returns_min_val);
    RUN_TEST(test_clamp_negative_range_value_within);
    RUN_TEST(test_clamp_negative_range_value_below_min);
    RUN_TEST(test_clamp_negative_range_value_above_max);

    /* test_harness_is_valid_percentage */
    RUN_TEST(test_percentage_zero_is_valid);
    RUN_TEST(test_percentage_hundred_is_valid);
    RUN_TEST(test_percentage_midrange_is_valid);
    RUN_TEST(test_percentage_just_below_zero_is_invalid);
    RUN_TEST(test_percentage_just_above_hundred_is_invalid);
    RUN_TEST(test_percentage_negative_one_is_invalid);
    RUN_TEST(test_percentage_two_hundred_is_invalid);
    RUN_TEST(test_percentage_nan_is_invalid);

    /* test_harness_map_range */
    RUN_TEST(test_map_range_midpoint);
    RUN_TEST(test_map_range_at_input_min);
    RUN_TEST(test_map_range_at_input_max);
    RUN_TEST(test_map_range_inverse_mapping);
    RUN_TEST(test_map_range_zero_width_input_returns_out_min);
    RUN_TEST(test_map_range_extrapolation_above_max);
    RUN_TEST(test_map_range_negative_input_range);
    RUN_TEST(test_map_range_inverse_midpoint);

    return UNITY_END();
}
