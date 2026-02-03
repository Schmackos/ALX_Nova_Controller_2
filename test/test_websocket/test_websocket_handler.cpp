#include <unity.h>
#include <string>
#include <vector>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Mock WebSocket client
struct WSClient {
    uint32_t clientId;
    bool connected;
    std::string lastMessage;
};

// Mock WebSocket state
std::vector<WSClient> wsClients;
std::string lastBroadcastMessage;
int broadcastCount = 0;

// Mock app state
struct MockAppState {
    bool ledState = false;
    bool blinkingState = false;
    unsigned long uptime = 0;
    int cpuUsage = 0;
    int memoryUsage = 0;
} appState;

namespace TestWSState {
    void reset() {
        wsClients.clear();
        lastBroadcastMessage.clear();
        broadcastCount = 0;
        appState.ledState = false;
        appState.blinkingState = false;
        appState.uptime = 0;
        appState.cpuUsage = 0;
        appState.memoryUsage = 0;
#ifdef NATIVE_TEST
        ArduinoMock::reset();
#endif
    }
}

// ===== WEBSOCKET HANDLER IMPLEMENTATIONS =====

void addWSClient(uint32_t clientId) {
    WSClient client;
    client.clientId = clientId;
    client.connected = true;
    wsClients.push_back(client);
}

void removeWSClient(uint32_t clientId) {
    for (auto it = wsClients.begin(); it != wsClients.end(); ++it) {
        if (it->clientId == clientId) {
            wsClients.erase(it);
            return;
        }
    }
}

int getWSClientCount() {
    return wsClients.size();
}

std::string buildLedStateJson(bool state) {
    std::string json = "{\"type\":\"led_state\",\"state\":";
    json += state ? "true" : "false";
    json += "}";
    return json;
}

std::string buildBlinkingStateJson(bool state) {
    std::string json = "{\"type\":\"blinking_state\",\"state\":";
    json += state ? "true" : "false";
    json += "}";
    return json;
}

std::string buildHardwareStatsJson() {
    std::string json = "{\"type\":\"hardware_stats\",\"uptime\":";
    json += std::to_string(appState.uptime);
    json += ",\"cpu_usage\":";
    json += std::to_string(appState.cpuUsage);
    json += ",\"memory_usage\":";
    json += std::to_string(appState.memoryUsage);
    json += "}";
    return json;
}

void broadcastToClients(const std::string& message) {
    lastBroadcastMessage = message;
    broadcastCount++;

    for (auto& client : wsClients) {
        if (client.connected) {
            client.lastMessage = message;
        }
    }
}

void broadcastLedState(bool state) {
    std::string json = buildLedStateJson(state);
    broadcastToClients(json);
}

void broadcastBlinkingState(bool state) {
    std::string json = buildBlinkingStateJson(state);
    broadcastToClients(json);
}

void broadcastHardwareStats() {
    std::string json = buildHardwareStatsJson();
    broadcastToClients(json);
}

void cleanupDisconnectedClients() {
    for (auto it = wsClients.begin(); it != wsClients.end();) {
        if (!it->connected) {
            it = wsClients.erase(it);
        } else {
            ++it;
        }
    }
}

// ===== Test Setup/Teardown =====

void setUp(void) {
    TestWSState::reset();
}

void tearDown(void) {
    // Clean up after each test
}

// ===== Broadcast Tests =====

void test_broadcast_led_state_on(void) {
    addWSClient(1);
    addWSClient(2);

    broadcastLedState(true);

    TEST_ASSERT_EQUAL(2, broadcastCount);
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("true") != std::string::npos);
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("led_state") != std::string::npos);
}

void test_broadcast_led_state_off(void) {
    addWSClient(1);

    broadcastLedState(false);

    TEST_ASSERT_EQUAL(1, broadcastCount);
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("false") != std::string::npos);
}

void test_broadcast_blinking_state_on(void) {
    addWSClient(1);

    broadcastBlinkingState(true);

    TEST_ASSERT_EQUAL(1, broadcastCount);
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("blinking_state") != std::string::npos);
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("true") != std::string::npos);
}

void test_broadcast_blinking_state_off(void) {
    addWSClient(1);

    broadcastBlinkingState(false);

    TEST_ASSERT_TRUE(lastBroadcastMessage.find("false") != std::string::npos);
}

void test_broadcast_json_format(void) {
    addWSClient(1);

    broadcastLedState(true);

    // Verify JSON format
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("{") != std::string::npos);
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("}") != std::string::npos);
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("\"type\"") != std::string::npos);
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("\"state\"") != std::string::npos);
}

// ===== Hardware Stats Tests =====

void test_broadcast_hardware_stats(void) {
    addWSClient(1);
    appState.uptime = 3600; // 1 hour
    appState.cpuUsage = 45;
    appState.memoryUsage = 65;

    broadcastHardwareStats();

    TEST_ASSERT_TRUE(lastBroadcastMessage.find("3600") != std::string::npos);
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("45") != std::string::npos);
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("65") != std::string::npos);
    TEST_ASSERT_TRUE(lastBroadcastMessage.find("hardware_stats") != std::string::npos);
}

void test_broadcast_zero_stats(void) {
    addWSClient(1);
    appState.uptime = 0;
    appState.cpuUsage = 0;
    appState.memoryUsage = 0;

    broadcastHardwareStats();

    TEST_ASSERT_TRUE(lastBroadcastMessage.find("\"uptime\":0") != std::string::npos);
}

// ===== Client Management Tests =====

void test_websocket_client_cleanup(void) {
    addWSClient(1);
    addWSClient(2);
    addWSClient(3);

    TEST_ASSERT_EQUAL(3, getWSClientCount());

    // Mark client 2 as disconnected
    wsClients[1].connected = false;

    cleanupDisconnectedClients();

    TEST_ASSERT_EQUAL(2, getWSClientCount());
    TEST_ASSERT_EQUAL(1, wsClients[0].clientId);
    TEST_ASSERT_EQUAL(3, wsClients[1].clientId);
}

void test_websocket_add_client(void) {
    addWSClient(1);
    TEST_ASSERT_EQUAL(1, getWSClientCount());

    addWSClient(2);
    TEST_ASSERT_EQUAL(2, getWSClientCount());
}

void test_websocket_remove_client(void) {
    addWSClient(1);
    addWSClient(2);

    removeWSClient(1);

    TEST_ASSERT_EQUAL(1, getWSClientCount());
    TEST_ASSERT_EQUAL(2, wsClients[0].clientId);
}

void test_websocket_broadcast_to_all_clients(void) {
    addWSClient(1);
    addWSClient(2);
    addWSClient(3);

    broadcastLedState(true);

    // All clients should have received the message
    for (const auto& client : wsClients) {
        TEST_ASSERT_EQUAL_STRING(lastBroadcastMessage.c_str(), client.lastMessage.c_str());
    }
}

void test_websocket_no_broadcast_when_empty(void) {
    broadcastLedState(true);

    // Should still record broadcast count (not skip empty broadcasts)
    TEST_ASSERT_EQUAL(1, broadcastCount);
}

// ===== Message Encoding Tests =====

void test_websocket_message_json_valid(void) {
    std::string json = buildLedStateJson(true);

    // Validate JSON structure
    TEST_ASSERT_EQUAL('{', json[0]);
    TEST_ASSERT_EQUAL('}', json[json.length() - 1]);
    TEST_ASSERT_TRUE(json.find("\"type\"") != std::string::npos);
}

void test_websocket_message_escaping(void) {
    // Test that special characters are handled
    std::string json = buildLedStateJson(true);

    // Should not have unescaped quotes breaking the JSON
    TEST_ASSERT_EQUAL(-1, json.find("'"));
}

void test_websocket_message_size(void) {
    std::string json = buildHardwareStatsJson();

    // Message should be reasonably sized
    TEST_ASSERT_GREATER_THAN(10, json.length());
    TEST_ASSERT_LESS_THAN(1000, json.length());
}

// ===== Test Runner =====

int runUnityTests(void) {
    UNITY_BEGIN();

    // Broadcast tests
    RUN_TEST(test_broadcast_led_state_on);
    RUN_TEST(test_broadcast_led_state_off);
    RUN_TEST(test_broadcast_blinking_state_on);
    RUN_TEST(test_broadcast_blinking_state_off);
    RUN_TEST(test_broadcast_json_format);

    // Hardware stats tests
    RUN_TEST(test_broadcast_hardware_stats);
    RUN_TEST(test_broadcast_zero_stats);

    // Client management tests
    RUN_TEST(test_websocket_client_cleanup);
    RUN_TEST(test_websocket_add_client);
    RUN_TEST(test_websocket_remove_client);
    RUN_TEST(test_websocket_broadcast_to_all_clients);
    RUN_TEST(test_websocket_no_broadcast_when_empty);

    // Message encoding tests
    RUN_TEST(test_websocket_message_json_valid);
    RUN_TEST(test_websocket_message_escaping);
    RUN_TEST(test_websocket_message_size);

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
