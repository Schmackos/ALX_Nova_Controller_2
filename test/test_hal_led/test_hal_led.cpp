// test_hal_led.cpp — HalLed HAL device lifecycle tests
// Verifies: descriptor, probe, init, deinit, setOn (no-op under NATIVE_TEST)

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"

// Inline the .cpp files for native testing (test_build_src = no)
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// ===== config.h stubs =====
#ifndef LED_PIN
#define LED_PIN 1
#endif

// ===== Mock HalLed (mirrors src/hal/hal_led.cpp) =====
class HalLedTest : public HalDevice {
public:
    int _pin;

    HalLedTest(int pin) : _pin(pin) {
        memset(&_descriptor, 0, sizeof(_descriptor));
        strncpy(_descriptor.compatible, "generic,status-led", 31);
        strncpy(_descriptor.name, "Status LED", 32);
        strncpy(_descriptor.manufacturer, "Generic", 32);
        _descriptor.type = HAL_DEV_GPIO;
        _descriptor.bus.type = HAL_BUS_GPIO;
        _descriptor.bus.index = 0;
        _descriptor.bus.pinA = pin;
        _descriptor.channelCount = 1;
        _initPriority = HAL_PRIORITY_LATE;
    }

    bool probe() override {
        _state = HAL_STATE_DETECTED;
        return true;
    }

    HalInitResult init() override {
        HalDeviceManager::instance().claimPin(_pin, HAL_BUS_GPIO, 0, _slot);
        // pinMode/digitalWrite guarded by NATIVE_TEST in real impl
        _state = HAL_STATE_AVAILABLE;
        _ready = true;
        return hal_init_ok();
    }

    void deinit() override {
        HalDeviceManager::instance().releasePin(_pin);
        _ready = false;
        _state = HAL_STATE_REMOVED;
    }

    void setOn(bool state) {
        // No-op under NATIVE_TEST — real impl calls digitalWrite
        (void)state;
    }

    void dumpConfig() override {}
    bool healthCheck() override { return _ready; }
};

// ===== Reset helpers =====
static HalLedTest* dev = nullptr;

void setUp() {
    ArduinoMock::reset();
    HalDeviceManager::instance().reset();
    dev = new HalLedTest(LED_PIN);
    HalDeviceManager::instance().registerDevice(dev, HAL_DISC_BUILTIN);
}

void tearDown() {
    HalDeviceManager::instance().reset();
    dev = nullptr;
}

// ===== Tests =====

void test_led_descriptor_fields() {
    TEST_ASSERT_EQUAL_STRING("generic,status-led", dev->getDescriptor().compatible);
    TEST_ASSERT_EQUAL(HAL_DEV_GPIO, dev->getDescriptor().type);
    TEST_ASSERT_EQUAL(HAL_BUS_GPIO, dev->getDescriptor().bus.type);
    TEST_ASSERT_EQUAL_STRING("Status LED", dev->getDescriptor().name);
    TEST_ASSERT_EQUAL_STRING("Generic", dev->getDescriptor().manufacturer);
    TEST_ASSERT_EQUAL(LED_PIN, dev->getDescriptor().bus.pinA);
}

void test_led_probe_succeeds() {
    TEST_ASSERT_TRUE(dev->probe());
    TEST_ASSERT_EQUAL(HAL_STATE_DETECTED, dev->_state);
}

void test_led_init_sets_available() {
    dev->probe();
    HalInitResult r = dev->init();
    TEST_ASSERT_TRUE(r.success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev->_state);
    TEST_ASSERT_TRUE(dev->_ready);
}

void test_led_deinit_resets_state() {
    dev->probe();
    dev->init();
    TEST_ASSERT_TRUE(dev->_ready);

    dev->deinit();
    TEST_ASSERT_FALSE(dev->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, dev->_state);
}

void test_led_health_check() {
    TEST_ASSERT_FALSE(dev->healthCheck());  // before init
    dev->probe();
    dev->init();
    TEST_ASSERT_TRUE(dev->healthCheck());   // after init
    dev->deinit();
    TEST_ASSERT_FALSE(dev->healthCheck());  // after deinit
}

void test_led_set_on_no_crash() {
    dev->probe();
    dev->init();
    // setOn is a no-op under NATIVE_TEST — verify it doesn't crash
    dev->setOn(true);
    dev->setOn(false);
    TEST_ASSERT_TRUE(dev->_ready);
}

void test_led_init_priority_is_late() {
    TEST_ASSERT_EQUAL(HAL_PRIORITY_LATE, dev->getInitPriority());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_led_descriptor_fields);
    RUN_TEST(test_led_probe_succeeds);
    RUN_TEST(test_led_init_sets_available);
    RUN_TEST(test_led_deinit_resets_state);
    RUN_TEST(test_led_health_check);
    RUN_TEST(test_led_set_on_no_crash);
    RUN_TEST(test_led_init_priority_is_late);
    return UNITY_END();
}
