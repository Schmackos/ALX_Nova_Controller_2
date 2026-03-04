#include "buzzer_handler.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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
  {1500, 8},
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

// Shutdown melody: reversed startup chime (E6 → C6 → G5 → E5 → C5)
static const ToneStep pat_shutdown[] = {
  {1319, 120},   // E6
  {0, 40},
  {1047, 120},   // C6
  {0, 40},
  {784, 120},    // G5
  {0, 40},
  {659, 120},    // E5
  {0, 40},
  {523, 300},    // C5 (held longer for resolution)
  {0, 0}
};

// OTA update melody: descending D-minor alert + rising resolution
static const ToneStep pat_ota_update[] = {
  {1175, 100},   // D6 - attention
  {0, 30},
  {880, 100},    // A5
  {0, 30},
  {698, 120},    // F5 - descent bottom
  {0, 80},       // phrase gap
  {587, 100},    // D5 - resolve start
  {0, 30},
  {880, 200},    // A5 - held resolve
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
    case BUZZ_OTA_UPDATE:     return pat_ota_update;
    case BUZZ_SHUTDOWN:       return pat_shutdown;
    default:                  return nullptr;
  }
}

// ===== LEDC attachment tracking =====
static bool ledc_attached = false;  // track LEDC attachment to avoid spurious warnings

// ===== Volume scale factors (percentage of 50% duty, which is max for piezo) =====
// ledcWriteTone() sets 50% duty automatically; we scale it down for lower volumes
// Low=20%, Medium=50%, High=100% of the 50%-duty value
static const uint8_t volume_pct[] = {20, 50, 100};

// ===== Set tone frequency + volume =====
// ledcWriteTone() always uses 10-bit resolution internally (50% duty = 512).
// Do NOT use ledcRead() to get halfDuty — on P4 it returns 0, causing
// ledcWrite(0) which auto-deregisters the pin and silences the buzzer.
static void buzzer_set_tone(uint16_t freq_hz, int vol) {
  if (ledc_attached) ledcDetach(BUZZER_PIN);    // detach only if previously attached (suppresses first-call warning)
  ledcWriteTone(BUZZER_PIN, freq_hz);           // attaches fresh + sets 50% duty (10-bit = 512)
  ledc_attached = true;
  // Hardcode 10-bit half-duty instead of ledcRead() which returns 0 on P4.
  static const uint32_t LEDC_TONE_HALF_DUTY = 512u;  // 50% of 10-bit max (1023)
  uint32_t duty = LEDC_TONE_HALF_DUTY * volume_pct[vol] / 100;
  ledcWrite(BUZZER_PIN, duty);
}

// ===== Sequencer state =====
static const ToneStep *current_pattern = nullptr;
static int current_step = 0;
static unsigned long step_start_ms = 0;
static bool playing = false;

// Pending pattern request (from non-ISR context)
static volatile BuzzerPattern pending_pattern = BUZZ_NONE;

static SemaphoreHandle_t buzzer_mutex = nullptr;

void buzzer_init() {
  // Start with pin as OUTPUT LOW — attach to LEDC only during active playback
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  if (buzzer_mutex == nullptr) {
    buzzer_mutex = xSemaphoreCreateMutex();
  }
  LOG_I("[Buzzer] Initialized on GPIO %d", BUZZER_PIN);
}

void buzzer_play(BuzzerPattern pattern) {
  if (pattern != BUZZ_TICK && pattern != BUZZ_CLICK && pattern != BUZZ_NONE) {
    LOG_D("[Buzzer] Play request: %d", (int)pattern);
  }
  pending_pattern = pattern;
}

static void start_pattern(const ToneStep *pat) {
  LOG_D("[Buzzer] Start pattern: freq=%d, dur=%d", pat[0].freq_hz, pat[0].duration_ms);

  // Reset attachment state when interrupting — buzzer_set_tone() will
  // call ledcDetach() before ledcWriteTone() if ledc_attached is true.
  if (playing && ledc_attached) {
    ledc_attached = false;
  }

  current_pattern = pat;
  current_step = 0;
  step_start_ms = millis();
  playing = true;

  // Start first step — NO ledcAttach() here; buzzer_set_tone() handles attach
  // via ledcWriteTone() which picks the optimal resolution for each frequency
  if (pat[0].duration_ms > 0) {
    int vol = AppState::getInstance().buzzerVolume;
    if (vol < 0) vol = 0;
    if (vol > 2) vol = 2;
    if (pat[0].freq_hz > 0) {
      buzzer_set_tone(pat[0].freq_hz, vol);
    } else {
      // Silence gap as first step — drive pin LOW via GPIO (no LEDC needed).
      // Avoids ledcWrite(0) which auto-deregisters the pin on P4.
      if (ledc_attached) { ledcDetach(BUZZER_PIN); ledc_attached = false; }
      pinMode(BUZZER_PIN, OUTPUT);
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}

static void stop_buzzer() {
  LOG_D("[Buzzer] Pattern complete");
  // On ESP32-P4, ledcWrite(0) auto-deregisters the pin from the peripheral
  // manager, so ledcDetach() after it always fails ("pin not attached").
  // Silence via GPIO override instead — sufficient to drive the pin low.
  if (ledc_attached) {
    ledcWrite(BUZZER_PIN, 0);
    ledc_attached = false;
  }
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  playing = false;
  current_pattern = nullptr;
}

void buzzer_update() {
  if (buzzer_mutex == nullptr) return;
  if (xSemaphoreTake(buzzer_mutex, 0) != pdTRUE) return;

  // Sequence current pattern FIRST so completed patterns free up
  // before we check for new requests (prevents eating queued ticks)
  if (playing && current_pattern != nullptr) {
    unsigned long elapsed = millis() - step_start_ms;
    if (elapsed >= current_pattern[current_step].duration_ms) {
      // Advance to next step
      current_step++;
      if (current_pattern[current_step].duration_ms == 0) {
        // Pattern complete
        stop_buzzer();
      } else {
        // Start next step
        step_start_ms = millis();
        int vol = AppState::getInstance().buzzerVolume;
        if (vol < 0) vol = 0;
        if (vol > 2) vol = 2;
        if (current_pattern[current_step].freq_hz > 0) {
          buzzer_set_tone(current_pattern[current_step].freq_hz, vol);
        } else {
          // Silence gap — on P4, ledcWrite(0) auto-deregisters the pin,
          // so mark as detached so buzzer_set_tone() won't try to ledcDetach() next.
          ledcWrite(BUZZER_PIN, 0);
          ledc_attached = false;
        }
      }
    }
  }

  // Check ISR-safe tick/click flags (after sequencing, so finished patterns don't block)
  if (_buzzer_tick_pending) {
    _buzzer_tick_pending = false;
    if (AppState::getInstance().buzzerEnabled && !playing) {
      start_pattern(pat_tick);
    }
  }
  if (_buzzer_click_pending) {
    _buzzer_click_pending = false;
    if (AppState::getInstance().buzzerEnabled && !playing) {
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

  xSemaphoreGive(buzzer_mutex);
}

void buzzer_play_blocking(BuzzerPattern pattern, uint16_t timeout_ms) {
  buzzer_play(pattern);
  unsigned long start = millis();
  while (millis() - start < timeout_ms) {
    buzzer_update();
    delay(1);
  }
  buzzer_update();
}
