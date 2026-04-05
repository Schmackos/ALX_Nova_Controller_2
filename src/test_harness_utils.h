#ifndef TEST_HARNESS_UTILS_H
#define TEST_HARNESS_UTILS_H

#include <stdbool.h>

/**
 * @brief Clamp a float value to [min, max].
 *
 * @param value  The value to clamp.
 * @param min    Lower bound (inclusive).
 * @param max    Upper bound (inclusive).
 * @return       value clamped to [min, max].
 */
static inline float test_harness_clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief Check whether a float represents a valid percentage.
 *
 * A valid percentage is any value in the closed interval [0.0, 100.0].
 *
 * @param value  The value to test.
 * @return       true if value is in [0.0, 100.0], false otherwise.
 */
static inline bool test_harness_is_valid_percentage(float value) {
    return (value >= 0.0f) && (value <= 100.0f);
}

/**
 * @brief Linearly map a value from one range to another.
 *
 * Maps @p value from [in_min, in_max] to [out_min, out_max].  When
 * in_min == in_max (degenerate input range) out_min is returned to
 * avoid a division-by-zero.
 *
 * @param value    Input value to map.
 * @param in_min   Minimum of the input range.
 * @param in_max   Maximum of the input range.
 * @param out_min  Minimum of the output range.
 * @param out_max  Maximum of the output range.
 * @return         Mapped value in output range, or out_min when in_min == in_max.
 */
static inline float test_harness_map_range(float value,
                                           float in_min, float in_max,
                                           float out_min, float out_max) {
    if (in_min == in_max) return out_min;
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

#endif /* TEST_HARNESS_UTILS_H */
