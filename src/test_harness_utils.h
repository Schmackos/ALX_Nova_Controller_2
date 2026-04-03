#ifndef TEST_HARNESS_UTILS_H
#define TEST_HARNESS_UTILS_H

#include <cstdint>

// Utility functions for the ALX Nova test harness.
// All functions are static inline so they can be included directly
// by native unit tests (test_build_src = no — src/ is not compiled).

// Clamp value to [min_val, max_val].
static inline int32_t test_harness_clamp(int32_t value, int32_t min_val, int32_t max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

// Return true if value is in the inclusive range [0.0, 100.0].
// Returns false for NaN because NaN comparisons always evaluate to false.
static inline bool test_harness_is_valid_percentage(float value) {
    return (value >= 0.0f) && (value <= 100.0f);
}

// Linear map of value from [in_min, in_max] to [out_min, out_max].
// If in_min == in_max the degenerate case returns out_min.
static inline float test_harness_map_range(float value,
                                            float in_min, float in_max,
                                            float out_min, float out_max) {
    if (in_min == in_max) return out_min;
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

#endif // TEST_HARNESS_UTILS_H
