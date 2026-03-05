#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline HAL core for native testing =====
#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// ===== Inline mock DacDriver for testing =====
// Minimal DacDriver implementation matching src/dac_hal.h interface
// We can't include dac_hal.h directly (DAC_ENABLED guard + hardware deps)
// so we replicate the interface inline (matches test_io_registry pattern)

struct DacCapabilities {
    const char* name;
    const char* manufacturer;
    uint16_t deviceId;
    uint8_t maxChannels;
    bool hasHardwareVolume;
    bool hasI2cControl;
    bool needsIndependentClock;
    uint8_t i2cAddress;
    const uint32_t* supportedRates;
    uint8_t numSupportedRates;
    bool hasFilterModes;
    uint8_t numFilterModes;
};

class DacDriver {
public:
    virtual ~DacDriver() {}
    virtual const DacCapabilities& getCapabilities() const = 0;
    virtual bool init(const void* pins) = 0;
    virtual void deinit() = 0;
    virtual bool configure(uint32_t sampleRate, uint8_t bitDepth) = 0;
    virtual bool setVolume(uint8_t volume) = 0;
    virtual bool setMute(bool mute) = 0;
    virtual bool isReady() const = 0;
    virtual bool setFilterMode(uint8_t mode) { (void)mode; return false; }
    virtual const char* getFilterModeName(uint8_t mode) { (void)mode; return nullptr; }
};

// ===== HalAudioDevice (inline for test — without DAC_ENABLED guard) =====
class HalAudioDevice : public HalDevice {
public:
    virtual ~HalAudioDevice() {}
    virtual bool configure(uint32_t sampleRate, uint8_t bitDepth) = 0;
    virtual bool setVolume(uint8_t percent) = 0;
    virtual bool setMute(bool mute) = 0;
    virtual bool setFilterMode(uint8_t mode) { (void)mode; return false; }
    virtual const DacCapabilities* getLegacyCapabilities() const { return nullptr; }
protected:
    HalAudioDevice() : HalDevice() {}
};

// ===== HalDacAdapter (inline — references the mock DacDriver above) =====
class HalDacAdapter : public HalAudioDevice {
public:
    HalDacAdapter(DacDriver* driver, const DacCapabilities& caps, uint16_t priority = HAL_PRIORITY_HARDWARE)
        : _driver(driver), _caps(caps) {
        if (caps.name) strncpy(_descriptor.name, caps.name, 32);
        _descriptor.name[32] = '\0';
        if (caps.manufacturer) strncpy(_descriptor.manufacturer, caps.manufacturer, 32);
        _descriptor.manufacturer[32] = '\0';
        _descriptor.type = HAL_DEV_DAC;
        _descriptor.legacyId = caps.deviceId;
        _descriptor.i2cAddr = caps.i2cAddress;
        _descriptor.channelCount = caps.maxChannels;
        _descriptor.capabilities = 0;
        if (caps.hasHardwareVolume) _descriptor.capabilities |= HAL_CAP_HW_VOLUME;
        if (caps.hasFilterModes)    _descriptor.capabilities |= HAL_CAP_FILTERS;
        _descriptor.sampleRatesMask = 0;
        for (uint8_t i = 0; i < caps.numSupportedRates; i++) {
            switch (caps.supportedRates[i]) {
                case 8000:   _descriptor.sampleRatesMask |= HAL_RATE_8K;   break;
                case 16000:  _descriptor.sampleRatesMask |= HAL_RATE_16K;  break;
                case 44100:  _descriptor.sampleRatesMask |= HAL_RATE_44K1; break;
                case 48000:  _descriptor.sampleRatesMask |= HAL_RATE_48K;  break;
                case 96000:  _descriptor.sampleRatesMask |= HAL_RATE_96K;  break;
                case 192000: _descriptor.sampleRatesMask |= HAL_RATE_192K; break;
            }
        }
        _initPriority = priority;
    }

    bool probe() override {
        if (!_driver) return false;
        if (!_caps.hasI2cControl || _caps.i2cAddress == 0) return true;
        return true;  // In tests, I2C always succeeds
    }
    bool init() override { return _driver ? _driver->isReady() : false; }
    void deinit() override {
        if (_driver) _driver->deinit();
        _ready = false;
        _state = HAL_STATE_REMOVED;
    }
    void dumpConfig() override {}
    bool healthCheck() override { return _driver ? _driver->isReady() : false; }
    bool configure(uint32_t sr, uint8_t bd) override { return _driver ? _driver->configure(sr, bd) : false; }
    bool setVolume(uint8_t pct) override { return _driver ? _driver->setVolume(pct) : false; }
    bool setMute(bool m) override { return _driver ? _driver->setMute(m) : false; }
    bool setFilterMode(uint8_t mode) override { return _driver ? _driver->setFilterMode(mode) : false; }
    const DacCapabilities* getLegacyCapabilities() const override { return &_caps; }
    DacDriver* getDriver() { return _driver; }

private:
    DacDriver* _driver;
    DacCapabilities _caps;
};

// ===== Mock DacDriver (PCM5102A-like) =====
static const uint32_t PCM_RATES[] = {44100, 48000, 96000};

class MockPcm5102 : public DacDriver {
public:
    bool _ready = true;
    bool _muted = false;
    uint8_t _volume = 80;
    bool _deinited = false;
    DacCapabilities _caps;

    MockPcm5102() {
        _caps.name = "PCM5102A";
        _caps.manufacturer = "Texas Instruments";
        _caps.deviceId = 0x0001;
        _caps.maxChannels = 2;
        _caps.hasHardwareVolume = false;
        _caps.hasI2cControl = false;
        _caps.needsIndependentClock = false;
        _caps.i2cAddress = 0;
        _caps.supportedRates = PCM_RATES;
        _caps.numSupportedRates = 3;
        _caps.hasFilterModes = false;
        _caps.numFilterModes = 0;
    }

    const DacCapabilities& getCapabilities() const override { return _caps; }
    bool init(const void*) override { _ready = true; return true; }
    void deinit() override { _ready = false; _deinited = true; }
    bool configure(uint32_t, uint8_t) override { return true; }
    bool setVolume(uint8_t v) override { _volume = v; return false; } // No HW volume
    bool setMute(bool m) override { _muted = m; return false; }
    bool isReady() const override { return _ready; }
};

// ===== Mock DacDriver (ES8311-like) =====
static const uint32_t ES_RATES[] = {8000, 16000, 44100, 48000, 96000};

class MockEs8311 : public DacDriver {
public:
    bool _ready = true;
    DacCapabilities _caps;

    MockEs8311() {
        _caps.name = "ES8311";
        _caps.manufacturer = "Evergrande";
        _caps.deviceId = 0x0004;
        _caps.maxChannels = 1;
        _caps.hasHardwareVolume = true;
        _caps.hasI2cControl = true;
        _caps.needsIndependentClock = false;
        _caps.i2cAddress = 0x18;
        _caps.supportedRates = ES_RATES;
        _caps.numSupportedRates = 5;
        _caps.hasFilterModes = false;
        _caps.numFilterModes = 0;
    }

    const DacCapabilities& getCapabilities() const override { return _caps; }
    bool init(const void*) override { _ready = true; return true; }
    void deinit() override { _ready = false; }
    bool configure(uint32_t, uint8_t) override { return true; }
    bool setVolume(uint8_t) override { return true; }
    bool setMute(bool) override { return true; }
    bool isReady() const override { return _ready; }
};

// ===== Test Fixtures =====
static HalDeviceManager* mgr;

void setUp() {
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
}

void tearDown() {}

// ===== Test 1: Adapter wraps DacDriver correctly =====
void test_adapter_wraps_driver() {
    MockPcm5102 drv;
    HalDacAdapter adapter(&drv, drv.getCapabilities());

    TEST_ASSERT_EQUAL_STRING("PCM5102A", adapter.getDescriptor().name);
    TEST_ASSERT_EQUAL_STRING("Texas Instruments", adapter.getDescriptor().manufacturer);
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, adapter.getType());
    TEST_ASSERT_EQUAL(0x0001, adapter.getDescriptor().legacyId);
    TEST_ASSERT_EQUAL(0, adapter.getDescriptor().i2cAddr);
    TEST_ASSERT_EQUAL(2, adapter.getDescriptor().channelCount);
    TEST_ASSERT_EQUAL_PTR(&drv, adapter.getDriver());
}

// ===== Test 2: Adapter init delegates to driver =====
void test_adapter_init_delegates() {
    MockPcm5102 drv;
    HalDacAdapter adapter(&drv, drv.getCapabilities());

    TEST_ASSERT_TRUE(adapter.init());
    // When driver reports not ready, init returns false
    drv._ready = false;
    TEST_ASSERT_FALSE(adapter.init());
}

// ===== Test 3: Adapter deinit delegates and sets state =====
void test_adapter_deinit_sets_removed() {
    MockPcm5102 drv;
    HalDacAdapter adapter(&drv, drv.getCapabilities());
    adapter._ready = true;
    adapter._state = HAL_STATE_AVAILABLE;

    adapter.deinit();
    TEST_ASSERT_FALSE(adapter._ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, adapter._state);
    TEST_ASSERT_TRUE(drv._deinited);
}

// ===== Test 4: ready/state set correctly on lifecycle =====
void test_ready_state_lifecycle() {
    MockPcm5102 drv;
    HalDacAdapter adapter(&drv, drv.getCapabilities());

    // Register and init via manager
    int slot = mgr->registerDevice(&adapter, HAL_DISC_BUILTIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);
    TEST_ASSERT_FALSE(adapter._ready);
    TEST_ASSERT_EQUAL(HAL_STATE_UNKNOWN, adapter._state);

    // initAll triggers init → available
    mgr->initAll();
    TEST_ASSERT_TRUE(adapter._ready);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adapter._state);
}

// ===== Test 5: Compatible string populated from registration =====
void test_compatible_string() {
    MockPcm5102 drv;
    HalDacAdapter adapter(&drv, drv.getCapabilities());

    // Compatible is set when used with registry, not by adapter itself
    strncpy(const_cast<char*>(adapter.getDescriptor().compatible), "ti,pcm5102a", 31);
    TEST_ASSERT_EQUAL_STRING("ti,pcm5102a", adapter.getDescriptor().compatible);
}

// ===== Test 6: healthCheck delegates to driver =====
void test_health_check_delegates() {
    MockPcm5102 drv;
    HalDacAdapter adapter(&drv, drv.getCapabilities());

    TEST_ASSERT_TRUE(adapter.healthCheck());
    drv._ready = false;
    TEST_ASSERT_FALSE(adapter.healthCheck());
}

// ===== Test 7: Builtin registration produces correct entries =====
void test_builtin_registration() {
    // Simulate hal_register_builtins inline
    hal_registry_init();

    HalDriverEntry e1;
    memset(&e1, 0, sizeof(e1));
    strncpy(e1.compatible, "ti,pcm5102a", 31);
    e1.type = HAL_DEV_DAC;
    e1.legacyId = 0x0001;
    hal_registry_register(e1);

    HalDriverEntry e2;
    memset(&e2, 0, sizeof(e2));
    strncpy(e2.compatible, "evergrande,es8311", 31);
    e2.type = HAL_DEV_CODEC;
    e2.legacyId = 0x0004;
    hal_registry_register(e2);

    HalDriverEntry e3;
    memset(&e3, 0, sizeof(e3));
    strncpy(e3.compatible, "ti,pcm1808", 31);
    e3.type = HAL_DEV_ADC;
    e3.legacyId = 0;
    hal_registry_register(e3);

    TEST_ASSERT_EQUAL(3, hal_registry_count());
    TEST_ASSERT_NOT_NULL(hal_registry_find("ti,pcm5102a"));
    TEST_ASSERT_NOT_NULL(hal_registry_find("evergrande,es8311"));
    TEST_ASSERT_NOT_NULL(hal_registry_find("ti,pcm1808"));
    TEST_ASSERT_EQUAL(HAL_DEV_CODEC, hal_registry_find("evergrande,es8311")->type);
}

// ===== Test 8: Adapter probe — no I2C returns true immediately =====
void test_probe_no_i2c_returns_true() {
    MockPcm5102 drv;  // hasI2cControl=false, i2cAddress=0
    HalDacAdapter adapter(&drv, drv.getCapabilities());

    TEST_ASSERT_TRUE(adapter.probe());
}

// ===== Test 9: Adapter probe — with I2C returns true (mock) =====
void test_probe_with_i2c_returns_true() {
    MockEs8311 drv;  // hasI2cControl=true, i2cAddress=0x18
    HalDacAdapter adapter(&drv, drv.getCapabilities());

    // In native test, no Wire — probe skips I2C check
    TEST_ASSERT_TRUE(adapter.probe());
}

// ===== Test Runner =====
int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_adapter_wraps_driver);
    RUN_TEST(test_adapter_init_delegates);
    RUN_TEST(test_adapter_deinit_sets_removed);
    RUN_TEST(test_ready_state_lifecycle);
    RUN_TEST(test_compatible_string);
    RUN_TEST(test_health_check_delegates);
    RUN_TEST(test_builtin_registration);
    RUN_TEST(test_probe_no_i2c_returns_true);
    RUN_TEST(test_probe_with_i2c_returns_true);

    return UNITY_END();
}
