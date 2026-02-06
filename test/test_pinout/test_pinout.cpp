#include <cstdio>
#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

/**
 * Pinout String Formatting Tests
 *
 * Tests the pin configuration string generation logic
 * used in the debug screen. Mirrors the snprintf call
 * in scr_debug.cpp to verify output format and pin values.
 */

/* Pin constants matching config.h defaults */
#ifndef LED_PIN
static const int LED_PIN = 2;
#endif
#ifndef AMPLIFIER_PIN
static const int AMPLIFIER_PIN = 4;
#endif
#ifndef VOLTAGE_SENSE_PIN
static const int VOLTAGE_SENSE_PIN = 1;
#endif
#ifndef RESET_BUTTON_PIN
static const int RESET_BUTTON_PIN = 15;
#endif
#ifndef BUZZER_PIN
static const int BUZZER_PIN = 8;
#endif

/* GUI pin defines (normally under #ifdef GUI_ENABLED) */
#ifndef TFT_CS_PIN
#define TFT_CS_PIN 10
#endif
#ifndef TFT_MOSI_PIN
#define TFT_MOSI_PIN 11
#endif
#ifndef TFT_SCLK_PIN
#define TFT_SCLK_PIN 12
#endif
#ifndef TFT_DC_PIN
#define TFT_DC_PIN 13
#endif
#ifndef TFT_RST_PIN
#define TFT_RST_PIN 14
#endif
#ifndef TFT_BL_PIN
#define TFT_BL_PIN 21
#endif
#ifndef ENCODER_A_PIN
#define ENCODER_A_PIN 5
#endif
#ifndef ENCODER_B_PIN
#define ENCODER_B_PIN 6
#endif
#ifndef ENCODER_SW_PIN
#define ENCODER_SW_PIN 7
#endif

/* ===== Local copy of pin string formatter (mirrors scr_debug.cpp) ===== */

static void format_pin_info(char *buf, int len) {
    snprintf(buf, len,
             "Core: LED=%d Amp=%d ADC=%d\n"
             "  Btn=%d Buzz=%d\n"
             "Enc: A=%d B=%d SW=%d\n"
             "TFT: CS=%d MOSI=%d CLK=%d\n"
             "  DC=%d RST=%d BL=%d",
             LED_PIN, AMPLIFIER_PIN, VOLTAGE_SENSE_PIN,
             RESET_BUTTON_PIN, BUZZER_PIN,
             ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_SW_PIN,
             TFT_CS_PIN, TFT_MOSI_PIN, TFT_SCLK_PIN,
             TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN);
}

/* ===== Tests ===== */

void setUp(void) {}
void tearDown(void) {}

/* Test: format_pin_info produces expected output */
void test_pin_info_format_output(void) {
    char buf[128];
    format_pin_info(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Core:"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Enc:"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "TFT:"));
}

/* Test: all core pin values present in output */
void test_pin_info_core_pins(void) {
    char buf[128];
    format_pin_info(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "LED=2"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Amp=4"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "ADC=1"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Btn=15"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Buzz=8"));
}

/* Test: all encoder pin values present */
void test_pin_info_encoder_pins(void) {
    char buf[128];
    format_pin_info(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "A=5"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "B=6"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "SW=7"));
}

/* Test: all TFT pin values present */
void test_pin_info_tft_pins(void) {
    char buf[128];
    format_pin_info(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "CS=10"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "MOSI=11"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "CLK=12"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "DC=13"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "RST=14"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "BL=21"));
}

/* Test: output fits in expected buffer size */
void test_pin_info_buffer_size(void) {
    char buf[128];
    format_pin_info(buf, sizeof(buf));
    size_t len = strlen(buf);
    TEST_ASSERT_LESS_THAN(128, len);
    TEST_ASSERT_GREATER_THAN(50, len);
}

/* Test: small buffer truncates without crash */
void test_pin_info_small_buffer(void) {
    char buf[32];
    format_pin_info(buf, sizeof(buf));
    TEST_ASSERT_EQUAL(31, strlen(buf));
}

/* Test: exact first line content */
void test_pin_info_first_line(void) {
    char buf[128];
    format_pin_info(buf, sizeof(buf));
    char *newline = strchr(buf, '\n');
    TEST_ASSERT_NOT_NULL(newline);
    size_t first_line_len = newline - buf;
    char first_line[64];
    strncpy(first_line, buf, first_line_len);
    first_line[first_line_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("Core: LED=2 Amp=4 ADC=1", first_line);
}

/* Test: all 14 pin numbers are unique in output */
void test_pin_info_all_14_pins_present(void) {
    char buf[128];
    format_pin_info(buf, sizeof(buf));
    int eq_count = 0;
    for (size_t i = 0; i < strlen(buf); i++) {
        if (buf[i] == '=') eq_count++;
    }
    TEST_ASSERT_EQUAL(14, eq_count);
}

/* Test: multiline output has expected line count */
void test_pin_info_line_count(void) {
    char buf[128];
    format_pin_info(buf, sizeof(buf));
    int newline_count = 0;
    for (size_t i = 0; i < strlen(buf); i++) {
        if (buf[i] == '\n') newline_count++;
    }
    TEST_ASSERT_EQUAL(4, newline_count);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_pin_info_format_output);
    RUN_TEST(test_pin_info_core_pins);
    RUN_TEST(test_pin_info_encoder_pins);
    RUN_TEST(test_pin_info_tft_pins);
    RUN_TEST(test_pin_info_buffer_size);
    RUN_TEST(test_pin_info_small_buffer);
    RUN_TEST(test_pin_info_first_line);
    RUN_TEST(test_pin_info_all_14_pins_present);
    RUN_TEST(test_pin_info_line_count);
    return UNITY_END();
}
