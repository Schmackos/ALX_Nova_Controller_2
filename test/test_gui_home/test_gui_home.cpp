#include <cstdio>
#include <cstring>
#include <unity.h>

/**
 * GUI Home Screen Tests
 *
 * Tests the formatting and status logic used by the 6-cell home dashboard.
 * Pure functions mirroring scr_home.cpp logic — no LVGL dependencies.
 */

/* ===== Sensing mode enum (mirrors src/config.h) ===== */
enum SensingMode { ALWAYS_ON, ALWAYS_OFF, SMART_AUTO };

/* ===== Dot color enum for status indicators ===== */
enum DotColor { DOT_GREEN, DOT_RED, DOT_ORANGE, DOT_GRAY };

/* ===== Format functions (mirrors scr_home.cpp logic) ===== */

static void format_signal(float level_dBFS, float threshold_dBFS, char *buf, int len) {
    (void)threshold_dBFS;
    snprintf(buf, len, "%+.0f dBFS", level_dBFS);
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

/* Mode display with timer alternation */
static void format_mode_display(SensingMode mode, unsigned long timer_remaining,
                                unsigned long mock_millis, char *buf, int len) {
    if (mode != SMART_AUTO || timer_remaining == 0) {
        snprintf(buf, len, "%s", format_mode(mode));
    } else if ((mock_millis / 3000) % 2 == 0) {
        snprintf(buf, len, "Smart Auto");
    } else {
        unsigned long mins = timer_remaining / 60;
        unsigned long secs = timer_remaining % 60;
        snprintf(buf, len, "%02lu:%02lu", mins, secs);
    }
}

/* ===== Dot color helpers ===== */

static DotColor get_amp_dot(bool state) {
    return state ? DOT_GREEN : DOT_RED;
}

static DotColor get_signal_dot(float level, float threshold) {
    return (level >= threshold) ? DOT_GREEN : DOT_GRAY;
}

static DotColor get_wifi_dot(bool connected, bool apMode) {
    if (connected) return DOT_GREEN;
    if (apMode) return DOT_ORANGE;
    return DOT_RED;
}

static DotColor get_mqtt_dot(bool enabled, bool connected) {
    if (!enabled) return DOT_GRAY;
    if (connected) return DOT_GREEN;
    return DOT_RED;
}

/* ===== Tests ===== */

void setUp(void) {}
void tearDown(void) {}

/* Signal formatting (simplified — dot conveys detection) */
void test_home_signal_detected(void) {
    char buf[32];
    format_signal(-18.0f, -40.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-18 dBFS", buf);
}

void test_home_signal_not_detected(void) {
    char buf[32];
    format_signal(-55.0f, -40.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-55 dBFS", buf);
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

/* Mode display alternation tests */
void test_home_mode_shows_name_when_no_timer(void) {
    char buf[32];
    format_mode_display(SMART_AUTO, 0, 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Smart Auto", buf);
}

void test_home_mode_shows_name_when_not_smart_auto(void) {
    char buf[32];
    format_mode_display(ALWAYS_ON, 300, 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Always On", buf);
}

void test_home_mode_alternates_shows_mode_phase(void) {
    char buf[32];
    /* millis=0 -> 0/3000=0, 0%2=0 -> show mode name */
    format_mode_display(SMART_AUTO, 870, 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Smart Auto", buf);
}

void test_home_mode_alternates_shows_timer_phase(void) {
    char buf[32];
    /* millis=3000 -> 3000/3000=1, 1%2=1 -> show timer */
    format_mode_display(SMART_AUTO, 870, 3000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("14:30", buf);
}

void test_home_mode_timer_format(void) {
    char buf[32];
    /* 5 seconds */
    format_mode_display(SMART_AUTO, 5, 3000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("00:05", buf);
    /* 61 minutes 1 second */
    format_mode_display(SMART_AUTO, 3661, 3000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("61:01", buf);
}

/* Dot color tests */
void test_home_amp_dot_colors(void) {
    TEST_ASSERT_EQUAL(DOT_GREEN, get_amp_dot(true));
    TEST_ASSERT_EQUAL(DOT_RED, get_amp_dot(false));
}

void test_home_signal_dot_colors(void) {
    TEST_ASSERT_EQUAL(DOT_GREEN, get_signal_dot(-18.0f, -40.0f));
    TEST_ASSERT_EQUAL(DOT_GRAY, get_signal_dot(-55.0f, -40.0f));
}

void test_home_wifi_dot_colors(void) {
    TEST_ASSERT_EQUAL(DOT_GREEN, get_wifi_dot(true, false));
    TEST_ASSERT_EQUAL(DOT_ORANGE, get_wifi_dot(false, true));
    TEST_ASSERT_EQUAL(DOT_RED, get_wifi_dot(false, false));
}

void test_home_mqtt_dot_colors(void) {
    TEST_ASSERT_EQUAL(DOT_GREEN, get_mqtt_dot(true, true));
    TEST_ASSERT_EQUAL(DOT_RED, get_mqtt_dot(true, false));
    TEST_ASSERT_EQUAL(DOT_GRAY, get_mqtt_dot(false, false));
}

void test_home_level_bar_range(void) {
    /* VU value should be clamped to -96..0 range */
    float vu = -120.0f;
    int clamped = (int)vu;
    if (clamped < -96) clamped = -96;
    if (clamped > 0) clamped = 0;
    TEST_ASSERT_EQUAL(-96, clamped);

    vu = 5.0f;
    clamped = (int)vu;
    if (clamped < -96) clamped = -96;
    if (clamped > 0) clamped = 0;
    TEST_ASSERT_EQUAL(0, clamped);

    vu = -22.0f;
    clamped = (int)vu;
    if (clamped < -96) clamped = -96;
    if (clamped > 0) clamped = 0;
    TEST_ASSERT_EQUAL(-22, clamped);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    /* Kept tests (6) */
    RUN_TEST(test_home_signal_detected);
    RUN_TEST(test_home_signal_not_detected);
    RUN_TEST(test_home_wifi_connected);
    RUN_TEST(test_home_wifi_ap_mode);
    RUN_TEST(test_home_wifi_disconnected);
    RUN_TEST(test_home_mqtt_states);
    RUN_TEST(test_home_amplifier_on_off);
    RUN_TEST(test_home_mode_strings);

    /* New tests (10) */
    RUN_TEST(test_home_mode_shows_name_when_no_timer);
    RUN_TEST(test_home_mode_shows_name_when_not_smart_auto);
    RUN_TEST(test_home_mode_alternates_shows_mode_phase);
    RUN_TEST(test_home_mode_alternates_shows_timer_phase);
    RUN_TEST(test_home_mode_timer_format);
    RUN_TEST(test_home_amp_dot_colors);
    RUN_TEST(test_home_signal_dot_colors);
    RUN_TEST(test_home_wifi_dot_colors);
    RUN_TEST(test_home_mqtt_dot_colors);
    RUN_TEST(test_home_level_bar_range);

    return UNITY_END();
}
