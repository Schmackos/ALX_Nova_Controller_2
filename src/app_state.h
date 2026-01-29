#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <PubSubClient.h>
#include "config.h"

// ===== Server Instances =====
extern WebServer server;
extern WebSocketsServer webSocket;

// ===== WiFi State =====
extern String wifiSSID;
extern String wifiPassword;

// ===== Device Serial Number =====
extern String deviceSerialNumber;

// ===== LED State =====
extern bool blinkingEnabled;
extern bool ledState;
extern unsigned long previousMillis;

// ===== AP Mode State =====
extern bool isAPMode;
extern bool apEnabled;
extern String apSSID;
extern String apPassword;

// ===== Factory Reset State =====
extern bool factoryResetInProgress;

// ===== OTA Update State =====
extern const char* firmwareVer;
extern const char* githubRepoOwner;
extern const char* githubRepoName;
extern unsigned long lastOTACheck;
extern bool otaInProgress;
extern int otaProgress;
extern String otaStatus;
extern String otaStatusMessage;
extern int otaProgressBytes;
extern int otaTotalBytes;
extern bool autoUpdateEnabled;
extern String cachedFirmwareUrl;
extern String cachedChecksum;
extern int timezoneOffset;
extern bool nightMode;
extern bool updateAvailable;
extern String cachedLatestVersion;
extern unsigned long updateDiscoveredTime;

// ===== Smart Sensing State =====
extern SensingMode currentMode;
extern unsigned long timerDuration;
extern unsigned long timerRemaining;
extern unsigned long lastVoltageDetection;
extern unsigned long lastTimerUpdate;
extern float voltageThreshold;
extern bool amplifierState;
extern float lastVoltageReading;
extern bool previousVoltageState;

// Smart Sensing broadcast optimization
extern SensingMode prevBroadcastMode;
extern bool prevBroadcastAmplifierState;
extern unsigned long prevBroadcastTimerRemaining;
extern float prevBroadcastVoltageReading;
extern unsigned long lastSmartSensingHeartbeat;

// ===== Certificate Validation =====
extern bool enableCertValidation;

// ===== MQTT State =====
extern bool mqttEnabled;
extern String mqttBroker;
extern int mqttPort;
extern String mqttUsername;
extern String mqttPassword;
extern String mqttBaseTopic;
extern bool mqttHADiscovery;

// MQTT client objects
extern WiFiClient mqttWifiClient;
extern PubSubClient mqttClient;

// MQTT connection state
extern unsigned long lastMqttReconnect;
extern bool mqttConnected;

// MQTT state tracking for change detection
extern bool prevMqttLedState;
extern bool prevMqttBlinkingEnabled;
extern bool prevMqttAmplifierState;
extern SensingMode prevMqttSensingMode;
extern unsigned long prevMqttTimerRemaining;
extern float prevMqttVoltageReading;
extern unsigned long lastMqttPublish;

// ===== GitHub Root CA Certificate =====
extern const char* github_root_ca;

#endif // APP_STATE_H
