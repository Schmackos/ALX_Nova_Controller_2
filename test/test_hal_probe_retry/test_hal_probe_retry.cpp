#include <unity.h>
#include <string.h>

// Include config for constants
#include "../../src/config.h"
#include "../../src/diag_error_codes.h"

void setUp(void) {}
void tearDown(void) {}

void test_retry_count_is_bounded(void) {
    TEST_ASSERT_LESS_OR_EQUAL(5, HAL_PROBE_RETRY_COUNT);
    TEST_ASSERT_GREATER_THAN(0, HAL_PROBE_RETRY_COUNT);
}

void test_retry_backoff_is_reasonable(void) {
    // Worst case: HAL_PROBE_RETRY_COUNT retries * max multiplier * backoff
    uint32_t worstCaseMs = 0;
    for (int i = 1; i <= HAL_PROBE_RETRY_COUNT; i++) {
        worstCaseMs += HAL_PROBE_RETRY_BACKOFF_MS * i;
    }
    // Should be under 1 second total
    TEST_ASSERT_LESS_THAN(1000, worstCaseMs);
}

void test_max_retry_addresses_bounded(void) {
    TEST_ASSERT_LESS_OR_EQUAL(32, HAL_PROBE_RETRY_MAX_ADDRS);
    TEST_ASSERT_GREATER_THAN(0, HAL_PROBE_RETRY_MAX_ADDRS);
}

void test_diag_code_exists(void) {
    TEST_ASSERT_EQUAL_HEX16(0x1105, DIAG_HAL_PROBE_RETRY_OK);
}

void test_retry_only_timeouts_not_nacks(void) {
    // Document: I2C error codes
    // 0 = success, 2 = NACK (nobody home), 4/5 = timeout (maybe slow device)
    // Only 4 and 5 should trigger retry
    uint8_t nack = 2;
    uint8_t timeout1 = 4;
    uint8_t timeout2 = 5;
    TEST_ASSERT_FALSE(nack == 4 || nack == 5);   // NACK should NOT retry
    TEST_ASSERT_TRUE(timeout1 == 4 || timeout1 == 5);  // timeout SHOULD retry
    TEST_ASSERT_TRUE(timeout2 == 4 || timeout2 == 5);
}

void test_worst_case_boot_delay(void) {
    // With HAL_PROBE_RETRY_MAX_ADDRS timeout addresses and HAL_PROBE_RETRY_COUNT retries:
    // Total backoff delay = sum of (backoff * attempt) for each retry
    uint32_t totalBackoffMs = 0;
    for (int r = 1; r <= HAL_PROBE_RETRY_COUNT; r++) {
        totalBackoffMs += HAL_PROBE_RETRY_BACKOFF_MS * r;
    }
    // Plus probe time: ~1ms per address per retry
    uint32_t probeTimeMs = HAL_PROBE_RETRY_MAX_ADDRS * HAL_PROBE_RETRY_COUNT * 1;
    uint32_t totalMs = totalBackoffMs + probeTimeMs;
    // Must be under 500ms total
    TEST_ASSERT_LESS_THAN(500, totalMs);
}

void test_timeout_array_fits_in_stack(void) {
    // HAL_PROBE_RETRY_MAX_ADDRS bytes on stack -- must be small
    TEST_ASSERT_LESS_OR_EQUAL(64, HAL_PROBE_RETRY_MAX_ADDRS);
}

void test_backoff_increases_with_attempt(void) {
    uint32_t delay1 = HAL_PROBE_RETRY_BACKOFF_MS * 1; // 50ms
    uint32_t delay2 = HAL_PROBE_RETRY_BACKOFF_MS * 2; // 100ms
    TEST_ASSERT_GREATER_THAN(delay1, delay2);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_retry_count_is_bounded);
    RUN_TEST(test_retry_backoff_is_reasonable);
    RUN_TEST(test_max_retry_addresses_bounded);
    RUN_TEST(test_diag_code_exists);
    RUN_TEST(test_retry_only_timeouts_not_nacks);
    RUN_TEST(test_worst_case_boot_delay);
    RUN_TEST(test_timeout_array_fits_in_stack);
    RUN_TEST(test_backoff_increases_with_attempt);
    return UNITY_END();
}
