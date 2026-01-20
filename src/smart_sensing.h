#ifndef SMART_SENSING_H
#define SMART_SENSING_H

#include <Arduino.h>

// ===== Smart Sensing Core Functions =====
bool detectVoltage();
void setAmplifierState(bool state);
void updateSmartSensingLogic();

// ===== Smart Sensing State Broadcasting =====
void sendSmartSensingState();
void sendSmartSensingStateInternal();

// ===== Smart Sensing Settings =====
bool loadSmartSensingSettings();
void saveSmartSensingSettings();

// ===== Smart Sensing HTTP API Handlers =====
void handleSmartSensingGet();
void handleSmartSensingUpdate();

#endif // SMART_SENSING_H
