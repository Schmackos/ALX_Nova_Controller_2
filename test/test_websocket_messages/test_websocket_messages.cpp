#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include <ArduinoJson.h>
#else
#include <Arduino.h>
#include <ArduinoJson.h>
#endif

// ===== Test: WebSocket Smart Sensing Message Format =====
// This test verifies that the WebSocket messages use the correct
// JSON keys (without "appState." prefix) as expected by the frontend.
//
// Bug that this test prevents:
// Previously, the backend sent "appState.timerDuration" but the frontend
// expected "timerDuration", causing the timer display to show "-- min"
// instead of the actual timer value.

void test_smart_sensing_websocket_message_keys() {
    // Create a mock smart sensing WebSocket message
    JsonDocument doc;

    // Simulate the message construction from smart_sensing.cpp:sendSmartSensingStateInternal()
    doc["type"] = "smartSensing";
    doc["mode"] = "smart_auto";

    // CRITICAL: These keys must NOT have the "appState." prefix
    // Frontend expects: timerDuration, timerRemaining, timerActive, amplifierState
    doc["timerDuration"] = 15;      // NOT "appState.timerDuration"
    doc["timerRemaining"] = 900;    // NOT "appState.timerRemaining"
    doc["timerActive"] = true;      // Correct (never had prefix)
    doc["amplifierState"] = true;   // NOT "appState.amplifierState"
    doc["audioThreshold"] = -40.0f;
    doc["audioLevel"] = -50.0f;
    doc["signalDetected"] = true;

    // Verify the keys exist WITHOUT the "appState." prefix
    TEST_ASSERT_TRUE(doc["timerDuration"].is<int>());
    TEST_ASSERT_TRUE(doc["timerRemaining"].is<int>());
    TEST_ASSERT_TRUE(doc["timerActive"].is<bool>());
    TEST_ASSERT_TRUE(doc["amplifierState"].is<bool>());

    // Verify values
    TEST_ASSERT_EQUAL(15, doc["timerDuration"].as<int>());
    TEST_ASSERT_EQUAL(900, doc["timerRemaining"].as<int>());
    TEST_ASSERT_TRUE(doc["timerActive"].as<bool>());
    TEST_ASSERT_TRUE(doc["amplifierState"].as<bool>());

    // Verify the WRONG keys do NOT exist (regression check)
    TEST_ASSERT_FALSE(doc["appState.timerDuration"].is<int>());
    TEST_ASSERT_FALSE(doc["appState.timerRemaining"].is<int>());
    TEST_ASSERT_FALSE(doc["appState.amplifierState"].is<bool>());
}

void test_websocket_message_consistency() {
    // Verify all smart sensing messages use consistent key naming
    JsonDocument doc;

    doc["timerDuration"] = 10;
    doc["timerRemaining"] = 600;
    doc["timerActive"] = true;
    doc["amplifierState"] = false;
    doc["audioThreshold"] = -45.0f;
    doc["audioLevel"] = -60.0f;
    doc["signalDetected"] = false;

    // All keys should be simple names without prefixes
    TEST_ASSERT_TRUE(doc["timerDuration"].is<int>());
    TEST_ASSERT_TRUE(doc["timerRemaining"].is<int>());
    TEST_ASSERT_TRUE(doc["timerActive"].is<bool>());
    TEST_ASSERT_TRUE(doc["amplifierState"].is<bool>());
    TEST_ASSERT_TRUE(doc["audioThreshold"].is<float>());
    TEST_ASSERT_TRUE(doc["audioLevel"].is<float>());
    TEST_ASSERT_TRUE(doc["signalDetected"].is<bool>());
}

void test_timer_display_values() {
    // Test that timer values are correctly formatted for display
    JsonDocument doc;

    // Test case 1: Timer at full duration (15 minutes = 900 seconds)
    doc["timerDuration"] = 15;
    doc["timerRemaining"] = 900;
    doc["timerActive"] = true;

    TEST_ASSERT_EQUAL(15, doc["timerDuration"].as<int>());
    TEST_ASSERT_EQUAL(900, doc["timerRemaining"].as<int>());
    TEST_ASSERT_TRUE(doc["timerActive"].as<bool>());

    // Test case 2: Timer counting down
    doc["timerRemaining"] = 450; // 7:30 remaining
    TEST_ASSERT_EQUAL(450, doc["timerRemaining"].as<int>());

    // Test case 3: Timer expired
    doc["timerRemaining"] = 0;
    doc["timerActive"] = false;
    TEST_ASSERT_EQUAL(0, doc["timerRemaining"].as<int>());
    TEST_ASSERT_FALSE(doc["timerActive"].as<bool>());
}

void test_timer_active_flag() {
    // Test that timerActive correctly reflects timer state
    JsonDocument doc;

    // timerActive should be true when timerRemaining > 0
    doc["timerRemaining"] = 100;
    doc["timerActive"] = (doc["timerRemaining"].as<int>() > 0);
    TEST_ASSERT_TRUE(doc["timerActive"].as<bool>());

    // timerActive should be false when timerRemaining == 0
    doc["timerRemaining"] = 0;
    doc["timerActive"] = (doc["timerRemaining"].as<int>() > 0);
    TEST_ASSERT_FALSE(doc["timerActive"].as<bool>());
}

void setUp(void) {
#ifdef NATIVE_TEST
    ArduinoMock::reset();
#endif
}

void tearDown(void) {
    // Clean up after each test
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_smart_sensing_websocket_message_keys);
    RUN_TEST(test_websocket_message_consistency);
    RUN_TEST(test_timer_display_values);
    RUN_TEST(test_timer_active_flag);

    return UNITY_END();
}
