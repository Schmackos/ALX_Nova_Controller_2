#include <cstring>
#include <string>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Minimal AppState mock for dim timeout testing =====
// Mirrors relevant fields from src/app_state.h without pulling in
// ESP32 dependencies (WiFi, PubSubClient, WebServer, etc.)

class MockAppState {
public:
    bool dimEnabled = false;
    unsigned long dimTimeout = 10000;
    unsigned long screenTimeout = 60000;
    uint8_t backlightBrightness = 255;
    uint8_t dimBrightness = 26;
    bool backlightOn = true;

    void setDimEnabled(bool enabled) {
        if (dimEnabled != enabled) {
            dimEnabled = enabled;
            _displayDirty = true;
        }
    }

    void setDimTimeout(unsigned long timeout) {
        if (dimTimeout != timeout) {
            dimTimeout = timeout;
            _displayDirty = true;
        }
    }

    void setDimBrightness(uint8_t brightness) {
        if (brightness < 1) brightness = 1;
        if (dimBrightness != brightness) {
            dimBrightness = brightness;
            _displayDirty = true;
        }
    }

    void setBacklightOn(bool state) {
        if (backlightOn != state) {
            backlightOn = state;
            _displayDirty = true;
        }
    }

    bool isDisplayDirty() const { return _displayDirty; }
    void clearDisplayDirty() { _displayDirty = false; }

    void reset() {
        dimEnabled = false;
        dimTimeout = 10000;
        screenTimeout = 60000;
        backlightBrightness = 255;
        dimBrightness = 26;
        backlightOn = true;
        _displayDirty = false;
    }

private:
    bool _displayDirty = false;
};

static MockAppState appState;

// ===== Dim state machine simulation =====
// Mirrors the logic in gui_manager.cpp gui_task loop for unit testing
// without pulling in LVGL, TFT_eSPI, or FreeRTOS dependencies.

static bool sim_screen_awake = true;
static bool sim_screen_dimmed = false;
static unsigned long sim_last_activity = 0;
static uint8_t sim_backlight_level = 255;

static void sim_reset(void) {
    sim_screen_awake = true;
    sim_screen_dimmed = false;
    sim_last_activity = 0;
    sim_backlight_level = 255;
    ArduinoMock::mockMillis = 0;
}

static void sim_set_backlight(uint8_t level) {
    sim_backlight_level = level;
}

static void sim_screen_dim(void) {
    if (sim_screen_dimmed || !sim_screen_awake) return;
    sim_screen_dimmed = true;
    sim_set_backlight(appState.dimBrightness);
}

static void sim_screen_sleep(void) {
    if (!sim_screen_awake) return;
    sim_screen_awake = false;
    sim_screen_dimmed = false;
    sim_set_backlight(0);
}

static void sim_screen_wake(void) {
    sim_last_activity = millis();
    sim_screen_dimmed = false;
    if (sim_screen_awake) return;
    sim_screen_awake = true;
    sim_set_backlight(appState.backlightBrightness);
}

static void sim_activity(void) {
    if (sim_screen_dimmed) {
        sim_screen_dimmed = false;
        sim_set_backlight(appState.backlightBrightness);
        sim_last_activity = millis();
    } else {
        sim_screen_wake();
    }
}

// Run one iteration of the dim/sleep timeout logic (mirrors gui_task)
static void sim_tick(void) {
    // Dim timeout check (requires dimEnabled)
    unsigned long dim_ms = appState.dimTimeout;
    if (sim_screen_awake && !sim_screen_dimmed && appState.dimEnabled && dim_ms > 0) {
        if (millis() - sim_last_activity > dim_ms) {
            sim_screen_dim();
        }
    }

    // Screen timeout check
    unsigned long timeout_ms = appState.screenTimeout;
    if (sim_screen_awake && timeout_ms > 0) {
        if (millis() - sim_last_activity > timeout_ms) {
            sim_screen_sleep();
        }
    }
}

// ===== Test Setup =====

void setUp(void) {
    ArduinoMock::reset();
    appState.reset();
    sim_reset();
}

void tearDown(void) {}

// ===== Dim Enabled AppState Tests =====

void test_dimEnabled_default_is_false(void) {
    TEST_ASSERT_FALSE(appState.dimEnabled);
}

void test_setDimEnabled_stores_value(void) {
    appState.setDimEnabled(true);
    TEST_ASSERT_TRUE(appState.dimEnabled);
}

void test_setDimEnabled_sets_display_dirty(void) {
    appState.clearDisplayDirty();
    appState.setDimEnabled(true);
    TEST_ASSERT_TRUE(appState.isDisplayDirty());
}

void test_setDimEnabled_no_dirty_when_unchanged(void) {
    appState.dimEnabled = true;
    appState.clearDisplayDirty();
    appState.setDimEnabled(true);  // Same value
    TEST_ASSERT_FALSE(appState.isDisplayDirty());
}

// ===== Dim Timeout AppState Tests =====

void test_dimTimeout_default_is_10000(void) {
    TEST_ASSERT_EQUAL_UINT32(10000, appState.dimTimeout);
}

void test_setDimTimeout_stores_value(void) {
    appState.setDimTimeout(5000);
    TEST_ASSERT_EQUAL_UINT32(5000, appState.dimTimeout);
}

void test_setDimTimeout_sets_display_dirty(void) {
    appState.clearDisplayDirty();
    appState.setDimTimeout(5000);
    TEST_ASSERT_TRUE(appState.isDisplayDirty());
}

void test_setDimTimeout_no_dirty_when_unchanged(void) {
    appState.dimTimeout = 5000;
    appState.clearDisplayDirty();
    appState.setDimTimeout(5000);  // Same value
    TEST_ASSERT_FALSE(appState.isDisplayDirty());
}

void test_dimTimeout_valid_values(void) {
    unsigned long valid[] = {5000, 10000, 15000, 30000, 60000};
    for (int i = 0; i < 5; i++) {
        appState.setDimTimeout(valid[i]);
        TEST_ASSERT_EQUAL_UINT32(valid[i], appState.dimTimeout);
    }
}

// ===== Dim Brightness AppState Tests =====

void test_dimBrightness_default_is_26(void) {
    TEST_ASSERT_EQUAL_UINT8(26, appState.dimBrightness);
}

void test_setDimBrightness_stores_value(void) {
    appState.setDimBrightness(128);
    TEST_ASSERT_EQUAL_UINT8(128, appState.dimBrightness);
}

void test_setDimBrightness_sets_display_dirty(void) {
    appState.clearDisplayDirty();
    appState.setDimBrightness(64);
    TEST_ASSERT_TRUE(appState.isDisplayDirty());
}

void test_setDimBrightness_clamps_min(void) {
    appState.setDimBrightness(0);
    TEST_ASSERT_EQUAL_UINT8(1, appState.dimBrightness);
}

void test_dimBrightness_valid_pwm_values(void) {
    uint8_t valid[] = {26, 64, 128, 191};
    for (int i = 0; i < 4; i++) {
        appState.setDimBrightness(valid[i]);
        TEST_ASSERT_EQUAL_UINT8(valid[i], appState.dimBrightness);
    }
}

// ===== Dim State Machine Tests =====

void test_dim_triggers_after_timeout(void) {
    appState.setDimEnabled(true);
    appState.setDimTimeout(5000);

    // Advance time past dim timeout
    ArduinoMock::mockMillis = 5001;
    sim_tick();

    TEST_ASSERT_TRUE(sim_screen_dimmed);
    TEST_ASSERT_TRUE(sim_screen_awake);
    TEST_ASSERT_EQUAL_UINT8(appState.dimBrightness, sim_backlight_level);
}

void test_dim_disabled_when_not_enabled(void) {
    appState.setDimEnabled(false);
    appState.setDimTimeout(5000);
    appState.screenTimeout = 0;  // Disable sleep so screen stays on

    ArduinoMock::mockMillis = 100000;
    sim_tick();

    TEST_ASSERT_FALSE(sim_screen_dimmed);
    TEST_ASSERT_TRUE(sim_screen_awake);
    TEST_ASSERT_EQUAL_UINT8(255, sim_backlight_level);
}

void test_activity_clears_dim_state(void) {
    appState.setDimEnabled(true);
    appState.setDimTimeout(5000);

    // Dim the screen
    ArduinoMock::mockMillis = 5001;
    sim_tick();
    TEST_ASSERT_TRUE(sim_screen_dimmed);
    TEST_ASSERT_EQUAL_UINT8(appState.dimBrightness, sim_backlight_level);

    // Activity restores brightness
    ArduinoMock::mockMillis = 6000;
    sim_activity();
    TEST_ASSERT_FALSE(sim_screen_dimmed);
    TEST_ASSERT_TRUE(sim_screen_awake);
    TEST_ASSERT_EQUAL_UINT8(255, sim_backlight_level);
}

void test_sleep_before_dim_when_shorter(void) {
    appState.setDimEnabled(true);
    appState.setDimTimeout(30000);    // Dim at 30s
    appState.screenTimeout = 10000;   // Sleep at 10s

    // At 10s, sleep fires first — screen should sleep, not dim
    ArduinoMock::mockMillis = 10001;
    sim_tick();

    TEST_ASSERT_FALSE(sim_screen_awake);
    TEST_ASSERT_FALSE(sim_screen_dimmed);
    TEST_ASSERT_EQUAL_UINT8(0, sim_backlight_level);
}

void test_dim_works_with_never_sleep(void) {
    appState.setDimEnabled(true);
    appState.setDimTimeout(5000);
    appState.screenTimeout = 0;  // Never sleep

    ArduinoMock::mockMillis = 5001;
    sim_tick();

    TEST_ASSERT_TRUE(sim_screen_dimmed);
    TEST_ASSERT_TRUE(sim_screen_awake);
    TEST_ASSERT_EQUAL_UINT8(appState.dimBrightness, sim_backlight_level);

    // Even after a long time, should stay dimmed not sleeping
    ArduinoMock::mockMillis = 600000;
    sim_tick();
    TEST_ASSERT_TRUE(sim_screen_awake);
}

void test_wake_from_sleep_clears_dim(void) {
    appState.setDimEnabled(true);
    appState.setDimTimeout(5000);
    appState.screenTimeout = 10000;

    // Dim at 5s
    ArduinoMock::mockMillis = 5001;
    sim_tick();
    TEST_ASSERT_TRUE(sim_screen_dimmed);

    // Sleep at 10s
    ArduinoMock::mockMillis = 10001;
    sim_tick();
    TEST_ASSERT_FALSE(sim_screen_awake);

    // Wake up — should start fresh, no stale dim
    ArduinoMock::mockMillis = 15000;
    sim_screen_wake();
    TEST_ASSERT_TRUE(sim_screen_awake);
    TEST_ASSERT_FALSE(sim_screen_dimmed);
    TEST_ASSERT_EQUAL_UINT8(255, sim_backlight_level);
}

void test_brightness_not_overridden_while_dimmed(void) {
    appState.setDimEnabled(true);
    appState.setDimTimeout(5000);
    appState.backlightBrightness = 200;

    // Dim the screen
    ArduinoMock::mockMillis = 5001;
    sim_tick();
    TEST_ASSERT_EQUAL_UINT8(appState.dimBrightness, sim_backlight_level);

    // Change brightness in AppState (e.g. from web UI) — dim should hold
    appState.backlightBrightness = 180;
    // The gui_task skips brightness updates while dimmed
    if (sim_screen_awake && !sim_screen_dimmed) {
        sim_set_backlight(appState.backlightBrightness);
    }
    TEST_ASSERT_EQUAL_UINT8(appState.dimBrightness, sim_backlight_level);
}

void test_dim_not_triggered_before_timeout(void) {
    appState.setDimEnabled(true);
    appState.setDimTimeout(10000);

    // Just before timeout
    ArduinoMock::mockMillis = 9999;
    sim_tick();

    TEST_ASSERT_FALSE(sim_screen_dimmed);
    TEST_ASSERT_EQUAL_UINT8(255, sim_backlight_level);
}

void test_dim_then_sleep_sequence(void) {
    appState.setDimEnabled(true);
    appState.setDimTimeout(5000);
    appState.screenTimeout = 30000;

    // Before dim — normal
    ArduinoMock::mockMillis = 4000;
    sim_tick();
    TEST_ASSERT_FALSE(sim_screen_dimmed);
    TEST_ASSERT_TRUE(sim_screen_awake);

    // After dim timeout — dimmed
    ArduinoMock::mockMillis = 5001;
    sim_tick();
    TEST_ASSERT_TRUE(sim_screen_dimmed);
    TEST_ASSERT_TRUE(sim_screen_awake);
    TEST_ASSERT_EQUAL_UINT8(appState.dimBrightness, sim_backlight_level);

    // After screen timeout — asleep
    ArduinoMock::mockMillis = 30001;
    sim_tick();
    TEST_ASSERT_FALSE(sim_screen_awake);
    TEST_ASSERT_EQUAL_UINT8(0, sim_backlight_level);
}

void test_activity_resets_dim_timer(void) {
    appState.setDimEnabled(true);
    appState.setDimTimeout(5000);

    // Advance to just before dim
    ArduinoMock::mockMillis = 4000;
    sim_tick();
    TEST_ASSERT_FALSE(sim_screen_dimmed);

    // Activity at 4s resets timer
    sim_activity();

    // 4s after activity (at 8s) — should not be dimmed yet
    ArduinoMock::mockMillis = 8000;
    sim_tick();
    TEST_ASSERT_FALSE(sim_screen_dimmed);

    // 5s after activity (at 9001) — should dim
    ArduinoMock::mockMillis = 9001;
    sim_tick();
    TEST_ASSERT_TRUE(sim_screen_dimmed);
}

void test_custom_dim_brightness_used_during_dim(void) {
    appState.setDimEnabled(true);
    appState.setDimBrightness(128);  // 50%
    appState.setDimTimeout(5000);

    ArduinoMock::mockMillis = 5001;
    sim_tick();

    TEST_ASSERT_TRUE(sim_screen_dimmed);
    TEST_ASSERT_EQUAL_UINT8(128, sim_backlight_level);
}

void test_enabling_dim_allows_dimming(void) {
    appState.setDimEnabled(false);
    appState.setDimTimeout(5000);

    // Should not dim when disabled
    ArduinoMock::mockMillis = 5001;
    sim_tick();
    TEST_ASSERT_FALSE(sim_screen_dimmed);

    // Enable dim
    appState.setDimEnabled(true);
    sim_last_activity = 0;  // Reset activity timer
    ArduinoMock::mockMillis = 5001;
    sim_tick();
    TEST_ASSERT_TRUE(sim_screen_dimmed);
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Dim Enabled AppState tests
    RUN_TEST(test_dimEnabled_default_is_false);
    RUN_TEST(test_setDimEnabled_stores_value);
    RUN_TEST(test_setDimEnabled_sets_display_dirty);
    RUN_TEST(test_setDimEnabled_no_dirty_when_unchanged);

    // Dim Timeout AppState tests
    RUN_TEST(test_dimTimeout_default_is_10000);
    RUN_TEST(test_setDimTimeout_stores_value);
    RUN_TEST(test_setDimTimeout_sets_display_dirty);
    RUN_TEST(test_setDimTimeout_no_dirty_when_unchanged);
    RUN_TEST(test_dimTimeout_valid_values);

    // Dim Brightness AppState tests
    RUN_TEST(test_dimBrightness_default_is_26);
    RUN_TEST(test_setDimBrightness_stores_value);
    RUN_TEST(test_setDimBrightness_sets_display_dirty);
    RUN_TEST(test_setDimBrightness_clamps_min);
    RUN_TEST(test_dimBrightness_valid_pwm_values);

    // Dim state machine tests
    RUN_TEST(test_dim_triggers_after_timeout);
    RUN_TEST(test_dim_disabled_when_not_enabled);
    RUN_TEST(test_activity_clears_dim_state);
    RUN_TEST(test_sleep_before_dim_when_shorter);
    RUN_TEST(test_dim_works_with_never_sleep);
    RUN_TEST(test_wake_from_sleep_clears_dim);
    RUN_TEST(test_brightness_not_overridden_while_dimmed);
    RUN_TEST(test_dim_not_triggered_before_timeout);
    RUN_TEST(test_dim_then_sleep_sequence);
    RUN_TEST(test_activity_resets_dim_timer);
    RUN_TEST(test_custom_dim_brightness_used_during_dim);
    RUN_TEST(test_enabling_dim_allows_dimming);

    return UNITY_END();
}
