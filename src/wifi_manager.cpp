#include "wifi_manager.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include "mqtt_handler.h"
#include "ota_updater.h"
#include "websocket_handler.h"
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>

// DNS Server for Captive Portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

// External function declarations
extern int compareVersions(const String &v1, const String &v2);
extern void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload,
                           size_t length);

// External HTML page reference
extern const char apHtmlPage[] PROGMEM;

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

void connectToWiFi(const char *ssid, const char *password, bool useStaticIP,
                   const char *staticIP, const char *subnet,
                   const char *gateway, const char *dns1, const char *dns2) {
  WiFi.mode(WIFI_STA);

  // Configure static IP if enabled
  if (useStaticIP && strlen(staticIP) > 0) {
    IPAddress ip, gw, sn, d1, d2;

    if (ip.fromString(staticIP) && gw.fromString(gateway) &&
        sn.fromString(subnet)) {
      // Parse DNS servers (optional)
      if (strlen(dns1) > 0) {
        d1.fromString(dns1);
      }
      if (strlen(dns2) > 0) {
        d2.fromString(dns2);
      }

      // Apply static IP configuration
      if (WiFi.config(ip, gw, sn, d1, d2)) {
        DebugOut.printf("Static IP configured: %s\n", staticIP);
      } else {
        DebugOut.println("Failed to configure static IP");
      }
    } else {
      DebugOut.println("Invalid IP address format");
    }
  }

  WiFi.begin(ssid, password);

  DebugOut.print("Connecting to WiFi: ");
  DebugOut.println(ssid);
  if (useStaticIP) {
    DebugOut.printf("  Using Static IP: %s\n", staticIP);
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

    // If AP is enabled, keep it running alongside STA
    if (apEnabled && !isAPMode) {
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(apSSID.c_str(), apPassword);

      // Start DNS server for AP mode
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
      isAPMode = true;

      DebugOut.printf("Access Point also running at: %s\n",
                      WiFi.softAPIP().toString().c_str());
    } else if (!apEnabled) {
      if (isAPMode) {
        dnsServer.stop();
      }
      isAPMode = false;
    }

    server.stop();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    // Connect DebugSerial to WebSocket for log broadcasting
    DebugOut.setWebSocket(&webSocket);

    server.begin();
    DebugOut.println("=== Web server started on port 80 ===");
    DebugOut.println("=== WebSocket server started on port 81 ===");
    DebugOut.printf("Navigate to http://%s\n",
                    WiFi.localIP().toString().c_str());

    // Setup MQTT client
    setupMqtt();
  } else {
    DebugOut.println("WARNING: Failed to connect to WiFi. Starting AP mode...");
    startAccessPoint();
  }
}

// ===== WiFi Credentials Persistence =====

bool loadWiFiCredentials(String &ssid, String &password) {
  File file = LittleFS.open("/wifi_config.txt", "r");
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

void saveWiFiCredentials(const char *ssid, const char *password) {
  File file = LittleFS.open("/wifi_config.txt", "w");
  if (file) {
    file.println(ssid);
    file.println(password);
    file.close();
    DebugOut.println("Credentials saved to LittleFS");
  }
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

// Save WiFi network (add or update)
bool saveWiFiNetwork(const char *ssid, const char *password, bool useStaticIP,
                     const char *staticIP, const char *subnet,
                     const char *gateway, const char *dns1, const char *dns2) {
  if (ssid == nullptr || strlen(ssid) == 0) {
    return false;
  }

  Preferences prefs;
  prefs.begin("wifi-list", false);

  int count = prefs.getUChar("count", 0);

  // Check if SSID already exists
  for (int i = 0; i < count; i++) {
    String existingSSID = prefs.getString(("s" + String(i)).c_str(), "");
    if (existingSSID == ssid) {
      // Update existing network (password and static IP config)
      // Only update password if provided (non-empty)
      if (strlen(password) > 0) {
        prefs.putString(("p" + String(i)).c_str(), password);
      }
      prefs.putBool(("static" + String(i)).c_str(), useStaticIP);
      prefs.putString(("ip" + String(i)).c_str(), staticIP);
      prefs.putString(("subnet" + String(i)).c_str(), subnet);
      prefs.putString(("gw" + String(i)).c_str(), gateway);
      prefs.putString(("dns1_" + String(i)).c_str(), dns1);
      prefs.putString(("dns2_" + String(i)).c_str(), dns2);
      prefs.end();
      DebugOut.printf("Updated network: %s (Static IP: %s)\n", ssid,
                      useStaticIP ? "enabled" : "disabled");
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
  prefs.putString(("s" + String(count)).c_str(), ssid);
  prefs.putString(("p" + String(count)).c_str(), password);
  prefs.putBool(("static" + String(count)).c_str(), useStaticIP);
  prefs.putString(("ip" + String(count)).c_str(), staticIP);
  prefs.putString(("subnet" + String(count)).c_str(), subnet);
  prefs.putString(("gw" + String(count)).c_str(), gateway);
  prefs.putString(("dns1_" + String(count)).c_str(), dns1);
  prefs.putString(("dns2_" + String(count)).c_str(), dns2);
  prefs.putUChar("count", count + 1);
  prefs.end();

  DebugOut.printf("Saved new network: %s (total: %d, Static IP: %s)\n", ssid,
                  count + 1, useStaticIP ? "enabled" : "disabled");
  return true;
}

// Remove network by index
bool removeWiFiNetwork(int index) {
  Preferences prefs;
  prefs.begin("wifi-list", false);

  int count = prefs.getUChar("count", 0);

  if (index < 0 || index >= count) {
    prefs.end();
    return false;
  }

  // Shift all networks after this index down by one
  for (int i = index; i < count - 1; i++) {
    String ssid = prefs.getString(("s" + String(i + 1)).c_str(), "");
    String pass = prefs.getString(("p" + String(i + 1)).c_str(), "");
    bool useStatic = prefs.getBool(("static" + String(i + 1)).c_str(), false);
    String ip = prefs.getString(("ip" + String(i + 1)).c_str(), "");
    String subnet = prefs.getString(("subnet" + String(i + 1)).c_str(), "");
    String gw = prefs.getString(("gw" + String(i + 1)).c_str(), "");
    String dns1 = prefs.getString(("dns1_" + String(i + 1)).c_str(), "");
    String dns2 = prefs.getString(("dns2_" + String(i + 1)).c_str(), "");

    prefs.putString(("s" + String(i)).c_str(), ssid);
    prefs.putString(("p" + String(i)).c_str(), pass);
    prefs.putBool(("static" + String(i)).c_str(), useStatic);
    prefs.putString(("ip" + String(i)).c_str(), ip);
    prefs.putString(("subnet" + String(i)).c_str(), subnet);
    prefs.putString(("gw" + String(i)).c_str(), gw);
    prefs.putString(("dns1_" + String(i)).c_str(), dns1);
    prefs.putString(("dns2_" + String(i)).c_str(), dns2);
  }

  // Remove the last entry
  prefs.remove(("s" + String(count - 1)).c_str());
  prefs.remove(("p" + String(count - 1)).c_str());
  prefs.remove(("static" + String(count - 1)).c_str());
  prefs.remove(("ip" + String(count - 1)).c_str());
  prefs.remove(("subnet" + String(count - 1)).c_str());
  prefs.remove(("gw" + String(count - 1)).c_str());
  prefs.remove(("dns1_" + String(count - 1)).c_str());
  prefs.remove(("dns2_" + String(count - 1)).c_str());
  prefs.putUChar("count", count - 1);

  prefs.end();
  DebugOut.printf("Removed network at index %d\n", index);
  return true;
}

// Try connecting to all saved networks in order
bool connectToStoredNetworks() {
  Preferences prefs;
  prefs.begin("wifi-list", true); // Read-only

  int count = prefs.getUChar("count", 0);

  if (count == 0) {
    prefs.end();
    DebugOut.println("No saved WiFi networks. Starting AP mode...");
    startAccessPoint(); // Fix: Ensure AP starts if list is empty
    return false;
  }

  DebugOut.printf("Trying %d saved network(s)...\n", count);

  for (int i = 0; i < count; i++) {
    String ssid = prefs.getString(("s" + String(i)).c_str(), "");
    String password = prefs.getString(("p" + String(i)).c_str(), "");
    bool useStatic = prefs.getBool(("static" + String(i)).c_str(), false);
    String ip = prefs.getString(("ip" + String(i)).c_str(), "");
    String subnet = prefs.getString(("subnet" + String(i)).c_str(), "");
    String gw = prefs.getString(("gw" + String(i)).c_str(), "");
    String dns1 = prefs.getString(("dns1_" + String(i)).c_str(), "");
    String dns2 = prefs.getString(("dns2_" + String(i)).c_str(), "");

    if (ssid.length() == 0) {
      continue;
    }

    DebugOut.printf("Attempting connection %d/%d: %s\n", i + 1, count,
                    ssid.c_str());

    WiFi.mode(WIFI_STA);

    // Configure static IP if enabled
    if (useStatic && ip.length() > 0) {
      IPAddress ipAddr, gwAddr, snAddr, dns1Addr, dns2Addr;
      if (ipAddr.fromString(ip) && gwAddr.fromString(gw) &&
          snAddr.fromString(subnet)) {
        if (dns1.length() > 0)
          dns1Addr.fromString(dns1);
        if (dns2.length() > 0)
          dns2Addr.fromString(dns2);

        WiFi.config(ipAddr, gwAddr, snAddr, dns1Addr, dns2Addr);
        DebugOut.printf("  Using Static IP: %s\n", ip.c_str());
      }
    } else {
      DebugOut.println("  Using DHCP");
    }

    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startTime < WIFI_CONNECT_TIMEOUT) {
      delay(500);
      DebugOut.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      prefs.end();
      DebugOut.println("\nWiFi connected!");
      DebugOut.printf("Connected to: %s\n", ssid.c_str());
      DebugOut.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

      // Move successful network to index 0 (priority) if not already there
      if (i != 0) {
        Preferences prefsWrite;
        prefsWrite.begin("wifi-list", false);

        // Save the successful network
        String successSSID = ssid;
        String successPass = password;

        // Shift networks 0 to i-1 down by one
        for (int j = i; j > 0; j--) {
          String shiftSSID =
              prefsWrite.getString(("s" + String(j - 1)).c_str(), "");
          String shiftPass =
              prefsWrite.getString(("p" + String(j - 1)).c_str(), "");
          prefsWrite.putString(("s" + String(j)).c_str(), shiftSSID);
          prefsWrite.putString(("p" + String(j)).c_str(), shiftPass);
        }

        // Put successful network at index 0
        prefsWrite.putString("s0", successSSID);
        prefsWrite.putString("p0", successPass);
        prefsWrite.end();

        DebugOut.println("Moved successful network to priority position");
      }

      // Synchronize time with NTP
      syncTimeWithNTP();

      // Setup WebSocket and server
      if (apEnabled && !isAPMode) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(apSSID.c_str(), apPassword);

        // Start DNS server for AP mode
        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
        isAPMode = true;

        DebugOut.printf("Access Point also running at: %s\n",
                        WiFi.softAPIP().toString().c_str());
      } else if (!apEnabled) {
        if (isAPMode) {
          dnsServer.stop();
        }
        isAPMode = false;
      }

      server.stop();
      webSocket.begin();
      webSocket.onEvent(webSocketEvent);
      DebugOut.setWebSocket(&webSocket);
      server.begin();

      DebugOut.println("=== Web server started on port 80 ===");
      DebugOut.println("=== WebSocket server started on port 81 ===");
      DebugOut.printf("Navigate to http://%s\n",
                      WiFi.localIP().toString().c_str());

      // Setup MQTT
      setupMqtt();

      return true;
    } else {
      DebugOut.printf("\nFailed to connect to: %s\n", ssid.c_str());
    }
  }

  prefs.end();
  DebugOut.println("All networks failed. Starting AP mode...");
  startAccessPoint();
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

void handleAPRoot() { server.send_P(200, "text/html", apHtmlPage); }

void handleAPConfig() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid JSON\"}");
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

  saveWiFiCredentials(ssid.c_str(), password.c_str());

  // Start asynchronous connection
  wifiSSID = ssid;
  wifiPassword = password;
  wifiConnecting = true;
  wifiConnectSuccess = false;
  wifiNewIP = "";

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Connection initiated\"}");

  DebugOut.printf("Credentials saved. Connecting to %s in background...\n",
                  ssid.c_str());
}

void handleAPConfigUpdate() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid JSON\"}");
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
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid JSON\"}");
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
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }

  String ssid = doc["ssid"].as<String>();
  String password = doc["password"].as<String>();

  // Extract static IP configuration
  bool useStaticIP = doc["useStaticIP"] | false;
  String staticIP = doc["staticIP"] | "";
  String subnet = doc["subnet"] | "";
  String gateway = doc["gateway"] | "";
  String dns1 = doc["dns1"] | "";
  String dns2 = doc["dns2"] | "";

  if (ssid.length() == 0) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"SSID required\"}");
    return;
  }

  // Save to multi-WiFi list with static IP configuration
  if (!saveWiFiNetwork(ssid.c_str(), password.c_str(), useStaticIP,
                       staticIP.c_str(), subnet.c_str(), gateway.c_str(),
                       dns1.c_str(), dns2.c_str())) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Failed to save network. "
                "Maximum 5 networks reached.\"}");
    return;
  }

  // Start asynchronous connection
  wifiSSID = ssid;
  wifiPassword = password;
  wifiConnecting = true;
  wifiConnectSuccess = false;
  wifiNewIP = "";

  WiFi.mode(WIFI_AP_STA);

  // Configure static IP if enabled
  if (useStaticIP && staticIP.length() > 0) {
    IPAddress ip, gw, sn, d1, d2;
    if (ip.fromString(staticIP) && gw.fromString(gateway) &&
        sn.fromString(subnet)) {
      if (dns1.length() > 0)
        d1.fromString(dns1);
      if (dns2.length() > 0)
        d2.fromString(dns2);
      WiFi.config(ip, gw, sn, d1, d2);
      DebugOut.printf("Static IP configured: %s\n", staticIP.c_str());
    }
  }

  WiFi.begin(ssid.c_str(), password.c_str());

  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Connection initiated\"}");

  DebugOut.printf("Network saved. Connecting to %s in background...\n",
                  ssid.c_str());
}

void handleWiFiSave() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }

  String ssid = doc["ssid"].as<String>();
  String password = doc["password"].as<String>();

  // Extract static IP configuration
  bool useStaticIP = doc["useStaticIP"] | false;
  String staticIP = doc["staticIP"] | "";
  String subnet = doc["subnet"] | "";
  String gateway = doc["gateway"] | "";
  String dns1 = doc["dns1"] | "";
  String dns2 = doc["dns2"] | "";

  if (ssid.length() == 0) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"SSID required\"}");
    return;
  }

  // Save to multi-WiFi list with static IP configuration
  if (!saveWiFiNetwork(ssid.c_str(), password.c_str(), useStaticIP,
                       staticIP.c_str(), subnet.c_str(), gateway.c_str(),
                       dns1.c_str(), dns2.c_str())) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Failed to save network. "
                "Maximum 5 networks reached.\"}");
    return;
  }

  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Network settings saved\"}");

  DebugOut.printf("Network saved: %s (without connecting)\n", ssid.c_str());
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

  // Start async scan if not already scanning
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_FAILED) {
    // Start a new scan
    WiFi.scanNetworks(true); // Async scan
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

  DebugOut.printf("Found %d unique networks\n", networks.size());

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleWiFiList() {
  Preferences prefs;
  prefs.begin("wifi-list", true); // Read-only

  int count = prefs.getUChar("count", 0);

  JsonDocument doc;
  doc["success"] = true;
  doc["count"] = count;
  JsonArray networks = doc["networks"].to<JsonArray>();

  for (int i = 0; i < count; i++) {
    String ssid = prefs.getString(("s" + String(i)).c_str(), "");
    if (ssid.length() > 0) {
      JsonObject net = networks.add<JsonObject>();
      net["ssid"] = ssid;
      net["index"] = i;
      net["priority"] = (i == 0) ? true : false;

      // Include static IP configuration
      bool useStatic = prefs.getBool(("static" + String(i)).c_str(), false);
      net["useStaticIP"] = useStatic;
      if (useStatic) {
        net["staticIP"] = prefs.getString(("ip" + String(i)).c_str(), "");
        net["subnet"] = prefs.getString(("subnet" + String(i)).c_str(), "");
        net["gateway"] = prefs.getString(("gw" + String(i)).c_str(), "");
        net["dns1"] = prefs.getString(("dns1_" + String(i)).c_str(), "");
        net["dns2"] = prefs.getString(("dns2_" + String(i)).c_str(), "");
      }
    }
  }

  prefs.end();

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleWiFiRemove() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }

  if (!doc["index"].is<int>()) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Index required\"}");
    return;
  }

  int index = doc["index"].as<int>();

  // Get the SSID of the network being removed before removing it
  String removedSSID = "";
  bool wasConnectedToRemovedNetwork = false;

  Preferences prefs;
  prefs.begin("wifi-list", true); // Read-only first
  removedSSID = prefs.getString(("s" + String(index)).c_str(), "");
  prefs.end();

  // Check if we're currently connected to this network
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == removedSSID) {
    wasConnectedToRemovedNetwork = true;
    DebugOut.printf("Removing currently connected network: %s\n", removedSSID.c_str());
  }

  if (removeWiFiNetwork(index)) {
    server.send(200, "application/json", "{\"success\": true}");

    // If we were connected to the removed network, disconnect and try to reconnect
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
    connectionStarted = 0;

    DebugOut.println("\nWiFi connected in background!");
    DebugOut.printf("IP address: %s\n", wifiNewIP.c_str());

    // Sync time and setup services
    syncTimeWithNTP();
    setupMqtt();

    // Broadcast success to WebSocket clients
    sendWiFiStatus();
  } else if (millis() - connectionStarted > 20000) { // 20s timeout
    wifiConnectSuccess = false;
    wifiConnecting = false;
    connectionStarted = 0;
    DebugOut.println("\nWiFi connection failed (timeout)");
    sendWiFiStatus();
  }
}
