#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline HAL implementation for native testing =====
#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_audio_device.h"

// Inline the .cpp files
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
#include "../../src/hal/hal_pipeline_bridge.cpp"
#include "../../src/hal/hal_settings.cpp"

// ===== Mock DacDriver for adapter tests =====
#ifdef DAC_ENABLED

// Minimal DacDriver mock
static uint32_t mockSupportedRates[] = {44100, 48000, 96000};
static DacCapabilities mockDacCaps = {
    "MockDAC",             // name
    "MockManufacturer",    // manufacturer
    0x0001,                // deviceId
    2,                     // maxChannels
    false,                 // hasHardwareVolume
    false,                 // hasI2cControl
    false,                 // needsIndependentClock
    0x00,                  // i2cAddress
    mockSupportedRates,    // supportedRates
    3,                     // numSupportedRates
    false,                 // hasFilterModes
    0                      // numFilterModes
};

class MockDacDriver : public DacDriver {
public:
    bool initCalled = false;
    bool deinitCalled = false;
    bool readyState = true;
    uint8_t lastVolume = 0;
    bool lastMute = false;

    const DacCapabilities& getCapabilities() const override { return mockDacCaps; }
    bool init(const DacPinConfig& pins) override { initCalled = true; return true; }
    void deinit() override { deinitCalled = true; }
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override { return true; }
    bool setVolume(uint8_t volume) override { lastVolume = volume; return true; }
    bool setMute(bool mute) override { lastMute = mute; return true; }
    bool isReady() const override { return readyState; }
};

#include "../../src/hal/hal_dac_adapter.cpp"

#endif // DAC_ENABLED

// ===== Test Audio Device (non-DAC test device with audio interface) =====
class TestAudioDevice : public HalAudioDevice {
public:
    bool probeResult = true;
    bool initResult = true;
    bool healthResult = true;
    int initCount = 0;

    TestAudioDevice(const char* compat, HalDeviceType type, uint16_t priority = HAL_PRIORITY_HARDWARE) {
        strncpy(_descriptor.compatible, compat, 31);
        _descriptor.type = type;
        _descriptor.channelCount = 2;
        _initPriority = priority;
    }

    bool probe() override { return probeResult; }
    bool init() override { initCount++; return initResult; }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return healthResult; }
    bool configure(uint32_t, uint8_t) override { return true; }
    bool setVolume(uint8_t) override { return true; }
    bool setMute(bool) override { return true; }
};

// ===== Test Fixtures =====

void setUp(void) {
    HalDeviceManager::instance().reset();
    // Reset pipeline bridge state
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        _pipelineOutputSlots[i] = false;
        _pipelineInputSlots[i] = false;
    }
}

void tearDown(void) {}

// ===== Tests =====

void test_pipeline_sync_registers_available_outputs(void) {
    HalDeviceManager& mgr = HalDeviceManager::instance();

    TestAudioDevice dac1("ti,pcm5102a", HAL_DEV_DAC);
    dac1._state = HAL_STATE_AVAILABLE;
    dac1._ready = true;
    mgr.registerDevice(&dac1, HAL_DISC_BUILTIN);

    TestAudioDevice dac2("evergrande,es8311", HAL_DEV_CODEC);
    dac2._state = HAL_STATE_AVAILABLE;
    dac2._ready = true;
    mgr.registerDevice(&dac2, HAL_DISC_BUILTIN);

    hal_pipeline_sync();

    TEST_ASSERT_EQUAL(2, hal_pipeline_output_count());
    // Codec also counts as input
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());
}

void test_pipeline_ignores_unavailable_devices(void) {
    HalDeviceManager& mgr = HalDeviceManager::instance();

    TestAudioDevice dac1("ti,pcm5102a", HAL_DEV_DAC);
    dac1._state = HAL_STATE_AVAILABLE;
    dac1._ready = true;
    mgr.registerDevice(&dac1, HAL_DISC_BUILTIN);

    TestAudioDevice dac2("evergrande,es8311", HAL_DEV_CODEC);
    dac2._state = HAL_STATE_ERROR;
    dac2._ready = false;
    mgr.registerDevice(&dac2, HAL_DISC_BUILTIN);

    hal_pipeline_sync();

    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

void test_pipeline_on_device_available(void) {
    HalDeviceManager& mgr = HalDeviceManager::instance();

    TestAudioDevice dac1("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr.registerDevice(&dac1, HAL_DISC_BUILTIN);
    dac1._state = HAL_STATE_AVAILABLE;
    dac1._ready = true;

    hal_pipeline_on_device_available(slot);

    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
}

void test_pipeline_on_device_removed(void) {
    HalDeviceManager& mgr = HalDeviceManager::instance();

    TestAudioDevice dac1("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr.registerDevice(&dac1, HAL_DISC_BUILTIN);
    dac1._state = HAL_STATE_AVAILABLE;
    dac1._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    hal_pipeline_on_device_removed(slot);
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

void test_pipeline_adc_counts_as_input(void) {
    HalDeviceManager& mgr = HalDeviceManager::instance();

    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    adc._state = HAL_STATE_AVAILABLE;
    adc._ready = true;
    int slot = mgr.registerDevice(&adc, HAL_DISC_BUILTIN);

    hal_pipeline_on_device_available(slot);

    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());
}

void test_pipeline_multiple_dacs_separate_slots(void) {
    HalDeviceManager& mgr = HalDeviceManager::instance();

    TestAudioDevice dac1("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice dac2("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice dac3("ti,pcm5102a", HAL_DEV_DAC);

    int s0 = mgr.registerDevice(&dac1, HAL_DISC_BUILTIN);
    int s1 = mgr.registerDevice(&dac2, HAL_DISC_EEPROM);
    int s2 = mgr.registerDevice(&dac3, HAL_DISC_MANUAL);

    // All different slots
    TEST_ASSERT_NOT_EQUAL(s0, s1);
    TEST_ASSERT_NOT_EQUAL(s1, s2);

    dac1._state = HAL_STATE_AVAILABLE; dac1._ready = true;
    dac2._state = HAL_STATE_AVAILABLE; dac2._ready = true;
    dac3._state = HAL_STATE_AVAILABLE; dac3._ready = true;

    hal_pipeline_sync();
    TEST_ASSERT_EQUAL(3, hal_pipeline_output_count());
}

#ifdef DAC_ENABLED

void test_dac_adapter_wraps_driver(void) {
    MockDacDriver driver;

    HalDeviceDescriptor desc;
    memset(&desc, 0, sizeof(desc));
    strncpy(desc.compatible, "ti,pcm5102a", 31);
    strncpy(desc.name, "PCM5102A", 32);
    desc.type = HAL_DEV_DAC;
    desc.legacyId = 0x0001;
    desc.channelCount = 2;

    HalDacAdapter adapter(&driver, desc, true);

    TEST_ASSERT_TRUE(adapter._ready);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adapter._state);
    TEST_ASSERT_EQUAL_STRING("ti,pcm5102a", adapter.getDescriptor().compatible);
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, adapter.getType());
}

void test_dac_adapter_health_delegates(void) {
    MockDacDriver driver;
    HalDeviceDescriptor desc;
    memset(&desc, 0, sizeof(desc));
    strncpy(desc.compatible, "mock,dac", 31);
    desc.type = HAL_DEV_DAC;

    HalDacAdapter adapter(&driver, desc, true);

    driver.readyState = true;
    TEST_ASSERT_TRUE(adapter.healthCheck());

    driver.readyState = false;
    TEST_ASSERT_FALSE(adapter.healthCheck());
}

void test_dac_adapter_legacy_capabilities(void) {
    MockDacDriver driver;
    HalDeviceDescriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = HAL_DEV_DAC;

    HalDacAdapter adapter(&driver, desc, true);

    const DacCapabilities* caps = adapter.getLegacyCapabilities();
    TEST_ASSERT_NOT_NULL(caps);
    TEST_ASSERT_EQUAL_STRING("MockDAC", caps->name);
    TEST_ASSERT_EQUAL(2, caps->maxChannels);
}

void test_dac_adapter_init_already_initialized(void) {
    MockDacDriver driver;
    HalDeviceDescriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = HAL_DEV_DAC;

    HalDacAdapter adapter(&driver, desc, true);

    // init() should succeed immediately since already initialized
    TEST_ASSERT_TRUE(adapter.init());
    TEST_ASSERT_TRUE(adapter._ready);
}

#endif // DAC_ENABLED

// ===== Main =====

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_pipeline_sync_registers_available_outputs);
    RUN_TEST(test_pipeline_ignores_unavailable_devices);
    RUN_TEST(test_pipeline_on_device_available);
    RUN_TEST(test_pipeline_on_device_removed);
    RUN_TEST(test_pipeline_adc_counts_as_input);
    RUN_TEST(test_pipeline_multiple_dacs_separate_slots);

#ifdef DAC_ENABLED
    RUN_TEST(test_dac_adapter_wraps_driver);
    RUN_TEST(test_dac_adapter_health_delegates);
    RUN_TEST(test_dac_adapter_legacy_capabilities);
    RUN_TEST(test_dac_adapter_init_already_initialized);
#endif

    return UNITY_END();
}
