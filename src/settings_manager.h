#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>

// ===== Settings Persistence =====
bool loadSettings();
void saveSettings();

// ===== Certificate Persistence =====
bool loadCertificate();
void saveCertificate();
void resetCertificateToDefault();
String getDefaultCertificate();

// ===== Factory Reset =====
void performFactoryReset();

// ===== Settings HTTP API Handlers =====
void handleSettingsGet();
void handleSettingsUpdate();
void handleSettingsExport();
void handleSettingsImport();
void handleFactoryReset();
void handleReboot();

// ===== Certificate HTTP API Handlers =====
void handleCertificateGet();
void handleCertificateUpdate();
void handleCertificateReset();

#endif // SETTINGS_MANAGER_H
