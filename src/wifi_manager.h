#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// ===== Constants =====
constexpr uint8_t DNS_PORT = 53;
constexpr unsigned long RECONNECT_DELAY_MS = 5000;
constexpr unsigned long WARNING_THROTTLE_MS = 30000;
constexpr unsigned long WIFI_SCAN_TIMEOUT_MS = 30000;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;

constexpr uint8_t ROAM_MAX_CHECKS = 3;
constexpr unsigned long ROAM_CHECK_INTERVAL_MS = 300000UL;  // 5 minutes
constexpr int8_t ROAM_RSSI_EXCELLENT = -49;                 // Skip scan above this dBm
constexpr int8_t ROAM_RSSI_IMPROVEMENT_DB = 10;             // Min improvement to roam

// ===== WiFi Network Configuration Struct =====
struct WiFiNetworkConfig {
  String ssid;
  String password;
  bool useStaticIP = false;
  String staticIP;
  String subnet;
  String gateway;
  String dns1;
  String dns2;

  void clear() {
    ssid = "";
    password = "";
    useStaticIP = false;
    staticIP = "";
    subnet = "";
    gateway = "";
    dns1 = "";
    dns2 = "";
  }
};

// ===== Deferred Connection State =====
struct WiFiConnectionRequest {
  bool requested = false;
  unsigned long requestTime = 0;
  WiFiNetworkConfig config;
};

extern WiFiConnectionRequest pendingConnection;
extern bool wifiStatusUpdateRequested;

// ===== WiFi Core Functions =====
void startAccessPoint();
void stopAccessPoint();
void connectToWiFi(const WiFiNetworkConfig &config);
void connectToWiFi(const char *ssid, const char *password,
                   bool useStaticIP = false, const char *staticIP = "",
                   const char *subnet = "", const char *gateway = "",
                   const char *dns1 = "", const char *dns2 = "");
void updateWiFiConnection();
bool configureStaticIP(const char *staticIP, const char *subnet,
                       const char *gateway, const char *dns1 = "",
                       const char *dns2 = "");

// ===== WiFi Event Handler & Reconnection =====
void initWiFiEventHandler();
void checkWiFiConnection();
void checkWiFiRoaming();

// ===== WiFi Credentials Persistence (Legacy - for migration only) =====
bool loadWiFiCredentials(String &ssid, String &password);

// ===== Multi-WiFi Management =====
void migrateWiFiCredentials();
bool connectToStoredNetworks();
bool saveWiFiNetwork(const WiFiNetworkConfig &config);
bool saveWiFiNetwork(const char *ssid, const char *password,
                     bool useStaticIP = false, const char *staticIP = "",
                     const char *subnet = "", const char *gateway = "",
                     const char *dns1 = "", const char *dns2 = "");
bool removeWiFiNetwork(int index);
int getWiFiNetworkCount();
bool readNetworkFromPrefs(int index, WiFiNetworkConfig &config);

// ===== Preferences Key Helpers =====
String getNetworkKey(const char *prefix, int index);

// ===== Helper Functions =====
bool parseJsonRequest(JsonDocument &doc);
void extractStaticIPConfig(const JsonDocument &doc, WiFiNetworkConfig &config);
void initializeNetworkServices();
void ensureAPModeWithSTA();

// ===== WiFi Status Broadcasting =====
void buildWiFiStatusJson(JsonDocument &doc, bool fetchVersionIfMissing);
void sendWiFiStatus();

// ===== WiFi HTTP API Handlers =====
void handleAPRoot();
void handleAPConfig();
void handleAPConfigUpdate();
void handleAPToggle();
void handleWiFiConfig();
void handleWiFiStatus();
void handleWiFiScan();
void handleWiFiList();   // GET /api/wifilist - return all saved networks (no
                         // passwords)
void handleWiFiRemove(); // POST /api/wifiremove - remove network by index
void handleWiFiSave();   // POST /api/wifisave - save network without connecting

#endif // WIFI_MANAGER_H
