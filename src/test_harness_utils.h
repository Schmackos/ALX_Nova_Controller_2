#ifndef TEST_HARNESS_UTILS_H
#define TEST_HARNESS_UTILS_H

#include <stdint.h>
#include <stdbool.h>

/*
 * test_harness_utils.h — pure inline utility functions for test harness use.
 *
 * TEST ARTIFACT ONLY. No Arduino, ESP-IDF, or any other library dependencies.
 * All functions are static inline and C99 compatible.
 */

/*
 * Clamp value to [min_val, max_val] range.
 * If min_val > max_val, return min_val (defined behavior for inverted range).
 */
static inline int32_t test_harness_clamp(int32_t value, int32_t min_val, int32_t max_val)
{
    if (min_val > max_val) {
        return min_val;
    }
    if (value < min_val) {
        return min_val;
    }
    if (value > max_val) {
        return max_val;
    }
    return value;
}

/*
 * Return true if value is in [0, 100] inclusive.
 */
static inline bool test_harness_is_valid_percentage(int32_t value)
{
    return (value >= 0) && (value <= 100);
}

/*
 * Map value from input range [in_min, in_max] to output range [out_min, out_max]
 * using linear interpolation.
 * If in_min == in_max, return out_min to avoid division by zero.
 * Note: intermediate product (value - in_min) * (out_max - out_min) must fit
 * in int32_t. Safe for ranges up to ~46,000.
 */
static inline int32_t test_harness_map_range(int32_t value, int32_t in_min, int32_t in_max,
                                              int32_t out_min, int32_t out_max)
{
    if (in_min == in_max) {
        return out_min;
    }
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

#endif /* TEST_HARNESS_UTILS_H */
