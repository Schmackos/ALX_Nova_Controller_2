#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>

// ===== Settings Persistence =====
bool loadSettings();
void saveSettings();

// Note: Certificate management removed - now using Mozilla certificate bundle
// via ESP32CertBundle library for automatic SSL validation

// ===== Factory Reset =====
void performFactoryReset();

// ===== Settings HTTP API Handlers =====
void handleSettingsGet();
void handleSettingsUpdate();
void handleSettingsExport();
void handleSettingsImport();
void handleFactoryReset();
void handleReboot();
void handleDiagnostics();

#endif // SETTINGS_MANAGER_H
