#include "app_state.h"

// ===== FSM State Management =====
void AppState::setFSMState(AppFSMState newState) {
  if (fsmState != newState) {
    fsmState = newState;
    _fsmStateDirty = true;
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
}

bool AppState::hasAnyDirtyFlag() const {
  bool hasDirty = _fsmStateDirty ||
         _amplifierDirty || _sensingModeDirty || _timerDirty ||
         _audioDirty || _displayDirty || _buzzerDirty || _settingsDirty ||
         _adcEnabledDirty || _sigGenDirty || _otaDirty;
  return hasDirty;
}
