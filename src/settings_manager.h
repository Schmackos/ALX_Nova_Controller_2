#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>

// ===== Settings Persistence =====
bool loadSettings();
void saveSettings();

// ===== Factory Reset =====
void performFactoryReset();

// ===== Settings HTTP API Handlers =====
void handleSettingsGet();
void handleSettingsUpdate();
void handleSettingsExport();
void handleSettingsImport();
void handleFactoryReset();
void handleReboot();

#endif // SETTINGS_MANAGER_H
