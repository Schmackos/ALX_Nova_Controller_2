#include "ota_updater.h"
#include "config.h"
#include "app_state.h"
#include "debug_serial.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <mbedtls/md.h>
#include <time.h>

// External functions from main.cpp that OTA needs to call
extern int compareVersions(const String& v1, const String& v2);
extern void sendWiFiStatus();

// ===== OTA Status Broadcasting =====
void broadcastUpdateStatus() {
  JsonDocument doc;
  doc["type"] = "updateStatus";
  doc["status"] = otaStatus;
  doc["progress"] = otaProgress;
  doc["message"] = otaStatusMessage;
  doc["updateAvailable"] = updateAvailable;
  doc["currentVersion"] = firmwareVer;
  doc["latestVersion"] = cachedLatestVersion;
  doc["autoUpdateEnabled"] = autoUpdateEnabled;
  
  if (otaTotalBytes > 0) {
    doc["bytesDownloaded"] = otaProgressBytes;
    doc["totalBytes"] = otaTotalBytes;
  }
  
  if (autoUpdateEnabled && updateAvailable && updateDiscoveredTime > 0) {
    unsigned long elapsed = millis() - updateDiscoveredTime;
    unsigned long remaining = (elapsed < AUTO_UPDATE_COUNTDOWN) 
                              ? (AUTO_UPDATE_COUNTDOWN - elapsed) / 1000 
                              : 0;
    doc["countdownSeconds"] = remaining;
  } else {
    doc["countdownSeconds"] = 0;
  }
  
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

// ===== OTA HTTP API Handlers =====

void handleCheckUpdate() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(200, "application/json", "{\"success\": false, \"message\": \"Not connected to WiFi\"}");
    return;
  }
  
  DebugOut.println("Manual update check requested");
  
  // Trigger fresh check
  checkForFirmwareUpdate();
  
  JsonDocument doc;
  doc["success"] = true;
  doc["currentVersion"] = firmwareVer;
  doc["latestVersion"] = cachedLatestVersion.length() > 0 ? cachedLatestVersion : "Unknown";
  doc["updateAvailable"] = updateAvailable;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleStartUpdate() {
  if (otaInProgress) {
    server.send(200, "application/json", "{\"success\": false, \"message\": \"OTA update already in progress\"}");
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    server.send(200, "application/json", "{\"success\": false, \"message\": \"Not connected to WiFi\"}");
    return;
  }
  
  if (!updateAvailable || cachedLatestVersion.length() == 0 || cachedFirmwareUrl.length() == 0) {
    server.send(200, "application/json", "{\"success\": false, \"message\": \"No update available\"}");
    return;
  }
  
  DebugOut.println("Manual OTA update started");
  otaStatus = "downloading";
  otaProgress = 0;
  
  server.send(200, "application/json", "{\"success\": true, \"message\": \"Update started\"}");
  
  // Perform update in background (non-blocking)
  // Note: This is a simplified approach. In production, you might want to use a task
  bool updateSuccess = performOTAUpdate(cachedFirmwareUrl);
  
  if (updateSuccess) {
    otaStatus = "complete";
    DebugOut.println("OTA update successful! Rebooting...");
    delay(2000);
    ESP.restart();
  } else {
    otaStatus = "error";
    otaInProgress = false;
  }
}

void handleUpdateStatus() {
  JsonDocument doc;
  doc["status"] = otaStatus;
  doc["progress"] = otaProgress;
  doc["message"] = otaStatusMessage;
  
  if (otaTotalBytes > 0) {
    doc["bytesDownloaded"] = otaProgressBytes;
    doc["totalBytes"] = otaTotalBytes;
  }
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// Release Notes Handler
void handleGetReleaseNotes() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(200, "application/json", "{\"success\": false, \"message\": \"Not connected to WiFi\"}");
    return;
  }
  
  if (!server.hasArg("version")) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Version parameter required\"}");
    return;
  }
  
  String version = server.arg("version");
  
  // Fetch release notes from GitHub releases API with secure HTTPS
  String releaseNotesUrl = String("https://api.github.com/repos/") + githubRepoOwner + "/" + githubRepoName + "/releases/tags/" + version;
  
  DebugOut.printf("Fetching release notes from: %s\n", releaseNotesUrl.c_str());
  
  WiFiClientSecure client;
  
  if (enableCertValidation) {
    client.setCACert(github_root_ca.c_str());
  } else {
    client.setInsecure();  // Skip certificate validation
  }
  
  client.setTimeout(10000);
  
  HTTPClient https;
  if (!https.begin(client, releaseNotesUrl)) {
    server.send(200, "application/json", "{\"success\": false, \"message\": \"Failed to initialize secure connection\"}");
    return;
  }
  
  https.addHeader("Accept", "application/vnd.github.v3+json");
  https.addHeader("User-Agent", "ESP32-OTA-Updater");
  https.setTimeout(10000);
  
  int httpCode = https.GET();
  
  JsonDocument doc;
  if (httpCode == HTTP_CODE_OK) {
    String response = https.getString();
    
    // Parse GitHub API response
    JsonDocument apiDoc;
    DeserializationError error = deserializeJson(apiDoc, response);
    
    if (!error && apiDoc["body"].is<String>()) {
      String releaseNotes = apiDoc["body"].as<String>();
      doc["success"] = true;
      doc["version"] = version;
      doc["notes"] = releaseNotes;
      doc["url"] = String("https://github.com/") + githubRepoOwner + "/" + githubRepoName + "/releases/tag/" + version;
    } else {
      doc["success"] = false;
      doc["message"] = "Failed to parse release notes";
      doc["notes"] = "Could not parse release notes for version " + version;
    }
  } else {
    doc["success"] = false;
    doc["message"] = "Release notes not found";
    doc["notes"] = "No release notes available for version " + version + "\n\nYou can view releases at:\nhttps://github.com/" + String(githubRepoOwner) + "/" + String(githubRepoName) + "/releases";
  }
  
  String json;
  serializeJson(doc, json);
  https.end();
  server.send(200, "application/json", json);
}

// ===== NTP Time Synchronization =====
void syncTimeWithNTP() {
  DebugOut.println("\n=== Synchronizing Time with NTP ===");
  DebugOut.printf("Timezone offset: %d seconds (%.1f hours)\n", timezoneOffset, timezoneOffset / 3600.0);
  
  // Configure NTP with timezone offset (no DST offset for simplicity)
  configTime(timezoneOffset, 0, "pool.ntp.org", "time.nist.gov");
  
  DebugOut.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  int attempts = 0;
  
  while (now < 1000000000 && attempts < 20) {  // Wait until we have a valid timestamp
    delay(500);
    DebugOut.print(".");
    now = time(nullptr);
    attempts++;
  }
  
  if (now < 1000000000) {
    DebugOut.println("\n⚠️  Failed to sync time with NTP server");
    DebugOut.println("⚠️  SSL certificate validation may fail!");
  } else {
    DebugOut.println("\n✅ Time synchronized successfully");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      DebugOut.printf("📅 Current local time: %04d-%02d-%02d %02d:%02d:%02d\n",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
  }
}

// ===== OTA Core Functions =====

void checkForFirmwareUpdate() {
  if (otaInProgress) {
    return;
  }
  
  DebugOut.println("\n=== Checking for Firmware Update ===");
  DebugOut.printf("Current firmware version installed: %s\n", firmwareVer);
  
  String latestVersion = "";
  String firmwareUrl = "";
  String checksum = "";
  
  if (!getLatestReleaseInfo(latestVersion, firmwareUrl, checksum)) {
    DebugOut.println("Failed to retrieve release information");
    return;
  }
  
  latestVersion.trim();
  DebugOut.printf("Latest firmware version available: %s\n", latestVersion.c_str());
  
  int cmp = compareVersions(latestVersion, String(firmwareVer));
  
  if (cmp > 0 && cachedLatestVersion != latestVersion) {
    // New update detected
    updateAvailable = true;
    cachedLatestVersion = latestVersion;
    cachedFirmwareUrl = firmwareUrl;
    cachedChecksum = checksum;
    updateDiscoveredTime = millis();
    DebugOut.printf("New version available: %s\n", latestVersion.c_str());
    if (checksum.length() > 0) {
      DebugOut.printf("SHA256 checksum: %s\n", checksum.c_str());
    }
    broadcastUpdateStatus();
    sendWiFiStatus();  // Broadcast to update UI
  } else if (cmp <= 0) {
    // Up to date or downgrade
    updateAvailable = false;
    cachedLatestVersion = latestVersion;  // Keep the version even if up-to-date
    cachedFirmwareUrl = firmwareUrl;
    cachedChecksum = checksum;
    updateDiscoveredTime = 0;
    if (cmp == 0) {
      DebugOut.println("Firmware is up to date!");
    } else {
      DebugOut.println("Remote firmware version is older; skipping downgrade.");
    }
    sendWiFiStatus();  // Broadcast to update UI
  }
}

// Get latest release information from GitHub API
bool getLatestReleaseInfo(String& version, String& firmwareUrl, String& checksum) {
  WiFiClientSecure client;
  
  if (enableCertValidation) {
    DebugOut.println("🔐 Certificate validation: ENABLED");
    client.setCACert(github_root_ca.c_str());
  } else {
    DebugOut.println("⚠️  Certificate validation: DISABLED (insecure mode)");
    client.setInsecure();  // Skip certificate validation
  }
  
  client.setTimeout(15000);
  
  HTTPClient https;
  String apiUrl = String("https://api.github.com/repos/") + githubRepoOwner + "/" + githubRepoName + "/releases/latest";
  
  DebugOut.printf("Fetching release info from: %s\n", apiUrl.c_str());
  
  if (!https.begin(client, apiUrl)) {
    DebugOut.println("❌ Failed to initialize HTTPS connection");
    return false;
  }
  
  https.addHeader("Accept", "application/vnd.github.v3+json");
  https.addHeader("User-Agent", "ESP32-OTA-Updater");
  https.setTimeout(15000);
  
  DebugOut.println("📡 Performing HTTPS request...");
  int httpCode = https.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    DebugOut.printf("❌ Failed to get release info. HTTP code: %d\n", httpCode);
    if (httpCode == -1) {
      DebugOut.println("⚠️  Connection failed - possible causes:");
      DebugOut.println("   - SSL certificate validation failed (check NTP time sync)");
      DebugOut.println("   - Network/firewall blocking HTTPS");
      DebugOut.println("   - GitHub API temporarily unavailable");
    }
    https.end();
    return false;
  }
  
  DebugOut.println("✅ HTTPS request successful");
  
  String payload = https.getString();
  https.end();
  
  // Parse JSON response
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    DebugOut.printf("JSON parsing failed: %s\n", error.c_str());
    return false;
  }
  
  // Extract version from tag_name
  if (!doc["tag_name"].is<String>()) {
    DebugOut.println("No tag_name found in release");
    return false;
  }
  
  version = doc["tag_name"].as<String>();
  
  // Find firmware.bin in assets
  JsonArray assets = doc["assets"].as<JsonArray>();
  bool foundFirmware = false;
  
  for (JsonObject asset : assets) {
    String assetName = asset["name"].as<String>();
    
    if (assetName == "firmware.bin") {
      firmwareUrl = asset["browser_download_url"].as<String>();
      foundFirmware = true;
      DebugOut.printf("Found firmware asset: %s\n", firmwareUrl.c_str());
    } else if (assetName == "firmware.bin.sha256") {
      // If there's a separate checksum file, we could download it
      // For now, we'll look for it in the release body
      String checksumUrl = asset["browser_download_url"].as<String>();
      DebugOut.printf("Found checksum file: %s\n", checksumUrl.c_str());
    }
  }
  
  if (!foundFirmware) {
    DebugOut.println("firmware.bin not found in release assets");
    return false;
  }
  
  // Try to extract checksum from release body if available
  if (doc["body"].is<String>()) {
    String body = doc["body"].as<String>();
    // Look for SHA256 pattern in release notes
    int shaIndex = body.indexOf("SHA256:");
    if (shaIndex == -1) {
      shaIndex = body.indexOf("sha256:");
    }
    if (shaIndex != -1) {
      // Extract the next 64 hex characters
      String temp = body.substring(shaIndex);
      int start = temp.indexOf(':') + 1;
      while (start < temp.length() && (temp[start] == ' ' || temp[start] == '\n' || temp[start] == '\r')) {
        start++;
      }
      checksum = temp.substring(start, start + 64);
      checksum.trim();
      // Validate it's a proper hex string
      if (checksum.length() == 64) {
        bool validHex = true;
        for (int i = 0; i < 64; i++) {
          char c = checksum[i];
          if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            validHex = false;
            break;
          }
        }
        if (!validHex) {
          checksum = "";
        }
      } else {
        checksum = "";
      }
    }
  }
  
  return true;
}

// Calculate SHA256 hash of data
String calculateSHA256(uint8_t* data, size_t len) {
  byte shaResult[32];
  
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, data, len);
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);
  
  // Convert to hex string
  String hashString = "";
  for (int i = 0; i < 32; i++) {
    char str[3];
    sprintf(str, "%02x", shaResult[i]);
    hashString += str;
  }
  
  return hashString;
}

bool performOTAUpdate(String firmwareUrl) {
  otaInProgress = true;
  otaStatus = "preparing";
  otaStatusMessage = "Preparing for update...";
  otaProgress = 0;
  broadcastUpdateStatus();
  
  DebugOut.println("\n=== Starting OTA Update ===");
  DebugOut.printf("Downloading from: %s\n", firmwareUrl.c_str());
  
  // Use secure HTTPS connection with certificate validation
  WiFiClientSecure client;
  
  if (enableCertValidation) {
    DebugOut.println("🔐 Certificate validation: ENABLED");
    client.setCACert(github_root_ca.c_str());
  } else {
    DebugOut.println("⚠️  Certificate validation: DISABLED (insecure mode)");
    client.setInsecure();  // Skip certificate validation
  }
  
  client.setTimeout(30000);
  
  HTTPClient https;
  if (!https.begin(client, firmwareUrl)) {
    DebugOut.println("❌ Failed to initialize HTTPS connection");
    otaStatus = "error";
    otaStatusMessage = "Failed to initialize secure connection";
    otaInProgress = false;
    broadcastUpdateStatus();
    return false;
  }
  
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  https.setTimeout(30000);
  
  otaStatusMessage = "Connecting to server...";
  broadcastUpdateStatus();
  
  int httpCode = https.GET();
  
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY && httpCode != HTTP_CODE_FOUND) {
    DebugOut.printf("❌ Failed to download firmware. HTTP code: %d\n", httpCode);
    https.end();
    otaStatus = "error";
    otaStatusMessage = "Failed to connect to server";
    otaInProgress = false;
    broadcastUpdateStatus();
    return false;
  }
  
  int contentLength = https.getSize();
  otaTotalBytes = contentLength;
  DebugOut.printf("📦 Firmware size: %d bytes (%.2f KB)\n", contentLength, contentLength / 1024.0);
  
  if (contentLength <= 0) {
    DebugOut.println("❌ Invalid firmware size");
    https.end();
    otaStatus = "error";
    otaStatusMessage = "Invalid firmware file";
    otaInProgress = false;
    broadcastUpdateStatus();
    return false;
  }
  
  // Check if enough space is available
  int freeSpace = ESP.getFreeSketchSpace() - 0x1000;
  if (contentLength > freeSpace) {
    DebugOut.printf("❌ Not enough space. Need: %d, Available: %d\n", contentLength, freeSpace);
    https.end();
    otaStatus = "error";
    otaStatusMessage = "Not enough storage space";
    otaInProgress = false;
    broadcastUpdateStatus();
    return false;
  }
  
  // Begin OTA update
  if (!Update.begin(contentLength)) {
    DebugOut.printf("❌ Failed to begin OTA. Free space: %d\n", ESP.getFreeSketchSpace());
    https.end();
    otaStatus = "error";
    otaStatusMessage = "Failed to initialize update";
    otaInProgress = false;
    broadcastUpdateStatus();
    return false;
  }
  
  otaProgress = 0;
  otaProgressBytes = 0;
  otaStatus = "downloading";
  otaStatusMessage = "Downloading firmware...";
  broadcastUpdateStatus();
  
  DebugOut.println("📥 Downloading firmware to flash...");
  
  WiFiClient* stream = https.getStreamPtr();
  
  size_t written = 0;
  uint8_t buffer[1024];
  unsigned long lastBroadcast = 0;
  
  // For checksum calculation
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  bool calculatingChecksum = (cachedChecksum.length() == 64);
  
  if (calculatingChecksum) {
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    DebugOut.println("🔐 Checksum verification enabled");
  }
  
  while (https.connected() && (written < contentLength)) {
    size_t available = stream->available();
    if (available) {
      int bytesRead = stream->readBytes(buffer, min(available, sizeof(buffer)));
      
      // Update checksum calculation
      if (calculatingChecksum) {
        mbedtls_md_update(&ctx, buffer, bytesRead);
      }
      
      if (Update.write(buffer, bytesRead) != bytesRead) {
        DebugOut.println("❌ Error writing firmware data");
        Update.abort();
        if (calculatingChecksum) {
          mbedtls_md_free(&ctx);
        }
        https.end();
        otaStatus = "error";
        otaStatusMessage = "Write error during download";
        otaInProgress = false;
        broadcastUpdateStatus();
        return false;
      }
      
      written += bytesRead;
      otaProgressBytes = written;
      
      // Update progress percentage
      otaProgress = (written * 100) / contentLength;
      
      // Broadcast every 5% or every 2 seconds
      unsigned long now = millis();
      if (otaProgress % 5 == 0 || (now - lastBroadcast) >= 2000) {
        otaStatusMessage = String("Downloading: ") + String(written / 1024) + " / " + String(contentLength / 1024) + " KB";
        broadcastUpdateStatus();
        lastBroadcast = now;
        DebugOut.printf("📊 Progress: %d%% (%d KB / %d KB)\n", otaProgress, written / 1024, contentLength / 1024);
      }
    }
    delay(1);
  }
  
  https.end();
  
  // Verify checksum if available
  if (calculatingChecksum) {
    byte shaResult[32];
    mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);
    
    // Convert to hex string
    String calculatedChecksum = "";
    for (int i = 0; i < 32; i++) {
      char str[3];
      sprintf(str, "%02x", shaResult[i]);
      calculatedChecksum += str;
    }
    
    DebugOut.printf("📋 Expected checksum:   %s\n", cachedChecksum.c_str());
    DebugOut.printf("📋 Calculated checksum: %s\n", calculatedChecksum.c_str());
    
    if (calculatedChecksum.equalsIgnoreCase(cachedChecksum)) {
      DebugOut.println("✅ Checksum verification passed!");
    } else {
      DebugOut.println("❌ Checksum verification FAILED!");
      Update.abort();
      otaStatus = "error";
      otaStatusMessage = "Checksum verification failed - firmware corrupted";
      otaInProgress = false;
      broadcastUpdateStatus();
      return false;
    }
  } else {
    DebugOut.println("⚠️  No checksum available for verification");
  }
  
  otaStatusMessage = "Verifying firmware...";
  otaProgress = 100;
  broadcastUpdateStatus();
  DebugOut.println("✅ Download complete. Verifying...");
  
  if (Update.end()) {
    if (Update.isFinished()) {
      DebugOut.println("✅ OTA update completed successfully!");
      DebugOut.println("🔄 Rebooting device in 3 seconds...");
      otaProgress = 100;
      otaStatus = "complete";
      otaStatusMessage = "Update complete! Rebooting...";
      otaInProgress = false;
      broadcastUpdateStatus();
      return true;
    } else {
      DebugOut.println("❌ OTA update did not finish correctly");
      Update.abort();
      otaStatus = "error";
      otaStatusMessage = "Update verification failed";
      otaInProgress = false;
      broadcastUpdateStatus();
      return false;
    }
  } else {
    DebugOut.printf("❌ OTA update error: %s\n", Update.errorString());
    Update.abort();
    otaStatus = "error";
    otaStatusMessage = String("Update error: ") + Update.errorString();
    otaInProgress = false;
    broadcastUpdateStatus();
    return false;
  }
}
