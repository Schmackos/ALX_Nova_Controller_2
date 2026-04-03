#ifndef TEST_HARNESS_UTILS_H
#define TEST_HARNESS_UTILS_H

/*
 * test_harness_utils.h — Pure inline utility functions for test harness use.
 *
 * TEST/VALIDATION ARTIFACT — not production code.
 * All identifiers carry the test_harness_ prefix for safe grep-and-delete
 * cleanup.  Functions are fully inline so that native test builds (which use
 * test_build_src = no and therefore cannot compile src/) can still include
 * this header without linker errors.
 *
 * C99/C11 compatible.  No Arduino, FreeRTOS, or ESP-IDF dependencies.
 */

#include <math.h>   /* isnan(), isinf() */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * test_harness_clamp — Clamp `value` to the closed interval [lo, hi].
 *
 * Returns lo when value < lo, hi when value > hi, otherwise value.
 * NaN input propagates as-is (comparison with NaN is always false, so the
 * function returns the unmodified value rather than silently clamping to a
 * boundary).
 */
static inline double test_harness_clamp(double value, double lo, double hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

/*
 * test_harness_is_valid_percentage — Returns non-zero if value is in [0, 100].
 *
 * NaN and infinity return 0 (invalid).
 */
static inline int test_harness_is_valid_percentage(double value)
{
    if (isnan(value) || isinf(value)) return 0;
    return (value >= 0.0 && value <= 100.0);
}

/*
 * test_harness_map_range — Linear map from [in_min, in_max] to [out_min, out_max].
 *
 * Equivalent to Arduino's map() but operates on doubles and is numerically
 * correct for floating-point ranges.
 *
 * Edge cases:
 *   - in_min == in_max (zero-width input range): returns out_min to avoid
 *     division by zero.
 *   - NaN or infinity in any argument: result is unspecified but will not
 *     trap or invoke undefined behaviour — standard IEEE 754 propagation
 *     applies.
 */
static inline double test_harness_map_range(double value,
                                            double in_min,  double in_max,
                                            double out_min, double out_max)
{
    double in_span = in_max - in_min;
    if (in_span == 0.0) return out_min;
    return out_min + (value - in_min) * (out_max - out_min) / in_span;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TEST_HARNESS_UTILS_H */
