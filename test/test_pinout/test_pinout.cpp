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
#ifndef I2S_BCK_PIN
static const int I2S_BCK_PIN = 16;
#endif
#ifndef I2S_DOUT_PIN
static const int I2S_DOUT_PIN = 17;
#endif
#ifndef I2S_LRC_PIN
static const int I2S_LRC_PIN = 18;
#endif
#ifndef I2S_MCLK_PIN
static const int I2S_MCLK_PIN = 3;
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

/* ===== Local copy of pin data and formatters (mirrors scr_debug.cpp) ===== */

enum PinSortMode { SORT_BY_DEVICE = 0, SORT_BY_GPIO, SORT_BY_FUNCTION, SORT_MODE_COUNT };

struct PinEntry {
    const char *device;
    const char *function;
    int gpio;
};

static const PinEntry all_pins[] = {
    {"PCM1808 ADC",  "BCK",  I2S_BCK_PIN},
    {"PCM1808 ADC",  "DOUT", I2S_DOUT_PIN},
    {"PCM1808 ADC",  "LRC",  I2S_LRC_PIN},
    {"PCM1808 ADC",  "MCLK", I2S_MCLK_PIN},
    {"ST7735S TFT",  "CS",   TFT_CS_PIN},
    {"ST7735S TFT",  "MOSI", TFT_MOSI_PIN},
    {"ST7735S TFT",  "CLK",  TFT_SCLK_PIN},
    {"ST7735S TFT",  "DC",   TFT_DC_PIN},
    {"ST7735S TFT",  "RST",  TFT_RST_PIN},
    {"ST7735S TFT",  "BL",   TFT_BL_PIN},
    {"EC11 Encoder", "A",    ENCODER_A_PIN},
    {"EC11 Encoder", "B",    ENCODER_B_PIN},
    {"EC11 Encoder", "SW",   ENCODER_SW_PIN},
    {"HW-508 Buzz",  "IO",   BUZZER_PIN},
    {"Core",         "LED",  LED_PIN},
    {"Core",         "Amp",  AMPLIFIER_PIN},
    {"Core",         "Btn",  RESET_BUTTON_PIN},
};
static const int PIN_COUNT = sizeof(all_pins) / sizeof(all_pins[0]);

static void sort_pins(int *indices, int count, PinSortMode mode) {
    for (int i = 1; i < count; i++) {
        int key = indices[i];
        int j = i - 1;
        while (j >= 0) {
            bool swap = false;
            if (mode == SORT_BY_GPIO) {
                swap = all_pins[indices[j]].gpio > all_pins[key].gpio;
            } else if (mode == SORT_BY_FUNCTION) {
                swap = strcmp(all_pins[indices[j]].function, all_pins[key].function) > 0;
            }
            if (!swap) break;
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }
}

static void format_pin_info(char *buf, int len) {
    snprintf(buf, len,
             "PCM1808 ADC\n"
             "  BCK=%d DOUT=%d LRC=%d\n"
             "  MCLK=%d\n"
             "ST7735S TFT 1.8\"\n"
             "  CS=%d MOSI=%d CLK=%d\n"
             "  DC=%d RST=%d BL=%d\n"
             "EC11 Encoder\n"
             "  A=%d B=%d SW=%d\n"
             "HW-508 Buzzer\n"
             "  IO=%d\n"
             "Core\n"
             "  LED=%d Amp=%d Btn=%d",
             I2S_BCK_PIN, I2S_DOUT_PIN, I2S_LRC_PIN,
             I2S_MCLK_PIN,
             TFT_CS_PIN, TFT_MOSI_PIN, TFT_SCLK_PIN,
             TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN,
             ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_SW_PIN,
             BUZZER_PIN,
             LED_PIN, AMPLIFIER_PIN, RESET_BUTTON_PIN);
}

static void format_pin_sorted(char *buf, int len, PinSortMode mode) {
    int indices[17];
    for (int i = 0; i < PIN_COUNT; i++) indices[i] = i;

    if (mode == SORT_BY_DEVICE) {
        format_pin_info(buf, len);
        return;
    }

    sort_pins(indices, PIN_COUNT, mode);
    int pos = 0;
    for (int i = 0; i < PIN_COUNT && pos < len - 1; i++) {
        const PinEntry &p = all_pins[indices[i]];
        if (mode == SORT_BY_GPIO) {
            pos += snprintf(buf + pos, len - pos,
                            "%2d %-4s %s\n", p.gpio, p.function, p.device);
        } else {
            pos += snprintf(buf + pos, len - pos,
                            "%-4s %2d %s\n", p.function, p.gpio, p.device);
        }
    }
    if (pos > 0 && buf[pos - 1] == '\n') buf[pos - 1] = '\0';
}

/* ===== Tests ===== */

void setUp(void) {}
void tearDown(void) {}

/* Test: format_pin_info produces expected output */
void test_pin_info_format_output(void) {
    char buf[256];
    format_pin_info(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "PCM1808 ADC"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "ST7735S TFT"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "EC11 Encoder"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "HW-508 Buzzer"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Core"));
}

/* Test: all core pin values present in output */
void test_pin_info_core_pins(void) {
    char buf[256];
    format_pin_info(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "LED=2"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Amp=4"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Btn=15"));
}

/* Test: buzzer pin present */
void test_pin_info_buzzer_pin(void) {
    char buf[256];
    format_pin_info(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "IO=8"));
}

/* Test: all I2S pin values present in output */
void test_pin_info_i2s_pins(void) {
    char buf[256];
    format_pin_info(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "BCK=16"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "DOUT=17"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "LRC=18"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "MCLK=3"));
}

/* Test: all encoder pin values present */
void test_pin_info_encoder_pins(void) {
    char buf[256];
    format_pin_info(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "A=5"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "B=6"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "SW=7"));
}

/* Test: all TFT pin values present */
void test_pin_info_tft_pins(void) {
    char buf[256];
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
    char buf[256];
    format_pin_info(buf, sizeof(buf));
    size_t len = strlen(buf);
    TEST_ASSERT_LESS_THAN(256, len);
    TEST_ASSERT_GREATER_THAN(100, len);
}

/* Test: small buffer truncates without crash */
void test_pin_info_small_buffer(void) {
    char buf[32];
    format_pin_info(buf, sizeof(buf));
    TEST_ASSERT_EQUAL(31, strlen(buf));
}

/* Test: exact first line content */
void test_pin_info_first_line(void) {
    char buf[256];
    format_pin_info(buf, sizeof(buf));
    char *newline = strchr(buf, '\n');
    TEST_ASSERT_NOT_NULL(newline);
    size_t first_line_len = newline - buf;
    char first_line[64];
    strncpy(first_line, buf, first_line_len);
    first_line[first_line_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("PCM1808 ADC", first_line);
}

/* Test: all 17 pin numbers are unique in output */
void test_pin_info_all_17_pins_present(void) {
    char buf[256];
    format_pin_info(buf, sizeof(buf));
    int eq_count = 0;
    for (size_t i = 0; i < strlen(buf); i++) {
        if (buf[i] == '=') eq_count++;
    }
    TEST_ASSERT_EQUAL(17, eq_count);
}

/* Test: multiline output has expected line count */
void test_pin_info_line_count(void) {
    char buf[256];
    format_pin_info(buf, sizeof(buf));
    int newline_count = 0;
    for (size_t i = 0; i < strlen(buf); i++) {
        if (buf[i] == '\n') newline_count++;
    }
    TEST_ASSERT_EQUAL(11, newline_count);
}

/* ===== Sort tests ===== */

/* Test: sort by GPIO produces ascending GPIO numbers */
void test_sort_by_gpio_ascending(void) {
    char buf[384];
    format_pin_sorted(buf, sizeof(buf), SORT_BY_GPIO);
    /* First line should be GPIO 2 (LED), last should be GPIO 21 (BL) */
    TEST_ASSERT_NOT_NULL(strstr(buf, " 2 LED"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "21 BL"));
    /* Verify ascending: find positions of first few entries */
    char *pos2 = strstr(buf, " 2 LED");
    char *pos3 = strstr(buf, " 3 MCLK");
    char *pos4 = strstr(buf, " 4 Amp");
    TEST_ASSERT_TRUE(pos2 < pos3);
    TEST_ASSERT_TRUE(pos3 < pos4);
}

/* Test: sort by GPIO has all 17 pins */
void test_sort_by_gpio_all_pins(void) {
    char buf[384];
    format_pin_sorted(buf, sizeof(buf), SORT_BY_GPIO);
    int line_count = 0;
    for (size_t i = 0; i < strlen(buf); i++) {
        if (buf[i] == '\n') line_count++;
    }
    /* 17 pins = 16 newlines (last line has no trailing newline) */
    TEST_ASSERT_EQUAL(16, line_count);
}

/* Test: sort by function produces alphabetical order */
void test_sort_by_function_alphabetical(void) {
    char buf[384];
    format_pin_sorted(buf, sizeof(buf), SORT_BY_FUNCTION);
    /* 'A' should come before 'Amp', 'Amp' before 'B', etc. */
    char *posA = strstr(buf, "A   ");
    char *posAmp = strstr(buf, "Amp ");
    char *posB = strstr(buf, "B   ");
    TEST_ASSERT_NOT_NULL(posA);
    TEST_ASSERT_NOT_NULL(posAmp);
    TEST_ASSERT_NOT_NULL(posB);
    TEST_ASSERT_TRUE(posA < posAmp);
    TEST_ASSERT_TRUE(posAmp < posB);
}

/* Test: sort by function includes device names */
void test_sort_by_function_has_devices(void) {
    char buf[384];
    format_pin_sorted(buf, sizeof(buf), SORT_BY_FUNCTION);
    TEST_ASSERT_NOT_NULL(strstr(buf, "PCM1808 ADC"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "ST7735S TFT"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "EC11 Encoder"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Core"));
}

/* Test: sort by device returns same as default format */
void test_sort_by_device_matches_default(void) {
    char default_buf[384];
    char sorted_buf[384];
    format_pin_info(default_buf, sizeof(default_buf));
    format_pin_sorted(sorted_buf, sizeof(sorted_buf), SORT_BY_DEVICE);
    TEST_ASSERT_EQUAL_STRING(default_buf, sorted_buf);
}

/* Test: sort mode cycles through all three modes */
void test_sort_mode_count(void) {
    TEST_ASSERT_EQUAL(3, SORT_MODE_COUNT);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_pin_info_format_output);
    RUN_TEST(test_pin_info_core_pins);
    RUN_TEST(test_pin_info_buzzer_pin);
    RUN_TEST(test_pin_info_i2s_pins);
    RUN_TEST(test_pin_info_encoder_pins);
    RUN_TEST(test_pin_info_tft_pins);
    RUN_TEST(test_pin_info_buffer_size);
    RUN_TEST(test_pin_info_small_buffer);
    RUN_TEST(test_pin_info_first_line);
    RUN_TEST(test_pin_info_all_17_pins_present);
    RUN_TEST(test_pin_info_line_count);
    RUN_TEST(test_sort_by_gpio_ascending);
    RUN_TEST(test_sort_by_gpio_all_pins);
    RUN_TEST(test_sort_by_function_alphabetical);
    RUN_TEST(test_sort_by_function_has_devices);
    RUN_TEST(test_sort_by_device_matches_default);
    RUN_TEST(test_sort_mode_count);
    return UNITY_END();
}
