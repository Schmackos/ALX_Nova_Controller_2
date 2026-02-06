#include "wifi_manager.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include "mqtt_handler.h"
#include "ota_updater.h"
#include "web_pages.h"
#include "websocket_handler.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>

// ===== Deferred Connection State =====
WiFiConnectionRequest pendingConnection;
bool wifiStatusUpdateRequested = false;

// DNS Server for Captive Portal
DNSServer dnsServer;

// External function declarations
extern int compareVersions(const String &v1, const String &v2);
extern void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload,
                           size_t length);

// External HTML page reference
extern const char apHtmlPage[] PROGMEM;
extern const uint8_t apHtmlPage_gz[] PROGMEM;
extern const size_t apHtmlPage_gz_len;

// WiFi reconnection state
static bool wifiDisconnected = false;
static unsigned long lastDisconnectTime = 0;
static unsigned long lastReconnectAttempt = 0;
static unsigned long lastDisconnectWarning = 0;

// WiFi scan state - prevents reconnection logic during scanning
static bool wifiScanInProgress = false;
static unsigned long wifiScanStartTime = 0;

// WiFi retry state - for intelligent network retry logic
static bool wifiRetryInProgress = false;
static unsigned long lastFullRetryAttempt = 0;
static const unsigned long RETRY_INTERVAL_MS = 30000; // 30 seconds between full list retries
static int currentRetryCount = 0;
static String lastFailedSSID = ""; // Track last failed network to avoid immediate retry

// Helper function to convert WiFi disconnect reason to user-friendly message
String getWiFiDisconnectReason(uint8_t reason) {
  switch (reason) {
  case 1:
    return "Unspecified error";
  case 2:
    return "Authentication expired";
  case 3:
    return "Deauthenticated - AP is leaving";
  case 4:
    return "Disconnected due to inactivity";
  case 5:
    return "AP is busy, too many connected clients";
  case 6:
    return "Class 2 frame received from unauthenticated STA";
  case 7:
    return "Class 3 frame received from unassociated STA";
  case 8:
    return "Disassociated - AP is leaving";
  case 9:
    return "Not authenticated with AP";
  case 10:
    return "Power capability not valid";
  case 11:
    return "Supported channel not valid";
  case 13:
    return "Invalid information element";
  case 14:
    return "MIC failure";
  case 15:
    return "Authentication failed - check password";
  case 16:
    return "Group key handshake timeout";
  case 17:
    return "Invalid group key";
  case 18:
    return "Invalid pairwise cipher";
  case 19:
    return "Invalid AKMP";
  case 20:
    return "Unsupported RSN information element";
  case 21:
    return "Invalid RSN capabilities";
  case 22:
    return "IEEE 802.1X authentication failed";
  case 23:
    return "Cipher suite rejected";
  case 24:
    return "TDLS teardown unreachable";
  case 25:
    return "TDLS teardown unspecified";
  case 26:
    return "SSP requested disassociation";
  case 27:
    return "No SSP roaming agreement";
  case 200:
    return "Beacon timeout - AP not responding";
  case 201:
    return "Network not found";
  case 202:
    return "Authentication failed";
  case 203:
    return "Association failed";
  case 204:
    return "Handshake timeout - check password";
  case 205:
    return "Connection failed";
  case 206:
    return "AP TSF reset";
  case 207:
    return "Roaming link probe failed";
  default:
    return "Connection failed (code: " + String(reason) + ")";
  }
}

// ===== Helper Functions =====

// Generate Preferences key for network at given index
String getNetworkKey(const char *prefix, int index) {
  return String(prefix) + String(index);
}

// Parse JSON from HTTP request body
bool parseJsonRequest(JsonDocument &doc) {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return false;
  }

  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return false;
  }

  return true;
}

// Extract static IP configuration from JSON into config struct
void extractStaticIPConfig(const JsonDocument &doc, WiFiNetworkConfig &config) {
  config.ssid = doc["ssid"].as<String>();
  config.password = doc["password"].as<String>();
  config.useStaticIP = doc["useStaticIP"] | false;
  config.staticIP = doc["staticIP"] | "";
  config.subnet = doc["subnet"] | "";
  config.gateway = doc["gateway"] | "";
  config.dns1 = doc["dns1"] | "";
  config.dns2 = doc["dns2"] | "";
}

// Read network configuration from Preferences at given index
bool readNetworkFromPrefs(int index, WiFiNetworkConfig &config) {
  Preferences prefs;
  prefs.begin("wifi-list", true);

  int count = prefs.getUChar("count", 0);
  if (index < 0 || index >= count) {
    prefs.end();
    return false;
  }

  config.ssid = prefs.getString(getNetworkKey("s", index).c_str(), "");
  config.password = prefs.getString(getNetworkKey("p", index).c_str(), "");
  config.useStaticIP = prefs.getBool(getNetworkKey("static", index).c_str(), false);
  config.staticIP = prefs.getString(getNetworkKey("ip", index).c_str(), "");
  config.subnet = prefs.getString(getNetworkKey("subnet", index).c_str(), "");
  config.gateway = prefs.getString(getNetworkKey("gw", index).c_str(), "");
  config.dns1 = prefs.getString(getNetworkKey("dns1_", index).c_str(), "");
  config.dns2 = prefs.getString(getNetworkKey("dns2_", index).c_str(), "");

  prefs.end();
  return config.ssid.length() > 0;
}

// Write network configuration to Preferences at given index
static void writeNetworkToPrefs(Preferences &prefs, int index,
                                const WiFiNetworkConfig &config) {
  prefs.putString(getNetworkKey("s", index).c_str(), config.ssid);
  prefs.putString(getNetworkKey("p", index).c_str(), config.password);
  prefs.putBool(getNetworkKey("static", index).c_str(), config.useStaticIP);
  prefs.putString(getNetworkKey("ip", index).c_str(), config.staticIP);
  prefs.putString(getNetworkKey("subnet", index).c_str(), config.subnet);
  prefs.putString(getNetworkKey("gw", index).c_str(), config.gateway);
  prefs.putString(getNetworkKey("dns1_", index).c_str(), config.dns1);
  prefs.putString(getNetworkKey("dns2_", index).c_str(), config.dns2);
}

// Initialize network services (WebSocket, HTTP server, MQTT)
void initializeNetworkServices() {
  server.stop();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  DebugOut.setWebSocket(&webSocket);
  server.begin();

  DebugOut.println("=== Web server started on port 80 ===");
  DebugOut.println("=== WebSocket server started on port 81 ===");
  DebugOut.printf("Navigate to http://%s\n",
                  WiFi.localIP().toString().c_str());

  setupMqtt();
}

// Ensure AP mode is running alongside STA mode
void ensureAPModeWithSTA() {
  if (apEnabled && !isAPMode) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID.c_str(), apPassword);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    isAPMode = true;
    DebugOut.printf("Access Point also running at: %s\n",
                    WiFi.softAPIP().toString().c_str());
  } else if (!apEnabled && isAPMode) {
    dnsServer.stop();
    isAPMode = false;
  }
}

// WiFi Event Handler with reason info
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
    uint8_t reason = info.wifi_sta_disconnected.reason;
    String reasonStr = getWiFiDisconnectReason(reason);

    // Always print when actively connecting, otherwise throttle
    if (wifiConnecting) {
      DebugOut.printf("⚠️  WiFi connection failed: %s (reason %d)\n",
                      reasonStr.c_str(), reason);
      wifiConnectError = reasonStr;
    } else if (millis() - lastDisconnectWarning > WARNING_THROTTLE_MS) {
      DebugOut.printf("⚠️  WiFi disconnected: %s (reason %d)\n",
                      reasonStr.c_str(), reason);
      lastDisconnectWarning = millis();
    }

    // Detect "Network not found" error (201) - trigger retry of other networks
    if (reason == 201) {
      lastFailedSSID = WiFi.SSID();
      wifiRetryInProgress = true;
      DebugOut.printf("Network not found (%s) - will try other saved networks\n",
                      lastFailedSSID.c_str());
    }

    wifiDisconnected = true;
    lastDisconnectTime = millis();
    wifiStatusUpdateRequested = true; // Defer update to main loop
    break;
  }

  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    DebugOut.println("✓ WiFi connected to access point");
    wifiDisconnected = false;
    wifiConnectError = "";            // Clear any previous error
    wifiRetryInProgress = false;      // Clear retry flag on success
    currentRetryCount = 0;            // Reset retry counter
    lastFailedSSID = "";              // Clear failed SSID
    wifiStatusUpdateRequested = true; // Defer update to main loop
    break;

  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    DebugOut.printf("✓ WiFi IP address: %s\n",
                    WiFi.localIP().toString().c_str());
    wifiDisconnected = false;
    wifiConnectError = "";            // Clear any previous error
    wifiRetryInProgress = false;      // Clear retry flag on success
    currentRetryCount = 0;            // Reset retry counter
    lastFailedSSID = "";              // Clear failed SSID
    wifiStatusUpdateRequested = true; // Defer update to main loop
    break;

  default:
    break;
  }
}

// Initialize WiFi event handler (call this in setup)
void initWiFiEventHandler() {
  WiFi.onEvent(onWiFiEvent);
  DebugOut.println("WiFi event handler initialized");
}

// Check WiFi connection and attempt reconnection if needed
void checkWiFiConnection() {
  // Skip reconnection logic during WiFi scanning (with timeout safeguard)
  if (wifiScanInProgress) {
    if (millis() - wifiScanStartTime > WIFI_SCAN_TIMEOUT_MS) {
      // Scan timed out, clear the flag
      wifiScanInProgress = false;
      DebugOut.println("WiFi scan timeout - clearing scan flag");
    } else {
      return;
    }
  }

  // Handle immediate retry when network not found (error 201)
  if (wifiRetryInProgress && !wifiConnecting && WiFi.getMode() != WIFI_AP) {
    DebugOut.printf("Network '%s' not found - trying other saved networks...\n",
                    lastFailedSSID.c_str());

    if (connectToStoredNetworks()) {
      DebugOut.println("✓ Connected to alternative network!");
      wifiRetryInProgress = false;
      wifiDisconnected = false;
      currentRetryCount = 0;
    } else {
      // All networks failed, schedule full list retry
      lastFullRetryAttempt = millis();
      currentRetryCount++;
      DebugOut.printf("All networks failed (attempt %d). Will retry in %d seconds...\n",
                      currentRetryCount, RETRY_INTERVAL_MS / 1000);
      wifiRetryInProgress = false; // Clear immediate retry flag
    }
    return;
  }

  // Handle periodic retry of full network list
  if (wifiDisconnected && WiFi.getMode() != WIFI_AP && !wifiConnecting) {
    unsigned long timeSinceLastRetry = millis() - lastFullRetryAttempt;

    // Retry full list at interval if disconnected
    if (timeSinceLastRetry > RETRY_INTERVAL_MS &&
        millis() - lastReconnectAttempt > RECONNECT_DELAY_MS) {

      lastReconnectAttempt = millis();
      lastFullRetryAttempt = millis();
      currentRetryCount++;

      DebugOut.printf("Periodic retry attempt #%d - trying all saved networks...\n",
                      currentRetryCount);

      // Try to connect to stored networks
      if (connectToStoredNetworks()) {
        DebugOut.println("✓ Reconnection successful!");
        wifiDisconnected = false;
        currentRetryCount = 0;
      } else {
        // If no networks are available and we're not already in AP mode
        if (!isAPMode && autoAPEnabled) {
          DebugOut.println("No saved networks available. Starting AP mode...");
          startAccessPoint();
          sendWiFiStatus();
        } else {
          DebugOut.printf("No networks available. Next retry in %d seconds...\n",
                          RETRY_INTERVAL_MS / 1000);
        }
      }
    }
    // For initial disconnect, use old 10-second logic
    else if (timeSinceLastRetry == 0 && millis() - lastDisconnectTime > 10000 &&
             millis() - lastReconnectAttempt > RECONNECT_DELAY_MS) {

      lastReconnectAttempt = millis();
      lastFullRetryAttempt = millis();

      DebugOut.println("Initial reconnection attempt to saved networks...");

      if (connectToStoredNetworks()) {
        DebugOut.println("✓ Reconnection successful!");
        wifiDisconnected = false;
      } else if (!isAPMode && autoAPEnabled) {
        DebugOut.println("No saved networks available. Starting AP mode...");
        startAccessPoint();
        sendWiFiStatus();
      }
    }
  }
}

// ===== WiFi Core Functions =====

void startAccessPoint() {
  isAPMode = true;
  apEnabled = true;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str(), apPassword);

  IPAddress apIP = WiFi.softAPIP();
  DebugOut.println("\n=== Access Point Started ===");
  DebugOut.printf("SSID: %s\n", apSSID.c_str());
  DebugOut.printf("Password: %s\n", apPassword);
  DebugOut.print("AP IP address: ");
  DebugOut.println(apIP);

  // Start the DNS server for Captive Portal
  dnsServer.start(DNS_PORT, "*", apIP);
  DebugOut.println("DNS server started (Captive Portal active)");

  // We no longer overwrite "/" with a simple setup page.
  // The main dashboard routes from main.cpp will now handle requests.

  DebugOut.println("Web server configured for AP Mode");
}

void stopAccessPoint() {
  if (isAPMode) {
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    isAPMode = false;
    DebugOut.println("Access Point and DNS server stopped");
  }
}

// Helper function to configure static IP
bool configureStaticIP(const char *staticIP, const char *subnet,
                       const char *gateway, const char *dns1,
                       const char *dns2) {
  if (!staticIP || strlen(staticIP) == 0) {
    DebugOut.println("No static IP provided");
    return false;
  }

  IPAddress ip, gw, sn, d1, d2;

  if (!ip.fromString(staticIP)) {
    DebugOut.println("Invalid static IP address format");
    return false;
  }

  if (!gw.fromString(gateway)) {
    DebugOut.println("Invalid gateway address format");
    return false;
  }

  if (!sn.fromString(subnet)) {
    DebugOut.println("Invalid subnet mask format");
    return false;
  }

  // DNS servers are optional
  if (dns1 && strlen(dns1) > 0) {
    if (!d1.fromString(dns1)) {
      DebugOut.println("Invalid DNS1 address format");
      return false;
    }
  }

  if (dns2 && strlen(dns2) > 0) {
    if (!d2.fromString(dns2)) {
      DebugOut.println("Invalid DNS2 address format");
      return false;
    }
  }

  if (!WiFi.config(ip, gw, sn, d1, d2)) {
    DebugOut.println("Failed to configure static IP");
    return false;
  }

  DebugOut.printf("Static IP configured: %s\n", staticIP);
  return true;
}

// Connect to WiFi using config struct
void connectToWiFi(const WiFiNetworkConfig &config) {
  WiFi.mode(WIFI_STA);

  // Configure static IP if enabled
  if (config.useStaticIP) {
    configureStaticIP(config.staticIP.c_str(), config.subnet.c_str(),
                      config.gateway.c_str(), config.dns1.c_str(),
                      config.dns2.c_str());
  }

  WiFi.begin(config.ssid.c_str(), config.password.c_str());

  DebugOut.print("Connecting to WiFi: ");
  DebugOut.println(config.ssid);
  if (config.useStaticIP) {
    DebugOut.printf("  Using Static IP: %s\n", config.staticIP.c_str());
  } else {
    DebugOut.println("  Using DHCP");
  }

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    DebugOut.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    DebugOut.println("\nWiFi connected!");
    DebugOut.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

    // Synchronize time with NTP (required for SSL certificate validation)
    syncTimeWithNTP();

    // Setup AP mode if enabled
    ensureAPModeWithSTA();

    // Initialize network services
    initializeNetworkServices();
  } else {
    DebugOut.println("WARNING: Failed to connect to WiFi. Starting AP mode...");
    startAccessPoint();
  }
}

// Connect to WiFi (legacy overload for compatibility)
void connectToWiFi(const char *ssid, const char *password, bool useStaticIP,
                   const char *staticIP, const char *subnet,
                   const char *gateway, const char *dns1, const char *dns2) {
  WiFiNetworkConfig config;
  config.ssid = ssid ? ssid : "";
  config.password = password ? password : "";
  config.useStaticIP = useStaticIP;
  config.staticIP = staticIP ? staticIP : "";
  config.subnet = subnet ? subnet : "";
  config.gateway = gateway ? gateway : "";
  config.dns1 = dns1 ? dns1 : "";
  config.dns2 = dns2 ? dns2 : "";
  connectToWiFi(config);
}

// ===== WiFi Credentials Persistence =====

bool loadWiFiCredentials(String &ssid, String &password) {
  // Use create=true to avoid "no permits for creation" error log if file is
  // missing
  File file = LittleFS.open("/wifi_config.txt", "r", true);
  if (!file || file.size() == 0) {
    if (file)
      file.close();
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

// ===== Multi-WiFi Management =====

// One-time migration from LittleFS to Preferences
void migrateWiFiCredentials() {
  Preferences prefs;
  prefs.begin("wifi-list", false);

  // Check if migration already done
  if (prefs.getUChar("migrated", 0) == 1) {
    prefs.end();
    return;
  }

  // Load old credentials from LittleFS
  String oldSSID, oldPassword;
  if (loadWiFiCredentials(oldSSID, oldPassword)) {
    DebugOut.println(
        "Migrating WiFi credentials from LittleFS to Preferences...");

    // Save as first network
    prefs.putString("s0", oldSSID);
    prefs.putString("p0", oldPassword);
    prefs.putUChar("count", 1);

    // Delete old file
    LittleFS.remove("/wifi_config.txt");

    DebugOut.printf("Migrated network: %s\n", oldSSID.c_str());
  } else {
    // No old credentials, initialize empty
    prefs.putUChar("count", 0);
  }

  // Mark migration as complete
  prefs.putUChar("migrated", 1);
  prefs.end();

  DebugOut.println("WiFi credential migration complete");
}

// Get number of saved networks
int getWiFiNetworkCount() {
  Preferences prefs;
  prefs.begin("wifi-list", true); // Read-only
  int count = prefs.getUChar("count", 0);
  prefs.end();
  return count;
}

// Save WiFi network using config struct
bool saveWiFiNetwork(const WiFiNetworkConfig &config) {
  if (config.ssid.length() == 0) {
    return false;
  }

  Preferences prefs;
  prefs.begin("wifi-list", false);

  int count = prefs.getUChar("count", 0);

  // Check if SSID already exists
  for (int i = 0; i < count; i++) {
    String existingSSID = prefs.getString(getNetworkKey("s", i).c_str(), "");
    if (existingSSID == config.ssid) {
      // Update existing network - only update password if provided
      WiFiNetworkConfig updateConfig = config;
      if (config.password.length() == 0) {
        updateConfig.password =
            prefs.getString(getNetworkKey("p", i).c_str(), "");
      }
      writeNetworkToPrefs(prefs, i, updateConfig);
      prefs.end();
      DebugOut.printf("Updated network: %s (Static IP: %s)\n",
                      config.ssid.c_str(),
                      config.useStaticIP ? "enabled" : "disabled");
      return true;
    }
  }

  // Check if we've reached the maximum
  if (count >= MAX_WIFI_NETWORKS) {
    prefs.end();
    DebugOut.println("Maximum number of WiFi networks reached (5)");
    return false;
  }

  // Add new network at the end
  writeNetworkToPrefs(prefs, count, config);
  prefs.putUChar("count", count + 1);
  prefs.end();

  DebugOut.printf("Saved new network: %s (total: %d, Static IP: %s)\n",
                  config.ssid.c_str(), count + 1,
                  config.useStaticIP ? "enabled" : "disabled");
  return true;
}

// Save WiFi network (legacy overload for compatibility)
bool saveWiFiNetwork(const char *ssid, const char *password, bool useStaticIP,
                     const char *staticIP, const char *subnet,
                     const char *gateway, const char *dns1, const char *dns2) {
  WiFiNetworkConfig config;
  config.ssid = ssid ? ssid : "";
  config.password = password ? password : "";
  config.useStaticIP = useStaticIP;
  config.staticIP = staticIP ? staticIP : "";
  config.subnet = subnet ? subnet : "";
  config.gateway = gateway ? gateway : "";
  config.dns1 = dns1 ? dns1 : "";
  config.dns2 = dns2 ? dns2 : "";
  return saveWiFiNetwork(config);
}

// Helper to read network config directly from open Preferences
static void readNetworkFromOpenPrefs(Preferences &prefs, int index,
                                     WiFiNetworkConfig &config) {
  config.ssid = prefs.getString(getNetworkKey("s", index).c_str(), "");
  config.password = prefs.getString(getNetworkKey("p", index).c_str(), "");
  config.useStaticIP =
      prefs.getBool(getNetworkKey("static", index).c_str(), false);
  config.staticIP = prefs.getString(getNetworkKey("ip", index).c_str(), "");
  config.subnet = prefs.getString(getNetworkKey("subnet", index).c_str(), "");
  config.gateway = prefs.getString(getNetworkKey("gw", index).c_str(), "");
  config.dns1 = prefs.getString(getNetworkKey("dns1_", index).c_str(), "");
  config.dns2 = prefs.getString(getNetworkKey("dns2_", index).c_str(), "");
}

// Helper to remove all keys for a network at given index
static void removeNetworkKeys(Preferences &prefs, int index) {
  prefs.remove(getNetworkKey("s", index).c_str());
  prefs.remove(getNetworkKey("p", index).c_str());
  prefs.remove(getNetworkKey("static", index).c_str());
  prefs.remove(getNetworkKey("ip", index).c_str());
  prefs.remove(getNetworkKey("subnet", index).c_str());
  prefs.remove(getNetworkKey("gw", index).c_str());
  prefs.remove(getNetworkKey("dns1_", index).c_str());
  prefs.remove(getNetworkKey("dns2_", index).c_str());
}

// Remove network by index
bool removeWiFiNetwork(int index) {
  Preferences prefs;
  prefs.begin("wifi-list", false);

  int count = prefs.getUChar("count", 0);

  DebugOut.printf("removeWiFiNetwork called. Index: %d, Current count: %d\n",
                  index, count);

  if (index < 0 || index >= count) {
    DebugOut.printf("Invalid index %d for count %d\n", index, count);
    prefs.end();
    return false;
  }

  // Get SSID being removed for debug
  String removingSSID = prefs.getString(getNetworkKey("s", index).c_str(), "");
  DebugOut.printf("Removing network at index %d: %s\n", index,
                  removingSSID.c_str());

  // Shift all networks after this index down by one
  for (int i = index; i < count - 1; i++) {
    WiFiNetworkConfig config;
    readNetworkFromOpenPrefs(prefs, i + 1, config);
    DebugOut.printf("  Shifting index %d -> %d: %s\n", i + 1, i,
                    config.ssid.c_str());
    writeNetworkToPrefs(prefs, i, config);
  }

  // Remove the last entry
  DebugOut.printf("Removing last entry at index %d\n", count - 1);
  removeNetworkKeys(prefs, count - 1);
  prefs.putUChar("count", count - 1);

  prefs.end();
  DebugOut.printf("Successfully removed network. New count: %d\n", count - 1);
  return true;
}

// Try connecting to all saved networks in order
bool connectToStoredNetworks() {
  Preferences prefs;
  prefs.begin("wifi-list", true); // Read-only

  int count = prefs.getUChar("count", 0);

  if (count == 0) {
    prefs.end();
    DebugOut.println("No saved WiFi networks.");
    if (autoAPEnabled) {
      DebugOut.println("Auto AP enabled: Starting AP mode...");
      startAccessPoint();
    } else {
      DebugOut.println("Auto AP disabled: Not starting AP mode.");
    }
    return false;
  }

  DebugOut.printf("Trying %d saved network(s)...\n", count);

  for (int i = 0; i < count; i++) {
    WiFiNetworkConfig config;
    readNetworkFromOpenPrefs(prefs, i, config);

    if (config.ssid.length() == 0) {
      continue;
    }

    DebugOut.printf("Attempting connection %d/%d: %s\n", i + 1, count,
                    config.ssid.c_str());

    WiFi.mode(WIFI_STA);

    // Configure static IP if enabled
    if (config.useStaticIP && config.staticIP.length() > 0) {
      if (configureStaticIP(config.staticIP.c_str(), config.subnet.c_str(),
                            config.gateway.c_str(), config.dns1.c_str(),
                            config.dns2.c_str())) {
        DebugOut.printf("  Using Static IP: %s\n", config.staticIP.c_str());
      }
    } else {
      // Reset to DHCP
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
      DebugOut.println("  Using DHCP");
    }

    WiFi.begin(config.ssid.c_str(), config.password.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startTime < WIFI_CONNECT_TIMEOUT_MS) {
      delay(500);
      DebugOut.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      prefs.end();
      DebugOut.println("\nWiFi connected!");
      DebugOut.printf("Connected to: %s\n", config.ssid.c_str());
      DebugOut.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

      // Move successful network to index 0 (priority) if not already there
      if (i != 0) {
        Preferences prefsWrite;
        prefsWrite.begin("wifi-list", false);

        // Shift networks 0 to i-1 down by one
        for (int j = i; j > 0; j--) {
          WiFiNetworkConfig shiftConfig;
          readNetworkFromOpenPrefs(prefsWrite, j - 1, shiftConfig);
          writeNetworkToPrefs(prefsWrite, j, shiftConfig);
        }

        // Put successful network at index 0
        writeNetworkToPrefs(prefsWrite, 0, config);
        prefsWrite.end();

        DebugOut.println("Moved successful network to priority position");
      }

      // Synchronize time with NTP
      syncTimeWithNTP();

      // Setup AP mode if enabled
      ensureAPModeWithSTA();

      // Initialize network services
      initializeNetworkServices();

      return true;
    } else {
      DebugOut.printf("\nFailed to connect to: %s\n", config.ssid.c_str());
    }
  }

  prefs.end();
  DebugOut.println("All networks failed.");
  if (autoAPEnabled) {
    DebugOut.println("Auto AP enabled: Starting AP mode...");
    startAccessPoint();
  } else {
    DebugOut.println("Auto AP disabled: Not starting AP mode.");
  }
  return false;
}

// ===== WiFi Status Broadcasting =====

void buildWiFiStatusJson(JsonDocument &doc, bool fetchVersionIfMissing) {
  doc["connected"] = (WiFi.status() == WL_CONNECTED);
  doc["mode"] = isAPMode ? "ap" : "sta";
  doc["apEnabled"] = apEnabled;
  doc["autoUpdateEnabled"] = autoUpdateEnabled;
  doc["timezoneOffset"] = timezoneOffset;
  doc["dstOffset"] = dstOffset;
  doc["nightMode"] = nightMode;
  doc["enableCertValidation"] = enableCertValidation;
  doc["hardwareStatsInterval"] =
      hardwareStatsInterval / 1000; // Send as seconds
  doc["screenTimeout"] = appState.screenTimeout / 1000; // Send as seconds
  doc["backlightOn"] = appState.backlightOn;
  doc["autoAPEnabled"] = autoAPEnabled;
#ifdef GUI_ENABLED
  doc["bootAnimEnabled"] = appState.bootAnimEnabled;
  doc["bootAnimStyle"] = appState.bootAnimStyle;
#endif
  doc["mac"] = WiFi.macAddress();
  doc["firmwareVersion"] = firmwareVer;

  // Device information
  doc["manufacturer"] = MANUFACTURER_NAME;
  doc["model"] = MANUFACTURER_MODEL;
  doc["serialNumber"] = deviceSerialNumber;

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

  // Always include AP SSID for pre-filling config
  doc["apSSID"] = apSSID;

  // Populate AP details if enabled
  if (isAPMode) {
    doc["apIP"] = WiFi.softAPIP().toString();
    doc["apClients"] = WiFi.softAPgetStationNum();
  }

  // Populate STA details if connected or trying to connect
  doc["ssid"] = WiFi.SSID();
  if (WiFi.status() == WL_CONNECTED) {
    doc["staIP"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    // Primary IP preference: STA > AP
    doc["ip"] = WiFi.localIP().toString();

    // Check if current network is using static IP
    String currentSSID = WiFi.SSID();
    Preferences prefs;
    prefs.begin("wifi-list", true); // Read-only
    int count = prefs.getUChar("count", 0);
    bool isUsingStaticIP = false;

    for (int i = 0; i < count; i++) {
      String savedSSID = prefs.getString(("s" + String(i)).c_str(), "");
      if (savedSSID == currentSSID) {
        isUsingStaticIP = prefs.getBool(("static" + String(i)).c_str(), false);
        break;
      }
    }
    prefs.end();

    doc["usingStaticIP"] = isUsingStaticIP;
  } else {
    doc["ip"] = isAPMode ? WiFi.softAPIP().toString() : "";
    doc["usingStaticIP"] = false;
  }

  // Add saved networks count
  doc["networkCount"] = getWiFiNetworkCount();

  // Add async connection status
  doc["wifiConnecting"] = wifiConnecting;
  doc["wifiConnectSuccess"] = wifiConnectSuccess;
  doc["wifiNewIP"] = wifiNewIP;

  // Add error message if connection failed
  if (!wifiConnecting && !wifiConnectSuccess && wifiConnectError.length() > 0) {
    doc["message"] = wifiConnectError;
  }
}

void sendWiFiStatus() {
  JsonDocument doc;
  doc["type"] = "wifiStatus";
  buildWiFiStatusJson(doc, false); // Don't block on version fetch for WebSocket

  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t *)json.c_str(), json.length());
}

// ===== WiFi HTTP API Handlers =====

void handleAPRoot() {
  if (!sendGzipped(server, apHtmlPage_gz, apHtmlPage_gz_len)) {
    server.send_P(200, "text/html", apHtmlPage);
  }
}

void handleAPConfig() {
  JsonDocument doc;
  if (!parseJsonRequest(doc)) {
    return;
  }

  String ssid = doc["ssid"].as<String>();
  String password = doc["password"].as<String>();

  if (ssid.length() == 0 || password.length() == 0) {
    server.send(
        400, "application/json",
        "{\"success\": false, \"message\": \"SSID and password required\"}");
    return;
  }

  // Save to multi-WiFi list (replaces deprecated LittleFS storage)
  WiFiNetworkConfig config;
  config.ssid = ssid;
  config.password = password;
  saveWiFiNetwork(config);

  // Start asynchronous connection
  wifiSSID = ssid;
  wifiPassword = password;
  wifiConnecting = true;
  wifiConnectSuccess = false;
  wifiNewIP = "";
  wifiConnectError = "";

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Connection initiated\"}");

  DebugOut.printf("Credentials saved. Connecting to %s in background...\n",
                  ssid.c_str());
}

void handleAPConfigUpdate() {
  JsonDocument doc;
  if (!parseJsonRequest(doc)) {
    return;
  }

  String newSSID = doc["ssid"].as<String>();

  if (newSSID.length() == 0) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"SSID required\"}");
    return;
  }

  // Update AP SSID
  apSSID = newSSID;

  // Only update password if provided (otherwise keep existing)
  if (doc["password"].is<String>()) {
    String newPassword = doc["password"].as<String>();
    if (newPassword.length() >= 8) {
      apPassword = newPassword;
      DebugOut.println("AP password updated");
    } else if (newPassword.length() > 0) {
      server.send(400, "application/json",
                  "{\"success\": false, \"message\": \"Password must be at "
                  "least 8 characters\"}");
      return;
    }
    // If password is empty string, keep existing password
  }

  DebugOut.printf("AP Configuration updated: SSID=%s\n", apSSID.c_str());

  // If AP is currently running, restart it with new credentials
  if (isAPMode) {
    DebugOut.println("Restarting AP with new configuration...");
    WiFi.softAPdisconnect(true); // Disconnect all clients
    delay(100);

    if (WiFi.status() == WL_CONNECTED) {
      // If connected to WiFi, run in AP_STA mode
      WiFi.mode(WIFI_AP_STA);
    } else {
      // If not connected, run in AP-only mode
      WiFi.mode(WIFI_AP);
    }

    WiFi.softAP(apSSID.c_str(), apPassword.c_str());
    DebugOut.printf("AP restarted with new SSID: %s\n", apSSID.c_str());
    DebugOut.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  }

  sendWiFiStatus();
  server.send(200, "application/json", "{\"success\": true}");
}

void handleAPToggle() {
  JsonDocument doc;
  if (!parseJsonRequest(doc)) {
    return;
  }

  bool enabled = doc["enabled"].as<bool>();
  apEnabled = enabled;

  if (enabled) {
    if (!isAPMode) {
      // Start AP mode (can run alongside STA mode)
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(apSSID, apPassword);

      // Start DNS server for AP mode
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
      isAPMode = true;

      DebugOut.println("Access Point enabled");
      DebugOut.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
  } else {
    if (isAPMode) {
      // Always stop DNS server before disabling AP mode
      dnsServer.stop();

      if (WiFi.status() == WL_CONNECTED) {
        // Disable AP but keep STA connection
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
      } else {
        // Disable AP but keep WiFi in STA mode for scanning
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
      }
      isAPMode = false;
      DebugOut.println("Access Point disabled");
    }
  }

  sendWiFiStatus();
  server.send(200, "application/json", "{\"success\": true}");
}

void handleWiFiConfig() {
  JsonDocument doc;
  if (!parseJsonRequest(doc)) {
    return;
  }

  WiFiNetworkConfig config;
  extractStaticIPConfig(doc, config);

  if (config.ssid.length() == 0) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"SSID required\"}");
    return;
  }

  // If password is empty, fetch the stored password for this SSID
  String connectionPassword = config.password;
  if (config.password.length() == 0) {
    WiFiNetworkConfig storedConfig;
    Preferences prefs;
    prefs.begin("wifi-list", true);
    int count = prefs.getUChar("count", 0);
    for (int i = 0; i < count; i++) {
      readNetworkFromOpenPrefs(prefs, i, storedConfig);
      if (storedConfig.ssid == config.ssid) {
        connectionPassword = storedConfig.password;
        DebugOut.printf("Using stored password for network: %s\n",
                        config.ssid.c_str());
        break;
      }
    }
    prefs.end();
  }

  // Save to multi-WiFi list with static IP configuration
  if (!saveWiFiNetwork(config)) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Failed to save network. "
                "Maximum 5 networks reached.\"}");
    return;
  }

  // Start asynchronous connection request
  pendingConnection.requested = true;
  pendingConnection.requestTime = millis();
  pendingConnection.config = config;
  pendingConnection.config.password = connectionPassword;

  // Reset status flags for UI polling
  wifiConnecting = true;
  wifiConnectSuccess = false;
  wifiNewIP = "";
  wifiConnectError = "";

  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Connection initiated\"}");

  DebugOut.printf("Network saved. Connection request queued for %s...\n",
                  config.ssid.c_str());
}

void handleWiFiSave() {
  JsonDocument doc;
  if (!parseJsonRequest(doc)) {
    return;
  }

  WiFiNetworkConfig config;
  extractStaticIPConfig(doc, config);

  if (config.ssid.length() == 0) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"SSID required\"}");
    return;
  }

  // Save to multi-WiFi list with static IP configuration
  if (!saveWiFiNetwork(config)) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Failed to save network. "
                "Maximum 5 networks reached.\"}");
    return;
  }

  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Network settings saved\"}");

  DebugOut.printf("Network saved: %s (without connecting)\n",
                  config.ssid.c_str());
}

void handleWiFiStatus() {
  JsonDocument doc;
  buildWiFiStatusJson(doc,
                      true); // Fetch version if not cached for HTTP requests

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleWiFiScan() {
  DebugOut.println("Scanning for WiFi networks...");

  // Mark scan as in progress to prevent reconnection logic from interfering
  if (!wifiScanInProgress) {
    wifiScanStartTime = millis();
  }
  wifiScanInProgress = true;

  // Ensure WiFi is in proper mode for scanning
  wifi_mode_t wifiMode = WiFi.getMode();
  if (wifiMode == WIFI_MODE_NULL) {
    // If WiFi is off, set to STA mode
    WiFi.mode(WIFI_STA);
  } else if (wifiMode == WIFI_MODE_AP) {
    // If only AP mode, switch to AP+STA for scanning
    WiFi.mode(WIFI_AP_STA);
  }
  // For WIFI_MODE_STA or WIFI_MODE_APSTA, no change needed

  // Start async scan if not already scanning
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_FAILED) {
    // Clean up any previous failed scan
    WiFi.scanDelete();
    // Start a new scan
    int result = WiFi.scanNetworks(true); // Async scan
    if (result == WIFI_SCAN_FAILED) {
      DebugOut.println("Failed to start WiFi scan");
      wifiScanInProgress = false;
      server.send(500, "application/json",
                  "{\"scanning\": false, \"networks\": [], \"error\": \"Failed "
                  "to start scan\"}");
      return;
    }
    server.send(200, "application/json",
                "{\"scanning\": true, \"networks\": []}");
    return;
  }

  if (n == WIFI_SCAN_RUNNING) {
    // Scan still in progress
    server.send(200, "application/json",
                "{\"scanning\": true, \"networks\": []}");
    return;
  }

  // Scan complete, return results
  JsonDocument doc;
  doc["scanning"] = false;
  JsonArray networks = doc["networks"].to<JsonArray>();

  // Sort by signal strength and remove duplicates
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0)
      continue; // Skip hidden networks

    // Check for duplicates (same SSID)
    bool duplicate = false;
    for (JsonVariant v : networks) {
      if (v["ssid"].as<String>() == ssid) {
        // Keep the one with stronger signal
        if (WiFi.RSSI(i) > v["rssi"].as<int>()) {
          v["rssi"] = WiFi.RSSI(i);
          v["encryption"] =
              WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "secured" : "open";
        }
        duplicate = true;
        break;
      }
    }

    if (!duplicate) {
      JsonObject network = networks.add<JsonObject>();
      network["ssid"] = ssid;
      network["rssi"] = WiFi.RSSI(i);
      network["encryption"] =
          WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "secured" : "open";
    }
  }

  // Clear scan results to free memory
  WiFi.scanDelete();

  // Scan complete, clear the flag
  wifiScanInProgress = false;

  DebugOut.printf("Found %d unique networks\n", networks.size());

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleWiFiList() {
  int count = getWiFiNetworkCount();

  JsonDocument doc;
  doc["success"] = true;
  doc["count"] = count;
  JsonArray networks = doc["networks"].to<JsonArray>();

  for (int i = 0; i < count; i++) {
    WiFiNetworkConfig config;
    if (readNetworkFromPrefs(i, config) && config.ssid.length() > 0) {
      JsonObject net = networks.add<JsonObject>();
      net["ssid"] = config.ssid;
      net["index"] = i;
      net["priority"] = (i == 0);
      net["useStaticIP"] = config.useStaticIP;
      if (config.useStaticIP) {
        net["staticIP"] = config.staticIP;
        net["subnet"] = config.subnet;
        net["gateway"] = config.gateway;
        net["dns1"] = config.dns1;
        net["dns2"] = config.dns2;
      }
    }
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleWiFiRemove() {
  JsonDocument doc;
  if (!parseJsonRequest(doc)) {
    return;
  }

  if (!doc["index"].is<int>()) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Index required\"}");
    return;
  }

  int index = doc["index"].as<int>();

  // Get the SSID of the network being removed before removing it
  WiFiNetworkConfig removedConfig;
  bool wasConnectedToRemovedNetwork = false;
  readNetworkFromPrefs(index, removedConfig);

  // Check if we're currently connected to this network
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == removedConfig.ssid) {
    wasConnectedToRemovedNetwork = true;
    DebugOut.printf("Removing currently connected network: %s\n",
                    removedConfig.ssid.c_str());
  }

  if (removeWiFiNetwork(index)) {
    server.send(200, "application/json", "{\"success\": true}");

    // If we were connected to the removed network, disconnect and try to
    // reconnect
    if (wasConnectedToRemovedNetwork) {
      DebugOut.println("Disconnecting from removed network...");
      WiFi.disconnect();

      // Stop AP if it was running in STA+AP mode
      if (isAPMode && WiFi.status() != WL_CONNECTED) {
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        isAPMode = false;
      }

      // Try to connect to other saved networks, or start AP if none available
      delay(500); // Brief delay to allow disconnect to complete
      if (!connectToStoredNetworks()) {
        DebugOut.println("No saved networks available. AP mode started.");
      }

      // Broadcast updated WiFi status to all WebSocket clients
      sendWiFiStatus();
    }
  } else {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid index or removal "
                "failed\"}");
  }
}
void updateWiFiConnection() {
  // Handle deferred WiFi status broadcasting
  if (wifiStatusUpdateRequested) {
    wifiStatusUpdateRequested = false;
    sendWiFiStatus();
  }

  // Handle deferred connection request
  if (pendingConnection.requested) {
    // Wait for HTTP response to be sent (non-blocking delay)
    if (millis() - pendingConnection.requestTime < 500) {
      return;
    }

    DebugOut.println("\n=== Processing Deferred Connection Request ===");
    pendingConnection.requested = false;

    // Set global connection variables for status reporting
    wifiSSID = pendingConnection.config.ssid;
    wifiPassword = pendingConnection.config.password;

    // IMPORTANT: Maintain AP mode if enabled so frontend doesn't lose
    // connection. This allows the UI to poll for status (success/failure)
    if (apEnabled || isAPMode) {
      DebugOut.println("Maintaining AP mode during connection attempt...");
      WiFi.mode(WIFI_AP_STA);
      // Ensure AP is up if it wasn't
      if (!isAPMode) {
        WiFi.softAP(apSSID.c_str(), apPassword);
        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
        isAPMode = true;
      }
    } else {
      WiFi.mode(WIFI_STA);
    }

    // Disconnect current STA connection but keep radio on (false) if in AP_STA
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.disconnect(false);
      delay(100);
    }

    // Configure static IP logic
    const WiFiNetworkConfig &cfg = pendingConnection.config;
    if (cfg.useStaticIP && cfg.staticIP.length() > 0) {
      if (!configureStaticIP(cfg.staticIP.c_str(), cfg.subnet.c_str(),
                             cfg.gateway.c_str(), cfg.dns1.c_str(),
                             cfg.dns2.c_str())) {
        DebugOut.println("Failed to configure static IP");
        wifiConnectSuccess = false;
        wifiConnectError = "Invalid Static IP Configuration";
        wifiConnecting = false;
        pendingConnection.config.clear();
        return;
      }
    } else {
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }

    DebugOut.printf("Initiating connection to: %s\n", cfg.ssid.c_str());
    WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());
  }

  if (!wifiConnecting)
    return;

  static unsigned long connectionStarted = 0;
  if (connectionStarted == 0) {
    connectionStarted = millis();
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnectSuccess = true;
    wifiConnecting = false;
    wifiNewIP = WiFi.localIP().toString();
    wifiConnectError = "";
    connectionStarted = 0;
    pendingConnection.config.clear();

    DebugOut.println("\nWiFi connected in background!");
    DebugOut.printf("IP address: %s\n", wifiNewIP.c_str());

    // Sync time and setup services
    syncTimeWithNTP();
    setupMqtt();

    // Broadcast success to WebSocket clients
    sendWiFiStatus();
  } else if (millis() - connectionStarted > WIFI_CONNECT_TIMEOUT_MS) {
    wifiConnectSuccess = false;
    wifiConnecting = false;
    connectionStarted = 0;
    pendingConnection.config.clear();

    // Set timeout error if no specific disconnect reason was captured
    if (wifiConnectError.length() == 0) {
      wifiConnectError = "Connection timed out - check password and signal";
    }
    DebugOut.printf("\nWiFi connection failed: %s\n", wifiConnectError.c_str());

    // Restore AP mode if it was enabled
    if (apEnabled && !isAPMode) {
      DebugOut.println("Restoring AP mode after failed connection...");
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(apSSID.c_str(), apPassword);
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
      isAPMode = true;
      DebugOut.printf("AP restored at: %s\n",
                      WiFi.softAPIP().toString().c_str());
    }

    sendWiFiStatus();
  }
}
