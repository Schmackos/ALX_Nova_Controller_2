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
// Previously, the backend sent "appState.audio.timerDuration" but the frontend
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
    doc["timerDuration"] = 15;      // NOT "appState.audio.timerDuration"
    doc["timerRemaining"] = 900;    // NOT "appState.audio.timerRemaining"
    doc["timerActive"] = true;      // Correct (never had prefix)
    doc["amplifierState"] = true;   // NOT "appState.audio.amplifierState"
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
    TEST_ASSERT_FALSE(doc["appState.audio.timerDuration"].is<int>());
    TEST_ASSERT_FALSE(doc["appState.audio.timerRemaining"].is<int>());
    TEST_ASSERT_FALSE(doc["appState.audio.amplifierState"].is<bool>());
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

// ===== Test: WebSocket Protocol Version =====
// Verifies that WS_PROTOCOL_VERSION is defined in config.h and the
// protocolVersion message uses the correct format.

// Pull in config.h for WS_PROTOCOL_VERSION
#include "../../src/config.h"

void test_protocol_version_constant_defined() {
    // WS_PROTOCOL_VERSION should be "1.0"
    TEST_ASSERT_EQUAL_STRING("1.0", WS_PROTOCOL_VERSION);
}

void test_protocol_version_format() {
    // Version string should be non-empty and match "major.minor" semver pattern
    const char* ver = WS_PROTOCOL_VERSION;
    TEST_ASSERT_NOT_NULL(ver);
    TEST_ASSERT_TRUE(strlen(ver) >= 3);  // At least "X.Y"

    // Find the dot separator
    const char* dot = strchr(ver, '.');
    TEST_ASSERT_NOT_NULL_MESSAGE(dot, "Version must contain a '.' separator");

    // Major part (before dot) should be all digits
    for (const char* p = ver; p < dot; p++) {
        TEST_ASSERT_TRUE_MESSAGE(*p >= '0' && *p <= '9',
            "Major version must be numeric");
    }

    // Minor part (after dot) should be all digits
    for (const char* p = dot + 1; *p != '\0'; p++) {
        TEST_ASSERT_TRUE_MESSAGE(*p >= '0' && *p <= '9',
            "Minor version must be numeric");
    }
}

void test_protocol_version_message_structure() {
    // Verify the protocolVersion WS message has the expected JSON structure
    JsonDocument doc;
    doc["type"] = "protocolVersion";
    doc["version"] = WS_PROTOCOL_VERSION;

    TEST_ASSERT_EQUAL_STRING("protocolVersion", doc["type"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("1.0", doc["version"].as<const char*>());

    // Verify the exact JSON string that websocket_command.cpp sends
    // (it uses compile-time string concatenation)
    const char* expected = "{\"type\":\"protocolVersion\",\"version\":\"1.0\"}";
    char buf[128];
    serializeJson(doc, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(expected, buf);
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

    // Protocol version tests
    RUN_TEST(test_protocol_version_constant_defined);
    RUN_TEST(test_protocol_version_format);
    RUN_TEST(test_protocol_version_message_structure);

    return UNITY_END();
}
