/*
 * test_harness_utils.h
 *
 * Test harness artifact for pipeline validation (kitchen-sink workflow).
 * Pure inline utility functions with no Arduino or ESP-IDF dependencies so
 * they compile on native (desktop) targets as well as firmware builds.
 *
 * All symbols use the "test_harness_" prefix as required by the validation
 * artifact naming convention.
 */

#ifndef TEST_HARNESS_UTILS_H
#define TEST_HARNESS_UTILS_H

#include <stdbool.h>

/* Clamp value to the closed interval [min_val, max_val]. */
static inline float test_harness_clamp(float value, float min_val, float max_val)
{
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/* Return true if value is within [0.0, 100.0] inclusive. */
static inline bool test_harness_is_valid_percentage(float value)
{
    return (value >= 0.0f && value <= 100.0f);
}

/*
 * Linearly map value from [in_min, in_max] to [out_min, out_max].
 * If in_min == in_max (degenerate range), returns out_min to avoid
 * division by zero.
 */
static inline float test_harness_map_range(float value,
                                           float in_min, float in_max,
                                           float out_min, float out_max)
{
    if (in_min == in_max) return out_min;
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

#endif /* TEST_HARNESS_UTILS_H */
