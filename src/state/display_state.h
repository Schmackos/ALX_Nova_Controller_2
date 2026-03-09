#ifndef STATE_DISPLAY_STATE_H
#define STATE_DISPLAY_STATE_H

#include "config.h"

// Display/backlight settings extracted from AppState
struct DisplayState {
  unsigned long screenTimeout = 60000; // Screen timeout in ms (default 60s)
  bool backlightOn = true;             // Runtime backlight state (not persisted)
  uint8_t backlightBrightness = 255;   // Backlight brightness (1-255, persisted)
  bool dimEnabled = false;             // Dim feature enabled
  unsigned long dimTimeout = 10000;    // Dim timeout in ms (default 10s)
  uint8_t dimBrightness = 26;          // Dim brightness PWM (1-255, default 10%)
};

#endif // STATE_DISPLAY_STATE_H
