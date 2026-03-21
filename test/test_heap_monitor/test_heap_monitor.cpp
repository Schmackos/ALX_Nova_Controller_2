// test_heap_monitor.cpp
// Tests for heap monitoring thresholds and state management (Phase 1).
//
// Verifies HEAP_WARNING_THRESHOLD / HEAP_CRITICAL_THRESHOLD ordering,
// DebugState flag transitions, and diagnostic error code values.

#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/config.h"
#include "../../src/state/debug_state.h"
#include "../../src/diag_error_codes.h"

// ---------------------------------------------------------------------------
// Helper -- replicates the main.cpp heap-check logic in isolation
// ---------------------------------------------------------------------------

static void update_heap_state(DebugState& debug, uint32_t maxBlock) {
    debug.heapCritical = (maxBlock < HEAP_CRITICAL_THRESHOLD);
    debug.heapWarning  = !debug.heapCritical && (maxBlock < HEAP_WARNING_THRESHOLD);
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Test 1: Compile-time threshold ordering
// ---------------------------------------------------------------------------

void test_thresholds_ordering(void) {
    static_assert(HEAP_CRITICAL_THRESHOLD < HEAP_WARNING_THRESHOLD,
                  "CRITICAL must be below WARNING");
    TEST_ASSERT_TRUE(HEAP_CRITICAL_THRESHOLD < HEAP_WARNING_THRESHOLD);
}

// ---------------------------------------------------------------------------
// Test 2: Normal state -- maxBlock well above both thresholds
// ---------------------------------------------------------------------------

void test_normal_state(void) {
    DebugState debug = {};
    update_heap_state(debug, 60000);

    TEST_ASSERT_FALSE(debug.heapWarning);
    TEST_ASSERT_FALSE(debug.heapCritical);
}

// ---------------------------------------------------------------------------
// Test 3: Warning state -- between critical and warning thresholds
// ---------------------------------------------------------------------------

void test_warning_state(void) {
    DebugState debug = {};
    update_heap_state(debug, 45000);

    TEST_ASSERT_TRUE(debug.heapWarning);
    TEST_ASSERT_FALSE(debug.heapCritical);
}

// ---------------------------------------------------------------------------
// Test 4: Critical state -- below critical threshold
// ---------------------------------------------------------------------------

void test_critical_state(void) {
    DebugState debug = {};
    update_heap_state(debug, 30000);

    TEST_ASSERT_FALSE(debug.heapWarning);
    TEST_ASSERT_TRUE(debug.heapCritical);
}

// ---------------------------------------------------------------------------
// Test 5: Warning clears on recovery
// ---------------------------------------------------------------------------

void test_warning_clears_on_recovery(void) {
    DebugState debug = {};

    update_heap_state(debug, 45000);
    TEST_ASSERT_TRUE(debug.heapWarning);

    update_heap_state(debug, 55000);
    TEST_ASSERT_FALSE(debug.heapWarning);
    TEST_ASSERT_FALSE(debug.heapCritical);
}

// ---------------------------------------------------------------------------
// Test 6: Critical clears on recovery
// ---------------------------------------------------------------------------

void test_critical_clears_on_recovery(void) {
    DebugState debug = {};

    update_heap_state(debug, 30000);
    TEST_ASSERT_TRUE(debug.heapCritical);

    update_heap_state(debug, 55000);
    TEST_ASSERT_FALSE(debug.heapCritical);
    TEST_ASSERT_FALSE(debug.heapWarning);
}

// ---------------------------------------------------------------------------
// Test 7: Critical directly to normal clears both flags
// ---------------------------------------------------------------------------

void test_critical_to_normal_clears_both(void) {
    DebugState debug = {};

    update_heap_state(debug, 20000);
    TEST_ASSERT_TRUE(debug.heapCritical);
    TEST_ASSERT_FALSE(debug.heapWarning);

    update_heap_state(debug, 100000);
    TEST_ASSERT_FALSE(debug.heapCritical);
    TEST_ASSERT_FALSE(debug.heapWarning);
}

// ---------------------------------------------------------------------------
// Test 8: Warning to critical transition
// ---------------------------------------------------------------------------

void test_warning_to_critical_transition(void) {
    DebugState debug = {};

    update_heap_state(debug, 45000);
    TEST_ASSERT_TRUE(debug.heapWarning);
    TEST_ASSERT_FALSE(debug.heapCritical);

    update_heap_state(debug, 30000);
    TEST_ASSERT_FALSE(debug.heapWarning);
    TEST_ASSERT_TRUE(debug.heapCritical);
}

// ---------------------------------------------------------------------------
// Test 9: Critical to warning transition
// ---------------------------------------------------------------------------

void test_critical_to_warning_transition(void) {
    DebugState debug = {};

    update_heap_state(debug, 30000);
    TEST_ASSERT_TRUE(debug.heapCritical);

    update_heap_state(debug, 45000);
    TEST_ASSERT_FALSE(debug.heapCritical);
    TEST_ASSERT_TRUE(debug.heapWarning);
}

// ---------------------------------------------------------------------------
// Test 10: Diagnostic error codes exist with expected values
// ---------------------------------------------------------------------------

void test_diag_error_codes_exist(void) {
    TEST_ASSERT_EQUAL_HEX16(0x0107, DIAG_SYS_HEAP_WARNING);
    TEST_ASSERT_EQUAL_HEX16(0x0108, DIAG_SYS_HEAP_WARNING_CLEARED);
    TEST_ASSERT_EQUAL_HEX16(0x0101, DIAG_SYS_HEAP_CRITICAL);
    TEST_ASSERT_EQUAL_HEX16(0x0102, DIAG_SYS_HEAP_RECOVERED);
}

// ---------------------------------------------------------------------------
// Test 11: Boundary -- exactly at warning threshold
// ---------------------------------------------------------------------------

void test_boundary_at_warning_threshold(void) {
    DebugState debug = {};
    update_heap_state(debug, HEAP_WARNING_THRESHOLD);

    TEST_ASSERT_FALSE(debug.heapWarning);
    TEST_ASSERT_FALSE(debug.heapCritical);
}

// ---------------------------------------------------------------------------
// Test 12: Boundary -- one byte below warning threshold
// ---------------------------------------------------------------------------

void test_boundary_below_warning_threshold(void) {
    DebugState debug = {};
    update_heap_state(debug, HEAP_WARNING_THRESHOLD - 1);

    TEST_ASSERT_TRUE(debug.heapWarning);
    TEST_ASSERT_FALSE(debug.heapCritical);
}

// ---------------------------------------------------------------------------
// Test 13: Boundary -- exactly at critical threshold
// ---------------------------------------------------------------------------

void test_boundary_at_critical_threshold(void) {
    DebugState debug = {};
    update_heap_state(debug, HEAP_CRITICAL_THRESHOLD);

    // At threshold (not below) -- falls in warning band
    TEST_ASSERT_TRUE(debug.heapWarning);
    TEST_ASSERT_FALSE(debug.heapCritical);
}

// ---------------------------------------------------------------------------
// Test 14: Boundary -- one byte below critical threshold
// ---------------------------------------------------------------------------

void test_boundary_below_critical_threshold(void) {
    DebugState debug = {};
    update_heap_state(debug, HEAP_CRITICAL_THRESHOLD - 1);

    TEST_ASSERT_FALSE(debug.heapWarning);
    TEST_ASSERT_TRUE(debug.heapCritical);
}

// ---------------------------------------------------------------------------
// Test 15: Zero maxBlock -- critical
// ---------------------------------------------------------------------------

void test_zero_max_block(void) {
    DebugState debug = {};
    update_heap_state(debug, 0);

    TEST_ASSERT_TRUE(debug.heapCritical);
    TEST_ASSERT_FALSE(debug.heapWarning);
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_thresholds_ordering);
    RUN_TEST(test_normal_state);
    RUN_TEST(test_warning_state);
    RUN_TEST(test_critical_state);
    RUN_TEST(test_warning_clears_on_recovery);
    RUN_TEST(test_critical_clears_on_recovery);
    RUN_TEST(test_critical_to_normal_clears_both);
    RUN_TEST(test_warning_to_critical_transition);
    RUN_TEST(test_critical_to_warning_transition);
    RUN_TEST(test_diag_error_codes_exist);
    RUN_TEST(test_boundary_at_warning_threshold);
    RUN_TEST(test_boundary_below_warning_threshold);
    RUN_TEST(test_boundary_at_critical_threshold);
    RUN_TEST(test_boundary_below_critical_threshold);
    RUN_TEST(test_zero_max_block);

    return UNITY_END();
}
