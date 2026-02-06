#ifndef BUZZER_HANDLER_H
#define BUZZER_HANDLER_H

#include <stdint.h>

// ===== Buzzer Sound Patterns =====
enum BuzzerPattern {
  BUZZ_NONE,
  BUZZ_TICK,           // Encoder rotation tick
  BUZZ_CLICK,          // Encoder button press
  BUZZ_CONFIRM,        // Menu value confirm
  BUZZ_BTN_SHORT,      // Reset button short press
  BUZZ_BTN_LONG,       // Reset button long press
  BUZZ_BTN_VERY_LONG,  // Reset button very long press
  BUZZ_BTN_DOUBLE,     // Reset button double click
  BUZZ_BTN_TRIPLE,     // Reset button triple click
  BUZZ_NAV,            // Screen navigation transition
  BUZZ_STARTUP,        // Boot startup melody
};

// ===== Public API =====
void buzzer_init();
void buzzer_play(BuzzerPattern pattern);
void buzzer_update();  // Call from main loop — non-blocking sequencer

// ===== ISR-safe request functions (inlined volatile writes) =====
// These are safe to call from any context including IRAM_ATTR ISRs.
extern volatile bool _buzzer_tick_pending;
extern volatile bool _buzzer_click_pending;

inline void buzzer_request_tick()  { _buzzer_tick_pending = true; }
inline void buzzer_request_click() { _buzzer_click_pending = true; }

#endif // BUZZER_HANDLER_H
