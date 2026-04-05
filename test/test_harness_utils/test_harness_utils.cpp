#include <unity.h>
#include "../../src/test_harness_utils.h"

void setUp(void) { }
void tearDown(void) { }

// ===== test_harness_clamp =====

void test_clamp_below_min(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_harness_clamp(-5.0f, 0.0f, 10.0f));
}

void test_clamp_above_max(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, test_harness_clamp(15.0f, 0.0f, 10.0f));
}

void test_clamp_in_range(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, test_harness_clamp(5.0f, 0.0f, 10.0f));
}

void test_clamp_at_min_boundary(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, test_harness_clamp(0.0f, 0.0f, 10.0f));
}

void test_clamp_at_max_boundary(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, test_harness_clamp(10.0f, 0.0f, 10.0f));
}

void test_clamp_negative_range(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.0f, test_harness_clamp(-3.0f, -5.0f, -1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, test_harness_clamp(-10.0f, -5.0f, -1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, test_harness_clamp(0.0f, -5.0f, -1.0f));
}

// ===== test_harness_is_valid_percentage =====

void test_percentage_zero_is_valid(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(0.0f));
}

void test_percentage_hundred_is_valid(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(100.0f));
}

void test_percentage_midpoint_is_valid(void) {
    TEST_ASSERT_TRUE(test_harness_is_valid_percentage(50.0f));
}

void test_percentage_negative_is_invalid(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(-0.1f));
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(-50.0f));
}

void test_percentage_above_hundred_is_invalid(void) {
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(100.1f));
    TEST_ASSERT_FALSE(test_harness_is_valid_percentage(200.0f));
}

// ===== test_harness_map_range =====

void test_map_range_basic(void) {
    /* 5 in [0,10] -> 50 in [0,100] */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f,
        test_harness_map_range(5.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

void test_map_range_inverse(void) {
    /* 5 in [0,10] -> 50 in [100,0] (inverted output) */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f,
        test_harness_map_range(5.0f, 0.0f, 10.0f, 100.0f, 0.0f));
}

void test_map_range_identity(void) {
    /* Same input and output range -> value unchanged */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.5f,
        test_harness_map_range(7.5f, 0.0f, 10.0f, 0.0f, 10.0f));
}

void test_map_range_degenerate_input(void) {
    /* in_min == in_max: division-by-zero guard, returns out_min */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
        test_harness_map_range(5.0f, 3.0f, 3.0f, 0.0f, 100.0f));
}

void test_map_range_negative_ranges(void) {
    /* -5 in [-10,0] -> -50 in [-100,0] */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -50.0f,
        test_harness_map_range(-5.0f, -10.0f, 0.0f, -100.0f, 0.0f));
}

void test_map_range_boundary_in_min(void) {
    /* value == in_min -> out_min */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
        test_harness_map_range(0.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

void test_map_range_boundary_in_max(void) {
    /* value == in_max -> out_max */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f,
        test_harness_map_range(10.0f, 0.0f, 10.0f, 0.0f, 100.0f));
}

// ===== Test runner =====

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* clamp */
    RUN_TEST(test_clamp_below_min);
    RUN_TEST(test_clamp_above_max);
    RUN_TEST(test_clamp_in_range);
    RUN_TEST(test_clamp_at_min_boundary);
    RUN_TEST(test_clamp_at_max_boundary);
    RUN_TEST(test_clamp_negative_range);

    /* is_valid_percentage */
    RUN_TEST(test_percentage_zero_is_valid);
    RUN_TEST(test_percentage_hundred_is_valid);
    RUN_TEST(test_percentage_midpoint_is_valid);
    RUN_TEST(test_percentage_negative_is_invalid);
    RUN_TEST(test_percentage_above_hundred_is_invalid);

    /* map_range */
    RUN_TEST(test_map_range_basic);
    RUN_TEST(test_map_range_inverse);
    RUN_TEST(test_map_range_identity);
    RUN_TEST(test_map_range_degenerate_input);
    RUN_TEST(test_map_range_negative_ranges);
    RUN_TEST(test_map_range_boundary_in_min);
    RUN_TEST(test_map_range_boundary_in_max);

    return UNITY_END();
}
