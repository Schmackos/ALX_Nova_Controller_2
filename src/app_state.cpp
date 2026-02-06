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

void AppState::setVoltageReading(float reading) {
  // Use a small threshold to avoid float comparison issues
  if (abs(lastVoltageReading - reading) > 0.01) {
    lastVoltageReading = reading;
    _voltageDirty = true;
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

// ===== Error State Management =====
void AppState::setError(int code, const String &message) {
  errorCode = code;
  errorMessage = message;
  setFSMState(STATE_ERROR);
}

void AppState::clearError() {
  errorCode = 0;
  errorMessage = "";
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
  _voltageDirty = false;
  _displayDirty = false;
  _buzzerDirty = false;
  _settingsDirty = false;
}

bool AppState::hasAnyDirtyFlag() const {
  return _fsmStateDirty || _ledStateDirty || _blinkingDirty ||
         _amplifierDirty || _sensingModeDirty || _timerDirty ||
         _voltageDirty || _displayDirty || _buzzerDirty || _settingsDirty;
}
