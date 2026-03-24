/**
 * test_hal_bridge.cpp
 *
 * Tests for hal_pipeline_bridge — the functional layer that maps HAL device
 * slots to audio pipeline sink slots and ADC lanes.
 *
 * Under NATIVE_TEST the bridge's appState/pipeline calls are compiled out,
 * but the mapping table updates (_halSlotToSinkSlot[], _halSlotToAdcLane[])
 * are NOT guarded and are fully testable via hal_pipeline_output_count() and
 * hal_pipeline_input_count().
 *
 * Technique: inline-includes hal_device_manager.cpp, hal_driver_registry.cpp,
 * hal_pipeline_bridge.cpp, and hal_settings.cpp — same pattern as
 * test_hal_integration.cpp.
 */

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
#include "../../src/audio_input_source.h"

// Inline the .cpp files
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
#include "../../src/hal/hal_ns4150b.cpp"
#include "../../src/hal/hal_pipeline_bridge.cpp"
#include "../../src/hal/hal_settings.cpp"

// ===== Test Device — HalDevice subclass with configurable type =====
class TestAudioDevice : public HalDevice {
public:
    bool probeResult;
    bool initResult;
    bool healthResult;
    int  initCount;

    TestAudioDevice(const char* compat, HalDeviceType type,
                    uint16_t priority = HAL_PRIORITY_HARDWARE) {
        strncpy(_descriptor.compatible, compat, 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, compat, 32);
        _descriptor.name[32] = '\0';
        _descriptor.type = type;
        _descriptor.channelCount = 2;
        _initPriority = priority;
        probeResult  = true;
        initResult   = true;
        healthResult = true;
        initCount    = 0;
    }

    void setCapabilities(uint8_t caps) { _descriptor.capabilities = caps; }

    bool probe() override { return probeResult; }
    HalInitResult init() override {
        initCount++;
        return initResult ? hal_init_ok() : hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return healthResult; }

    // buildSink — required for bridge activation (DEBT-6: bridge is sole sink owner)
    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override {
        if (_descriptor.type != HAL_DEV_DAC && _descriptor.type != HAL_DEV_CODEC)
            return false;
        *out = AUDIO_OUTPUT_SINK_INIT;
        out->name = _descriptor.name;
        out->firstChannel = (uint8_t)(sinkSlot * 2);
        out->channelCount = 2;
        out->halSlot = _slot;
        return true;
    }
};

// ===== Test Device that deliberately has NO buildSink (for failure-path tests) =====
class TestNoBuildSinkDevice : public HalDevice {
public:
    TestNoBuildSinkDevice(const char* compat, HalDeviceType type) {
        strncpy(_descriptor.compatible, compat, 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, compat, 32);
        _descriptor.name[32] = '\0';
        _descriptor.type = type;
        _descriptor.channelCount = 2;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }
    bool probe() override { return true; }
    HalInitResult init() override { return hal_init_ok(); }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return true; }
    // Intentionally NO buildSink override — inherits HalDevice::buildSink -> false
};

// ===== Test Device with buildSink() support (DEBT-6 Phase 1.4) =====
// Extends TestAudioDevice but overrides buildSink() to simulate a
// HalAudioDevice that can populate its own AudioOutputSink.
class TestBuildSinkDevice : public HalDevice {
public:
    bool probeResult;
    bool initResult;
    bool healthResult;
    bool buildSinkResult;
    int  initCount;
    int  deinitCount;
    int  buildSinkCount;

    TestBuildSinkDevice(const char* compat, HalDeviceType type,
                        uint16_t priority = HAL_PRIORITY_HARDWARE) {
        strncpy(_descriptor.compatible, compat, 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, compat, 32);
        _descriptor.name[32] = '\0';
        _descriptor.type = type;
        _descriptor.channelCount = 2;
        _descriptor.capabilities = HAL_CAP_DAC_PATH;
        _initPriority = priority;
        probeResult     = true;
        initResult      = true;
        healthResult    = true;
        buildSinkResult = true;
        initCount       = 0;
        deinitCount     = 0;
        buildSinkCount  = 0;
    }

    bool probe() override { return probeResult; }
    HalInitResult init() override {
        initCount++;
        return initResult ? hal_init_ok() : hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override { deinitCount++; }
    void dumpConfig() override {}
    bool healthCheck() override { return healthResult; }

    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override {
        buildSinkCount++;
        if (!buildSinkResult) return false;
        // Populate a minimal valid sink
        *out = AUDIO_OUTPUT_SINK_INIT;
        out->name = _descriptor.name;
        out->firstChannel = (uint8_t)(sinkSlot * 2);
        out->channelCount = 2;
        out->halSlot = _slot;
        return true;
    }
};

// ===== Multi-sink DAC test device (like ES9038PRO with 4 stereo pairs) =====
class TestMultiSinkDevice : public HalDevice {
public:
    int sinkCount;
    int deinitCount;

    TestMultiSinkDevice(const char* compat, int numSinks = 4) {
        strncpy(_descriptor.compatible, compat, 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, compat, 32);
        _descriptor.name[32] = '\0';
        _descriptor.type = HAL_DEV_DAC;
        _descriptor.channelCount = (uint8_t)(numSinks * 2);
        _descriptor.capabilities = HAL_CAP_DAC_PATH;
        _initPriority = HAL_PRIORITY_HARDWARE;
        sinkCount = numSinks;
        deinitCount = 0;
    }
    bool probe() override { return true; }
    HalInitResult init() override { return hal_init_ok(); }
    void deinit() override { deinitCount++; }
    void dumpConfig() override {}
    bool healthCheck() override { return true; }
    int getSinkCount() const override { return sinkCount; }
    bool buildSinkAt(int idx, uint8_t sinkSlot, AudioOutputSink* out) override {
        if (idx >= sinkCount) return false;
        *out = AUDIO_OUTPUT_SINK_INIT;
        out->name = _descriptor.name;
        out->firstChannel = (uint8_t)(sinkSlot * 2);
        out->channelCount = 2;
        out->halSlot = _slot;
        return true;
    }
    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override {
        return buildSinkAt(0, sinkSlot, out);
    }
};

// ===== Multi-source ADC test device (like ES9843PRO with 2 stereo pairs) =====
class TestMultiSourceDevice : public HalDevice {
public:
    AudioInputSource _sources[2];

    TestMultiSourceDevice(const char* compat) {
        strncpy(_descriptor.compatible, compat, 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, compat, 32);
        _descriptor.name[32] = '\0';
        _descriptor.type = HAL_DEV_ADC;
        _descriptor.channelCount = 4;
        _descriptor.capabilities = HAL_CAP_ADC_PATH;
        _initPriority = HAL_PRIORITY_HARDWARE;
        memset(_sources, 0, sizeof(_sources));
    }
    bool probe() override { return true; }
    HalInitResult init() override { return hal_init_ok(); }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return true; }
    int getInputSourceCount() const override { return 2; }
    const AudioInputSource* getInputSourceAt(int idx) const override {
        if (idx < 0 || idx >= 2) return nullptr;
        return &_sources[idx];
    }
    const AudioInputSource* getInputSource() const override {
        return &_sources[0];
    }
};

// ===== Fixtures =====

static HalDeviceManager* mgr;

void setUp() {
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
    hal_pipeline_reset();
    hal_pipeline_reset_mock_counters();
}

void tearDown() {}

// =====================================================================
// Group 1: DAC/CODEC available -> mapping recorded (7 tests)
// =====================================================================

// 1a: DAC available sets sink slot mapping (AUDIO_SINK_SLOT_PRIMARY = 0)
void test_dac_available_sets_sink_mapping() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    hal_pipeline_on_device_available(slot);

    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// 1b: CODEC available sets sink slot mapping (AUDIO_SINK_SLOT_ES8311 = 1)
void test_codec_available_sets_sink_mapping() {
    TestAudioDevice codec("evergrande,es8311", HAL_DEV_CODEC);
    int slot = mgr->registerDevice(&codec, HAL_DISC_BUILTIN);
    codec._state = HAL_STATE_AVAILABLE;
    codec._ready = true;

    hal_pipeline_on_device_available(slot);

    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// 1c: ADC available does NOT set sink mapping (only sets ADC lane)
void test_adc_available_does_not_set_sink_mapping() {
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    int slot = mgr->registerDevice(&adc, HAL_DISC_BUILTIN);
    adc._state = HAL_STATE_AVAILABLE;
    adc._ready = true;

    hal_pipeline_on_device_available(slot);

    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());
}

// 1d: SENSOR type is ignored — no sink or ADC lane
void test_sensor_ignored_no_mapping() {
    TestAudioDevice sensor("internal,temp", HAL_DEV_SENSOR);
    int slot = mgr->registerDevice(&sensor, HAL_DISC_BUILTIN);
    sensor._state = HAL_STATE_AVAILABLE;
    sensor._ready = true;

    hal_pipeline_on_device_available(slot);

    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// 1e: Out-of-bounds slot is silently ignored
void test_available_oob_slot_ignored() {
    hal_pipeline_on_device_available(HAL_MAX_DEVICES);
    hal_pipeline_on_device_available(HAL_MAX_DEVICES + 10);
    hal_pipeline_on_device_available(255);

    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// 1f: Null device at valid slot is silently ignored
void test_available_null_device_at_slot_ignored() {
    // Register then remove device so slot is empty
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    mgr->removeDevice(slot);

    // Now slot has no device
    hal_pipeline_on_device_available(slot);

    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

// 1g: Making a DAC available increments output count
void test_dac_available_marks_output_count() {
    TestAudioDevice dac1("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice dac2("ess,es9038", HAL_DEV_DAC);
    int s1 = mgr->registerDevice(&dac1, HAL_DISC_BUILTIN);
    int s2 = mgr->registerDevice(&dac2, HAL_DISC_EEPROM);

    dac1._state = HAL_STATE_AVAILABLE; dac1._ready = true;
    dac2._state = HAL_STATE_AVAILABLE; dac2._ready = true;

    hal_pipeline_on_device_available(s1);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    hal_pipeline_on_device_available(s2);
    TEST_ASSERT_EQUAL(2, hal_pipeline_output_count());
}

// =====================================================================
// Group 2: Device removed -> mapping cleared (6 tests)
// =====================================================================

// 2a: DAC removed clears sink mapping
void test_dac_removed_clears_sink_mapping() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    hal_pipeline_on_device_removed(slot);
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

// 2b: ADC removed does not affect sink count (only ADC lane)
void test_adc_removed_no_sink_change() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    int sd = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    int sa = mgr->registerDevice(&adc, HAL_DISC_BUILTIN);

    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;
    adc._state = HAL_STATE_AVAILABLE; adc._ready = true;

    hal_pipeline_on_device_available(sd);
    hal_pipeline_on_device_available(sa);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());

    // Remove ADC — sink count stays 1
    hal_pipeline_on_device_removed(sa);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// 2c: Remove decrements output count correctly
void test_remove_decrements_output_count() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice codec("evergrande,es8311", HAL_DEV_CODEC);
    int s1 = mgr->registerDevice(&dac,   HAL_DISC_BUILTIN);
    int s2 = mgr->registerDevice(&codec, HAL_DISC_BUILTIN);

    dac._state   = HAL_STATE_AVAILABLE; dac._ready   = true;
    codec._state = HAL_STATE_AVAILABLE; codec._ready = true;

    hal_pipeline_on_device_available(s1);
    hal_pipeline_on_device_available(s2);
    TEST_ASSERT_EQUAL(2, hal_pipeline_output_count());

    hal_pipeline_on_device_removed(s1);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
}

// 2d: Double remove does not crash and count stays 0
void test_double_remove_no_crash() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    hal_pipeline_on_device_available(slot);
    hal_pipeline_on_device_removed(slot);
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());

    // Second remove — mapping already cleared, no crash
    hal_pipeline_on_device_removed(slot);
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

// 2e: Out-of-bounds slot remove is silently ignored
void test_remove_oob_slot_ignored() {
    hal_pipeline_on_device_removed(HAL_MAX_DEVICES);
    hal_pipeline_on_device_removed(255);

    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// 2f: Removing a slot that was never registered does not change counts
void test_remove_never_registered_no_change() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    // Remove a different (unregistered-in-bridge) slot
    TestAudioDevice sensor("chip,temp", HAL_DEV_SENSOR);
    int s2 = mgr->registerDevice(&sensor, HAL_DISC_BUILTIN);
    hal_pipeline_on_device_removed(s2);

    // DAC still mapped
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
}

// =====================================================================
// Group 3: ADC HAL state (8 tests)
// =====================================================================

// 3a: PCM1808 available enables ADC lane mapping (lane 0)
void test_pcm1808_available_enables_adc_lane() {
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    int slot = mgr->registerDevice(&adc, HAL_DISC_BUILTIN);
    adc._state = HAL_STATE_AVAILABLE; adc._ready = true;

    hal_pipeline_on_device_available(slot);

    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());
}

// 3b: ADC removed clears the ADC lane mapping
void test_adc_removed_clears_lane() {
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    int slot = mgr->registerDevice(&adc, HAL_DISC_BUILTIN);
    adc._state = HAL_STATE_AVAILABLE; adc._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());

    hal_pipeline_on_device_removed(slot);
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// 3c: Second ADC gets lane 1
void test_second_adc_gets_lane_1() {
    TestAudioDevice adc1("ti,pcm1808-a", HAL_DEV_ADC);
    TestAudioDevice adc2("ti,pcm1808-b", HAL_DEV_ADC);
    int s1 = mgr->registerDevice(&adc1, HAL_DISC_BUILTIN);
    int s2 = mgr->registerDevice(&adc2, HAL_DISC_BUILTIN);

    adc1._state = HAL_STATE_AVAILABLE; adc1._ready = true;
    adc2._state = HAL_STATE_AVAILABLE; adc2._ready = true;

    hal_pipeline_on_device_available(s1);
    hal_pipeline_on_device_available(s2);

    TEST_ASSERT_EQUAL(2, hal_pipeline_input_count());
}

// 3d: Three ADCs all get lanes (limit is now AUDIO_PIPELINE_MAX_INPUTS=8)
void test_three_adcs_all_get_lanes() {
    TestAudioDevice adc1("adc1", HAL_DEV_ADC);
    TestAudioDevice adc2("adc2", HAL_DEV_ADC);
    TestAudioDevice adc3("adc3", HAL_DEV_ADC);
    int s1 = mgr->registerDevice(&adc1, HAL_DISC_BUILTIN);
    int s2 = mgr->registerDevice(&adc2, HAL_DISC_BUILTIN);
    int s3 = mgr->registerDevice(&adc3, HAL_DISC_BUILTIN);

    adc1._state = HAL_STATE_AVAILABLE; adc1._ready = true;
    adc2._state = HAL_STATE_AVAILABLE; adc2._ready = true;
    adc3._state = HAL_STATE_AVAILABLE; adc3._ready = true;

    hal_pipeline_on_device_available(s1);
    hal_pipeline_on_device_available(s2);
    hal_pipeline_on_device_available(s3);

    // All 3 ADCs get lanes (limit is 8, not 2)
    TEST_ASSERT_EQUAL(3, hal_pipeline_input_count());
}

// 3e: Non-ADC device does not touch ADC lanes
void test_non_adc_no_adc_lane() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice codec("evergrande,es8311", HAL_DEV_CODEC);
    TestAudioDevice sensor("chip,temp", HAL_DEV_SENSOR);
    int s1 = mgr->registerDevice(&dac,    HAL_DISC_BUILTIN);
    int s2 = mgr->registerDevice(&codec,  HAL_DISC_BUILTIN);
    int s3 = mgr->registerDevice(&sensor, HAL_DISC_BUILTIN);

    dac._state    = HAL_STATE_AVAILABLE; dac._ready    = true;
    codec._state  = HAL_STATE_AVAILABLE; codec._ready  = true;
    sensor._state = HAL_STATE_AVAILABLE; sensor._ready = true;

    hal_pipeline_on_device_available(s1);
    hal_pipeline_on_device_available(s2);
    hal_pipeline_on_device_available(s3);

    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// 3f: Input and output counts are independent
void test_input_output_counts_independent() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    int sd = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    int sa = mgr->registerDevice(&adc, HAL_DISC_BUILTIN);

    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;
    adc._state = HAL_STATE_AVAILABLE; adc._ready = true;

    hal_pipeline_on_device_available(sd);
    hal_pipeline_on_device_available(sa);

    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());
}

// 3g: AMP type does not register as ADC
void test_amp_not_adc() {
    TestAudioDevice amp("ns,ns4150b", HAL_DEV_AMP);
    int slot = mgr->registerDevice(&amp, HAL_DISC_BUILTIN);
    amp._state = HAL_STATE_AVAILABLE; amp._ready = true;

    hal_pipeline_on_device_available(slot);

    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

// 3h: ADC count matches after add and remove
void test_adc_counts_after_add_remove() {
    TestAudioDevice adc1("adc1", HAL_DEV_ADC);
    TestAudioDevice adc2("adc2", HAL_DEV_ADC);
    int s1 = mgr->registerDevice(&adc1, HAL_DISC_BUILTIN);
    int s2 = mgr->registerDevice(&adc2, HAL_DISC_BUILTIN);

    adc1._state = HAL_STATE_AVAILABLE; adc1._ready = true;
    adc2._state = HAL_STATE_AVAILABLE; adc2._ready = true;

    hal_pipeline_on_device_available(s1);
    hal_pipeline_on_device_available(s2);
    TEST_ASSERT_EQUAL(2, hal_pipeline_input_count());

    hal_pipeline_on_device_removed(s1);
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());

    hal_pipeline_on_device_removed(s2);
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// =====================================================================
// Group 4: Hybrid transient policy (8 tests)
// =====================================================================

// 4a: UNAVAILABLE does NOT clear the sink mapping
void test_unavailable_preserves_sink_mapping() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    hal_pipeline_on_device_unavailable(slot);
    // Mapping preserved — still counts as output
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
}

// 4b: UNAVAILABLE does NOT clear the ADC lane mapping
void test_unavailable_preserves_adc_mapping() {
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    int slot = mgr->registerDevice(&adc, HAL_DISC_BUILTIN);
    adc._state = HAL_STATE_AVAILABLE; adc._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());

    hal_pipeline_on_device_unavailable(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());
}

// 4c: MANUAL (via state_change dispatcher) clears mapping
void test_manual_state_clears_mapping() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    hal_pipeline_state_change(slot, HAL_STATE_AVAILABLE, HAL_STATE_DISABLED);
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

// 4d: ERROR (via state_change dispatcher) clears mapping
void test_error_state_clears_mapping() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    hal_pipeline_state_change(slot, HAL_STATE_AVAILABLE, HAL_STATE_ERROR);
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

// 4e: REMOVED (via state_change dispatcher) clears mapping
void test_removed_state_clears_mapping() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    hal_pipeline_state_change(slot, HAL_STATE_AVAILABLE, HAL_STATE_REMOVED);
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

// 4f: Unavailable -> Available keeps the mapping (no re-register needed)
void test_unavailable_to_available_keeps_mapping() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    // Transient unavailable — mapping preserved
    hal_pipeline_on_device_unavailable(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    // Back to available — still 1 (mapping already existed, overwritten in-place)
    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
}

// 4g: Manual -> Available re-registers the mapping after clear
void test_manual_to_available_reregisters_mapping() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    // DISABLED clears the mapping
    hal_pipeline_state_change(slot, HAL_STATE_AVAILABLE, HAL_STATE_DISABLED);
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());

    // Re-available restores it
    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
}

// 4h: CONFIGURING state does not trigger any bridge action
void test_configuring_state_no_bridge_action() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);

    hal_pipeline_state_change(slot, HAL_STATE_UNKNOWN, HAL_STATE_CONFIGURING);

    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// =====================================================================
// Group 5: Boot sync (4 tests)
// =====================================================================

// 5a: sync registers an AVAILABLE DAC
void test_sync_registers_available_dac() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    hal_pipeline_sync();

    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
}

// 5b: sync skips ERROR devices
void test_sync_skips_error_devices() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_ERROR;
    dac._ready = false;

    hal_pipeline_sync();

    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

// 5c: sync handles mixed states (some AVAILABLE, some not)
void test_sync_mixed_states() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice codec("evergrande,es8311", HAL_DEV_CODEC);
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    TestAudioDevice sensor("chip,temp", HAL_DEV_SENSOR);

    mgr->registerDevice(&dac,    HAL_DISC_BUILTIN);
    mgr->registerDevice(&codec,  HAL_DISC_BUILTIN);
    mgr->registerDevice(&adc,    HAL_DISC_BUILTIN);
    mgr->registerDevice(&sensor, HAL_DISC_BUILTIN);

    dac._state    = HAL_STATE_AVAILABLE;   dac._ready    = true;
    codec._state  = HAL_STATE_ERROR;       codec._ready  = false;
    adc._state    = HAL_STATE_AVAILABLE;   adc._ready    = true;
    sensor._state = HAL_STATE_UNAVAILABLE; sensor._ready = false;

    hal_pipeline_sync();

    // DAC is AVAILABLE => 1 output; CODEC ERROR => skipped; ADC AVAILABLE => 1 input
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());
}

// 5d: sync properly counts multiple AVAILABLE outputs
void test_sync_marks_counts() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice codec("evergrande,es8311", HAL_DEV_CODEC);
    TestAudioDevice adc1("adc1", HAL_DEV_ADC);
    TestAudioDevice adc2("adc2", HAL_DEV_ADC);

    mgr->registerDevice(&dac,   HAL_DISC_BUILTIN);
    mgr->registerDevice(&codec, HAL_DISC_BUILTIN);
    mgr->registerDevice(&adc1,  HAL_DISC_BUILTIN);
    mgr->registerDevice(&adc2,  HAL_DISC_BUILTIN);

    dac._state   = HAL_STATE_AVAILABLE; dac._ready   = true;
    codec._state = HAL_STATE_AVAILABLE; codec._ready = true;
    adc1._state  = HAL_STATE_AVAILABLE; adc1._ready  = true;
    adc2._state  = HAL_STATE_AVAILABLE; adc2._ready  = true;

    hal_pipeline_sync();

    TEST_ASSERT_EQUAL(2, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(2, hal_pipeline_input_count());
}

// =====================================================================
// Group 5e: Boot sync edge cases (6 tests)
// =====================================================================

// 5e: sync with already-AVAILABLE devices registers sinks for each
void test_sync_registers_all_available_devices() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice codec("evergrande,es8311", HAL_DEV_CODEC);
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);

    mgr->registerDevice(&dac,   HAL_DISC_BUILTIN);
    mgr->registerDevice(&codec, HAL_DISC_BUILTIN);
    mgr->registerDevice(&adc,   HAL_DISC_BUILTIN);

    // All set to AVAILABLE before sync (simulates boot-time init completing
    // before the bridge is wired up)
    dac._state   = HAL_STATE_AVAILABLE; dac._ready   = true;
    codec._state = HAL_STATE_AVAILABLE; codec._ready = true;
    adc._state   = HAL_STATE_AVAILABLE; adc._ready   = true;

    // No mappings before sync
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());

    hal_pipeline_sync();

    // DAC + CODEC are output devices, ADC is an input device
    TEST_ASSERT_EQUAL(2, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());

    // Forward lookups confirm each device got a valid mapping
    TEST_ASSERT_TRUE(hal_pipeline_get_sink_slot(0) >= 0);
    TEST_ASSERT_TRUE(hal_pipeline_get_sink_slot(1) >= 0);
    TEST_ASSERT_TRUE(hal_pipeline_get_input_lane(2) >= 0);
}

// 5f: Double sync does not create duplicate sink mappings
void test_sync_double_call_no_duplicates() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);

    mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    mgr->registerDevice(&adc, HAL_DISC_BUILTIN);

    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;
    adc._state = HAL_STATE_AVAILABLE; adc._ready = true;

    hal_pipeline_sync();
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());

    // Second sync -- clears tables internally then re-scans, so counts
    // should remain exactly the same, not double
    hal_pipeline_sync();
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());
}

// 5g: sync with mixed AVAILABLE/ERROR/REMOVED -- only AVAILABLE gets sinks
void test_sync_mixed_available_error_removed() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice codec("evergrande,es8311", HAL_DEV_CODEC);
    TestAudioDevice adc1("adc1", HAL_DEV_ADC);
    TestAudioDevice adc2("adc2", HAL_DEV_ADC);
    TestAudioDevice sensor("chip,temp", HAL_DEV_SENSOR);

    mgr->registerDevice(&dac,    HAL_DISC_BUILTIN);
    mgr->registerDevice(&codec,  HAL_DISC_BUILTIN);
    mgr->registerDevice(&adc1,   HAL_DISC_BUILTIN);
    mgr->registerDevice(&adc2,   HAL_DISC_BUILTIN);
    mgr->registerDevice(&sensor, HAL_DISC_BUILTIN);

    dac._state    = HAL_STATE_AVAILABLE; dac._ready    = true;   // should register
    codec._state  = HAL_STATE_ERROR;     codec._ready  = false;  // skipped
    adc1._state   = HAL_STATE_REMOVED;   adc1._ready   = false;  // skipped
    adc2._state   = HAL_STATE_AVAILABLE; adc2._ready   = true;   // should register
    sensor._state = HAL_STATE_DISABLED;  sensor._ready  = false;  // skipped

    hal_pipeline_sync();

    // Only DAC (output) and ADC2 (input) should be mapped
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());
}

// 5h: sync uses stack-local counting (Phase 1a fix) -- no heap allocation.
//     The Phase 1a fix replaced heap-allocated count tracking with
//     stack-local int[2] in the forEach lambda. This test confirms
//     correctness by running sync many times and checking deterministic
//     output -- if there were a leak or stale pointer, repeated runs
//     would eventually corrupt state.
void test_sync_no_heap_allocation_phase_1a() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);

    mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    mgr->registerDevice(&adc, HAL_DISC_BUILTIN);

    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;
    adc._state = HAL_STATE_AVAILABLE; adc._ready = true;

    // Run sync many times -- stack-local counting means each call is
    // self-contained. sync() clears mapping tables first, so the count
    // must be stable across every iteration.
    for (int i = 0; i < 50; i++) {
        hal_pipeline_sync();
    }

    // Counts must remain correct after many iterations
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());
}

// 5i: sync with empty device manager is a safe no-op
void test_sync_empty_device_manager_no_crash() {
    // No devices registered -- setUp() already reset the manager
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());

    hal_pipeline_sync();

    // Still zero -- no crash, no spurious mappings
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// 5j: sync after device removal -- removed device does not get a sink
void test_sync_after_device_removal_no_sink() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice codec("evergrande,es8311", HAL_DEV_CODEC);

    int sd = mgr->registerDevice(&dac,   HAL_DISC_BUILTIN);
    mgr->registerDevice(&codec, HAL_DISC_BUILTIN);

    dac._state   = HAL_STATE_AVAILABLE; dac._ready   = true;
    codec._state = HAL_STATE_AVAILABLE; codec._ready = true;

    // Remove the DAC between registration and sync -- simulates a device
    // that was detected at boot but then physically disconnected before
    // the bridge runs its initial scan
    mgr->removeDevice(sd);

    hal_pipeline_sync();

    // Only the CODEC should be mapped -- the removed DAC slot is null
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// =====================================================================
// Group 6: Count helpers + reset (3 tests)
// =====================================================================

// 6a: Counts reflect add/remove correctly across mixed types
void test_counts_add_remove() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice codec("evergrande,es8311", HAL_DEV_CODEC);
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    int sd = mgr->registerDevice(&dac,   HAL_DISC_BUILTIN);
    int sc = mgr->registerDevice(&codec, HAL_DISC_BUILTIN);
    int sa = mgr->registerDevice(&adc,   HAL_DISC_BUILTIN);

    dac._state   = HAL_STATE_AVAILABLE; dac._ready   = true;
    codec._state = HAL_STATE_AVAILABLE; codec._ready = true;
    adc._state   = HAL_STATE_AVAILABLE; adc._ready   = true;

    hal_pipeline_on_device_available(sd);
    hal_pipeline_on_device_available(sc);
    hal_pipeline_on_device_available(sa);
    TEST_ASSERT_EQUAL(2, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());

    hal_pipeline_on_device_removed(sc);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());

    hal_pipeline_on_device_removed(sa);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// 6b: reset() zeros all counts
void test_reset_zeros_all() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    int sd = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    int sa = mgr->registerDevice(&adc, HAL_DISC_BUILTIN);

    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;
    adc._state = HAL_STATE_AVAILABLE; adc._ready = true;

    hal_pipeline_on_device_available(sd);
    hal_pipeline_on_device_available(sa);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());

    hal_pipeline_reset();
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// 6c: After reset, re-adding works cleanly
void test_reset_then_readd() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    hal_pipeline_reset();
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());

    // Re-add after reset
    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
}

// =====================================================================
// Group 7: Dynamic ordinal slot assignment (6 tests)
// =====================================================================

// 7a: Two DACs get sequential sink slots (0, 1)
void test_two_dacs_get_sequential_slots() {
    TestAudioDevice dac1("ti,pcm5102a", HAL_DEV_DAC);
    TestAudioDevice dac2("ess,es9038q2m", HAL_DEV_DAC);
    int s1 = mgr->registerDevice(&dac1, HAL_DISC_BUILTIN);
    int s2 = mgr->registerDevice(&dac2, HAL_DISC_EEPROM);

    dac1._state = HAL_STATE_AVAILABLE; dac1._ready = true;
    dac2._state = HAL_STATE_AVAILABLE; dac2._ready = true;

    hal_pipeline_on_device_available(s1);
    hal_pipeline_on_device_available(s2);

    TEST_ASSERT_EQUAL(0, hal_pipeline_get_sink_slot(s1));
    TEST_ASSERT_EQUAL(1, hal_pipeline_get_sink_slot(s2));
}

// 7b: Removing a DAC frees its slot for reuse
void test_removed_sink_slot_reused() {
    TestAudioDevice dac1("dac1", HAL_DEV_DAC);
    TestAudioDevice dac2("dac2", HAL_DEV_DAC);
    TestAudioDevice dac3("dac3", HAL_DEV_DAC);
    int s1 = mgr->registerDevice(&dac1, HAL_DISC_BUILTIN);
    int s2 = mgr->registerDevice(&dac2, HAL_DISC_BUILTIN);
    int s3 = mgr->registerDevice(&dac3, HAL_DISC_BUILTIN);

    dac1._state = HAL_STATE_AVAILABLE; dac1._ready = true;
    dac2._state = HAL_STATE_AVAILABLE; dac2._ready = true;
    dac3._state = HAL_STATE_AVAILABLE; dac3._ready = true;

    hal_pipeline_on_device_available(s1);  // slot 0
    hal_pipeline_on_device_available(s2);  // slot 1
    hal_pipeline_on_device_removed(s1);    // free slot 0

    hal_pipeline_on_device_available(s3);  // should get slot 0 (first free)
    TEST_ASSERT_EQUAL(0, hal_pipeline_get_sink_slot(s3));
    TEST_ASSERT_EQUAL(1, hal_pipeline_get_sink_slot(s2));
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(s1));
}

// 7c: ADCs get sequential input lanes
void test_adcs_get_sequential_lanes() {
    TestAudioDevice adc1("adc1", HAL_DEV_ADC);
    TestAudioDevice adc2("adc2", HAL_DEV_ADC);
    TestAudioDevice adc3("adc3", HAL_DEV_ADC);
    int s1 = mgr->registerDevice(&adc1, HAL_DISC_BUILTIN);
    int s2 = mgr->registerDevice(&adc2, HAL_DISC_BUILTIN);
    int s3 = mgr->registerDevice(&adc3, HAL_DISC_BUILTIN);

    adc1._state = HAL_STATE_AVAILABLE; adc1._ready = true;
    adc2._state = HAL_STATE_AVAILABLE; adc2._ready = true;
    adc3._state = HAL_STATE_AVAILABLE; adc3._ready = true;

    hal_pipeline_on_device_available(s1);
    hal_pipeline_on_device_available(s2);
    hal_pipeline_on_device_available(s3);

    TEST_ASSERT_EQUAL(0, hal_pipeline_get_input_lane(s1));
    TEST_ASSERT_EQUAL(1, hal_pipeline_get_input_lane(s2));
    TEST_ASSERT_EQUAL(2, hal_pipeline_get_input_lane(s3));
}

// 7d: Input lanes capped at AUDIO_PIPELINE_MAX_INPUTS (8)
void test_input_lanes_capped_at_max() {
    TestAudioDevice adcs[9] = {
        TestAudioDevice("a0", HAL_DEV_ADC),
        TestAudioDevice("a1", HAL_DEV_ADC),
        TestAudioDevice("a2", HAL_DEV_ADC),
        TestAudioDevice("a3", HAL_DEV_ADC),
        TestAudioDevice("a4", HAL_DEV_ADC),
        TestAudioDevice("a5", HAL_DEV_ADC),
        TestAudioDevice("a6", HAL_DEV_ADC),
        TestAudioDevice("a7", HAL_DEV_ADC),
        TestAudioDevice("a8", HAL_DEV_ADC),
    };
    int slots[9];
    for (int i = 0; i < 9; i++) {
        slots[i] = mgr->registerDevice(&adcs[i], HAL_DISC_MANUAL);
        adcs[i]._state = HAL_STATE_AVAILABLE;
        adcs[i]._ready = true;
        hal_pipeline_on_device_available(slots[i]);
    }

    // AUDIO_PIPELINE_MAX_INPUTS = 8; 9th ADC is rejected
    TEST_ASSERT_EQUAL(8, hal_pipeline_input_count());
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_input_lane(slots[8]));
}

// 7e: Device with explicit HAL_CAP_ADC_PATH | HAL_CAP_DAC_PATH gets both
void test_explicit_caps_dual_path() {
    TestAudioDevice codec("custom,codec", HAL_DEV_CODEC);
    codec.setCapabilities(HAL_CAP_ADC_PATH | HAL_CAP_DAC_PATH);
    int slot = mgr->registerDevice(&codec, HAL_DISC_MANUAL);
    codec._state = HAL_STATE_AVAILABLE;
    codec._ready = true;

    hal_pipeline_on_device_available(slot);

    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(1, hal_pipeline_input_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_get_sink_slot(slot));
    TEST_ASSERT_EQUAL(0, hal_pipeline_get_input_lane(slot));
}

// 7f: CODEC without explicit caps only gets output (conservative inference)
void test_codec_default_caps_output_only() {
    TestAudioDevice codec("evergrande,es8311", HAL_DEV_CODEC);
    // No explicit capabilities set — defaults to 0
    int slot = mgr->registerDevice(&codec, HAL_DISC_BUILTIN);
    codec._state = HAL_STATE_AVAILABLE;
    codec._ready = true;

    hal_pipeline_on_device_available(slot);

    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

// =====================================================================
// Group 8: Forward-lookup API (4 tests)
// =====================================================================

// 8a: Forward lookup returns correct sink slot
void test_forward_lookup_sink_slot() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(slot));  // Not yet mapped
    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(0, hal_pipeline_get_sink_slot(slot));   // Now mapped
}

// 8b: Forward lookup returns correct input lane
void test_forward_lookup_input_lane() {
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    int slot = mgr->registerDevice(&adc, HAL_DISC_BUILTIN);
    adc._state = HAL_STATE_AVAILABLE; adc._ready = true;

    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_input_lane(slot));
    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(0, hal_pipeline_get_input_lane(slot));
}

// 8c: Forward lookup returns -1 for out-of-bounds
void test_forward_lookup_oob() {
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(HAL_MAX_DEVICES));
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_input_lane(HAL_MAX_DEVICES));
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(255));
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_input_lane(255));
}

// 8d: Forward lookup returns -1 after device removed
void test_forward_lookup_after_remove() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE; dac._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(0, hal_pipeline_get_sink_slot(slot));

    hal_pipeline_on_device_removed(slot);
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(slot));
}

// =====================================================================
// Group 9: Bridge sink ownership — activate/deactivate (DEBT-6 Phase 1.4)
// =====================================================================

// 9a: activate_device calls buildSink and audio_pipeline_set_sink
void test_bridge_activate_calls_buildSink_and_set_sink() {
    TestBuildSinkDevice dac("test,dac-bs", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    hal_pipeline_activate_device(slot);

    // buildSink should have been called once
    TEST_ASSERT_EQUAL(1, dac.buildSinkCount);
    // audio_pipeline_set_sink should have been called once
    TEST_ASSERT_EQUAL(1, _mock_set_sink_count);
    // Mapping should be set
    TEST_ASSERT_EQUAL(0, hal_pipeline_get_sink_slot(slot));
    // Only buildSink path exists -- no legacy fallback
}

// 9b: activate_device fails when buildSink returns false (no legacy fallback)
void test_bridge_activate_fails_without_buildSink() {
    // TestNoBuildSinkDevice does NOT override buildSink (inherits HalDevice::buildSink -> false)
    TestNoBuildSinkDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    hal_pipeline_activate_device(slot);

    // audio_pipeline_set_sink should NOT have been called (buildSink returned false)
    TEST_ASSERT_EQUAL(0, _mock_set_sink_count);
    // Mapping should be cleared on failure
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(slot));
}

// 9c: deactivate_device removes sink and clears mapping
void test_bridge_deactivate_removes_sink() {
    TestBuildSinkDevice dac("test,dac-bs", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    hal_pipeline_activate_device(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    // Reset counters after activate
    _mock_remove_sink_count = 0;

    hal_pipeline_deactivate_device(slot);

    // audio_pipeline_remove_sink should have been called
    TEST_ASSERT_EQUAL(1, _mock_remove_sink_count);
    // Mapping should be cleared
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(slot));
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

// 9d: deactivate_device calls device deinit (HC-3: device owns TX teardown)
void test_bridge_deactivate_calls_deinit() {
    TestBuildSinkDevice dac("test,dac-bs", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    hal_pipeline_activate_device(slot);
    TEST_ASSERT_EQUAL(0, dac.deinitCount);

    hal_pipeline_deactivate_device(slot);
    TEST_ASSERT_EQUAL(1, dac.deinitCount);
}

// 9e: activate_device is idempotent (HC-5: same slot on repeated calls)
void test_bridge_activate_idempotent() {
    TestBuildSinkDevice dac("test,dac-bs", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    hal_pipeline_activate_device(slot);
    int8_t firstSinkSlot = hal_pipeline_get_sink_slot(slot);
    TEST_ASSERT_EQUAL(0, firstSinkSlot);
    TEST_ASSERT_EQUAL(1, dac.buildSinkCount);

    // Reset set_sink counter
    _mock_set_sink_count = 0;

    // Second activate — should get same sink slot (idempotent mapping)
    hal_pipeline_activate_device(slot);
    int8_t secondSinkSlot = hal_pipeline_get_sink_slot(slot);
    TEST_ASSERT_EQUAL(firstSinkSlot, secondSinkSlot);
    // buildSink called again (re-register is allowed, same slot)
    TEST_ASSERT_EQUAL(2, dac.buildSinkCount);
    // set_sink called for the re-register
    TEST_ASSERT_EQUAL(1, _mock_set_sink_count);
    // Output count stays 1
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
}

// 9f: activate_device ignores non-DAC-path devices
void test_bridge_activate_ignores_non_dac() {
    TestAudioDevice adc("ti,pcm1808", HAL_DEV_ADC);
    int slot = mgr->registerDevice(&adc, HAL_DISC_BUILTIN);
    adc._state = HAL_STATE_AVAILABLE;
    adc._ready = true;

    hal_pipeline_activate_device(slot);

    // No sink registration should occur for ADC device
    TEST_ASSERT_EQUAL(0, _mock_set_sink_count);
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(slot));
}

// 9g: deactivate_device is safe on unmapped slot
void test_bridge_deactivate_unmapped_slot_safe() {
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);

    // No activate — slot is unmapped
    hal_pipeline_deactivate_device(slot);

    // Should be a no-op (no crash, no remove_sink call)
    TEST_ASSERT_EQUAL(0, _mock_remove_sink_count);
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

// 9h: activate then deactivate then activate restores mapping
void test_bridge_activate_deactivate_reactivate() {
    TestBuildSinkDevice dac("test,dac-bs", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    hal_pipeline_activate_device(slot);
    TEST_ASSERT_EQUAL(0, hal_pipeline_get_sink_slot(slot));

    hal_pipeline_deactivate_device(slot);
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(slot));

    // Re-activate — should get a slot again (slot 0 is now free)
    hal_pipeline_activate_device(slot);
    TEST_ASSERT_EQUAL(0, hal_pipeline_get_sink_slot(slot));
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
}

// 9i: on_device_available uses activate path (integration)
void test_on_device_available_uses_activate_path() {
    TestBuildSinkDevice dac("test,dac-bs", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    hal_pipeline_on_device_available(slot);

    // buildSink should have been called via the activate path
    TEST_ASSERT_EQUAL(1, dac.buildSinkCount);
    // audio_pipeline_set_sink should have been called
    TEST_ASSERT_EQUAL(1, _mock_set_sink_count);
    // Mapping should be set
    TEST_ASSERT_EQUAL(0, hal_pipeline_get_sink_slot(slot));
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
}

// 9j: on_device_removed uses deactivate path (integration)
void test_on_device_removed_uses_deactivate_path() {
    TestBuildSinkDevice dac("test,dac-bs", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    hal_pipeline_on_device_available(slot);
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());

    _mock_remove_sink_count = 0;
    hal_pipeline_on_device_removed(slot);

    // audio_pipeline_remove_sink should have been called via deactivate
    TEST_ASSERT_EQUAL(1, _mock_remove_sink_count);
    // deinit should have been called
    TEST_ASSERT_EQUAL(1, dac.deinitCount);
    // Mapping should be cleared
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(slot));
    TEST_ASSERT_EQUAL(0, hal_pipeline_output_count());
}

// 9k: buildSink failure clears mapping (no legacy fallback)
void test_bridge_activate_buildSink_failure_clears_mapping() {
    TestBuildSinkDevice dac("test,dac-bs", HAL_DEV_DAC);
    dac.buildSinkResult = false;  // buildSink will fail
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    hal_pipeline_activate_device(slot);

    // buildSink was called but returned false
    TEST_ASSERT_EQUAL(1, dac.buildSinkCount);
    // audio_pipeline_set_sink should NOT have been called
    TEST_ASSERT_EQUAL(0, _mock_set_sink_count);
    // Mapping should be cleared on failure
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(slot));
}

// ===== Group 10: Multi-sink/source edge cases =====

void test_fragmented_multi_sink_reuse(void) {
    // Register 3 x 4-sink devices, filling slots [0-3], [4-7], [8-11]
    TestMultiSinkDevice dac0("dac-quad-0");
    TestMultiSinkDevice dac1("dac-quad-1");
    TestMultiSinkDevice dac2("dac-quad-2");

    int slot0 = mgr->registerDevice(&dac0, HAL_DISC_BUILTIN);
    int slot1 = mgr->registerDevice(&dac1, HAL_DISC_BUILTIN);
    int slot2 = mgr->registerDevice(&dac2, HAL_DISC_BUILTIN);

    dac0._state = HAL_STATE_AVAILABLE;
    dac1._state = HAL_STATE_AVAILABLE;
    dac2._state = HAL_STATE_AVAILABLE;
    hal_pipeline_activate_device(slot0);
    hal_pipeline_activate_device(slot1);
    hal_pipeline_activate_device(slot2);

    TEST_ASSERT_EQUAL(12, hal_pipeline_output_count());  // 3 * 4
    int8_t sinkSlot1 = hal_pipeline_get_sink_slot(slot1);
    TEST_ASSERT_EQUAL(4, sinkSlot1);  // Middle device starts at slot 4

    // Remove middle device
    hal_pipeline_deactivate_device(slot1);
    TEST_ASSERT_EQUAL(8, hal_pipeline_output_count());  // 2 * 4

    // Register new 4-sink device — should reuse freed slots [4-7]
    TestMultiSinkDevice dac3("dac-quad-3");
    int slot3 = mgr->registerDevice(&dac3, HAL_DISC_BUILTIN);
    dac3._state = HAL_STATE_AVAILABLE;
    hal_pipeline_activate_device(slot3);

    int8_t newSlot = hal_pipeline_get_sink_slot(slot3);
    TEST_ASSERT_EQUAL(4, newSlot);  // Reuses first free consecutive run
    TEST_ASSERT_EQUAL(12, hal_pipeline_output_count());
}

void test_sink_overflow_at_capacity(void) {
    // Fill all 16 sink slots with 4 x 4-sink devices
    TestMultiSinkDevice dacs[4] = {
        TestMultiSinkDevice("dac-fill-0"),
        TestMultiSinkDevice("dac-fill-1"),
        TestMultiSinkDevice("dac-fill-2"),
        TestMultiSinkDevice("dac-fill-3")
    };

    for (int i = 0; i < 4; i++) {
        int s = mgr->registerDevice(&dacs[i], HAL_DISC_BUILTIN);
        dacs[i]._state = HAL_STATE_AVAILABLE;
        hal_pipeline_activate_device(s);
    }
    TEST_ASSERT_EQUAL(16, hal_pipeline_output_count());

    // 5th device should fail — no room
    TestMultiSinkDevice dacOverflow("dac-overflow");
    int overflowSlot = mgr->registerDevice(&dacOverflow, HAL_DISC_BUILTIN);
    dacOverflow._state = HAL_STATE_AVAILABLE;
    hal_pipeline_activate_device(overflowSlot);
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_sink_slot(overflowSlot));
    TEST_ASSERT_EQUAL(16, hal_pipeline_output_count());  // Unchanged
}

void test_simultaneous_multi_sink_and_multi_source(void) {
    // 2 x 4-sink DACs + 2 x 2-source ADCs simultaneously
    TestMultiSinkDevice dacA("dac-multi-a");
    TestMultiSinkDevice dacB("dac-multi-b");
    TestMultiSourceDevice adcA("adc-multi-a");
    TestMultiSourceDevice adcB("adc-multi-b");

    int sA = mgr->registerDevice(&dacA, HAL_DISC_BUILTIN);
    int sB = mgr->registerDevice(&dacB, HAL_DISC_BUILTIN);
    int sC = mgr->registerDevice(&adcA, HAL_DISC_BUILTIN);
    int sD = mgr->registerDevice(&adcB, HAL_DISC_BUILTIN);

    dacA._state = HAL_STATE_AVAILABLE;
    dacB._state = HAL_STATE_AVAILABLE;
    adcA._state = HAL_STATE_AVAILABLE;
    adcB._state = HAL_STATE_AVAILABLE;

    hal_pipeline_activate_device(sA);
    hal_pipeline_activate_device(sB);
    hal_pipeline_on_device_available(sC);
    hal_pipeline_on_device_available(sD);

    // 8 sink slots (2 * 4)
    TEST_ASSERT_EQUAL(8, hal_pipeline_output_count());
    // 4 input lanes (2 * 2)
    TEST_ASSERT_EQUAL(4, hal_pipeline_input_count());

    // Verify no overlap — DAC slots and ADC lanes assigned independently
    int8_t dacASlot = hal_pipeline_get_sink_slot(sA);
    int8_t dacBSlot = hal_pipeline_get_sink_slot(sB);
    int8_t adcALane = hal_pipeline_get_input_lane(sC);
    int8_t adcBLane = hal_pipeline_get_input_lane(sD);

    TEST_ASSERT_GREATER_OR_EQUAL(0, dacASlot);
    TEST_ASSERT_GREATER_OR_EQUAL(0, dacBSlot);
    TEST_ASSERT_GREATER_OR_EQUAL(0, adcALane);
    TEST_ASSERT_GREATER_OR_EQUAL(0, adcBLane);
    // DAC slots should be separate
    TEST_ASSERT_NOT_EQUAL(dacASlot, dacBSlot);
    // ADC lanes should be separate
    TEST_ASSERT_NOT_EQUAL(adcALane, adcBLane);
}

// ===== Test Runner =====
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    // Group 1: DAC/CODEC available -> mapping recorded
    RUN_TEST(test_dac_available_sets_sink_mapping);
    RUN_TEST(test_codec_available_sets_sink_mapping);
    RUN_TEST(test_adc_available_does_not_set_sink_mapping);
    RUN_TEST(test_sensor_ignored_no_mapping);
    RUN_TEST(test_available_oob_slot_ignored);
    RUN_TEST(test_available_null_device_at_slot_ignored);
    RUN_TEST(test_dac_available_marks_output_count);

    // Group 2: Device removed -> mapping cleared
    RUN_TEST(test_dac_removed_clears_sink_mapping);
    RUN_TEST(test_adc_removed_no_sink_change);
    RUN_TEST(test_remove_decrements_output_count);
    RUN_TEST(test_double_remove_no_crash);
    RUN_TEST(test_remove_oob_slot_ignored);
    RUN_TEST(test_remove_never_registered_no_change);

    // Group 3: ADC HAL state
    RUN_TEST(test_pcm1808_available_enables_adc_lane);
    RUN_TEST(test_adc_removed_clears_lane);
    RUN_TEST(test_second_adc_gets_lane_1);
    RUN_TEST(test_three_adcs_all_get_lanes);
    RUN_TEST(test_non_adc_no_adc_lane);
    RUN_TEST(test_input_output_counts_independent);
    RUN_TEST(test_amp_not_adc);
    RUN_TEST(test_adc_counts_after_add_remove);

    // Group 4: Hybrid transient policy
    RUN_TEST(test_unavailable_preserves_sink_mapping);
    RUN_TEST(test_unavailable_preserves_adc_mapping);
    RUN_TEST(test_manual_state_clears_mapping);
    RUN_TEST(test_error_state_clears_mapping);
    RUN_TEST(test_removed_state_clears_mapping);
    RUN_TEST(test_unavailable_to_available_keeps_mapping);
    RUN_TEST(test_manual_to_available_reregisters_mapping);
    RUN_TEST(test_configuring_state_no_bridge_action);

    // Group 5: Boot sync
    RUN_TEST(test_sync_registers_available_dac);
    RUN_TEST(test_sync_skips_error_devices);
    RUN_TEST(test_sync_mixed_states);
    RUN_TEST(test_sync_marks_counts);

    // Group 5e: Boot sync edge cases
    RUN_TEST(test_sync_registers_all_available_devices);
    RUN_TEST(test_sync_double_call_no_duplicates);
    RUN_TEST(test_sync_mixed_available_error_removed);
    RUN_TEST(test_sync_no_heap_allocation_phase_1a);
    RUN_TEST(test_sync_empty_device_manager_no_crash);
    RUN_TEST(test_sync_after_device_removal_no_sink);

    // Group 6: Count helpers + reset
    RUN_TEST(test_counts_add_remove);
    RUN_TEST(test_reset_zeros_all);
    RUN_TEST(test_reset_then_readd);

    // Group 7: Dynamic ordinal slot assignment
    RUN_TEST(test_two_dacs_get_sequential_slots);
    RUN_TEST(test_removed_sink_slot_reused);
    RUN_TEST(test_adcs_get_sequential_lanes);
    RUN_TEST(test_input_lanes_capped_at_max);
    RUN_TEST(test_explicit_caps_dual_path);
    RUN_TEST(test_codec_default_caps_output_only);

    // Group 8: Forward-lookup API
    RUN_TEST(test_forward_lookup_sink_slot);
    RUN_TEST(test_forward_lookup_input_lane);
    RUN_TEST(test_forward_lookup_oob);
    RUN_TEST(test_forward_lookup_after_remove);

    // Group 9: Bridge sink ownership — activate/deactivate (DEBT-6 Phase 1.4)
    RUN_TEST(test_bridge_activate_calls_buildSink_and_set_sink);
    RUN_TEST(test_bridge_activate_fails_without_buildSink);
    RUN_TEST(test_bridge_deactivate_removes_sink);
    RUN_TEST(test_bridge_deactivate_calls_deinit);
    RUN_TEST(test_bridge_activate_idempotent);
    RUN_TEST(test_bridge_activate_ignores_non_dac);
    RUN_TEST(test_bridge_deactivate_unmapped_slot_safe);
    RUN_TEST(test_bridge_activate_deactivate_reactivate);
    RUN_TEST(test_on_device_available_uses_activate_path);
    RUN_TEST(test_on_device_removed_uses_deactivate_path);
    RUN_TEST(test_bridge_activate_buildSink_failure_clears_mapping);

    // Group 10: Multi-sink/source edge cases
    RUN_TEST(test_fragmented_multi_sink_reuse);
    RUN_TEST(test_sink_overflow_at_capacity);
    RUN_TEST(test_simultaneous_multi_sink_and_multi_source);

    return UNITY_END();
}
