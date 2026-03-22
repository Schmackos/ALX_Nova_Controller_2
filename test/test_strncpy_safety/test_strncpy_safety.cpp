#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Pull in hal_safe_strcpy and hal_init_descriptor via the types header
#include "../../src/hal/hal_types.h"

void setUp(void) {}
void tearDown(void) {}

// ===== hal_safe_strcpy tests =====

void test_hal_safe_strcpy_normal(void) {
    char buf[32] = {0};
    hal_safe_strcpy(buf, sizeof(buf), "hello");
    TEST_ASSERT_EQUAL_STRING("hello", buf);
}

void test_hal_safe_strcpy_boundary(void) {
    // src is exactly 31 chars — fits in 32-byte buf with null at [31]
    const char* src = "1234567890123456789012345678901"; // 31 chars
    char buf[32] = {0xFF};
    hal_safe_strcpy(buf, sizeof(buf), src);
    TEST_ASSERT_EQUAL_STRING(src, buf);
    TEST_ASSERT_EQUAL_CHAR('\0', buf[31]);
}

void test_hal_safe_strcpy_truncation(void) {
    // src is 40 chars — must be truncated, buf[31] must be '\0'
    const char* src = "12345678901234567890123456789012345678901"; // 40 chars
    char buf[32];
    memset(buf, 0xFF, sizeof(buf));
    hal_safe_strcpy(buf, sizeof(buf), src);
    // First 31 chars copied, then null terminator
    TEST_ASSERT_EQUAL_INT(31, (int)strlen(buf));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[31]);
}

void test_hal_safe_strcpy_empty(void) {
    char buf[32];
    memset(buf, 0xAA, sizeof(buf));
    hal_safe_strcpy(buf, sizeof(buf), "");
    TEST_ASSERT_EQUAL_STRING("", buf);
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
}

void test_hal_safe_strcpy_size_one(void) {
    char buf[1] = {0xFF};
    hal_safe_strcpy(buf, 1, "hello");
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
}

// ===== hal_init_descriptor tests =====

void test_hal_init_descriptor_null_termination(void) {
    HalDeviceDescriptor d;
    // compatible field is char[32], so max 31 usable chars
    const char* compat31 = "1234567890123456789012345678901"; // 31 chars
    hal_init_descriptor(d, compat31, "TestName", "TestMfr",
                        HAL_DEV_DAC, 2, 0x48,
                        HAL_BUS_I2C, 2,
                        HAL_RATE_48K, HAL_CAP_DAC_PATH);
    TEST_ASSERT_EQUAL_STRING(compat31, d.compatible);
    TEST_ASSERT_EQUAL_CHAR('\0', d.compatible[31]);
    TEST_ASSERT_EQUAL_CHAR('\0', d.compatible[sizeof(d.compatible) - 1]);
}

void test_hal_init_descriptor_truncation(void) {
    HalDeviceDescriptor d;
    // 40-char strings — all must be truncated with null termination
    const char* longCompat = "ess,very-long-compatible-string-overflow"; // 40 chars
    const char* longName   = "ESS Very Long Device Name String Overflow"; // 41 chars
    const char* longMfr    = "ESS Technology Incorporated Very Long MFR"; // 41 chars
    hal_init_descriptor(d, longCompat, longName, longMfr,
                        HAL_DEV_DAC, 2, 0x48,
                        HAL_BUS_I2C, 2,
                        HAL_RATE_48K, HAL_CAP_DAC_PATH);
    // null terminator must be present at last byte of each field
    TEST_ASSERT_EQUAL_CHAR('\0', d.compatible[sizeof(d.compatible) - 1]);
    TEST_ASSERT_EQUAL_CHAR('\0', d.name[sizeof(d.name) - 1]);
    TEST_ASSERT_EQUAL_CHAR('\0', d.manufacturer[sizeof(d.manufacturer) - 1]);
    // lengths must not exceed capacity
    TEST_ASSERT_TRUE(strlen(d.compatible) < sizeof(d.compatible));
    TEST_ASSERT_TRUE(strlen(d.name) < sizeof(d.name));
    TEST_ASSERT_TRUE(strlen(d.manufacturer) < sizeof(d.manufacturer));
}

void test_hal_init_descriptor_empty_strings(void) {
    HalDeviceDescriptor d;
    hal_init_descriptor(d, "", "", "",
                        HAL_DEV_DAC, 2, 0,
                        HAL_BUS_I2C, 2,
                        0, 0);
    TEST_ASSERT_EQUAL_STRING("", d.compatible);
    TEST_ASSERT_EQUAL_STRING("", d.name);
    TEST_ASSERT_EQUAL_STRING("", d.manufacturer);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_hal_safe_strcpy_normal);
    RUN_TEST(test_hal_safe_strcpy_boundary);
    RUN_TEST(test_hal_safe_strcpy_truncation);
    RUN_TEST(test_hal_safe_strcpy_empty);
    RUN_TEST(test_hal_safe_strcpy_size_one);
    RUN_TEST(test_hal_init_descriptor_null_termination);
    RUN_TEST(test_hal_init_descriptor_truncation);
    RUN_TEST(test_hal_init_descriptor_empty_strings);
    return UNITY_END();
}
