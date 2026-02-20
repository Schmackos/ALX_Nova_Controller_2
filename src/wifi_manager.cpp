#include "wifi_manager.h"
#include "app_state.h"
#include "audio_quality.h"
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

// WiFi roaming scan state
static bool roamScanInProgress = false;
static unsigned long roamScanStartTime = 0;

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

  LOG_I("[WiFi] Web server started on port 80");
  LOG_I("[WiFi] WebSocket server started on port 81");
  LOG_I("[WiFi] Navigate to http://%s", WiFi.localIP().toString().c_str());

  setupMqtt();
}

// Ensure AP mode is running alongside STA mode
void ensureAPModeWithSTA() {
  if (appState.apEnabled && !appState.isAPMode) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(appState.apSSID, appState.apPassword);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    appState.isAPMode = true;
    LOG_I("[WiFi] Access Point also running at: %s", WiFi.softAPIP().toString().c_str());
  } else if (!appState.apEnabled && appState.isAPMode) {
    dnsServer.stop();
    appState.isAPMode = false;
  }
}

// WiFi Event Handler with reason info
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
    uint8_t reason = info.wifi_sta_disconnected.reason;
    String reasonStr = getWiFiDisconnectReason(reason);

    // If WE triggered this disconnect for roaming, skip normal reconnect logic
    if (appState.roamingInProgress) {
      LOG_D("[WiFi] Roaming disconnect (expected), reason %d", reason);
      break;
    }
    // Non-roaming disconnect: reset roam counter for fresh start
    appState.roamCheckCount = 0;
    appState.lastRoamCheckTime = 0;

    // Always print when actively connecting, otherwise throttle
    if (appState.wifiConnecting) {
      LOG_W("[WiFi] Connection failed: %s (reason %d)", reasonStr.c_str(), reason);
      setCharField(appState.wifiConnectError, sizeof(appState.wifiConnectError), reasonStr.c_str());
    } else if (millis() - lastDisconnectWarning > WARNING_THROTTLE_MS) {
      LOG_W("[WiFi] Disconnected: %s (reason %d)", reasonStr.c_str(), reason);
      lastDisconnectWarning = millis();
    }

    // Detect "Network not found" error (201) - trigger retry of other networks
    if (reason == 201) {
      lastFailedSSID = WiFi.SSID();
      wifiRetryInProgress = true;
      LOG_W("[WiFi] Network not found (%s) - will try other saved networks", lastFailedSSID.c_str());
    }

    wifiDisconnected = true;
    lastDisconnectTime = millis();
    wifiStatusUpdateRequested = true; // Defer update to main loop
    // Mark WiFi event for audio quality correlation (Phase 3)
    audio_quality_mark_event("wifi_disconnected");
    break;
  }

  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    LOG_I("[WiFi] Connected to access point");
    wifiDisconnected = false;
    appState.wifiConnectError[0] = '\0';       // Clear any previous error
    wifiRetryInProgress = false;      // Clear retry flag on success
    currentRetryCount = 0;            // Reset retry counter
    lastFailedSSID = "";              // Clear failed SSID
    wifiStatusUpdateRequested = true; // Defer update to main loop
    // Mark WiFi event for audio quality correlation (Phase 3)
    audio_quality_mark_event("wifi_connected");
    // Clear roaming flag after a successful connection
    if (appState.roamingInProgress) {
      LOG_I("[WiFi] Roam successful");
      appState.roamingInProgress = false;
    }
    break;

  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    LOG_I("[WiFi] IP address: %s", WiFi.localIP().toString().c_str());
    wifiDisconnected = false;
    appState.wifiConnectError[0] = '\0';       // Clear any previous error
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
  LOG_I("[WiFi] Event handler initialized");
}

// Check WiFi connection and attempt reconnection if needed
void checkWiFiConnection() {
  // Skip reconnection logic during WiFi scanning (with timeout safeguard)
  if (wifiScanInProgress) {
    if (millis() - wifiScanStartTime > WIFI_SCAN_TIMEOUT_MS) {
      // Scan timed out, clear the flag
      wifiScanInProgress = false;
      LOG_W("[WiFi] Scan timeout - clearing scan flag");
    } else {
      return;
    }
  }

  // Don't run reconnection logic while a roaming scan is in progress
  if (roamScanInProgress) {
    if (millis() - roamScanStartTime > WIFI_SCAN_TIMEOUT_MS) {
      roamScanInProgress = false;
      LOG_W("[WiFi] Roam scan timeout in checkWiFiConnection - clearing flag");
    } else {
      return;
    }
  }

  // Handle immediate retry when network not found (error 201)
  if (wifiRetryInProgress && !appState.wifiConnecting && WiFi.getMode() != WIFI_AP) {
    LOG_W("[WiFi] Network '%s' not found - trying other saved networks", lastFailedSSID.c_str());

    if (connectToStoredNetworks()) {
      LOG_I("[WiFi] Connected to alternative network");
      wifiRetryInProgress = false;
      wifiDisconnected = false;
      currentRetryCount = 0;
    } else {
      // All networks failed, schedule full list retry
      lastFullRetryAttempt = millis();
      currentRetryCount++;
      LOG_W("[WiFi] All networks failed (attempt %d). Will retry in %d seconds", currentRetryCount, RETRY_INTERVAL_MS / 1000);
      wifiRetryInProgress = false; // Clear immediate retry flag
    }
    return;
  }

  // Handle periodic retry of full network list
  if (wifiDisconnected && WiFi.getMode() != WIFI_AP && !appState.wifiConnecting) {
    unsigned long timeSinceLastRetry = millis() - lastFullRetryAttempt;

    // Retry full list at interval if disconnected
    if (timeSinceLastRetry > RETRY_INTERVAL_MS &&
        millis() - lastReconnectAttempt > RECONNECT_DELAY_MS) {

      lastReconnectAttempt = millis();
      lastFullRetryAttempt = millis();
      currentRetryCount++;

      LOG_I("[WiFi] Periodic retry attempt #%d - trying all saved networks", currentRetryCount);

      // Try to connect to stored networks
      if (connectToStoredNetworks()) {
        LOG_I("[WiFi] Reconnection successful");
        wifiDisconnected = false;
        currentRetryCount = 0;
      } else {
        // If no networks are available and we're not already in AP mode
        if (!appState.isAPMode && appState.autoAPEnabled) {
          LOG_W("[WiFi] No saved networks available, starting AP mode");
          startAccessPoint();
          sendWiFiStatus();
        } else {
          LOG_W("[WiFi] No networks available. Next retry in %d seconds", RETRY_INTERVAL_MS / 1000);
        }
      }
    }
    // For initial disconnect, use old 10-second logic
    else if (timeSinceLastRetry == 0 && millis() - lastDisconnectTime > 10000 &&
             millis() - lastReconnectAttempt > RECONNECT_DELAY_MS) {

      lastReconnectAttempt = millis();
      lastFullRetryAttempt = millis();

      LOG_I("[WiFi] Initial reconnection attempt to saved networks");

      if (connectToStoredNetworks()) {
        LOG_I("[WiFi] Reconnection successful");
        wifiDisconnected = false;
      } else if (!appState.isAPMode && appState.autoAPEnabled) {
        LOG_W("[WiFi] No saved networks available, starting AP mode");
        startAccessPoint();
        sendWiFiStatus();
      }
    }
  }

  // Post-connect roaming check (only when stably connected)
  if (WiFi.status() == WL_CONNECTED && !wifiDisconnected) {
    checkWiFiRoaming();
  }
}

// ===== WiFi Core Functions =====

void checkWiFiRoaming() {
  // Prerequisites: must be stably connected, no competing operations
  if (WiFi.status() != WL_CONNECTED) return;
  if (appState.wifiConnecting) return;
  if (appState.roamingInProgress) return;
  if (wifiScanInProgress) return;  // User-initiated scan in progress

  // Convergence limit: after 3 checks we are done
  if (appState.roamCheckCount >= ROAM_MAX_CHECKS) return;

  // Skip roaming on hidden networks (SSID is empty string)
  String currentSSID = WiFi.SSID();
  if (currentSSID.length() == 0) return;

  // Time gate: only check every 5 minutes
  if (appState.lastRoamCheckTime != 0 &&
      millis() - appState.lastRoamCheckTime < ROAM_CHECK_INTERVAL_MS) {
    return;
  }

  // ---- Phase 1: Start async scan ----
  if (!roamScanInProgress) {
    int currentRSSI = WiFi.RSSI();
    if (currentRSSI > ROAM_RSSI_EXCELLENT) {
      // Signal is already excellent — skip scan but count toward limit
      appState.roamCheckCount++;
      appState.lastRoamCheckTime = millis();
      LOG_D("[WiFi] Roam check %d/%d: RSSI %d dBm (excellent, skipped)",
            appState.roamCheckCount, ROAM_MAX_CHECKS, currentRSSI);
      return;
    }

    // Start async scan (non-blocking, skip hidden networks)
    WiFi.scanDelete();
    int result = WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false);
    if (result == WIFI_SCAN_FAILED) {
      LOG_W("[WiFi] Roam scan failed to start");
      appState.roamCheckCount++;
      appState.lastRoamCheckTime = millis();
      return;
    }
    roamScanInProgress = true;
    roamScanStartTime = millis();
    LOG_D("[WiFi] Roam scan started (check %d/%d, RSSI %d dBm)",
          appState.roamCheckCount + 1, ROAM_MAX_CHECKS, currentRSSI);
    return;
  }

  // ---- Phase 2: Check if scan has completed (called on subsequent polls) ----

  // Timeout protection
  if (millis() - roamScanStartTime > WIFI_SCAN_TIMEOUT_MS) {
    roamScanInProgress = false;
    WiFi.scanDelete();
    appState.roamCheckCount++;
    appState.lastRoamCheckTime = millis();
    LOG_W("[WiFi] Roam scan timed out");
    return;
  }

  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;  // Still scanning, check again next poll

  roamScanInProgress = false;

  if (n == WIFI_SCAN_FAILED || n < 0) {
    appState.roamCheckCount++;
    appState.lastRoamCheckTime = millis();
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

  appState.roamCheckCount++;
  appState.lastRoamCheckTime = millis();

  if (bestIndex >= 0) {
    // Found a significantly better AP — look up password and roam
    uint8_t bssid[6];
    memcpy(bssid, WiFi.BSSID(bestIndex), 6);
    int32_t channel = WiFi.channel(bestIndex);

    // Find stored password for this SSID
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

    LOG_I("[WiFi] Roaming: current %d dBm -> target %d dBm (ch %d)",
          currentRSSI, bestRSSI, channel);

    // Set flag BEFORE WiFi.begin so the disconnect event handler knows this is intentional
    appState.roamingInProgress = true;
    WiFi.scanDelete();

    // Begin connection to specific BSSID + channel for fast roam
    WiFi.begin(currentSSID.c_str(), password.c_str(), channel, bssid);
  } else {
    LOG_D("[WiFi] Roam check %d/%d: no better AP found (current %d dBm)",
          appState.roamCheckCount, ROAM_MAX_CHECKS, currentRSSI);
    WiFi.scanDelete();
  }
}

void startAccessPoint() {
  appState.isAPMode = true;
  appState.apEnabled = true;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(appState.apSSID, appState.apPassword);

  IPAddress apIP = WiFi.softAPIP();
  LOG_I("[WiFi] Access Point started");
  LOG_I("[WiFi] SSID: %s", appState.apSSID);
  LOG_D("[WiFi] Password: %s", appState.apPassword);
  LOG_I("[WiFi] AP IP address: %s", apIP.toString().c_str());

  // Start the DNS server for Captive Portal
  dnsServer.start(DNS_PORT, "*", apIP);
  LOG_I("[WiFi] DNS server started (Captive Portal active)");

  // We no longer overwrite "/" with a simple setup page.
  // The main dashboard routes from main.cpp will now handle requests.

  LOG_I("[WiFi] Web server configured for AP mode");
}

void stopAccessPoint() {
  if (appState.isAPMode) {
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    appState.isAPMode = false;
    LOG_I("[WiFi] Access Point and DNS server stopped");
  }
}

// Helper function to configure static IP
bool configureStaticIP(const char *staticIP, const char *subnet,
                       const char *gateway, const char *dns1,
                       const char *dns2) {
  if (!staticIP || strlen(staticIP) == 0) {
    LOG_E("[WiFi] No static IP provided");
    return false;
  }

  IPAddress ip, gw, sn, d1, d2;

  if (!ip.fromString(staticIP)) {
    LOG_E("[WiFi] Invalid static IP address format");
    return false;
  }

  if (!gw.fromString(gateway)) {
    LOG_E("[WiFi] Invalid gateway address format");
    return false;
  }

  if (!sn.fromString(subnet)) {
    LOG_E("[WiFi] Invalid subnet mask format");
    return false;
  }

  // DNS servers are optional
  if (dns1 && strlen(dns1) > 0) {
    if (!d1.fromString(dns1)) {
      LOG_E("[WiFi] Invalid DNS1 address format");
      return false;
    }
  }

  if (dns2 && strlen(dns2) > 0) {
    if (!d2.fromString(dns2)) {
      LOG_E("[WiFi] Invalid DNS2 address format");
      return false;
    }
  }

  if (!WiFi.config(ip, gw, sn, d1, d2)) {
    LOG_E("[WiFi] Failed to configure static IP");
    return false;
  }

  LOG_I("[WiFi] Static IP configured: %s", staticIP);
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

  LOG_I("[WiFi] Connecting to: %s", config.ssid.c_str());
  if (config.useStaticIP) {
    LOG_D("[WiFi] Using Static IP: %s", config.staticIP.c_str());
  } else {
    LOG_D("[WiFi] Using DHCP");
  }

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    LOG_I("[WiFi] Connected");
    LOG_I("[WiFi] IP address: %s", WiFi.localIP().toString().c_str());

    // Synchronize time with NTP (required for SSL certificate validation)
    syncTimeWithNTP();

    // Setup AP mode if enabled
    ensureAPModeWithSTA();

    // Initialize network services
    initializeNetworkServices();
  } else {
    LOG_W("[WiFi] Failed to connect, starting AP mode");
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
    LOG_D("[WiFi] Migrating credentials from LittleFS to Preferences");

    // Save as first network
    prefs.putString("s0", oldSSID);
    prefs.putString("p0", oldPassword);
    prefs.putUChar("count", 1);

    // Delete old file
    LittleFS.remove("/wifi_config.txt");

    LOG_D("[WiFi] Migrated network: %s", oldSSID.c_str());
  } else {
    // No old credentials, initialize empty
    prefs.putUChar("count", 0);
  }

  // Mark migration as complete
  prefs.putUChar("migrated", 1);
  prefs.end();

  LOG_D("[WiFi] Credential migration complete");
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
      LOG_D("[WiFi] Updated network: %s (Static IP: %s)", config.ssid.c_str(), config.useStaticIP ? "enabled" : "disabled");
      return true;
    }
  }

  // Check if we've reached the maximum
  if (count >= MAX_WIFI_NETWORKS) {
    prefs.end();
    LOG_W("[WiFi] Maximum number of WiFi networks reached (5)");
    return false;
  }

  // Add new network at the end
  writeNetworkToPrefs(prefs, count, config);
  prefs.putUChar("count", count + 1);
  prefs.end();

  LOG_D("[WiFi] Saved new network: %s (total: %d, Static IP: %s)", config.ssid.c_str(), count + 1, config.useStaticIP ? "enabled" : "disabled");
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

  LOG_D("[WiFi] removeWiFiNetwork called. Index: %d, Current count: %d", index, count);

  if (index < 0 || index >= count) {
    LOG_D("[WiFi] Invalid index %d for count %d", index, count);
    prefs.end();
    return false;
  }

  // Get SSID being removed for debug
  String removingSSID = prefs.getString(getNetworkKey("s", index).c_str(), "");
  LOG_D("[WiFi] Removing network at index %d: %s", index, removingSSID.c_str());

  // Shift all networks after this index down by one
  for (int i = index; i < count - 1; i++) {
    WiFiNetworkConfig config;
    readNetworkFromOpenPrefs(prefs, i + 1, config);
    LOG_D("[WiFi] Shifting index %d -> %d: %s", i + 1, i, config.ssid.c_str());
    writeNetworkToPrefs(prefs, i, config);
  }

  // Remove the last entry
  LOG_D("[WiFi] Removing last entry at index %d", count - 1);
  removeNetworkKeys(prefs, count - 1);
  prefs.putUChar("count", count - 1);

  prefs.end();
  LOG_D("[WiFi] Successfully removed network. New count: %d", count - 1);
  return true;
}

// Try connecting to all saved networks in order
bool connectToStoredNetworks() {
  Preferences prefs;
  prefs.begin("wifi-list", true); // Read-only

  int count = prefs.getUChar("count", 0);

  if (count == 0) {
    prefs.end();
    LOG_W("[WiFi] No saved WiFi networks");
    if (appState.autoAPEnabled) {
      LOG_I("[WiFi] Auto AP enabled, starting AP mode");
      startAccessPoint();
    } else {
      LOG_I("[WiFi] Auto AP disabled, not starting AP mode");
    }
    return false;
  }

  LOG_I("[WiFi] Trying %d saved network(s)", count);

  for (int i = 0; i < count; i++) {
    WiFiNetworkConfig config;
    readNetworkFromOpenPrefs(prefs, i, config);

    if (config.ssid.length() == 0) {
      continue;
    }

    LOG_I("[WiFi] Attempting connection %d/%d: %s", i + 1, count, config.ssid.c_str());

    WiFi.mode(WIFI_STA);

    // Configure static IP if enabled
    if (config.useStaticIP && config.staticIP.length() > 0) {
      if (configureStaticIP(config.staticIP.c_str(), config.subnet.c_str(),
                            config.gateway.c_str(), config.dns1.c_str(),
                            config.dns2.c_str())) {
        LOG_D("[WiFi] Using Static IP: %s", config.staticIP.c_str());
      }
    } else {
      // Reset to DHCP
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
      LOG_D("[WiFi] Using DHCP");
    }

    WiFi.begin(config.ssid.c_str(), config.password.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startTime < WIFI_CONNECT_TIMEOUT_MS) {
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      prefs.end();
      LOG_I("[WiFi] Connected to: %s", config.ssid.c_str());
      LOG_I("[WiFi] IP address: %s", WiFi.localIP().toString().c_str());

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

        LOG_D("[WiFi] Moved successful network to priority position");
      }

      // Synchronize time with NTP
      syncTimeWithNTP();

      // Setup AP mode if enabled
      ensureAPModeWithSTA();

      // Initialize network services
      initializeNetworkServices();

      return true;
    } else {
      LOG_W("[WiFi] Failed to connect to: %s", config.ssid.c_str());
    }
  }

  prefs.end();
  LOG_W("[WiFi] All networks failed");
  if (appState.autoAPEnabled) {
    LOG_I("[WiFi] Auto AP enabled, starting AP mode");
    startAccessPoint();
  } else {
    LOG_I("[WiFi] Auto AP disabled, not starting AP mode");
  }
  return false;
}

// ===== WiFi Status Broadcasting =====

void buildWiFiStatusJson(JsonDocument &doc, bool fetchVersionIfMissing) {
  doc["connected"] = (WiFi.status() == WL_CONNECTED);
  doc["mode"] = appState.isAPMode ? "ap" : "sta";
  doc["appState.apEnabled"] = appState.apEnabled;
  doc["appState.autoUpdateEnabled"] = appState.autoUpdateEnabled;
  doc["appState.timezoneOffset"] = appState.timezoneOffset;
  doc["appState.dstOffset"] = appState.dstOffset;
  doc["appState.darkMode"] = appState.darkMode;
  doc["appState.enableCertValidation"] = appState.enableCertValidation;
  doc["appState.hardwareStatsInterval"] =
      appState.hardwareStatsInterval / 1000; // Send as seconds
  doc["audioUpdateRate"] = appState.audioUpdateRate;
  doc["screenTimeout"] = appState.screenTimeout / 1000; // Send as seconds
  doc["backlightOn"] = appState.backlightOn;
  doc["appState.autoAPEnabled"] = appState.autoAPEnabled;
#ifdef GUI_ENABLED
  doc["bootAnimEnabled"] = appState.bootAnimEnabled;
  doc["bootAnimStyle"] = appState.bootAnimStyle;
#endif
  doc["mac"] = WiFi.macAddress();
  doc["firmwareVersion"] = firmwareVer;

  // Device information
  doc["manufacturer"] = MANUFACTURER_NAME;
  doc["model"] = MANUFACTURER_MODEL;
  doc["serialNumber"] = appState.deviceSerialNumber;
  doc["customDeviceName"] = appState.customDeviceName;

  // Include latest version info if available
  if (appState.cachedLatestVersion.length() > 0) {
    doc["latestVersion"] = appState.cachedLatestVersion;
    doc["appState.updateAvailable"] = appState.updateAvailable;
  } else if (fetchVersionIfMissing) {
    // Try to fetch version info (for HTTP requests)
    String latestVersion = "";
    String firmwareUrl = "";
    String checksum = "";
    if (getLatestReleaseInfo(latestVersion, firmwareUrl, checksum)) {
      latestVersion.trim();
      appState.cachedLatestVersion = latestVersion;
      appState.cachedFirmwareUrl = firmwareUrl;
      appState.cachedChecksum = checksum;
      int cmp = compareVersions(latestVersion, String(firmwareVer));
      appState.updateAvailable = (cmp > 0);
      doc["latestVersion"] = latestVersion;
      doc["appState.updateAvailable"] = appState.updateAvailable;
    } else {
      doc["latestVersion"] = "Unknown";
      doc["appState.updateAvailable"] = false;
    }
  } else {
    // For WebSocket, don't block - just show checking
    doc["latestVersion"] = "Checking...";
    doc["appState.updateAvailable"] = false;
  }

  // Always include AP SSID for pre-filling config
  doc["appState.apSSID"] = appState.apSSID;

  // Populate AP details if enabled
  if (appState.isAPMode) {
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
    doc["ip"] = appState.isAPMode ? WiFi.softAPIP().toString() : "";
    doc["usingStaticIP"] = false;
  }

  // Add saved networks count
  doc["networkCount"] = getWiFiNetworkCount();

  // Add async connection status
  doc["appState.wifiConnecting"] = appState.wifiConnecting;
  doc["appState.wifiConnectSuccess"] = appState.wifiConnectSuccess;
  doc["appState.wifiNewIP"] = appState.wifiNewIP;

  // Add error message if connection failed
  if (!appState.wifiConnecting && !appState.wifiConnectSuccess && strlen(appState.wifiConnectError) > 0) {
    doc["message"] = appState.wifiConnectError;
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
  setCharField(appState.wifiSSID, sizeof(appState.wifiSSID), ssid.c_str());
  setCharField(appState.wifiPassword, sizeof(appState.wifiPassword), password.c_str());
  appState.wifiConnecting = true;
  appState.wifiConnectSuccess = false;
  appState.wifiNewIP[0] = '\0';
  appState.wifiConnectError[0] = '\0';

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Connection initiated\"}");

  LOG_I("[WiFi] Credentials saved. Connecting to %s in background", ssid.c_str());
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
  setCharField(appState.apSSID, sizeof(appState.apSSID), newSSID.c_str());

  // Only update password if provided (otherwise keep existing)
  if (doc["password"].is<String>()) {
    String newPassword = doc["password"].as<String>();
    if (newPassword.length() >= 8) {
      setCharField(appState.apPassword, sizeof(appState.apPassword), newPassword.c_str());
      LOG_D("[WiFi] AP password updated");
    } else if (newPassword.length() > 0) {
      server.send(400, "application/json",
                  "{\"success\": false, \"message\": \"Password must be at "
                  "least 8 characters\"}");
      return;
    }
    // If password is empty string, keep existing password
  }

  LOG_I("[WiFi] AP configuration updated: SSID=%s", appState.apSSID);

  // If AP is currently running, restart it with new credentials
  if (appState.isAPMode) {
    LOG_I("[WiFi] Restarting AP with new configuration");
    WiFi.softAPdisconnect(true); // Disconnect all clients
    delay(100);

    if (WiFi.status() == WL_CONNECTED) {
      // If connected to WiFi, run in AP_STA mode
      WiFi.mode(WIFI_AP_STA);
    } else {
      // If not connected, run in AP-only mode
      WiFi.mode(WIFI_AP);
    }

    WiFi.softAP(appState.apSSID, appState.apPassword);
    LOG_I("[WiFi] AP restarted with new SSID: %s", appState.apSSID);
    LOG_I("[WiFi] AP IP: %s", WiFi.softAPIP().toString().c_str());
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
  appState.apEnabled = enabled;

  if (enabled) {
    if (!appState.isAPMode) {
      // Start AP mode (can run alongside STA mode)
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(appState.apSSID, appState.apPassword);

      // Start DNS server for AP mode
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
      appState.isAPMode = true;

      LOG_I("[WiFi] Access Point enabled");
      LOG_I("[WiFi] AP IP: %s", WiFi.softAPIP().toString().c_str());
    }
  } else {
    if (appState.isAPMode) {
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
      appState.isAPMode = false;
      LOG_I("[WiFi] Access Point disabled");
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
        LOG_D("[WiFi] Using stored password for network: %s", config.ssid.c_str());
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
  appState.wifiConnecting = true;
  appState.wifiConnectSuccess = false;
  appState.wifiNewIP[0] = '\0';
  appState.wifiConnectError[0] = '\0';

  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Connection initiated\"}");

  LOG_I("[WiFi] Network saved. Connection request queued for %s", config.ssid.c_str());
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

  LOG_D("[WiFi] Network saved: %s (without connecting)", config.ssid.c_str());
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
  LOG_D("[WiFi] Scanning for networks");

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
      LOG_W("[WiFi] Failed to start scan");
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

  LOG_D("[WiFi] Found %d unique networks", networks.size());

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
    LOG_I("[WiFi] Removing currently connected network: %s", removedConfig.ssid.c_str());
  }

  if (removeWiFiNetwork(index)) {
    server.send(200, "application/json", "{\"success\": true}");

    // If we were connected to the removed network, disconnect and try to
    // reconnect
    if (wasConnectedToRemovedNetwork) {
      LOG_I("[WiFi] Disconnecting from removed network");
      WiFi.disconnect();

      // Stop AP if it was running in STA+AP mode
      if (appState.isAPMode && WiFi.status() != WL_CONNECTED) {
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        appState.isAPMode = false;
      }

      // Try to connect to other saved networks, or start AP if none available
      delay(500); // Brief delay to allow disconnect to complete
      if (!connectToStoredNetworks()) {
        LOG_W("[WiFi] No saved networks available, AP mode started");
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

    LOG_I("[WiFi] Processing deferred connection request");
    pendingConnection.requested = false;

    // Set global connection variables for status reporting
    setCharField(appState.wifiSSID, sizeof(appState.wifiSSID), pendingConnection.config.ssid.c_str());
    setCharField(appState.wifiPassword, sizeof(appState.wifiPassword), pendingConnection.config.password.c_str());

    // IMPORTANT: Maintain AP mode if enabled so frontend doesn't lose
    // connection. This allows the UI to poll for status (success/failure)
    if (appState.apEnabled || appState.isAPMode) {
      LOG_D("[WiFi] Maintaining AP mode during connection attempt");
      WiFi.mode(WIFI_AP_STA);
      // Ensure AP is up if it wasn't
      if (!appState.isAPMode) {
        WiFi.softAP(appState.apSSID, appState.apPassword);
        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
        appState.isAPMode = true;
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
        LOG_E("[WiFi] Failed to configure static IP");
        appState.wifiConnectSuccess = false;
        setCharField(appState.wifiConnectError, sizeof(appState.wifiConnectError), "Invalid Static IP Configuration");
        appState.wifiConnecting = false;
        pendingConnection.config.clear();
        return;
      }
    } else {
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }

    LOG_I("[WiFi] Initiating connection to: %s", cfg.ssid.c_str());
    WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());
  }

  if (!appState.wifiConnecting)
    return;

  static unsigned long connectionStarted = 0;
  if (connectionStarted == 0) {
    connectionStarted = millis();
  }

  if (WiFi.status() == WL_CONNECTED) {
    appState.wifiConnectSuccess = true;
    appState.wifiConnecting = false;
    { String _tmp = WiFi.localIP().toString(); setCharField(appState.wifiNewIP, sizeof(appState.wifiNewIP), _tmp.c_str()); }
    appState.wifiConnectError[0] = '\0';
    connectionStarted = 0;
    pendingConnection.config.clear();

    LOG_I("[WiFi] Connected in background");
    LOG_I("[WiFi] IP address: %s", appState.wifiNewIP);

    // Sync time and setup services
    syncTimeWithNTP();
    setupMqtt();

    // Broadcast success to WebSocket clients
    sendWiFiStatus();
  } else if (millis() - connectionStarted > WIFI_CONNECT_TIMEOUT_MS) {
    appState.wifiConnectSuccess = false;
    appState.wifiConnecting = false;
    connectionStarted = 0;
    pendingConnection.config.clear();

    // Set timeout error if no specific disconnect reason was captured
    if (strlen(appState.wifiConnectError) == 0) {
      setCharField(appState.wifiConnectError, sizeof(appState.wifiConnectError), "Connection timed out - check password and signal");
    }
    LOG_W("[WiFi] Connection failed: %s", appState.wifiConnectError);

    // Restore AP mode if it was enabled
    if (appState.apEnabled && !appState.isAPMode) {
      LOG_I("[WiFi] Restoring AP mode after failed connection");
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(appState.apSSID, appState.apPassword);
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
      appState.isAPMode = true;
      LOG_I("[WiFi] AP restored at: %s", WiFi.softAPIP().toString().c_str());
    }

    sendWiFiStatus();
  }
}
