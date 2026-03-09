// test_hal_button_device.cpp — HalButton HAL device lifecycle tests
// Verifies: descriptor, probe, init with default/config pin, deinit, reinit, health

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
#ifndef RESET_BUTTON_PIN
#define RESET_BUTTON_PIN 46
#endif

// ===== Mock HalButton (matches src/hal/hal_button.cpp) =====
class HalButtonTest : public HalDevice {
public:
    int _pin;

    HalButtonTest(int pin) : _pin(pin) {
        memset(&_descriptor, 0, sizeof(_descriptor));
        strncpy(_descriptor.compatible, "generic,tact-switch", 31);
        strncpy(_descriptor.name, "Reset Button", 32);
        strncpy(_descriptor.manufacturer, "Generic", 32);
        _descriptor.type = HAL_DEV_INPUT;
        _descriptor.bus.type = HAL_BUS_GPIO;
        _descriptor.bus.index = 0;
        _descriptor.bus.pinA = pin;
        _descriptor.channelCount = 0;
        _initPriority = HAL_PRIORITY_IO;
    }

    bool probe() override {
        _state = HAL_STATE_DETECTED;
        return true;
    }

    HalInitResult init() override {
        HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
        if (cfg && cfg->valid && cfg->gpioA >= 0) {
            _pin = cfg->gpioA;
        }
        _descriptor.bus.pinA = _pin;
        _state = HAL_STATE_AVAILABLE;
        _ready = true;
        return hal_init_ok();
    }

    void deinit() override {
        _ready = false;
        _state = HAL_STATE_REMOVED;
    }

    void dumpConfig() override {}
    bool healthCheck() override { return _ready; }
    int getPin() const { return _pin; }
};

// ===== Reset helpers =====
static HalButtonTest* dev = nullptr;

void setUp() {
    ArduinoMock::reset();
    HalDeviceManager::instance().reset();
    dev = new HalButtonTest(RESET_BUTTON_PIN);
    HalDeviceManager::instance().registerDevice(dev, HAL_DISC_BUILTIN);
}

void tearDown() {
    HalDeviceManager::instance().reset();
    dev = nullptr;
}

// ===== Tests =====

void test_button_descriptor_fields() {
    TEST_ASSERT_EQUAL_STRING("generic,tact-switch", dev->getDescriptor().compatible);
    TEST_ASSERT_EQUAL(HAL_DEV_INPUT, dev->getDescriptor().type);
    TEST_ASSERT_EQUAL(HAL_BUS_GPIO, dev->getDescriptor().bus.type);
    TEST_ASSERT_EQUAL_STRING("Reset Button", dev->getDescriptor().name);
}

void test_button_probe_succeeds() {
    TEST_ASSERT_TRUE(dev->probe());
    TEST_ASSERT_EQUAL(HAL_STATE_DETECTED, dev->_state);
}

void test_button_init_default_pin() {
    dev->probe();
    HalInitResult r = dev->init();
    TEST_ASSERT_TRUE(r.success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev->_state);
    TEST_ASSERT_TRUE(dev->_ready);
    TEST_ASSERT_EQUAL(RESET_BUTTON_PIN, dev->getPin());
}

void test_button_init_config_override() {
    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.valid = true;
    cfg.gpioA = 12;
    cfg.gpioB = -1; cfg.gpioC = -1; cfg.gpioD = -1;
    cfg.pinSda = -1; cfg.pinScl = -1; cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255; cfg.enabled = true;
    HalDeviceManager::instance().setConfig(dev->getSlot(), cfg);

    dev->probe();
    dev->init();
    TEST_ASSERT_EQUAL(12, dev->getPin());
    TEST_ASSERT_EQUAL(12, dev->getDescriptor().bus.pinA);
}

void test_button_deinit() {
    dev->probe();
    dev->init();
    TEST_ASSERT_TRUE(dev->_ready);

    dev->deinit();
    TEST_ASSERT_FALSE(dev->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, dev->_state);
}

void test_button_reinit_with_new_pin() {
    dev->probe();
    dev->init();
    TEST_ASSERT_EQUAL(RESET_BUTTON_PIN, dev->getPin());

    dev->deinit();

    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.valid = true;
    cfg.gpioA = 8;
    cfg.gpioB = -1; cfg.gpioC = -1; cfg.gpioD = -1;
    cfg.pinSda = -1; cfg.pinScl = -1; cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255; cfg.enabled = true;
    HalDeviceManager::instance().setConfig(dev->getSlot(), cfg);

    dev->init();
    TEST_ASSERT_EQUAL(8, dev->getPin());
    TEST_ASSERT_EQUAL(8, dev->getDescriptor().bus.pinA);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_button_descriptor_fields);
    RUN_TEST(test_button_probe_succeeds);
    RUN_TEST(test_button_init_default_pin);
    RUN_TEST(test_button_init_config_override);
    RUN_TEST(test_button_deinit);
    RUN_TEST(test_button_reinit_with_new_pin);
    return UNITY_END();
}
