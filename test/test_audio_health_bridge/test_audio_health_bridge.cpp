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
#include "../../src/hal/hal_ns4150b.cpp"
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
    diag_journal_reset_for_test();
    diag_journal_init();
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
        bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override {
            *out = AUDIO_OUTPUT_SINK_INIT;
            out->name = _descriptor.name;
            out->firstChannel = (uint8_t)(sinkSlot * 2);
            out->channelCount = 2;
            out->halSlot = _slot;
            return true;
        }
    };

    TestDacDevice dac;
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;
    hal_pipeline_on_device_available(slot);

    // Forward lookup: get the pipeline sink slot assigned to this HAL device
    int8_t sinkSlot = hal_pipeline_get_sink_slot(slot);
    TEST_ASSERT_NOT_EQUAL(-1, sinkSlot);

    // Reverse lookup: get the HAL device slot assigned to that sink slot
    int8_t foundSlot = hal_pipeline_get_slot_for_sink(sinkSlot);
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

// =====================================================================
// Group 8: Diagnostic rules 1, 4, 5 (14 tests)
// =====================================================================

// ---- Rule 1: I2S Recovery Storm ----

// 8a: No storm when counter is stable (no increment)
void test_rule1_no_storm_when_counter_stable() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    hal_audio_health_bridge_set_mock_i2s_recoveries(0, 0);
    hal_audio_health_check(); // counter unchanged — no diag entry

    // Journal should have 0 entries (diag_journal was reset in setUp via
    // diag_journal_reset_for_test inside hal_audio_health_bridge_reset_for_test
    // → actually we just check no I2S_RECOVERY code was emitted)
    DiagEvent ev;
    bool found = false;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        if (diag_journal_read(i, &ev) && ev.code == DIAG_AUDIO_I2S_RECOVERY) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_FALSE(found);
}

// 8b: Each individual recovery increment below the storm threshold does not emit
void test_rule1_below_threshold_no_emit() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    // 3 increments at 10s intervals — exactly at threshold, not above it
    for (uint32_t i = 1; i <= 3; i++) {
        ArduinoMock::mockMillis = i * 10000UL;
        hal_audio_health_bridge_set_mock_i2s_recoveries(0, i);
        hal_audio_health_check();
    }

    DiagEvent ev;
    bool found = false;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        if (diag_journal_read(i, &ev) && ev.code == DIAG_AUDIO_I2S_RECOVERY) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_FALSE(found);
}

// 8c: Fourth increment within 60s triggers DIAG_AUDIO_I2S_RECOVERY
void test_rule1_storm_emits_on_fourth_increment() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    // Start at base=120000 (> window size of 60000) to avoid uint32_t underflow
    // in the cutoff calculation (nowMs - 60000).
    uint32_t base = 120000UL;

    // 4 increments all within 60s
    for (uint32_t i = 1; i <= 4; i++) {
        ArduinoMock::mockMillis = base + i * 5000UL;
        hal_audio_health_bridge_set_mock_i2s_recoveries(0, i);
        hal_audio_health_check();
    }

    DiagEvent ev;
    bool found = false;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        DiagEvent e2;
        if (diag_journal_read(i, &e2) &&
            (uint16_t)e2.code == (uint16_t)DIAG_AUDIO_I2S_RECOVERY) {
            ev = e2;
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL(DIAG_SEV_WARN, ev.severity);
}

// 8d: Increments spread >60s apart do not storm (old timestamps expire)
void test_rule1_spread_over_time_no_storm() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    // First 3 at t=0,10,20s
    for (uint32_t i = 1; i <= 3; i++) {
        ArduinoMock::mockMillis = (i - 1) * 10000UL;
        hal_audio_health_bridge_set_mock_i2s_recoveries(0, i);
        hal_audio_health_check();
    }
    // Fourth at t=75s — first three timestamps have expired
    ArduinoMock::mockMillis = 75000UL;
    hal_audio_health_bridge_set_mock_i2s_recoveries(0, 4);
    hal_audio_health_check();

    DiagEvent ev;
    bool found = false;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        if (diag_journal_read(i, &ev) && ev.code == DIAG_AUDIO_I2S_RECOVERY) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_FALSE(found);
}

// 8e: Rule 1 is per-lane independent
void test_rule1_independent_per_lane() {
    TestAdcDevice adc1("ti,pcm1808-a");
    TestAdcDevice adc2("ti,pcm1808-b");
    registerAvailableAdc(&adc1);
    registerAvailableAdc(&adc2);

    // Start at base=120000 to avoid uint32_t underflow in cutoff calculation.
    uint32_t base = 120000UL;

    // Storm on lane 0 only (4 increments within the 60s window)
    for (uint32_t i = 1; i <= 4; i++) {
        ArduinoMock::mockMillis = base + i * 5000UL;
        hal_audio_health_bridge_set_mock_i2s_recoveries(0, i);
        hal_audio_health_check();
    }

    // Verify a DIAG_AUDIO_I2S_RECOVERY entry exists (lane 0)
    DiagEvent ev;
    bool found = false;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        DiagEvent e2;
        if (diag_journal_read(i, &e2) &&
            (uint16_t)e2.code == (uint16_t)DIAG_AUDIO_I2S_RECOVERY) {
            ev = e2;
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
    // Lane 1 counter was never incremented — no storm entry for lane 1's slot
    uint8_t stormLane1 = 0;
    n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        DiagEvent ev2;
        if (diag_journal_read(i, &ev2) &&
            (uint16_t)ev2.code == (uint16_t)DIAG_AUDIO_I2S_RECOVERY &&
            ev2.slot != ev.slot) {
            stormLane1++;
        }
    }
    TEST_ASSERT_EQUAL(0, stormLane1);
}

// ---- Rule 4: Audio No-Data with HAL AVAILABLE ----

// 8f: NO_DATA while AVAILABLE emits DIAG_AUDIO_PIPELINE_STALL once
void test_rule4_no_data_available_emits_once() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    hal_audio_health_bridge_set_mock_status(0, AUDIO_NO_DATA);
    // Run 3 checks — should only emit on the first
    hal_audio_health_check();
    hal_audio_health_check();
    hal_audio_health_check();

    // Scan journal for PIPELINE_STALL entries only (journal may have other entries
    // from device registration at setUp time, e.g. DIAG_HAL_DEVICE_DETECTED)
    uint8_t stallCount = 0;
    DiagEvent ev;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        DiagEvent e2;
        if (diag_journal_read(i, &e2) &&
            (uint16_t)e2.code == (uint16_t)DIAG_AUDIO_PIPELINE_STALL) {
            ev = e2;
            stallCount++;
        }
    }
    TEST_ASSERT_EQUAL(1, stallCount);
    TEST_ASSERT_EQUAL(DIAG_SEV_WARN, ev.severity);
}

// 8g: NO_DATA while UNAVAILABLE does NOT emit pipeline stall
void test_rule4_no_data_unavailable_no_emit() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    adc._state = HAL_STATE_UNAVAILABLE;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_NO_DATA);
    hal_audio_health_check();

    DiagEvent ev;
    bool found = false;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        if (diag_journal_read(i, &ev) && ev.code == DIAG_AUDIO_PIPELINE_STALL) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_FALSE(found);
}

// 8h: After NO_DATA resolves, a new episode emits again
void test_rule4_new_episode_emits_again() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    // Episode 1
    hal_audio_health_bridge_set_mock_status(0, AUDIO_NO_DATA);
    hal_audio_health_check();

    // Resolve
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();

    // Episode 2 — counter resets so we emit again
    hal_audio_health_bridge_set_mock_status(0, AUDIO_NO_DATA);
    hal_audio_health_check();

    uint8_t stallCount = 0;
    DiagEvent ev;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        if (diag_journal_read(i, &ev) && ev.code == DIAG_AUDIO_PIPELINE_STALL) {
            stallCount++;
        }
    }
    TEST_ASSERT_EQUAL(2, stallCount);
}

// 8i: OK status (not NO_DATA) never emits stall
void test_rule4_ok_no_stall() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();
    hal_audio_health_check();

    DiagEvent ev;
    bool found = false;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        if (diag_journal_read(i, &ev) && ev.code == DIAG_AUDIO_PIPELINE_STALL) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_FALSE(found);
}

// ---- Rule 5: DC Offset Drift ----

// 8j: DC offset below threshold never emits
void test_rule5_dc_below_threshold_no_emit() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    hal_audio_health_bridge_set_mock_dc_offset(0, 0.04f);
    for (int i = 0; i < 15; i++) {
        ArduinoMock::mockMillis = (uint32_t)(i * 5000);
        hal_audio_health_check();
    }

    DiagEvent ev;
    bool found = false;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        if (diag_journal_read(i, &ev) && ev.code == DIAG_AUDIO_DC_OFFSET_HIGH) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_FALSE(found);
}

// 8k: DC offset above threshold emits exactly once after 11 checks
void test_rule5_dc_high_emits_after_hold_period() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    hal_audio_health_bridge_set_mock_dc_offset(0, 0.10f);

    // Run 10 checks — hold count reaches 10, not yet at 11
    for (int i = 0; i < 10; i++) {
        ArduinoMock::mockMillis = (uint32_t)(i * 5000);
        hal_audio_health_check();
    }
    // Verify not yet emitted (need 11 checks to cross the hold threshold)
    DiagEvent ev;
    uint8_t preCount = 0;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        DiagEvent e2;
        if (diag_journal_read(i, &e2) &&
            (uint16_t)e2.code == (uint16_t)DIAG_AUDIO_DC_OFFSET_HIGH)
            preCount++;
    }
    TEST_ASSERT_EQUAL(0, preCount);

    // 11th check — should emit exactly once
    ArduinoMock::mockMillis = 50000UL;
    hal_audio_health_check();

    uint8_t postCount = 0;
    n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        DiagEvent e2;
        if (diag_journal_read(i, &e2) &&
            (uint16_t)e2.code == (uint16_t)DIAG_AUDIO_DC_OFFSET_HIGH) {
            ev = e2;
            postCount++;
        }
    }
    TEST_ASSERT_EQUAL(1, postCount);
    TEST_ASSERT_EQUAL(DIAG_SEV_WARN, ev.severity);

    // Additional checks — still only one emit (sentinel resets to avoid re-fire)
    ArduinoMock::mockMillis = 55000UL;
    hal_audio_health_check();
    ArduinoMock::mockMillis = 60000UL;
    hal_audio_health_check();

    uint8_t finalCount = 0;
    n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        DiagEvent e2;
        if (diag_journal_read(i, &e2) &&
            (uint16_t)e2.code == (uint16_t)DIAG_AUDIO_DC_OFFSET_HIGH)
            finalCount++;
    }
    TEST_ASSERT_EQUAL(1, finalCount);
}

// 8l: DC offset resets when it drops back below threshold
void test_rule5_dc_resets_on_clear() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    hal_audio_health_bridge_set_mock_dc_offset(0, 0.10f);

    // 5 checks high — not yet emitted
    for (int i = 0; i < 5; i++) {
        ArduinoMock::mockMillis = (uint32_t)(i * 5000);
        hal_audio_health_check();
    }

    // Drop below threshold — counter resets
    hal_audio_health_bridge_set_mock_dc_offset(0, 0.01f);
    ArduinoMock::mockMillis = 25000UL;
    hal_audio_health_check();

    // Go high again — need a fresh 11 checks, no emit yet
    hal_audio_health_bridge_set_mock_dc_offset(0, 0.10f);
    for (int i = 0; i < 10; i++) {
        ArduinoMock::mockMillis = 30000UL + (uint32_t)(i * 5000);
        hal_audio_health_check();
    }

    DiagEvent ev;
    uint8_t n = diag_journal_count();
    uint8_t count = 0;
    for (uint8_t i = 0; i < n; i++) {
        if (diag_journal_read(i, &ev) && ev.code == DIAG_AUDIO_DC_OFFSET_HIGH)
            count++;
    }
    TEST_ASSERT_EQUAL(0, count);
}

// 8m: DC offset high is per-lane independent
void test_rule5_independent_per_lane() {
    TestAdcDevice adc1("ti,pcm1808-a");
    TestAdcDevice adc2("ti,pcm1808-b");
    registerAvailableAdc(&adc1);
    registerAvailableAdc(&adc2);

    // Only lane 0 is high
    hal_audio_health_bridge_set_mock_dc_offset(0, 0.10f);
    hal_audio_health_bridge_set_mock_dc_offset(1, 0.01f);

    for (int i = 0; i <= 11; i++) {
        ArduinoMock::mockMillis = (uint32_t)(i * 5000);
        hal_audio_health_check();
    }

    // Only 1 emit (from lane 0)
    DiagEvent ev;
    uint8_t count = 0;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        if (diag_journal_read(i, &ev) && ev.code == DIAG_AUDIO_DC_OFFSET_HIGH)
            count++;
    }
    TEST_ASSERT_EQUAL(1, count);
}

// 8n: reset_for_test zeroes all Rule 1/4/5 state (continues below after Group 9)
void test_rule_state_cleared_by_reset() {
    TestAdcDevice adc("ti,pcm1808");
    registerAvailableAdc(&adc);

    // Drive Rule 5 counter partway
    hal_audio_health_bridge_set_mock_dc_offset(0, 0.10f);
    for (int i = 0; i < 5; i++) {
        ArduinoMock::mockMillis = (uint32_t)(i * 5000);
        hal_audio_health_check();
    }

    // Reset
    hal_audio_health_bridge_reset_for_test();
    mgr->reset();
    hal_pipeline_reset();

    // Re-register and confirm we need a fresh full hold period
    TestAdcDevice adc2("ti,pcm1808");
    registerAvailableAdc(&adc2);
    hal_audio_health_bridge_set_mock_dc_offset(0, 0.10f);

    for (int i = 0; i < 10; i++) {
        ArduinoMock::mockMillis = 50000UL + (uint32_t)(i * 5000);
        hal_audio_health_check();
    }

    DiagEvent ev;
    uint8_t count = 0;
    uint8_t n = diag_journal_count();
    for (uint8_t i = 0; i < n; i++) {
        if (diag_journal_read(i, &ev) && ev.code == DIAG_AUDIO_DC_OFFSET_HIGH)
            count++;
    }
    TEST_ASSERT_EQUAL(0, count); // Still no emit after only 10 checks
}

// =====================================================================
// Group 9: Flap guard edge cases (8 tests)
// =====================================================================

// 9a: Exactly 2 transitions within 30s should NOT escalate to ERROR.
// FLAP_MAX_TRANSITIONS == 2 means >2 triggers. 2 is at the threshold, not over.
void test_flap_exactly_two_transitions_no_escalation() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    uint32_t base = 60000;

    // Transition 1: AVAILABLE -> UNAVAILABLE (fault)
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);

    // Transition 2: UNAVAILABLE -> AVAILABLE (recovery)
    ArduinoMock::mockMillis = base + 5000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();

    // Should remain AVAILABLE — 2 transitions is not >2
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);
    TEST_ASSERT_TRUE(adc._ready);
}

// 9b: Exactly 3 transitions within 30s SHOULD escalate to ERROR.
// 3 > FLAP_MAX_TRANSITIONS(2) triggers the flap guard.
void test_flap_exactly_three_transitions_escalates() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    uint32_t base = 60000;

    // Transition 1: AVAILABLE -> UNAVAILABLE
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);

    // Transition 2: UNAVAILABLE -> AVAILABLE
    ArduinoMock::mockMillis = base + 2000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);

    // Transition 3: AVAILABLE -> UNAVAILABLE — 3 > 2, escalate
    ArduinoMock::mockMillis = base + 4000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc._state);
    TEST_ASSERT_FALSE(adc._ready);
}

// 9c: Timer wraparound at uint32_t boundary.
// millis() wraps at ~4.29 billion. The fix uses:
//   cutoff = (nowMs >= FLAP_WINDOW_MS) ? (nowMs - FLAP_WINDOW_MS) : 0
// When nowMs < FLAP_WINDOW_MS (30000), cutoff should be 0 (include all).
// Pre-fix bug would compute nowMs - 30000 wrapping to ~4.29B, pruning everything.
void test_flap_millis_wraparound_at_uint32_boundary() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    // Simulate millis near max uint32_t, then wrapping to low values.
    // Phase 1: first transition at a very high millis value (near max)
    uint32_t nearMax = 0xFFFFFFFF - 5000;  // ~4.29B - 5s
    ArduinoMock::mockMillis = nearMax;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);

    // Phase 2: recovery at a very low millis (wrapped around)
    // nowMs=1000 < FLAP_WINDOW_MS=30000, so cutoff=0 — the old timestamp
    // at nearMax will be >= 0 so it stays valid. This is correct behavior:
    // after wrapping, we can't know the true elapsed time, so we keep history.
    ArduinoMock::mockMillis = 1000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);

    // Phase 3: fault again at nowMs=3000, still < 30000 so cutoff=0.
    // All 3 transitions (nearMax, 1000, 3000) remain in window → escalate.
    ArduinoMock::mockMillis = 3000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    // With cutoff=0 all stored timestamps survive → 3 transitions → ERROR
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc._state);
    TEST_ASSERT_FALSE(adc._ready);
}

// 9d: ERROR-state persistence across multiple health check cycles.
// Once a device reaches ERROR, subsequent OK or fault health checks
// should NOT change the state. Neither fault nor recovery paths
// trigger for ERROR state.
void test_error_state_persists_across_health_checks() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    // Force ERROR via 3 rapid transitions
    uint32_t base = 60000;
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    ArduinoMock::mockMillis = base + 1000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();

    ArduinoMock::mockMillis = base + 2000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc._state);

    // Now run multiple health checks with AUDIO_OK — ERROR should persist
    for (int i = 1; i <= 5; i++) {
        ArduinoMock::mockMillis = base + 2000 + (uint32_t)(i * 5000);
        hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
        hal_audio_health_check();
        TEST_ASSERT_EQUAL_MESSAGE(HAL_STATE_ERROR, adc._state,
            "ERROR state must persist even with AUDIO_OK");
    }

    // Also verify with AUDIO_HW_FAULT — still stays ERROR
    ArduinoMock::mockMillis = base + 50000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc._state);
    TEST_ASSERT_FALSE(adc._ready);
}

// 9e: When flap guard escalates to ERROR, the pipeline bridge removes
// the ADC lane mapping via hal_pipeline_on_device_removed(). Verify
// that the HAL-to-ADC-lane mapping table entry is cleared.
void test_flap_error_clears_adc_lane_mapping() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    // Confirm ADC lane 0 is mapped before flapping
    int8_t laneBeforeFlap = hal_pipeline_get_input_lane((uint8_t)slot);
    TEST_ASSERT_EQUAL(0, laneBeforeFlap);

    // Trigger flap guard (3 transitions → ERROR)
    uint32_t base = 60000;
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    ArduinoMock::mockMillis = base + 1000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();

    ArduinoMock::mockMillis = base + 2000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc._state);

    // ADC lane mapping should now be cleared (-1) by on_device_removed
    int8_t laneAfterError = hal_pipeline_get_input_lane((uint8_t)slot);
    TEST_ASSERT_EQUAL(-1, laneAfterError);

    // Reverse lookup for lane 0 should also return -1 (unmapped)
    int8_t slotForLane0 = hal_pipeline_get_slot_for_adc_lane(0);
    TEST_ASSERT_EQUAL(-1, slotForLane0);
}

// 9f: Recovery from ERROR requires explicit re-enable — ERROR state
// does NOT auto-recover when AUDIO_OK is observed.
// The recovery path (section 3) only fires for currentState==UNAVAILABLE.
void test_error_no_auto_recovery() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    // Manually set to ERROR (simulating a previous flap escalation)
    adc._state = HAL_STATE_ERROR;
    adc._ready = false;

    // Multiple AUDIO_OK health checks should NOT change state
    for (int i = 0; i < 10; i++) {
        ArduinoMock::mockMillis = 60000 + (uint32_t)(i * 5000);
        hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
        hal_audio_health_check();
    }
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc._state);
    TEST_ASSERT_FALSE(adc._ready);

    // Also verify HW_FAULT doesn't transition ERROR devices (the fault
    // path only fires from AVAILABLE)
    ArduinoMock::mockMillis = 200000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc._state);
}

// 9g: Multiple ADCs flapping independently — lane 0 flapping should
// NOT affect lane 1's state. Each lane has its own FlapState struct.
void test_flap_multiple_adcs_independent_flap_state() {
    TestAdcDevice adc0("ti,pcm1808-a");
    TestAdcDevice adc1("ti,pcm1808-b");
    registerAvailableAdc(&adc0);
    registerAvailableAdc(&adc1);

    uint32_t base = 60000;

    // Flap lane 0 to ERROR (3 transitions)
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    ArduinoMock::mockMillis = base + 1000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();

    ArduinoMock::mockMillis = base + 2000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc0._state);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc1._state);

    // Now give lane 1 a single fault — should go UNAVAILABLE, not ERROR
    ArduinoMock::mockMillis = base + 3000;
    hal_audio_health_bridge_set_mock_status(1, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc1._state);

    // Recover lane 1 — should go AVAILABLE (only 2 transitions, not >2)
    ArduinoMock::mockMillis = base + 4000;
    hal_audio_health_bridge_set_mock_status(1, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc1._state);
    TEST_ASSERT_TRUE(adc1._ready);

    // Lane 0 still in ERROR throughout
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc0._state);
}

// 9h: Transition at exactly the window boundary.
// A transition recorded at exactly 30000ms after the first transition
// should cause the first one to be pruned (cutoff = nowMs - 30000 = base,
// and timestamps[i] >= cutoff is checked with >=, so base == cutoff means
// the old entry survives). A transition at base+30001 should prune it.
void test_flap_transition_at_exact_window_boundary() {
    TestAdcDevice adc("ti,pcm1808");
    int slot = registerAvailableAdc(&adc);

    uint32_t base = 60000;

    // Transition 1: fault at base
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adc._state);

    // Transition 2: recover at base+10000
    ArduinoMock::mockMillis = base + 10000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adc._state);

    // Transition 3: fault at exactly base+30000 (exactly 30s after first)
    // cutoff = (base+30000) - 30000 = base = 60000
    // timestamps[0] = base = 60000 >= cutoff(60000) → survives (>=)
    // So all 3 transitions remain in window → escalate to ERROR
    ArduinoMock::mockMillis = base + 30000;
    hal_audio_health_bridge_set_mock_status(0, AUDIO_HW_FAULT);
    hal_audio_health_check();

    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adc._state);
    TEST_ASSERT_FALSE(adc._ready);
}

// =====================================================================
// Group 10: 8-Lane Health Check Coverage (3 tests)
//
// Existing tests cover lanes 0 and 1. These verify that the health
// bridge loops correctly up to AUDIO_PIPELINE_MAX_INPUTS (8) without
// out-of-bounds access, and that flap guard state initializes for
// higher lanes.
// =====================================================================

void test_health_check_lane_2() {
    // Register 3 ADC devices (lanes 0, 1, 2)
    TestAdcDevice adc0("ti,pcm1808-l0");
    TestAdcDevice adc1("ti,pcm1808-l1");
    TestAdcDevice adc2("ti,pcm1808-l2");
    registerAvailableAdc(&adc0);
    registerAvailableAdc(&adc1);
    registerAvailableAdc(&adc2);
    TestAdcDevice* adcs[] = {&adc0, &adc1, &adc2};

    // Fault on lane 2 only
    hal_audio_health_bridge_set_mock_status(2, AUDIO_HW_FAULT);
    hal_audio_health_check();

    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adcs[0]->_state);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adcs[1]->_state);
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adcs[2]->_state);
    TEST_ASSERT_FALSE(adcs[2]->_ready);

    // Recovery on lane 2
    ArduinoMock::mockMillis = 5000;
    hal_audio_health_bridge_set_mock_status(2, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adcs[2]->_state);
    TEST_ASSERT_TRUE(adcs[2]->_ready);
}

void test_health_check_lane_7() {
    // Register 8 ADC devices (lanes 0-7) to exercise the full range
    TestAdcDevice a0("ti,pcm1808-0"), a1("ti,pcm1808-1"),
                  a2("ti,pcm1808-2"), a3("ti,pcm1808-3"),
                  a4("ti,pcm1808-4"), a5("ti,pcm1808-5"),
                  a6("ti,pcm1808-6"), a7("ti,pcm1808-7");
    registerAvailableAdc(&a0); registerAvailableAdc(&a1);
    registerAvailableAdc(&a2); registerAvailableAdc(&a3);
    registerAvailableAdc(&a4); registerAvailableAdc(&a5);
    registerAvailableAdc(&a6); registerAvailableAdc(&a7);
    TestAdcDevice* adcs[] = {&a0, &a1, &a2, &a3, &a4, &a5, &a6, &a7};

    // Fault on lane 7 only
    hal_audio_health_bridge_set_mock_status(7, AUDIO_HW_FAULT);
    hal_audio_health_check();

    // Lanes 0-6 should be unaffected
    for (int i = 0; i < 7; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(HAL_STATE_AVAILABLE, adcs[i]->_state,
            "Lanes 0-6 should remain AVAILABLE");
    }
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adcs[7]->_state);
    TEST_ASSERT_FALSE(adcs[7]->_ready);

    // Recovery on lane 7
    ArduinoMock::mockMillis = 5000;
    hal_audio_health_bridge_set_mock_status(7, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adcs[7]->_state);
    TEST_ASSERT_TRUE(adcs[7]->_ready);
}

void test_flap_guard_init_higher_lanes() {
    // Register 4 ADC devices (lanes 0-3) — verify flap guard works on lane 3
    TestAdcDevice d0("ti,pcm1808-f0"), d1("ti,pcm1808-f1"),
                  d2("ti,pcm1808-f2"), d3("ti,pcm1808-f3");
    registerAvailableAdc(&d0); registerAvailableAdc(&d1);
    registerAvailableAdc(&d2); registerAvailableAdc(&d3);
    TestAdcDevice* adcs[] = {&d0, &d1, &d2, &d3};

    uint32_t base = 60000;

    // Flap lane 3: 3 transitions within 30s → ERROR
    ArduinoMock::mockMillis = base;
    hal_audio_health_bridge_set_mock_status(3, AUDIO_HW_FAULT);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, adcs[3]->_state);

    ArduinoMock::mockMillis = base + 2000;
    hal_audio_health_bridge_set_mock_status(3, AUDIO_OK);
    hal_audio_health_check();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, adcs[3]->_state);

    ArduinoMock::mockMillis = base + 4000;
    hal_audio_health_bridge_set_mock_status(3, AUDIO_HW_FAULT);
    hal_audio_health_check();

    // Lane 3 should escalate to ERROR; lanes 0-2 remain AVAILABLE
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, adcs[3]->_state);
    TEST_ASSERT_FALSE(adcs[3]->_ready);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(HAL_STATE_AVAILABLE, adcs[i]->_state,
            "Lanes 0-2 should remain AVAILABLE during lane 3 flap");
    }
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

    // Group 8: Diagnostic rules 1, 4, 5
    RUN_TEST(test_rule1_no_storm_when_counter_stable);
    RUN_TEST(test_rule1_below_threshold_no_emit);
    RUN_TEST(test_rule1_storm_emits_on_fourth_increment);
    RUN_TEST(test_rule1_spread_over_time_no_storm);
    RUN_TEST(test_rule1_independent_per_lane);
    RUN_TEST(test_rule4_no_data_available_emits_once);
    RUN_TEST(test_rule4_no_data_unavailable_no_emit);
    RUN_TEST(test_rule4_new_episode_emits_again);
    RUN_TEST(test_rule4_ok_no_stall);
    RUN_TEST(test_rule5_dc_below_threshold_no_emit);
    RUN_TEST(test_rule5_dc_high_emits_after_hold_period);
    RUN_TEST(test_rule5_dc_resets_on_clear);
    RUN_TEST(test_rule5_independent_per_lane);
    RUN_TEST(test_rule_state_cleared_by_reset);

    // Group 9: Flap guard edge cases
    RUN_TEST(test_flap_exactly_two_transitions_no_escalation);
    RUN_TEST(test_flap_exactly_three_transitions_escalates);
    RUN_TEST(test_flap_millis_wraparound_at_uint32_boundary);
    RUN_TEST(test_error_state_persists_across_health_checks);
    RUN_TEST(test_flap_error_clears_adc_lane_mapping);
    RUN_TEST(test_error_no_auto_recovery);
    RUN_TEST(test_flap_multiple_adcs_independent_flap_state);
    RUN_TEST(test_flap_transition_at_exact_window_boundary);

    // Group 10: 8-Lane Health Check Coverage
    RUN_TEST(test_health_check_lane_2);
    RUN_TEST(test_health_check_lane_7);
    RUN_TEST(test_flap_guard_init_higher_lanes);

    return UNITY_END();
}
