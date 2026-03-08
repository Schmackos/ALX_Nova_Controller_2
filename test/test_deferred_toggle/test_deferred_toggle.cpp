/**
 * test_deferred_toggle.cpp
 *
 * Unit tests for DAC / ES8311 deferred toggle logic.
 *
 * Tests the requestDacToggle() and requestEs8311Toggle() validated setters
 * on the _pendingDacToggle / _pendingEs8311Toggle volatile int8_t fields
 * in AppState. These flags are consumed by the main loop to safely
 * init/deinit DAC-path devices without audio task races.
 *
 * Covers: input validation (-1, 0, 1 accepted; others rejected),
 *         last-writer-wins semantics, and post-processing clear.
 */

#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Minimal AppState mock for deferred toggle testing =====
// Mirrors the _pendingDacToggle / _pendingEs8311Toggle fields and
// requestDacToggle() / requestEs8311Toggle() setters from src/app_state.h
// without pulling in WiFi, PubSubClient, WebServer, or DSP dependencies.

struct MockAppState {
    volatile int8_t _pendingDacToggle = 0;
    volatile int8_t _pendingEs8311Toggle = 0;

    // Validated setters — identical to AppState implementation
    void requestDacToggle(int8_t action) {
        if (action >= -1 && action <= 1) _pendingDacToggle = action;
    }

    void requestEs8311Toggle(int8_t action) {
        if (action >= -1 && action <= 1) _pendingEs8311Toggle = action;
    }

    void reset() {
        _pendingDacToggle = 0;
        _pendingEs8311Toggle = 0;
    }
};

static MockAppState appState;

// ===== Test Setup =====

void setUp(void) {
    appState.reset();
}

void tearDown(void) {}

// ===== requestDacToggle — valid values =====

void test_requestDacToggle_enable_sets_flag_to_1(void) {
    appState.requestDacToggle(1);
    TEST_ASSERT_EQUAL_INT8(1, appState._pendingDacToggle);
}

void test_requestDacToggle_disable_sets_flag_to_minus1(void) {
    appState.requestDacToggle(-1);
    TEST_ASSERT_EQUAL_INT8(-1, appState._pendingDacToggle);
}

void test_requestDacToggle_clear_sets_flag_to_0(void) {
    // Pre-set to non-zero so we can confirm it gets written to 0
    appState._pendingDacToggle = 1;
    appState.requestDacToggle(0);
    TEST_ASSERT_EQUAL_INT8(0, appState._pendingDacToggle);
}

// ===== requestDacToggle — invalid values rejected =====

void test_requestDacToggle_rejects_invalid_values(void) {
    // Set a known baseline so we can verify it stays unchanged
    appState._pendingDacToggle = 1;

    // Each invalid value must leave the field unchanged
    int8_t invalid[] = {2, -2, 127, -128, 3, -3, 10, -10};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        appState.requestDacToggle(invalid[i]);
        TEST_ASSERT_EQUAL_INT8_MESSAGE(1, appState._pendingDacToggle,
                                       "Invalid value should not modify flag");
    }
}

// ===== requestEs8311Toggle — valid values =====

void test_requestEs8311Toggle_enable_sets_flag_to_1(void) {
    appState.requestEs8311Toggle(1);
    TEST_ASSERT_EQUAL_INT8(1, appState._pendingEs8311Toggle);
}

void test_requestEs8311Toggle_disable_sets_flag_to_minus1(void) {
    appState.requestEs8311Toggle(-1);
    TEST_ASSERT_EQUAL_INT8(-1, appState._pendingEs8311Toggle);
}

// ===== Double-toggle: last writer wins =====

void test_double_toggle_last_value_wins(void) {
    // Simulate two rapid requests before main loop processes
    appState.requestDacToggle(1);
    TEST_ASSERT_EQUAL_INT8(1, appState._pendingDacToggle);

    appState.requestDacToggle(-1);
    TEST_ASSERT_EQUAL_INT8(-1, appState._pendingDacToggle);
}

// ===== Flag cleared after processing =====

void test_flag_cleared_after_processing(void) {
    // Simulate: request toggle, main loop reads and acts, then clears
    appState.requestDacToggle(1);
    TEST_ASSERT_EQUAL_INT8(1, appState._pendingDacToggle);

    // Main loop would read the value, perform the action, then clear:
    int8_t action = appState._pendingDacToggle;
    appState._pendingDacToggle = 0;

    TEST_ASSERT_EQUAL_INT8(1, action);
    TEST_ASSERT_EQUAL_INT8(0, appState._pendingDacToggle);
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // DAC toggle — valid values
    RUN_TEST(test_requestDacToggle_enable_sets_flag_to_1);
    RUN_TEST(test_requestDacToggle_disable_sets_flag_to_minus1);
    RUN_TEST(test_requestDacToggle_clear_sets_flag_to_0);

    // DAC toggle — invalid values rejected
    RUN_TEST(test_requestDacToggle_rejects_invalid_values);

    // ES8311 toggle — valid values
    RUN_TEST(test_requestEs8311Toggle_enable_sets_flag_to_1);
    RUN_TEST(test_requestEs8311Toggle_disable_sets_flag_to_minus1);

    // Double-toggle semantics
    RUN_TEST(test_double_toggle_last_value_wins);

    // Flag lifecycle
    RUN_TEST(test_flag_cleared_after_processing);

    return UNITY_END();
}
