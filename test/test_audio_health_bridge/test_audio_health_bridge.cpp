/**
 * test_audio_health_bridge.cpp
 *
 * Tests for hal_audio_health_bridge — the module that reads audio ADC health
 * status and drives HAL device state transitions with flap guard.
 *
 * Technique: inline-includes hal_device_manager.cpp, hal_driver_registry.cpp,
 * hal_pipeline_bridge.cpp, hal_audio_health_bridge.cpp, and hal_settings.cpp.
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
#include "../../src/hal/hal_audio_health_bridge.cpp"

// ===== Test ADC Device =====
class TestAdcDevice : public HalDevice {
public:
    bool probeResult;
    bool initResult;
    bool healthResult;

    TestAdcDevice(const char* compat) {
        strncpy(_descriptor.compatible, compat, 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, compat, 32);
        _descriptor.name[32] = '\0';
        _descriptor.type = HAL_DEV_ADC;
        _descriptor.channelCount = 2;
        _initPriority = HAL_PRIORITY_HARDWARE;
        probeResult  = true;
        initResult   = true;
        healthResult = true;
    }

    bool probe() override { return probeResult; }
    HalInitResult init() override {
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
    hal_audio_health_bridge_reset_for_test();
    ArduinoMock::mockMillis = 0;
}

void tearDown() {}

// Helper: register an ADC device, make it available, and map to pipeline
static int registerAvailableAdc(TestAdcDevice* dev) {
    int slot = mgr->registerDevice(dev, HAL_DISC_BUILTIN);
    dev->_state = HAL_STATE_AVAILABLE;
    dev->_ready = true;
    hal_pipeline_on_device_available(slot);
    return slot;
}

// =====================================================================
// Group 1: HW_FAULT / I2S_ERROR → UNAVAILABLE (5 tests)
// =====================================================================

void test_hw_fault_makes_device_unavailable() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);

    // Inject HW_FAULT on lane 0
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);
    TEST_ASSERT_FALSE(adc._ready);
}

void test_i2s_error_makes_device_unavailable() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    hal_audio_health_bridge_set_mock_status(0, AUDIO_I2S_ERROR);
    hal_audio_health_check();

    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);
    TEST_ASSERT_FALSE(adc._ready);
}

void test_fault_on_second_lane() {
    TestAdcDevice adc1("ti,pcm1808-a");
    TestAdcDevice adc2("ti,pcm1808-b");
    registerAvailableAdc(&adc1);
    registerAvailableAdc(&adc2);

    // Only lane 1 faults
    hal_audio_health_bridge_set_mock_status(1, AUDIO_HW_FAULT);
    hal_audio_health_check();

    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc1._state);
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc2._state);
}

void test_fault_does_nothing_if_already_unavailable() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    // First fault → UNAVAILABLE
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);

    // Second call with fault — should stay UNAVAILABLE (not change again)
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);
}

void test_fault_skips_unmapped_lane() {
    // No devices registered — lane has no HAL slot
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check(); // Should not crash
}

// =====================================================================
// Group 2: Recovery: OK → AVAILABLE (4 tests)
// =====================================================================

void test_recovery_ok_after_unavailable() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    // Fault → UNAVAILABLE
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);

    // Recovery → AVAILABLE
    ArduinoMock::mockMillis = 5000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);
    TEST_ASSERT_TRUE(adc._ready);
}

void test_recovery_does_nothing_if_already_available() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    // OK while already AVAILABLE — no state change
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);
}

void test_recovery_does_not_affect_error_state() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    // Manually set to ERROR
    adc._state = HAL_STATE_ERROR;
    adc._ready = false;

    // OK signal should NOT recover ERROR devices
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc._state);
}

void test_recovery_independent_per_lane() {
    TestAdcDevice adc1("ti,pcm1808-a");
    TestAdcDevice adc2("ti,pcm1808-b");
    registerAvailableAdc(&adc1);
    registerAvailableAdc(&adc2);

    // Both fault
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_bridge_set_mock_status(1, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc1._state);
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc2._state);

    // Only lane 0 recovers
    ArduinoMock::mockMillis = 5000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc1._state);
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc2._state);
}

// =====================================================================
// Group 3: Warning-only statuses (no state change) (3 tests)
// =====================================================================

void test_noise_only_no_state_change() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    hal_audio_health_bridge_set_mock_status(0, AUDIO_NOISE_ONLY);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);
}

void test_clipping_no_state_change() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    hal_audio_health_bridge_set_mock_status(0, AUDIO_CLIPPING);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);
}

void test_no_data_no_state_change() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    hal_audio_health_bridge_set_mock_status(0, AUDIO_NO_DATA);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);
}

// =====================================================================
// Group 4: Flap guard (5 tests)
// =====================================================================

void test_flap_guard_escalates_to_error() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    // Use base time > FLAP_WINDOW_MS (30s) to avoid uint32_t underflow
    uint32_t base = 60000;

    // Transition 1: AVAILABLE → UNAVAILABLE
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);

    // Transition 2: UNAVAILABLE → AVAILABLE (+2s)
    ArduinoMock::mockMillis = base + 2000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);

    // Transition 3: AVAILABLE → UNAVAILABLE (+5s) — >2 transitions, escalate
    ArduinoMock::mockMillis = base + 5000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    // Should escalate to ERROR due to flapping
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc._state);
    TEST_ASSERT_FALSE(adc._ready);
}

void test_flap_guard_does_not_trigger_if_spread_over_time() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    uint32_t base = 60000;

    // Transition 1
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    // Recover at +10s
    ArduinoMock::mockMillis = base + 10000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();

    // Transition 2 at +35s (>30s after first) — old transition expired
    ArduinoMock::mockMillis = base + 35000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    // Should be UNAVAILABLE, not ERROR (only 1 transition in window)
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);
}

void test_flap_guard_resets_after_reset() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    uint32_t base = 60000;

    // Create 2 transitions
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    ArduinoMock::mockMillis = base + 1000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();

    // Reset flap state
    hal_health_flap_reset();

    // Now another transition should NOT trigger flap guard (history cleared)
    adc._state = HAL_STATE_AVAILABLE;
    adc._ready = true;
    ArduinoMock::mockMillis = base + 2000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);
    // NOT ERROR — history was cleared
}

void test_flap_guard_independent_per_lane() {
    TestAdcDevice adc1("ti,pcm1808-a");
    TestAdcDevice adc2("ti,pcm1808-b");
    registerAvailableAdc(&adc1);
    registerAvailableAdc(&adc2);

    uint32_t base = 60000;

    // Flap lane 0 (3 transitions)
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    ArduinoMock::mockMillis = base + 1000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();
    ArduinoMock::mockMillis = base + 2000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    // Lane 0 → ERROR (flapping), lane 1 still AVAILABLE
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc1._state);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc2._state);
}

void test_flap_on_recovery_side() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    uint32_t base = 60000;

    // T1: fault
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);

    // T2: recover (+1s)
    ArduinoMock::mockMillis = base + 1000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);

    // T3: fault again (+2s) — 3 transitions total, flap guard fires
    ArduinoMock::mockMillis = base + 2000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc._state);
}

// =====================================================================
// Group 5: Reverse lookup (3 tests)
// =====================================================================

void test_reverse_lookup_mapped_lane() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    int8_t foundSlot = hal_pipeline_get_slot_for_adc_lane(0);
    TEST_ASSERT_EQUAL(slot, foundSlot);
}

void test_reverse_lookup_unmapped_lane() {
    // No devices registered
    int8_t foundSlot = hal_pipeline_get_slot_for_adc_lane(0);
    TEST_ASSERT_EQUAL(-1, foundSlot);
}

void test_reverse_lookup_sink_slot() {
    // Register a DAC-type device for sink slot test
    class TestDacDevice : public HalDevice {
    public:
        TestDacDevice() {
            strncpy(_descriptor.compatible, "ti,pcm5102a", 31);
            strncpy(_descriptor.name, "PCM5102A", 32);
            _descriptor.type = HAL_DEV_DAC;
            _descriptor.channelCount = 2;
            _initPriority = HAL_PRIORITY_HARDWARE;
        }
        bool probe() override { return true; }
        HalInitResult init() override { return hal_init_ok(); }
        void deinit() override {}
        void dumpConfig() override {}
        bool healthCheck() override { return true; }
    };

    TestDacDevice dac;
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;
    hal_pipeline_on_device_available(slot);

    int8_t foundSlot = hal_pipeline_get_slot_for_sink(AUDIO_SINK_SLOT_PRIMARY);
    TEST_ASSERT_EQUAL(slot, foundSlot);

    // Unmapped sink slot
    TEST_ASSERT_EQUAL(-1, hal_pipeline_get_slot_for_sink(7));
}

// =====================================================================
// Group 6: Correlation IDs (4 tests)
// =====================================================================

void test_corr_id_starts_at_zero() {
    TEST_ASSERT_EQUAL(0, hal_pipeline_active_corr_id());
}

void test_corr_id_begin_returns_nonzero() {
    uint16_t id = hal_pipeline_begin_correlation();
    TEST_ASSERT_NOT_EQUAL(0, id);
    TEST_ASSERT_EQUAL(id, hal_pipeline_active_corr_id());
}

void test_corr_id_end_resets_to_zero() {
    hal_pipeline_begin_correlation();
    hal_pipeline_end_correlation();
    TEST_ASSERT_EQUAL(0, hal_pipeline_active_corr_id());
}

void test_corr_id_increments() {
    uint16_t id1 = hal_pipeline_begin_correlation();
    hal_pipeline_end_correlation();
    uint16_t id2 = hal_pipeline_begin_correlation();
    hal_pipeline_end_correlation();
    TEST_ASSERT_EQUAL(id1 + 1, id2);
}

// =====================================================================
// Group 7: Reset for test (2 tests)
// =====================================================================

void test_reset_clears_mock_health() {
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_bridge_reset_for_test();

    // After reset, health should be AUDIO_OK — no state changes
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);
}

void test_reset_clears_flap_state() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    uint32_t base = 60000;

    // Build up flap history
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    ArduinoMock::mockMillis = base + 1000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();

    // Reset everything
    hal_audio_health_bridge_reset_for_test();
    mgr->reset();
    hal_pipeline_reset();

    // Re-register and verify no flap history
    TestAdcDevice adc2("ti,pcm1808");
    int slot2 = registerAvailableAdc(&adc2);

    ArduinoMock::mockMillis = base + 2000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    // Should be UNAVAILABLE, not ERROR (no history)
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc2._state);
}

// ===== Test Runner =====
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    // Group 1: HW_FAULT / I2S_ERROR → UNAVAILABLE
    RUN_TEST(test_hw_fault_makes_device_unavailable);
    RUN_TEST(test_i2s_error_makes_device_unavailable);
    RUN_TEST(test_fault_on_second_lane);
    RUN_TEST(test_fault_does_nothing_if_already_unavailable);
    RUN_TEST(test_fault_skips_unmapped_lane);

    // Group 2: Recovery
    RUN_TEST(test_recovery_ok_after_unavailable);
    RUN_TEST(test_recovery_does_nothing_if_already_available);
    RUN_TEST(test_recovery_does_not_affect_error_state);
    RUN_TEST(test_recovery_independent_per_lane);

    // Group 3: Warning-only
    RUN_TEST(test_noise_only_no_state_change);
    RUN_TEST(test_clipping_no_state_change);
    RUN_TEST(test_no_data_no_state_change);

    // Group 4: Flap guard
    RUN_TEST(test_flap_guard_escalates_to_error);
    RUN_TEST(test_flap_guard_does_not_trigger_if_spread_over_time);
    RUN_TEST(test_flap_guard_resets_after_reset);
    RUN_TEST(test_flap_guard_independent_per_lane);
    RUN_TEST(test_flap_on_recovery_side);

    // Group 5: Reverse lookup
    RUN_TEST(test_reverse_lookup_mapped_lane);
    RUN_TEST(test_reverse_lookup_unmapped_lane);
    RUN_TEST(test_reverse_lookup_sink_slot);

    // Group 6: Correlation IDs
    RUN_TEST(test_corr_id_starts_at_zero);
    RUN_TEST(test_corr_id_begin_returns_nonzero);
    RUN_TEST(test_corr_id_end_resets_to_zero);
    RUN_TEST(test_corr_id_increments);

    // Group 7: Reset
    RUN_TEST(test_reset_clears_mock_health);
    RUN_TEST(test_reset_clears_flap_state);

    return UNITY_END();
}
