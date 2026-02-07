#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// ===== MQTT Core Functions =====
void setupMqtt();
void mqttLoop();
void mqttReconnect();
void mqttCallback(char* topic, byte* payload, unsigned int length);

// ===== MQTT Settings =====
bool loadMqttSettings();
void saveMqttSettings();

// ===== MQTT State Publishing =====
void publishMqttState();
void publishMqttLedState();
void publishMqttBlinkingState();
void publishMqttSmartSensingState();
void publishMqttWifiStatus();
void publishMqttSystemStatus();
void publishMqttUpdateState();
void publishMqttHardwareStats();
void publishMqttDisplayState();
void publishMqttBuzzerState();
void publishMqttSignalGenState();

// ===== Home Assistant Discovery =====
void publishHADiscovery();
void removeHADiscovery();
void subscribeToMqttTopics();
String getMqttDeviceId();

// ===== MQTT HTTP API Handlers =====
void handleMqttGet();
void handleMqttUpdate();

#endif // MQTT_HANDLER_H
