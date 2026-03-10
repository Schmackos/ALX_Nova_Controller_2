// test_dac_settings.cpp
// Tests for DacState after DEBT-5 Phase 6 cleanup.
//
// DacState no longer holds device-specific fields (enabled, volume, mute, etc.)
// — those live in HalDeviceConfig via the HAL device manager.
// DacState retains: txUnderruns, pendingToggle, requestDeviceToggle(),
// and eepromDiag. filterMode moved to HalDeviceConfig (DEBT-6 Phase 2).

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/app_state.h"
#include "../../src/hal/hal_types.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void resetDacFields() {
    AppState& as = AppState::getInstance();
    as.dac.txUnderruns = 0;
    as.dac.pendingToggle.halSlot = 0xFF;
    as.dac.pendingToggle.action = 0;
    memset(&as.dac.eepromDiag, 0, sizeof(as.dac.eepromDiag));
    as.clearDacDirty();
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
    resetDacFields();
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// Test: DacState defaults after reset
// ---------------------------------------------------------------------------

void test_dac_state_defaults(void) {
    AppState& as = AppState::getInstance();
    TEST_ASSERT_EQUAL_UINT32(0, as.dac.txUnderruns);
    TEST_ASSERT_EQUAL_UINT8(0xFF, as.dac.pendingToggle.halSlot);
    TEST_ASSERT_EQUAL_INT8(0, as.dac.pendingToggle.action);
}

// ---------------------------------------------------------------------------
// Test: filterMode lives in HalDeviceConfig (moved from DacState in DEBT-6)
// ---------------------------------------------------------------------------

void test_filter_mode_in_hal_device_config(void) {
    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    TEST_ASSERT_EQUAL_UINT8(0, cfg.filterMode);
    cfg.filterMode = 3;
    TEST_ASSERT_EQUAL_UINT8(3, cfg.filterMode);
    cfg.filterMode = 0;
    TEST_ASSERT_EQUAL_UINT8(0, cfg.filterMode);
}

// ---------------------------------------------------------------------------
// Test: txUnderruns increments correctly
// ---------------------------------------------------------------------------

void test_dac_state_tx_underruns(void) {
    AppState& as = AppState::getInstance();
    as.dac.txUnderruns++;
    as.dac.txUnderruns++;
    TEST_ASSERT_EQUAL_UINT32(2, as.dac.txUnderruns);
}

// ---------------------------------------------------------------------------
// Test: DacState has generic requestDeviceToggle but NOT legacy methods
// ---------------------------------------------------------------------------

void test_dac_state_generic_toggle_only(void) {
    DacState ds = {};
    // Generic toggle should work
    ds.requestDeviceToggle(0, 1);
    TEST_ASSERT_EQUAL_UINT8(0, ds.pendingToggle.halSlot);
    TEST_ASSERT_EQUAL_INT8(1, ds.pendingToggle.action);
    // Legacy requestDacToggle and requestEs8311Toggle should NOT compile
    // (compile-time verification -- if this file compiles, legacy is gone)
}

// ---------------------------------------------------------------------------
// Test: PendingDeviceToggle rejects invalid values
// ---------------------------------------------------------------------------

void test_pending_device_toggle_rejects_invalid(void) {
    DacState ds = {};
    // Invalid slot (0xFF) -- should be rejected
    ds.requestDeviceToggle(0xFF, 1);
    TEST_ASSERT_EQUAL_UINT8(0xFF, ds.pendingToggle.halSlot);  // unchanged (still default 0xFF)
    TEST_ASSERT_EQUAL_INT8(0, ds.pendingToggle.action);        // unchanged

    // Invalid action (2) -- should be rejected
    ds.requestDeviceToggle(0, 2);
    TEST_ASSERT_EQUAL_UINT8(0xFF, ds.pendingToggle.halSlot);  // unchanged
    TEST_ASSERT_EQUAL_INT8(0, ds.pendingToggle.action);        // unchanged

    // Valid request
    ds.requestDeviceToggle(3, -1);
    TEST_ASSERT_EQUAL_UINT8(3, ds.pendingToggle.halSlot);
    TEST_ASSERT_EQUAL_INT8(-1, ds.pendingToggle.action);
}

// ---------------------------------------------------------------------------
// Test: EepromDiag struct can be populated and read
// ---------------------------------------------------------------------------

void test_eeprom_diag_fields(void) {
    AppState& as = AppState::getInstance();
    EepromDiag& ed = as.dac.eepromDiag;
    ed.scanned = true;
    ed.found = true;
    ed.eepromAddr = 0x50;
    ed.deviceId = 0x0001;
    strncpy(ed.deviceName, "PCM5102A", sizeof(ed.deviceName) - 1);

    TEST_ASSERT_TRUE(ed.scanned);
    TEST_ASSERT_TRUE(ed.found);
    TEST_ASSERT_EQUAL_UINT8(0x50, ed.eepromAddr);
    TEST_ASSERT_EQUAL_UINT16(0x0001, ed.deviceId);
    TEST_ASSERT_EQUAL_STRING("PCM5102A", ed.deviceName);
}

// ---------------------------------------------------------------------------
// Test: DAC dirty flag lifecycle
// ---------------------------------------------------------------------------

void test_dac_dirty_flag_lifecycle(void) {
    AppState& as = AppState::getInstance();
    TEST_ASSERT_FALSE(as.isDacDirty());
    as.markDacDirty();
    TEST_ASSERT_TRUE(as.isDacDirty());
    as.clearDacDirty();
    TEST_ASSERT_FALSE(as.isDacDirty());
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_dac_state_defaults);
    RUN_TEST(test_filter_mode_in_hal_device_config);
    RUN_TEST(test_dac_state_tx_underruns);
    RUN_TEST(test_dac_state_generic_toggle_only);
    RUN_TEST(test_pending_device_toggle_rejects_invalid);
    RUN_TEST(test_eeprom_diag_fields);
    RUN_TEST(test_dac_dirty_flag_lifecycle);

    return UNITY_END();
}
