#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// ===== WiFi Core Functions =====
void startAccessPoint();
void stopAccessPoint();
void connectToWiFi(const char* ssid, const char* password);

// ===== WiFi Credentials Persistence =====
bool loadWiFiCredentials(String& ssid, String& password);
void saveWiFiCredentials(const char* ssid, const char* password);

// ===== WiFi Status Broadcasting =====
void buildWiFiStatusJson(JsonDocument& doc, bool fetchVersionIfMissing);
void sendWiFiStatus();

// ===== WiFi HTTP API Handlers =====
void handleAPRoot();
void handleAPConfig();
void handleAPConfigUpdate();
void handleAPToggle();
void handleWiFiConfig();
void handleWiFiStatus();
void handleWiFiScan();

#endif // WIFI_MANAGER_H
