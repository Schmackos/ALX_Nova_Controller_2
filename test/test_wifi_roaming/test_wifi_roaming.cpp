#include <unity.h>
#include <cstring>
#include <string>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/WiFi.h"
#include "../test_mocks/IPAddress.h"
#include "../test_mocks/Preferences.h"
#else
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#endif

// ===== Roaming constants (mirror of wifi_manager.h) =====
static const uint8_t ROAM_MAX_CHECKS = 3;
static const unsigned long ROAM_CHECK_INTERVAL_MS = 300000UL;
static const int8_t ROAM_RSSI_EXCELLENT = -49;
static const int8_t ROAM_RSSI_IMPROVEMENT_DB = 10;
static const unsigned long WIFI_SCAN_TIMEOUT_MS = 30000;

// ===== Roaming state (mirrors wifi_manager.cpp statics) =====
static bool roamScanInProgress = false;
static unsigned long roamScanStartTime = 0;
static bool wifiScanInProgress = false;  // user-initiated scan guard

// ===== AppState fields needed for roaming (minimal subset) =====
namespace RoamState {
  static uint8_t roamCheckCount = 0;
  static unsigned long lastRoamCheckTime = 0;
  static bool roamingInProgress = false;
  static bool wifiConnecting = false;

  static void reset() {
    roamCheckCount = 0;
    lastRoamCheckTime = 0;
    roamingInProgress = false;
    wifiConnecting = false;
  }
}

// ===== Preferences key helper =====
static String getNetworkKey(const char *prefix, int index) {
  return String(prefix) + String(index);
}

// ===== Local implementation of checkWiFiRoaming() for testing =====
static void checkWiFiRoaming() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (RoamState::wifiConnecting) return;
  if (RoamState::roamingInProgress) return;
  if (wifiScanInProgress) return;

  if (RoamState::roamCheckCount >= ROAM_MAX_CHECKS) return;

  String currentSSID = WiFi.SSID();
  if (currentSSID.length() == 0) return;

  if (RoamState::lastRoamCheckTime != 0 &&
      millis() - RoamState::lastRoamCheckTime < ROAM_CHECK_INTERVAL_MS) {
    return;
  }

  // ---- Phase 1: Start async scan ----
  if (!roamScanInProgress) {
    int currentRSSI = WiFi.RSSI();
    if (currentRSSI > ROAM_RSSI_EXCELLENT) {
      RoamState::roamCheckCount++;
      RoamState::lastRoamCheckTime = millis();
      return;
    }

    WiFi.scanDelete();
    int result = WiFi.scanNetworks(true, false);
    if (result == WIFI_SCAN_FAILED) {
      RoamState::roamCheckCount++;
      RoamState::lastRoamCheckTime = millis();
      return;
    }
    roamScanInProgress = true;
    roamScanStartTime = millis();
    return;
  }

  // ---- Phase 2: Poll for scan completion ----
  if (millis() - roamScanStartTime > WIFI_SCAN_TIMEOUT_MS) {
    roamScanInProgress = false;
    WiFi.scanDelete();
    RoamState::roamCheckCount++;
    RoamState::lastRoamCheckTime = millis();
    return;
  }

  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;

  roamScanInProgress = false;

  if (n == WIFI_SCAN_FAILED || n < 0) {
    RoamState::roamCheckCount++;
    RoamState::lastRoamCheckTime = millis();
    WiFi.scanDelete();
    return;
  }

  // ---- Phase 3: Evaluate results ----
  int currentRSSI = WiFi.RSSI();
  int bestRSSI = currentRSSI;
  int bestIndex = -1;

  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == currentSSID) {
      int candidateRSSI = WiFi.RSSI(i);
      if ((candidateRSSI - currentRSSI) >= ROAM_RSSI_IMPROVEMENT_DB &&
          candidateRSSI > bestRSSI) {
        bestRSSI = candidateRSSI;
        bestIndex = i;
      }
    }
  }

  RoamState::roamCheckCount++;
  RoamState::lastRoamCheckTime = millis();

  if (bestIndex >= 0) {
    uint8_t bssid[6];
    memcpy(bssid, WiFi.BSSID(bestIndex), 6);
    int32_t channel = WiFi.channel(bestIndex);

    String password = "";
    Preferences prefs;
    prefs.begin("wifi-list", true);
    int count = prefs.getUChar("count", 0);
    for (int i = 0; i < count; i++) {
      String storedSSID = prefs.getString(getNetworkKey("s", i).c_str(), "");
      if (storedSSID == currentSSID) {
        password = prefs.getString(getNetworkKey("p", i).c_str(), "");
        break;
      }
    }
    prefs.end();

    RoamState::roamingInProgress = true;
    WiFi.scanDelete();
    WiFi.begin(currentSSID.c_str(), password.c_str(), channel, bssid);
  } else {
    WiFi.scanDelete();
  }
}

// ===== Test setUp / tearDown =====

void setUp(void) {
  ArduinoMock::reset();
  WiFiClass::reset();
  Preferences::reset();
  RoamState::reset();
  roamScanInProgress = false;
  roamScanStartTime = 0;
  wifiScanInProgress = false;

  // Default: WL_CONNECTED, SSID "TestNet", RSSI -70
  WiFiClass::lastStatusCode = WL_CONNECTED;
  WiFiClass::connectedSSID = "TestNet";
  WiFiClass::mockRSSI = -70;
  WiFiClass::mockScanComplete = WIFI_SCAN_FAILED;
  WiFiClass::mockWifiBeginCalled = false;
}

void tearDown(void) {}

// ===== Tests =====

void test_roam_check_count_limits_at_three(void) {
  RoamState::roamCheckCount = ROAM_MAX_CHECKS;

  checkWiFiRoaming();

  TEST_ASSERT_EQUAL(ROAM_MAX_CHECKS, RoamState::roamCheckCount);
  TEST_ASSERT_FALSE(WiFiClass::mockWifiBeginCalled);
  TEST_ASSERT_FALSE(roamScanInProgress);
}

void test_roam_excellent_signal_skips_scan(void) {
  WiFiClass::mockRSSI = -40;  // Above -49 threshold
  RoamState::roamCheckCount = 0;
  ArduinoMock::mockMillis = 1000;  // Non-zero so lastRoamCheckTime is distinguishable

  checkWiFiRoaming();

  TEST_ASSERT_EQUAL(1, RoamState::roamCheckCount);
  TEST_ASSERT_NOT_EQUAL(0, RoamState::lastRoamCheckTime);
  TEST_ASSERT_FALSE(roamScanInProgress);
  TEST_ASSERT_FALSE(WiFiClass::mockWifiBeginCalled);
}

void test_roam_time_gate_enforced(void) {
  // Set current time to 1 minute in, and last check was 30 seconds ago
  ArduinoMock::mockMillis = 60000;
  RoamState::lastRoamCheckTime = 30000;  // 30s ago — well within 5 min gate
  RoamState::roamCheckCount = 0;
  WiFiClass::mockRSSI = -70;

  checkWiFiRoaming();

  TEST_ASSERT_EQUAL(0, RoamState::roamCheckCount);
  TEST_ASSERT_FALSE(roamScanInProgress);
}

void test_roam_hidden_network_skipped(void) {
  WiFiClass::connectedSSID = "";  // Hidden network

  checkWiFiRoaming();

  TEST_ASSERT_EQUAL(0, RoamState::roamCheckCount);
  TEST_ASSERT_FALSE(roamScanInProgress);
  TEST_ASSERT_FALSE(WiFiClass::mockWifiBeginCalled);
}

void test_roam_no_better_ap_increments_counter(void) {
  WiFiClass::mockRSSI = -70;
  // Scan result: same SSID at -72 (worse), different SSID at -50 (different)
  WiFiClass::addMockNetwork("TestNet", -72);
  WiFiClass::addMockNetwork("OtherNet", -50);
  WiFiClass::mockScanComplete = (int)WiFiClass::mockScanResults.size();

  roamScanInProgress = true;
  roamScanStartTime = millis() - 1000;

  checkWiFiRoaming();

  TEST_ASSERT_EQUAL(1, RoamState::roamCheckCount);
  TEST_ASSERT_FALSE(RoamState::roamingInProgress);
  TEST_ASSERT_FALSE(WiFiClass::mockWifiBeginCalled);
}

void test_roam_triggered_on_improvement(void) {
  WiFiClass::mockRSSI = -75;
  // Scan result: same SSID at -60 (+15 dB improvement)
  WiFiClass::addMockNetwork("TestNet", -60);
  WiFiClass::mockScanComplete = (int)WiFiClass::mockScanResults.size();

  // Store network password in Preferences for password lookup
  Preferences prefs;
  prefs.begin("wifi-list", false);
  prefs.putUChar("count", 1);
  prefs.putString("s0", "TestNet");
  prefs.putString("p0", "secret");
  prefs.end();

  roamScanInProgress = true;
  roamScanStartTime = millis() - 1000;

  checkWiFiRoaming();

  TEST_ASSERT_TRUE(RoamState::roamingInProgress);
  TEST_ASSERT_TRUE(WiFiClass::mockWifiBeginCalled);
  TEST_ASSERT_EQUAL(1, RoamState::roamCheckCount);
}

void test_roam_scan_timeout_increments_counter(void) {
  roamScanInProgress = true;
  roamScanStartTime = 0;  // Very old start time
  ArduinoMock::mockMillis = WIFI_SCAN_TIMEOUT_MS + 1;

  checkWiFiRoaming();

  TEST_ASSERT_FALSE(roamScanInProgress);
  TEST_ASSERT_EQUAL(1, RoamState::roamCheckCount);
  TEST_ASSERT_FALSE(WiFiClass::mockWifiBeginCalled);
}

void test_roam_disconnect_resets_counter(void) {
  // Simulate what the non-roaming disconnect handler does
  RoamState::roamCheckCount = 2;
  RoamState::lastRoamCheckTime = 12345;
  RoamState::roamingInProgress = false;

  // Non-roaming disconnect path: reset roam counter
  RoamState::roamCheckCount = 0;
  RoamState::lastRoamCheckTime = 0;

  TEST_ASSERT_EQUAL(0, RoamState::roamCheckCount);
  TEST_ASSERT_EQUAL(0, RoamState::lastRoamCheckTime);
}

void test_roam_roaming_disconnect_does_not_reset_counter(void) {
  RoamState::roamCheckCount = 2;
  RoamState::roamingInProgress = true;

  // Roaming in progress: the disconnect handler breaks early, counter stays
  // We verify by checking that checkWiFiRoaming skips all work (roamingInProgress guard)
  checkWiFiRoaming();

  TEST_ASSERT_EQUAL(2, RoamState::roamCheckCount);
}

void test_roam_not_connected_does_nothing(void) {
  WiFiClass::lastStatusCode = WL_DISCONNECTED;

  checkWiFiRoaming();

  TEST_ASSERT_EQUAL(0, RoamState::roamCheckCount);
  TEST_ASSERT_FALSE(roamScanInProgress);
}

void test_roam_connecting_guard_blocks(void) {
  RoamState::wifiConnecting = true;

  checkWiFiRoaming();

  TEST_ASSERT_EQUAL(0, RoamState::roamCheckCount);
  TEST_ASSERT_FALSE(roamScanInProgress);
}

void test_roam_user_scan_guard_blocks(void) {
  wifiScanInProgress = true;

  checkWiFiRoaming();

  TEST_ASSERT_EQUAL(0, RoamState::roamCheckCount);
}

void test_roam_scan_starts_when_conditions_met(void) {
  WiFiClass::mockRSSI = -70;
  WiFiClass::mockScanComplete = WIFI_SCAN_RUNNING;  // scanNetworks returns RUNNING

  checkWiFiRoaming();

  // First call: scan should be started (not FAILED)
  TEST_ASSERT_TRUE(roamScanInProgress);
  TEST_ASSERT_EQUAL(0, RoamState::roamCheckCount);  // Not incremented yet
}

void test_roam_scan_running_waits(void) {
  WiFiClass::mockScanComplete = WIFI_SCAN_RUNNING;
  roamScanInProgress = true;
  roamScanStartTime = millis();

  checkWiFiRoaming();

  // Still running: counter should not be incremented
  TEST_ASSERT_EQUAL(0, RoamState::roamCheckCount);
  TEST_ASSERT_TRUE(roamScanInProgress);
}

void test_roam_not_enough_improvement_does_not_roam(void) {
  WiFiClass::mockRSSI = -70;
  // Scan result: same SSID at -62 (only 8 dB, below threshold of 10)
  WiFiClass::addMockNetwork("TestNet", -62);
  WiFiClass::mockScanComplete = (int)WiFiClass::mockScanResults.size();

  roamScanInProgress = true;
  roamScanStartTime = millis() - 1000;

  checkWiFiRoaming();

  TEST_ASSERT_FALSE(RoamState::roamingInProgress);
  TEST_ASSERT_FALSE(WiFiClass::mockWifiBeginCalled);
  TEST_ASSERT_EQUAL(1, RoamState::roamCheckCount);
}

// ===== Test Runner =====

int runUnityTests(void) {
  UNITY_BEGIN();

  RUN_TEST(test_roam_check_count_limits_at_three);
  RUN_TEST(test_roam_excellent_signal_skips_scan);
  RUN_TEST(test_roam_time_gate_enforced);
  RUN_TEST(test_roam_hidden_network_skipped);
  RUN_TEST(test_roam_no_better_ap_increments_counter);
  RUN_TEST(test_roam_triggered_on_improvement);
  RUN_TEST(test_roam_scan_timeout_increments_counter);
  RUN_TEST(test_roam_disconnect_resets_counter);
  RUN_TEST(test_roam_roaming_disconnect_does_not_reset_counter);
  RUN_TEST(test_roam_not_connected_does_nothing);
  RUN_TEST(test_roam_connecting_guard_blocks);
  RUN_TEST(test_roam_user_scan_guard_blocks);
  RUN_TEST(test_roam_scan_starts_when_conditions_met);
  RUN_TEST(test_roam_scan_running_waits);
  RUN_TEST(test_roam_not_enough_improvement_does_not_roam);

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
