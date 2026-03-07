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

// Inline the .cpp files
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
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

    bool probe() override { return probeResult; }
    HalInitResult init() override {
        initCount++;
        return initResult ? hal_init_ok() : hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return healthResult; }
};

// ===== Fixtures =====

static HalDeviceManager* mgr;

void setUp() {
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
    hal_pipeline_reset();
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

// 3d: Third ADC is rejected (only 2 lanes)
void test_third_adc_rejected() {
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

    // Only 2 ADC lanes available
    TEST_ASSERT_EQUAL(2, hal_pipeline_input_count());
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

    hal_pipeline_state_change(slot, HAL_STATE_AVAILABLE, HAL_STATE_MANUAL);
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

    // MANUAL clears the mapping
    hal_pipeline_state_change(slot, HAL_STATE_AVAILABLE, HAL_STATE_MANUAL);
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
    RUN_TEST(test_third_adc_rejected);
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

    // Group 6: Count helpers + reset
    RUN_TEST(test_counts_add_remove);
    RUN_TEST(test_reset_zeros_all);
    RUN_TEST(test_reset_then_readd);

    return UNITY_END();
}
