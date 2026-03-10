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
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
#include "../../src/hal/hal_ns4150b.cpp"
#include "../../src/hal/hal_pipeline_bridge.cpp"
#include "../../src/hal/hal_settings.cpp"

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
    HalInitResult init() override {
        initCount++;
        return initResult ? hal_init_ok() : hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return healthResult; }
    bool configure(uint32_t, uint8_t) override { return true; }
    bool setVolume(uint8_t) override { return true; }
    bool setMute(bool) override { return true; }

    // buildSink — required for bridge activation (DEBT-6: bridge is sole sink owner)
    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override {
        if (_descriptor.type != HAL_DEV_DAC && _descriptor.type != HAL_DEV_CODEC)
            return false;
        *out = AUDIO_OUTPUT_SINK_INIT;
        out->name = _descriptor.compatible;
        out->firstChannel = (uint8_t)(sinkSlot * 2);
        out->channelCount = 2;
        out->halSlot = _slot;
        return true;
    }
};

// ===== Test Fixtures =====

void setUp(void) {
    HalDeviceManager::instance().reset();
    hal_pipeline_reset();
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

    // DAC -> sink slot 0, CODEC -> sink slot 1: 2 unique sink slots
    TEST_ASSERT_EQUAL(2, hal_pipeline_output_count());
    // Only HAL_DEV_ADC devices contribute to input_count().
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
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
    TestAudioDevice codec("evergrande,es8311", HAL_DEV_CODEC);

    int s0 = mgr.registerDevice(&dac1,  HAL_DISC_BUILTIN);
    int s1 = mgr.registerDevice(&dac2,  HAL_DISC_EEPROM);
    int s2 = mgr.registerDevice(&codec, HAL_DISC_MANUAL);

    // All different HAL slots
    TEST_ASSERT_NOT_EQUAL(s0, s1);
    TEST_ASSERT_NOT_EQUAL(s1, s2);

    dac1._state  = HAL_STATE_AVAILABLE; dac1._ready  = true;
    dac2._state  = HAL_STATE_AVAILABLE; dac2._ready  = true;
    codec._state = HAL_STATE_AVAILABLE; codec._ready = true;

    hal_pipeline_sync();
    // output_count() counts HAL slots that have an active sink mapping.
    // All three devices (2 DAC + 1 CODEC) are mapped, so count is 3.
    TEST_ASSERT_EQUAL(3, hal_pipeline_output_count());
}

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

    return UNITY_END();
}
