#include <unity.h>
#include "../test_mocks/Arduino.h"
#include "../../src/rate_limiter.h"
#include "../../src/rate_limiter.cpp"

void setUp(void) {
    ArduinoMock::reset();
    ArduinoMock::mockMillis = 1000; // start at t=1000 to avoid ip==0 edge case at t=0
    rate_limit_reset();
}

void tearDown(void) {}

// ===== Basic acceptance =====

void test_under_limit_passes(void) {
    uint32_t ip = 0xC0A80001; // 192.168.0.1
    for (int i = 0; i < RATE_LIMIT_MAX_REQUESTS; i++) {
        TEST_ASSERT_TRUE_MESSAGE(rate_limit_check(ip), "Request within limit should pass");
    }
}

void test_burst_exceeds_limit(void) {
    uint32_t ip = 0xC0A80001;
    for (int i = 0; i < RATE_LIMIT_MAX_REQUESTS; i++) {
        rate_limit_check(ip);
    }
    TEST_ASSERT_FALSE_MESSAGE(rate_limit_check(ip), "31st request should be rejected");
}

// ===== Window rollover =====

void test_window_rollover_resets(void) {
    uint32_t ip = 0xC0A80001;
    // Exhaust the limit
    for (int i = 0; i < RATE_LIMIT_MAX_REQUESTS + 5; i++) {
        rate_limit_check(ip);
    }
    // Advance time past the window
    ArduinoMock::mockMillis += RATE_LIMIT_WINDOW_MS;
    TEST_ASSERT_TRUE_MESSAGE(rate_limit_check(ip), "After window expires, requests should be allowed again");
}

// ===== IP independence =====

void test_different_ips_independent(void) {
    uint32_t ip1 = 0xC0A80001;
    uint32_t ip2 = 0xC0A80002;
    // Exhaust ip1
    for (int i = 0; i < RATE_LIMIT_MAX_REQUESTS + 1; i++) {
        rate_limit_check(ip1);
    }
    // ip2 should still be allowed
    TEST_ASSERT_TRUE_MESSAGE(rate_limit_check(ip2), "Different IP should have independent counter");
}

// ===== Slot eviction =====

void test_slot_eviction(void) {
    // Fill all 8 slots with unique IPs
    for (uint32_t i = 1; i <= RATE_LIMIT_SLOTS; i++) {
        ArduinoMock::mockMillis = 1000 + i; // each at a slightly later time
        rate_limit_check(i);
    }
    // 9th IP should evict oldest (ip=1, windowStart=1001)
    ArduinoMock::mockMillis = 2000;
    uint32_t newIp = 0xAAAAAAAA;
    TEST_ASSERT_TRUE_MESSAGE(rate_limit_check(newIp), "New IP after eviction should be allowed");
    // The evicted IP (1) should now act as a fresh entry
    ArduinoMock::mockMillis = 2001;
    TEST_ASSERT_TRUE_MESSAGE(rate_limit_check((uint32_t)1), "Evicted IP re-entering should be allowed");
}

// ===== Reset =====

void test_reset_clears_all(void) {
    uint32_t ip = 0xC0A80001;
    // Exhaust the limit
    for (int i = 0; i < RATE_LIMIT_MAX_REQUESTS + 5; i++) {
        rate_limit_check(ip);
    }
    TEST_ASSERT_FALSE(rate_limit_check(ip));
    // Reset and verify it works again
    rate_limit_reset();
    TEST_ASSERT_TRUE_MESSAGE(rate_limit_check(ip), "After reset, requests should be allowed");
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_under_limit_passes);
    RUN_TEST(test_burst_exceeds_limit);
    RUN_TEST(test_window_rollover_resets);
    RUN_TEST(test_different_ips_independent);
    RUN_TEST(test_slot_eviction);
    RUN_TEST(test_reset_clears_all);
    return UNITY_END();
}
