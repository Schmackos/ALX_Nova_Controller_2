#include <unity.h>

// config.h already handles the NATIVE_TEST mock include path
#include "../../src/config.h"

void setUp(void) {}
void tearDown(void) {}

// ===== Heap Threshold Sanity Checks =====
// These are compile-time constants from config.h. The tests verify the
// hierarchy is internally consistent so that code using them behaves correctly.

// Test 1: All thresholds are positive (non-zero)
void test_heap_constants_all_positive(void) {
    TEST_ASSERT_GREATER_THAN(0, HEAP_CRITICAL_THRESHOLD_BYTES);
    TEST_ASSERT_GREATER_THAN(0, HEAP_WARNING_THRESHOLD_BYTES);
    TEST_ASSERT_GREATER_THAN(0, HEAP_TLS_MIN_THRESHOLD_BYTES);
    TEST_ASSERT_GREATER_THAN(0, HEAP_TLS_SECURE_THRESHOLD_BYTES);
    TEST_ASSERT_GREATER_THAN(0, HEAP_OTA_ABORT_THRESHOLD_BYTES);
    TEST_ASSERT_GREATER_THAN(0, HEAP_WIFI_RESERVE_BYTES);
}

// Test 2: WARNING threshold is above CRITICAL threshold (60KB > 40KB)
void test_heap_warning_above_critical(void) {
    TEST_ASSERT_GREATER_THAN(HEAP_CRITICAL_THRESHOLD_BYTES, HEAP_WARNING_THRESHOLD_BYTES);
}

// Test 3: CRITICAL threshold is above OTA_ABORT threshold (40KB > 10KB)
void test_heap_critical_above_ota_abort(void) {
    TEST_ASSERT_GREATER_THAN(HEAP_OTA_ABORT_THRESHOLD_BYTES, HEAP_CRITICAL_THRESHOLD_BYTES);
}

// Test 4: OTA_ABORT is strictly less than CRITICAL (task requirement)
void test_heap_ota_abort_less_than_critical(void) {
    TEST_ASSERT_LESS_THAN(HEAP_CRITICAL_THRESHOLD_BYTES, HEAP_OTA_ABORT_THRESHOLD_BYTES + 1);
}

// Test 5: TLS_MIN threshold is below CRITICAL (30KB < 40KB)
// i.e. TLS handshakes can fail even before WiFi RX starts dropping
void test_heap_tls_min_below_critical(void) {
    TEST_ASSERT_LESS_THAN(HEAP_CRITICAL_THRESHOLD_BYTES, HEAP_TLS_MIN_THRESHOLD_BYTES);
}

// Test 6: TLS_SECURE threshold is between CRITICAL and WARNING (50KB)
void test_heap_tls_secure_between_critical_and_warning(void) {
    TEST_ASSERT_GREATER_THAN(HEAP_CRITICAL_THRESHOLD_BYTES, HEAP_TLS_SECURE_THRESHOLD_BYTES);
    TEST_ASSERT_LESS_THAN(HEAP_WARNING_THRESHOLD_BYTES, HEAP_TLS_SECURE_THRESHOLD_BYTES);
}

// Test 7: WIFI_RESERVE equals CRITICAL (both represent the 40KB WiFi floor)
void test_heap_wifi_reserve_equals_critical(void) {
    TEST_ASSERT_EQUAL(HEAP_CRITICAL_THRESHOLD_BYTES, HEAP_WIFI_RESERVE_BYTES);
}

// Test 8: Thresholds are plausible for an ESP32-S3 with 512KB SRAM
// (not larger than total internal SRAM)
void test_heap_thresholds_within_esp32_sram(void) {
    const int ESP32_S3_INTERNAL_SRAM = 512 * 1024; // 512 KB
    TEST_ASSERT_LESS_THAN(ESP32_S3_INTERNAL_SRAM, HEAP_WARNING_THRESHOLD_BYTES);
}

// ===== Test Runner =====

int runUnityTests(void) {
    UNITY_BEGIN();

    RUN_TEST(test_heap_constants_all_positive);
    RUN_TEST(test_heap_warning_above_critical);
    RUN_TEST(test_heap_critical_above_ota_abort);
    RUN_TEST(test_heap_ota_abort_less_than_critical);
    RUN_TEST(test_heap_tls_min_below_critical);
    RUN_TEST(test_heap_tls_secure_between_critical_and_warning);
    RUN_TEST(test_heap_wifi_reserve_equals_critical);
    RUN_TEST(test_heap_thresholds_within_esp32_sram);

    return UNITY_END();
}

#ifdef NATIVE_TEST
int main(void) {
    return runUnityTests();
}
#endif

#ifndef NATIVE_TEST
void setup() {
    delay(2000);
    runUnityTests();
}

void loop() {}
#endif
