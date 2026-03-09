// test_hal_buzzer.cpp — HalBuzzer HAL device lifecycle tests
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

// ===== Minimal buzzer stubs for native test =====
static int _test_buzzer_init_pin = -1;
static int _test_buzzer_init_count = 0;
static int _test_buzzer_deinit_count = 0;

void buzzer_init(int pin) {
    _test_buzzer_init_pin = (pin >= 0) ? pin : 45;
    _test_buzzer_init_count++;
}
void buzzer_deinit() {
    _test_buzzer_deinit_count++;
    _test_buzzer_init_pin = -1;
}

// ===== Minimal config.h stubs =====
#ifndef BUZZER_PIN
#define BUZZER_PIN 45
#endif

// ===== Mock HalBuzzer (matches src/hal/hal_buzzer.cpp) =====
class HalBuzzerTest : public HalDevice {
public:
    int _pin;

    HalBuzzerTest(int pin) : _pin(pin) {
        memset(&_descriptor, 0, sizeof(_descriptor));
        strncpy(_descriptor.compatible, "generic,piezo-buzzer", 31);
        strncpy(_descriptor.name, "Piezo Buzzer", 32);
        strncpy(_descriptor.manufacturer, "Generic", 32);
        _descriptor.type = HAL_DEV_GPIO;
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
        buzzer_init(_pin);
        _state = HAL_STATE_AVAILABLE;
        _ready = true;
        return hal_init_ok();
    }

    void deinit() override {
        buzzer_deinit();
        _ready = false;
        _state = HAL_STATE_REMOVED;
    }

    void dumpConfig() override {}
    bool healthCheck() override { return _ready; }
};

// ===== Reset helpers =====
static HalBuzzerTest* dev = nullptr;

void setUp() {
    ArduinoMock::reset();
    HalDeviceManager::instance().reset();
    _test_buzzer_init_pin = -1;
    _test_buzzer_init_count = 0;
    _test_buzzer_deinit_count = 0;
    dev = new HalBuzzerTest(BUZZER_PIN);
    HalDeviceManager::instance().registerDevice(dev, HAL_DISC_BUILTIN);
}

void tearDown() {
    HalDeviceManager::instance().reset();
    dev = nullptr;
}

// ===== Tests =====

void test_buzzer_descriptor_fields() {
    TEST_ASSERT_EQUAL_STRING("generic,piezo-buzzer", dev->getDescriptor().compatible);
    TEST_ASSERT_EQUAL(HAL_DEV_GPIO, dev->getDescriptor().type);
    TEST_ASSERT_EQUAL(HAL_BUS_GPIO, dev->getDescriptor().bus.type);
    TEST_ASSERT_EQUAL_STRING("Piezo Buzzer", dev->getDescriptor().name);
    TEST_ASSERT_EQUAL_STRING("Generic", dev->getDescriptor().manufacturer);
}

void test_buzzer_probe_succeeds() {
    TEST_ASSERT_TRUE(dev->probe());
    TEST_ASSERT_EQUAL(HAL_STATE_DETECTED, dev->_state);
}

void test_buzzer_init_default_pin() {
    dev->probe();
    HalInitResult r = dev->init();
    TEST_ASSERT_TRUE(r.success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev->_state);
    TEST_ASSERT_TRUE(dev->_ready);
    TEST_ASSERT_EQUAL(BUZZER_PIN, _test_buzzer_init_pin);
    TEST_ASSERT_EQUAL(1, _test_buzzer_init_count);
}

void test_buzzer_init_config_override() {
    // Set config with gpioA=10 before init
    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.valid = true;
    cfg.gpioA = 10;
    cfg.gpioB = -1; cfg.gpioC = -1; cfg.gpioD = -1;
    cfg.pinSda = -1; cfg.pinScl = -1; cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255; cfg.enabled = true;
    HalDeviceManager::instance().setConfig(dev->getSlot(), cfg);

    dev->probe();
    dev->init();
    TEST_ASSERT_EQUAL(10, _test_buzzer_init_pin);
    TEST_ASSERT_EQUAL(10, dev->getDescriptor().bus.pinA);
}

void test_buzzer_deinit_resets_state() {
    dev->probe();
    dev->init();
    TEST_ASSERT_TRUE(dev->_ready);

    dev->deinit();
    TEST_ASSERT_FALSE(dev->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, dev->_state);
    TEST_ASSERT_EQUAL(1, _test_buzzer_deinit_count);
}

void test_buzzer_reinit_with_new_pin() {
    dev->probe();
    dev->init();
    TEST_ASSERT_EQUAL(BUZZER_PIN, _test_buzzer_init_pin);

    dev->deinit();

    // Change config to new pin
    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.valid = true;
    cfg.gpioA = 6;
    cfg.gpioB = -1; cfg.gpioC = -1; cfg.gpioD = -1;
    cfg.pinSda = -1; cfg.pinScl = -1; cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255; cfg.enabled = true;
    HalDeviceManager::instance().setConfig(dev->getSlot(), cfg);

    dev->init();
    TEST_ASSERT_EQUAL(6, _test_buzzer_init_pin);
    TEST_ASSERT_EQUAL(6, dev->getDescriptor().bus.pinA);
    TEST_ASSERT_EQUAL(2, _test_buzzer_init_count);  // init called twice
}

void test_buzzer_health_check() {
    TEST_ASSERT_FALSE(dev->healthCheck());  // before init
    dev->probe();
    dev->init();
    TEST_ASSERT_TRUE(dev->healthCheck());   // after init
    dev->deinit();
    TEST_ASSERT_FALSE(dev->healthCheck());  // after deinit
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_buzzer_descriptor_fields);
    RUN_TEST(test_buzzer_probe_succeeds);
    RUN_TEST(test_buzzer_init_default_pin);
    RUN_TEST(test_buzzer_init_config_override);
    RUN_TEST(test_buzzer_deinit_resets_state);
    RUN_TEST(test_buzzer_reinit_with_new_pin);
    RUN_TEST(test_buzzer_health_check);
    return UNITY_END();
}
