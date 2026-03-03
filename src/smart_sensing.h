#ifndef SMART_SENSING_H
#define SMART_SENSING_H

#include <Arduino.h>

// ===== Smart Sensing Core Functions =====
bool detectSignal();
void setAmplifierState(bool state);
void updateSmartSensingLogic();

// ===== Smart Sensing State Broadcasting =====
void sendSmartSensingState();
void sendSmartSensingStateInternal();

// ===== Smart Sensing Settings =====
bool loadSmartSensingSettings();
void saveSmartSensingSettings();
void saveSmartSensingSettingsDeferred();  // Mark dirty — actual save after 2s idle
void checkDeferredSmartSensingSave();     // Call from main loop

// ===== Smart Sensing HTTP API Handlers =====
void handleSmartSensingGet();
void handleSmartSensingUpdate();

#endif // SMART_SENSING_H
