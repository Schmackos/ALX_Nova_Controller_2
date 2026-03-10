// test_dac_settings.cpp
// Tests for DacState after toggle queue moved to HalCoordState.
//
// DacState retains only DAC-specific concerns: txUnderruns and eepromDiag.
// Device toggle queue lives in HalCoordState (tested in test_hal_coord).
// filterMode moved to HalDeviceConfig (DEBT-6 Phase 2).

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
    RUN_TEST(test_eeprom_diag_fields);
    RUN_TEST(test_dac_dirty_flag_lifecycle);

    return UNITY_END();
}
