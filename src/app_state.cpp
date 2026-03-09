#include "app_state.h"
#include "app_events.h"

// ===== FSM State Management =====
void AppState::setFSMState(AppFSMState newState) {
  if (fsmState != newState) {
    fsmState = newState;
  }
}

// ===== Smart Sensing State Management =====
void AppState::setSensingMode(SensingMode mode) {
  if (audio.currentMode != mode) {
    audio.currentMode = mode;
  }
}

// ===== Display State Management =====
void AppState::setBacklightOn(bool state) {
  if (display.backlightOn != state) {
    display.backlightOn = state;
    _displayDirty = true;
    app_events_signal(EVT_DISPLAY);
  }
}

void AppState::setScreenTimeout(unsigned long timeout) {
  if (display.screenTimeout != timeout) {
    display.screenTimeout = timeout;
    _displayDirty = true;
    app_events_signal(EVT_DISPLAY);
  }
}

void AppState::setBacklightBrightness(uint8_t brightness) {
  if (brightness < 1) brightness = 1;
  if (display.backlightBrightness != brightness) {
    display.backlightBrightness = brightness;
    _displayDirty = true;
    app_events_signal(EVT_DISPLAY);
  }
}

void AppState::setDimEnabled(bool enabled) {
  if (display.dimEnabled != enabled) {
    display.dimEnabled = enabled;
    _displayDirty = true;
    app_events_signal(EVT_DISPLAY);
  }
}

void AppState::setDimTimeout(unsigned long timeout) {
  if (display.dimTimeout != timeout) {
    display.dimTimeout = timeout;
    _displayDirty = true;
    app_events_signal(EVT_DISPLAY);
  }
}

void AppState::setDimBrightness(uint8_t brightness) {
  if (brightness < 1) brightness = 1;
  if (display.dimBrightness != brightness) {
    display.dimBrightness = brightness;
    _displayDirty = true;
    app_events_signal(EVT_DISPLAY);
  }
}

// ===== Buzzer State Management =====
void AppState::setBuzzerEnabled(bool enabled) {
  if (buzzer.enabled != enabled) {
    buzzer.enabled = enabled;
    _buzzerDirty = true;
    app_events_signal(EVT_BUZZER);
  }
}

void AppState::setBuzzerVolume(int volume) {
  if (volume < 0) volume = 0;
  if (volume > 2) volume = 2;
  if (buzzer.volume != volume) {
    buzzer.volume = volume;
    _buzzerDirty = true;
    app_events_signal(EVT_BUZZER);
  }
}

// ===== Signal Generator State Management =====
void AppState::setSignalGenEnabled(bool enabled) {
  if (sigGen.enabled != enabled) {
    sigGen.enabled = enabled;
    _sigGenDirty = true;
    app_events_signal(EVT_SIGGEN);
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
