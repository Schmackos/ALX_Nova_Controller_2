/**
 * test_evt_any.cpp
 *
 * Unit tests for the event bit definitions in app_events.h.
 * Validates EVT_ANY covers all defined bits, each bit is a distinct
 * power of two, and specific bit positions are correct.
 *
 * Pure header-only constant-value tests -- no mocks or stubs needed.
 * Under UNIT_TEST the function signatures become no-ops but all
 * #define constants remain available.
 */

#include <unity.h>
#include <stdint.h>

#include "../../src/app_events.h"

// ===== Helper: collect all defined EVT_* bit constants into an array =====
// 15 individual event bits are defined (bits 0-11, 13-15; bit 12 is spare).
static const unsigned long ALL_EVT_BITS[] = {
    EVT_OTA,
    EVT_DISPLAY,
    EVT_BUZZER,
    EVT_SIGGEN,
    EVT_DSP_CONFIG,
    EVT_DAC,
    EVT_EEPROM,
    EVT_USB_AUDIO,
    EVT_USB_VU,
    EVT_SETTINGS,
    EVT_ADC_ENABLED,
    EVT_ETHERNET,
    EVT_DAC_SETTINGS,
    EVT_HAL_DEVICE,
    EVT_CHANNEL_MAP,
};
static const int NUM_EVT_BITS = (int)(sizeof(ALL_EVT_BITS) / sizeof(ALL_EVT_BITS[0]));

// ===== setUp / tearDown =====

void setUp() {}
void tearDown() {}

// ===== Test 1: EVT_ANY is exactly the lower 24 bits =====

void test_EVT_ANY_is_24_bits() {
    TEST_ASSERT_EQUAL_HEX32(0x00FFFFFFUL, EVT_ANY);
}

// ===== Test 2: OR of all defined bits is a subset of EVT_ANY =====

void test_EVT_ANY_covers_all_defined_bits() {
    unsigned long all_bits = 0;
    for (int i = 0; i < NUM_EVT_BITS; i++) {
        all_bits |= ALL_EVT_BITS[i];
    }
    TEST_ASSERT_EQUAL_HEX32(all_bits, all_bits & EVT_ANY);
}

// ===== Test 3: EVT_CHANNEL_MAP (bit 15) is within EVT_ANY =====

void test_EVT_CHANNEL_MAP_bit15_within_EVT_ANY() {
    TEST_ASSERT_NOT_EQUAL(0, EVT_CHANNEL_MAP & EVT_ANY);
}

// ===== Test 4: EVT_ANY does not include bit 24 or above =====

void test_EVT_ANY_does_not_include_bit24_plus() {
    TEST_ASSERT_EQUAL_HEX32(0, EVT_ANY & (1UL << 24));
    TEST_ASSERT_EQUAL_HEX32(0, EVT_ANY & (1UL << 25));
    TEST_ASSERT_EQUAL_HEX32(0, EVT_ANY & (1UL << 31));
}

// ===== Test 5: EVT_ANY has exactly 24 set bits =====

void test_EVT_ANY_all_lower_24_bits_set() {
    unsigned long v = EVT_ANY;
    int count = 0;
    while (v) {
        count += (int)(v & 1);
        v >>= 1;
    }
    TEST_ASSERT_EQUAL_INT(24, count);
}

// ===== Test 6: every individual EVT_* constant is a power of two =====

void test_all_event_bits_are_powers_of_two() {
    for (int i = 0; i < NUM_EVT_BITS; i++) {
        unsigned long bit = ALL_EVT_BITS[i];
        // A power of two satisfies: bit != 0 && (bit & (bit - 1)) == 0
        TEST_ASSERT_NOT_EQUAL(0UL, bit);
        TEST_ASSERT_EQUAL_HEX32(0UL, bit & (bit - 1));
    }
}

// ===== Test 7: no event bit collisions — all bits are distinct =====

void test_no_event_bit_collisions() {
    unsigned long all_bits = 0;
    int pop_count = 0;
    for (int i = 0; i < NUM_EVT_BITS; i++) {
        all_bits |= ALL_EVT_BITS[i];
    }
    // Count set bits in the OR result
    unsigned long v = all_bits;
    while (v) {
        pop_count += (int)(v & 1);
        v >>= 1;
    }
    // The number of set bits must equal the number of distinct EVT_* defines
    TEST_ASSERT_EQUAL_INT(NUM_EVT_BITS, pop_count);
}

// ===== Test 8: EVT_OTA is bit 0 =====

void test_EVT_OTA_is_bit0() {
    TEST_ASSERT_EQUAL_HEX32(1UL << 0, EVT_OTA);
}

// ===== Test 9: EVT_HAL_DEVICE is bit 14 =====

void test_EVT_HAL_DEVICE_is_bit14() {
    TEST_ASSERT_EQUAL_HEX32(1UL << 14, EVT_HAL_DEVICE);
}

// ===== Test 10: EVT_CHANNEL_MAP is bit 15 =====

void test_EVT_CHANNEL_MAP_is_bit15() {
    TEST_ASSERT_EQUAL_HEX32(1UL << 15, EVT_CHANNEL_MAP);
}

// ===== Main =====

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_EVT_ANY_is_24_bits);
    RUN_TEST(test_EVT_ANY_covers_all_defined_bits);
    RUN_TEST(test_EVT_CHANNEL_MAP_bit15_within_EVT_ANY);
    RUN_TEST(test_EVT_ANY_does_not_include_bit24_plus);
    RUN_TEST(test_EVT_ANY_all_lower_24_bits_set);
    RUN_TEST(test_all_event_bits_are_powers_of_two);
    RUN_TEST(test_no_event_bit_collisions);
    RUN_TEST(test_EVT_OTA_is_bit0);
    RUN_TEST(test_EVT_HAL_DEVICE_is_bit14);
    RUN_TEST(test_EVT_CHANNEL_MAP_is_bit15);
    return UNITY_END();
}
