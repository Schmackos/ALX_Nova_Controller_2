#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>

// ===== Settings Persistence =====
bool loadSettings();
void saveSettings();
void saveSettingsDeferred();       // Mark dirty — actual save after 2s idle
void checkDeferredSettingsSave();  // Call from main loop

// Returns true if loadSettings() already loaded MQTT settings from /config.json.
// Used by loadMqttSettings() to skip /mqtt_config.txt when not needed.
bool settingsMqttLoadedFromJson();

// Note: Certificate management removed - now using Mozilla certificate bundle
// via ESP32CertBundle library for automatic SSL validation

// ===== Signal Generator Settings =====
bool loadSignalGenSettings();
void saveSignalGenSettings();
void saveSignalGenSettingsDeferred();  // Mark dirty — actual save after 2s idle
void checkDeferredSignalGenSave();     // Call from main loop

// ===== Input Names Settings =====
bool loadInputNames();
void saveInputNames();

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
