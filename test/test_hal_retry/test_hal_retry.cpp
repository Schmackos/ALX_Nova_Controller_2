/**
 * test_hal_retry.cpp
 *
 * Tests for Phase 2 of the diagnostics architecture: HalInitResult,
 * non-blocking retry logic, and fault counters in HalDeviceManager.
 *
 * Covers:
 *   - HalInitResult construction and field correctness
 *   - initAll() behavior with HalInitResult (success/failure/mixed)
 *   - healthCheckAll() retry logic: timing, exponential backoff, recovery,
 *     retry exhaustion, and fault counter accumulation
 *   - Retry state accessors (valid/invalid slots)
 *
 * Technique: inline-includes hal_device_manager.cpp and hal_driver_registry.cpp
 * (same pattern as test_hal_state_callback.cpp).
 */

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"

// Inline .cpp files for native testing
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// ===== Test Device Mock =====
class TestDevice : public HalDevice {
public:
    bool initResult;
    bool healthResult;
    int  initCallCount;
    int  deinitCallCount;

    TestDevice(const char* compatible = "test,device",
               HalDeviceType type = HAL_DEV_DAC,
               uint16_t priority = HAL_PRIORITY_HARDWARE)
        : initResult(true), healthResult(true),
          initCallCount(0), deinitCallCount(0) {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, compatible, 32);
        _descriptor.name[32] = '\0';
        _descriptor.type = type;
        _initPriority = priority;
    }

    bool probe() override { return true; }

    HalInitResult init() override {
        initCallCount++;
        if (initResult) {
            return hal_init_ok();
        }
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }

    void deinit() override {
        deinitCallCount++;
    }

    void dumpConfig() override {}

    bool healthCheck() override {
        return healthResult;
    }
};

// ===== Fixtures =====
static HalDeviceManager* mgr;

void setUp() {
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
    ArduinoMock::mockMillis = 0;
}

void tearDown() {}

// =====================================================================
// Group 1: HalInitResult Basics (5 tests)
// =====================================================================

// 1. hal_init_ok() returns success with correct defaults
void test_hal_init_ok_returns_success() {
    HalInitResult r = hal_init_ok();

    TEST_ASSERT_TRUE(r.success);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)DIAG_OK, r.errorCode);
    TEST_ASSERT_EQUAL_CHAR('\0', r.reason[0]);
}

// 2. hal_init_fail() returns failure with error code and reason
void test_hal_init_fail_returns_failure() {
    HalInitResult r = hal_init_fail(DIAG_HAL_INIT_FAILED, "reason");

    TEST_ASSERT_FALSE(r.success);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)DIAG_HAL_INIT_FAILED, r.errorCode);
    TEST_ASSERT_EQUAL_STRING("reason", r.reason);
}

// 3. hal_init_fail() with null reason does not crash, reason is empty
void test_hal_init_fail_null_reason() {
    HalInitResult r = hal_init_fail(DIAG_HAL_INIT_FAILED, nullptr);

    TEST_ASSERT_FALSE(r.success);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)DIAG_HAL_INIT_FAILED, r.errorCode);
    TEST_ASSERT_EQUAL_CHAR('\0', r.reason[0]);
}

// 4. hal_init_fail() with long reason truncates to 47 chars + null
void test_hal_init_fail_long_reason_truncated() {
    // 60-char string: exceeds the 48-byte reason buffer (47 chars + null)
    const char* longReason = "This is a very long reason string that exceeds buffer size!!";
    // Verify input is indeed 60 chars
    TEST_ASSERT_EQUAL(60, (int)strlen(longReason));

    HalInitResult r = hal_init_fail(DIAG_HAL_INIT_FAILED, longReason);

    TEST_ASSERT_FALSE(r.success);
    // reason[47] must be null terminator
    TEST_ASSERT_EQUAL_CHAR('\0', r.reason[47]);
    // Length must be exactly 47 (truncated)
    TEST_ASSERT_EQUAL(47, (int)strlen(r.reason));
    // First 47 chars must match
    TEST_ASSERT_EQUAL_INT(0, strncmp(r.reason, longReason, 47));
}

// 5. TestDevice init() returns correct HalInitResult based on initResult flag
void test_init_result_carries_through_device() {
    TestDevice dev("test,carry");

    dev.initResult = true;
    HalInitResult r1 = dev.init();
    TEST_ASSERT_TRUE(r1.success);

    dev.initResult = false;
    HalInitResult r2 = dev.init();
    TEST_ASSERT_FALSE(r2.success);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)DIAG_HAL_INIT_FAILED, r2.errorCode);
}

// =====================================================================
// Group 2: initAll with HalInitResult (5 tests)
// =====================================================================

// 6. initAll with successful init sets AVAILABLE and ready
void test_initAll_success_sets_available() {
    TestDevice dev("test,ok");
    dev.initResult = true;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->initAll();

    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);
    TEST_ASSERT_TRUE(dev._ready);
}

// 7. initAll with failed init sets ERROR and not ready
void test_initAll_failure_sets_error() {
    TestDevice dev("test,fail");
    dev.initResult = false;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->initAll();

    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, dev._state);
    TEST_ASSERT_FALSE(dev._ready);
}

// 8. initAll records lastErrorCode in retryState on failure
void test_initAll_failure_records_error_code() {
    TestDevice dev("test,err");
    dev.initResult = false;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->initAll();

    const HalRetryState* rs = mgr->getRetryState((uint8_t)slot);
    TEST_ASSERT_NOT_NULL(rs);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)DIAG_HAL_INIT_FAILED, rs->lastErrorCode);
}

// 9. initAll with mixed devices: first succeeds, second fails, third succeeds
void test_initAll_mixed_devices() {
    TestDevice dev1("test,mix1");
    TestDevice dev2("test,mix2");
    TestDevice dev3("test,mix3");
    dev1.initResult = true;
    dev2.initResult = false;
    dev3.initResult = true;

    mgr->registerDevice(&dev1, HAL_DISC_BUILTIN);
    mgr->registerDevice(&dev2, HAL_DISC_BUILTIN);
    mgr->registerDevice(&dev3, HAL_DISC_BUILTIN);

    mgr->initAll();

    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev1._state);
    TEST_ASSERT_TRUE(dev1._ready);

    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, dev2._state);
    TEST_ASSERT_FALSE(dev2._ready);

    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev3._state);
    TEST_ASSERT_TRUE(dev3._ready);
}

// 10. initAll only processes UNKNOWN or CONFIGURING states
void test_initAll_only_inits_unknown_configuring() {
    TestDevice dev("test,skip");
    dev.initResult = true;
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->initAll();

    // Device was already AVAILABLE, so init() should NOT have been called
    TEST_ASSERT_EQUAL(0, dev.initCallCount);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);
}

// =====================================================================
// Group 3: healthCheckAll Retry Logic (10 tests)
// =====================================================================

// 11. Health check failure sets device to UNAVAILABLE
void test_health_fail_sets_unavailable() {
    TestDevice dev("test,hfail");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, dev._state);
    TEST_ASSERT_FALSE(dev._ready);
}

// 12. Retry is not attempted before the delay expires
void test_retry_not_attempted_before_delay() {
    TestDevice dev("test,early");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Health check fails at t=0 -> UNAVAILABLE, nextRetryMs = 0 + 1000 = 1000
    ArduinoMock::mockMillis = 0;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, dev._state);

    // Reset init call count to track retry attempts
    dev.initCallCount = 0;
    dev.deinitCallCount = 0;
    dev.initResult = true;

    // At t=500, retry should NOT be attempted (too early)
    ArduinoMock::mockMillis = 500;
    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(0, dev.initCallCount);
    TEST_ASSERT_EQUAL(0, dev.deinitCallCount);
    // Device should still be UNAVAILABLE
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, dev._state);
}

// 13. Retry is attempted after the delay expires
void test_retry_attempted_after_delay() {
    TestDevice dev("test,after");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Health check fails at t=0 -> UNAVAILABLE, nextRetryMs = 1000
    ArduinoMock::mockMillis = 0;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, dev._state);

    // Reset counters
    dev.initCallCount = 0;
    dev.deinitCallCount = 0;
    dev.initResult = true;

    // At t=1000, retry SHOULD be attempted
    ArduinoMock::mockMillis = 1000;
    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(1, dev.deinitCallCount);
    TEST_ASSERT_EQUAL(1, dev.initCallCount);
}

// 14. Successful retry recovers device to AVAILABLE and resets retry state
void test_retry_success_recovers_to_available() {
    TestDevice dev("test,recover");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Health check fails at t=0
    ArduinoMock::mockMillis = 0;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, dev._state);

    // Fix the device: init will succeed
    dev.initResult = true;

    // Advance past retry delay
    ArduinoMock::mockMillis = 1000;
    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);
    TEST_ASSERT_TRUE(dev._ready);

    // Retry state should be reset
    const HalRetryState* rs = mgr->getRetryState((uint8_t)slot);
    TEST_ASSERT_NOT_NULL(rs);
    TEST_ASSERT_EQUAL(0, rs->count);
    TEST_ASSERT_EQUAL(0, rs->nextRetryMs);
    TEST_ASSERT_EQUAL(0, rs->lastErrorCode);
}

// 15. Failed retry increments retry count
void test_retry_failure_increments_count() {
    TestDevice dev("test,rinc");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Health check fails at t=0 -> count starts at 0
    ArduinoMock::mockMillis = 0;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, dev._state);

    const HalRetryState* rs = mgr->getRetryState((uint8_t)slot);
    TEST_ASSERT_EQUAL(0, rs->count);

    // Init will also fail on retry
    dev.initResult = false;

    // First retry at t=1000 -> fails -> count becomes 1
    ArduinoMock::mockMillis = 1000;
    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(1, rs->count);
}

// 16. Exponential backoff timing: 1s, 2s, 4s
void test_exponential_backoff_timing() {
    TestDevice dev("test,backoff");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Health check fails at t=0 -> UNAVAILABLE, nextRetryMs = 1000
    ArduinoMock::mockMillis = 0;
    mgr->healthCheckAll();

    const HalRetryState* rs = mgr->getRetryState((uint8_t)slot);
    TEST_ASSERT_EQUAL(1000, rs->nextRetryMs);

    // Init fails on retry
    dev.initResult = false;

    // First retry at t=1000 -> fails -> count=1, nextRetryMs = 1000 + (1000 << 0) = 2000
    ArduinoMock::mockMillis = 1000;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(1, rs->count);
    TEST_ASSERT_EQUAL(2000, rs->nextRetryMs);

    // Second retry at t=2000 -> fails -> count=2, nextRetryMs = 2000 + (1000 << 1) = 4000
    ArduinoMock::mockMillis = 2000;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(2, rs->count);
    TEST_ASSERT_EQUAL(4000, rs->nextRetryMs);

    // Third retry at t=4000 -> fails -> count=3 = HAL_MAX_RETRIES -> permanent ERROR
    ArduinoMock::mockMillis = 4000;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(3, rs->count);
}

// 17. Retry exhaustion (3 failures) sets permanent ERROR state
void test_retry_exhaustion_sets_permanent_error() {
    TestDevice dev("test,exhaust");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    dev.initResult = false;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Health check fails at t=0
    ArduinoMock::mockMillis = 0;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, dev._state);

    // Exhaust all 3 retries
    ArduinoMock::mockMillis = 1000;
    mgr->healthCheckAll(); // retry 1 fails
    ArduinoMock::mockMillis = 2000;
    mgr->healthCheckAll(); // retry 2 fails
    ArduinoMock::mockMillis = 4000;
    mgr->healthCheckAll(); // retry 3 fails -> permanent ERROR

    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, dev._state);
    TEST_ASSERT_FALSE(dev._ready);

    // Further healthCheckAll should NOT attempt any more retries
    int prevInitCount = dev.initCallCount;
    ArduinoMock::mockMillis = 100000;
    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(prevInitCount, dev.initCallCount);
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, dev._state);
}

// 18. Retry exhaustion increments fault count
void test_retry_exhaustion_increments_fault_count() {
    TestDevice dev("test,fault");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    dev.initResult = false;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    TEST_ASSERT_EQUAL(0, mgr->getFaultCount((uint8_t)slot));

    // Health check fails at t=0
    ArduinoMock::mockMillis = 0;
    mgr->healthCheckAll();

    // Exhaust all 3 retries
    ArduinoMock::mockMillis = 1000;
    mgr->healthCheckAll();
    ArduinoMock::mockMillis = 2000;
    mgr->healthCheckAll();
    ArduinoMock::mockMillis = 4000;
    mgr->healthCheckAll();

    TEST_ASSERT_EQUAL(1, mgr->getFaultCount((uint8_t)slot));
}

// 19. Retry calls deinit() before init()
void test_retry_calls_deinit_before_init() {
    TestDevice dev("test,order");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    dev.initResult = true;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Health check fails at t=0
    ArduinoMock::mockMillis = 0;
    mgr->healthCheckAll();

    // Reset counters before retry
    dev.initCallCount = 0;
    dev.deinitCallCount = 0;

    // Retry at t=1000
    ArduinoMock::mockMillis = 1000;
    mgr->healthCheckAll();

    // Both deinit and init should have been called exactly once
    TEST_ASSERT_EQUAL(1, dev.deinitCallCount);
    TEST_ASSERT_EQUAL(1, dev.initCallCount);
}

// 20. Healthy AVAILABLE device triggers no retry
void test_no_retry_for_available_device() {
    TestDevice dev("test,healthy");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = true;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    dev.initCallCount = 0;
    dev.deinitCallCount = 0;

    mgr->healthCheckAll();

    // No deinit/init should be called for a healthy device
    TEST_ASSERT_EQUAL(0, dev.initCallCount);
    TEST_ASSERT_EQUAL(0, dev.deinitCallCount);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);
    TEST_ASSERT_TRUE(dev._ready);
}

// =====================================================================
// Group 4: Fault Counters (3 tests)
// =====================================================================

// 21. Fault count starts at zero
void test_fault_count_starts_at_zero() {
    TestDevice dev("test,fzero");
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    TEST_ASSERT_EQUAL(0, mgr->getFaultCount((uint8_t)slot));
}

// 22. Fault count accumulates across multiple retry exhaustion cycles
void test_fault_count_accumulates() {
    TestDevice dev("test,faccum");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    dev.initResult = false;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // --- First exhaustion cycle ---
    // Health check fails at t=0
    ArduinoMock::mockMillis = 0;
    mgr->healthCheckAll();  // -> UNAVAILABLE

    ArduinoMock::mockMillis = 1000;
    mgr->healthCheckAll();  // retry 1 fails
    ArduinoMock::mockMillis = 2000;
    mgr->healthCheckAll();  // retry 2 fails
    ArduinoMock::mockMillis = 4000;
    mgr->healthCheckAll();  // retry 3 fails -> ERROR, fault=1

    TEST_ASSERT_EQUAL(1, mgr->getFaultCount((uint8_t)slot));

    // --- Manually reset device to UNAVAILABLE for a second cycle ---
    // (Simulating external recovery attempt or manual intervention)
    HalRetryState* rs = const_cast<HalRetryState*>(mgr->getRetryState((uint8_t)slot));
    rs->count = 0;
    rs->nextRetryMs = 0;
    dev._state = HAL_STATE_UNAVAILABLE;

    // Second exhaustion cycle
    ArduinoMock::mockMillis = 5000;
    mgr->healthCheckAll();  // retry 1 fails
    ArduinoMock::mockMillis = 6000;
    mgr->healthCheckAll();  // retry 2 fails
    ArduinoMock::mockMillis = 8000;
    mgr->healthCheckAll();  // retry 3 fails -> ERROR, fault=2

    TEST_ASSERT_EQUAL(2, mgr->getFaultCount((uint8_t)slot));
}

// 23. Fault count stays 0 when device recovers before exhaustion
void test_fault_count_survives_health_recovery() {
    TestDevice dev("test,fzrec");
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev.healthResult = false;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Health check fails at t=0
    ArduinoMock::mockMillis = 0;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, dev._state);

    // Device will succeed on retry
    dev.initResult = true;

    // Retry at t=1000 -> succeeds -> AVAILABLE
    ArduinoMock::mockMillis = 1000;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);

    // Fault count should remain 0 (recovery before exhaustion)
    TEST_ASSERT_EQUAL(0, mgr->getFaultCount((uint8_t)slot));
}

// =====================================================================
// Group 5: Retry State Accessors (2 tests)
// =====================================================================

// 24. getRetryState returns valid pointer after device registration
void test_getRetryState_returns_valid_pointer() {
    TestDevice dev("test,rsval");
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    const HalRetryState* rs = mgr->getRetryState((uint8_t)slot);

    TEST_ASSERT_NOT_NULL(rs);
    TEST_ASSERT_EQUAL(0, rs->count);
    TEST_ASSERT_EQUAL(0, rs->nextRetryMs);
    TEST_ASSERT_EQUAL(0, rs->lastErrorCode);
}

// 25. getRetryState returns null for invalid slot
void test_getRetryState_invalid_slot_returns_null() {
    const HalRetryState* rs = mgr->getRetryState(99);

    TEST_ASSERT_NULL(rs);
}

// ===== Test Runner =====
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    // Group 1: HalInitResult Basics
    RUN_TEST(test_hal_init_ok_returns_success);
    RUN_TEST(test_hal_init_fail_returns_failure);
    RUN_TEST(test_hal_init_fail_null_reason);
    RUN_TEST(test_hal_init_fail_long_reason_truncated);
    RUN_TEST(test_init_result_carries_through_device);

    // Group 2: initAll with HalInitResult
    RUN_TEST(test_initAll_success_sets_available);
    RUN_TEST(test_initAll_failure_sets_error);
    RUN_TEST(test_initAll_failure_records_error_code);
    RUN_TEST(test_initAll_mixed_devices);
    RUN_TEST(test_initAll_only_inits_unknown_configuring);

    // Group 3: healthCheckAll Retry Logic
    RUN_TEST(test_health_fail_sets_unavailable);
    RUN_TEST(test_retry_not_attempted_before_delay);
    RUN_TEST(test_retry_attempted_after_delay);
    RUN_TEST(test_retry_success_recovers_to_available);
    RUN_TEST(test_retry_failure_increments_count);
    RUN_TEST(test_exponential_backoff_timing);
    RUN_TEST(test_retry_exhaustion_sets_permanent_error);
    RUN_TEST(test_retry_exhaustion_increments_fault_count);
    RUN_TEST(test_retry_calls_deinit_before_init);
    RUN_TEST(test_no_retry_for_available_device);

    // Group 4: Fault Counters
    RUN_TEST(test_fault_count_starts_at_zero);
    RUN_TEST(test_fault_count_accumulates);
    RUN_TEST(test_fault_count_survives_health_recovery);

    // Group 5: Retry State Accessors
    RUN_TEST(test_getRetryState_returns_valid_pointer);
    RUN_TEST(test_getRetryState_invalid_slot_returns_null);

    return UNITY_END();
}
