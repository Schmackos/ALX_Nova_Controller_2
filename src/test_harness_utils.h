#ifndef TEST_HARNESS_UTILS_H
#define TEST_HARNESS_UTILS_H

#include <stdbool.h>

/**
 * @brief Clamp a float value to [min, max] range.
 *
 * Returns min if value < min, max if value > max, value otherwise.
 *
 * @param value The float value to clamp.
 * @param min   The lower bound of the clamp range.
 * @param max   The upper bound of the clamp range.
 * @return      The clamped float value.
 */
static inline float test_harness_clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief Check whether a float value is a valid percentage.
 *
 * Returns true if value is in [0.0, 100.0] inclusive, false otherwise.
 *
 * @param value The float value to check.
 * @return      true if value is a valid percentage, false otherwise.
 */
static inline bool test_harness_is_valid_percentage(float value) {
    return (value >= 0.0f) && (value <= 100.0f);
}

/**
 * @brief Linearly map a float value from one range to another.
 *
 * Maps value from [in_min, in_max] to [out_min, out_max] using linear
 * interpolation.  When in_min == in_max the function returns out_min to
 * avoid division by zero.
 *
 * @param value   The float value to map.
 * @param in_min  Lower bound of the input range.
 * @param in_max  Upper bound of the input range.
 * @param out_min Lower bound of the output range.
 * @param out_max Upper bound of the output range.
 * @return        The linearly mapped float value.
 */
static inline float test_harness_map_range(float value,
                                           float in_min, float in_max,
                                           float out_min, float out_max) {
    if (in_min == in_max) return out_min;
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

#endif /* TEST_HARNESS_UTILS_H */
