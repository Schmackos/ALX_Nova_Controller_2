#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include <ArduinoJson.h>
#else
#include <Arduino.h>
#include <ArduinoJson.h>
#endif

#include <string>
#include <map>

// ===== Mock WebServer =====
namespace MockWebServer {
    std::string requestBody;
    std::map<std::string, std::string> responses;
    int lastResponseCode = 0;
    std::string lastContentType;
    std::string lastResponse;

    void reset() {
        requestBody.clear();
        responses.clear();
        lastResponseCode = 0;
        lastContentType.clear();
        lastResponse.clear();
    }

    bool hasArg(const char* name) {
        return name == std::string("plain") && !requestBody.empty();
    }

    std::string arg(const char* name) {
        if (name == std::string("plain")) {
            return requestBody;
        }
        return "";
    }

    void send(int code, const char* content_type, const std::string& content) {
        lastResponseCode = code;
        lastContentType = content_type;
        lastResponse = content;
    }
}

// Mock server object
class MockServer {
public:
    bool hasArg(const char* name) { return MockWebServer::hasArg(name); }
    std::string arg(const char* name) { return MockWebServer::arg(name); }
    void send(int code, const char* content_type, const std::string& content) {
        MockWebServer::send(code, content_type, content);
    }
} server;

// ===== Mock AppState for API testing =====
namespace TestAPIState {
    enum SensingMode { ALWAYS_ON, ALWAYS_OFF, SMART_AUTO };

    SensingMode currentMode = ALWAYS_ON;
    unsigned long timerDuration = 5;
    unsigned long timerRemaining = 0;
    float audioThreshold = -40.0f;
    bool amplifierState = false;
    float audioLevel = -96.0f;

    void reset() {
        currentMode = ALWAYS_ON;
        timerDuration = 5;
        timerRemaining = 0;
        audioThreshold = -40.0f;
        amplifierState = false;
        audioLevel = -96.0f;
    }
}

// Mock functions
void setAmplifierState(bool state) {
    TestAPIState::amplifierState = state;
}

// ===== API Handler Functions (simplified versions) =====

void handleSmartSensingGet() {
    JsonDocument doc;
    doc["success"] = true;

    // Convert mode enum to string
    std::string modeStr;
    switch (TestAPIState::currentMode) {
        case TestAPIState::ALWAYS_ON: modeStr = "always_on"; break;
        case TestAPIState::ALWAYS_OFF: modeStr = "always_off"; break;
        case TestAPIState::SMART_AUTO: modeStr = "smart_auto"; break;
    }
    doc["mode"] = modeStr;

    doc["timerDuration"] = TestAPIState::timerDuration;
    doc["timerRemaining"] = TestAPIState::timerRemaining;
    doc["timerActive"] = (TestAPIState::timerRemaining > 0);
    doc["amplifierState"] = TestAPIState::amplifierState;
    doc["audioThreshold"] = TestAPIState::audioThreshold;
    doc["audioLevel"] = TestAPIState::audioLevel;
    doc["signalDetected"] = (TestAPIState::audioLevel >= TestAPIState::audioThreshold);

    std::string json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

void handleSmartSensingUpdate() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\": false, \"message\": \"No data received\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid JSON\"}");
        return;
    }

    // Update mode
    if (doc["mode"].is<std::string>()) {
        std::string modeStr = doc["mode"].as<std::string>();
        TestAPIState::SensingMode newMode;

        if (modeStr == "always_on") {
            newMode = TestAPIState::ALWAYS_ON;
        } else if (modeStr == "always_off") {
            newMode = TestAPIState::ALWAYS_OFF;
        } else if (modeStr == "smart_auto") {
            newMode = TestAPIState::SMART_AUTO;
        } else {
            server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid mode\"}");
            return;
        }

        TestAPIState::currentMode = newMode;
    }

    // Update timer duration
    if (doc["timerDuration"].is<int>()) {
        int duration = doc["timerDuration"].as<int>();

        if (duration >= 1 && duration <= 60) {
            TestAPIState::timerDuration = duration;
        } else {
            server.send(400, "application/json", "{\"success\": false, \"message\": \"Timer duration must be between 1 and 60 minutes\"}");
            return;
        }
    }

    // Update audio threshold
    if (doc["audioThreshold"].is<float>() || doc["audioThreshold"].is<int>()) {
        float threshold = doc["audioThreshold"].as<float>();

        if (threshold >= -96.0f && threshold <= 0.0f) {
            TestAPIState::audioThreshold = threshold;
        } else {
            server.send(400, "application/json", "{\"success\": false, \"message\": \"Audio threshold must be between -96 and 0 dBFS\"}");
            return;
        }
    }

    // Manual override
    if (doc["manualOverride"].is<bool>()) {
        bool state = doc["manualOverride"].as<bool>();
        setAmplifierState(state);
    }

    JsonDocument resp;
    resp["success"] = true;
    std::string json;
    serializeJson(resp, json);
    server.send(200, "application/json", json);
}

// ===== Test Setup/Teardown =====

void setUp(void) {
    MockWebServer::reset();
    TestAPIState::reset();
}

void tearDown(void) {
    // Clean up
}

// ===== Tier 2.1: HTTP API Endpoint Tests =====

// Test 1: GET /api/smartsensing returns current state
void test_get_smartsensing_returns_state(void) {
    TestAPIState::currentMode = TestAPIState::SMART_AUTO;
    TestAPIState::timerDuration = 10;
    TestAPIState::timerRemaining = 150;
    TestAPIState::amplifierState = true;
    TestAPIState::audioThreshold = -30.0f;
    TestAPIState::audioLevel = -18.0f;

    handleSmartSensingGet();

    TEST_ASSERT_EQUAL(200, MockWebServer::lastResponseCode);
    TEST_ASSERT_EQUAL_STRING("application/json", MockWebServer::lastContentType.c_str());

    // Parse response
    JsonDocument doc;
    deserializeJson(doc, MockWebServer::lastResponse);

    TEST_ASSERT_TRUE(doc["success"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("smart_auto", doc["mode"].as<std::string>().c_str());
    TEST_ASSERT_EQUAL(10, doc["timerDuration"].as<int>());
    TEST_ASSERT_EQUAL(150, doc["timerRemaining"].as<int>());
    TEST_ASSERT_TRUE(doc["timerActive"].as<bool>());
    TEST_ASSERT_TRUE(doc["amplifierState"].as<bool>());
    TEST_ASSERT_FLOAT_WITHIN(0.01, -30.0, doc["audioThreshold"].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.01, -18.0, doc["audioLevel"].as<float>());
    TEST_ASSERT_TRUE(doc["signalDetected"].as<bool>());
}

// Test 2: POST /api/smartsensing updates mode
void test_post_smartsensing_updates_mode(void) {
    MockWebServer::requestBody = R"({"mode": "smart_auto"})";

    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(200, MockWebServer::lastResponseCode);
    TEST_ASSERT_EQUAL(TestAPIState::SMART_AUTO, TestAPIState::currentMode);

    // Parse response
    JsonDocument doc;
    deserializeJson(doc, MockWebServer::lastResponse);
    TEST_ASSERT_TRUE(doc["success"].as<bool>());
}

// Test 3: POST /api/smartsensing updates timer duration
void test_post_smartsensing_updates_timer_duration(void) {
    MockWebServer::requestBody = R"({"timerDuration": 15})";

    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(200, MockWebServer::lastResponseCode);
    TEST_ASSERT_EQUAL(15, TestAPIState::timerDuration);
}

// Test 4: POST /api/smartsensing validates timer duration (too low)
void test_post_smartsensing_rejects_invalid_timer_low(void) {
    MockWebServer::requestBody = R"({"timerDuration": 0})";

    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(400, MockWebServer::lastResponseCode);

    JsonDocument doc;
    deserializeJson(doc, MockWebServer::lastResponse);
    TEST_ASSERT_FALSE(doc["success"].as<bool>());
    TEST_ASSERT_NOT_NULL(strstr(doc["message"].as<std::string>().c_str(), "1 and 60"));
}

// Test 5: POST /api/smartsensing validates timer duration (too high)
void test_post_smartsensing_rejects_invalid_timer_high(void) {
    MockWebServer::requestBody = R"({"timerDuration": 61})";

    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(400, MockWebServer::lastResponseCode);

    JsonDocument doc;
    deserializeJson(doc, MockWebServer::lastResponse);
    TEST_ASSERT_FALSE(doc["success"].as<bool>());
}

// Test 6: POST /api/smartsensing updates audio threshold
void test_post_smartsensing_updates_audio_threshold(void) {
    MockWebServer::requestBody = R"({"audioThreshold": -30.0})";

    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(200, MockWebServer::lastResponseCode);
    TEST_ASSERT_FLOAT_WITHIN(0.01, -30.0, TestAPIState::audioThreshold);
}

// Test 7: POST /api/smartsensing validates audio threshold (too low)
void test_post_smartsensing_rejects_invalid_audio_threshold_low(void) {
    MockWebServer::requestBody = R"({"audioThreshold": -100.0})";

    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(400, MockWebServer::lastResponseCode);
}

// Test 8: POST /api/smartsensing validates audio threshold (too high)
void test_post_smartsensing_rejects_invalid_audio_threshold_high(void) {
    MockWebServer::requestBody = R"({"audioThreshold": 5.0})";

    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(400, MockWebServer::lastResponseCode);
}

// Test 9: POST /api/smartsensing handles invalid JSON
void test_post_smartsensing_rejects_invalid_json(void) {
    MockWebServer::requestBody = "invalid json{";

    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(400, MockWebServer::lastResponseCode);

    JsonDocument doc;
    deserializeJson(doc, MockWebServer::lastResponse);
    TEST_ASSERT_FALSE(doc["success"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("Invalid JSON", doc["message"].as<std::string>().c_str());
}

// Test 10: POST /api/smartsensing handles missing body
void test_post_smartsensing_rejects_missing_body(void) {
    MockWebServer::requestBody = ""; // Empty body

    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(400, MockWebServer::lastResponseCode);

    JsonDocument doc;
    deserializeJson(doc, MockWebServer::lastResponse);
    TEST_ASSERT_FALSE(doc["success"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("No data received", doc["message"].as<std::string>().c_str());
}

// Test 11: POST /api/smartsensing handles manual override
void test_post_smartsensing_manual_override(void) {
    TestAPIState::amplifierState = false;

    MockWebServer::requestBody = R"({"manualOverride": true})";
    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(200, MockWebServer::lastResponseCode);
    TEST_ASSERT_TRUE(TestAPIState::amplifierState);
}

// Test 12: POST /api/smartsensing handles invalid mode
void test_post_smartsensing_rejects_invalid_mode(void) {
    MockWebServer::requestBody = R"({"mode": "invalid_mode"})";

    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(400, MockWebServer::lastResponseCode);

    JsonDocument doc;
    deserializeJson(doc, MockWebServer::lastResponse);
    TEST_ASSERT_FALSE(doc["success"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("Invalid mode", doc["message"].as<std::string>().c_str());
}

// Test 13: POST /api/smartsensing handles multiple parameters
void test_post_smartsensing_multiple_parameters(void) {
    MockWebServer::requestBody = R"({
        "mode": "smart_auto",
        "timerDuration": 20,
        "audioThreshold": -18.0
    })";

    handleSmartSensingUpdate();

    TEST_ASSERT_EQUAL(200, MockWebServer::lastResponseCode);
    TEST_ASSERT_EQUAL(TestAPIState::SMART_AUTO, TestAPIState::currentMode);
    TEST_ASSERT_EQUAL(20, TestAPIState::timerDuration);
    TEST_ASSERT_FLOAT_WITHIN(0.01, -18.0, TestAPIState::audioThreshold);
}

// ===== Test Runner =====

int runUnityTests(void) {
    UNITY_BEGIN();

    RUN_TEST(test_get_smartsensing_returns_state);
    RUN_TEST(test_post_smartsensing_updates_mode);
    RUN_TEST(test_post_smartsensing_updates_timer_duration);
    RUN_TEST(test_post_smartsensing_rejects_invalid_timer_low);
    RUN_TEST(test_post_smartsensing_rejects_invalid_timer_high);
    RUN_TEST(test_post_smartsensing_updates_audio_threshold);
    RUN_TEST(test_post_smartsensing_rejects_invalid_audio_threshold_low);
    RUN_TEST(test_post_smartsensing_rejects_invalid_audio_threshold_high);
    RUN_TEST(test_post_smartsensing_rejects_invalid_json);
    RUN_TEST(test_post_smartsensing_rejects_missing_body);
    RUN_TEST(test_post_smartsensing_manual_override);
    RUN_TEST(test_post_smartsensing_rejects_invalid_mode);
    RUN_TEST(test_post_smartsensing_multiple_parameters);

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
