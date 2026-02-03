#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// ===== WiFi Core Functions =====
void startAccessPoint();
void stopAccessPoint();
void connectToWiFi(const char *ssid, const char *password,
                   bool useStaticIP = false, const char *staticIP = "",
                   const char *subnet = "", const char *gateway = "",
                   const char *dns1 = "", const char *dns2 = "");
void updateWiFiConnection();
bool configureStaticIP(const char *staticIP, const char *subnet,
                       const char *gateway, const char *dns1 = "",
                       const char *dns2 = ""); // Helper for static IP config

// ===== WiFi Event Handler & Reconnection =====
void initWiFiEventHandler(); // Initialize WiFi event handler (call in setup)
void checkWiFiConnection();  // Check and handle reconnection (call in loop)

// ===== Deferred Connection Globals =====
extern bool wifiConnectRequested;
extern unsigned long wifiConnectRequestTime;
extern String pendingSSID;
extern String pendingPassword;
extern bool pendingUseStaticIP;
extern String pendingStaticIP;
extern String pendingSubnet;
extern String pendingGateway;
extern String pendingDNS1;
extern String pendingDNS2;

// ===== WiFi Credentials Persistence =====
bool loadWiFiCredentials(String &ssid, String &password);
void saveWiFiCredentials(const char *ssid, const char *password);

// ===== Multi-WiFi Management =====
void migrateWiFiCredentials();  // One-time migration from LittleFS to
                                // Preferences
bool connectToStoredNetworks(); // Try all saved networks in order
bool saveWiFiNetwork(const char *ssid, const char *password,
                     bool useStaticIP = false, const char *staticIP = "",
                     const char *subnet = "", const char *gateway = "",
                     const char *dns1 = "",
                     const char *dns2 = ""); // Add/update network
bool removeWiFiNetwork(int index);           // Remove network by index
int getWiFiNetworkCount();                   // Get number of saved networks

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
