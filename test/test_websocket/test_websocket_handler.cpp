#include <cstring>
#include <string>
#include <unity.h>
#include <vector>


#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/IPAddress.h"
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
} // namespace TestWSState

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

int getWSClientCount() { return wsClients.size(); }

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

void broadcastToClients(const std::string &message) {
  lastBroadcastMessage = message;
  broadcastCount++;

  for (auto &client : wsClients) {
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

void setUp(void) { TestWSState::reset(); }

void tearDown(void) {
  // Clean up after each test
}

// ===== Broadcast Tests =====

void test_broadcast_led_state_on(void) {
  addWSClient(1);
  addWSClient(2);

  broadcastLedState(true);

  TEST_ASSERT_EQUAL(1, broadcastCount);
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
  TEST_ASSERT_TRUE(lastBroadcastMessage.find("blinking_state") !=
                   std::string::npos);
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
  TEST_ASSERT_TRUE(lastBroadcastMessage.find("hardware_stats") !=
                   std::string::npos);
}

void test_broadcast_zero_stats(void) {
  addWSClient(1);
  appState.uptime = 0;
  appState.cpuUsage = 0;
  appState.memoryUsage = 0;

  broadcastHardwareStats();

  TEST_ASSERT_TRUE(lastBroadcastMessage.find("\"uptime\":0") !=
                   std::string::npos);
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
  for (const auto &client : wsClients) {
    TEST_ASSERT_EQUAL_STRING(lastBroadcastMessage.c_str(),
                             client.lastMessage.c_str());
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

// ===== IP Binding Mock Infrastructure =====
// A minimal stand-alone implementation of the per-client IP binding logic
// that mirrors the production code in websocket_handler.cpp, allowing it to
// be exercised without pulling in the full ESP32 build chain.

#define IP_BIND_MAX_CLIENTS 10

struct MockWSServer {
  IPAddress clientIPs[IP_BIND_MAX_CLIENTS];
  bool disconnectCalled[IP_BIND_MAX_CLIENTS];
  int disconnectCalledCount = 0;

  void reset() {
    for (int i = 0; i < IP_BIND_MAX_CLIENTS; i++) {
      clientIPs[i] = IPAddress();
      disconnectCalled[i] = false;
    }
    disconnectCalledCount = 0;
  }

  IPAddress remoteIP(uint8_t num) {
    if (num < IP_BIND_MAX_CLIENTS) return clientIPs[num];
    return IPAddress();
  }

  void disconnect(uint8_t num) {
    if (num < IP_BIND_MAX_CLIENTS) disconnectCalled[num] = true;
    disconnectCalledCount++;
  }
} mockWS;

// Mirrors the production wsClientIP[] array
IPAddress testWsClientIP[IP_BIND_MAX_CLIENTS];

// Mirrors production: store IP on connect
void ipbind_on_connect(uint8_t num) {
  testWsClientIP[num] = mockWS.remoteIP(num);
}

// Mirrors production: clear IP on disconnect
void ipbind_on_disconnect(uint8_t num) {
  testWsClientIP[num] = IPAddress();
}

// Mirrors production: check IP on each message (returns false = mismatch → drop)
bool ipbind_check_message(uint8_t num) {
  if (mockWS.remoteIP(num) != testWsClientIP[num]) {
    mockWS.disconnect(num);
    return false;
  }
  return true;
}

// Mirrors production: confirm/update IP on auth success
void ipbind_on_auth(uint8_t num) {
  testWsClientIP[num] = mockWS.remoteIP(num);
}

static void ipbind_reset() {
  mockWS.reset();
  for (int i = 0; i < IP_BIND_MAX_CLIENTS; i++) {
    testWsClientIP[i] = IPAddress();
  }
}

// ===== IP Binding Tests =====

void test_ws_ip_match_passes(void) {
  ipbind_reset();
  // Client 0 connects from 192.168.1.50
  mockWS.clientIPs[0] = IPAddress(192, 168, 1, 50);
  ipbind_on_connect(0);

  // Message arrives from the same IP — should pass
  bool allowed = ipbind_check_message(0);

  TEST_ASSERT_TRUE(allowed);
  TEST_ASSERT_FALSE(mockWS.disconnectCalled[0]);
}

void test_ws_ip_mismatch_rejected(void) {
  ipbind_reset();
  // Client 0 connects from 192.168.1.50
  mockWS.clientIPs[0] = IPAddress(192, 168, 1, 50);
  ipbind_on_connect(0);

  // A different IP now claims to be client 0
  mockWS.clientIPs[0] = IPAddress(10, 0, 0, 99);
  bool allowed = ipbind_check_message(0);

  TEST_ASSERT_FALSE(allowed);
  TEST_ASSERT_TRUE(mockWS.disconnectCalled[0]);
  TEST_ASSERT_EQUAL(1, mockWS.disconnectCalledCount);
}

void test_ws_ip_cleared_on_disconnect(void) {
  ipbind_reset();
  // Client 2 connects from 172.16.0.5
  mockWS.clientIPs[2] = IPAddress(172, 16, 0, 5);
  ipbind_on_connect(2);

  // Stored IP should be set
  TEST_ASSERT_TRUE(testWsClientIP[2] == IPAddress(172, 16, 0, 5));

  // Client 2 disconnects
  ipbind_on_disconnect(2);

  // Stored IP should be cleared to default (0.0.0.0)
  TEST_ASSERT_TRUE(testWsClientIP[2] == IPAddress());
}

void test_ws_ip_updated_on_auth(void) {
  ipbind_reset();
  // Slot 1 has a stale IP (e.g., from a previous connection cycle)
  testWsClientIP[1] = IPAddress(192, 168, 1, 10);

  // Auth arrives with a new IP for slot 1
  mockWS.clientIPs[1] = IPAddress(192, 168, 1, 20);
  ipbind_on_auth(1);

  // Stored IP must be updated to the auth-time IP
  TEST_ASSERT_TRUE(testWsClientIP[1] == IPAddress(192, 168, 1, 20));
  // A message from the new IP should be accepted
  bool allowed = ipbind_check_message(1);
  TEST_ASSERT_TRUE(allowed);
}

void test_ws_ip_multiple_clients_independent(void) {
  ipbind_reset();
  // Client 0: 10.0.0.1, client 3: 10.0.0.2
  mockWS.clientIPs[0] = IPAddress(10, 0, 0, 1);
  mockWS.clientIPs[3] = IPAddress(10, 0, 0, 2);
  ipbind_on_connect(0);
  ipbind_on_connect(3);

  // Correct IPs — both should pass
  TEST_ASSERT_TRUE(ipbind_check_message(0));
  TEST_ASSERT_TRUE(ipbind_check_message(3));

  // Client 0 now reports a mismatched IP; client 3 is still correct
  mockWS.clientIPs[0] = IPAddress(10, 0, 0, 99);
  TEST_ASSERT_FALSE(ipbind_check_message(0));
  TEST_ASSERT_TRUE(ipbind_check_message(3));

  // Client 0 was disconnected; client 3 was not
  TEST_ASSERT_TRUE(mockWS.disconnectCalled[0]);
  TEST_ASSERT_FALSE(mockWS.disconnectCalled[3]);
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

  // IP binding tests
  RUN_TEST(test_ws_ip_match_passes);
  RUN_TEST(test_ws_ip_mismatch_rejected);
  RUN_TEST(test_ws_ip_cleared_on_disconnect);
  RUN_TEST(test_ws_ip_updated_on_auth);
  RUN_TEST(test_ws_ip_multiple_clients_independent);

  return UNITY_END();
}

// For native platform
#ifdef NATIVE_TEST
int main(void) { return runUnityTests(); }
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
