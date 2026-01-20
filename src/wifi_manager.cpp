#include "wifi_manager.h"
#include "config.h"
#include "app_state.h"
#include "mqtt_handler.h"
#include "ota_updater.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// External function declarations
extern int compareVersions(const String& v1, const String& v2);
extern void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

// External HTML page reference
extern const char apHtmlPage[] PROGMEM;

// ===== WiFi Core Functions =====

void startAccessPoint() {
  isAPMode = true;
  apEnabled = true;
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str(), apPassword);
  
  IPAddress apIP = WiFi.softAPIP();
  Serial.println("\n=== Access Point Started ===");
  Serial.printf("SSID: %s\n", apSSID.c_str());
  Serial.printf("Password: %s\n", apPassword);
  Serial.print("AP IP address: ");
  Serial.println(apIP);
  
  server.on("/", handleAPRoot);
  server.on("/config", HTTP_POST, handleAPConfig);
  
  server.begin();
  Serial.println("Web server started on port 80 (AP Mode)");
}

void stopAccessPoint() {
  if (isAPMode) {
    WiFi.softAPdisconnect(true);
    isAPMode = false;
    Serial.println("Access Point stopped");
  }
}

void connectToWiFi(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Synchronize time with NTP (required for SSL certificate validation)
    syncTimeWithNTP();
    
    // If AP is enabled, keep it running alongside STA
    if (apEnabled && !isAPMode) {
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(apSSID.c_str(), apPassword);
      isAPMode = true;
      Serial.printf("Access Point also running at: %s\n", WiFi.softAPIP().toString().c_str());
    } else if (!apEnabled) {
      isAPMode = false;
    }
    
    server.stop();
    
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    server.begin();
    Serial.println("Web server started on port 80");
    Serial.println("WebSocket server started on port 81");
    Serial.printf("Navigate to http://%s\n", WiFi.localIP().toString().c_str());
    
    // Setup MQTT client
    setupMqtt();
  } else {
    Serial.println("\nFailed to connect to WiFi. Starting AP mode...");
    startAccessPoint();
  }
}

// ===== WiFi Credentials Persistence =====

bool loadWiFiCredentials(String& ssid, String& password) {
  File file = SPIFFS.open("/wifi_config.txt", "r");
  if (!file) {
    return false;
  }
  
  String line1 = file.readStringUntil('\n');
  String line2 = file.readStringUntil('\n');
  file.close();
  
  if (line1.length() > 0 && line2.length() > 0) {
    ssid = line1;
    password = line2;
    ssid.trim();
    password.trim();
    return true;
  }
  
  return false;
}

void saveWiFiCredentials(const char* ssid, const char* password) {
  File file = SPIFFS.open("/wifi_config.txt", "w");
  if (file) {
    file.println(ssid);
    file.println(password);
    file.close();
    Serial.println("Credentials saved to SPIFFS");
  }
}

// ===== WiFi Status Broadcasting =====

void buildWiFiStatusJson(JsonDocument& doc, bool fetchVersionIfMissing) {
  doc["connected"] = (WiFi.status() == WL_CONNECTED);
  doc["mode"] = isAPMode ? "ap" : "sta";
  doc["apEnabled"] = apEnabled;
  doc["autoUpdateEnabled"] = autoUpdateEnabled;
  doc["timezoneOffset"] = timezoneOffset;
  doc["nightMode"] = nightMode;
  doc["enableCertValidation"] = enableCertValidation;
  doc["mac"] = WiFi.macAddress();
  doc["firmwareVersion"] = firmwareVer;
  
  // Include latest version info if available
  if (cachedLatestVersion.length() > 0) {
    doc["latestVersion"] = cachedLatestVersion;
    doc["updateAvailable"] = updateAvailable;
  } else if (fetchVersionIfMissing) {
    // Try to fetch version info (for HTTP requests)
    String latestVersion = "";
    String firmwareUrl = "";
    String checksum = "";
    if (getLatestReleaseInfo(latestVersion, firmwareUrl, checksum)) {
      latestVersion.trim();
      cachedLatestVersion = latestVersion;
      cachedFirmwareUrl = firmwareUrl;
      cachedChecksum = checksum;
      int cmp = compareVersions(latestVersion, String(firmwareVer));
      updateAvailable = (cmp > 0);
      doc["latestVersion"] = latestVersion;
      doc["updateAvailable"] = updateAvailable;
    } else {
      doc["latestVersion"] = "Unknown";
      doc["updateAvailable"] = false;
    }
  } else {
    // For WebSocket, don't block - just show checking
    doc["latestVersion"] = "Checking...";
    doc["updateAvailable"] = false;
  }
  
  if (isAPMode) {
    doc["apSSID"] = apSSID;
    doc["ip"] = WiFi.softAPIP().toString();
  } else {
    doc["ssid"] = WiFi.SSID();
    if (WiFi.status() == WL_CONNECTED) {
      doc["ip"] = WiFi.localIP().toString();
      doc["rssi"] = WiFi.RSSI();
    } else {
      doc["ip"] = "";
    }
  }
}

void sendWiFiStatus() {
  JsonDocument doc;
  doc["type"] = "wifiStatus";
  buildWiFiStatusJson(doc, false);  // Don't block on version fetch for WebSocket
  
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

// ===== WiFi HTTP API Handlers =====

void handleAPRoot() {
  server.send_P(200, "text/html", apHtmlPage);
}

void handleAPConfig() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }
  
  String ssid = doc["ssid"].as<String>();
  String password = doc["password"].as<String>();
  
  if (ssid.length() == 0 || password.length() == 0) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"SSID and password required\"}");
    return;
  }
  
  saveWiFiCredentials(ssid.c_str(), password.c_str());
  server.send(200, "application/json", "{\"success\": true}");
  
  Serial.printf("Credentials saved. Connecting to %s...\n", ssid.c_str());
  delay(1000);
  
  connectToWiFi(ssid.c_str(), password.c_str());
}

void handleAPConfigUpdate() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }
  
  String newSSID = doc["ssid"].as<String>();
  
  if (newSSID.length() == 0) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"SSID required\"}");
    return;
  }
  
  // Update AP SSID
  apSSID = newSSID;
  
  // Only update password if provided (otherwise keep existing)
  if (doc["password"].is<String>()) {
    String newPassword = doc["password"].as<String>();
    if (newPassword.length() >= 8) {
      apPassword = newPassword;
      Serial.println("AP password updated");
    } else if (newPassword.length() > 0) {
      server.send(400, "application/json", "{\"success\": false, \"message\": \"Password must be at least 8 characters\"}");
      return;
    }
    // If password is empty string, keep existing password
  }
  
  Serial.printf("AP Configuration updated: SSID=%s\n", apSSID.c_str());
  
  // If AP is currently running, restart it with new credentials
  if (isAPMode) {
    Serial.println("Restarting AP with new configuration...");
    WiFi.softAPdisconnect(true);  // Disconnect all clients
    delay(100);
    
    if (WiFi.status() == WL_CONNECTED) {
      // If connected to WiFi, run in AP_STA mode
      WiFi.mode(WIFI_AP_STA);
    } else {
      // If not connected, run in AP-only mode
      WiFi.mode(WIFI_AP);
    }
    
    WiFi.softAP(apSSID.c_str(), apPassword.c_str());
    Serial.printf("AP restarted with new SSID: %s\n", apSSID.c_str());
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  }
  
  sendWiFiStatus();
  server.send(200, "application/json", "{\"success\": true}");
}

void handleAPToggle() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }
  
  bool enabled = doc["enabled"].as<bool>();
  apEnabled = enabled;
  
  if (enabled) {
    if (!isAPMode) {
      // Start AP mode (can run alongside STA mode)
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(apSSID, apPassword);
      isAPMode = true;
      Serial.println("Access Point enabled");
      Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
  } else {
    if (isAPMode && WiFi.status() == WL_CONNECTED) {
      // Disable AP but keep STA connection
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      isAPMode = false;
      Serial.println("Access Point disabled");
    }
  }
  
  sendWiFiStatus();
  server.send(200, "application/json", "{\"success\": true}");
}

void handleWiFiConfig() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }
  
  String ssid = doc["ssid"].as<String>();
  String password = doc["password"].as<String>();
  
  if (ssid.length() == 0) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"SSID required\"}");
    return;
  }
  
  saveWiFiCredentials(ssid.c_str(), password.c_str());
  wifiSSID = ssid;
  wifiPassword = password;
  
  server.send(200, "application/json", "{\"success\": true}");
  
  Serial.printf("WiFi credentials updated. Connecting to %s...\n", ssid.c_str());
  
  // Disconnect current connection and reconnect with new credentials
  if (isAPMode) {
    stopAccessPoint();
  }
  WiFi.disconnect();
  delay(500);
  connectToWiFi(ssid.c_str(), password.c_str());
}

void handleWiFiStatus() {
  JsonDocument doc;
  buildWiFiStatusJson(doc, true);  // Fetch version if not cached for HTTP requests
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}
