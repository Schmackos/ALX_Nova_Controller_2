#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// ===== WiFi Core Functions =====
void startAccessPoint();
void stopAccessPoint();
void connectToWiFi(const char *ssid, const char *password);
void updateWiFiConnection();

// ===== WiFi Credentials Persistence =====
bool loadWiFiCredentials(String &ssid, String &password);
void saveWiFiCredentials(const char *ssid, const char *password);

// ===== Multi-WiFi Management =====
void migrateWiFiCredentials();  // One-time migration from LittleFS to
                                // Preferences
bool connectToStoredNetworks(); // Try all saved networks in order
bool saveWiFiNetwork(const char *ssid,
                     const char *password); // Add/update network
bool removeWiFiNetwork(int index);          // Remove network by index
int getWiFiNetworkCount();                  // Get number of saved networks

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

#endif // WIFI_MANAGER_H
