// test_hal_wire_mock.cpp
// Tests the Wire.h mock infrastructure used by HAL I2C device tests.
// Validates register-map simulation, bus-index detection, probe ACK/NACK,
// multi-address isolation, and mock reset behaviour.

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

void setUp(void) {
    WireMock::reset();
}

void tearDown(void) {}

// ----- Test 1: begin() maps SDA/SCL to the correct bus index -----
void test_begin_initializes_bus_by_pin(void) {
    Wire.begin(7, 8, 400000);
    TEST_ASSERT_TRUE(WireMock::isBusInitialized(1));   // GPIO7/8 -> ONBOARD bus 1
    TEST_ASSERT_FALSE(WireMock::isBusInitialized(0));
    TEST_ASSERT_FALSE(WireMock::isBusInitialized(2));
}

// ----- Test 2: probe returns ACK (0) for a registered address -----
void test_probe_ack_when_device_registered(void) {
    WireMock::registerDevice(0x18, 1);
    Wire.beginTransmission(0x18);
    TEST_ASSERT_EQUAL(0, Wire.endTransmission());
}

// ----- Test 3: probe returns NACK (2) for an unregistered address -----
void test_probe_nack_when_device_absent(void) {
    Wire.beginTransmission(0x99);
    TEST_ASSERT_EQUAL(2, Wire.endTransmission());
}

// ----- Test 4: a two-byte write stores value at the given register -----
void test_write_register_captured_in_map(void) {
    WireMock::registerDevice(0x18, 1);
    Wire.beginTransmission(0x18);
    Wire.write((uint8_t)0x32);
    Wire.write((uint8_t)0xAB);
    Wire.endTransmission();
    TEST_ASSERT_EQUAL(0xAB, WireMock::registerMap[0x18][0x32]);
}

// ----- Test 5: requestFrom returns bytes pre-loaded by registerDevice -----
void test_request_from_returns_register_data(void) {
    uint8_t chipId[] = {0x80, 0x45};
    WireMock::registerDevice(0x18, 1, chipId, 2);
    Wire.requestFrom((uint8_t)0x18, (uint8_t)2);
    TEST_ASSERT_EQUAL(2, Wire.available());
    TEST_ASSERT_EQUAL(0x80, Wire.read());
    TEST_ASSERT_EQUAL(0x45, Wire.read());
}

// ----- Test 6: available() decrements correctly after each read -----
void test_available_decrements_on_read(void) {
    uint8_t d[] = {1, 2, 3, 4};
    WireMock::registerDevice(0x50, 0, d, 4);
    Wire.requestFrom((uint8_t)0x50, (uint8_t)4);
    TEST_ASSERT_EQUAL(4, Wire.available());
    Wire.read();
    TEST_ASSERT_EQUAL(3, Wire.available());
    Wire.read();
    TEST_ASSERT_EQUAL(2, Wire.available());
}

// ----- Test 7: reading past the last byte returns -1 -----
void test_read_past_end_returns_minus_one(void) {
    WireMock::registerDevice(0x18, 1);
    Wire.requestFrom((uint8_t)0x18, (uint8_t)1);
    Wire.read();                               // Consume the one byte
    TEST_ASSERT_EQUAL(-1, Wire.read());        // Now empty
}

// ----- Test 8: writes to two different addresses are fully isolated -----
void test_separate_addresses_isolated(void) {
    WireMock::registerDevice(0x18, 1);
    WireMock::registerDevice(0x50, 0);

    Wire.beginTransmission(0x18);
    Wire.write((uint8_t)0x00); Wire.write((uint8_t)0xAA);
    Wire.endTransmission();

    Wire.beginTransmission(0x50);
    Wire.write((uint8_t)0x00); Wire.write((uint8_t)0xBB);
    Wire.endTransmission();

    TEST_ASSERT_EQUAL(0xAA, WireMock::registerMap[0x18][0x00]);
    TEST_ASSERT_EQUAL(0xBB, WireMock::registerMap[0x50][0x00]);
}

// ----- Test 9: reset clears all state including register map and bus flags -----
void test_reset_clears_all_state(void) {
    WireMock::registerDevice(0x18, 1);
    Wire.begin(7, 8);
    Wire.beginTransmission(0x18);
    Wire.write((uint8_t)0x01); Wire.write((uint8_t)0xFF);
    Wire.endTransmission();

    WireMock::reset();

    TEST_ASSERT_TRUE(WireMock::registerMap.empty());
    TEST_ASSERT_FALSE(WireMock::isBusInitialized(0));
    TEST_ASSERT_FALSE(WireMock::isBusInitialized(1));
    TEST_ASSERT_FALSE(WireMock::isBusInitialized(2));
}

// ----- Test 10: all three I2C buses can be initialised independently -----
void test_three_bus_instances_independent(void) {
    Wire.begin(48, 54);   // Bus 0 EXT
    Wire.begin(7, 8);     // Bus 1 ONBOARD
    Wire.begin(28, 29);   // Bus 2 EXP
    TEST_ASSERT_TRUE(WireMock::isBusInitialized(0));
    TEST_ASSERT_TRUE(WireMock::isBusInitialized(1));
    TEST_ASSERT_TRUE(WireMock::isBusInitialized(2));
}

// ----- Test 11: pin claimed status is set after begin -----
void test_pin_claimed_after_begin(void) {
    Wire.begin(7, 8);
    TEST_ASSERT_TRUE(WireMock::pinClaimed(7));
    TEST_ASSERT_TRUE(WireMock::pinClaimed(8));
    TEST_ASSERT_FALSE(WireMock::pinClaimed(48));
}

// ----- Test 12: write to unregistered address returns NACK and does not store -----
void test_unregistered_address_write_ignored(void) {
    Wire.beginTransmission(0x77);
    Wire.write((uint8_t)0x10); Wire.write((uint8_t)0xFF);
    uint8_t r = Wire.endTransmission();
    TEST_ASSERT_EQUAL(2, r);  // NACK

    bool stored = WireMock::registerMap.count(0x77) &&
                  WireMock::registerMap[0x77].count(0x10);
    TEST_ASSERT_FALSE(stored);
}

// ----- Test 13: ES8311 chip-ID bytes (0x80, 0x45) are readable via requestFrom -----
void test_chip_id_es8311_readable(void) {
    uint8_t id[] = {0x80, 0x45};
    WireMock::registerDevice(0x18, 1, id, 2);
    Wire.requestFrom((uint8_t)0x18, (uint8_t)2);
    uint8_t b0 = (uint8_t)Wire.read();
    uint8_t b1 = (uint8_t)Wire.read();
    TEST_ASSERT_EQUAL(0x80, b0);
    TEST_ASSERT_EQUAL(0x45, b1);
}

// ----- Test 14: writing two different registers preserves both values -----
void test_write_preserves_other_registers(void) {
    WireMock::registerDevice(0x18, 1);

    Wire.beginTransmission(0x18); Wire.write((uint8_t)0x10); Wire.write((uint8_t)0xAA); Wire.endTransmission();
    Wire.beginTransmission(0x18); Wire.write((uint8_t)0x20); Wire.write((uint8_t)0xBB); Wire.endTransmission();

    TEST_ASSERT_EQUAL(0xAA, WireMock::registerMap[0x18][0x10]);
    TEST_ASSERT_EQUAL(0xBB, WireMock::registerMap[0x18][0x20]);
}

// ----- Test 15: single-byte write (address-only) does not crash or corrupt -----
void test_write_single_byte_only_does_not_crash(void) {
    WireMock::registerDevice(0x18, 1);
    Wire.beginTransmission(0x18);
    Wire.write((uint8_t)0xFF);  // Only a register pointer — no data byte
    Wire.endTransmission();
    // Should complete without crash or assertion; mock state is consistent
    TEST_ASSERT_TRUE(true);
}

// ----- Test 16: Wire1 alias calls the same global WireMock state -----
void test_wire1_alias_works(void) {
    Wire1.begin(48, 54);
    TEST_ASSERT_TRUE(WireMock::isBusInitialized(0));
}

// ----- Test 17: begin() not required before transmit — mock accepts registered address -----
void test_no_begin_before_transmit_still_acks_registered_device(void) {
    WireMock::registerDevice(0x18, 1);
    // Wire.begin() deliberately NOT called
    Wire.beginTransmission(0x18);
    uint8_t r = Wire.endTransmission();
    // Registered address -> ACK even without begin()
    TEST_ASSERT_EQUAL(0, r);
}

// ===== Main =====
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_begin_initializes_bus_by_pin);
    RUN_TEST(test_probe_ack_when_device_registered);
    RUN_TEST(test_probe_nack_when_device_absent);
    RUN_TEST(test_write_register_captured_in_map);
    RUN_TEST(test_request_from_returns_register_data);
    RUN_TEST(test_available_decrements_on_read);
    RUN_TEST(test_read_past_end_returns_minus_one);
    RUN_TEST(test_separate_addresses_isolated);
    RUN_TEST(test_reset_clears_all_state);
    RUN_TEST(test_three_bus_instances_independent);
    RUN_TEST(test_pin_claimed_after_begin);
    RUN_TEST(test_unregistered_address_write_ignored);
    RUN_TEST(test_chip_id_es8311_readable);
    RUN_TEST(test_write_preserves_other_registers);
    RUN_TEST(test_write_single_byte_only_does_not_crash);
    RUN_TEST(test_wire1_alias_works);
    RUN_TEST(test_no_begin_before_transmit_still_acks_registered_device);

    return UNITY_END();
}
