#ifndef OTA_UPDATER_H
#define OTA_UPDATER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ===== OTA Core Functions =====
void checkForFirmwareUpdate();
bool performOTAUpdate(String firmwareUrl);
bool getLatestReleaseInfo(String& version, String& firmwareUrl, String& checksum);
String calculateSHA256(uint8_t* data, size_t len);

// ===== OTA Status Broadcasting =====
void broadcastUpdateStatus();

// ===== NTP Time Synchronization =====
void syncTimeWithNTP();

// ===== OTA Success Flag =====
void saveOTASuccessFlag(const String& previousVersion);
bool checkAndClearOTASuccessFlag(String& previousVersion);
void broadcastJustUpdated();

// ===== OTA HTTP API Handlers =====
void handleCheckUpdate();
void handleStartUpdate();
void handleUpdateStatus();
void handleGetReleaseNotes();

// ===== Non-Blocking OTA Task =====
void startOTADownloadTask();
void startOTACheckTask();
bool isOTATaskRunning();
unsigned long getOTAEffectiveInterval();  // Backoff-aware check interval

// loopTaskHandle is defined in Arduino framework's main.cpp — OTA tasks
// temporarily unsubscribe it from WDT during TLS handshakes that can
// block the WiFi/lwIP stack for >15s
extern TaskHandle_t loopTaskHandle;

// ===== Manual Firmware Upload Handlers =====
void handleFirmwareUploadComplete();
void handleFirmwareUploadChunk();

#endif // OTA_UPDATER_H
