#include <unity.h>
#include <cstring>
#include <cstdlib>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline HAL EEPROM v3 for native testing =====
#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_eeprom_v3.cpp"

// Helper: build a valid v1/v2 EEPROM header in a 128-byte buffer
static void build_v2_header(uint8_t* buf) {
    memset(buf, 0, 128);
    // Magic "ALXD"
    buf[0] = 'A'; buf[1] = 'L'; buf[2] = 'X'; buf[3] = 'D';
    // Version = 3 (for v3 extension)
    buf[4] = 3;
    // Device ID = 0x0001 (PCM5102A)
    buf[5] = 0x01; buf[6] = 0x00;
    // HW revision
    buf[7] = 1;
    // Device name at 0x08
    strncpy((char*)&buf[0x08], "TestDevice", 32);
    // Manufacturer at 0x28
    strncpy((char*)&buf[0x28], "TestMfg", 32);
    // Max channels at 0x48
    buf[0x48] = 2;
    // I2C address at 0x49
    buf[0x49] = 0x48;
    // Flags at 0x4A
    buf[0x4A] = 0x02; // HW volume
    // Num rates at 0x4B
    buf[0x4B] = 1;
    // Sample rate at 0x4C (48000 = 0xBB80)
    buf[0x4C] = 0x80; buf[0x4D] = 0xBB; buf[0x4E] = 0x00; buf[0x4F] = 0x00;
}

void setUp() {}
void tearDown() {}

// ===== Test 1: CRC-16/CCITT known values =====
void test_crc16_known_values() {
    // "123456789" → CRC should be 0x29B1
    const uint8_t data[] = "123456789";
    uint16_t crc = hal_crc16_ccitt(data, 9);
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc);
}

// ===== Test 2: CRC-16 empty data =====
void test_crc16_empty() {
    uint16_t crc = hal_crc16_ccitt(nullptr, 0);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc);
}

// ===== Test 3: Serialize and parse roundtrip =====
void test_v3_serialize_parse_roundtrip() {
    uint8_t buf[128];
    build_v2_header(buf);

    // Serialize v3 fields
    TEST_ASSERT_TRUE(hal_eeprom_serialize_v3(buf, 128, "ti,pcm5102a"));

    // Parse back
    char compatible[32];
    TEST_ASSERT_TRUE(hal_eeprom_parse_v3(buf, 128, compatible));
    TEST_ASSERT_EQUAL_STRING("ti,pcm5102a", compatible);
}

// ===== Test 4: Parse with invalid CRC =====
void test_v3_parse_invalid_crc() {
    uint8_t buf[128];
    build_v2_header(buf);
    hal_eeprom_serialize_v3(buf, 128, "evergrande,es8311");

    // Corrupt one byte in the CRC-protected area
    buf[0x60] ^= 0xFF;

    char compatible[32];
    TEST_ASSERT_FALSE(hal_eeprom_parse_v3(buf, 128, compatible));
    TEST_ASSERT_EQUAL_STRING("", compatible);
}

// ===== Test 5: Parse with buffer too short =====
void test_v3_parse_short_buffer() {
    uint8_t buf[64];
    memset(buf, 0, 64);

    char compatible[32];
    TEST_ASSERT_FALSE(hal_eeprom_parse_v3(buf, 64, compatible));
}

// ===== Test 6: Serialize with null compatible =====
void test_v3_serialize_null_compatible() {
    uint8_t buf[128];
    memset(buf, 0, 128);
    TEST_ASSERT_FALSE(hal_eeprom_serialize_v3(buf, 128, nullptr));
}

// ===== Test 7: Compatible string truncation =====
void test_v3_compatible_truncation() {
    uint8_t buf[128];
    build_v2_header(buf);

    // Very long compatible string — should be truncated to 31 chars
    const char* longStr = "very-long-vendor-name,very-long-model-name-that-exceeds-the-limit";
    TEST_ASSERT_TRUE(hal_eeprom_serialize_v3(buf, 128, longStr));

    char compatible[32];
    TEST_ASSERT_TRUE(hal_eeprom_parse_v3(buf, 128, compatible));
    TEST_ASSERT_EQUAL(31, strlen(compatible));
}

// ===== Test 8: Multiple roundtrips with different strings =====
void test_v3_multiple_roundtrips() {
    const char* strings[] = {"ti,pcm5102a", "evergrande,es8311", "ess,es9038q2m", "ti,pcm1808"};
    uint8_t buf[128];
    char result[32];

    for (int i = 0; i < 4; i++) {
        build_v2_header(buf);
        TEST_ASSERT_TRUE(hal_eeprom_serialize_v3(buf, 128, strings[i]));
        TEST_ASSERT_TRUE(hal_eeprom_parse_v3(buf, 128, result));
        TEST_ASSERT_EQUAL_STRING(strings[i], result);
    }
}

// ===== Test Runner =====
int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_crc16_known_values);
    RUN_TEST(test_crc16_empty);
    RUN_TEST(test_v3_serialize_parse_roundtrip);
    RUN_TEST(test_v3_parse_invalid_crc);
    RUN_TEST(test_v3_parse_short_buffer);
    RUN_TEST(test_v3_serialize_null_compatible);
    RUN_TEST(test_v3_compatible_truncation);
    RUN_TEST(test_v3_multiple_roundtrips);

    return UNITY_END();
}
