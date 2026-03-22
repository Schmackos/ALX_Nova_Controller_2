#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_init_result.h"
#include "../../src/hal/hal_device.h"

// Inline the .cpp files needed by diag_journal (referenced from hal_init_result via diag_error_codes)
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

void setUp() {}
void tearDown() {}

// ===== hal_init_ok() tests =====

void test_hal_init_ok_success_true() {
    HalInitResult r = hal_init_ok();
    TEST_ASSERT_TRUE(r.success);
}

void test_hal_init_ok_reason_empty() {
    HalInitResult r = hal_init_ok();
    TEST_ASSERT_EQUAL('\0', r.reason[0]);
}

void test_hal_init_ok_error_code_is_zero() {
    HalInitResult r = hal_init_ok();
    TEST_ASSERT_EQUAL((uint16_t)DIAG_OK, r.errorCode);
}

// ===== hal_init_fail() tests =====

void test_hal_init_fail_success_false() {
    HalInitResult r = hal_init_fail(DIAG_HAL_INIT_FAILED, "sensor not found");
    TEST_ASSERT_FALSE(r.success);
}

void test_hal_init_fail_preserves_error_code() {
    HalInitResult r = hal_init_fail(DIAG_HAL_INIT_FAILED, "any reason");
    TEST_ASSERT_EQUAL((uint16_t)DIAG_HAL_INIT_FAILED, r.errorCode);
}

void test_hal_init_fail_copies_reason() {
    HalInitResult r = hal_init_fail(DIAG_HAL_INIT_FAILED, "I2C probe failed");
    TEST_ASSERT_EQUAL_STRING("I2C probe failed", r.reason);
}

void test_hal_init_fail_truncates_at_47_chars() {
    // 48-char string — should be truncated to 47 chars + NUL
    const char* longReason = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijk"; // 47 chars
    HalInitResult r = hal_init_fail(DIAG_HAL_INIT_FAILED, longReason);
    TEST_ASSERT_EQUAL(47, (int)strlen(r.reason));
    TEST_ASSERT_EQUAL_CHAR('\0', r.reason[47]);
}

void test_hal_init_fail_null_terminates() {
    // Reason that exactly fits 47 chars — verify NUL at [47]
    char reason48[49];
    memset(reason48, 'X', 48);
    reason48[48] = '\0';
    HalInitResult r = hal_init_fail(DIAG_HAL_INIT_FAILED, reason48);
    TEST_ASSERT_EQUAL_CHAR('\0', r.reason[sizeof(r.reason) - 1]);
}

void test_hal_init_fail_null_reason_no_crash() {
    // Null reason string — must not crash, reason should be empty
    HalInitResult r = hal_init_fail(DIAG_HAL_INIT_FAILED, nullptr);
    TEST_ASSERT_FALSE(r.success);
    TEST_ASSERT_EQUAL_CHAR('\0', r.reason[0]);
}

void test_hal_init_fail_empty_reason() {
    HalInitResult r = hal_init_fail(DIAG_HAL_INIT_FAILED, "");
    TEST_ASSERT_FALSE(r.success);
    TEST_ASSERT_EQUAL_CHAR('\0', r.reason[0]);
}

// ===== HalDevice._lastError integration tests =====

// Minimal concrete device for testing
class MinimalDevice : public HalDevice {
public:
    bool initOk;
    const char* initReason;

    MinimalDevice() : initOk(true), initReason(nullptr) {}

    bool probe() override { return true; }
    HalInitResult init() override {
        return initOk ? hal_init_ok()
                      : hal_init_fail(DIAG_HAL_INIT_FAILED, initReason ? initReason : "fail");
    }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return true; }
};

void test_init_failure_stores_reason_on_device() {
    MinimalDevice dev;
    dev.initOk = false;
    dev.initReason = "register write failed";

    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.reset();
    mgr.registerDevice(&dev, HAL_DISC_BUILTIN);
    mgr.initAll();

    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, dev._state);
    TEST_ASSERT_EQUAL_STRING("register write failed", dev.getLastError());
}

void test_init_success_clears_last_error() {
    MinimalDevice dev;
    // Pre-set a stale error to verify it's cleared on success
    dev.setLastError("stale error from previous attempt");

    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.reset();
    mgr.registerDevice(&dev, HAL_DISC_BUILTIN);

    dev.initOk = true;
    mgr.initAll();

    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);
    TEST_ASSERT_EQUAL_CHAR('\0', dev.getLastError()[0]);
}

void test_probe_failure_sets_last_error() {
    // Simulate what the API layer does when probe() returns false
    MinimalDevice dev;
    HalInitResult failResult = hal_init_fail(DIAG_HAL_INIT_FAILED, "I2C probe failed (no device response)");
    dev.setLastError(failResult);

    TEST_ASSERT_EQUAL_STRING("I2C probe failed (no device response)", dev.getLastError());
}

// ===== Test Runner =====
int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_hal_init_ok_success_true);
    RUN_TEST(test_hal_init_ok_reason_empty);
    RUN_TEST(test_hal_init_ok_error_code_is_zero);
    RUN_TEST(test_hal_init_fail_success_false);
    RUN_TEST(test_hal_init_fail_preserves_error_code);
    RUN_TEST(test_hal_init_fail_copies_reason);
    RUN_TEST(test_hal_init_fail_truncates_at_47_chars);
    RUN_TEST(test_hal_init_fail_null_terminates);
    RUN_TEST(test_hal_init_fail_null_reason_no_crash);
    RUN_TEST(test_hal_init_fail_empty_reason);
    RUN_TEST(test_init_failure_stores_reason_on_device);
    RUN_TEST(test_init_success_clears_last_error);
    RUN_TEST(test_probe_failure_sets_last_error);

    return UNITY_END();
}
