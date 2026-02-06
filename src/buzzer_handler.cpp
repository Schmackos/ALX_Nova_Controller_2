#include "buzzer_handler.h"
#include "app_state.h"
#include "config.h"
#include <Arduino.h>

// ===== ISR-safe volatile flags =====
volatile bool _buzzer_tick_pending = false;
volatile bool _buzzer_click_pending = false;

// ===== Tone step definition =====
struct ToneStep {
  uint16_t freq_hz;      // 0 = silence gap
  uint16_t duration_ms;  // 0 = terminator
};

// ===== Pattern definitions (terminated by {0,0}) =====
static const ToneStep pat_tick[] = {
  {4000, 8},
  {0, 0}
};

static const ToneStep pat_click[] = {
  {2000, 30},
  {0, 0}
};

static const ToneStep pat_confirm[] = {
  {2000, 60},
  {3000, 80},
  {0, 0}
};

static const ToneStep pat_btn_short[] = {
  {1500, 100},
  {0, 0}
};

static const ToneStep pat_btn_long[] = {
  {2000, 100},
  {1500, 100},
  {1000, 100},
  {0, 0}
};

static const ToneStep pat_btn_very_long[] = {
  {1000, 100},
  {2000, 100},
  {1000, 100},
  {2000, 100},
  {1000, 100},
  {2000, 100},
  {0, 0}
};

static const ToneStep pat_btn_double[] = {
  {2000, 40},
  {0, 60},     // silence gap
  {2000, 40},
  {0, 0}
};

static const ToneStep pat_btn_triple[] = {
  {2000, 40},
  {0, 50},
  {2000, 40},
  {0, 50},
  {2000, 40},
  {0, 0}
};

static const ToneStep pat_nav[] = {
  {3000, 10},
  {0, 0}
};

// Startup melody: ascending chime (C5 → E5 → G5 → C6 → E6)
static const ToneStep pat_startup[] = {
  {523, 120},    // C5
  {0, 40},
  {659, 120},    // E5
  {0, 40},
  {784, 120},    // G5
  {0, 40},
  {1047, 150},   // C6
  {0, 50},
  {1319, 300},   // E6 (held longer for resolution)
  {0, 0}
};

// ===== Pattern lookup =====
static const ToneStep *get_pattern(BuzzerPattern p) {
  switch (p) {
    case BUZZ_TICK:           return pat_tick;
    case BUZZ_CLICK:          return pat_click;
    case BUZZ_CONFIRM:        return pat_confirm;
    case BUZZ_BTN_SHORT:      return pat_btn_short;
    case BUZZ_BTN_LONG:       return pat_btn_long;
    case BUZZ_BTN_VERY_LONG:  return pat_btn_very_long;
    case BUZZ_BTN_DOUBLE:     return pat_btn_double;
    case BUZZ_BTN_TRIPLE:     return pat_btn_triple;
    case BUZZ_NAV:            return pat_nav;
    case BUZZ_STARTUP:        return pat_startup;
    default:                  return nullptr;
  }
}

// ===== Volume duty lookup: Low=10%, Medium=30%, High=60% of 255 =====
static const uint8_t volume_duty[] = {25, 76, 153};

// ===== Sequencer state =====
static const ToneStep *current_pattern = nullptr;
static int current_step = 0;
static unsigned long step_start_ms = 0;
static bool playing = false;

// Pending pattern request (from non-ISR context)
static volatile BuzzerPattern pending_pattern = BUZZ_NONE;

void buzzer_init() {
  ledcSetup(BUZZER_PWM_CHANNEL, 2000, BUZZER_PWM_RESOLUTION);
  ledcAttachPin(BUZZER_PIN, BUZZER_PWM_CHANNEL);
  ledcWrite(BUZZER_PWM_CHANNEL, 0);  // Start silent
}

void buzzer_play(BuzzerPattern pattern) {
  pending_pattern = pattern;
}

static void start_pattern(const ToneStep *pat) {
  current_pattern = pat;
  current_step = 0;
  step_start_ms = millis();
  playing = true;

  // Start first step
  if (pat[0].duration_ms > 0) {
    int vol = AppState::getInstance().buzzerVolume;
    if (vol < 0) vol = 0;
    if (vol > 2) vol = 2;
    if (pat[0].freq_hz > 0) {
      ledcWriteTone(BUZZER_PWM_CHANNEL, pat[0].freq_hz);
      ledcWrite(BUZZER_PWM_CHANNEL, volume_duty[vol]);
    } else {
      // Silence gap
      ledcWrite(BUZZER_PWM_CHANNEL, 0);
    }
  }
}

static void stop_buzzer() {
  ledcWrite(BUZZER_PWM_CHANNEL, 0);
  ledcWriteTone(BUZZER_PWM_CHANNEL, 0);
  playing = false;
  current_pattern = nullptr;
}

void buzzer_update() {
  // Check ISR-safe tick/click flags first
  if (_buzzer_tick_pending) {
    _buzzer_tick_pending = false;
    if (AppState::getInstance().buzzerEnabled) {
      start_pattern(pat_tick);
    }
  }
  if (_buzzer_click_pending) {
    _buzzer_click_pending = false;
    if (AppState::getInstance().buzzerEnabled) {
      start_pattern(pat_click);
    }
  }

  // Check pending pattern request from buzzer_play()
  BuzzerPattern req = pending_pattern;
  if (req != BUZZ_NONE) {
    pending_pattern = BUZZ_NONE;
    if (AppState::getInstance().buzzerEnabled) {
      const ToneStep *pat = get_pattern(req);
      if (pat) {
        start_pattern(pat);
      }
    }
  }

  // Sequence current pattern
  if (!playing || current_pattern == nullptr) return;

  unsigned long elapsed = millis() - step_start_ms;
  if (elapsed >= current_pattern[current_step].duration_ms) {
    // Advance to next step
    current_step++;
    if (current_pattern[current_step].duration_ms == 0) {
      // Pattern complete
      stop_buzzer();
      return;
    }

    // Start next step
    step_start_ms = millis();
    int vol = AppState::getInstance().buzzerVolume;
    if (vol < 0) vol = 0;
    if (vol > 2) vol = 2;
    if (current_pattern[current_step].freq_hz > 0) {
      ledcWriteTone(BUZZER_PWM_CHANNEL, current_pattern[current_step].freq_hz);
      ledcWrite(BUZZER_PWM_CHANNEL, volume_duty[vol]);
    } else {
      // Silence gap
      ledcWrite(BUZZER_PWM_CHANNEL, 0);
    }
  }
}
