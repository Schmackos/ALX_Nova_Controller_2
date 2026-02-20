#ifndef APP_STATE_H
#define APP_STATE_H

#include "config.h"

#ifdef NATIVE_TEST
#include "../test/test_mocks/Arduino.h"
// Native test stubs for Arduino libraries
class WiFiClient {
public:
    bool connected() { return false; }
    void stop() {}
};
class PubSubClient {
public:
    bool connected() { return false; }
    bool connect(const char*) { return false; }
    bool publish(const char*, const char*) { return false; }
    void loop() {}
};
class WebServer {
public:
    void handleClient() {}
};
class WebSocketsServer {
public:
    void loop() {}
    void broadcastTXT(const String&) {}
};
class WiFiClass {
public:
    bool isConnected() { return false; }
};
#else
#include <Arduino.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#endif

#include <cstring>

// ===== Helper: safe char[] field assignment =====
inline void setCharField(char *dst, size_t dstSize, const char *src) {
  if (src) {
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
  } else {
    dst[0] = '\0';
  }
}

// ===== FFT Window Types =====
enum FftWindowType : uint8_t {
  FFT_WINDOW_HANN = 0,
  FFT_WINDOW_BLACKMAN,
  FFT_WINDOW_BLACKMAN_HARRIS,
  FFT_WINDOW_BLACKMAN_NUTTALL,
  FFT_WINDOW_NUTTALL,
  FFT_WINDOW_FLAT_TOP,
  FFT_WINDOW_COUNT
};

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
  char wifiSSID[33] = {};
  char wifiPassword[65] = {};

  // ===== Device Information =====
  char deviceSerialNumber[17] = {};
  char customDeviceName[33] = {};  // User-configurable name used as AP SSID (overrides auto-generated)

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
  char apSSID[33] = {};
  char apPassword[33] = {};

  // ===== Web Authentication =====
  char webPassword[65] = {};  // SHA256 hex hash = 64 chars + null

  // ===== WiFi Connection State (Async) =====
  bool wifiConnecting = false;
  bool wifiConnectSuccess = false;
  char wifiNewIP[16] = {};
  char wifiConnectError[64] = {};

  // ===== Factory Reset State =====
  bool factoryResetInProgress = false;

  // ===== OTA Update State =====
  unsigned long lastOTACheck = 0;
  bool otaInProgress = false;
  int otaProgress = 0;
  char otaStatus[16] = "idle";
  char otaStatusMessage[64] = "idle";
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

  bool otaHttpFallback = false;  // True when HTTP fallback was used (SHA256-verified)

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
    float rms1 = 0.0f, rms2 = 0.0f, rmsCombined = 0.0f;
    float vu1 = 0.0f, vu2 = 0.0f, vuCombined = 0.0f;
    float peak1 = 0.0f, peak2 = 0.0f, peakCombined = 0.0f;
    float vrms1 = 0.0f, vrms2 = 0.0f, vrmsCombined = 0.0f;
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
    uint32_t i2sRecoveries = 0;      // I2S driver restart count (timeout recovery)
  };
  AdcState audioAdc[NUM_AUDIO_INPUTS]; // Per-input state (ADC1, ADC2, USB)
  int numAdcsDetected = 1;  // How many I2S ADCs are producing data
  int numInputsDetected = 1; // Total audio inputs detected (ADCs + USB)

  // ===== ADC Clock Sync Diagnostics =====
  float adcSyncOffsetSamples = 0.0f;  // Phase offset ADC1->ADC2 in samples
  float adcSyncCorrelation   = 0.0f;  // Cross-correlation peak (0.0-1.0)
  bool  adcSyncOk            = true;  // true when |offset| <= threshold

  // ===== I2S Runtime Metrics (written by audio task, read by diagnostics) =====
  struct I2sRuntimeMetrics {
    uint32_t audioTaskStackFree = 0;           // bytes remaining (high watermark × 4)
    float buffersPerSec[NUM_AUDIO_INPUTS] = {};  // actual buf/s per input
    float avgReadLatencyUs[NUM_AUDIO_INPUTS] = {};// avg read time in µs
  };
  I2sRuntimeMetrics i2sMetrics;

  // Legacy flat accessors (convenience aliases for audioAdc[0], used by existing WS/MQTT code)
  float &audioRmsLeft = audioAdc[0].rms1;
  float &audioRmsRight = audioAdc[0].rms2;
  float &audioRmsCombined = audioAdc[0].rmsCombined;
  float &audioVuLeft = audioAdc[0].vu1;
  float &audioVuRight = audioAdc[0].vu2;
  float &audioVuCombined = audioAdc[0].vuCombined;
  float &audioPeakLeft = audioAdc[0].peak1;
  float &audioPeakRight = audioAdc[0].peak2;
  float &audioPeakCombined = audioAdc[0].peakCombined;
  float &audioVrms1 = audioAdc[0].vrms1;
  float &audioVrms2 = audioAdc[0].vrms2;
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
  bool adcEnabled[NUM_AUDIO_INPUTS] = {true, true, false}; // Per-input enable (USB default off)
  volatile bool audioPaused = false; // Set true to pause audio_capture_task I2S reads (for I2S reinit)

  // ===== Stack Overflow Detection (set by vApplicationStackOverflowHook, handled in loop()) =====
  volatile bool stackOverflowDetected = false;
  char stackOverflowTaskName[16] = {0};

  // Input channel names (user-configurable, 6 channels = 3 inputs x 2 channels)
  String inputNames[NUM_AUDIO_INPUTS * 2];

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

  // ===== FFT Window Type =====
  FftWindowType fftWindowType = FFT_WINDOW_HANN; // Default to Hann

  // ===== Audio Signal Quality Metrics =====
  float audioSnrDb[NUM_AUDIO_INPUTS] = {};        // Signal-to-Noise Ratio (dB)
  float audioSfdrDb[NUM_AUDIO_INPUTS] = {};       // Spurious-Free Dynamic Range (dB)

  // ===== Heap Health =====
  bool heapCritical = false;       // True when largest free block < 40KB — WiFi RX drops silently
  bool heapWarning  = false;       // True when largest free block < 60KB — approaching critical
  uint32_t heapMaxBlockBytes = 0;  // Current largest contiguous free block in internal SRAM
  uint32_t wifiRxWatchdogRecoveries = 0;  // Number of WiFi reconnects triggered by RX watchdog
  unsigned long heapCriticalSinceMs = 0;  // millis() when heap first went critical; 0 if not critical

  // ===== Debug Mode Toggles =====
  bool debugMode = true;           // Master debug gate
  int debugSerialLevel = 2;        // 0=Off, 1=Errors, 2=Info, 3=Debug
  bool debugHwStats = true;        // HW stats WS broadcast + web tab
  bool debugI2sMetrics = true;     // I2S runtime metrics in audio task
  bool debugTaskMonitor = false;   // Task monitor update & serial print (opt-in)

  // ===== Hardware Stats =====
  unsigned long hardwareStatsInterval = HARDWARE_STATS_INTERVAL;

  // ===== MQTT State =====
  bool mqttEnabled = false;
  char mqttBroker[65] = {};
  int mqttPort = DEFAULT_MQTT_PORT;
  char mqttUsername[33] = {};
  char mqttPassword[65] = {};
  char mqttBaseTopic[65] = {};
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
  bool prevMqttDebugMode = true;
  int prevMqttDebugSerialLevel = 2;
  bool prevMqttDebugHwStats = true;
  bool prevMqttDebugI2sMetrics = true;
  bool prevMqttDebugTaskMonitor = true;
  FftWindowType prevMqttFftWindowType = FFT_WINDOW_HANN;

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

  // ===== Emergency Safety Limiter (Speaker Protection) =====
  bool emergencyLimiterEnabled = true;       // Default ON for safety
  float emergencyLimiterThresholdDb = -0.1f; // Threshold in dBFS (-6.0 to 0.0)

  void setEmergencyLimiterEnabled(bool enabled);
  void setEmergencyLimiterThreshold(float dbfs);
  bool isEmergencyLimiterDirty() const { return _emergencyLimiterDirty; }
  void clearEmergencyLimiterDirty() { _emergencyLimiterDirty = false; }

  // ===== Audio Quality Diagnostics (Phase 3) =====
  bool audioQualityEnabled = false;          // Default OFF (opt-in)
  float audioQualityGlitchThreshold = 0.5f;  // Discontinuity threshold (0.1-1.0)

  void setAudioQualityEnabled(bool enabled);
  void setAudioQualityThreshold(float threshold);
  bool isAudioQualityDirty() const { return _audioQualityDirty; }
  void clearAudioQualityDirty() { _audioQualityDirty = false; }

  // ===== ADC Enabled Dirty Flag (for WS/MQTT sync) =====
  void markAdcEnabledDirty() { _adcEnabledDirty = true; }
  bool isAdcEnabledDirty() const { return _adcEnabledDirty; }
  void clearAdcEnabledDirty() { _adcEnabledDirty = false; }

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
  int sigGenChannel = 2;                // 0=Ch1, 1=Ch2, 2=Both
  int sigGenOutputMode = 0;             // 0=software, 1=PWM
  float sigGenSweepSpeed = 1000.0f;     // Hz per second
  int sigGenTargetAdc = 2;              // 0=ADC 1, 1=ADC 2, 2=Both

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
  float prevMqttSigGenSweepSpeed = 1000.0f;

  // ===== DSP Pipeline State =====
#ifdef DSP_ENABLED
  bool dspEnabled = false;     // Master DSP enable
  bool dspBypass = false;      // Master bypass (pass-through)

  // DSP Presets (up to 32 named slots)
  int8_t dspPresetIndex = -1;         // -1 = custom/no preset, 0-31 = active preset
  char dspPresetNames[DSP_PRESET_MAX_SLOTS][21] = {};    // 20 char max + null

  void markDspConfigDirty() {
    _dspConfigDirty = true;
    if (dspPresetIndex >= 0) { dspPresetIndex = -1; _dspPresetDirty = true; }
  }
  bool isDspConfigDirty() const { return _dspConfigDirty; }
  void clearDspConfigDirty() { _dspConfigDirty = false; }

  void markDspMetricsDirty() { _dspMetricsDirty = true; }
  bool isDspMetricsDirty() const { return _dspMetricsDirty; }
  void clearDspMetricsDirty() { _dspMetricsDirty = false; }

  void markDspPresetDirty() { _dspPresetDirty = true; }
  bool isDspPresetDirty() const { return _dspPresetDirty; }
  void clearDspPresetDirty() { _dspPresetDirty = false; }

  // DSP config swap diagnostics
  uint32_t dspSwapFailures = 0;
  uint32_t dspSwapSuccesses = 0;
  unsigned long lastDspSwapFailure = 0;

  // NOTE: Delay alignment removed in v1.8.3 - incomplete feature, never functional

  // MQTT state tracking for DSP
  bool prevMqttDspEnabled = false;
  bool prevMqttDspBypass = false;
  bool prevMqttDspChBypass[DSP_MAX_CHANNELS] = {};
  int8_t prevMqttDspPresetIndex = -1;

  // NOTE: DC Block removed in v1.8.3 - use DSP highpass stage instead
#endif

  // ===== USB Audio Routing =====
  bool usbAutoPriority = false;   // Auto-route USB to DAC when streaming starts
  uint8_t dacSourceInput = 0;     // Which input routes to DAC (0=ADC1, 1=ADC2, 2=USB)

  // ===== USB Audio State =====
#ifdef USB_AUDIO_ENABLED
  bool usbAudioEnabled = false;      // USB audio enable (persisted, default off — avoids EMI when unused)
  bool usbAudioConnected = false;    // USB host connected
  bool usbAudioStreaming = false;    // Host is actively sending audio
  uint32_t usbAudioSampleRate = 48000;
  uint8_t usbAudioBitDepth = 16;
  uint8_t usbAudioChannels = 2;
  int16_t usbAudioVolume = 0;       // Host volume in 1/256 dB units (-32768 to 0)
  bool usbAudioMute = false;        // Host mute state
  uint32_t usbAudioBufferUnderruns = 0;
  uint32_t usbAudioBufferOverruns = 0;

  void markUsbAudioDirty() { _usbAudioDirty = true; }
  bool isUsbAudioDirty() const { return _usbAudioDirty; }
  void clearUsbAudioDirty() { _usbAudioDirty = false; }
#endif

  // ===== DAC Output State =====
#ifdef DAC_ENABLED
  bool dacEnabled = false;          // Master DAC enable
  uint8_t dacVolume = 80;           // 0-100 percent
  bool dacMute = false;             // Mute output
  uint16_t dacDeviceId = 0x0001;    // DAC_ID_PCM5102A default
  char dacModelName[33] = "PCM5102A";
  uint8_t dacOutputChannels = 2;    // From driver capabilities
  bool dacDetected = false;         // EEPROM or manual selection made
  bool dacReady = false;            // Driver init + I2S TX active
  uint8_t dacFilterMode = 0;        // Digital filter mode (DAC-specific)
  uint32_t dacTxUnderruns = 0;      // TX DMA full count

  void markDacDirty() { _dacDirty = true; }
  bool isDacDirty() const { return _dacDirty; }
  void clearDacDirty() { _dacDirty = false; }

  // ===== EEPROM Diagnostics =====
  struct EepromDiag {
    bool scanned = false;           // Has a scan been performed?
    bool found = false;             // Was a valid ALXD EEPROM found?
    uint8_t eepromAddr = 0;         // I2C address where EEPROM was found
    uint8_t i2cDevicesMask = 0;     // Bitmask of 0x50-0x57 that ACK'd
    int i2cTotalDevices = 0;        // Total I2C devices found on bus
    uint32_t readErrors = 0;
    uint32_t writeErrors = 0;
    unsigned long lastScanMs = 0;
    // Parsed EEPROM fields (duplicated for WS/GUI access without re-reading)
    uint16_t deviceId = 0;
    uint8_t hwRevision = 0;
    char deviceName[33] = {};
    char manufacturer[33] = {};
    uint8_t maxChannels = 0;
    uint8_t dacI2cAddress = 0;
    uint8_t flags = 0;
    uint8_t numSampleRates = 0;
    uint32_t sampleRates[4] = {};
  };
  EepromDiag eepromDiag;

  void markEepromDirty() { _eepromDirty = true; }
  bool isEepromDirty() const { return _eepromDirty; }
  void clearEepromDirty() { _eepromDirty = false; }

  // MQTT state tracking for DAC
  bool prevMqttDacEnabled = false;
  uint8_t prevMqttDacVolume = 80;
  bool prevMqttDacMute = false;
#endif

  // MQTT state tracking for boot animation
#ifdef GUI_ENABLED
  bool prevMqttBootAnimEnabled = true;
  int prevMqttBootAnimStyle = 0;
#endif

  // ===== GUI State =====
#ifdef GUI_ENABLED
  bool guiDarkMode = false;            // GUI dark mode (separate from web darkMode)
  bool bootAnimEnabled = true;         // Enable/disable boot animation
  int bootAnimStyle = 0;               // 0-5 animation style index
#endif

  // ===== Error State =====
  int errorCode = 0;
  char errorMessage[64] = {};

  void setError(int code, const char *message);
  void clearError();
  bool hasError() const { return errorCode != 0; }

  // ===== WiFi Roaming State =====
  uint8_t roamCheckCount = 0;           // 0-3, reset on non-roam disconnect
  unsigned long lastRoamCheckTime = 0;  // millis() of last roam check
  bool roamingInProgress = false;       // True during self-triggered roam

  // ===== Reconnection Backoff =====
  unsigned long wifiBackoffDelay = 1000;
  unsigned long mqttBackoffDelay = 5000;
  static const unsigned long MAX_BACKOFF_DELAY = 60000;

  void increaseWiFiBackoff();
  void increaseMqttBackoff();
  void resetWiFiBackoff() { wifiBackoffDelay = 1000; }
  void resetMqttBackoff() { mqttBackoffDelay = 5000; }

  // ===== Utility Methods =====
  void clearAllDirtyFlags();
  bool hasAnyDirtyFlag() const;

private:
  AppState() {
    setCharField(apPassword, sizeof(apPassword), DEFAULT_AP_PASSWORD);
    setCharField(webPassword, sizeof(webPassword), DEFAULT_AP_PASSWORD);
  }

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
  bool _adcEnabledDirty = false;
  bool _sigGenDirty = false;
  bool _otaDirty = false;
#ifdef DSP_ENABLED
  bool _dspConfigDirty = false;
  bool _dspMetricsDirty = false;
  bool _dspPresetDirty = false;
  bool _emergencyLimiterDirty = false;
  bool _audioQualityDirty = false;
#endif
#ifdef USB_AUDIO_ENABLED
  bool _usbAudioDirty = false;
#endif
#ifdef DAC_ENABLED
  bool _dacDirty = false;
  bool _eepromDirty = false;
#endif
};

// Convenience macro for accessing AppState
#define appState AppState::getInstance()

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
