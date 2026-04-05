/*
 * test_harness_utils.cpp
 *
 * Comprehensive Unity tests for test_harness_utils.h utility functions:
 *   - test_harness_clamp()
 *   - test_harness_is_valid_percentage()
 *   - test_harness_map_range()
 *
 * No Arduino or ESP-IDF dependencies — header is pure inline C99.
 */

#include <unity.h>
#include <stdint.h>

#include "../../src/test_harness_utils.h"

void setUp(void) {}
void tearDown(void) {}

/* -----------------------------------------------------------------------
 * test_harness_clamp
 * ----------------------------------------------------------------------- */

void test_clamp_value_within_range_returns_unchanged(void)
{
    TEST_ASSERT_EQUAL_INT32(5, test_harness_clamp(5, 0, 10));
}

void test_clamp_value_below_min_returns_min(void)
{
    TEST_ASSERT_EQUAL_INT32(0, test_harness_clamp(-5, 0, 10));
}

void test_clamp_value_above_max_returns_max(void)
{
    TEST_ASSERT_EQUAL_INT32(10, test_harness_clamp(99, 0, 10));
}

void test_clamp_value_equals_min_returns_min(void)
{
    TEST_ASSERT_EQUAL_INT32(0, test_harness_clamp(0, 0, 10));
}

void test_clamp_value_equals_max_returns_max(void)
{
    TEST_ASSERT_EQUAL_INT32(10, test_harness_clamp(10, 0, 10));
}

void test_clamp_inverted_range_returns_min_val(void)
{
    /* When min_val > max_val the contract is: return min_val. */
    TEST_ASSERT_EQUAL_INT32(10, test_harness_clamp(5, 10, 0));
}

void test_clamp_negative_range_value_within(void)
{
    TEST_ASSERT_EQUAL_INT32(-5, test_harness_clamp(-5, -10, -1));
}

void test_clamp_negative_range_value_below_min(void)
{
    TEST_ASSERT_EQUAL_INT32(-10, test_harness_clamp(-99, -10, -1));
}

void test_clamp_negative_range_value_above_max(void)
{
    TEST_ASSERT_EQUAL_INT32(-1, test_harness_clamp(0, -10, -1));
}

void test_clamp_zero_width_range_returns_that_value(void)
{
    /* min == max: any input must return that single value. */
    TEST_ASSERT_EQUAL_INT32(7, test_harness_clamp(3,  7, 7));
    TEST_ASSERT_EQUAL_INT32(7, test_harness_clamp(7,  7, 7));
    TEST_ASSERT_EQUAL_INT32(7, test_harness_clamp(99, 7, 7));
}

/* -----------------------------------------------------------------------
 * test_harness_is_valid_percentage
 * ----------------------------------------------------------------------- */

void test_percentage_zero_is_valid(void)
{
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(0));
}

void test_percentage_hundred_is_valid(void)
{
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(100));
}

void test_percentage_midpoint_is_valid(void)
{
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(50));
}

void test_percentage_minus_one_is_invalid(void)
{
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(-1));
}

void test_percentage_one_hundred_one_is_invalid(void)
{
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(101));
}

void test_percentage_large_negative_is_invalid(void)
{
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(-1000));
}

void test_percentage_large_positive_is_invalid(void)
{
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(1000));
}

/* -----------------------------------------------------------------------
 * test_harness_map_range
 * ----------------------------------------------------------------------- */

void test_map_range_midpoint(void)
{
    /* 5 in [0,10] maps to 50 in [0,100]. */
    TEST_ASSERT_EQUAL_INT32(50, test_harness_map_range(5, 0, 10, 0, 100));
}

void test_map_range_in_min_maps_to_out_min(void)
{
    TEST_ASSERT_EQUAL_INT32(0, test_harness_map_range(0, 0, 10, 0, 100));
}

void test_map_range_in_max_maps_to_out_max(void)
{
    TEST_ASSERT_EQUAL_INT32(100, test_harness_map_range(10, 0, 10, 0, 100));
}

void test_map_range_equal_in_bounds_returns_out_min(void)
{
    /* Division by zero guard: in_min == in_max → return out_min. */
    TEST_ASSERT_EQUAL_INT32(42, test_harness_map_range(7, 5, 5, 42, 99));
}

void test_map_range_negative_input_range(void)
{
    /* Map -10 in [-10,10] to [0,100] → 0. */
    TEST_ASSERT_EQUAL_INT32(0, test_harness_map_range(-10, -10, 10, 0, 100));
    /* Map 0 in [-10,10] to [0,100] → 50. */
    TEST_ASSERT_EQUAL_INT32(50, test_harness_map_range(0, -10, 10, 0, 100));
    /* Map 10 in [-10,10] to [0,100] → 100. */
    TEST_ASSERT_EQUAL_INT32(100, test_harness_map_range(10, -10, 10, 0, 100));
}

void test_map_range_reverse_output(void)
{
    /* out_min > out_max (decreasing output). */
    /* 0 in [0,10] → 100 in [100,0]. */
    TEST_ASSERT_EQUAL_INT32(100, test_harness_map_range(0, 0, 10, 100, 0));
    /* 10 in [0,10] → 0 in [100,0]. */
    TEST_ASSERT_EQUAL_INT32(0, test_harness_map_range(10, 0, 10, 100, 0));
    /* 5 in [0,10] → 50 in [100,0]. */
    TEST_ASSERT_EQUAL_INT32(50, test_harness_map_range(5, 0, 10, 100, 0));
}

void test_map_range_value_below_in_min_extrapolates(void)
{
    /* The function performs unclamped linear interpolation.
       -5 in [0,10] to [0,100] → -50 (extrapolation below in_min). */
    TEST_ASSERT_EQUAL_INT32(-50, test_harness_map_range(-5, 0, 10, 0, 100));
}

void test_map_range_value_above_in_max_extrapolates(void)
{
    /* 20 in [0,10] to [0,100] → 200 (extrapolation above in_max). */
    TEST_ASSERT_EQUAL_INT32(200, test_harness_map_range(20, 0, 10, 0, 100));
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    /* clamp tests */
    RUN_TEST(test_clamp_value_within_range_returns_unchanged);
    RUN_TEST(test_clamp_value_below_min_returns_min);
    RUN_TEST(test_clamp_value_above_max_returns_max);
    RUN_TEST(test_clamp_value_equals_min_returns_min);
    RUN_TEST(test_clamp_value_equals_max_returns_max);
    RUN_TEST(test_clamp_inverted_range_returns_min_val);
    RUN_TEST(test_clamp_negative_range_value_within);
    RUN_TEST(test_clamp_negative_range_value_below_min);
    RUN_TEST(test_clamp_negative_range_value_above_max);
    RUN_TEST(test_clamp_zero_width_range_returns_that_value);

    /* is_valid_percentage tests */
    RUN_TEST(test_percentage_zero_is_valid);
    RUN_TEST(test_percentage_hundred_is_valid);
    RUN_TEST(test_percentage_midpoint_is_valid);
    RUN_TEST(test_percentage_minus_one_is_invalid);
    RUN_TEST(test_percentage_one_hundred_one_is_invalid);
    RUN_TEST(test_percentage_large_negative_is_invalid);
    RUN_TEST(test_percentage_large_positive_is_invalid);

    /* map_range tests */
    RUN_TEST(test_map_range_midpoint);
    RUN_TEST(test_map_range_in_min_maps_to_out_min);
    RUN_TEST(test_map_range_in_max_maps_to_out_max);
    RUN_TEST(test_map_range_equal_in_bounds_returns_out_min);
    RUN_TEST(test_map_range_negative_input_range);
    RUN_TEST(test_map_range_reverse_output);
    RUN_TEST(test_map_range_value_below_in_min_extrapolates);
    RUN_TEST(test_map_range_value_above_in_max_extrapolates);

    return UNITY_END();
}
