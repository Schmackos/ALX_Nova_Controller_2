#ifndef APP_STATE_H
#define APP_STATE_H

#include "config.h"
#include "app_events.h"
#include "state/enums.h"
#include "state/general_state.h"
#include "state/ethernet_state.h"
#include "state/ota_state.h"
#include "state/usb_audio_state.h"
#include "state/signal_gen_state.h"
#include "state/display_state.h"
#include "state/buzzer_state.h"
#include "state/dsp_state.h"
#include "state/dac_state.h"
#include "state/audio_state.h"
#include "state/wifi_state.h"
#include "state/mqtt_state.h"
#include "state/debug_state.h"
#include "state/hal_coord_state.h"
#include "state/health_check_state.h"

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

  // ===== WiFi + AP Mode State =====
  WifiState wifi;

  // ===== General Settings =====
  GeneralState general;

  // ===== Ethernet State =====
  EthernetState ethernet;

  // ===== OTA Update State =====
  OtaState ota;

  // ===== Audio + Smart Sensing State =====
  AudioState audio;

  void setSensingMode(SensingMode mode);

  // ===== Debug + Hardware Stats =====
  DebugState debug;

  // ===== HAL Coordination State (deferred device toggles) =====
  HalCoordState halCoord;

  // ===== Health Check State =====
  HealthCheckState healthCheck;

  void markHealthCheckDirty() { _healthCheckDirty = true; app_events_signal(EVT_HEALTH); }
  bool isHealthCheckDirty() const { return _healthCheckDirty; }
  void clearHealthCheckDirty() { _healthCheckDirty = false; }

  // ===== Cross-task Coordination Flags =====
  volatile bool _mqttReconfigPending = false;  // set by HTTP handler; mqtt_task reconnects
  volatile int8_t _pendingApToggle = 0;        // 0=none, 1=enable AP, -1=disable AP; main loop executes

  // ===== MQTT State =====
  MqttState mqtt;

  // ===== Display State (accessible from all interfaces) =====
  DisplayState display;

  void setBacklightOn(bool state);
  void setScreenTimeout(unsigned long timeout);
  void setBacklightBrightness(uint8_t brightness);
  void setDimEnabled(bool enabled);
  void setDimTimeout(unsigned long timeout);
  void setDimBrightness(uint8_t brightness);
  bool isDisplayDirty() const { return _displayDirty; }
  void clearDisplayDirty() { _displayDirty = false; }

  // ===== Buzzer State (accessible from all interfaces) =====
  BuzzerState buzzer;

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
  SignalGenState sigGen;

  void setSignalGenEnabled(bool enabled);
  void markSignalGenDirty() { _sigGenDirty = true; app_events_signal(EVT_SIGGEN); }
  bool isSignalGenDirty() const { return _sigGenDirty; }
  void clearSignalGenDirty() { _sigGenDirty = false; }

  // ===== DSP Pipeline State =====
#ifdef DSP_ENABLED
  DspSettingsState dsp;

  void markDspConfigDirty() {
    _dspConfigDirty = true;
    if (dsp.presetIndex >= 0) { dsp.presetIndex = -1; }
    app_events_signal(EVT_DSP_CONFIG);
  }
  bool isDspConfigDirty() const { return _dspConfigDirty; }
  void clearDspConfigDirty() { _dspConfigDirty = false; }
#endif

  // ===== USB Audio State =====
#ifdef USB_AUDIO_ENABLED
  UsbAudioState usbAudio{};  // Aggregate-init (all fields have in-class defaults)

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
  DacState dac;

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

  // ===== Heap Pressure Dirty Flag =====
  void markHeapDirty() { _heapDirty = true; app_events_signal(EVT_HEAP_PRESSURE); }
  bool isHeapDirty() const { return _heapDirty; }
  void clearHeapDirty() { _heapDirty = false; }

  // ===== Diagnostic Journal Dirty Flag =====
  void markDiagJournalDirty() { _diagJournalDirty = true; app_events_signal(EVT_DIAG); }
  bool isDiagJournalDirty() const { return _diagJournalDirty; }
  void clearDiagJournalDirty() { _diagJournalDirty = false; }

  // ===== Diagnostic Config =====
  uint8_t mqttErrorThreshold = 2;  // 0=all, 1=warn+, 2=error+ (default), 3=crit only
  bool halSafeMode = false;        // Boot loop safe mode — skip HAL init, WiFi+web only

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
  bool pipelineInputBypass[AUDIO_PIPELINE_MAX_INPUTS] = {false, true, false, true, true, true, true, true};  // ADC1+SigGen active; ADC2/USB/rest bypassed
  bool pipelineDspBypass[AUDIO_PIPELINE_MAX_INPUTS]   = {false, false, true, true, true, true, true, true};  // ADC1+ADC2 use DSP; rest bypass DSP
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
  bool _eepromDirty = false;
  bool _halDeviceDirty = false;
  bool _channelMapDirty = false;
#endif
  bool _diagJournalDirty = false;
  bool _heapDirty = false;
  bool _healthCheckDirty = false;
};

// Convenience macro for accessing AppState
#define appState AppState::getInstance()

#endif // APP_STATE_H
