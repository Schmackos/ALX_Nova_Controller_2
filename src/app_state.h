#ifndef APP_STATE_H
#define APP_STATE_H

#include "config.h"
#include <Arduino.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

// ===== FSM Application States =====
enum AppFSMState {
  STATE_IDLE,
  STATE_SIGNAL_DETECTED,
  STATE_AUTO_OFF_TIMER,
  STATE_WEB_CONFIG,
  STATE_OTA_UPDATE,
  STATE_ERROR
};

// ===== AppState Singleton Class =====
class AppState {
public:
  // Singleton access
  static AppState &getInstance() {
    static AppState instance;
    return instance;
  }

  // Delete copy/move constructors
  AppState(const AppState &) = delete;
  AppState &operator=(const AppState &) = delete;

  // ===== FSM State =====
  AppFSMState fsmState = STATE_IDLE;
  void setFSMState(AppFSMState newState);
  bool isFSMStateDirty() const { return _fsmStateDirty; }
  void clearFSMStateDirty() { _fsmStateDirty = false; }

  // ===== WiFi State =====
  String wifiSSID;
  String wifiPassword;

  // ===== Device Information =====
  String deviceSerialNumber;

  // ===== LED State =====
  bool blinkingEnabled = true;
  bool ledState = false;
  unsigned long previousMillis = 0;

  void setLedState(bool state);
  void setBlinkingEnabled(bool enabled);
  bool isLedStateDirty() const { return _ledStateDirty; }
  bool isBlinkingDirty() const { return _blinkingDirty; }
  void clearLedStateDirty() { _ledStateDirty = false; }
  void clearBlinkingDirty() { _blinkingDirty = false; }

  // ===== AP Mode State =====
  bool isAPMode = false;
  bool apEnabled = false;
  bool autoAPEnabled = true; // Default to true per requirements
  String apSSID;
  String apPassword = DEFAULT_AP_PASSWORD;

  // ===== Web Authentication =====
  String webPassword = DEFAULT_AP_PASSWORD;

  // ===== WiFi Connection State (Async) =====
  bool wifiConnecting = false;
  bool wifiConnectSuccess = false;
  String wifiNewIP;
  String wifiConnectError;

  // ===== Factory Reset State =====
  bool factoryResetInProgress = false;

  // ===== OTA Update State =====
  unsigned long lastOTACheck = 0;
  bool otaInProgress = false;
  int otaProgress = 0;
  String otaStatus = "idle";
  String otaStatusMessage = "idle";
  int otaProgressBytes = 0;
  int otaTotalBytes = 0;
  bool autoUpdateEnabled = false;
  String cachedFirmwareUrl;
  String cachedChecksum;
  int timezoneOffset = 0;
  int dstOffset = 0;
  bool nightMode = false;
  bool updateAvailable = false;
  String cachedLatestVersion;
  unsigned long updateDiscoveredTime = 0;

  // ===== OTA Just Updated State =====
  bool justUpdated = false;
  String previousFirmwareVersion;

  // ===== Smart Sensing State =====
  SensingMode currentMode = ALWAYS_ON;
  unsigned long timerDuration = DEFAULT_TIMER_DURATION;
  unsigned long timerRemaining = 0;
  unsigned long lastVoltageDetection = 0;
  unsigned long lastTimerUpdate = 0;
  float voltageThreshold = DEFAULT_VOLTAGE_THRESHOLD;
  bool amplifierState = false;
  float lastVoltageReading = 0.0;
  bool previousVoltageState = false;

  void setAmplifierState(bool state);
  void setSensingMode(SensingMode mode);
  void setTimerRemaining(unsigned long remaining);
  void setVoltageReading(float reading);
  bool isAmplifierDirty() const { return _amplifierDirty; }
  bool isSensingModeDirty() const { return _sensingModeDirty; }
  bool isTimerDirty() const { return _timerDirty; }
  bool isVoltageDirty() const { return _voltageDirty; }
  void clearAmplifierDirty() { _amplifierDirty = false; }
  void clearSensingModeDirty() { _sensingModeDirty = false; }
  void clearTimerDirty() { _timerDirty = false; }
  void clearVoltageDirty() { _voltageDirty = false; }

  // Smart Sensing heartbeat
  unsigned long lastSmartSensingHeartbeat = 0;

  // ===== Certificate Validation =====
  bool enableCertValidation = true;

  // ===== Hardware Stats =====
  unsigned long hardwareStatsInterval = HARDWARE_STATS_INTERVAL;

  // ===== MQTT State =====
  bool mqttEnabled = false;
  String mqttBroker;
  int mqttPort = DEFAULT_MQTT_PORT;
  String mqttUsername;
  String mqttPassword;
  String mqttBaseTopic;
  bool mqttHADiscovery = false;
  unsigned long lastMqttReconnect = 0;
  bool mqttConnected = false;
  unsigned long lastMqttPublish = 0;

  // ===== MQTT State Tracking (for change detection) =====
  bool prevMqttLedState = false;
  bool prevMqttBlinkingEnabled = true;
  bool prevMqttAmplifierState = false;
  SensingMode prevMqttSensingMode = ALWAYS_ON;
  unsigned long prevMqttTimerRemaining = 0;
  float prevMqttVoltageReading = 0.0;
  bool prevMqttBacklightOn = true;
  unsigned long prevMqttScreenTimeout = 60000;

  // ===== Smart Sensing Broadcast State Tracking =====
  SensingMode prevBroadcastMode = ALWAYS_ON;
  bool prevBroadcastAmplifierState = false;
  unsigned long prevBroadcastTimerRemaining = 0;
  float prevBroadcastVoltageReading = 0.0;

  // ===== Display State (accessible from all interfaces) =====
  unsigned long screenTimeout = 60000; // Screen timeout in ms (default 60s)
  bool backlightOn = true;             // Runtime backlight state (not persisted)

  void setBacklightOn(bool state);
  void setScreenTimeout(unsigned long timeout);
  bool isDisplayDirty() const { return _displayDirty; }
  void clearDisplayDirty() { _displayDirty = false; }

  // ===== Settings Dirty Flag (for GUI -> WS/MQTT sync) =====
  bool isSettingsDirty() const { return _settingsDirty; }
  void clearSettingsDirty() { _settingsDirty = false; }
  void markSettingsDirty() { _settingsDirty = true; }

  // ===== GUI State =====
#ifdef GUI_ENABLED
  bool guiDarkMode = false;            // GUI dark mode (separate from web nightMode)
  bool bootAnimEnabled = true;         // Enable/disable boot animation
  int bootAnimStyle = 0;               // 0-5 animation style index
#endif

  // ===== Error State =====
  int errorCode = 0;
  String errorMessage;

  void setError(int code, const String &message);
  void clearError();
  bool hasError() const { return errorCode != 0; }

  // ===== Reconnection Backoff =====
  unsigned long wifiBackoffDelay = 1000;
  unsigned long mqttBackoffDelay = 1000;
  static const unsigned long MAX_BACKOFF_DELAY = 60000;

  void increaseWiFiBackoff();
  void increaseMqttBackoff();
  void resetWiFiBackoff() { wifiBackoffDelay = 1000; }
  void resetMqttBackoff() { mqttBackoffDelay = 1000; }

  // ===== Utility Methods =====
  void clearAllDirtyFlags();
  bool hasAnyDirtyFlag() const;

private:
  AppState() {} // Private constructor

  // Dirty flags for change detection
  bool _fsmStateDirty = false;
  bool _ledStateDirty = false;
  bool _blinkingDirty = false;
  bool _amplifierDirty = false;
  bool _sensingModeDirty = false;
  bool _timerDirty = false;
  bool _voltageDirty = false;
  bool _displayDirty = false;
  bool _settingsDirty = false;
};

// Convenience macro for accessing AppState
#define appState AppState::getInstance()

// ===== Legacy variable aliases for backward compatibility =====
// These macros allow existing code to use global variable names
// while accessing the AppState singleton internally.
// TODO: Gradually update handlers to use appState.memberName directly

// WiFi State
#define wifiSSID appState.wifiSSID
#define wifiPassword appState.wifiPassword

// Device Information
#define deviceSerialNumber appState.deviceSerialNumber

// LED State
#define blinkingEnabled appState.blinkingEnabled
#define ledState appState.ledState
#define previousMillis appState.previousMillis

// AP Mode State
#define isAPMode appState.isAPMode
#define apEnabled appState.apEnabled
#define apSSID appState.apSSID
#define apPassword appState.apPassword
#define autoAPEnabled appState.autoAPEnabled

// WiFi Connection State (Async)
#define wifiConnecting appState.wifiConnecting
#define wifiConnectSuccess appState.wifiConnectSuccess
#define wifiNewIP appState.wifiNewIP
#define wifiConnectError appState.wifiConnectError

// Factory Reset State
#define factoryResetInProgress appState.factoryResetInProgress

// OTA Update State
#define lastOTACheck appState.lastOTACheck
#define otaInProgress appState.otaInProgress
#define otaProgress appState.otaProgress
#define otaStatus appState.otaStatus
#define otaStatusMessage appState.otaStatusMessage
#define otaProgressBytes appState.otaProgressBytes
#define otaTotalBytes appState.otaTotalBytes
#define autoUpdateEnabled appState.autoUpdateEnabled
#define cachedFirmwareUrl appState.cachedFirmwareUrl
#define cachedChecksum appState.cachedChecksum
#define timezoneOffset appState.timezoneOffset
#define dstOffset appState.dstOffset
#define nightMode appState.nightMode
#define updateAvailable appState.updateAvailable
#define cachedLatestVersion appState.cachedLatestVersion
#define updateDiscoveredTime appState.updateDiscoveredTime

// OTA Just Updated State
#define justUpdated appState.justUpdated
#define previousFirmwareVersion appState.previousFirmwareVersion

// Smart Sensing State
#define currentMode appState.currentMode
#define timerDuration appState.timerDuration
#define timerRemaining appState.timerRemaining
#define lastVoltageDetection appState.lastVoltageDetection
#define lastTimerUpdate appState.lastTimerUpdate
#define voltageThreshold appState.voltageThreshold
#define amplifierState appState.amplifierState
#define lastVoltageReading appState.lastVoltageReading
#define previousVoltageState appState.previousVoltageState
#define lastSmartSensingHeartbeat appState.lastSmartSensingHeartbeat

// Certificate Validation
#define enableCertValidation appState.enableCertValidation

// Hardware Stats
#define hardwareStatsInterval appState.hardwareStatsInterval

// MQTT State
#define mqttEnabled appState.mqttEnabled
#define mqttBroker appState.mqttBroker
#define mqttPort appState.mqttPort
#define mqttUsername appState.mqttUsername
#define mqttPassword appState.mqttPassword
#define mqttBaseTopic appState.mqttBaseTopic
#define mqttHADiscovery appState.mqttHADiscovery
#define lastMqttReconnect appState.lastMqttReconnect
#define mqttConnected appState.mqttConnected
#define lastMqttPublish appState.lastMqttPublish

// MQTT State Tracking (for change detection)
#define prevMqttLedState appState.prevMqttLedState
#define prevMqttBlinkingEnabled appState.prevMqttBlinkingEnabled
#define prevMqttAmplifierState appState.prevMqttAmplifierState
#define prevMqttSensingMode appState.prevMqttSensingMode
#define prevMqttTimerRemaining appState.prevMqttTimerRemaining
#define prevMqttVoltageReading appState.prevMqttVoltageReading
#define prevMqttBacklightOn appState.prevMqttBacklightOn
#define prevMqttScreenTimeout appState.prevMqttScreenTimeout

// Smart Sensing Broadcast State Tracking
#define prevBroadcastMode appState.prevBroadcastMode
#define prevBroadcastAmplifierState appState.prevBroadcastAmplifierState
#define prevBroadcastTimerRemaining appState.prevBroadcastTimerRemaining
#define prevBroadcastVoltageReading appState.prevBroadcastVoltageReading

// ===== Global Object extern declarations =====
// These are actual global objects, not AppState members
extern WebServer server;
extern WebSocketsServer webSocket;
extern WiFiClient mqttWifiClient;
extern PubSubClient mqttClient;

// Firmware info (const, not state)
extern const char *firmwareVer;
extern const char *githubRepoOwner;
extern const char *githubRepoName;

#endif // APP_STATE_H
