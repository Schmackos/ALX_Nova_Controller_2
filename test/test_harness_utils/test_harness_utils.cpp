#include <unity.h>
#include "../../src/test_harness_utils.h"

void setUp(void) { }
void tearDown(void) { }

// ===== test_harness_clamp Tests =====

void test_clamp_value_below_min(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_harness_clamp(-5.0f, 0.0f, 10.0f));
}

void test_clamp_value_above_max(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, test_harness_clamp(15.0f, 0.0f, 10.0f));
}

void test_clamp_value_in_range(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, test_harness_clamp(5.0f, 0.0f, 10.0f));
}

void test_clamp_value_at_min_boundary(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_harness_clamp(0.0f, 0.0f, 10.0f));
}

void test_clamp_value_at_max_boundary(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, test_harness_clamp(10.0f, 0.0f, 10.0f));
}

void test_clamp_negative_range(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, test_harness_clamp(-3.0f, -10.0f, -5.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, test_harness_clamp(-15.0f, -10.0f, -5.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -7.0f, test_harness_clamp(-7.0f, -10.0f, -5.0f));
}

// ===== test_harness_is_valid_percentage Tests =====

void test_is_valid_percentage_zero(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(0.0f));
}

void test_is_valid_percentage_hundred(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(100.0f));
}

void test_is_valid_percentage_midpoint(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(50.0f));
}

void test_is_valid_percentage_just_below_zero(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(-0.1f));
}

void test_is_valid_percentage_just_above_hundred(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(100.1f));
}

void test_is_valid_percentage_negative_value(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(-50.0f));
}

// ===== test_harness_map_range Tests =====

void test_map_range_basic(void) {
    // Map 5 from [0, 10] to [0, 100] -> 50
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, test_harness_map_range(5.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

void test_map_range_inverse(void) {
    // Map 5 from [0, 10] to [100, 0] -> 50
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, test_harness_map_range(5.0f, 0.0f, 10.0f, 100.0f, 0.0f));
    // Map 0 from [0, 10] to [100, 0] -> 100
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, test_harness_map_range(0.0f, 0.0f, 10.0f, 100.0f, 0.0f));
}

void test_map_range_identity(void) {
    // Map from [0, 10] to [0, 10] -> same value
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.5f, test_harness_map_range(7.5f, 0.0f, 10.0f, 0.0f, 10.0f));
}

void test_map_range_edge_case_equal_in_bounds(void) {
    // in_min == in_max: should return out_min without division by zero
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, test_harness_map_range(3.0f, 3.0f, 3.0f, 5.0f, 10.0f));
}

void test_map_range_negative_ranges(void) {
    // Map 0 from [-10, 10] to [0, 100] -> 50
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, test_harness_map_range(0.0f, -10.0f, 10.0f, 0.0f, 100.0f));
}

void test_map_range_boundary_values(void) {
    // Map in_min -> out_min exactly
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_harness_map_range(0.0f, 0.0f, 10.0f, 0.0f, 100.0f));
    // Map in_max -> out_max exactly
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, test_harness_map_range(10.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    // clamp tests
    RUN_TEST(test_clamp_value_below_min);
    RUN_TEST(test_clamp_value_above_max);
    RUN_TEST(test_clamp_value_in_range);
    RUN_TEST(test_clamp_value_at_min_boundary);
    RUN_TEST(test_clamp_value_at_max_boundary);
    RUN_TEST(test_clamp_negative_range);

    // is_valid_percentage tests
    RUN_TEST(test_is_valid_percentage_zero);
    RUN_TEST(test_is_valid_percentage_hundred);
    RUN_TEST(test_is_valid_percentage_midpoint);
    RUN_TEST(test_is_valid_percentage_just_below_zero);
    RUN_TEST(test_is_valid_percentage_just_above_hundred);
    RUN_TEST(test_is_valid_percentage_negative_value);

    // map_range tests
    RUN_TEST(test_map_range_basic);
    RUN_TEST(test_map_range_inverse);
    RUN_TEST(test_map_range_identity);
    RUN_TEST(test_map_range_edge_case_equal_in_bounds);
    RUN_TEST(test_map_range_negative_ranges);
    RUN_TEST(test_map_range_boundary_values);

    return UNITY_END();
}
