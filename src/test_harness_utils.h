#ifndef TEST_HARNESS_UTILS_H
#define TEST_HARNESS_UTILS_H

#include <stdbool.h>
#include <math.h>

/*
 * test_harness_utils.h — Pure inline math utilities for native test harnesses.
 *
 * All functions are static inline: no translation unit, no linkage conflicts.
 * Fully standalone — no project headers, no Arduino/ESP-IDF dependencies.
 * Compatible with GCC (native) and Xtensa (ESP32-P4).
 */

/**
 * Clamp a float value to [min_val, max_val].
 *
 * If value is NaN, returns min_val (defensive: NaN comparisons are always false,
 * so neither branch would fire without the explicit isnan guard).
 */
static inline float test_harness_clamp(float value, float min_val, float max_val)
{
    if (isnan(value)) {
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

/**
 * Return true if value is in [0.0, 100.0].
 *
 * Returns false for NaN, any negative value, or any value above 100.0.
 * The double-comparison form (>= and <=) is intentional: it rejects NaN
 * because NaN comparisons always evaluate to false.
 */
static inline bool test_harness_is_valid_percentage(float value)
{
    return (value >= 0.0f) && (value <= 100.0f);
}

/**
 * Linearly map value from [in_min, in_max] to [out_min, out_max].
 *
 * No output clamping: values outside the input range produce extrapolated
 * output. If in_min == in_max (zero-width input range), returns out_min
 * to avoid division by zero.
 */
static inline float test_harness_map_range(float value,
                                           float in_min,  float in_max,
                                           float out_min, float out_max)
{
    float in_span = in_max - in_min;
    if (in_span == 0.0f) {
        return out_min;
    }
    return out_min + (value - in_min) * (out_max - out_min) / in_span;
}

#endif /* TEST_HARNESS_UTILS_H */
