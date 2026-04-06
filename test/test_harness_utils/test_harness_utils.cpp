/*
 * test_harness_utils.cpp
 *
 * Unity tests for src/test_harness_utils.h.
 * Covers: test_harness_clamp, test_harness_is_valid_percentage,
 *         test_harness_map_range.
 *
 * Native-only — no Arduino or ESP-IDF headers required.
 */

#include <unity.h>
#include "../../src/test_harness_utils.h"

void setUp(void) {}
void tearDown(void) {}

/* ===== test_harness_clamp ===== */

void test_clamp_value_within_range_returns_unchanged(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, test_harness_clamp(5.0f, 0.0f, 10.0f));
}

void test_clamp_value_below_min_returns_min(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_harness_clamp(-3.0f, 0.0f, 10.0f));
}

void test_clamp_value_above_max_returns_max(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, test_harness_clamp(15.0f, 0.0f, 10.0f));
}

void test_clamp_value_exactly_at_min_returns_min(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_harness_clamp(0.0f, 0.0f, 10.0f));
}

void test_clamp_value_exactly_at_max_returns_max(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, test_harness_clamp(10.0f, 0.0f, 10.0f));
}

void test_clamp_negative_range_values_work(void) {
    /* Range [-5, -1]: value -3 is inside the range */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.0f, test_harness_clamp(-3.0f, -5.0f, -1.0f));
    /* Value below negative range */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, test_harness_clamp(-10.0f, -5.0f, -1.0f));
    /* Value above negative range */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, test_harness_clamp(0.0f, -5.0f, -1.0f));
}

/* ===== test_harness_is_valid_percentage ===== */

void test_is_valid_percentage_zero_is_true(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(0.0f));
}

void test_is_valid_percentage_hundred_is_true(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(100.0f));
}

void test_is_valid_percentage_midrange_is_true(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(50.0f));
}

void test_is_valid_percentage_just_below_zero_is_false(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(-0.1f));
}

void test_is_valid_percentage_just_above_hundred_is_false(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(100.1f));
}

void test_is_valid_percentage_far_below_zero_is_false(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(-1000.0f));
}

/* ===== test_harness_map_range ===== */

void test_map_range_midpoint(void) {
    /* 0.5 in [0,1] → 50.0 in [0,100] */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f,
        test_harness_map_range(0.5f, 0.0f, 1.0f, 0.0f, 100.0f));
}

void test_map_range_lower_bound(void) {
    /* 0 in [0,1] → 0.0 in [0,100] */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
        test_harness_map_range(0.0f, 0.0f, 1.0f, 0.0f, 100.0f));
}

void test_map_range_upper_bound(void) {
    /* 1 in [0,1] → 100.0 in [0,100] */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f,
        test_harness_map_range(1.0f, 0.0f, 1.0f, 0.0f, 100.0f));
}

void test_map_range_degenerate_returns_out_min(void) {
    /* in_min == in_max → returns out_min regardless of value */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f,
        test_harness_map_range(7.0f, 5.0f, 5.0f, 42.0f, 99.0f));
}

void test_map_range_reversed_output_range(void) {
    /* 0.5 in [0,1] → 50.0 in [100,0] (reversed output) */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f,
        test_harness_map_range(0.5f, 0.0f, 1.0f, 100.0f, 0.0f));
}

void test_map_range_negative_ranges(void) {
    /* -5 in [-10,0] → -50.0 in [-100,0] */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -50.0f,
        test_harness_map_range(-5.0f, -10.0f, 0.0f, -100.0f, 0.0f));
}

/* ===== Test Runner ===== */

int main(int argc, char **argv) {
    UNITY_BEGIN();

    /* test_harness_clamp */
    RUN_TEST(test_clamp_value_within_range_returns_unchanged);
    RUN_TEST(test_clamp_value_below_min_returns_min);
    RUN_TEST(test_clamp_value_above_max_returns_max);
    RUN_TEST(test_clamp_value_exactly_at_min_returns_min);
    RUN_TEST(test_clamp_value_exactly_at_max_returns_max);
    RUN_TEST(test_clamp_negative_range_values_work);

    /* test_harness_is_valid_percentage */
    RUN_TEST(test_is_valid_percentage_zero_is_true);
    RUN_TEST(test_is_valid_percentage_hundred_is_true);
    RUN_TEST(test_is_valid_percentage_midrange_is_true);
    RUN_TEST(test_is_valid_percentage_just_below_zero_is_false);
    RUN_TEST(test_is_valid_percentage_just_above_hundred_is_false);
    RUN_TEST(test_is_valid_percentage_far_below_zero_is_false);

    /* test_harness_map_range */
    RUN_TEST(test_map_range_midpoint);
    RUN_TEST(test_map_range_lower_bound);
    RUN_TEST(test_map_range_upper_bound);
    RUN_TEST(test_map_range_degenerate_returns_out_min);
    RUN_TEST(test_map_range_reversed_output_range);
    RUN_TEST(test_map_range_negative_ranges);

    return UNITY_END();
}
