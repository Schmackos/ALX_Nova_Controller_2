/**
 * test_hal_state_callback.cpp
 *
 * Tests for HalDeviceManager state change callback mechanism.
 * Verifies that the setStateChangeCallback() registration works correctly,
 * and that callbacks fire with the right (slot, oldState, newState) arguments
 * during initAll(), healthCheckAll(), and removeDevice() operations.
 *
 * Technique: inline-includes hal_device_manager.cpp and hal_driver_registry.cpp
 * (same pattern as test_hal_core.cpp).
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

// Inline the .cpp files for native testing
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// ===== Spy callback state =====
static uint8_t        lastSlot;
static HalDeviceState lastOldState;
static HalDeviceState lastNewState;
static int            callbackCount;

// History buffer for multi-fire tests
#define CB_HISTORY_MAX 32
static uint8_t        histSlot[CB_HISTORY_MAX];
static HalDeviceState histOld[CB_HISTORY_MAX];
static HalDeviceState histNew[CB_HISTORY_MAX];
static int            histCount;

static void spy_cb(uint8_t slot, HalDeviceState oldState, HalDeviceState newState) {
    lastSlot     = slot;
    lastOldState = oldState;
    lastNewState = newState;
    callbackCount++;
    if (histCount < CB_HISTORY_MAX) {
        histSlot[histCount] = slot;
        histOld[histCount]  = oldState;
        histNew[histCount]  = newState;
        histCount++;
    }
}

// ===== Test Device Implementation =====
class TestDevice : public HalDevice {
public:
    bool probeResult;
    bool initResult;
    bool healthResult;
    int  initCallCount;
    int  healthCallCount;

    TestDevice(const char* compatible, HalDeviceType type,
               uint16_t priority = HAL_PRIORITY_HARDWARE)
        : probeResult(true), initResult(true), healthResult(true),
          initCallCount(0), healthCallCount(0) {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = type;
        _initPriority = priority;
    }

    bool probe() override { return probeResult; }
    HalInitResult init() override {
        initCallCount++;
        return initResult ? hal_init_ok() : hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { healthCallCount++; return healthResult; }
};

// ===== Fixtures =====
static HalDeviceManager* mgr;

void setUp() {
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();

    // Reset spy state
    lastSlot      = 255;
    lastOldState  = HAL_STATE_UNKNOWN;
    lastNewState  = HAL_STATE_UNKNOWN;
    callbackCount = 0;
    histCount     = 0;
    memset(histSlot, 0, sizeof(histSlot));
    memset(histOld, 0, sizeof(histOld));
    memset(histNew, 0, sizeof(histNew));
}

void tearDown() {}

// =====================================================================
// Group 1: Callback Registration (3 tests)
// =====================================================================

// 1a: Setting a callback succeeds (verified by it firing later)
void test_callback_registration_set_succeeds() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev("dev0", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->initAll();

    // Callback should have fired at least once during initAll
    TEST_ASSERT_GREATER_THAN(0, callbackCount);
    TEST_ASSERT_EQUAL(slot, lastSlot);
}

// 1b: Setting callback to null is accepted (no crash on state changes)
void test_callback_registration_null_accepted() {
    mgr->setStateChangeCallback(nullptr);

    TestDevice dev("dev0", HAL_DEV_DAC);
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Should not crash with null callback
    mgr->initAll();
    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(0, callbackCount);
}

// 1c: Replacing callback works — old one no longer fires
void test_callback_registration_replace_works() {
    // Register spy_cb first
    mgr->setStateChangeCallback(spy_cb);

    // Replace with null
    mgr->setStateChangeCallback(nullptr);

    TestDevice dev("dev0", HAL_DEV_DAC);
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);
    mgr->initAll();

    // spy_cb should NOT have been called — it was replaced
    TEST_ASSERT_EQUAL(0, callbackCount);

    // Replace back with spy_cb and verify it fires
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev2("dev1", HAL_DEV_ADC);
    mgr->registerDevice(&dev2, HAL_DISC_BUILTIN);
    mgr->initAll();

    TEST_ASSERT_GREATER_THAN(0, callbackCount);
}

// =====================================================================
// Group 2: Fires on initAll (4 tests)
// =====================================================================

// 2a: Successful init fires UNKNOWN -> CONFIGURING -> AVAILABLE
void test_initAll_success_fires_unknown_configuring_available() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev("dev0", HAL_DEV_DAC);
    dev.initResult = true;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->initAll();

    // Expect exactly 2 callbacks: UNKNOWN->CONFIGURING, CONFIGURING->AVAILABLE
    TEST_ASSERT_EQUAL(2, callbackCount);

    TEST_ASSERT_EQUAL(HAL_STATE_UNKNOWN,     histOld[0]);
    TEST_ASSERT_EQUAL(HAL_STATE_CONFIGURING, histNew[0]);

    TEST_ASSERT_EQUAL(HAL_STATE_CONFIGURING, histOld[1]);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE,   histNew[1]);
}

// 2b: Failed init fires UNKNOWN -> CONFIGURING -> ERROR
void test_initAll_failure_fires_configuring_error() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev("dev0", HAL_DEV_DAC);
    dev.initResult = false;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->initAll();

    TEST_ASSERT_EQUAL(2, callbackCount);

    TEST_ASSERT_EQUAL(HAL_STATE_UNKNOWN,     histOld[0]);
    TEST_ASSERT_EQUAL(HAL_STATE_CONFIGURING, histNew[0]);

    TEST_ASSERT_EQUAL(HAL_STATE_CONFIGURING, histOld[1]);
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR,       histNew[1]);
}

// 2c: Already-AVAILABLE device is skipped by initAll (no callback)
void test_initAll_skips_already_available() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev("dev0", HAL_DEV_DAC);
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->initAll();

    // initAll only processes UNKNOWN or CONFIGURING states
    TEST_ASSERT_EQUAL(0, callbackCount);
    TEST_ASSERT_EQUAL(0, dev.initCallCount);
}

// 2d: Multiple devices each fire their own callbacks
void test_initAll_multiple_devices_fire_for_each() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev1("dev1", HAL_DEV_DAC);
    TestDevice dev2("dev2", HAL_DEV_ADC);
    TestDevice dev3("dev3", HAL_DEV_CODEC);
    dev1.initResult = true;
    dev2.initResult = true;
    dev3.initResult = false;  // This one fails

    int slot1 = mgr->registerDevice(&dev1, HAL_DISC_BUILTIN);
    int slot2 = mgr->registerDevice(&dev2, HAL_DISC_BUILTIN);
    int slot3 = mgr->registerDevice(&dev3, HAL_DISC_BUILTIN);

    mgr->initAll();

    // 3 devices x 2 transitions each = 6 callbacks
    TEST_ASSERT_EQUAL(6, callbackCount);

    // Verify all 3 slot indices appear in the history.
    // The exact order depends on priority sort (all same priority = insertion order).
    bool sawSlot1 = false, sawSlot2 = false, sawSlot3 = false;
    for (int i = 0; i < histCount; i++) {
        if (histSlot[i] == (uint8_t)slot1) sawSlot1 = true;
        if (histSlot[i] == (uint8_t)slot2) sawSlot2 = true;
        if (histSlot[i] == (uint8_t)slot3) sawSlot3 = true;
    }
    TEST_ASSERT_TRUE(sawSlot1);
    TEST_ASSERT_TRUE(sawSlot2);
    TEST_ASSERT_TRUE(sawSlot3);
}

// =====================================================================
// Group 3: Fires on healthCheckAll (4 tests)
// =====================================================================

// 3a: Health fail fires AVAILABLE -> UNAVAILABLE
void test_healthCheck_fail_fires_available_unavailable() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev("dev0", HAL_DEV_DAC);
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(1, callbackCount);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE,   lastOldState);
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, lastNewState);
}

// 3b: Health recover fires UNAVAILABLE -> AVAILABLE
void test_healthCheck_recover_fires_unavailable_available() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev("dev0", HAL_DEV_DAC);
    dev._state = HAL_STATE_UNAVAILABLE;
    dev._ready = false;
    dev.healthResult = true;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(1, callbackCount);
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, lastOldState);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE,   lastNewState);
}

// 3c: No state change when health OK and already AVAILABLE -> no callback
void test_healthCheck_no_change_no_callback() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev("dev0", HAL_DEV_DAC);
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = true;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(0, callbackCount);
}

// 3d: ERROR state with exhausted retries is skipped by healthCheckAll (no callback)
void test_healthCheck_skips_error_state() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev("dev0", HAL_DEV_DAC);
    dev._state = HAL_STATE_ERROR;
    dev._ready = false;
    dev.healthResult = false;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Simulate exhausted retries so healthCheckAll truly skips this device
    const_cast<HalRetryState*>(mgr->getRetryState((uint8_t)slot))->count = 3;

    mgr->healthCheckAll();

    // ERROR with exhausted retries is skipped — no callback, no health check
    TEST_ASSERT_EQUAL(0, callbackCount);
    TEST_ASSERT_EQUAL(0, dev.healthCallCount);
}

// =====================================================================
// Group 4: Fires on removeDevice (2 tests)
// =====================================================================

// 4a: Removing an AVAILABLE device fires oldState -> REMOVED with correct slot
void test_removeDevice_fires_available_removed() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev("dev0", HAL_DEV_DAC);
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->removeDevice(slot);

    TEST_ASSERT_EQUAL(1, callbackCount);
    TEST_ASSERT_EQUAL(slot, lastSlot);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, lastOldState);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED,   lastNewState);
}

// 4b: Removing a nonexistent slot does NOT fire the callback
void test_removeDevice_nonexistent_no_callback() {
    mgr->setStateChangeCallback(spy_cb);

    // No devices registered — remove slot 0
    bool result = mgr->removeDevice(0);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(0, callbackCount);
}

// =====================================================================
// Group 5: Slot/State Correctness (3 tests)
// =====================================================================

// 5a: Slot in callback matches the device's registered slot
void test_callback_slot_matches_device() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev1("dev1", HAL_DEV_DAC);
    TestDevice dev2("dev2", HAL_DEV_ADC);
    mgr->registerDevice(&dev1, HAL_DISC_BUILTIN);   // slot 0
    int slot2 = mgr->registerDevice(&dev2, HAL_DISC_BUILTIN);   // slot 1

    // Make dev1 already available so initAll skips it
    dev1._state = HAL_STATE_AVAILABLE;
    dev1._ready = true;

    mgr->initAll();

    // Only dev2 should have triggered callbacks (slot 1)
    // Check that both callbacks reference slot2
    for (int i = 0; i < histCount; i++) {
        TEST_ASSERT_EQUAL(slot2, histSlot[i]);
    }
}

// 5b: oldState reflects the actual previous state before transition
void test_callback_correct_old_state() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev("dev0", HAL_DEV_DAC);
    dev._state = HAL_STATE_UNAVAILABLE;
    dev._ready = false;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Remove from UNAVAILABLE — oldState should be UNAVAILABLE
    mgr->removeDevice(0);

    TEST_ASSERT_EQUAL(1, callbackCount);
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, lastOldState);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED,     lastNewState);
}

// 5c: newState reflects the actual resulting state
void test_callback_correct_new_state() {
    mgr->setStateChangeCallback(spy_cb);

    TestDevice dev("dev0", HAL_DEV_DAC);
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(1, callbackCount);
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, lastNewState);

    // Verify the device's actual state matches what the callback reported
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, dev._state);
}

// ===== Test Runner =====
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    // Group 1: Callback Registration
    RUN_TEST(test_callback_registration_set_succeeds);
    RUN_TEST(test_callback_registration_null_accepted);
    RUN_TEST(test_callback_registration_replace_works);

    // Group 2: Fires on initAll
    RUN_TEST(test_initAll_success_fires_unknown_configuring_available);
    RUN_TEST(test_initAll_failure_fires_configuring_error);
    RUN_TEST(test_initAll_skips_already_available);
    RUN_TEST(test_initAll_multiple_devices_fire_for_each);

    // Group 3: Fires on healthCheckAll
    RUN_TEST(test_healthCheck_fail_fires_available_unavailable);
    RUN_TEST(test_healthCheck_recover_fires_unavailable_available);
    RUN_TEST(test_healthCheck_no_change_no_callback);
    RUN_TEST(test_healthCheck_skips_error_state);

    // Group 4: Fires on removeDevice
    RUN_TEST(test_removeDevice_fires_available_removed);
    RUN_TEST(test_removeDevice_nonexistent_no_callback);

    // Group 5: Slot/State Correctness
    RUN_TEST(test_callback_slot_matches_device);
    RUN_TEST(test_callback_correct_old_state);
    RUN_TEST(test_callback_correct_new_state);

    return UNITY_END();
}
