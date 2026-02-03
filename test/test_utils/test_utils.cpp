#include <unity.h>
#include <string>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Mock for esp_reset_reason
enum esp_reset_reason_t {
    ESP_RST_POWERON = 1,
    ESP_RST_EXT = 2,
    ESP_RST_SW = 3,
    ESP_RST_PANIC = 4,
    ESP_RST_INT_WDT = 5,
    ESP_RST_TASK_WDT = 6,
    ESP_RST_WDT = 7,
    ESP_RST_DEEPSLEEP = 8,
    ESP_RST_BROWNOUT = 9,
    ESP_RST_SDIO = 10,
};

namespace TestUtilsState {
    esp_reset_reason_t mockResetReason = ESP_RST_POWERON;

    void reset() {
        mockResetReason = ESP_RST_POWERON;
#ifdef NATIVE_TEST
        ArduinoMock::reset();
#endif
    }
}

// Mock esp_reset_reason
esp_reset_reason_t esp_reset_reason() {
    return TestUtilsState::mockResetReason;
}

// ===== UTILS IMPLEMENTATIONS =====

// Compare semantic version strings like "1.0.7" and "1.1.2"
// Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
int compareVersions(const String& v1, const String& v2) {
    int i = 0, j = 0;
    const int n1 = v1.length();
    const int n2 = v2.length();

    while (i < n1 || j < n2) {
        long num1 = 0;
        long num2 = 0;

        while (i < n1 && isDigit(v1[i])) {
            num1 = num1 * 10 + (v1[i] - '0');
            i++;
        }
        // Skip non-digit separators like '.'
        while (i < n1 && !isDigit(v1[i]))
            i++;

        while (j < n2 && isDigit(v2[j])) {
            num2 = num2 * 10 + (v2[j] - '0');
            j++;
        }
        while (j < n2 && !isDigit(v2[j]))
            j++;

        if (num1 < num2)
            return -1;
        if (num1 > num2)
            return 1;
    }

    return 0;
}

// Convert RSSI to signal quality percentage (0-100%)
int rssiToQuality(int rssi) {
    if (rssi <= -100)
        return 0;
    if (rssi >= -50)
        return 100;
    return 2 * (rssi + 100); // Linear scale: -100dBm=0%, -50dBm=100%
}

// Get human-readable reset reason
String getResetReasonString() {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON:
            return "power_on";
        case ESP_RST_EXT:
            return "external_reset";
        case ESP_RST_SW:
            return "software_reset";
        case ESP_RST_PANIC:
            return "exception_panic";
        case ESP_RST_INT_WDT:
            return "interrupt_watchdog";
        case ESP_RST_TASK_WDT:
            return "task_watchdog";
        case ESP_RST_WDT:
            return "other_watchdog";
        case ESP_RST_DEEPSLEEP:
            return "deep_sleep_wake";
        case ESP_RST_BROWNOUT:
            return "brownout";
        case ESP_RST_SDIO:
            return "sdio_reset";
        default:
            return "unknown";
    }
}

// ===== Test Setup/Teardown =====

void setUp(void) {
    TestUtilsState::reset();
}

void tearDown(void) {
    // Clean up after each test
}

// ===== Version Comparison Tests =====

void test_version_comparison_equal(void) {
    TEST_ASSERT_EQUAL(0, compareVersions("1.0.0", "1.0.0"));
    TEST_ASSERT_EQUAL(0, compareVersions("2.5.3", "2.5.3"));
}

void test_version_comparison_less(void) {
    TEST_ASSERT_EQUAL(-1, compareVersions("1.0.0", "1.0.1"));
    TEST_ASSERT_EQUAL(-1, compareVersions("1.0.7", "1.1.0"));
    TEST_ASSERT_EQUAL(-1, compareVersions("0.9.5", "1.0.0"));
}

void test_version_comparison_greater(void) {
    TEST_ASSERT_EQUAL(1, compareVersions("1.0.1", "1.0.0"));
    TEST_ASSERT_EQUAL(1, compareVersions("1.1.0", "1.0.7"));
    TEST_ASSERT_EQUAL(1, compareVersions("2.0.0", "1.9.9"));
}

void test_version_comparison_major_minor_patch(void) {
    // Test major version difference
    TEST_ASSERT_EQUAL(-1, compareVersions("1.5.5", "2.0.0"));
    TEST_ASSERT_EQUAL(1, compareVersions("3.0.0", "2.9.9"));

    // Test minor version difference
    TEST_ASSERT_EQUAL(-1, compareVersions("1.4.9", "1.5.0"));
    TEST_ASSERT_EQUAL(1, compareVersions("1.5.0", "1.4.9"));

    // Test patch version difference
    TEST_ASSERT_EQUAL(-1, compareVersions("1.5.7", "1.5.8"));
    TEST_ASSERT_EQUAL(1, compareVersions("1.5.9", "1.5.8"));
}

// ===== RSSI to Quality Tests =====

void test_rssi_to_quality_boundaries(void) {
    // At and below minimum RSSI
    TEST_ASSERT_EQUAL(0, rssiToQuality(-100));
    TEST_ASSERT_EQUAL(0, rssiToQuality(-101));
    TEST_ASSERT_EQUAL(0, rssiToQuality(-110));

    // At and above maximum RSSI
    TEST_ASSERT_EQUAL(100, rssiToQuality(-50));
    TEST_ASSERT_EQUAL(100, rssiToQuality(-49));
    TEST_ASSERT_EQUAL(100, rssiToQuality(0));
}

void test_rssi_to_quality_linear_scale(void) {
    // Test linear scaling in the middle range
    TEST_ASSERT_EQUAL(50, rssiToQuality(-75));  // Exactly midpoint
    TEST_ASSERT_EQUAL(25, rssiToQuality(-87));  // 25% quality
    TEST_ASSERT_EQUAL(75, rssiToQuality(-62));  // 75% quality
}

// ===== Reset Reason Tests =====

void test_reset_reason_poweron(void) {
    TestUtilsState::mockResetReason = ESP_RST_POWERON;
    TEST_ASSERT_EQUAL_STRING("power_on", getResetReasonString().c_str());
}

void test_reset_reason_external(void) {
    TestUtilsState::mockResetReason = ESP_RST_EXT;
    TEST_ASSERT_EQUAL_STRING("external_reset", getResetReasonString().c_str());
}

void test_reset_reason_software(void) {
    TestUtilsState::mockResetReason = ESP_RST_SW;
    TEST_ASSERT_EQUAL_STRING("software_reset", getResetReasonString().c_str());
}

void test_reset_reason_panic(void) {
    TestUtilsState::mockResetReason = ESP_RST_PANIC;
    TEST_ASSERT_EQUAL_STRING("exception_panic", getResetReasonString().c_str());
}

void test_reset_reason_int_watchdog(void) {
    TestUtilsState::mockResetReason = ESP_RST_INT_WDT;
    TEST_ASSERT_EQUAL_STRING("interrupt_watchdog", getResetReasonString().c_str());
}

void test_reset_reason_task_watchdog(void) {
    TestUtilsState::mockResetReason = ESP_RST_TASK_WDT;
    TEST_ASSERT_EQUAL_STRING("task_watchdog", getResetReasonString().c_str());
}

void test_reset_reason_deepsleep(void) {
    TestUtilsState::mockResetReason = ESP_RST_DEEPSLEEP;
    TEST_ASSERT_EQUAL_STRING("deep_sleep_wake", getResetReasonString().c_str());
}

void test_reset_reason_brownout(void) {
    TestUtilsState::mockResetReason = ESP_RST_BROWNOUT;
    TEST_ASSERT_EQUAL_STRING("brownout", getResetReasonString().c_str());
}

void test_reset_reason_unknown(void) {
    TestUtilsState::mockResetReason = static_cast<esp_reset_reason_t>(255);
    TEST_ASSERT_EQUAL_STRING("unknown", getResetReasonString().c_str());
}

// ===== Test Runner =====

int runUnityTests(void) {
    UNITY_BEGIN();

    // Version comparison tests
    RUN_TEST(test_version_comparison_equal);
    RUN_TEST(test_version_comparison_less);
    RUN_TEST(test_version_comparison_greater);
    RUN_TEST(test_version_comparison_major_minor_patch);

    // RSSI to quality tests
    RUN_TEST(test_rssi_to_quality_boundaries);
    RUN_TEST(test_rssi_to_quality_linear_scale);

    // Reset reason tests
    RUN_TEST(test_reset_reason_poweron);
    RUN_TEST(test_reset_reason_external);
    RUN_TEST(test_reset_reason_software);
    RUN_TEST(test_reset_reason_panic);
    RUN_TEST(test_reset_reason_int_watchdog);
    RUN_TEST(test_reset_reason_task_watchdog);
    RUN_TEST(test_reset_reason_deepsleep);
    RUN_TEST(test_reset_reason_brownout);
    RUN_TEST(test_reset_reason_unknown);

    return UNITY_END();
}

// For native platform
#ifdef NATIVE_TEST
int main(void) {
    return runUnityTests();
}
#endif

// For Arduino platform
#ifndef NATIVE_TEST
void setup() {
    delay(2000);
    runUnityTests();
}

void loop() {
    // Do nothing
}
#endif
