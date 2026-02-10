#include "ota_updater.h"
#include "config.h"
#include "app_state.h"
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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Root CA certificates for GitHub (api.github.com, github.com, and objects.githubusercontent.com)
// GitHub migrated to Sectigo/USERTrust in March 2024
// Includes both USERTrust (ECC and RSA) for github.com/api.github.com 
// and DigiCert Global Root G2 for objects.githubusercontent.com CDN
// Valid until 2030-2038
static const char* GITHUB_ROOT_CA = \
// USERTrust ECC Certification Authority (for api.github.com and github.com) - valid until 2038
"-----BEGIN CERTIFICATE-----\n" \
"MIICjzCCAhWgAwIBAgIQXIuZxVqUxdJxVt7NiYDMJjAKBggqhkjOPQQDAzCBiDEL\n" \
"MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl\n" \
"eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT\n" \
"JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMjAx\n" \
"MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNVBAgT\n" \
"Ck5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVUaGUg\n" \
"VVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBFQ0MgQ2VydGlm\n" \
"aWNhdGlvbiBBdXRob3JpdHkwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAQarFRaqflo\n" \
"I+d61SRvU8Za2EurxtW20eZzca7dnNYMYf3boIkDuAUU7FfO7l0/4iGzzvfUinng\n" \
"o4N+LZfQYcTxmdwlkWOrfzCjtHDix6EznPO/LlxTsV+zfTJ/ijTjeXmjQjBAMB0G\n" \
"A1UdDgQWBBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAOBgNVHQ8BAf8EBAMCAQYwDwYD\n" \
"VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAwNoADBlAjA2Z6EWCNzklwBBHU6+4WMB\n" \
"zzuqQhFkoJ2UOQIReVx7Hfpkue4WQrO/isIJxOzksU0CMQDpKmFHjFJKS04YcPbW\n" \
"RNZu9YO6bVi9JNlWSOrvxKJGgYhqOkbRqZtNyWHa0V1Xahg=\n" \
"-----END CERTIFICATE-----\n" \
// USERTrust RSA Certification Authority (for github.com RSA certificates) - valid until 2038
"-----BEGIN CERTIFICATE-----\n" \
"MIIFgTCCBGmgAwIBAgIQOXJEOvkit1HX02wQ3TE1lTANBgkqhkiG9w0BAQwFADB7\n" \
"MQswCQYDVQQGEwJHQjEbMBkGA1UECAwSR3JlYXRlciBNYW5jaGVzdGVyMRAwDgYD\n" \
"VQQHDAdTYWxmb3JkMRowGAYDVQQKDBFDb21vZG8gQ0EgTGltaXRlZDEhMB8GA1UE\n" \
"AwwYQUFBIENlcnRpZmljYXRlIFNlcnZpY2VzMB4XDTE5MDMxMjAwMDAwMFoXDTI4\n" \
"MTIzMTIzNTk1OVowgYgxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpOZXcgSmVyc2V5\n" \
"MRQwEgYDVQQHEwtKZXJzZXkgQ2l0eTEeMBwGA1UEChMVVGhlIFVTRVJUUlVTVCBO\n" \
"ZXR3b3JrMS4wLAYDVQQDEyVVU0VSVHJ1c3QgUlNBIENlcnRpZmljYXRpb24gQXV0\n" \
"aG9yaXR5MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAgBJlFzYOw9sI\n" \
"s9CsVw127c0n00ytUINh4qogTQktZAnczomfzD2p7PbPwdzx07HWezcoEStH2jnG\n" \
"vDoZtF+mvX2do2NCtnbyqTsrkfjib9DsFiCQCT7i6HTJGLSR1GJk23+jBvGIGGqQ\n" \
"Ijy8/hPwhxR79uQfjtTkUcYRZ0YIUcuGFFQ/vDP+fmyc/xadGL1RjjWmp2bIcmfb\n" \
"IWax1Jt4A8BQOujM8Ny8nkz+rwWWNR9XWrf/zvk9tyy29lTdyOcSOk2uTIq3XJq0\n" \
"tyA9yn8iNK5+O2hmAUTnAU5GU5szYPeUvlM3kHND8zLDU+/bqv50TmnHa4xgk97E\n" \
"xwzf4TKuzJM7UXiVZ4vuPVb+DNBpDxsP8yUmazNt925H+nND5X4OpWaxKXwyhGNV\n" \
"icQNwZNUMBkTrNN9N6frXTpsNVzbQdcS2qlJC9/YgIoJk2KOtWbPJYjNhLixP6Q5\n" \
"D9kCnusSTJV882sFqV4Wg8y4Z+LoE53MW4LTTLPtW//e5XOsIzstAL81VXQJSdhJ\n" \
"WBp/kjbmUZIO8yZ9HE0XvMnsQybQv0FfQKlERPSZ51eHnlAfV1SoPv10Yy+xUGUJ\n" \
"5lhCLkMaTLTwJUdZ+gQek9QmRkpQgbLevni3/GcV4clXhB4PY9bpYrrWX1Uu6lzG\n" \
"KAgEJTm4Diup8kyXHAc/DVL17e8vgg8CAwEAAaOB8jCB7zAfBgNVHSMEGDAWgBSg\n" \
"EQojPpbxB+zirynvgqV/0DCktDAdBgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rID\n" \
"ZsswDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQFMAMBAf8wEQYDVR0gBAowCDAG\n" \
"BgRVHSAAMEMGA1UdHwQ8MDowOKA2oDSGMmh0dHA6Ly9jcmwuY29tb2RvY2EuY29t\n" \
"L0FBQUNlcnRpZmljYXRlU2VydmljZXMuY3JsMDQGCCsGAQUFBwEBBCgwJjAkBggr\n" \
"BgEFBQcwAYYYaHR0cDovL29jc3AuY29tb2RvY2EuY29tMA0GCSqGSIb3DQEBDAUA\n" \
"A4IBAQAYh1HcdCE9nIrgJ7cz0C7M7PDmy14R3iJvm3WOnnL+5Nb+qh+cli3vA0p+\n" \
"rvSNb3I8QzvAP+u431yqqcau8vzY7qN7Q/aGNnwU4M309z/+3ri0ivCRlv79Q2R+\n" \
"/czSAaF9ffgZGclCKxO/WIu6pKJmBHaIkU4MiRTOok3JMrO66BQavHHxW/BBC5gA\n" \
"CiIDEOUMsfnNkjcZ7Tvx5Dq2+UUTJnWvu6rvP3t3O9LEApE9GQDTF1w52z97GA1F\n" \
"zZOFli9d31kWTz9RvdVFGD/tSo7oBmF0Ixa1DVBzJ0RHfxBdiSprhTEUxOipakyA\n" \
"vGp4z7h/jnZymQyd/teRCBaho1+V\n" \
"-----END CERTIFICATE-----\n" \
// DigiCert Global Root G2 (for objects.githubusercontent.com - GitHub releases CDN) - valid until 2038
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

// ===== OTA Progress Helper (thread-safe via dirty flag) =====
static void setOTAProgress(const char* status, const char* message, int progress) {
  appState.otaStatus = status;
  appState.otaStatusMessage = message;
  appState.otaProgress = progress;
  appState.markOTADirty();
}

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
  doc["amplifierInUse"] = amplifierState;
  
  if (otaTotalBytes > 0) {
    doc["bytesDownloaded"] = otaProgressBytes;
    doc["totalBytes"] = otaTotalBytes;
  }
  
  // Show countdown when auto-update enabled, update available, amplifier is off, and countdown active
  if (autoUpdateEnabled && updateAvailable && !amplifierState && updateDiscoveredTime > 0) {
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
  doc["latestVersion"] = cachedLatestVersion.length() > 0 ? cachedLatestVersion : "Checking...";
  doc["updateAvailable"] = updateAvailable;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleStartUpdate() {
  if (otaInProgress || isOTATaskRunning()) {
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

  LOG_I("[OTA] Manual OTA update started");

  // Send HTTP response immediately (non-blocking)
  server.send(200, "application/json", "{\"success\": true, \"message\": \"Update started\"}");

  // Launch OTA download in a FreeRTOS task
  startOTADownloadTask();
}

void handleUpdateStatus() {
  JsonDocument doc;
  doc["status"] = otaStatus;
  doc["progress"] = otaProgress;
  doc["message"] = otaStatusMessage;
  doc["updateAvailable"] = updateAvailable;
  doc["currentVersion"] = firmwareVer;
  doc["latestVersion"] = cachedLatestVersion.length() > 0 ? cachedLatestVersion : "Unknown";
  doc["autoUpdateEnabled"] = autoUpdateEnabled;
  doc["amplifierInUse"] = amplifierState;
  
  if (otaTotalBytes > 0) {
    doc["bytesDownloaded"] = otaProgressBytes;
    doc["totalBytes"] = otaTotalBytes;
  }
  
  // Include countdown if auto-update is active
  if (autoUpdateEnabled && updateAvailable && !amplifierState && updateDiscoveredTime > 0) {
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
  
  WiFiClientSecure client;
  
  if (enableCertValidation) {
    client.setCACert(GITHUB_ROOT_CA);  // Use bundled GitHub root certificates
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

// ===== OTA Core Functions =====

void checkForFirmwareUpdate() {
  if (otaInProgress) {
    return;
  }
  
  LOG_I("[OTA] Checking for firmware update");
  LOG_I("[OTA] Current firmware version: %s", firmwareVer);
  
  String latestVersion = "";
  String firmwareUrl = "";
  String checksum = "";
  
  if (!getLatestReleaseInfo(latestVersion, firmwareUrl, checksum)) {
    LOG_E("[OTA] Failed to retrieve release information");
    return;
  }
  
  latestVersion.trim();
  LOG_I("[OTA] Latest firmware version available: %s", latestVersion.c_str());
  
  // Always update cached version info
  cachedLatestVersion = latestVersion;
  cachedFirmwareUrl = firmwareUrl;
  cachedChecksum = checksum;
  
  int cmp = compareVersions(latestVersion, String(firmwareVer));
  
  if (cmp > 0) {
    // Update available
    bool isNewUpdate = !updateAvailable;  // Track if this is a newly discovered update
    updateAvailable = true;
    
    if (isNewUpdate || updateDiscoveredTime == 0) {
      // Start countdown timer for new update or if timer was reset
      updateDiscoveredTime = millis();
      LOG_I("[OTA] New version available: %s", latestVersion.c_str());
      if (checksum.length() > 0) {
        LOG_I("[OTA] SHA256 checksum: %s", checksum.c_str());
      }
    } else {
      LOG_I("[OTA] Update still available: %s", latestVersion.c_str());
    }
  } else {
    // Up to date or downgrade
    updateAvailable = false;
    updateDiscoveredTime = 0;
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
  WiFiClientSecure client;
  
  if (enableCertValidation) {
    LOG_I("[OTA] Certificate validation enabled");
    client.setCACert(GITHUB_ROOT_CA);  // Use bundled GitHub root certificates
  } else {
    LOG_W("[OTA] Certificate validation disabled (insecure mode)");
    client.setInsecure();  // Skip certificate validation
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
  setOTAProgress("preparing", "Preparing for update...", 0);

  LOG_I("[OTA] Starting OTA update");
  LOG_I("[OTA] Downloading from: %s", firmwareUrl.c_str());

  // Use secure HTTPS connection with certificate validation
  WiFiClientSecure client;

  if (enableCertValidation) {
    LOG_I("[OTA] Certificate validation enabled");
    client.setCACert(GITHUB_ROOT_CA);  // Use bundled GitHub root certificates
  } else {
    LOG_W("[OTA] Certificate validation disabled (insecure mode)");
    client.setInsecure();  // Skip certificate validation
  }

  client.setTimeout(30000);

  HTTPClient https;
  if (!https.begin(client, firmwareUrl)) {
    LOG_E("[OTA] Failed to initialize HTTPS connection");
    setOTAProgress("error", "Failed to initialize secure connection", 0);
    otaInProgress = false;
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
    otaInProgress = false;
    return false;
  }

  int contentLength = https.getSize();
  otaTotalBytes = contentLength;
  LOG_I("[OTA] Firmware size: %d bytes (%.2f KB)", contentLength, contentLength / 1024.0);

  if (contentLength <= 0) {
    LOG_E("[OTA] Invalid firmware size");
    https.end();
    setOTAProgress("error", "Invalid firmware file", 0);
    otaInProgress = false;
    return false;
  }

  // Check if enough space is available
  int freeSpace = ESP.getFreeSketchSpace() - 0x1000;
  if (contentLength > freeSpace) {
    LOG_E("[OTA] Not enough space, need: %d, available: %d", contentLength, freeSpace);
    https.end();
    setOTAProgress("error", "Not enough storage space", 0);
    otaInProgress = false;
    return false;
  }

  // Play OTA update melody before flashing begins
  buzzer_play_blocking(BUZZ_OTA_UPDATE, 850);

  // Begin OTA update
  if (!Update.begin(contentLength)) {
    LOG_E("[OTA] Failed to begin OTA, free space: %d", ESP.getFreeSketchSpace());
    https.end();
    setOTAProgress("error", "Failed to initialize update", 0);
    otaInProgress = false;
    return false;
  }

  otaProgressBytes = 0;
  setOTAProgress("downloading", "Downloading firmware...", 0);

  LOG_I("[OTA] Download started, writing to flash");

  WiFiClient* stream = https.getStreamPtr();

  size_t written = 0;
  uint8_t buffer[1024];
  unsigned long lastProgressUpdate = 0;

  // For checksum calculation
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  bool calculatingChecksum = (cachedChecksum.length() == 64);

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
        otaInProgress = false;
        return false;
      }

      written += bytesRead;
      otaProgressBytes = written;

      // Update progress every 1% change or every 2 seconds
      int newProgress = (written * 100) / contentLength;
      unsigned long now = millis();
      if (newProgress != otaProgress || (now - lastProgressUpdate) >= 2000) {
        appState.otaProgress = newProgress;
        appState.otaStatusMessage = String("Downloading: ") + String(written / 1024) + " / " + String(contentLength / 1024) + " KB";
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

    LOG_I("[OTA] Expected checksum:   %s", cachedChecksum.c_str());
    LOG_I("[OTA] Calculated checksum: %s", calculatedChecksum.c_str());

    if (calculatedChecksum.equalsIgnoreCase(cachedChecksum)) {
      LOG_I("[OTA] Checksum verification passed");
    } else {
      LOG_E("[OTA] Checksum verification failed");
      Update.abort();
      setOTAProgress("error", "Checksum verification failed - firmware corrupted", 0);
      otaInProgress = false;
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
      otaInProgress = false;
      return false;
    }
  } else {
    LOG_E("[OTA] Update error: %s", Update.errorString());
    Update.abort();
    appState.otaStatus = "error";
    appState.otaStatusMessage = String("Update error: ") + Update.errorString();
    appState.otaProgress = 0;
    appState.markOTADirty();
    otaInProgress = false;
    return false;
  }
}

// ===== OTA Success Flag Functions =====

// Save flag indicating OTA was successful (called before reboot)
void saveOTASuccessFlag(const String& previousVersion) {
  Preferences prefs;
  prefs.begin("ota", false);
  prefs.putBool("justUpdated", true);
  prefs.putString("prevVersion", previousVersion);
  prefs.end();
  LOG_I("[OTA] Saved OTA success flag (previous version: %s)", previousVersion.c_str());
}

// Check if device just rebooted after successful OTA and clear the flag
bool checkAndClearOTASuccessFlag(String& previousVersion) {
  Preferences prefs;
  prefs.begin("ota", false);
  
  bool wasJustUpdated = prefs.getBool("justUpdated", false);
  if (wasJustUpdated) {
    previousVersion = prefs.getString("prevVersion", "unknown");
    // Clear the flag
    prefs.putBool("justUpdated", false);
    prefs.remove("prevVersion");
    LOG_I("[OTA] Device just updated from version %s", previousVersion.c_str());
  }
  
  prefs.end();
  return wasJustUpdated;
}

// Broadcast "just updated" message to all WebSocket clients
void broadcastJustUpdated() {
  if (!justUpdated) return;
  
  JsonDocument doc;
  doc["type"] = "justUpdated";
  doc["previousVersion"] = previousFirmwareVersion;
  doc["currentVersion"] = firmwareVer;
  doc["message"] = String("Firmware successfully updated from ") + previousFirmwareVersion + " to " + firmwareVer;
  
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);
  
  LOG_I("[OTA] Broadcast: Firmware updated from %s to %s", previousFirmwareVersion.c_str(), firmwareVer);
  
  // Clear the flag after broadcasting
  justUpdated = false;
  previousFirmwareVersion = "";
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
    if (otaInProgress) {
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
    
    otaInProgress = true;
    otaStatus = "uploading";
    otaProgress = 0;
    otaProgressBytes = 0;
    otaTotalBytes = 0;  // Unknown until upload completes
    otaStatusMessage = "Receiving firmware file...";
    appState.markOTADirty();
    
    // Play OTA update melody before flashing begins
    buzzer_play_blocking(BUZZ_OTA_UPDATE, 850);

    // Begin OTA update with unknown size (will auto-detect)
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      LOG_E("[OTA] Failed to begin update: %s", Update.errorString());
      uploadError = true;
      uploadErrorMessage = String("Failed to begin update: ") + Update.errorString();
      otaStatus = "error";
      otaStatusMessage = uploadErrorMessage;
      otaInProgress = false;
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
      otaStatus = "error";
      otaStatusMessage = uploadErrorMessage;
      otaInProgress = false;
      appState.markOTADirty();
      return;
    }

    otaProgressBytes += upload.currentSize;

    // Signal progress periodically (every ~10KB or every 2 seconds)
    static unsigned long lastBroadcast = 0;
    static size_t lastBroadcastBytes = 0;
    unsigned long now = millis();

    if ((otaProgressBytes - lastBroadcastBytes) >= 10240 || (now - lastBroadcast) >= 2000) {
      otaStatusMessage = String("Uploading: ") + String(otaProgressBytes / 1024) + " KB received...";
      appState.markOTADirty();
      lastBroadcast = now;
      lastBroadcastBytes = otaProgressBytes;
      LOG_D("[OTA] Received: %d KB", otaProgressBytes / 1024);
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    // Skip finalization if there was an error
    if (uploadError) {
      return;
    }

    otaTotalBytes = upload.totalSize;
    LOG_I("[OTA] Upload complete: %d bytes (%.2f KB)", upload.totalSize, upload.totalSize / 1024.0);

    otaStatusMessage = "Verifying firmware...";
    otaProgress = 100;
    appState.markOTADirty();

    // Finalize update
    if (Update.end(true)) {
      if (Update.isFinished()) {
        LOG_I("[OTA] Firmware upload and verification successful");
        otaStatus = "complete";
        otaStatusMessage = "Upload complete! Rebooting...";
        appState.markOTADirty();
        // Note: Response and reboot handled in handleFirmwareUploadComplete
      } else {
        LOG_E("[OTA] Update did not finish correctly");
        uploadError = true;
        uploadErrorMessage = "Update verification failed";
        otaStatus = "error";
        otaStatusMessage = uploadErrorMessage;
        otaInProgress = false;
        appState.markOTADirty();
      }
    } else {
      LOG_E("[OTA] Update finalization error: %s", Update.errorString());
      uploadError = true;
      uploadErrorMessage = String("Update error: ") + Update.errorString();
      otaStatus = "error";
      otaStatusMessage = uploadErrorMessage;
      otaInProgress = false;
      appState.markOTADirty();
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    LOG_W("[OTA] Upload aborted by client");
    Update.abort();
    uploadError = true;
    uploadErrorMessage = "Upload aborted";
    otaStatus = "error";
    otaStatusMessage = "Upload aborted";
    otaInProgress = false;
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
  if (otaStatus == "complete") {
    doc["success"] = true;
    doc["message"] = "Firmware uploaded successfully! Rebooting...";
    doc["bytesReceived"] = otaTotalBytes;
    
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
    doc["message"] = otaStatusMessage.length() > 0 ? otaStatusMessage : "Upload failed";
    
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  }
  
  // Reset OTA state
  otaInProgress = false;
}

// ===== Non-Blocking OTA FreeRTOS Tasks =====

// OTA download task — runs performOTAUpdate() on a separate core
static void otaDownloadTask(void* param) {
  String firmwareUrl = appState.cachedFirmwareUrl;
  bool success = performOTAUpdate(firmwareUrl);

  if (success) {
    LOG_I("[OTA] Update successful, rebooting in 3 seconds");
    saveOTASuccessFlag(firmwareVer);
    vTaskDelay(pdMS_TO_TICKS(3000));  // Let main loop broadcast final status
    ESP.restart();
  } else {
    LOG_W("[OTA] Update failed");
    appState.otaInProgress = false;
    appState.updateDiscoveredTime = 0;
    appState.setFSMState(STATE_IDLE);
    appState.markOTADirty();
  }

  otaDownloadTaskHandle = NULL;
  vTaskDelete(NULL);
}

void startOTADownloadTask() {
  if (otaDownloadTaskHandle != NULL || appState.otaInProgress) {
    LOG_W("[OTA] Download task already running or OTA in progress");
    return;
  }

  appState.otaInProgress = true;
  appState.otaStatus = "preparing";
  appState.otaStatusMessage = "Preparing for update...";
  appState.otaProgress = 0;
  appState.setFSMState(STATE_OTA_UPDATE);
  appState.markOTADirty();

  BaseType_t result = xTaskCreatePinnedToCore(
    otaDownloadTask, "OTA_DL", TASK_STACK_SIZE_OTA,
    NULL, TASK_PRIORITY_WEB, &otaDownloadTaskHandle, 0  // Core 0 (network stack affinity)
  );

  if (result != pdPASS) {
    LOG_E("[OTA] Failed to create download task");
    appState.otaInProgress = false;
    setOTAProgress("error", "Failed to start update task", 0);
    appState.setFSMState(STATE_IDLE);
  }
}

// OTA check task — runs checkForFirmwareUpdate() on a separate core
static void otaCheckTaskFunc(void* param) {
  checkForFirmwareUpdate();

  // Also refresh WiFi status after check (needs dirty flag, not direct WS call)
  appState.markOTADirty();

  otaCheckTaskHandle = NULL;
  vTaskDelete(NULL);
}

void startOTACheckTask() {
  if (otaCheckTaskHandle != NULL || appState.otaInProgress) {
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
