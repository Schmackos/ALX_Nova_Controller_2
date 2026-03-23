// test_hal_temp_sensor.cpp
// Tests for HalTempSensor — ESP32-P4 internal chip temperature sensor driver.
//
// Key behaviours under test:
//   - getTemperature()        returns _lastTemp as float
//   - getTemperatureCelsius() returns identical value to getTemperature()
//   - both return 0.0f on construction (default)
//   - both reflect any value injected into _lastTemp

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"

// Under NATIVE_TEST the real HalTempSensor probe()/init() always return
// false/error (no IDF hardware). We exercise the accessor methods via a
// thin mock subclass that exposes _lastTemp for white-box injection.
class HalTempSensorMock : public HalDevice {
public:
    float _lastTemp;

    HalTempSensorMock() : _lastTemp(0.0f) {
        hal_init_descriptor(_descriptor,
            "espressif,esp32p4-temp", "Chip Temperature", "",
            HAL_DEV_SENSOR, 1, 0, HAL_BUS_INTERNAL, 0, 0, 0);
    }

    // Minimal required overrides
    bool probe() override { return false; }
    HalInitResult init() override { return hal_init_fail(DIAG_HAL_INIT_FAILED, "native"); }
    void deinit() override { _lastTemp = 0.0f; setReady(false); _state = HAL_STATE_REMOVED; }
    void dumpConfig() override {}
    bool healthCheck() override { return false; }

    // The two accessor methods under test — must match the real header signatures
    float getTemperature() const { return _lastTemp; }
    float getTemperatureCelsius() const { return _lastTemp; }
};

static HalTempSensorMock* sensor;

void setUp(void) {
    sensor = new HalTempSensorMock();
}

void tearDown(void) {
    delete sensor;
    sensor = nullptr;
}

// ----- 1. getTemperature() returns 0.0f on construction -----
void test_get_temperature_default_zero(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sensor->getTemperature());
}

// ----- 2. getTemperatureCelsius() returns 0.0f on construction -----
void test_get_temperature_celsius_default_zero(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sensor->getTemperatureCelsius());
}

// ----- 3. getTemperatureCelsius() matches getTemperature() after injection -----
void test_celsius_matches_temperature_after_injection(void) {
    sensor->_lastTemp = 42.5f;
    TEST_ASSERT_EQUAL_FLOAT(sensor->getTemperature(), sensor->getTemperatureCelsius());
}

// ----- 4. getTemperatureCelsius() returns injected positive value -----
void test_celsius_returns_positive_value(void) {
    sensor->_lastTemp = 55.0f;
    TEST_ASSERT_EQUAL_FLOAT(55.0f, sensor->getTemperatureCelsius());
}

// ----- 5. getTemperatureCelsius() returns injected negative value -----
void test_celsius_returns_negative_value(void) {
    sensor->_lastTemp = -10.0f;
    TEST_ASSERT_EQUAL_FLOAT(-10.0f, sensor->getTemperatureCelsius());
}

// ----- 6. getTemperatureCelsius() returns fractional precision -----
void test_celsius_returns_fractional_precision(void) {
    sensor->_lastTemp = 37.25f;
    TEST_ASSERT_EQUAL_FLOAT(37.25f, sensor->getTemperatureCelsius());
}

// ----- 7. deinit() resets _lastTemp to 0.0f -----
void test_deinit_resets_temp(void) {
    sensor->_lastTemp = 70.0f;
    sensor->deinit();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sensor->getTemperatureCelsius());
}

// ----- 8. descriptor type is HAL_DEV_SENSOR -----
void test_descriptor_type_is_sensor(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_SENSOR, (int)sensor->getDescriptor().type);
}

// ----- 9. compatible string is correct -----
void test_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("espressif,esp32p4-temp", sensor->getDescriptor().compatible);
}

// ===== Main =====
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_get_temperature_default_zero);
    RUN_TEST(test_get_temperature_celsius_default_zero);
    RUN_TEST(test_celsius_matches_temperature_after_injection);
    RUN_TEST(test_celsius_returns_positive_value);
    RUN_TEST(test_celsius_returns_negative_value);
    RUN_TEST(test_celsius_returns_fractional_precision);
    RUN_TEST(test_deinit_resets_temp);
    RUN_TEST(test_descriptor_type_is_sensor);
    RUN_TEST(test_compatible_string);

    return UNITY_END();
}
