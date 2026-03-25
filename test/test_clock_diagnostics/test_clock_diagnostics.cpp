/**
 * test_clock_diagnostics.cpp
 *
 * Unit tests for clock quality diagnostics feature:
 *
 *   1. ClockStatus struct defaults and field assignment
 *   2. HalDevice base class getClockStatus() returns not-available
 *   3. HAL_CAP_DPLL capability bit — position, no overlap, fits uint16_t
 *   4. Health check integration — locked/unlocked/error scenarios
 *   5. Edge cases — device without cap skipped, I2C failure graceful
 *
 * Runs on the native platform (no hardware needed).
 */

#include <unity.h>
#include <cstring>
#include <cstdint>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Pull in production headers
#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_init_result.h"
#include "../../src/diag_error_codes.h"
#include "../../src/app_events.h"

// ============================================================
// Section 1: ClockStatus struct — defaults and assignment
// ============================================================

void test_clock_status_default_construction() {
    ClockStatus cs = {};
    TEST_ASSERT_FALSE(cs.available);
    TEST_ASSERT_FALSE(cs.locked);
    TEST_ASSERT_EQUAL_STRING("", cs.description);
}

void test_clock_status_not_available_initializer() {
    // The canonical "not available" status
    ClockStatus cs;
    cs.available = false;
    cs.locked = false;
    strncpy(cs.description, "N/A", sizeof(cs.description) - 1);
    cs.description[sizeof(cs.description) - 1] = '\0';

    TEST_ASSERT_FALSE(cs.available);
    TEST_ASSERT_FALSE(cs.locked);
    TEST_ASSERT_EQUAL_STRING("N/A", cs.description);
}

void test_clock_status_locked_state() {
    ClockStatus cs;
    cs.available = true;
    cs.locked = true;
    strncpy(cs.description, "DPLL locked 48kHz", sizeof(cs.description) - 1);
    cs.description[sizeof(cs.description) - 1] = '\0';

    TEST_ASSERT_TRUE(cs.available);
    TEST_ASSERT_TRUE(cs.locked);
    TEST_ASSERT_EQUAL_STRING("DPLL locked 48kHz", cs.description);
}

void test_clock_status_unlocked_state() {
    ClockStatus cs;
    cs.available = true;
    cs.locked = false;
    strncpy(cs.description, "DPLL unlocked", sizeof(cs.description) - 1);
    cs.description[sizeof(cs.description) - 1] = '\0';

    TEST_ASSERT_TRUE(cs.available);
    TEST_ASSERT_FALSE(cs.locked);
    TEST_ASSERT_EQUAL_STRING("DPLL unlocked", cs.description);
}

void test_clock_status_description_max_length() {
    ClockStatus cs;
    cs.available = true;
    cs.locked = true;
    // Fill description to maximum capacity
    memset(cs.description, 'A', sizeof(cs.description) - 1);
    cs.description[sizeof(cs.description) - 1] = '\0';

    TEST_ASSERT_EQUAL(strlen(cs.description), sizeof(cs.description) - 1);
}

// ============================================================
// Section 2: HalDevice base class getClockStatus() default
// ============================================================

// Minimal concrete device for testing base class behavior
class MinimalTestDevice : public HalDevice {
public:
    bool probeResult = true;
    bool initResult = true;
    bool healthResult = true;

    MinimalTestDevice() {
        strncpy(_descriptor.compatible, "test,minimal", 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = HAL_DEV_DAC;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }
    void setCapabilities(uint32_t caps) { _descriptor.capabilities = caps; }

    bool probe() override { return probeResult; }
    HalInitResult init() override {
        if (initResult) return hal_init_ok();
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return healthResult; }
};

void test_base_device_get_clock_status_returns_not_available() {
    MinimalTestDevice dev;
    ClockStatus cs = dev.getClockStatus();
    TEST_ASSERT_FALSE(cs.available);
    TEST_ASSERT_FALSE(cs.locked);
}

// Device that overrides getClockStatus with locked clock
class ClockCapableDevice : public MinimalTestDevice {
public:
    bool clockLocked = true;
    bool clockReadError = false;

    ClockCapableDevice() {
        _descriptor.capabilities = HAL_CAP_DPLL;
    }

    ClockStatus getClockStatus() override {
        ClockStatus cs;
        if (clockReadError) {
            cs.available = true;
            cs.locked = false;
            strncpy(cs.description, "I2C read error", sizeof(cs.description) - 1);
            cs.description[sizeof(cs.description) - 1] = '\0';
            return cs;
        }
        cs.available = true;
        cs.locked = clockLocked;
        if (clockLocked) {
            strncpy(cs.description, "DPLL locked 48kHz", sizeof(cs.description) - 1);
        } else {
            strncpy(cs.description, "DPLL unlocked", sizeof(cs.description) - 1);
        }
        cs.description[sizeof(cs.description) - 1] = '\0';
        return cs;
    }
};

void test_overridden_clock_status_locked() {
    ClockCapableDevice dev;
    dev.clockLocked = true;
    ClockStatus cs = dev.getClockStatus();
    TEST_ASSERT_TRUE(cs.available);
    TEST_ASSERT_TRUE(cs.locked);
    TEST_ASSERT_EQUAL_STRING("DPLL locked 48kHz", cs.description);
}

void test_overridden_clock_status_unlocked() {
    ClockCapableDevice dev;
    dev.clockLocked = false;
    ClockStatus cs = dev.getClockStatus();
    TEST_ASSERT_TRUE(cs.available);
    TEST_ASSERT_FALSE(cs.locked);
    TEST_ASSERT_EQUAL_STRING("DPLL unlocked", cs.description);
}

void test_overridden_clock_status_i2c_read_error() {
    ClockCapableDevice dev;
    dev.clockReadError = true;
    ClockStatus cs = dev.getClockStatus();
    TEST_ASSERT_TRUE(cs.available);
    TEST_ASSERT_FALSE(cs.locked);
    TEST_ASSERT_EQUAL_STRING("I2C read error", cs.description);
}

// ============================================================
// Section 3: HAL_CAP_DPLL capability bit
// ============================================================

void test_hal_cap_dpll_is_bit15() {
    TEST_ASSERT_EQUAL_UINT32((1 << 15), HAL_CAP_DPLL);
}

void test_hal_cap_dpll_no_overlap_with_other_caps() {
    uint32_t all_other_caps =
        HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE |
        HAL_CAP_ADC_PATH  | HAL_CAP_DAC_PATH | HAL_CAP_PGA_CONTROL |
        HAL_CAP_HPF_CONTROL | HAL_CAP_CODEC | HAL_CAP_MQA |
        HAL_CAP_LINE_DRIVER | HAL_CAP_APLL | HAL_CAP_DSD |
        HAL_CAP_HP_AMP | HAL_CAP_POWER_MGMT | HAL_CAP_ASRC;
    TEST_ASSERT_EQUAL_UINT32(0, HAL_CAP_DPLL & all_other_caps);
}

void test_hal_cap_dpll_fits_in_uint32() {
    // capabilities field is uint32_t — HAL_CAP_DPLL (bit 15) must fit
    uint32_t cap = HAL_CAP_DPLL;
    TEST_ASSERT_NOT_EQUAL(0u, cap);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)HAL_CAP_DPLL, cap);
}

void test_hal_cap_dpll_distinct_from_apll() {
    // APLL (bit 10) and DPLL (bit 15) are different clock types
    TEST_ASSERT_NOT_EQUAL(HAL_CAP_APLL, HAL_CAP_DPLL);
    TEST_ASSERT_EQUAL_UINT32(0, HAL_CAP_APLL & HAL_CAP_DPLL);
}

void test_hal_cap_dpll_distinct_from_asrc() {
    // ASRC (bit 14) and DPLL (bit 15) are adjacent but distinct
    TEST_ASSERT_NOT_EQUAL(HAL_CAP_ASRC, HAL_CAP_DPLL);
    TEST_ASSERT_EQUAL_UINT32(0, HAL_CAP_ASRC & HAL_CAP_DPLL);
}

void test_device_capabilities_can_combine_dpll_with_others() {
    uint32_t caps = HAL_CAP_HW_VOLUME | HAL_CAP_DAC_PATH | HAL_CAP_DPLL;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DPLL);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_FALSE(caps & HAL_CAP_DSD);
}

// ============================================================
// Section 4: Health check integration scenarios
// ============================================================

// Simulate health check clock evaluation logic:
// For each device with HAL_CAP_DPLL, call getClockStatus().
// If locked: PASS. If unlocked: WARN. If read error (available but !locked with error desc): WARN.
// Device without HAL_CAP_DPLL: SKIP.

enum ClockHealthResult { CLK_PASS, CLK_WARN, CLK_SKIP, CLK_ERROR };

static ClockHealthResult evaluate_clock_health(HalDevice* dev) {
    if (!(dev->getDescriptor().capabilities & HAL_CAP_DPLL)) {
        return CLK_SKIP;
    }
    ClockStatus cs = dev->getClockStatus();
    if (!cs.available) {
        return CLK_SKIP;
    }
    if (cs.locked) {
        return CLK_PASS;
    }
    return CLK_WARN;
}

void test_health_clock_device_without_cap_returns_skip() {
    MinimalTestDevice dev;
    // No HAL_CAP_DPLL set
    TEST_ASSERT_EQUAL(CLK_SKIP, evaluate_clock_health(&dev));
}

void test_health_clock_device_locked_returns_pass() {
    ClockCapableDevice dev;
    dev.clockLocked = true;
    TEST_ASSERT_EQUAL(CLK_PASS, evaluate_clock_health(&dev));
}

void test_health_clock_device_unlocked_returns_warn() {
    ClockCapableDevice dev;
    dev.clockLocked = false;
    TEST_ASSERT_EQUAL(CLK_WARN, evaluate_clock_health(&dev));
}

void test_health_clock_device_read_error_returns_warn() {
    ClockCapableDevice dev;
    dev.clockReadError = true;
    TEST_ASSERT_EQUAL(CLK_WARN, evaluate_clock_health(&dev));
}

void test_health_all_clocks_locked_is_healthy() {
    ClockCapableDevice dev1;
    ClockCapableDevice dev2;
    dev1.clockLocked = true;
    dev2.clockLocked = true;

    HalDevice* devices[] = {&dev1, &dev2};
    int passCount = 0, warnCount = 0;
    for (int i = 0; i < 2; i++) {
        ClockHealthResult r = evaluate_clock_health(devices[i]);
        if (r == CLK_PASS) passCount++;
        if (r == CLK_WARN) warnCount++;
    }
    TEST_ASSERT_EQUAL(2, passCount);
    TEST_ASSERT_EQUAL(0, warnCount);
}

void test_health_one_unlocked_produces_warning() {
    ClockCapableDevice dev1;
    ClockCapableDevice dev2;
    dev1.clockLocked = true;
    dev2.clockLocked = false;

    HalDevice* devices[] = {&dev1, &dev2};
    int passCount = 0, warnCount = 0;
    for (int i = 0; i < 2; i++) {
        ClockHealthResult r = evaluate_clock_health(devices[i]);
        if (r == CLK_PASS) passCount++;
        if (r == CLK_WARN) warnCount++;
    }
    TEST_ASSERT_EQUAL(1, passCount);
    TEST_ASSERT_EQUAL(1, warnCount);
}

void test_health_mixed_devices_with_and_without_clock_cap() {
    MinimalTestDevice devNoCap;        // No HAL_CAP_DPLL
    ClockCapableDevice devLocked;      // HAL_CAP_DPLL, locked
    ClockCapableDevice devUnlocked;    // HAL_CAP_DPLL, unlocked
    devLocked.clockLocked = true;
    devUnlocked.clockLocked = false;

    HalDevice* devices[] = {&devNoCap, &devLocked, &devUnlocked};
    int passCount = 0, warnCount = 0, skipCount = 0;
    for (int i = 0; i < 3; i++) {
        ClockHealthResult r = evaluate_clock_health(devices[i]);
        if (r == CLK_PASS) passCount++;
        if (r == CLK_WARN) warnCount++;
        if (r == CLK_SKIP) skipCount++;
    }
    TEST_ASSERT_EQUAL(1, passCount);
    TEST_ASSERT_EQUAL(1, warnCount);
    TEST_ASSERT_EQUAL(1, skipCount);
}

// ============================================================
// Section 5: Edge cases
// ============================================================

void test_clock_status_description_truncation() {
    ClockStatus cs;
    cs.available = true;
    cs.locked = true;
    // Write a very long string — must be safely truncated
    const char* longDesc = "This is a very long clock description that exceeds the buffer size";
    strncpy(cs.description, longDesc, sizeof(cs.description) - 1);
    cs.description[sizeof(cs.description) - 1] = '\0';
    // Should not overflow
    TEST_ASSERT_LESS_OR_EQUAL(sizeof(cs.description) - 1, strlen(cs.description));
}

void test_device_with_dpll_cap_but_no_override_returns_not_available() {
    // A device that has HAL_CAP_DPLL in capabilities but does NOT override
    // getClockStatus() — uses base class default which returns not available
    MinimalTestDevice dev;
    dev.setCapabilities(HAL_CAP_DPLL);
    ClockStatus cs = dev.getClockStatus();
    TEST_ASSERT_FALSE(cs.available);
}

void test_clock_status_multiple_transitions() {
    ClockCapableDevice dev;

    // Start locked
    dev.clockLocked = true;
    dev.clockReadError = false;
    ClockStatus cs1 = dev.getClockStatus();
    TEST_ASSERT_TRUE(cs1.locked);

    // Transition to unlocked
    dev.clockLocked = false;
    ClockStatus cs2 = dev.getClockStatus();
    TEST_ASSERT_FALSE(cs2.locked);

    // Transition to error
    dev.clockReadError = true;
    ClockStatus cs3 = dev.getClockStatus();
    TEST_ASSERT_FALSE(cs3.locked);
    TEST_ASSERT_EQUAL_STRING("I2C read error", cs3.description);

    // Recover back to locked
    dev.clockReadError = false;
    dev.clockLocked = true;
    ClockStatus cs4 = dev.getClockStatus();
    TEST_ASSERT_TRUE(cs4.locked);
}

// ============================================================
// setUp / tearDown
// ============================================================

void setUp() {
    ArduinoMock::reset();
}

void tearDown() {}

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Section 1: ClockStatus struct
    RUN_TEST(test_clock_status_default_construction);
    RUN_TEST(test_clock_status_not_available_initializer);
    RUN_TEST(test_clock_status_locked_state);
    RUN_TEST(test_clock_status_unlocked_state);
    RUN_TEST(test_clock_status_description_max_length);

    // Section 2: HalDevice base class
    RUN_TEST(test_base_device_get_clock_status_returns_not_available);
    RUN_TEST(test_overridden_clock_status_locked);
    RUN_TEST(test_overridden_clock_status_unlocked);
    RUN_TEST(test_overridden_clock_status_i2c_read_error);

    // Section 3: HAL_CAP_DPLL capability bit
    RUN_TEST(test_hal_cap_dpll_is_bit15);
    RUN_TEST(test_hal_cap_dpll_no_overlap_with_other_caps);
    RUN_TEST(test_hal_cap_dpll_fits_in_uint32);
    RUN_TEST(test_hal_cap_dpll_distinct_from_apll);
    RUN_TEST(test_hal_cap_dpll_distinct_from_asrc);
    RUN_TEST(test_device_capabilities_can_combine_dpll_with_others);

    // Section 4: Health check integration
    RUN_TEST(test_health_clock_device_without_cap_returns_skip);
    RUN_TEST(test_health_clock_device_locked_returns_pass);
    RUN_TEST(test_health_clock_device_unlocked_returns_warn);
    RUN_TEST(test_health_clock_device_read_error_returns_warn);
    RUN_TEST(test_health_all_clocks_locked_is_healthy);
    RUN_TEST(test_health_one_unlocked_produces_warning);
    RUN_TEST(test_health_mixed_devices_with_and_without_clock_cap);

    // Section 5: Edge cases
    RUN_TEST(test_clock_status_description_truncation);
    RUN_TEST(test_device_with_dpll_cap_but_no_override_returns_not_available);
    RUN_TEST(test_clock_status_multiple_transitions);

    return UNITY_END();
}
