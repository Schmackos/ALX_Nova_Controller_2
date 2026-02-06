#include <cstdio>
#include <cstring>
#include <unity.h>

/**
 * GUI Home Screen Tests
 *
 * Tests the formatting logic used by scr_home without LVGL dependencies.
 * Mirrors the format functions from scr_home.cpp for unit testing.
 */

/* ===== Sensing mode enum (mirrors src/config.h) ===== */
enum SensingMode { ALWAYS_ON, ALWAYS_OFF, SMART_AUTO };

/* ===== Format functions (mirrors scr_home.cpp logic) ===== */

static void format_uptime(unsigned long ms, char *buf, int len) {
    unsigned long secs = ms / 1000;
    unsigned long mins = secs / 60;
    unsigned long hours = mins / 60;
    unsigned long days = hours / 24;
    if (days > 0) {
        snprintf(buf, len, "%lud %luh", days, hours % 24);
    } else if (hours > 0) {
        snprintf(buf, len, "%luh %lum", hours, mins % 60);
    } else if (mins > 0) {
        snprintf(buf, len, "%lum %lus", mins, secs % 60);
    } else {
        snprintf(buf, len, "%lus", secs);
    }
}

static void format_timer(unsigned long remaining_secs, char *buf, int len) {
    if (remaining_secs == 0) {
        snprintf(buf, len, "Idle");
    } else {
        unsigned long mins = remaining_secs / 60;
        unsigned long secs = remaining_secs % 60;
        snprintf(buf, len, "%02lu:%02lu", mins, secs);
    }
}

static void format_signal(float level_dBFS, float threshold_dBFS, char *buf, int len) {
    bool detected = level_dBFS >= threshold_dBFS;
    snprintf(buf, len, "%+.0f dBFS %s", level_dBFS, detected ? "Detected" : "\xE2\x80\x94");
}

static const char *format_wifi(bool connected, bool apMode) {
    if (connected) return "Connected";
    if (apMode) return "AP Mode";
    return "Disconnected";
}

static const char *format_mqtt(bool enabled, bool connected) {
    if (!enabled) return "Disabled";
    if (connected) return "Connected";
    return "Disconnected";
}

static const char *format_amplifier(bool state) {
    return state ? "ON" : "OFF";
}

static const char *format_mode(SensingMode mode) {
    if (mode == ALWAYS_ON) return "Always On";
    if (mode == ALWAYS_OFF) return "Always Off";
    return "Smart Auto";
}

/* ===== Tests ===== */

void setUp(void) {}
void tearDown(void) {}

/* Uptime formatting */
void test_home_uptime_format_seconds(void) {
    char buf[32];
    format_uptime(45000, buf, sizeof(buf)); /* 45 seconds */
    TEST_ASSERT_EQUAL_STRING("45s", buf);
}

void test_home_uptime_format_minutes(void) {
    char buf[32];
    format_uptime(185000, buf, sizeof(buf)); /* 3m 5s */
    TEST_ASSERT_EQUAL_STRING("3m 5s", buf);
}

void test_home_uptime_format_hours(void) {
    char buf[32];
    format_uptime(8100000, buf, sizeof(buf)); /* 2h 15m */
    TEST_ASSERT_EQUAL_STRING("2h 15m", buf);
}

void test_home_uptime_format_days(void) {
    char buf[32];
    format_uptime(90000000, buf, sizeof(buf)); /* 1d 1h */
    TEST_ASSERT_EQUAL_STRING("1d 1h", buf);
}

/* Timer formatting */
void test_home_timer_format_active(void) {
    char buf[32];
    format_timer(870, buf, sizeof(buf)); /* 14:30 */
    TEST_ASSERT_EQUAL_STRING("14:30", buf);
}

void test_home_timer_format_idle(void) {
    char buf[32];
    format_timer(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Idle", buf);
}

void test_home_timer_format_non_auto_mode(void) {
    /* When mode != SMART_AUTO, timer shows Idle regardless */
    /* This test validates the logic pattern used in scr_home_refresh:
       if (mode != SMART_AUTO || timerRemaining == 0) -> "Idle" */
    SensingMode mode = ALWAYS_ON;
    unsigned long remaining = 300;
    char buf[32];

    if (mode != SMART_AUTO || remaining == 0) {
        snprintf(buf, sizeof(buf), "Idle");
    } else {
        format_timer(remaining, buf, sizeof(buf));
    }
    TEST_ASSERT_EQUAL_STRING("Idle", buf);
}

/* Signal formatting */
void test_home_signal_detected(void) {
    char buf[32];
    format_signal(-18.0f, -40.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-18 dBFS Detected", buf);
}

void test_home_signal_not_detected(void) {
    char buf[32];
    format_signal(-55.0f, -40.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-55 dBFS \xE2\x80\x94", buf);
}

/* WiFi status */
void test_home_wifi_connected(void) {
    TEST_ASSERT_EQUAL_STRING("Connected", format_wifi(true, false));
}

void test_home_wifi_ap_mode(void) {
    TEST_ASSERT_EQUAL_STRING("AP Mode", format_wifi(false, true));
}

void test_home_wifi_disconnected(void) {
    TEST_ASSERT_EQUAL_STRING("Disconnected", format_wifi(false, false));
}

/* MQTT states */
void test_home_mqtt_states(void) {
    TEST_ASSERT_EQUAL_STRING("Connected", format_mqtt(true, true));
    TEST_ASSERT_EQUAL_STRING("Disconnected", format_mqtt(true, false));
    TEST_ASSERT_EQUAL_STRING("Disabled", format_mqtt(false, false));
}

/* Amplifier on/off */
void test_home_amplifier_on_off(void) {
    TEST_ASSERT_EQUAL_STRING("ON", format_amplifier(true));
    TEST_ASSERT_EQUAL_STRING("OFF", format_amplifier(false));
}

/* Mode strings */
void test_home_mode_strings(void) {
    TEST_ASSERT_EQUAL_STRING("Always On", format_mode(ALWAYS_ON));
    TEST_ASSERT_EQUAL_STRING("Always Off", format_mode(ALWAYS_OFF));
    TEST_ASSERT_EQUAL_STRING("Smart Auto", format_mode(SMART_AUTO));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_home_uptime_format_seconds);
    RUN_TEST(test_home_uptime_format_minutes);
    RUN_TEST(test_home_uptime_format_hours);
    RUN_TEST(test_home_uptime_format_days);
    RUN_TEST(test_home_timer_format_active);
    RUN_TEST(test_home_timer_format_idle);
    RUN_TEST(test_home_timer_format_non_auto_mode);
    RUN_TEST(test_home_signal_detected);
    RUN_TEST(test_home_signal_not_detected);
    RUN_TEST(test_home_wifi_connected);
    RUN_TEST(test_home_wifi_ap_mode);
    RUN_TEST(test_home_wifi_disconnected);
    RUN_TEST(test_home_mqtt_states);
    RUN_TEST(test_home_amplifier_on_off);
    RUN_TEST(test_home_mode_strings);

    return UNITY_END();
}
