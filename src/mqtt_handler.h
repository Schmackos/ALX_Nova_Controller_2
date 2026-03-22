#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

#ifdef USB_AUDIO_ENABLED
#define MQTT_TOPIC_USB_CONNECTED  "audio/usb/connected"
#define MQTT_TOPIC_USB_STREAMING  "audio/usb/streaming"
#define MQTT_TOPIC_USB_ENABLED    "audio/usb/enabled"
#define MQTT_TOPIC_USB_RATE       "audio/usb/sampleRate"
#define MQTT_TOPIC_USB_VOLUME     "audio/usb/volume"
#define MQTT_TOPIC_USB_OVERRUNS   "audio/usb/overruns"
#define MQTT_TOPIC_USB_UNDERRUNS  "audio/usb/underruns"
#define MQTT_TOPIC_USB_ENABLE_SET "audio/usb/enabled/set"
#endif

// ===== MQTT Core Functions =====
void setupMqtt();
void mqttLoop();
void mqttReconnect();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttPublishPendingState();
void mqttPublishHeartbeat();

// ===== MQTT Settings =====
bool loadMqttSettings();
void saveMqttSettings();

// ===== MQTT State Publishing =====
void publishMqttState();
void publishMqttSmartSensingState();
void publishMqttWifiStatus();
void publishMqttSystemStatusStatic();
void publishMqttSystemStatus();
void publishMqttUpdateState();
void publishMqttHardwareStatsStatic();
void publishMqttHardwareStats();
void publishMqttCrashDiagnosticsStatic();
void publishMqttDisplayState();
void publishMqttBuzzerState();
void publishMqttSignalGenState();
void publishMqttAudioDiagnostics();
void publishMqttAudioGraphState();
void publishMqttAdcEnabledState();
void publishMqttDebugState();
void publishMqttCrashDiagnostics();
void publishMqttInputNames();
#ifdef DAC_ENABLED
void publishMqttHalDevices();
#endif
#ifdef DSP_ENABLED
void publishMqttDspState();
#endif
#ifdef GUI_ENABLED
void publishMqttBootAnimState();
#endif
#ifdef USB_AUDIO_ENABLED
void publishMqttUsbAudioState();
#endif

// ===== Diagnostic Event Publishing =====
void publishMqttDiagEvent();

// ===== MQTT Topic Helpers =====
String getEffectiveMqttBaseTopic();

// ===== Home Assistant Discovery =====
void publishHADiscovery();
void removeHADiscovery();
void subscribeToMqttTopics();
String getMqttDeviceId();

// ===== MQTT HTTP API Handlers =====
void handleMqttGet();
void handleMqttUpdate();

#endif // MQTT_HANDLER_H
