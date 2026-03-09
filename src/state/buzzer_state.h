#ifndef STATE_BUZZER_STATE_H
#define STATE_BUZZER_STATE_H

#include "config.h"

// Buzzer settings extracted from AppState
struct BuzzerState {
  bool enabled = true;   // Enable/disable buzzer feedback
  int volume = 1;        // 0=Low, 1=Medium, 2=High
};

#endif // STATE_BUZZER_STATE_H
