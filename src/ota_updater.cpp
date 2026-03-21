#include "ota_updater.h"
#include "config.h"
#include "app_state.h"
#include "globals.h"
#include "debug_serial.h"
#include "utils.h"
#include "buzzer_handler.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <mbedtls/md.h>
#include <time.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Root CA certificates for GitHub (api.github.com, github.com, and objects.githubusercontent.com)
// Sectigo migrated from USERTrust to R46/E46 roots in 2025 (mandatory from Jan 2026).
// DigiCert G2 handles CDN downloads on objects.githubusercontent.com.
// Total: 3 certificates (R46 RSA, E46 ECC, DigiCert G2), valid until 2038-2046
static const char* GITHUB_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFijCCA3KgAwIBAgIQdY39i658BwD6qSWn4cetFDANBgkqhkiG9w0BAQwFADBf\n" \
"MQswCQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQD\n" \
"Ey1TZWN0aWdvIFB1YmxpYyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBSNDYw\n" \
"HhcNMjEwMzIyMDAwMDAwWhcNNDYwMzIxMjM1OTU5WjBfMQswCQYDVQQGEwJHQjEY\n" \
"MBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1TZWN0aWdvIFB1Ymxp\n" \
"YyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBSNDYwggIiMA0GCSqGSIb3DQEB\n" \
"AQUAA4ICDwAwggIKAoICAQCTvtU2UnXYASOgHEdCSe5jtrch/cSV1UgrJnwUUxDa\n" \
"ef0rty2k1Cz66jLdScK5vQ9IPXtamFSvnl0xdE8H/FAh3aTPaE8bEmNtJZlMKpnz\n" \
"SDBh+oF8HqcIStw+KxwfGExxqjWMrfhu6DtK2eWUAtaJhBOqbchPM8xQljeSM9xf\n" \
"iOefVNlI8JhD1mb9nxc4Q8UBUQvX4yMPFF1bFOdLvt30yNoDN9HWOaEhUTCDsG3X\n" \
"ME6WW5HwcCSrv0WBZEMNvSE6Lzzpng3LILVCJ8zab5vuZDCQOc2TZYEhMbUjUDM3\n" \
"IuM47fgxMMxF/mL50V0yeUKH32rMVhlATc6qu/m1dkmU8Sf4kaWD5QazYw6A3OAS\n" \
"VYCmO2a0OYctyPDQ0RTp5A1NDvZdV3LFOxxHVp3i1fuBYYzMTYCQNFu31xR13NgE\n" \
"SJ/AwSiItOkcyqex8Va3e0lMWeUgFaiEAin6OJRpmkkGj80feRQXEgyDet4fsZfu\n" \
"+Zd4KKTIRJLpfSYFplhym3kT2BFfrsU4YjRosoYwjviQYZ4ybPUHNs2iTG7sijbt\n" \
"8uaZFURww3y8nDnAtOFr94MlI1fZEoDlSfB1D++N6xybVCi0ITz8fAr/73trdf+L\n" \
"HaAZBav6+CuBQug4urv7qv094PPK306Xlynt8xhW6aWWrL3DkJiy4Pmi1KZHQ3xt\n" \
"zwIDAQABo0IwQDAdBgNVHQ4EFgQUVnNYZJX5khqwEioEYnmhQBWIIUkwDgYDVR0P\n" \
"AQH/BAQDAgGGMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAC9c\n" \
"mTz8Bl6MlC5w6tIyMY208FHVvArzZJ8HXtXBc2hkeqK5Duj5XYUtqDdFqij0lgVQ\n" \
"YKlJfp/imTYpE0RHap1VIDzYm/EDMrraQKFz6oOht0SmDpkBm+S8f74TlH7Kph52\n" \
"gDY9hAaLMyZlbcp+nv4fjFg4exqDsQ+8FxG75gbMY/qB8oFM2gsQa6H61SilzwZA\n" \
"Fv97fRheORKkU55+MkIQpiGRqRxOF3yEvJ+M0ejf5lG5Nkc/kLnHvALcWxxPDkjB\n" \
"JYOcCj+esQMzEhonrPcibCTRAUH4WAP+JWgiH5paPHxsnnVI84HxZmduTILA7rpX\n" \
"DhjvLpr3Etiga+kFpaHpaPi8TD8SHkXoUsCjvxInebnMMTzD9joiFgOgyY9mpFui\n" \
"TdaBJQbpdqQACj7LzTWb4OE4y2BThihCQRxEV+ioratF4yUQvNs+ZUH7G6aXD+u5\n" \
"dHn5HrwdVw1Hr8Mvn4dGp+smWg9WY7ViYG4A++MnESLn/pmPNPW56MORcr3Ywx65\n" \
"LvKRRFHQV80MNNVIIb/bE/FmJUNS0nAiNs2fxBx1IK1jcmMGDw4nztJqDby1ORrp\n" \
"0XZ60Vzk50lJLVU3aPAaOpg+VBeHVOmmJ1CJeyAvP/+/oYtKR5j/K3tJPsMpRmAY\n" \
"QqszKbrAKbkTidOIijlBO8n9pu0f9GBj39ItVQGL\n" \
"-----END CERTIFICATE-----\n" \
"-----BEGIN CERTIFICATE-----\n" \
"MIICOjCCAcGgAwIBAgIQQvLM2htpN0RfFf51KBC49DAKBggqhkjOPQQDAzBfMQsw\n" \
"CQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1T\n" \
"ZWN0aWdvIFB1YmxpYyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBFNDYwHhcN\n" \
"MjEwMzIyMDAwMDAwWhcNNDYwMzIxMjM1OTU5WjBfMQswCQYDVQQGEwJHQjEYMBYG\n" \
"A1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1TZWN0aWdvIFB1YmxpYyBT\n" \
"ZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBFNDYwdjAQBgcqhkjOPQIBBgUrgQQA\n" \
"IgNiAAR2+pmpbiDt+dd34wc7qNs9Xzjoq1WmVk/WSOrsfy2qw7LFeeyZYX8QeccC\n" \
"WvkEN/U0NSt3zn8gj1KjAIns1aeibVvjS5KToID1AZTc8GgHHs3u/iVStSBDHBv+\n" \
"6xnOQ6OjQjBAMB0GA1UdDgQWBBTRItpMWfFLXyY4qp3W7usNw/upYTAOBgNVHQ8B\n" \
"Af8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAwNnADBkAjAn7qRa\n" \
"qCG76UeXlImldCBteU/IvZNeWBj7LRoAasm4PdCkT0RHlAFWovgzJQxC36oCMB3q\n" \
"4S6ILuH5px0CMk7yn2xVdOOurvulGu7t0vzCAxHrRVxgED1cf5kDW21USAGKcw==\n" \
"-----END CERTIFICATE-----\n" \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n" \
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n" \
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n" \
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n" \
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n" \
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n" \
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n" \
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n" \
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n" \
"1Yl9PMCcit0BnuQ/8Kt+cTxZp2nh5w1TBVB0El8eTKt/IfVt8+yS2TY+Hp1VxS1x\n" \
"mfo/7SfE85AmUptT/PCTTp/ShpMrnJN/QvmKp5h/Db0OuCFq5g0rLM3MnvsU2rFy\n" \
"dHWqZdMI7TaWvRw0ng8NDYMBUu0F0R6xz1BqeOlF+v6PvMRyhPEZ7D6V3AVMuX4a\n" \
"IjEcnAcLJlCH4nRmFBsKgyJhSz1cMq6cIsPE3ha6uPCTsjHRM63UPk5ZLkIQ5SeE\n" \
"MppdNrYpMYL7I5lGCNvAk/b7QBQg9T3VwW8E4L8PqlKjFz5E8k4sFTbLCS7Q/l6a\n" \
"o5c=\n" \
"-----END CERTIFICATE-----\n";

// External functions from main.cpp that OTA needs to call
extern int compareVersions(const String& v1, const String& v2);
extern void sendWiFiStatus();

// ===== FreeRTOS Task Handles =====
static TaskHandle_t otaDownloadTaskHandle = NULL;
static TaskHandle_t otaCheckTaskHandle = NULL;

// ===== OTA Backoff State =====
static int _otaConsecutiveFailures = 0;

// Returns backoff-aware check interval (ms) based on consecutive failures:
//   0-2 failures: 5 min (300s) — normal
//   3-5 failures: 15 min (900s)
//   6-9 failures: 30 min (1800s)
//   10+ failures: 60 min (3600s)
unsigned long getOTAEffectiveInterval() {
    if (_otaConsecutiveFailures >= 10) return 3600000UL;
    if (_otaConsecutiveFailures >= 6)  return 1800000UL;
    if (_otaConsecutiveFailures >= 3)  return 900000UL;
    return OTA_CHECK_INTERVAL;  // 300000 (5 min)
}

// ===== OTA Progress Helper (thread-safe via dirty flag) =====
static void setOTAProgress(const char* status, const char* message, int progress) {
  appState.ota.status = status;
  appState.ota.statusMessage = message;
  appState.ota.progress = progress;
  appState.markOTADirty();
}

// ===== OTA Status Broadcasting =====
void broadcastUpdateStatus() {
  JsonDocument doc;
  doc["type"] = "updateStatus";
  doc["status"] = appState.ota.status;
  doc["progress"] = appState.ota.progress;
  doc["message"] = appState.ota.statusMessage;
  doc["appState.updateAvailable"] = appState.ota.updateAvailable;
  doc["currentVersion"] = firmwareVer;
  doc["latestVersion"] = appState.ota.cachedLatestVersion;
  doc["appState.autoUpdateEnabled"] = appState.ota.autoUpdateEnabled;
  doc["amplifierInUse"] = appState.audio.amplifierState;
  doc["otaChannel"] = appState.ota.channel;
  
  if (appState.ota.totalBytes > 0) {
    doc["bytesDownloaded"] = appState.ota.progressBytes;
    doc["totalBytes"] = appState.ota.totalBytes;
  }
  
  // Show countdown when auto-update enabled, update available, amplifier is off, and countdown active
  if (appState.ota.autoUpdateEnabled && appState.ota.updateAvailable && !appState.audio.amplifierState && appState.ota.updateDiscoveredTime > 0) {
    unsigned long elapsed = millis() - appState.ota.updateDiscoveredTime;
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

  if (isOTATaskRunning()) {
    server.send(200, "application/json", "{\"success\": true, \"message\": \"Check already in progress\"}");
    return;
  }

  LOG_I("[OTA] Manual update check requested");

  // Launch non-blocking check — result arrives via WebSocket updateStatus broadcast
  startOTACheckTask();

  // Return immediately; actual version info will be pushed via WebSocket
  // Include any cached info we already have
  JsonDocument doc;
  doc["success"] = true;
  doc["message"] = "Checking for updates...";
  doc["currentVersion"] = firmwareVer;
  doc["latestVersion"] = appState.ota.cachedLatestVersion.length() > 0 ? appState.ota.cachedLatestVersion : "Checking...";
  doc["appState.updateAvailable"] = appState.ota.updateAvailable;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleStartUpdate() {
  if (appState.ota.inProgress || isOTATaskRunning()) {
    server.send(200, "application/json", "{\"success\": false, \"message\": \"OTA update already in progress\"}");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    server.send(200, "application/json", "{\"success\": false, \"message\": \"Not connected to WiFi\"}");
    return;
  }

  if (!appState.ota.updateAvailable || appState.ota.cachedLatestVersion.length() == 0 || appState.ota.cachedFirmwareUrl.length() == 0) {
    server.send(200, "application/json", "{\"success\": false, \"message\": \"No update available\"}");
    return;
  }

  LOG_I("[OTA] Manual OTA update started");

  // Send HTTP response immediately (non-blocking)
  server.send(200, "application/json", "{\"success\": true, \"message\": \"Update started\"}");

  // Launch OTA download in a FreeRTOS task
  startOTADownloadTask();
}

void handleUpdateStatus() {
  JsonDocument doc;
  doc["status"] = appState.ota.status;
  doc["progress"] = appState.ota.progress;
  doc["message"] = appState.ota.statusMessage;
  doc["appState.updateAvailable"] = appState.ota.updateAvailable;
  doc["currentVersion"] = firmwareVer;
  doc["latestVersion"] = appState.ota.cachedLatestVersion.length() > 0 ? appState.ota.cachedLatestVersion : "Unknown";
  doc["appState.autoUpdateEnabled"] = appState.ota.autoUpdateEnabled;
  doc["amplifierInUse"] = appState.audio.amplifierState;
  
  if (appState.ota.totalBytes > 0) {
    doc["bytesDownloaded"] = appState.ota.progressBytes;
    doc["totalBytes"] = appState.ota.totalBytes;
  }
  
  // Include countdown if auto-update is active
  if (appState.ota.autoUpdateEnabled && appState.ota.updateAvailable && !appState.audio.amplifierState && appState.ota.updateDiscoveredTime > 0) {
    unsigned long elapsed = millis() - appState.ota.updateDiscoveredTime;
    unsigned long remaining = (elapsed < AUTO_UPDATE_COUNTDOWN) 
                              ? (AUTO_UPDATE_COUNTDOWN - elapsed) / 1000 
                              : 0;
    doc["countdownSeconds"] = remaining;
  } else {
    doc["countdownSeconds"] = 0;
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
  
  LOG_I("[OTA] Fetching release notes from: %s", releaseNotesUrl.c_str());

  uint32_t maxBlock = ESP.getMaxAllocHeap();
  LOG_I("[OTA] Heap before TLS: free=%lu maxBlock=%lu", (unsigned long)ESP.getFreeHeap(), (unsigned long)maxBlock);
  if (maxBlock < 35000) {
    LOG_E("[OTA] Heap too low for TLS: largest block=%lu bytes (<35KB)", (unsigned long)maxBlock);
    server.send(200, "application/json", "{\"success\": false, \"message\": \"Insufficient memory for secure connection\"}");
    return;
  }

  WiFiClientSecure client;

  if (maxBlock < 50000) {
    LOG_W("[OTA] Heap low (%lu bytes), using insecure TLS (no cert validation)", (unsigned long)maxBlock);
    client.setInsecure();
  } else if (appState.general.enableCertValidation) {
    client.setCACert(GITHUB_ROOT_CA);
  } else {
    client.setInsecure();
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

// ===== OTA Core Functions =====

void checkForFirmwareUpdate() {
  if (appState.ota.inProgress) {
    return;
  }
  
  LOG_I("[OTA] Checking for firmware update");
  LOG_I("[OTA] Current firmware version: %s", firmwareVer);
  
  String latestVersion = "";
  String firmwareUrl = "";
  String checksum = "";
  
  if (!getLatestReleaseInfo(latestVersion, firmwareUrl, checksum)) {
    _otaConsecutiveFailures++;
    if (_otaConsecutiveFailures > 20) _otaConsecutiveFailures = 20;
    unsigned long nextInterval = getOTAEffectiveInterval();
    LOG_E("[OTA] Failed to retrieve release information (failures=%d, next check in %lus)",
          _otaConsecutiveFailures, nextInterval / 1000);
    return;
  }

  // Success — reset backoff
  if (_otaConsecutiveFailures > 0) {
    LOG_I("[OTA] Connection restored after %d consecutive failures", _otaConsecutiveFailures);
  }
  _otaConsecutiveFailures = 0;
  
  latestVersion.trim();
  LOG_I("[OTA] Latest firmware version available: %s", latestVersion.c_str());
  
  // Always update cached version info
  appState.ota.cachedLatestVersion = latestVersion;
  appState.ota.cachedFirmwareUrl = firmwareUrl;
  appState.ota.cachedChecksum = checksum;
  
  int cmp = compareVersions(latestVersion, String(firmwareVer));
  
  if (cmp > 0) {
    // Update available
    bool isNewUpdate = !appState.ota.updateAvailable;  // Track if this is a newly discovered update
    appState.ota.updateAvailable = true;
    
    if (isNewUpdate || appState.ota.updateDiscoveredTime == 0) {
      // Start countdown timer for new update or if timer was reset
      appState.ota.updateDiscoveredTime = millis();
      LOG_I("[OTA] New version available: %s", latestVersion.c_str());
      if (checksum.length() > 0) {
        LOG_I("[OTA] SHA256 checksum: %s", checksum.c_str());
      }
    } else {
      LOG_I("[OTA] Update still available: %s", latestVersion.c_str());
    }
  } else {
    // Up to date or downgrade
    appState.ota.updateAvailable = false;
    appState.ota.updateDiscoveredTime = 0;
    if (cmp == 0) {
      LOG_I("[OTA] Firmware is up to date");
    } else {
      LOG_W("[OTA] Remote firmware version is older, skipping downgrade");
    }
  }
  
  // Signal main loop to broadcast status update via dirty flag
  appState.markOTADirty();
}

// Get latest release information from GitHub API
bool getLatestReleaseInfo(String& version, String& firmwareUrl, String& checksum) {
  // Beta channel: use /releases list, pick newest overall (including prereleases)
  if (appState.ota.channel == 1) {
    if (!fetchReleaseList(1)) return false;
    if (appState.ota.cachedReleaseListCount == 0) return false;
    version = appState.ota.cachedReleaseList[0].version;
    firmwareUrl = appState.ota.cachedReleaseList[0].firmwareUrl;
    checksum = appState.ota.cachedReleaseList[0].checksum;
    return true;
  }
  // Stable channel: existing /releases/latest path below

  // TLS needs ~50KB contiguous: mbedTLS buffers + AES DMA descriptors + handshake state
  uint32_t maxBlock = ESP.getMaxAllocHeap();
  LOG_I("[OTA] Heap before TLS: free=%lu maxBlock=%lu", (unsigned long)ESP.getFreeHeap(), (unsigned long)maxBlock);
  if (maxBlock < 50000) {
    LOG_E("[OTA] Heap too low for TLS: largest block=%lu bytes (<50KB), skipping", (unsigned long)maxBlock);
    return false;
  }

  WiFiClientSecure client;

  if (maxBlock < 65000) {
    LOG_W("[OTA] Heap low (%lu bytes), using insecure TLS (no cert validation)", (unsigned long)maxBlock);
    client.setInsecure();
  } else if (appState.general.enableCertValidation) {
    LOG_I("[OTA] Certificate validation enabled");
    client.setCACert(GITHUB_ROOT_CA);
  } else {
    LOG_W("[OTA] Certificate validation disabled (insecure mode)");
    client.setInsecure();
  }

  client.setTimeout(15000);

  HTTPClient https;
  String apiUrl = String("https://api.github.com/repos/") + githubRepoOwner + "/" + githubRepoName + "/releases/latest";

  LOG_I("[OTA] Fetching release info from: %s", apiUrl.c_str());
  
  if (!https.begin(client, apiUrl)) {
    LOG_E("[OTA] Failed to initialize HTTPS connection");
    return false;
  }
  
  https.addHeader("Accept", "application/vnd.github.v3+json");
  https.addHeader("User-Agent", "ESP32-OTA-Updater");
  https.setTimeout(15000);
  
  LOG_I("[OTA] Performing HTTPS request");
  int httpCode = https.GET();

  if (httpCode != HTTP_CODE_OK) {
    LOG_E("[OTA] Failed to get release info, HTTP code: %d", httpCode);
    if (httpCode == -1) {
      LOG_E("[OTA] Connection failed - check SSL certs, NTP sync, network, or GitHub availability");
    }
    https.end();
    return false;
  }

  LOG_I("[OTA] HTTPS request successful");
  
  String payload = https.getString();
  https.end();
  
  // Parse JSON response
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    LOG_E("[OTA] JSON parsing failed: %s", error.c_str());
    return false;
  }
  
  // Extract version from tag_name
  if (!doc["tag_name"].is<String>()) {
    LOG_E("[OTA] No tag_name found in release");
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
      LOG_I("[OTA] Found firmware asset: %s", firmwareUrl.c_str());
    } else if (assetName == "firmware.bin.sha256") {
      // If there's a separate checksum file, we could download it
      // For now, we'll look for it in the release body
      String checksumUrl = asset["browser_download_url"].as<String>();
      LOG_I("[OTA] Found checksum file: %s", checksumUrl.c_str());
    }
  }
  
  if (!foundFirmware) {
    LOG_E("[OTA] firmware.bin not found in release assets");
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

// Shared helper: extract checksum from release body string
static String extractChecksumFromBody(const String& body) {
  int shaIndex = body.indexOf("SHA256:");
  if (shaIndex == -1) shaIndex = body.indexOf("sha256:");
  if (shaIndex == -1) return "";
  String temp = body.substring(shaIndex);
  int start = temp.indexOf(':') + 1;
  while (start < (int)temp.length() && (temp[start] == ' ' || temp[start] == '\n' || temp[start] == '\r')) {
    start++;
  }
  String cs = temp.substring(start, start + 64);
  cs.trim();
  if (cs.length() != 64) return "";
  for (int i = 0; i < 64; i++) {
    char c = cs[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return "";
  }
  return cs;
}

// Fetch a list of recent releases from GitHub, filtered by channel.
// Stores qualifying entries in appState.ota.cachedReleaseList[] up to maxCount.
// Returns true on success.
bool fetchReleaseList(int maxCount) {
  uint32_t maxBlock = ESP.getMaxAllocHeap();
  LOG_I("[OTA] fetchReleaseList: free=%lu maxBlock=%lu", (unsigned long)ESP.getFreeHeap(), (unsigned long)maxBlock);
  if (maxBlock < 50000) {
    LOG_E("[OTA] Heap too low for TLS: largest block=%lu bytes (<50KB), skipping", (unsigned long)maxBlock);
    return false;
  }

  // Retry loop: transient CDN/TLS issues may return non-array on first attempt
  for (int attempt = 0; attempt < 2; attempt++) {
    if (attempt > 0) {
      uint32_t retryBlock = ESP.getMaxAllocHeap();
      LOG_W("[OTA] fetchReleaseList: retrying with fresh TLS connection (maxBlock=%lu)", (unsigned long)retryBlock);
      delay(1500);
    }

    WiFiClientSecure client;
    // Reduce TLS I/O buffer from 16KB default to 4KB to avoid esp-aes DMA
    // descriptor allocation failure on fragmented internal SRAM heap

    if (maxBlock < 65000) {
      LOG_W("[OTA] Heap low (%lu bytes), using insecure TLS (no cert validation)", (unsigned long)maxBlock);
      client.setInsecure();
    } else if (appState.general.enableCertValidation) {
      client.setCACert(GITHUB_ROOT_CA);
    } else {
      client.setInsecure();
    }

    client.setTimeout(15000);

    HTTPClient https;
    String apiUrl = String("https://api.github.com/repos/") + githubRepoOwner + "/" + githubRepoName + "/releases?per_page=5";

    if (attempt == 0) {
      LOG_I("[OTA] Fetching release list from: %s", apiUrl.c_str());
    }

    if (!https.begin(client, apiUrl)) {
      LOG_E("[OTA] Failed to initialize HTTPS connection");
      https.end();
      continue;  // Try next attempt
    }

    https.addHeader("Accept", "application/vnd.github.v3+json");
    https.addHeader("User-Agent", "ESP32-OTA-Updater");
    https.setTimeout(15000);

    int httpCode = https.GET();

    if (httpCode != HTTP_CODE_OK) {
      LOG_E("[OTA] fetchReleaseList: HTTP code %d", httpCode);
      https.end();
      return false;  // HTTP errors are not transient — hard fail
    }

    // Use filter to reduce memory usage — only parse fields we need
    JsonDocument filter;
    JsonObject filterItem = filter[0].to<JsonObject>();
    filterItem["tag_name"] = true;
    filterItem["prerelease"] = true;
    filterItem["published_at"] = true;
    filterItem["body"] = true;
    filterItem["assets"][0]["name"] = true;
    filterItem["assets"][0]["browser_download_url"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, https.getStream(), DeserializationOption::Filter(filter));
    https.end();

    if (error) {
      if (error == DeserializationError::IncompleteInput && attempt == 0) {
        LOG_W("[OTA] fetchReleaseList: TLS dropped mid-stream (IncompleteInput), retrying...");
        continue;  // TLS connection dropped — transient, retry with fresh connection
      }
      LOG_E("[OTA] fetchReleaseList JSON parse failed: %s", error.c_str());
      return false;
    }

    if (!doc.is<JsonArray>()) {
      // Transient error: retry once with fresh connection
      if (attempt == 0) {
        continue;  // Try again
      }
      // Second attempt also failed
      LOG_E("[OTA] fetchReleaseList: response is not an array after retry");
      return false;
    }

    // Array received — parse releases
    int count = 0;
    int limit = maxCount < OTA_MAX_RELEASES ? maxCount : OTA_MAX_RELEASES;

    for (JsonObject rel : doc.as<JsonArray>()) {
      if (count >= limit) break;

      bool isPrerelease = rel["prerelease"] | false;

      // Channel filter: stable (0) skips prereleases; beta (1) includes all
      if (appState.ota.channel == 0 && isPrerelease) continue;

      String tagName = rel["tag_name"] | "";
      if (tagName.length() == 0) continue;

      // Find firmware.bin in assets
      String fwUrl;
      JsonArray assets = rel["assets"].as<JsonArray>();
      for (JsonObject asset : assets) {
        String assetName = asset["name"] | "";
        if (assetName == "firmware.bin") {
          fwUrl = asset["browser_download_url"] | "";
          break;
        }
      }
      if (fwUrl.length() == 0) continue;  // Skip releases without firmware.bin

      // Extract checksum from body
      String body = rel["body"] | "";
      String cs = extractChecksumFromBody(body);

      // Truncate published_at to YYYY-MM-DD
      String publishedAt = rel["published_at"] | "";
      if (publishedAt.length() > 10) publishedAt = publishedAt.substring(0, 10);

      appState.ota.cachedReleaseList[count].version = tagName;
      appState.ota.cachedReleaseList[count].firmwareUrl = fwUrl;
      appState.ota.cachedReleaseList[count].checksum = cs;
      appState.ota.cachedReleaseList[count].isPrerelease = isPrerelease;
      appState.ota.cachedReleaseList[count].publishedAt = publishedAt;
      count++;
    }

    appState.ota.cachedReleaseListCount = count;
    LOG_I("[OTA] fetchReleaseList: found %d qualifying release(s) (channel=%d)", count, appState.ota.channel);
    return true;
  }

  return false;  // Both attempts exhausted
}

// ===== New HTTP Handlers =====

void handleGetReleaseList() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Not connected to WiFi\"}");
    return;
  }
  if (appState.ota.inProgress) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"OTA in progress\"}");
    return;
  }

  bool ok = fetchReleaseList(OTA_MAX_RELEASES);

  JsonDocument doc;
  doc["success"] = ok;
  if (ok) {
    JsonArray arr = doc["releases"].to<JsonArray>();
    for (int i = 0; i < appState.ota.cachedReleaseListCount; i++) {
      JsonObject rel = arr.add<JsonObject>();
      rel["version"] = appState.ota.cachedReleaseList[i].version;
      rel["isPrerelease"] = appState.ota.cachedReleaseList[i].isPrerelease;
      rel["publishedAt"] = appState.ota.cachedReleaseList[i].publishedAt;
      rel["hasChecksum"] = appState.ota.cachedReleaseList[i].checksum.length() == 64;
    }
  } else {
    doc["message"] = "Failed to fetch release list";
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleInstallRelease() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"No data\"}");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }

  String targetVersion = doc["version"].as<String>();
  if (targetVersion.length() == 0) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Version required\"}");
    return;
  }

  // Find version in cached release list
  bool found = false;
  for (int i = 0; i < appState.ota.cachedReleaseListCount; i++) {
    if (appState.ota.cachedReleaseList[i].version == targetVersion) {
      appState.ota.cachedFirmwareUrl = appState.ota.cachedReleaseList[i].firmwareUrl;
      appState.ota.cachedChecksum = appState.ota.cachedReleaseList[i].checksum;
      appState.ota.cachedLatestVersion = targetVersion;
      found = true;
      break;
    }
  }

  if (!found) {
    server.send(404, "application/json", "{\"success\":false,\"message\":\"Version not in cached list. Browse releases first.\"}");
    return;
  }

  if (appState.ota.inProgress) {
    server.send(409, "application/json", "{\"success\":false,\"message\":\"OTA already in progress\"}");
    return;
  }

  appState.ota.updateAvailable = true;
  LOG_I("[OTA] User selected release %s for install", targetVersion.c_str());

  startOTADownloadTask();

  server.send(200, "application/json", "{\"success\":true,\"message\":\"Installing...\"}");
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
  appState.ota.inProgress = true;
  setOTAProgress("preparing", "Preparing for update...", 0);

  LOG_I("[OTA] Starting OTA update");
  LOG_I("[OTA] Downloading from: %s", firmwareUrl.c_str());

  // TLS needs ~50KB contiguous: mbedTLS buffers + AES DMA descriptors + handshake state
  uint32_t maxBlock = ESP.getMaxAllocHeap();
  LOG_I("[OTA] Heap before TLS: free=%lu maxBlock=%lu", (unsigned long)ESP.getFreeHeap(), (unsigned long)maxBlock);
  if (maxBlock < 50000) {
    LOG_E("[OTA] Heap too low for TLS: largest block=%lu bytes (<50KB)", (unsigned long)maxBlock);
    setOTAProgress("error", "Insufficient memory for secure download", 0);
    appState.ota.inProgress = false;
    return false;
  }

  WiFiClientSecure client;

  if (maxBlock < 65000) {
    LOG_W("[OTA] Heap low (%lu bytes), using insecure TLS (no cert validation)", (unsigned long)maxBlock);
    client.setInsecure();
  } else if (appState.general.enableCertValidation) {
    LOG_I("[OTA] Certificate validation enabled");
    client.setCACert(GITHUB_ROOT_CA);
  } else {
    LOG_W("[OTA] Certificate validation disabled (insecure mode)");
    client.setInsecure();
  }

  client.setTimeout(30000);

  HTTPClient https;
  if (!https.begin(client, firmwareUrl)) {
    LOG_E("[OTA] Failed to initialize HTTPS connection");
    setOTAProgress("error", "Failed to initialize secure connection", 0);
    appState.ota.inProgress = false;
    return false;
  }

  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  https.setTimeout(30000);

  setOTAProgress("preparing", "Connecting to server...", 0);

  int httpCode = https.GET();

  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY && httpCode != HTTP_CODE_FOUND) {
    LOG_E("[OTA] Failed to download firmware, HTTP code: %d", httpCode);
    https.end();
    setOTAProgress("error", "Failed to connect to server", 0);
    appState.ota.inProgress = false;
    return false;
  }

  int contentLength = https.getSize();
  appState.ota.totalBytes = contentLength;
  LOG_I("[OTA] Firmware size: %d bytes (%.2f KB)", contentLength, contentLength / 1024.0);

  if (contentLength <= 0) {
    LOG_E("[OTA] Invalid firmware size");
    https.end();
    setOTAProgress("error", "Invalid firmware file", 0);
    appState.ota.inProgress = false;
    return false;
  }

  // Check if enough space is available
  int freeSpace = ESP.getFreeSketchSpace() - 0x1000;
  if (contentLength > freeSpace) {
    LOG_E("[OTA] Not enough space, need: %d, available: %d", contentLength, freeSpace);
    https.end();
    setOTAProgress("error", "Not enough storage space", 0);
    appState.ota.inProgress = false;
    return false;
  }

  // Play OTA update melody before flashing begins
  buzzer_play_blocking(BUZZ_OTA_UPDATE, 850);

  // Begin OTA update
  if (!Update.begin(contentLength)) {
    LOG_E("[OTA] Failed to begin OTA, free space: %d", ESP.getFreeSketchSpace());
    https.end();
    setOTAProgress("error", "Failed to initialize update", 0);
    appState.ota.inProgress = false;
    return false;
  }

  appState.ota.progressBytes = 0;
  setOTAProgress("downloading", "Downloading firmware...", 0);

  LOG_I("[OTA] Download started, writing to flash");

  WiFiClient* stream = https.getStreamPtr();

  size_t written = 0;
  uint8_t buffer[1024];
  unsigned long lastProgressUpdate = 0;

  // For checksum calculation
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  bool calculatingChecksum = (appState.ota.cachedChecksum.length() == 64);

  if (calculatingChecksum) {
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    LOG_I("[OTA] Checksum verification enabled");
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
        LOG_E("[OTA] Error writing firmware data");
        Update.abort();
        if (calculatingChecksum) {
          mbedtls_md_free(&ctx);
        }
        https.end();
        setOTAProgress("error", "Write error during download", 0);
        appState.ota.inProgress = false;
        return false;
      }

      written += bytesRead;
      appState.ota.progressBytes = written;

      // Update progress every 1% change or every 2 seconds
      int newProgress = (written * 100) / contentLength;
      unsigned long now = millis();
      if (newProgress != appState.ota.progress || (now - lastProgressUpdate) >= 2000) {
        appState.ota.progress = newProgress;
        appState.ota.statusMessage = String("Downloading: ") + String(written / 1024) + " / " + String(contentLength / 1024) + " KB";
        appState.markOTADirty();
        lastProgressUpdate = now;
        LOG_D("[OTA] Progress: %d%% (%d KB / %d KB)", newProgress, written / 1024, contentLength / 1024);
      }
    }
    delay(1);  // Yield to FreeRTOS scheduler
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

    LOG_I("[OTA] Expected checksum:   %s", appState.ota.cachedChecksum.c_str());
    LOG_I("[OTA] Calculated checksum: %s", calculatedChecksum.c_str());

    if (calculatedChecksum.equalsIgnoreCase(appState.ota.cachedChecksum)) {
      LOG_I("[OTA] Checksum verification passed");
    } else {
      LOG_E("[OTA] Checksum verification failed");
      Update.abort();
      setOTAProgress("error", "Checksum verification failed - firmware corrupted", 0);
      appState.ota.inProgress = false;
      return false;
    }
  } else {
    LOG_W("[OTA] No checksum available for verification");
  }

  setOTAProgress("downloading", "Verifying firmware...", 100);
  LOG_I("[OTA] Download complete, verifying");

  if (Update.end()) {
    if (Update.isFinished()) {
      LOG_I("[OTA] Update completed successfully");
      LOG_I("[OTA] Rebooting device in 3 seconds");
      setOTAProgress("complete", "Update complete! Rebooting...", 100);
      return true;
    } else {
      LOG_E("[OTA] Update did not finish correctly");
      Update.abort();
      setOTAProgress("error", "Update verification failed", 0);
      appState.ota.inProgress = false;
      return false;
    }
  } else {
    LOG_E("[OTA] Update error: %s", Update.errorString());
    Update.abort();
    appState.ota.status = "error";
    appState.ota.statusMessage = String("Update error: ") + Update.errorString();
    appState.ota.progress = 0;
    appState.markOTADirty();
    appState.ota.inProgress = false;
    return false;
  }
}

// ===== OTA Success Flag Functions =====

// Save flag indicating OTA was successful (called before reboot)
void saveOTASuccessFlag(const String& previousVersion) {
  Preferences prefs;
  prefs.begin("ota", false);
  prefs.putBool("appState.justUpdated", true);
  prefs.putString("prevVersion", previousVersion);
  prefs.end();
  LOG_I("[OTA] Saved OTA success flag (previous version: %s)", previousVersion.c_str());
}

// Check if device just rebooted after successful OTA and clear the flag
bool checkAndClearOTASuccessFlag(String& previousVersion) {
  Preferences prefs;
  prefs.begin("ota", false);
  
  bool wasJustUpdated = prefs.getBool("appState.justUpdated", false);
  if (wasJustUpdated) {
    previousVersion = prefs.getString("prevVersion", "unknown");
    // Clear the flag
    prefs.putBool("appState.justUpdated", false);
    prefs.remove("prevVersion");
    LOG_I("[OTA] Device just updated from version %s", previousVersion.c_str());
  }
  
  prefs.end();
  return wasJustUpdated;
}

// Broadcast "just updated" message to all WebSocket clients
void broadcastJustUpdated() {
  if (!appState.ota.justUpdated) return;
  
  JsonDocument doc;
  doc["type"] = "appState.justUpdated";
  doc["previousVersion"] = appState.ota.previousFirmwareVersion;
  doc["currentVersion"] = firmwareVer;
  doc["message"] = String("Firmware successfully updated from ") + appState.ota.previousFirmwareVersion + " to " + firmwareVer;
  
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);
  
  LOG_I("[OTA] Broadcast: Firmware updated from %s to %s", appState.ota.previousFirmwareVersion.c_str(), firmwareVer);
  
  // Clear the flag after broadcasting
  appState.ota.justUpdated = false;
  appState.ota.previousFirmwareVersion = "";
}

// ===== Manual Firmware Upload Handlers =====

// Static variable to track upload error state
static bool uploadError = false;
static String uploadErrorMessage = "";

// Handler called for each chunk of uploaded firmware data
void handleFirmwareUploadChunk() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    // Reset error state
    uploadError = false;
    uploadErrorMessage = "";
    
    LOG_I("[OTA] Manual firmware upload started");
    LOG_I("[OTA] Filename: %s", upload.filename.c_str());
    
    // Check if already in progress
    if (appState.ota.inProgress) {
      LOG_E("[OTA] Another update is already in progress");
      uploadError = true;
      uploadErrorMessage = "Another update is already in progress";
      return;
    }
    
    // Validate file extension
    if (!upload.filename.endsWith(".bin")) {
      LOG_E("[OTA] Invalid file type, only .bin files are allowed");
      uploadError = true;
      uploadErrorMessage = "Invalid file type. Only .bin files are allowed";
      return;
    }
    
    appState.ota.inProgress = true;
    appState.ota.status = "uploading";
    appState.ota.progress = 0;
    appState.ota.progressBytes = 0;
    appState.ota.totalBytes = 0;  // Unknown until upload completes
    appState.ota.statusMessage = "Receiving firmware file...";
    appState.markOTADirty();
    
    // Play OTA update melody before flashing begins
    buzzer_play_blocking(BUZZ_OTA_UPDATE, 850);

    // Begin OTA update with unknown size (will auto-detect)
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      LOG_E("[OTA] Failed to begin update: %s", Update.errorString());
      uploadError = true;
      uploadErrorMessage = String("Failed to begin update: ") + Update.errorString();
      appState.ota.status = "error";
      appState.ota.statusMessage = uploadErrorMessage;
      appState.ota.inProgress = false;
      appState.markOTADirty();
      return;
    }

    LOG_I("[OTA] Upload initialized, receiving data");
    
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Skip writing if there was an error
    if (uploadError) {
      return;
    }
    
    // Write chunk to flash
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      LOG_E("[OTA] Write error: %s", Update.errorString());
      uploadError = true;
      uploadErrorMessage = String("Write error: ") + Update.errorString();
      Update.abort();
      appState.ota.status = "error";
      appState.ota.statusMessage = uploadErrorMessage;
      appState.ota.inProgress = false;
      appState.markOTADirty();
      return;
    }

    appState.ota.progressBytes += upload.currentSize;

    // Signal progress periodically (every ~10KB or every 2 seconds)
    static unsigned long lastBroadcast = 0;
    static size_t lastBroadcastBytes = 0;
    unsigned long now = millis();

    if ((appState.ota.progressBytes - lastBroadcastBytes) >= 10240 || (now - lastBroadcast) >= 2000) {
      appState.ota.statusMessage = String("Uploading: ") + String(appState.ota.progressBytes / 1024) + " KB received...";
      appState.markOTADirty();
      lastBroadcast = now;
      lastBroadcastBytes = appState.ota.progressBytes;
      LOG_D("[OTA] Received: %d KB", appState.ota.progressBytes / 1024);
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    // Skip finalization if there was an error
    if (uploadError) {
      return;
    }

    appState.ota.totalBytes = upload.totalSize;
    LOG_I("[OTA] Upload complete: %d bytes (%.2f KB)", upload.totalSize, upload.totalSize / 1024.0);

    appState.ota.statusMessage = "Verifying firmware...";
    appState.ota.progress = 100;
    appState.markOTADirty();

    // Finalize update
    if (Update.end(true)) {
      if (Update.isFinished()) {
        LOG_I("[OTA] Firmware upload and verification successful");
        appState.ota.status = "complete";
        appState.ota.statusMessage = "Upload complete! Rebooting...";
        appState.markOTADirty();
        // Note: Response and reboot handled in handleFirmwareUploadComplete
      } else {
        LOG_E("[OTA] Update did not finish correctly");
        uploadError = true;
        uploadErrorMessage = "Update verification failed";
        appState.ota.status = "error";
        appState.ota.statusMessage = uploadErrorMessage;
        appState.ota.inProgress = false;
        appState.markOTADirty();
      }
    } else {
      LOG_E("[OTA] Update finalization error: %s", Update.errorString());
      uploadError = true;
      uploadErrorMessage = String("Update error: ") + Update.errorString();
      appState.ota.status = "error";
      appState.ota.statusMessage = uploadErrorMessage;
      appState.ota.inProgress = false;
      appState.markOTADirty();
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    LOG_W("[OTA] Upload aborted by client");
    Update.abort();
    uploadError = true;
    uploadErrorMessage = "Upload aborted";
    appState.ota.status = "error";
    appState.ota.statusMessage = "Upload aborted";
    appState.ota.inProgress = false;
    appState.markOTADirty();
  }
}

// Handler called when upload POST request completes
void handleFirmwareUploadComplete() {
  JsonDocument doc;
  
  if (uploadError) {
    doc["success"] = false;
    doc["message"] = uploadErrorMessage;
    
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
    
    // Reset state
    uploadError = false;
    uploadErrorMessage = "";
    return;
  }
  
  // Check if update was successful
  if (appState.ota.status == "complete") {
    doc["success"] = true;
    doc["message"] = "Firmware uploaded successfully! Rebooting...";
    doc["bytesReceived"] = appState.ota.totalBytes;
    
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
    
    // Save success flag and reboot
    LOG_I("[OTA] Rebooting in 2 seconds");
    saveOTASuccessFlag(firmwareVer);
    delay(2000);
    ESP.restart();
  } else {
    doc["success"] = false;
    doc["message"] = appState.ota.statusMessage.length() > 0 ? appState.ota.statusMessage : "Upload failed";
    
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  }
  
  // Reset OTA state
  appState.ota.inProgress = false;
}

// ===== Non-Blocking OTA FreeRTOS Tasks =====

// OTA download task — runs performOTAUpdate() on a separate core
static void otaDownloadTask(void* param) {
  String firmwareUrl = appState.ota.cachedFirmwareUrl;
  bool success = performOTAUpdate(firmwareUrl);

  if (success) {
    LOG_I("[OTA] Update successful, rebooting in 3 seconds");
    saveOTASuccessFlag(firmwareVer);
    vTaskDelay(pdMS_TO_TICKS(3000));  // Let main loop broadcast final status
    ESP.restart();
  } else {
    LOG_W("[OTA] Update failed");
    appState.audio.paused = false;  // Resume audio capture
    appState.ota.inProgress = false;
    appState.ota.updateDiscoveredTime = 0;
    appState.setFSMState(STATE_IDLE);
    appState.markOTADirty();
  }

  otaDownloadTaskHandle = NULL;
  vTaskDelete(NULL);
}

void startOTADownloadTask() {
  if (otaDownloadTaskHandle != NULL || appState.ota.inProgress) {
    LOG_W("[OTA] Download task already running or OTA in progress");
    return;
  }

  appState.ota.inProgress = true;
  appState.ota.status = "preparing";
  appState.ota.statusMessage = "Preparing for update...";
  appState.ota.progress = 0;
  appState.setFSMState(STATE_OTA_UPDATE);
  appState.markOTADirty();

  // Pause audio capture to prevent I2S driver conflicts during OTA
  appState.audio.paused = true;
  vTaskDelay(pdMS_TO_TICKS(50));  // Ensure audio task exits i2s_read()

  BaseType_t result = xTaskCreatePinnedToCore(
    otaDownloadTask, "OTA_DL", TASK_STACK_SIZE_OTA,
    NULL, TASK_PRIORITY_WEB, &otaDownloadTaskHandle, 0  // Core 0 (network stack affinity)
  );

  if (result != pdPASS) {
    LOG_E("[OTA] Failed to create download task");
    appState.audio.paused = false;  // Resume audio if task creation failed
    appState.ota.inProgress = false;
    setOTAProgress("error", "Failed to start update task", 0);
    appState.setFSMState(STATE_IDLE);
  }
}

// OTA check task — runs checkForFirmwareUpdate() on a separate core
static void otaCheckTaskFunc(void* param) {
  // Heap pre-flight: must match the inner TLS threshold (35KB) used by
  // getLatestReleaseInfo(). MbedTLS allocates two 16KB buffers (~33KB total).
  uint32_t maxBlock = ESP.getMaxAllocHeap();
  if (maxBlock < 50000) {
    LOG_W("[OTA] Heap too low for OTA check: %lu bytes (<50KB), skipping", (unsigned long)maxBlock);
    _otaConsecutiveFailures++;
    if (_otaConsecutiveFailures > 20) _otaConsecutiveFailures = 20;
    appState.markOTADirty();
    otaCheckTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }

  checkForFirmwareUpdate();

  // Also refresh WiFi status after check (needs dirty flag, not direct WS call)
  appState.markOTADirty();

  otaCheckTaskHandle = NULL;
  vTaskDelete(NULL);
}

void startOTACheckTask() {
  if (otaCheckTaskHandle != NULL || appState.ota.inProgress) {
    LOG_D("[OTA] Check task already running or OTA in progress, skipping");
    return;
  }

  BaseType_t result = xTaskCreatePinnedToCore(
    otaCheckTaskFunc, "OTA_CHK", TASK_STACK_SIZE_OTA,
    NULL, TASK_PRIORITY_WEB, &otaCheckTaskHandle, 0  // Core 0 (network stack affinity)
  );

  if (result != pdPASS) {
    LOG_E("[OTA] Failed to create check task");
  }
}

bool isOTATaskRunning() {
  return otaDownloadTaskHandle != NULL || otaCheckTaskHandle != NULL;
}
