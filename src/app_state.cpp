#include "app_state.h"

// ===== FSM State Management =====
void AppState::setFSMState(AppFSMState newState) {
  if (fsmState != newState) {
    fsmState = newState;
    _fsmStateDirty = true;
  }
}

// ===== LED State Management =====
void AppState::setLedState(bool state) {
  if (ledState != state) {
    ledState = state;
    _ledStateDirty = true;
  }
}

void AppState::setBlinkingEnabled(bool enabled) {
  if (blinkingEnabled != enabled) {
    blinkingEnabled = enabled;
    _blinkingDirty = true;
  }
}

// ===== Smart Sensing State Management =====
void AppState::setAmplifierState(bool state) {
  if (amplifierState != state) {
    amplifierState = state;
    _amplifierDirty = true;
  }
}

void AppState::setSensingMode(SensingMode mode) {
  if (currentMode != mode) {
    currentMode = mode;
    _sensingModeDirty = true;
  }
}

void AppState::setTimerRemaining(unsigned long remaining) {
  if (timerRemaining != remaining) {
    timerRemaining = remaining;
    _timerDirty = true;
  }
}

void AppState::setAudioLevel(float dBFS) {
  // Use a small threshold to avoid float comparison issues
  if (abs(audioLevel_dBFS - dBFS) > 0.1f) {
    audioLevel_dBFS = dBFS;
    _audioDirty = true;
  }
}

// ===== Display State Management =====
void AppState::setBacklightOn(bool state) {
  if (backlightOn != state) {
    backlightOn = state;
    _displayDirty = true;
  }
}

void AppState::setScreenTimeout(unsigned long timeout) {
  if (screenTimeout != timeout) {
    screenTimeout = timeout;
    _displayDirty = true;
  }
}

void AppState::setBacklightBrightness(uint8_t brightness) {
  if (brightness < 1) brightness = 1;
  if (backlightBrightness != brightness) {
    backlightBrightness = brightness;
    _displayDirty = true;
  }
}

void AppState::setDimEnabled(bool enabled) {
  if (dimEnabled != enabled) {
    dimEnabled = enabled;
    _displayDirty = true;
  }
}

void AppState::setDimTimeout(unsigned long timeout) {
  if (dimTimeout != timeout) {
    dimTimeout = timeout;
    _displayDirty = true;
  }
}

void AppState::setDimBrightness(uint8_t brightness) {
  if (brightness < 1) brightness = 1;
  if (dimBrightness != brightness) {
    dimBrightness = brightness;
    _displayDirty = true;
  }
}

// ===== Buzzer State Management =====
void AppState::setBuzzerEnabled(bool enabled) {
  if (buzzerEnabled != enabled) {
    buzzerEnabled = enabled;
    _buzzerDirty = true;
  }
}

void AppState::setBuzzerVolume(int volume) {
  if (volume < 0) volume = 0;
  if (volume > 2) volume = 2;
  if (buzzerVolume != volume) {
    buzzerVolume = volume;
    _buzzerDirty = true;
  }
}

// ===== Emergency Safety Limiter State Management =====
#ifdef DSP_ENABLED
void AppState::setEmergencyLimiterEnabled(bool enabled) {
  if (emergencyLimiterEnabled != enabled) {
    emergencyLimiterEnabled = enabled;
    _emergencyLimiterDirty = true;
  }
}

void AppState::setEmergencyLimiterThreshold(float dbfs) {
  // Validate range: -6.0 to 0.0 dBFS
  if (dbfs < -6.0f) dbfs = -6.0f;
  if (dbfs > 0.0f) dbfs = 0.0f;
  // Use small threshold to avoid float comparison issues
  if (abs(emergencyLimiterThresholdDb - dbfs) > 0.01f) {
    emergencyLimiterThresholdDb = dbfs;
    _emergencyLimiterDirty = true;
  }
}

// ===== Audio Quality Diagnostics State Management (Phase 3) =====
void AppState::setAudioQualityEnabled(bool enabled) {
  if (audioQualityEnabled != enabled) {
    audioQualityEnabled = enabled;
    _audioQualityDirty = true;
  }
}

void AppState::setAudioQualityThreshold(float threshold) {
  // Validate range: 0.1 to 1.0
  if (threshold < 0.1f) threshold = 0.1f;
  if (threshold > 1.0f) threshold = 1.0f;
  // Use small threshold to avoid float comparison issues
  if (abs(audioQualityGlitchThreshold - threshold) > 0.01f) {
    audioQualityGlitchThreshold = threshold;
    _audioQualityDirty = true;
  }
}
#endif

// ===== Signal Generator State Management =====
void AppState::setSignalGenEnabled(bool enabled) {
  if (sigGenEnabled != enabled) {
    sigGenEnabled = enabled;
    _sigGenDirty = true;
  }
}

// ===== Error State Management =====
void AppState::setError(int code, const char *message) {
  errorCode = code;
  setCharField(errorMessage, sizeof(errorMessage), message);
  setFSMState(STATE_ERROR);
}

void AppState::clearError() {
  errorCode = 0;
  errorMessage[0] = '\0';
  if (fsmState == STATE_ERROR) {
    setFSMState(STATE_IDLE);
  }
}

// ===== Exponential Backoff =====
void AppState::increaseWiFiBackoff() {
  wifiBackoffDelay = min(wifiBackoffDelay * 2, MAX_BACKOFF_DELAY);
}

void AppState::increaseMqttBackoff() {
  mqttBackoffDelay = min(mqttBackoffDelay * 2, MAX_BACKOFF_DELAY);
}

// ===== Dirty Flag Utilities =====
void AppState::clearAllDirtyFlags() {
  _fsmStateDirty = false;
  _ledStateDirty = false;
  _blinkingDirty = false;
  _amplifierDirty = false;
  _sensingModeDirty = false;
  _timerDirty = false;
  _audioDirty = false;
  _displayDirty = false;
  _buzzerDirty = false;
  _settingsDirty = false;
  _adcEnabledDirty = false;
  _sigGenDirty = false;
  _otaDirty = false;
#ifdef DSP_ENABLED
  _emergencyLimiterDirty = false;
#endif
}

bool AppState::hasAnyDirtyFlag() const {
  bool hasDirty = _fsmStateDirty || _ledStateDirty || _blinkingDirty ||
         _amplifierDirty || _sensingModeDirty || _timerDirty ||
         _audioDirty || _displayDirty || _buzzerDirty || _settingsDirty ||
         _adcEnabledDirty || _sigGenDirty || _otaDirty;
#ifdef DSP_ENABLED
  hasDirty = hasDirty || _emergencyLimiterDirty;
#endif
  return hasDirty;
}
