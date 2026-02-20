#include "ota_updater.h"
#include "config.h"
#include "app_state.h"
#include "debug_serial.h"
#include "utils.h"
#include "buzzer_handler.h"
#include "i2s_audio.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <mbedtls/md.h>
#include <time.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ===== MbedTLS PSRAM Allocation Override =====
// Redirect MbedTLS memory allocations to PSRAM via GCC linker --wrap.
// The precompiled libmbedcrypto.a calls esp_mbedtls_mem_calloc() for all
// internal allocations (SSL contexts, X.509 cert chains, I/O buffers ~32KB).
// By default these go to internal SRAM, competing with I2S DMA and WiFi.
// This wrapper sends them to PSRAM instead, keeping internal SRAM free
// for audio and network buffers that require DMA-capable memory.
extern "C" {

void *__wrap_esp_mbedtls_mem_calloc(size_t n, size_t size) {
  // Try PSRAM first, fall back to internal SRAM if PSRAM unavailable
  void *ptr = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ptr) {
    ptr = heap_caps_calloc(n, size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
  }
  return ptr;
}

void __wrap_esp_mbedtls_mem_free(void *ptr) {
  heap_caps_free(ptr);
}

}  // extern "C"

// Root and intermediate CA certificates for GitHub
// (api.github.com, github.com, and objects.githubusercontent.com)
// Updated Feb 2026: GitHub's chain now uses Sectigo Public Server Auth Root E46
// as an intermediate between the leaf and USERTrust ECC root.
// Chain: leaf → Sectigo DV E36 → Sectigo Root E46 → USERTrust ECC
// CDN:   leaf → Sectigo RSA DV → USERTrust RSA
// All certs valid until 2028-2038
static const char* GITHUB_ROOT_CA = \
// USERTrust ECC Certification Authority (root for api.github.com ECC chain) - valid until 2038
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
// Sectigo Public Server Authentication Root E46 (intermediate for api.github.com) - valid until 2038
// Signed by USERTrust ECC. Added Feb 2026 when GitHub's chain gained this intermediate.
"-----BEGIN CERTIFICATE-----\n" \
"MIIDRjCCAsugAwIBAgIQGp6v7G3o4ZtcGTFBto2Q3TAKBggqhkjOPQQDAzCBiDEL\n" \
"MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl\n" \
"eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT\n" \
"JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMjEwMzIy\n" \
"MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjBfMQswCQYDVQQGEwJHQjEYMBYGA1UEChMP\n" \
"U2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1TZWN0aWdvIFB1YmxpYyBTZXJ2ZXIg\n" \
"QXV0aGVudGljYXRpb24gUm9vdCBFNDYwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAR2\n" \
"+pmpbiDt+dd34wc7qNs9Xzjoq1WmVk/WSOrsfy2qw7LFeeyZYX8QeccCWvkEN/U0\n" \
"NSt3zn8gj1KjAIns1aeibVvjS5KToID1AZTc8GgHHs3u/iVStSBDHBv+6xnOQ6Oj\n" \
"ggEgMIIBHDAfBgNVHSMEGDAWgBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAdBgNVHQ4E\n" \
"FgQU0SLaTFnxS18mOKqd1u7rDcP7qWEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB\n" \
"/wQFMAMBAf8wHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBEGA1UdIAQK\n" \
"MAgwBgYEVR0gADBQBgNVHR8ESTBHMEWgQ6BBhj9odHRwOi8vY3JsLnVzZXJ0cnVz\n" \
"dC5jb20vVVNFUlRydXN0RUNDQ2VydGlmaWNhdGlvbkF1dGhvcml0eS5jcmwwNQYI\n" \
"KwYBBQUHAQEEKTAnMCUGCCsGAQUFBzABhhlodHRwOi8vb2NzcC51c2VydHJ1c3Qu\n" \
"Y29tMAoGCCqGSM49BAMDA2kAMGYCMQCMCyBit99vX2ba6xEkDe+YO7vC0twjbkv9\n" \
"PKpqGGuZ61JZryjFsp+DFpEclCVy4noCMQCwvZDXD/m2Ko1HA5Bkmz7YQOFAiNDD\n" \
"49IWa2wdT7R3DtODaSXH/BiXv8fwB9su4tU=\n" \
"-----END CERTIFICATE-----\n" \
// USERTrust RSA Certification Authority (root for CDN objects.githubusercontent.com) - valid until 2028
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
"-----END CERTIFICATE-----\n";

// External functions from main.cpp that OTA needs to call
extern int compareVersions(const String& v1, const String& v2);
extern void sendWiFiStatus();

// Check if NTP has synced (time > year 2000)
static bool isNtpSynced() {
  return time(nullptr) > 1000000000;
}

// Temporarily unsubscribe loopTask from WDT during TLS operations.
// TLS handshakes monopolize the WiFi/lwIP stack, blocking loopTask
// on Core 1 from feeding the WDT for >15s.
static void wdtSuspendLoopTask() {
  if (loopTaskHandle) {
    esp_task_wdt_delete(loopTaskHandle);
  }
}

static void wdtResumeLoopTask() {
  if (loopTaskHandle) {
    esp_task_wdt_add(loopTaskHandle);
  }
}

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
  setCharField(appState.otaStatus, sizeof(appState.otaStatus), status);
  setCharField(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), message);
  appState.otaProgress = progress;
  appState.markOTADirty();
}

// ===== OTA Status Broadcasting =====
void broadcastUpdateStatus() {
  JsonDocument doc;
  doc["type"] = "updateStatus";
  doc["status"] = appState.otaStatus;
  doc["progress"] = appState.otaProgress;
  doc["message"] = appState.otaStatusMessage;
  doc["appState.updateAvailable"] = appState.updateAvailable;
  doc["currentVersion"] = firmwareVer;
  doc["latestVersion"] = appState.cachedLatestVersion;
  doc["appState.autoUpdateEnabled"] = appState.autoUpdateEnabled;
  doc["amplifierInUse"] = appState.amplifierState;
  doc["httpFallback"] = appState.otaHttpFallback;

  if (appState.otaTotalBytes > 0) {
    doc["bytesDownloaded"] = appState.otaProgressBytes;
    doc["totalBytes"] = appState.otaTotalBytes;
  }
  
  // Show countdown when auto-update enabled, update available, amplifier is off, and countdown active
  if (appState.autoUpdateEnabled && appState.updateAvailable && !appState.amplifierState && appState.updateDiscoveredTime > 0) {
    unsigned long elapsed = millis() - appState.updateDiscoveredTime;
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
  doc["latestVersion"] = appState.cachedLatestVersion.length() > 0 ? appState.cachedLatestVersion : "Checking...";
  doc["appState.updateAvailable"] = appState.updateAvailable;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleStartUpdate() {
  if (appState.otaInProgress || isOTATaskRunning()) {
    server.send(200, "application/json", "{\"success\": false, \"message\": \"OTA update already in progress\"}");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    server.send(200, "application/json", "{\"success\": false, \"message\": \"Not connected to WiFi\"}");
    return;
  }

  if (!appState.updateAvailable || appState.cachedLatestVersion.length() == 0 || appState.cachedFirmwareUrl.length() == 0) {
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
  doc["status"] = appState.otaStatus;
  doc["progress"] = appState.otaProgress;
  doc["message"] = appState.otaStatusMessage;
  doc["appState.updateAvailable"] = appState.updateAvailable;
  doc["currentVersion"] = firmwareVer;
  doc["latestVersion"] = appState.cachedLatestVersion.length() > 0 ? appState.cachedLatestVersion : "Unknown";
  doc["appState.autoUpdateEnabled"] = appState.autoUpdateEnabled;
  doc["amplifierInUse"] = appState.amplifierState;
  
  if (appState.otaTotalBytes > 0) {
    doc["bytesDownloaded"] = appState.otaProgressBytes;
    doc["totalBytes"] = appState.otaTotalBytes;
  }
  
  // Include countdown if auto-update is active
  if (appState.autoUpdateEnabled && appState.updateAvailable && !appState.amplifierState && appState.updateDiscoveredTime > 0) {
    unsigned long elapsed = millis() - appState.updateDiscoveredTime;
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
  if (maxBlock < HEAP_TLS_MIN_THRESHOLD_BYTES) {
    LOG_E("[OTA] Heap too low for TLS: largest block=%lu bytes (<30KB)", (unsigned long)maxBlock);
    server.send(200, "application/json", "{\"success\": false, \"message\": \"Insufficient memory for secure connection\"}");
    return;
  }

  WiFiClientSecure client;

  if (maxBlock < HEAP_TLS_SECURE_THRESHOLD_BYTES) {
    LOG_W("[OTA] Heap low (%lu bytes), using insecure TLS (no cert validation)", (unsigned long)maxBlock);
    client.setInsecure();
  } else if (appState.enableCertValidation && isNtpSynced()) {
    client.setCACert(GITHUB_ROOT_CA);
  } else {
    if (appState.enableCertValidation && !isNtpSynced()) {
      LOG_W("[OTA] NTP not synced, skipping cert validation (clock not set)");
    }
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
    // Stream-parse with filter — only extract "body" field to save heap
    JsonDocument bodyFilter;
    bodyFilter["body"] = true;

    JsonDocument apiDoc;
    WiFiClient* streamPtr = https.getStreamPtr();
    DeserializationError error = deserializeJson(apiDoc, *streamPtr,
        DeserializationOption::Filter(bodyFilter));
    https.end();  // Close AFTER deserialization

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
    https.end();
    doc["success"] = false;
    doc["message"] = "Release notes not found";
    doc["notes"] = "No release notes available for version " + version + "\n\nYou can view releases at:\nhttps://github.com/" + String(githubRepoOwner) + "/" + String(githubRepoName) + "/releases";
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// ===== OTA Core Functions =====

void checkForFirmwareUpdate() {
  if (appState.otaInProgress) {
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
  appState.cachedLatestVersion = latestVersion;
  appState.cachedFirmwareUrl = firmwareUrl;
  appState.cachedChecksum = checksum;
  
  int cmp = compareVersions(latestVersion, String(firmwareVer));
  
  if (cmp > 0) {
    // Update available
    bool isNewUpdate = !appState.updateAvailable;  // Track if this is a newly discovered update
    appState.updateAvailable = true;
    
    if (isNewUpdate || appState.updateDiscoveredTime == 0) {
      // Start countdown timer for new update or if timer was reset
      appState.updateDiscoveredTime = millis();
      LOG_I("[OTA] New version available: %s", latestVersion.c_str());
      if (checksum.length() > 0) {
        LOG_I("[OTA] SHA256 checksum: %s", checksum.c_str());
      }
    } else {
      LOG_I("[OTA] Update still available: %s", latestVersion.c_str());
    }
  } else {
    // Up to date or downgrade
    appState.updateAvailable = false;
    appState.updateDiscoveredTime = 0;
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
  // TLS handshake needs ~40-50KB contiguous heap for MbedTLS buffers
  uint32_t maxBlock = ESP.getMaxAllocHeap();
  if (maxBlock < HEAP_TLS_MIN_THRESHOLD_BYTES) {
    LOG_E("[OTA] Heap too low for TLS: largest block=%lu bytes (<30KB), skipping", (unsigned long)maxBlock);
    return false;
  }

  WiFiClientSecure client;

  if (maxBlock < HEAP_TLS_SECURE_THRESHOLD_BYTES) {
    LOG_W("[OTA] Heap low (%lu bytes), using insecure TLS (no cert validation)", (unsigned long)maxBlock);
    client.setInsecure();
  } else if (appState.enableCertValidation && isNtpSynced()) {
    LOG_I("[OTA] Certificate validation enabled");
    client.setCACert(GITHUB_ROOT_CA);
  } else {
    if (appState.enableCertValidation && !isNtpSynced()) {
      LOG_W("[OTA] NTP not synced, skipping cert validation (clock not set)");
    } else {
      LOG_W("[OTA] Certificate validation disabled (insecure mode)");
    }
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

  // Stream-parse JSON directly from network — avoids 5-20KB heap String copy.
  // Filter to only store the fields we need (tag_name, body, assets[].name/url).
  JsonDocument filter;
  filter["tag_name"] = true;
  filter["body"] = true;
  JsonObject assetFilter = filter["assets"][0].to<JsonObject>();
  assetFilter["name"] = true;
  assetFilter["browser_download_url"] = true;

  JsonDocument doc;
  WiFiClient* streamPtr = https.getStreamPtr();
  DeserializationError error = deserializeJson(doc, *streamPtr,
      DeserializationOption::Filter(filter));
  https.end();  // Close AFTER deserialization completes

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

  // Convert to hex string using fixed-size char array (avoids 32 String concatenations)
  char hashStr[65];
  for (int i = 0; i < 32; i++) {
    snprintf(hashStr + i * 2, 3, "%02x", shaResult[i]);
  }
  hashStr[64] = '\0';

  return String(hashStr);
}

// Shared download+flash logic used by both HTTPS and HTTP paths.
// Streams firmware from an already-connected HTTPClient, writes to flash,
// computes SHA256 on the fly, and verifies against cachedChecksum.
static bool performDownloadAndFlash(HTTPClient& http, int contentLength) {
  appState.otaTotalBytes = contentLength;
  LOG_I("[OTA] Firmware size: %d bytes (%.2f KB)", contentLength, contentLength / 1024.0);

  if (contentLength <= 0) {
    LOG_E("[OTA] Invalid firmware size");
    http.end();
    setOTAProgress("error", "Invalid firmware file", 0);
    appState.otaInProgress = false;
    return false;
  }

  // Check if enough space is available
  int freeSpace = ESP.getFreeSketchSpace() - 0x1000;
  if (contentLength > freeSpace) {
    LOG_E("[OTA] Not enough space, need: %d, available: %d", contentLength, freeSpace);
    http.end();
    setOTAProgress("error", "Not enough storage space", 0);
    appState.otaInProgress = false;
    return false;
  }

  // Play OTA update melody before flashing begins
  buzzer_play_blocking(BUZZ_OTA_UPDATE, 850);

  // Begin OTA update
  if (!Update.begin(contentLength)) {
    LOG_E("[OTA] Failed to begin OTA, free space: %d", ESP.getFreeSketchSpace());
    http.end();
    setOTAProgress("error", "Failed to initialize update", 0);
    appState.otaInProgress = false;
    return false;
  }

  appState.otaProgressBytes = 0;
  setOTAProgress("downloading", "Downloading firmware...", 0);

  LOG_I("[OTA] Download started, writing to flash");

  WiFiClient* stream = http.getStreamPtr();

  size_t written = 0;
  uint8_t buffer[1024];
  unsigned long lastProgressUpdate = 0;

  // For checksum calculation
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  bool calculatingChecksum = (appState.cachedChecksum.length() == 64);

  if (calculatingChecksum) {
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    LOG_I("[OTA] Checksum verification enabled");
  }

  while (http.connected() && (written < (size_t)contentLength)) {
    size_t available = stream->available();
    if (available) {
      int bytesRead = stream->readBytes(buffer, min(available, sizeof(buffer)));

      // Update checksum calculation
      if (calculatingChecksum) {
        mbedtls_md_update(&ctx, buffer, bytesRead);
      }

      if (Update.write(buffer, bytesRead) != (size_t)bytesRead) {
        LOG_E("[OTA] Error writing firmware data");
        Update.abort();
        if (calculatingChecksum) {
          mbedtls_md_free(&ctx);
        }
        http.end();
        setOTAProgress("error", "Write error during download", 0);
        appState.otaInProgress = false;
        return false;
      }

      written += bytesRead;
      appState.otaProgressBytes = written;

      // Update progress every 1% change or every 2 seconds
      int newProgress = (written * 100) / contentLength;
      unsigned long now = millis();
      if (newProgress != appState.otaProgress || (now - lastProgressUpdate) >= 2000) {
        appState.otaProgress = newProgress;
        snprintf(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), "Downloading: %d / %d KB", (int)(written / 1024), contentLength / 1024);
        appState.markOTADirty();
        lastProgressUpdate = now;
        LOG_D("[OTA] Progress: %d%% (%d KB / %d KB)", newProgress, written / 1024, contentLength / 1024);
      }
    }
    delay(1);  // Yield to FreeRTOS scheduler
  }

  http.end();

  // Verify checksum if available
  if (calculatingChecksum) {
    byte shaResult[32];
    mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);

    // Convert to hex string using fixed-size char array (avoids 32 String concatenations)
    char hashStr[65];
    for (int i = 0; i < 32; i++) {
      snprintf(hashStr + i * 2, 3, "%02x", shaResult[i]);
    }
    hashStr[64] = '\0';
    String calculatedChecksum(hashStr);

    LOG_I("[OTA] Expected checksum:   %s", appState.cachedChecksum.c_str());
    LOG_I("[OTA] Calculated checksum: %s", calculatedChecksum.c_str());

    if (calculatedChecksum.equalsIgnoreCase(appState.cachedChecksum)) {
      LOG_I("[OTA] Checksum verification passed");
    } else {
      LOG_E("[OTA] Checksum verification failed");
      Update.abort();
      setOTAProgress("error", "Checksum verification failed - firmware corrupted", 0);
      appState.otaInProgress = false;
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
      appState.otaInProgress = false;
      return false;
    }
  } else {
    LOG_E("[OTA] Update error: %s", Update.errorString());
    Update.abort();
    setCharField(appState.otaStatus, sizeof(appState.otaStatus), "error");
    snprintf(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), "Update error: %s", Update.errorString());
    appState.otaProgress = 0;
    appState.markOTADirty();
    appState.otaInProgress = false;
    return false;
  }
}

// Downgrade HTTPS URL to HTTP for heap-constrained downloads.
// Safety: only the binary payload is fetched over HTTP — integrity is verified
// by SHA256 checksum obtained from the authenticated HTTPS version check.
static String downgradeToHttp(const String& url) {
    String httpUrl = url;
    httpUrl.replace("https://", "http://");
    return httpUrl;
}

bool performOTAUpdate(String firmwareUrl) {
  appState.otaInProgress = true;
  appState.otaHttpFallback = false;
  setOTAProgress("preparing", "Preparing for update...", 0);

  LOG_I("[OTA] Starting OTA update");
  LOG_I("[OTA] Downloading from: %s", firmwareUrl.c_str());

  // Heap-based transport selection:
  //   >= 50KB: HTTPS with full cert validation (~43KB TLS cost)
  //   30-50KB: HTTPS insecure (no cert check, ~35KB TLS cost)
  //   10-30KB + SHA256: HTTP fallback (~4KB cost, integrity via SHA256)
  //   < 10KB: Abort — not enough even for plain HTTP
  uint32_t maxBlock = ESP.getMaxAllocHeap();
  bool hasChecksum = (appState.cachedChecksum.length() == 64);

  LOG_I("[OTA] Heap largest block: %lu bytes, checksum available: %s",
        (unsigned long)maxBlock, hasChecksum ? "yes" : "no");

  if (maxBlock < HEAP_OTA_ABORT_THRESHOLD_BYTES) {
    LOG_E("[OTA] Heap critically low: %lu bytes (<10KB), aborting", (unsigned long)maxBlock);
    setOTAProgress("error", "Insufficient memory for download", 0);
    appState.otaInProgress = false;
    return false;
  }

  // HTTP fallback path: plain WiFiClient, no TLS overhead
  if (maxBlock < HEAP_TLS_MIN_THRESHOLD_BYTES && hasChecksum) {
    LOG_W("[OTA] Heap too low for TLS (%lu bytes), using HTTP fallback with SHA256 verification",
          (unsigned long)maxBlock);
    appState.otaHttpFallback = true;
    appState.markOTADirty();

    String httpUrl = downgradeToHttp(firmwareUrl);
    LOG_I("[OTA] HTTP fallback URL: %s", httpUrl.c_str());

    WiFiClient plainClient;
    plainClient.setTimeout(30);  // 30 seconds

    HTTPClient http;
    if (!http.begin(plainClient, httpUrl)) {
      LOG_E("[OTA] Failed to initialize HTTP connection");
      setOTAProgress("error", "Failed to initialize connection", 0);
      appState.otaInProgress = false;
      return false;
    }

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(30000);

    setOTAProgress("preparing", "Connecting (HTTP fallback)...", 0);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY && httpCode != HTTP_CODE_FOUND) {
      LOG_E("[OTA] HTTP fallback failed, code: %d", httpCode);
      http.end();
      setOTAProgress("error", "HTTP fallback failed to connect", 0);
      appState.otaInProgress = false;
      return false;
    }

    return performDownloadAndFlash(http, http.getSize());
  }

  // HTTPS fallback without checksum — refuse HTTP when integrity can't be verified
  if (maxBlock < HEAP_TLS_MIN_THRESHOLD_BYTES) {
    LOG_E("[OTA] Heap too low for TLS (%lu bytes) and no checksum for HTTP fallback",
          (unsigned long)maxBlock);
    setOTAProgress("error", "Insufficient memory and no checksum for fallback", 0);
    appState.otaInProgress = false;
    return false;
  }

  // HTTPS path (>= 30KB heap available)
  WiFiClientSecure client;

  if (maxBlock < HEAP_TLS_SECURE_THRESHOLD_BYTES) {
    LOG_W("[OTA] Heap low (%lu bytes), using insecure TLS (no cert validation)", (unsigned long)maxBlock);
    client.setInsecure();
  } else if (appState.enableCertValidation && isNtpSynced()) {
    LOG_I("[OTA] Certificate validation enabled");
    client.setCACert(GITHUB_ROOT_CA);
  } else {
    if (appState.enableCertValidation && !isNtpSynced()) {
      LOG_W("[OTA] NTP not synced, skipping cert validation (clock not set)");
    } else {
      LOG_W("[OTA] Certificate validation disabled (insecure mode)");
    }
    client.setInsecure();
  }

  client.setTimeout(30000);

  HTTPClient https;
  if (!https.begin(client, firmwareUrl)) {
    LOG_E("[OTA] Failed to initialize HTTPS connection");
    setOTAProgress("error", "Failed to initialize secure connection", 0);
    appState.otaInProgress = false;
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
    appState.otaInProgress = false;
    return false;
  }

  return performDownloadAndFlash(https, https.getSize());
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
  if (!appState.justUpdated) return;
  
  JsonDocument doc;
  doc["type"] = "appState.justUpdated";
  doc["previousVersion"] = appState.previousFirmwareVersion;
  doc["currentVersion"] = firmwareVer;
  doc["message"] = String("Firmware successfully updated from ") + appState.previousFirmwareVersion + " to " + firmwareVer;
  
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);
  
  LOG_I("[OTA] Broadcast: Firmware updated from %s to %s", appState.previousFirmwareVersion.c_str(), firmwareVer);
  
  // Clear the flag after broadcasting
  appState.justUpdated = false;
  appState.previousFirmwareVersion = "";
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
    if (appState.otaInProgress) {
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
    
    appState.otaInProgress = true;
    setCharField(appState.otaStatus, sizeof(appState.otaStatus), "uploading");
    appState.otaProgress = 0;
    appState.otaProgressBytes = 0;
    appState.otaTotalBytes = 0;  // Unknown until upload completes
    setCharField(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), "Receiving firmware file...");
    appState.markOTADirty();
    
    // Play OTA update melody before flashing begins
    buzzer_play_blocking(BUZZ_OTA_UPDATE, 850);

    // Begin OTA update with unknown size (will auto-detect)
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      LOG_E("[OTA] Failed to begin update: %s", Update.errorString());
      uploadError = true;
      uploadErrorMessage = String("Failed to begin update: ") + Update.errorString();
      setCharField(appState.otaStatus, sizeof(appState.otaStatus), "error");
      setCharField(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), uploadErrorMessage.c_str());
      appState.otaInProgress = false;
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
      setCharField(appState.otaStatus, sizeof(appState.otaStatus), "error");
      setCharField(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), uploadErrorMessage.c_str());
      appState.otaInProgress = false;
      appState.markOTADirty();
      return;
    }

    appState.otaProgressBytes += upload.currentSize;

    // Signal progress periodically (every ~10KB or every 2 seconds)
    static unsigned long lastBroadcast = 0;
    static size_t lastBroadcastBytes = 0;
    unsigned long now = millis();

    if ((appState.otaProgressBytes - lastBroadcastBytes) >= 10240 || (now - lastBroadcast) >= 2000) {
      snprintf(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), "Uploading: %d KB received...", (int)(appState.otaProgressBytes / 1024));
      appState.markOTADirty();
      lastBroadcast = now;
      lastBroadcastBytes = appState.otaProgressBytes;
      LOG_D("[OTA] Received: %d KB", appState.otaProgressBytes / 1024);
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    // Skip finalization if there was an error
    if (uploadError) {
      return;
    }

    appState.otaTotalBytes = upload.totalSize;
    LOG_I("[OTA] Upload complete: %d bytes (%.2f KB)", upload.totalSize, upload.totalSize / 1024.0);

    setCharField(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), "Verifying firmware...");
    appState.otaProgress = 100;
    appState.markOTADirty();

    // Finalize update
    if (Update.end(true)) {
      if (Update.isFinished()) {
        LOG_I("[OTA] Firmware upload and verification successful");
        setCharField(appState.otaStatus, sizeof(appState.otaStatus), "complete");
        setCharField(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), "Upload complete! Rebooting...");
        appState.markOTADirty();
        // Note: Response and reboot handled in handleFirmwareUploadComplete
      } else {
        LOG_E("[OTA] Update did not finish correctly");
        uploadError = true;
        uploadErrorMessage = "Update verification failed";
        setCharField(appState.otaStatus, sizeof(appState.otaStatus), "error");
        setCharField(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), uploadErrorMessage.c_str());
        appState.otaInProgress = false;
        appState.markOTADirty();
      }
    } else {
      LOG_E("[OTA] Update finalization error: %s", Update.errorString());
      uploadError = true;
      uploadErrorMessage = String("Update error: ") + Update.errorString();
      setCharField(appState.otaStatus, sizeof(appState.otaStatus), "error");
      setCharField(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), uploadErrorMessage.c_str());
      appState.otaInProgress = false;
      appState.markOTADirty();
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    LOG_W("[OTA] Upload aborted by client");
    Update.abort();
    uploadError = true;
    uploadErrorMessage = "Upload aborted";
    setCharField(appState.otaStatus, sizeof(appState.otaStatus), "error");
    setCharField(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), "Upload aborted");
    appState.otaInProgress = false;
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
  if (strcmp(appState.otaStatus, "complete") == 0) {
    doc["success"] = true;
    doc["message"] = "Firmware uploaded successfully! Rebooting...";
    doc["bytesReceived"] = appState.otaTotalBytes;
    
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
    doc["message"] = strlen(appState.otaStatusMessage) > 0 ? appState.otaStatusMessage : "Upload failed";
    
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  }
  
  // Reset OTA state
  appState.otaInProgress = false;
}

// ===== Non-Blocking OTA FreeRTOS Tasks =====

// OTA download task — runs performOTAUpdate() on a separate core
static void otaDownloadTask(void* param) {
  // OTA download can take minutes — unsubscribe from watchdog
  esp_task_wdt_delete(NULL);
  // Also suspend loopTask WDT — TLS monopolizes the WiFi/lwIP stack
  wdtSuspendLoopTask();

  String firmwareUrl = appState.cachedFirmwareUrl;
  bool success = performOTAUpdate(firmwareUrl);

  if (success) {
    LOG_I("[OTA] Update successful, rebooting in 3 seconds");
    saveOTASuccessFlag(firmwareVer);
    vTaskDelay(pdMS_TO_TICKS(3000));  // Let main loop broadcast final status
    ESP.restart();
  } else {
    LOG_W("[OTA] Update failed");
    i2s_audio_reinstall_drivers();  // Restore I2S DMA buffers
    appState.audioPaused = false;   // Resume audio capture
    appState.otaInProgress = false;
    appState.updateDiscoveredTime = 0;
    appState.setFSMState(STATE_IDLE);
    appState.markOTADirty();
    // Re-subscribe loopTask to WDT now that TLS is done
    wdtResumeLoopTask();
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
  setCharField(appState.otaStatus, sizeof(appState.otaStatus), "preparing");
  setCharField(appState.otaStatusMessage, sizeof(appState.otaStatusMessage), "Preparing for update...");
  appState.otaProgress = 0;
  appState.setFSMState(STATE_OTA_UPDATE);
  appState.markOTADirty();

  // Pause audio capture and free I2S DMA buffers (~16KB internal SRAM)
  appState.audioPaused = true;
  vTaskDelay(pdMS_TO_TICKS(50));  // Ensure audio task exits i2s_read()
  i2s_audio_uninstall_drivers();  // Free ~16KB DMA buffers for TLS

  BaseType_t result = xTaskCreatePinnedToCore(
    otaDownloadTask, "OTA_DL", TASK_STACK_SIZE_OTA,
    NULL, TASK_PRIORITY_WEB, &otaDownloadTaskHandle, 0  // Core 0 (network stack affinity)
  );

  if (result != pdPASS) {
    LOG_E("[OTA] Failed to create download task");
    i2s_audio_reinstall_drivers();  // Restore I2S if task creation failed
    appState.audioPaused = false;
    appState.otaInProgress = false;
    setOTAProgress("error", "Failed to start update task", 0);
    appState.setFSMState(STATE_IDLE);
  }
}

// OTA check task — runs checkForFirmwareUpdate() on a separate core
static void otaCheckTaskFunc(void* param) {
  // TLS handshake (ECDSA verification) can take 5-10s without yielding —
  // unsubscribe from watchdog to prevent IDLE0 starvation panic on Core 0
  esp_task_wdt_delete(NULL);
  // Also suspend loopTask WDT — TLS monopolizes the WiFi/lwIP stack,
  // blocking loopTask from feeding its watchdog for >15s
  wdtSuspendLoopTask();

  // Heap pre-flight: MbedTLS I/O buffers (~32KB) are allocated from PSRAM
  // via the __wrap_esp_mbedtls_mem_calloc linker override, so internal SRAM
  // only needs enough for WiFi/lwIP packet buffers (~10-15KB).
  uint32_t maxBlock = ESP.getMaxAllocHeap();
  if (maxBlock < HEAP_TLS_MIN_THRESHOLD_BYTES) {
    LOG_W("[OTA] Heap too low for OTA check: %lu bytes (<30KB), skipping", (unsigned long)maxBlock);
    _otaConsecutiveFailures++;
    if (_otaConsecutiveFailures > 20) _otaConsecutiveFailures = 20;
    appState.markOTADirty();
    wdtResumeLoopTask();
    otaCheckTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }

  checkForFirmwareUpdate();

  // Also refresh WiFi status after check (needs dirty flag, not direct WS call)
  appState.markOTADirty();

  // Re-subscribe loopTask to WDT now that TLS is done
  wdtResumeLoopTask();

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
