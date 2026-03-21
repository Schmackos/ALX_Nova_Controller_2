// test_hal_coord.cpp
// Tests for HalCoordState toggle queue — device-agnostic deferred lifecycle.
//
// Verifies the multi-slot toggle queue (replaces single-slot PendingDeviceToggle
// in DacState that silently dropped concurrent requests). Board- and
// component-agnostic: any device type (DAC, ADC, DSP, etc.) can use this.

#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/state/hal_coord_state.h"

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Test: defaults — empty queue
// ---------------------------------------------------------------------------

void test_hal_coord_defaults(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();
    TEST_ASSERT_EQUAL_UINT8(0, hc.pendingToggleCount());
    TEST_ASSERT_FALSE(hc.hasPendingToggles());
}

// ---------------------------------------------------------------------------
// Test: enqueue a single toggle
// ---------------------------------------------------------------------------

void test_hal_coord_single_toggle(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();
    TEST_ASSERT_TRUE(hc.requestDeviceToggle(3, 1));
    TEST_ASSERT_EQUAL_UINT8(1, hc.pendingToggleCount());
    TEST_ASSERT_TRUE(hc.hasPendingToggles());

    PendingDeviceToggle t = hc.pendingToggleAt(0);
    TEST_ASSERT_EQUAL_UINT8(3, t.halSlot);
    TEST_ASSERT_EQUAL_INT8(1, t.action);
}

// ---------------------------------------------------------------------------
// Test: multiple different devices — core bug fix verification
// ---------------------------------------------------------------------------

void test_hal_coord_multiple_devices(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();
    TEST_ASSERT_TRUE(hc.requestDeviceToggle(0, 1));   // enable slot 0
    TEST_ASSERT_TRUE(hc.requestDeviceToggle(2, -1));   // disable slot 2
    TEST_ASSERT_TRUE(hc.requestDeviceToggle(5, 1));    // enable slot 5

    TEST_ASSERT_EQUAL_UINT8(3, hc.pendingToggleCount());

    PendingDeviceToggle t0 = hc.pendingToggleAt(0);
    TEST_ASSERT_EQUAL_UINT8(0, t0.halSlot);
    TEST_ASSERT_EQUAL_INT8(1, t0.action);

    PendingDeviceToggle t1 = hc.pendingToggleAt(1);
    TEST_ASSERT_EQUAL_UINT8(2, t1.halSlot);
    TEST_ASSERT_EQUAL_INT8(-1, t1.action);

    PendingDeviceToggle t2 = hc.pendingToggleAt(2);
    TEST_ASSERT_EQUAL_UINT8(5, t2.halSlot);
    TEST_ASSERT_EQUAL_INT8(1, t2.action);
}

// ---------------------------------------------------------------------------
// Test: dedup — same slot updates action instead of adding duplicate
// ---------------------------------------------------------------------------

void test_hal_coord_dedup_same_slot(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();
    TEST_ASSERT_TRUE(hc.requestDeviceToggle(5, 1));    // enable
    TEST_ASSERT_TRUE(hc.requestDeviceToggle(5, -1));   // disable same slot

    TEST_ASSERT_EQUAL_UINT8(1, hc.pendingToggleCount()); // still 1, not 2

    PendingDeviceToggle t = hc.pendingToggleAt(0);
    TEST_ASSERT_EQUAL_UINT8(5, t.halSlot);
    TEST_ASSERT_EQUAL_INT8(-1, t.action);  // updated to disable
}

// ---------------------------------------------------------------------------
// Test: overflow — capacity+1 request returns false
// ---------------------------------------------------------------------------

void test_hal_coord_overflow(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();

    // Fill to capacity (each different slot to avoid dedup)
    for (uint8_t i = 0; i < PENDING_TOGGLE_CAPACITY; i++) {
        TEST_ASSERT_TRUE(hc.requestDeviceToggle(i, 1));
    }
    TEST_ASSERT_EQUAL_UINT8(PENDING_TOGGLE_CAPACITY, hc.pendingToggleCount());

    // One more should overflow
    TEST_ASSERT_FALSE(hc.requestDeviceToggle(PENDING_TOGGLE_CAPACITY, 1));
    TEST_ASSERT_EQUAL_UINT8(PENDING_TOGGLE_CAPACITY, hc.pendingToggleCount());
}

// ---------------------------------------------------------------------------
// Test: clear resets everything
// ---------------------------------------------------------------------------

void test_hal_coord_clear(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();
    hc.requestDeviceToggle(1, 1);
    hc.requestDeviceToggle(2, -1);
    TEST_ASSERT_EQUAL_UINT8(2, hc.pendingToggleCount());

    hc.clearPendingToggles();
    TEST_ASSERT_EQUAL_UINT8(0, hc.pendingToggleCount());
    TEST_ASSERT_FALSE(hc.hasPendingToggles());
}

// ---------------------------------------------------------------------------
// Test: hasPendingToggles lifecycle
// ---------------------------------------------------------------------------

void test_hal_coord_has_pending(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();
    TEST_ASSERT_FALSE(hc.hasPendingToggles());

    hc.requestDeviceToggle(0, 1);
    TEST_ASSERT_TRUE(hc.hasPendingToggles());

    hc.clearPendingToggles();
    TEST_ASSERT_FALSE(hc.hasPendingToggles());
}

// ---------------------------------------------------------------------------
// Test: rejects invalid slot (0xFF)
// ---------------------------------------------------------------------------

void test_hal_coord_rejects_invalid_slot(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();
    TEST_ASSERT_FALSE(hc.requestDeviceToggle(0xFF, 1));
    TEST_ASSERT_EQUAL_UINT8(0, hc.pendingToggleCount());
}

// ---------------------------------------------------------------------------
// Test: rejects invalid action (out of range)
// ---------------------------------------------------------------------------

void test_hal_coord_rejects_invalid_action(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();
    TEST_ASSERT_FALSE(hc.requestDeviceToggle(0, 2));
    TEST_ASSERT_EQUAL_UINT8(0, hc.pendingToggleCount());

    TEST_ASSERT_FALSE(hc.requestDeviceToggle(0, -2));
    TEST_ASSERT_EQUAL_UINT8(0, hc.pendingToggleCount());
}

// ---------------------------------------------------------------------------
// Test: valid request after invalid doesn't corrupt state
// ---------------------------------------------------------------------------

void test_hal_coord_valid_after_invalid(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();

    TEST_ASSERT_FALSE(hc.requestDeviceToggle(0xFF, 1)); // invalid
    TEST_ASSERT_FALSE(hc.requestDeviceToggle(0, 5));     // invalid
    TEST_ASSERT_TRUE(hc.requestDeviceToggle(3, -1));     // valid

    TEST_ASSERT_EQUAL_UINT8(1, hc.pendingToggleCount());
    PendingDeviceToggle t = hc.pendingToggleAt(0);
    TEST_ASSERT_EQUAL_UINT8(3, t.halSlot);
    TEST_ASSERT_EQUAL_INT8(-1, t.action);
}

// ---------------------------------------------------------------------------
// Test: pendingToggleAt out of bounds returns default
// ---------------------------------------------------------------------------

void test_hal_coord_at_out_of_bounds(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();
    hc.requestDeviceToggle(1, 1);

    PendingDeviceToggle t = hc.pendingToggleAt(99);
    TEST_ASSERT_EQUAL_UINT8(0xFF, t.halSlot);
    TEST_ASSERT_EQUAL_INT8(0, t.action);
}

// ---------------------------------------------------------------------------
// Test: fill to capacity, drain, refill
// ---------------------------------------------------------------------------

void test_hal_coord_fill_and_drain(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();

    // Fill
    for (uint8_t i = 0; i < PENDING_TOGGLE_CAPACITY; i++) {
        TEST_ASSERT_TRUE(hc.requestDeviceToggle(i, (i % 2 == 0) ? 1 : -1));
    }
    TEST_ASSERT_EQUAL_UINT8(PENDING_TOGGLE_CAPACITY, hc.pendingToggleCount());

    // Drain
    hc.clearPendingToggles();
    TEST_ASSERT_EQUAL_UINT8(0, hc.pendingToggleCount());

    // Refill with different data
    for (uint8_t i = 0; i < PENDING_TOGGLE_CAPACITY; i++) {
        TEST_ASSERT_TRUE(hc.requestDeviceToggle(i + 10, -1));
    }
    TEST_ASSERT_EQUAL_UINT8(PENDING_TOGGLE_CAPACITY, hc.pendingToggleCount());

    PendingDeviceToggle t0 = hc.pendingToggleAt(0);
    TEST_ASSERT_EQUAL_UINT8(10, t0.halSlot);
    TEST_ASSERT_EQUAL_INT8(-1, t0.action);
}

// ---------------------------------------------------------------------------
// Test: overflow counter increments on each dropped request
// ---------------------------------------------------------------------------

void test_hal_coord_overflow_counter(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();

    TEST_ASSERT_EQUAL_UINT32(0, hc.overflowCount());

    // Fill to capacity
    for (uint8_t i = 0; i < PENDING_TOGGLE_CAPACITY; i++) {
        TEST_ASSERT_TRUE(hc.requestDeviceToggle(i, 1));
    }
    TEST_ASSERT_EQUAL_UINT32(0, hc.overflowCount());

    // 3 overflow attempts
    TEST_ASSERT_FALSE(hc.requestDeviceToggle(PENDING_TOGGLE_CAPACITY, 1));
    TEST_ASSERT_FALSE(hc.requestDeviceToggle(PENDING_TOGGLE_CAPACITY + 1, -1));
    TEST_ASSERT_FALSE(hc.requestDeviceToggle(PENDING_TOGGLE_CAPACITY + 2, 1));

    TEST_ASSERT_EQUAL_UINT32(3, hc.overflowCount());
}

// ---------------------------------------------------------------------------
// Test: overflow flag is set on overflow
// ---------------------------------------------------------------------------

void test_hal_coord_overflow_flag_set(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();

    // No overflow yet — flag should be false
    TEST_ASSERT_FALSE(hc.consumeOverflowFlag());

    // Fill to capacity and trigger overflow
    for (uint8_t i = 0; i < PENDING_TOGGLE_CAPACITY; i++) {
        hc.requestDeviceToggle(i, 1);
    }
    hc.requestDeviceToggle(PENDING_TOGGLE_CAPACITY, 1);

    TEST_ASSERT_TRUE(hc.consumeOverflowFlag());
}

// ---------------------------------------------------------------------------
// Test: consumeOverflowFlag clears on read, counter persists
// ---------------------------------------------------------------------------

void test_hal_coord_overflow_flag_consume_clears(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();

    // Fill and overflow
    for (uint8_t i = 0; i < PENDING_TOGGLE_CAPACITY; i++) {
        hc.requestDeviceToggle(i, 1);
    }
    hc.requestDeviceToggle(PENDING_TOGGLE_CAPACITY, 1);

    // First consume returns true
    TEST_ASSERT_TRUE(hc.consumeOverflowFlag());
    // Second consume returns false (cleared)
    TEST_ASSERT_FALSE(hc.consumeOverflowFlag());
    // Counter still reflects the overflow
    TEST_ASSERT_EQUAL_UINT32(1, hc.overflowCount());
}

// ---------------------------------------------------------------------------
// Test: overflow counter survives clear — lifetime accumulator
// ---------------------------------------------------------------------------

void test_hal_coord_overflow_counter_survives_clear(void) {
    HalCoordState hc = {};
    hc.clearPendingToggles();

    // Fill and overflow once
    for (uint8_t i = 0; i < PENDING_TOGGLE_CAPACITY; i++) {
        hc.requestDeviceToggle(i, 1);
    }
    hc.requestDeviceToggle(PENDING_TOGGLE_CAPACITY, 1);
    TEST_ASSERT_EQUAL_UINT32(1, hc.overflowCount());

    // Drain (clear) and refill
    hc.clearPendingToggles();
    for (uint8_t i = 0; i < PENDING_TOGGLE_CAPACITY; i++) {
        hc.requestDeviceToggle(i + 20, -1);
    }

    // Overflow again
    hc.requestDeviceToggle(PENDING_TOGGLE_CAPACITY + 20, -1);
    hc.requestDeviceToggle(PENDING_TOGGLE_CAPACITY + 21, 1);

    // Counter accumulates across clear cycles
    TEST_ASSERT_EQUAL_UINT32(3, hc.overflowCount());
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_hal_coord_defaults);
    RUN_TEST(test_hal_coord_single_toggle);
    RUN_TEST(test_hal_coord_multiple_devices);
    RUN_TEST(test_hal_coord_dedup_same_slot);
    RUN_TEST(test_hal_coord_overflow);
    RUN_TEST(test_hal_coord_clear);
    RUN_TEST(test_hal_coord_has_pending);
    RUN_TEST(test_hal_coord_rejects_invalid_slot);
    RUN_TEST(test_hal_coord_rejects_invalid_action);
    RUN_TEST(test_hal_coord_valid_after_invalid);
    RUN_TEST(test_hal_coord_at_out_of_bounds);
    RUN_TEST(test_hal_coord_fill_and_drain);
    RUN_TEST(test_hal_coord_overflow_counter);
    RUN_TEST(test_hal_coord_overflow_flag_set);
    RUN_TEST(test_hal_coord_overflow_flag_consume_clears);
    RUN_TEST(test_hal_coord_overflow_counter_survives_clear);

    return UNITY_END();
}
