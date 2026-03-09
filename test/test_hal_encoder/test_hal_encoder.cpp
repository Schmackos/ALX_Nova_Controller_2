// test_hal_encoder.cpp — HalEncoder HAL device lifecycle tests
// Verifies: descriptor, probe, init with default/config pins, deinit, reinit, pin accessors

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
#ifndef ENCODER_A_PIN
#define ENCODER_A_PIN 5
#endif
#ifndef ENCODER_B_PIN
#define ENCODER_B_PIN 6
#endif
#ifndef ENCODER_SW_PIN
#define ENCODER_SW_PIN 7
#endif

// ===== Mock HalEncoder (matches src/hal/hal_encoder.cpp) =====
class HalEncoderTest : public HalDevice {
public:
    int _pinA, _pinB, _pinSw;

    HalEncoderTest(int pinA, int pinB, int pinSw)
        : _pinA(pinA), _pinB(pinB), _pinSw(pinSw)
    {
        memset(&_descriptor, 0, sizeof(_descriptor));
        strncpy(_descriptor.compatible, "alps,ec11", 31);
        strncpy(_descriptor.name, "Rotary Encoder", 32);
        strncpy(_descriptor.manufacturer, "Alps", 32);
        _descriptor.type = HAL_DEV_INPUT;
        _descriptor.bus.type = HAL_BUS_GPIO;
        _descriptor.bus.index = 0;
        _descriptor.bus.pinA = pinA;
        _descriptor.bus.pinB = pinB;
        _descriptor.channelCount = 0;
        _initPriority = HAL_PRIORITY_IO;
    }

    bool probe() override {
        _state = HAL_STATE_DETECTED;
        return true;
    }

    HalInitResult init() override {
        HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
        if (cfg && cfg->valid) {
            if (cfg->gpioA >= 0) _pinA = cfg->gpioA;
            if (cfg->gpioB >= 0) _pinB = cfg->gpioB;
            if (cfg->gpioC >= 0) _pinSw = cfg->gpioC;
        }
        _descriptor.bus.pinA = _pinA;
        _descriptor.bus.pinB = _pinB;
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

    int getPinA() const { return _pinA; }
    int getPinB() const { return _pinB; }
    int getPinSw() const { return _pinSw; }
};

// ===== Reset helpers =====
static HalEncoderTest* dev = nullptr;

void setUp() {
    ArduinoMock::reset();
    HalDeviceManager::instance().reset();
    dev = new HalEncoderTest(ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_SW_PIN);
    HalDeviceManager::instance().registerDevice(dev, HAL_DISC_BUILTIN);
}

void tearDown() {
    HalDeviceManager::instance().reset();
    dev = nullptr;
}

// ===== Tests =====

void test_encoder_descriptor_fields() {
    TEST_ASSERT_EQUAL_STRING("alps,ec11", dev->getDescriptor().compatible);
    TEST_ASSERT_EQUAL(HAL_DEV_INPUT, dev->getDescriptor().type);
    TEST_ASSERT_EQUAL(HAL_BUS_GPIO, dev->getDescriptor().bus.type);
    TEST_ASSERT_EQUAL_STRING("Rotary Encoder", dev->getDescriptor().name);
    TEST_ASSERT_EQUAL_STRING("Alps", dev->getDescriptor().manufacturer);
}

void test_encoder_probe_succeeds() {
    TEST_ASSERT_TRUE(dev->probe());
    TEST_ASSERT_EQUAL(HAL_STATE_DETECTED, dev->_state);
}

void test_encoder_init_default_pins() {
    dev->probe();
    HalInitResult r = dev->init();
    TEST_ASSERT_TRUE(r.success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev->_state);
    TEST_ASSERT_TRUE(dev->_ready);
    TEST_ASSERT_EQUAL(ENCODER_A_PIN, dev->getPinA());
    TEST_ASSERT_EQUAL(ENCODER_B_PIN, dev->getPinB());
    TEST_ASSERT_EQUAL(ENCODER_SW_PIN, dev->getPinSw());
}

void test_encoder_init_config_override_all() {
    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.valid = true;
    cfg.gpioA = 32; cfg.gpioB = 33; cfg.gpioC = 36;
    cfg.gpioD = -1;
    cfg.pinSda = -1; cfg.pinScl = -1; cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255; cfg.enabled = true;
    HalDeviceManager::instance().setConfig(dev->getSlot(), cfg);

    dev->probe();
    dev->init();
    TEST_ASSERT_EQUAL(32, dev->getPinA());
    TEST_ASSERT_EQUAL(33, dev->getPinB());
    TEST_ASSERT_EQUAL(36, dev->getPinSw());
    TEST_ASSERT_EQUAL(32, dev->getDescriptor().bus.pinA);
    TEST_ASSERT_EQUAL(33, dev->getDescriptor().bus.pinB);
}

void test_encoder_init_partial_override() {
    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.valid = true;
    cfg.gpioA = 10;  // Only override pin A
    cfg.gpioB = -1;  // Keep default
    cfg.gpioC = -1;  // Keep default
    cfg.gpioD = -1;
    cfg.pinSda = -1; cfg.pinScl = -1; cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255; cfg.enabled = true;
    HalDeviceManager::instance().setConfig(dev->getSlot(), cfg);

    dev->probe();
    dev->init();
    TEST_ASSERT_EQUAL(10, dev->getPinA());
    TEST_ASSERT_EQUAL(ENCODER_B_PIN, dev->getPinB());
    TEST_ASSERT_EQUAL(ENCODER_SW_PIN, dev->getPinSw());
}

void test_encoder_deinit() {
    dev->probe();
    dev->init();
    TEST_ASSERT_TRUE(dev->_ready);

    dev->deinit();
    TEST_ASSERT_FALSE(dev->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, dev->_state);
}

void test_encoder_reinit_with_new_pins() {
    dev->probe();
    dev->init();
    TEST_ASSERT_EQUAL(ENCODER_A_PIN, dev->getPinA());

    dev->deinit();

    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.valid = true;
    cfg.gpioA = 40; cfg.gpioB = 41; cfg.gpioC = 42;
    cfg.gpioD = -1;
    cfg.pinSda = -1; cfg.pinScl = -1; cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255; cfg.enabled = true;
    HalDeviceManager::instance().setConfig(dev->getSlot(), cfg);

    dev->init();
    TEST_ASSERT_EQUAL(40, dev->getPinA());
    TEST_ASSERT_EQUAL(41, dev->getPinB());
    TEST_ASSERT_EQUAL(42, dev->getPinSw());
}

void test_encoder_health_check() {
    TEST_ASSERT_FALSE(dev->healthCheck());
    dev->probe();
    dev->init();
    TEST_ASSERT_TRUE(dev->healthCheck());
    dev->deinit();
    TEST_ASSERT_FALSE(dev->healthCheck());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_encoder_descriptor_fields);
    RUN_TEST(test_encoder_probe_succeeds);
    RUN_TEST(test_encoder_init_default_pins);
    RUN_TEST(test_encoder_init_config_override_all);
    RUN_TEST(test_encoder_init_partial_override);
    RUN_TEST(test_encoder_deinit);
    RUN_TEST(test_encoder_reinit_with_new_pins);
    RUN_TEST(test_encoder_health_check);
    return UNITY_END();
}
