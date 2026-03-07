#ifndef APP_STATE_H
#define APP_STATE_H

#include "config.h"
#include "app_events.h"

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

// ===== Network Interface =====
enum NetIfType { NET_NONE, NET_ETHERNET, NET_WIFI };

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

  // ===== WiFi State =====
  String wifiSSID;
  String wifiPassword;

  // ===== Device Information =====
  String deviceSerialNumber;

  // ===== AP Mode State =====
  bool isAPMode = false;
  bool apEnabled = false;
  bool autoAPEnabled = true; // Default to true per requirements
  String apSSID;
  String apPassword = DEFAULT_AP_PASSWORD;

  // ===== Web Authentication =====
  String webPassword = DEFAULT_AP_PASSWORD;

  // ===== WiFi Security =====
  uint8_t wifiMinSecurity = 0;  // 0=any, 1=WPA2+, 2=WPA3 only

  // ===== WiFi Connection State (Async) =====
  bool wifiConnecting = false;
  bool wifiConnectSuccess = false;
  String wifiNewIP;
  String wifiConnectError;

  // ===== Ethernet State =====
  bool ethLinkUp = false;
  bool ethConnected = false;
  String ethIP = "";
  NetIfType activeInterface = NET_NONE;

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

  // ===== OTA Release Channel =====
  // 0 = stable (latest non-prerelease), 1 = beta (includes prereleases)
  uint8_t otaChannel = 0;

  // ===== Release List Cache (on-demand, populated by /api/releases) =====
  struct ReleaseInfo {
    String version;
    String firmwareUrl;
    String checksum;
    bool isPrerelease;
    String publishedAt;  // "YYYY-MM-DD"
  };
  static const int OTA_MAX_RELEASES = 5;
  ReleaseInfo cachedReleaseList[OTA_MAX_RELEASES];
  int cachedReleaseListCount = 0;

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
  AdcState audioAdc[NUM_AUDIO_ADCS];
  int numAdcsDetected = 1; // How many ADCs are currently producing data

  // ===== I2S Runtime Metrics (written by audio task, read by diagnostics) =====
  struct I2sRuntimeMetrics {
    uint32_t audioTaskStackFree = 0;           // bytes remaining (high watermark × 4)
    float buffersPerSec[NUM_AUDIO_ADCS] = {};  // actual buf/s per ADC
    float avgReadLatencyUs[NUM_AUDIO_ADCS] = {};// avg i2s_read() time in µs
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
  bool adcEnabled[NUM_AUDIO_ADCS] = {true, true}; // Per-ADC input enable (persisted)
  volatile bool audioPaused = false; // Set true to pause audio_capture_task I2S reads (for I2S reinit)
#ifndef UNIT_TEST
  SemaphoreHandle_t audioTaskPausedAck = nullptr; // Binary semaphore: audio task gives when it has seen audioPaused=true
#endif

  // Input channel names (user-configurable, 4 channels = 2 ADCs x 2 channels)
  String inputNames[NUM_AUDIO_ADCS * 2];

  void setSensingMode(SensingMode mode);

  // Smart Sensing heartbeat
  unsigned long lastSmartSensingHeartbeat = 0;

  // ===== Certificate Validation =====
  bool enableCertValidation = true;

  // ===== Audio Update Rate =====
  uint16_t audioUpdateRate = DEFAULT_AUDIO_UPDATE_RATE; // ms (33, 50, 100)

  // ===== Audio Graph Toggles =====
  bool vuMeterEnabled = true;      // Enable VU meter computation & display
  bool waveformEnabled = true;     // Enable waveform computation & display
  bool spectrumEnabled = true;     // Enable FFT/spectrum computation & display

  // ===== FFT Window Type =====
  FftWindowType fftWindowType = FFT_WINDOW_HANN; // Default to Hann

  // ===== ADC Signal Quality Metrics =====
  float audioSnrDb[NUM_AUDIO_ADCS] = {};         // Signal-to-Noise Ratio (dB)
  float audioSfdrDb[NUM_AUDIO_ADCS] = {};        // Spurious-Free Dynamic Range (dB)

  // ===== Heap Health =====
  bool heapCritical = false;       // True when largest free block < 40KB

  // ===== Debug Mode Toggles =====
  bool debugMode = true;           // Master debug gate
  int debugSerialLevel = 2;        // 0=Off, 1=Errors, 2=Info, 3=Debug
  bool debugHwStats = true;        // HW stats WS broadcast + web tab
  bool debugI2sMetrics = true;     // I2S runtime metrics in audio task
  bool debugTaskMonitor = false;   // Task monitor update & serial print (opt-in)

  // ===== Hardware Stats =====
  unsigned long hardwareStatsInterval = HARDWARE_STATS_INTERVAL;

  // ===== Cross-task Coordination Flags =====
  volatile bool _mqttReconfigPending = false;  // set by HTTP handler; mqtt_task reconnects
  volatile int8_t _pendingApToggle = 0;        // 0=none, 1=enable AP, -1=disable AP; main loop executes

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

  // ===== ADC Enabled Dirty Flag (for WS/MQTT sync) =====
  void markAdcEnabledDirty() { _adcEnabledDirty = true; app_events_signal(EVT_ADC_ENABLED); }
  bool isAdcEnabledDirty() const { return _adcEnabledDirty; }
  void clearAdcEnabledDirty() { _adcEnabledDirty = false; }

  // ===== Ethernet Dirty Flag =====
  bool isEthernetDirty() const { return _ethernetDirty; }
  void markEthernetDirty() { _ethernetDirty = true; app_events_signal(EVT_ETHERNET); }
  void clearEthernetDirty() { _ethernetDirty = false; }

  // ===== Settings Dirty Flag (for GUI -> WS/MQTT sync) =====
  bool isSettingsDirty() const { return _settingsDirty; }
  void clearSettingsDirty() { _settingsDirty = false; }
  void markSettingsDirty() { _settingsDirty = true; app_events_signal(EVT_SETTINGS); }

  // ===== OTA Dirty Flag (for OTA task -> main loop WS broadcast) =====
  bool isOTADirty() const { return _otaDirty; }
  void clearOTADirty() { _otaDirty = false; }
  void markOTADirty() { _otaDirty = true; app_events_signal(EVT_OTA); }

  // ===== Signal Generator State =====
  bool sigGenEnabled = false;           // Always boots false
  int sigGenWaveform = 0;               // 0=sine, 1=square, 2=noise, 3=sweep
  float sigGenFrequency = 1000.0f;      // 1.0 - 22000.0 Hz
  float sigGenAmplitude = -6.0f;        // -96.0 to 0.0 dBFS
  int sigGenChannel = 2;                // 0=Ch1, 1=Ch2, 2=Both
  int sigGenOutputMode = 0;             // 0=software, 1=PWM
  float sigGenSweepSpeed = 1000.0f;     // Hz per second

  void setSignalGenEnabled(bool enabled);
  void markSignalGenDirty() { _sigGenDirty = true; app_events_signal(EVT_SIGGEN); }
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
    if (dspPresetIndex >= 0) { dspPresetIndex = -1; }
    app_events_signal(EVT_DSP_CONFIG);
  }
  bool isDspConfigDirty() const { return _dspConfigDirty; }
  void clearDspConfigDirty() { _dspConfigDirty = false; }

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

  // VU metering (written by Core 1 pipeline task via markUsbAudioVuDirty)
  float usbAudioVuL = -90.0f;
  float usbAudioVuR = -90.0f;

  // Dynamically negotiated format (set by control_xfer_cb on SET_CUR)
  uint32_t usbAudioNegotiatedRate  = 48000;
  uint8_t  usbAudioNegotiatedDepth = 16;

  void markUsbAudioDirty() { _usbAudioDirty = true; app_events_signal(EVT_USB_AUDIO); }
  bool isUsbAudioDirty() const { return _usbAudioDirty; }
  void clearUsbAudioDirty() { _usbAudioDirty = false; }

  // Separate dirty flag for high-frequency VU updates (don't conflate with connection events)
  void markUsbAudioVuDirty()    { _usbAudioVuDirty = true; app_events_signal(EVT_USB_VU); }
  bool isUsbAudioVuDirty() const { return _usbAudioVuDirty; }
  void clearUsbAudioVuDirty()  { _usbAudioVuDirty = false; }
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

  void markDacDirty() { _dacDirty = true; app_events_signal(EVT_DAC); }
  bool isDacDirty() const { return _dacDirty; }
  void clearDacDirty() { _dacDirty = false; }

  // ES8311 secondary DAC (P4 onboard codec + NS4150B speaker amp)
  bool es8311Enabled = false;
  uint8_t es8311Volume = 80;     // 0-100 (hardware volume via I2C)
  bool es8311Mute = false;
  bool es8311Ready = false;
  volatile int8_t _pendingEs8311Toggle = 0;  // 0=none, 1=init, -1=deinit; main loop executes
  volatile int8_t _pendingDacToggle = 0;    // 0=none, 1=init, -1=deinit; main loop executes

  void markEs8311Dirty() { _es8311Dirty = true; app_events_signal(EVT_DAC); }
  bool isEs8311Dirty() const { return _es8311Dirty; }
  void clearEs8311Dirty() { _es8311Dirty = false; }

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

  void markEepromDirty() { _eepromDirty = true; app_events_signal(EVT_EEPROM); }
  bool isEepromDirty() const { return _eepromDirty; }
  void clearEepromDirty() { _eepromDirty = false; }

  // ===== HAL Scan State =====
  volatile bool _halScanInProgress = false;

  // ===== HAL Auto-Discovery =====
  bool halAutoDiscovery = true;   // Auto-init devices with known EEPROM/GPIO ID match

  // ===== HAL Device Dirty Flag =====
  void markHalDeviceDirty() { _halDeviceDirty = true; app_events_signal(EVT_HAL_DEVICE); }
  bool isHalDeviceDirty() const { return _halDeviceDirty; }
  void clearHalDeviceDirty() { _halDeviceDirty = false; }

  // ===== Audio Channel Map Dirty Flag =====
  void markChannelMapDirty() { _channelMapDirty = true; app_events_signal(EVT_CHANNEL_MAP); }
  bool isChannelMapDirty() const { return _channelMapDirty; }
  void clearChannelMapDirty() { _channelMapDirty = false; }

  // ===== Diagnostic Journal Dirty Flag =====
  void markDiagJournalDirty() { _diagJournalDirty = true; app_events_signal(EVT_DIAG); }
  bool isDiagJournalDirty() const { return _diagJournalDirty; }
  void clearDiagJournalDirty() { _diagJournalDirty = false; }

  // ===== Diagnostic Config =====
  uint8_t mqttErrorThreshold = 2;  // 0=all, 1=warn+, 2=error+ (default), 3=crit only

  // MQTT state tracking for DAC
  bool prevMqttDacEnabled = false;
  uint8_t prevMqttDacVolume = 80;
  bool prevMqttDacMute = false;
#endif

  // MQTT state tracking for OTA channel (not GUI-gated — MQTT works without GUI)
  uint8_t prevMqttOtaChannel = 0;

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

  // ===== Audio Pipeline Bypass Flags =====
  // Normal operation: ADC1 + Siggen active, matrix routes both to DAC.
  // ADC2 and USB bypassed until physically connected.
  bool pipelineInputBypass[4] = {false, true, false, true};  // ADC1+Siggen active; ADC2/USB bypassed
  bool pipelineDspBypass[4]   = {false, false, true, true};  // ADC1+ADC2 use DSP; Siggen+USB bypass DSP
  bool pipelineMatrixBypass   = false;   // Matrix active: routes ADC1 L/R + Siggen L/R → DAC L/R
  bool pipelineOutputBypass   = false;

  // ===== Error State =====
  int errorCode = 0;
  String errorMessage;

  void setError(int code, const String &message);
  void clearError();
  bool hasError() const { return errorCode != 0; }

  // ===== Reconnection Backoff =====
  unsigned long wifiBackoffDelay = 1000;
  unsigned long mqttBackoffDelay = 5000;
  static const unsigned long MAX_BACKOFF_DELAY = 60000;

  void increaseWiFiBackoff();
  void increaseMqttBackoff();
  void resetWiFiBackoff() { wifiBackoffDelay = 1000; }
  void resetMqttBackoff() { mqttBackoffDelay = 5000; }

private:
  AppState() {} // Private constructor

  // Dirty flags for change detection
  bool _ethernetDirty = false;
  bool _displayDirty = false;
  bool _buzzerDirty = false;
  bool _settingsDirty = false;
  bool _adcEnabledDirty = false;
  bool _sigGenDirty = false;
  bool _otaDirty = false;
#ifdef DSP_ENABLED
  bool _dspConfigDirty = false;
#endif
#ifdef USB_AUDIO_ENABLED
  bool _usbAudioDirty = false;
  bool _usbAudioVuDirty = false;
#endif
#ifdef DAC_ENABLED
  bool _dacDirty = false;
  bool _eepromDirty = false;
  bool _es8311Dirty = false;
  bool _halDeviceDirty = false;
  bool _channelMapDirty = false;
#endif
  bool _diagJournalDirty = false;
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
