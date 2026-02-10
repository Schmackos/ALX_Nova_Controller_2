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
  bool darkMode = false;
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
  unsigned long lastSignalDetection = 0;
  unsigned long lastTimerUpdate = 0;
  float audioThreshold_dBFS = DEFAULT_AUDIO_THRESHOLD;
  bool amplifierState = false;
  float audioLevel_dBFS = -96.0f;
  bool previousSignalState = false;

  // ===== Per-ADC Audio State =====
  struct AdcState {
    float rmsLeft = 0.0f, rmsRight = 0.0f, rmsCombined = 0.0f;
    float vuLeft = 0.0f, vuRight = 0.0f, vuCombined = 0.0f;
    float peakLeft = 0.0f, peakRight = 0.0f, peakCombined = 0.0f;
    float vrmsLeft = 0.0f, vrmsRight = 0.0f, vrmsCombined = 0.0f;
    float dBFS = -96.0f;
    // Diagnostics
    uint8_t healthStatus = 0;      // AudioHealthStatus enum value
    uint32_t i2sErrors = 0;
    uint32_t allZeroBuffers = 0;
    uint32_t consecutiveZeros = 0;
    float noiseFloorDbfs = -96.0f;
    float dcOffset = 0.0f;
    unsigned long lastNonZeroMs = 0;
    uint32_t totalBuffers = 0;
    uint32_t clippedSamples = 0;
    float clipRate = 0.0f;           // EMA clip rate (0.0-1.0)
  };
  AdcState audioAdc[NUM_AUDIO_ADCS];
  int numAdcsDetected = 1; // How many ADCs are currently producing data

  // Legacy flat accessors (convenience aliases for audioAdc[0], used by existing WS/MQTT code)
  float &audioRmsLeft = audioAdc[0].rmsLeft;
  float &audioRmsRight = audioAdc[0].rmsRight;
  float &audioRmsCombined = audioAdc[0].rmsCombined;
  float &audioVuLeft = audioAdc[0].vuLeft;
  float &audioVuRight = audioAdc[0].vuRight;
  float &audioVuCombined = audioAdc[0].vuCombined;
  float &audioPeakLeft = audioAdc[0].peakLeft;
  float &audioPeakRight = audioAdc[0].peakRight;
  float &audioPeakCombined = audioAdc[0].peakCombined;
  float &audioVrmsLeft = audioAdc[0].vrmsLeft;
  float &audioVrmsRight = audioAdc[0].vrmsRight;
  float &audioVrmsCombined = audioAdc[0].vrmsCombined;
  uint8_t &audioHealthStatus = audioAdc[0].healthStatus;
  uint32_t &audioI2sErrors = audioAdc[0].i2sErrors;
  uint32_t &audioAllZeroBuffers = audioAdc[0].allZeroBuffers;
  uint32_t &audioConsecutiveZeros = audioAdc[0].consecutiveZeros;
  float &audioNoiseFloorDbfs = audioAdc[0].noiseFloorDbfs;
  unsigned long &audioLastNonZeroMs = audioAdc[0].lastNonZeroMs;
  uint32_t &audioTotalBuffers = audioAdc[0].totalBuffers;
  uint32_t &audioClippedSamples = audioAdc[0].clippedSamples;
  float &audioClipRate = audioAdc[0].clipRate;
  float &audioDcOffset = audioAdc[0].dcOffset;

  float audioDominantFreq = 0.0f;
  float audioSpectrumBands[16] = {};
  uint32_t audioSampleRate = DEFAULT_AUDIO_SAMPLE_RATE;
  float adcVref = DEFAULT_ADC_VREF; // ADC reference voltage (1.0-5.0V)

  // Input channel names (user-configurable, 4 channels = 2 ADCs x 2 channels)
  String inputNames[NUM_AUDIO_ADCS * 2];

  void setAmplifierState(bool state);
  void setSensingMode(SensingMode mode);
  void setTimerRemaining(unsigned long remaining);
  void setAudioLevel(float dBFS);
  bool isAmplifierDirty() const { return _amplifierDirty; }
  bool isSensingModeDirty() const { return _sensingModeDirty; }
  bool isTimerDirty() const { return _timerDirty; }
  bool isAudioDirty() const { return _audioDirty; }
  void clearAmplifierDirty() { _amplifierDirty = false; }
  void clearSensingModeDirty() { _sensingModeDirty = false; }
  void clearTimerDirty() { _timerDirty = false; }
  void clearAudioDirty() { _audioDirty = false; }

  // Smart Sensing heartbeat
  unsigned long lastSmartSensingHeartbeat = 0;

  // ===== Certificate Validation =====
  bool enableCertValidation = true;

  // ===== Audio Update Rate =====
  uint16_t audioUpdateRate = DEFAULT_AUDIO_UPDATE_RATE; // ms (20, 33, 50, 100)

  // ===== Audio Graph Toggles =====
  bool vuMeterEnabled = true;      // Enable VU meter computation & display
  bool waveformEnabled = true;     // Enable waveform computation & display
  bool spectrumEnabled = true;     // Enable FFT/spectrum computation & display

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
  float prevMqttAudioLevel = -96.0f;
  bool prevMqttBacklightOn = true;
  unsigned long prevMqttScreenTimeout = 60000;
  bool prevMqttBuzzerEnabled = true;
  int prevMqttBuzzerVolume = 1;
  uint8_t prevMqttBrightness = 255;
  bool prevMqttDimEnabled = false;
  unsigned long prevMqttDimTimeout = 10000;
  uint8_t prevMqttDimBrightness = 26;
  bool prevMqttVuMeterEnabled = true;
  bool prevMqttWaveformEnabled = true;
  bool prevMqttSpectrumEnabled = true;

  // ===== Smart Sensing Broadcast State Tracking =====
  SensingMode prevBroadcastMode = ALWAYS_ON;
  bool prevBroadcastAmplifierState = false;
  unsigned long prevBroadcastTimerRemaining = 0;
  float prevBroadcastAudioLevel = -96.0f;

  // ===== Display State (accessible from all interfaces) =====
  unsigned long screenTimeout = 60000; // Screen timeout in ms (default 60s)
  bool backlightOn = true;             // Runtime backlight state (not persisted)
  uint8_t backlightBrightness = 255;   // Backlight brightness (1-255, persisted)
  bool dimEnabled = false;               // Dim feature enabled
  unsigned long dimTimeout = 10000;     // Dim timeout in ms (default 10s)
  uint8_t dimBrightness = 26;           // Dim brightness PWM (1-255, default 10%)

  void setBacklightOn(bool state);
  void setScreenTimeout(unsigned long timeout);
  void setBacklightBrightness(uint8_t brightness);
  void setDimEnabled(bool enabled);
  void setDimTimeout(unsigned long timeout);
  void setDimBrightness(uint8_t brightness);
  bool isDisplayDirty() const { return _displayDirty; }
  void clearDisplayDirty() { _displayDirty = false; }

  // ===== Buzzer State (accessible from all interfaces) =====
  bool buzzerEnabled = true;   // Enable/disable buzzer feedback
  int buzzerVolume = 1;        // 0=Low, 1=Medium, 2=High

  void setBuzzerEnabled(bool enabled);
  void setBuzzerVolume(int volume);
  bool isBuzzerDirty() const { return _buzzerDirty; }
  void clearBuzzerDirty() { _buzzerDirty = false; }

  // ===== Settings Dirty Flag (for GUI -> WS/MQTT sync) =====
  bool isSettingsDirty() const { return _settingsDirty; }
  void clearSettingsDirty() { _settingsDirty = false; }
  void markSettingsDirty() { _settingsDirty = true; }

  // ===== OTA Dirty Flag (for OTA task -> main loop WS broadcast) =====
  bool isOTADirty() const { return _otaDirty; }
  void clearOTADirty() { _otaDirty = false; }
  void markOTADirty() { _otaDirty = true; }

  // ===== Signal Generator State =====
  bool sigGenEnabled = false;           // Always boots false
  int sigGenWaveform = 0;               // 0=sine, 1=square, 2=noise, 3=sweep
  float sigGenFrequency = 1000.0f;      // 1.0 - 22000.0 Hz
  float sigGenAmplitude = -6.0f;        // -96.0 to 0.0 dBFS
  int sigGenChannel = 2;                // 0=L, 1=R, 2=Both
  int sigGenOutputMode = 0;             // 0=software, 1=PWM
  float sigGenSweepSpeed = 1000.0f;     // Hz per second
  int sigGenTargetAdc = 2;              // 0=Input 1, 1=Input 2, 2=Both

  void setSignalGenEnabled(bool enabled);
  void markSignalGenDirty() { _sigGenDirty = true; }
  bool isSignalGenDirty() const { return _sigGenDirty; }
  void clearSignalGenDirty() { _sigGenDirty = false; }

  // MQTT state tracking for signal generator
  bool prevMqttSigGenEnabled = false;
  int prevMqttSigGenWaveform = 0;
  float prevMqttSigGenFrequency = 1000.0f;
  float prevMqttSigGenAmplitude = -6.0f;
  int prevMqttSigGenOutputMode = 0;

  // ===== GUI State =====
#ifdef GUI_ENABLED
  bool guiDarkMode = false;            // GUI dark mode (separate from web darkMode)
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
  bool _audioDirty = false;
  bool _displayDirty = false;
  bool _buzzerDirty = false;
  bool _settingsDirty = false;
  bool _sigGenDirty = false;
  bool _otaDirty = false;
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
#define darkMode appState.darkMode
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
#define lastSignalDetection appState.lastSignalDetection
#define lastTimerUpdate appState.lastTimerUpdate
#define audioThreshold_dBFS appState.audioThreshold_dBFS
#define amplifierState appState.amplifierState
#define audioLevel_dBFS appState.audioLevel_dBFS
#define previousSignalState appState.previousSignalState
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
#define prevMqttAudioLevel appState.prevMqttAudioLevel
#define prevMqttBacklightOn appState.prevMqttBacklightOn
#define prevMqttScreenTimeout appState.prevMqttScreenTimeout
#define prevMqttBuzzerEnabled appState.prevMqttBuzzerEnabled
#define prevMqttBuzzerVolume appState.prevMqttBuzzerVolume
#define prevMqttBrightness appState.prevMqttBrightness
#define prevMqttDimTimeout appState.prevMqttDimTimeout

// Smart Sensing Broadcast State Tracking
#define prevBroadcastMode appState.prevBroadcastMode
#define prevBroadcastAmplifierState appState.prevBroadcastAmplifierState
#define prevBroadcastTimerRemaining appState.prevBroadcastTimerRemaining
#define prevBroadcastAudioLevel appState.prevBroadcastAudioLevel

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
