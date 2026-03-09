// test_ns4150b.cpp — HalNs4150b HAL device lifecycle tests
// Verifies: descriptor, probe, init starts disabled, setEnable, config override, deinit, reinit

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
#ifndef ES8311_PA_PIN
#define ES8311_PA_PIN 53
#endif

// ===== Mock HalNs4150b (matches src/hal/hal_ns4150b.cpp) =====
class HalNs4150bTest : public HalDevice {
public:
    int _paPin;
    bool _enabled;

    HalNs4150bTest(int paPin) : _paPin(paPin), _enabled(false) {
        memset(&_descriptor, 0, sizeof(_descriptor));
        strncpy(_descriptor.compatible, "ns,ns4150b-amp", 31);
        strncpy(_descriptor.name, "NS4150B Amp", 32);
        strncpy(_descriptor.manufacturer, "Nsiway", 32);
        _descriptor.type = HAL_DEV_AMP;
        _descriptor.bus.type = HAL_BUS_GPIO;
        _descriptor.bus.index = 0;
        _descriptor.bus.pinA = paPin;
        _descriptor.channelCount = 1;
        _initPriority = HAL_PRIORITY_IO;
    }

    bool probe() override {
        _state = HAL_STATE_DETECTED;
        return true;
    }

    HalInitResult init() override {
        HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
        if (cfg && cfg->valid && cfg->gpioA >= 0) {
            _paPin = cfg->gpioA;
        }
        _descriptor.bus.pinA = _paPin;
        // Start disabled — DAC readiness gates enable
        _enabled = false;
        _ready = true;
        _state = HAL_STATE_AVAILABLE;
        return hal_init_ok();
    }

    void deinit() override {
        _enabled = false;
        _ready = false;
        _state = HAL_STATE_REMOVED;
    }

    void dumpConfig() override {}
    bool healthCheck() override { return _ready; }

    void setEnable(bool en) { _enabled = en; }
    bool isEnabled() const { return _enabled; }
    int getPin() const { return _paPin; }
};

// ===== Reset helpers =====
static HalNs4150bTest* dev = nullptr;

void setUp() {
    ArduinoMock::reset();
    HalDeviceManager::instance().reset();
    dev = new HalNs4150bTest(ES8311_PA_PIN);
    HalDeviceManager::instance().registerDevice(dev, HAL_DISC_BUILTIN);
}

void tearDown() {
    HalDeviceManager::instance().reset();
    dev = nullptr;
}

// ===== Tests =====

void test_ns4150b_descriptor_fields() {
    TEST_ASSERT_EQUAL_STRING("ns,ns4150b-amp", dev->getDescriptor().compatible);
    TEST_ASSERT_EQUAL(HAL_DEV_AMP, dev->getDescriptor().type);
    TEST_ASSERT_EQUAL(HAL_BUS_GPIO, dev->getDescriptor().bus.type);
    TEST_ASSERT_EQUAL_STRING("NS4150B Amp", dev->getDescriptor().name);
    TEST_ASSERT_EQUAL_STRING("Nsiway", dev->getDescriptor().manufacturer);
}

void test_ns4150b_probe_succeeds() {
    TEST_ASSERT_TRUE(dev->probe());
    TEST_ASSERT_EQUAL(HAL_STATE_DETECTED, dev->_state);
}

void test_ns4150b_init_starts_disabled() {
    dev->probe();
    HalInitResult r = dev->init();
    TEST_ASSERT_TRUE(r.success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev->_state);
    TEST_ASSERT_TRUE(dev->_ready);
    TEST_ASSERT_FALSE(dev->isEnabled());  // Must start disabled
    TEST_ASSERT_EQUAL(ES8311_PA_PIN, dev->getPin());
}

void test_ns4150b_set_enable() {
    dev->probe();
    dev->init();
    TEST_ASSERT_FALSE(dev->isEnabled());

    dev->setEnable(true);
    TEST_ASSERT_TRUE(dev->isEnabled());

    dev->setEnable(false);
    TEST_ASSERT_FALSE(dev->isEnabled());
}

void test_ns4150b_config_pin_override() {
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
    TEST_ASSERT_EQUAL(10, dev->getPin());
    TEST_ASSERT_EQUAL(10, dev->getDescriptor().bus.pinA);
}

void test_ns4150b_default_pin_without_config() {
    dev->probe();
    dev->init();
    TEST_ASSERT_EQUAL(ES8311_PA_PIN, dev->getPin());
}

void test_ns4150b_deinit_disables() {
    dev->probe();
    dev->init();
    dev->setEnable(true);
    TEST_ASSERT_TRUE(dev->isEnabled());

    dev->deinit();
    TEST_ASSERT_FALSE(dev->isEnabled());
    TEST_ASSERT_FALSE(dev->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, dev->_state);
}

void test_ns4150b_reinit_with_new_pin() {
    dev->probe();
    dev->init();
    TEST_ASSERT_EQUAL(ES8311_PA_PIN, dev->getPin());

    dev->deinit();

    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.valid = true;
    cfg.gpioA = 25;
    cfg.gpioB = -1; cfg.gpioC = -1; cfg.gpioD = -1;
    cfg.pinSda = -1; cfg.pinScl = -1; cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255; cfg.enabled = true;
    HalDeviceManager::instance().setConfig(dev->getSlot(), cfg);

    dev->init();
    TEST_ASSERT_EQUAL(25, dev->getPin());
    TEST_ASSERT_FALSE(dev->isEnabled());  // Should start disabled again
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ns4150b_descriptor_fields);
    RUN_TEST(test_ns4150b_probe_succeeds);
    RUN_TEST(test_ns4150b_init_starts_disabled);
    RUN_TEST(test_ns4150b_set_enable);
    RUN_TEST(test_ns4150b_config_pin_override);
    RUN_TEST(test_ns4150b_default_pin_without_config);
    RUN_TEST(test_ns4150b_deinit_disables);
    RUN_TEST(test_ns4150b_reinit_with_new_pin);
    return UNITY_END();
}
