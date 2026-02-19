#include <unity.h>

// Include the pure function under test directly — no Arduino/hardware mocks needed.
// wifi_watchdog.h contains only inline logic with no platform dependencies.
#include "../../src/wifi_watchdog.h"

void setUp(void) {
    // Nothing to reset — all tests use pure function arguments
}

void tearDown(void) {}

// Test 1: Heap not critical => never reconnect regardless of other conditions
void test_watchdog_not_critical_returns_false(void) {
    TEST_ASSERT_FALSE(wifi_watchdog_should_reconnect(
        false,      // heapCritical
        true,       // wifiConnected
        false,      // otaInProgress
        300000UL    // criticalDurationMs (5 min — well beyond threshold)
    ));
}

// Test 2: Heap critical but duration < 2 minutes => not yet time to act
void test_watchdog_critical_under_2min_returns_false(void) {
    TEST_ASSERT_FALSE(wifi_watchdog_should_reconnect(
        true,       // heapCritical
        true,       // wifiConnected
        false,      // otaInProgress
        119999UL    // criticalDurationMs (1ms short of 2 minutes)
    ));
}

// Test 3: Heap critical for exactly 2 minutes => reconnect
void test_watchdog_critical_at_2min_returns_true(void) {
    TEST_ASSERT_TRUE(wifi_watchdog_should_reconnect(
        true,       // heapCritical
        true,       // wifiConnected
        false,      // otaInProgress
        120000UL    // criticalDurationMs (exactly 2 minutes)
    ));
}

// Test 4: OTA in progress blocks reconnect even if heap critical >2min
void test_watchdog_ota_blocks_reconnect(void) {
    TEST_ASSERT_FALSE(wifi_watchdog_should_reconnect(
        true,       // heapCritical
        true,       // wifiConnected
        true,       // otaInProgress
        120000UL    // criticalDurationMs (2 minutes)
    ));
}

// Test 5: WiFi not connected => nothing to reconnect
void test_watchdog_not_connected_returns_false(void) {
    TEST_ASSERT_FALSE(wifi_watchdog_should_reconnect(
        true,       // heapCritical
        false,      // wifiConnected
        false,      // otaInProgress
        120000UL    // criticalDurationMs (2 minutes)
    ));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_watchdog_not_critical_returns_false);
    RUN_TEST(test_watchdog_critical_under_2min_returns_false);
    RUN_TEST(test_watchdog_critical_at_2min_returns_true);
    RUN_TEST(test_watchdog_ota_blocks_reconnect);
    RUN_TEST(test_watchdog_not_connected_returns_false);

    return UNITY_END();
}
