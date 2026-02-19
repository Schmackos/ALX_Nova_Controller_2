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

// ===== Volume duty lookup: Low=10%, Medium=30%, High=60% of 255 =====
static const uint8_t volume_duty[] = {25, 76, 153};

// ===== Sequencer state =====
static const ToneStep *current_pattern = nullptr;
static int current_step = 0;
static unsigned long step_start_ms = 0;
static bool playing = false;

// ===== 3-slot circular queue for buzzer_play() requests =====
// Head = next slot to write; Tail = next slot to read.
// portMUX guards head/tail/count updates in buzzer_play() (may be called
// from any task); buzzer_update() runs from the main loop only.
static BuzzerPattern _buzzQueue[BUZZ_QUEUE_SIZE];
static int _buzzQueueHead  = 0;
static int _buzzQueueTail  = 0;
static int _buzzQueueCount = 0;
uint32_t   _buzzQueueDropped = 0;

#ifndef UNIT_TEST
static portMUX_TYPE _buzzQueueMux = portMUX_INITIALIZER_UNLOCKED;
#endif

static SemaphoreHandle_t buzzer_mutex = nullptr;

void buzzer_init() {
  // Arduino-ESP32 3.x: pin is attached at playback start via ledcAttach().
  // Keep pin LOW until the first pattern plays.
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  if (buzzer_mutex == nullptr) {
    buzzer_mutex = xSemaphoreCreateMutex();
  }
  // Reset circular queue
  _buzzQueueHead    = 0;
  _buzzQueueTail    = 0;
  _buzzQueueCount   = 0;
  _buzzQueueDropped = 0;
  LOG_I("[Buzzer] Initialized on GPIO %d", BUZZER_PIN);
}

void buzzer_play(BuzzerPattern pattern) {
  if (pattern == BUZZ_NONE) return;
  if (pattern != BUZZ_TICK && pattern != BUZZ_CLICK) {
    LOG_D("[Buzzer] Play request: %d", (int)pattern);
  }

#ifndef UNIT_TEST
  portENTER_CRITICAL(&_buzzQueueMux);
#endif
  if (_buzzQueueCount < BUZZ_QUEUE_SIZE) {
    // Normal enqueue at head position
    _buzzQueue[_buzzQueueHead] = pattern;
    _buzzQueueHead = (_buzzQueueHead + 1) % BUZZ_QUEUE_SIZE;
    _buzzQueueCount++;
  } else {
    // Queue full — drop oldest (tail) and enqueue new at head
    _buzzQueueDropped++;
    _buzzQueueTail = (_buzzQueueTail + 1) % BUZZ_QUEUE_SIZE;  // discard oldest
    _buzzQueue[_buzzQueueHead] = pattern;
    _buzzQueueHead = (_buzzQueueHead + 1) % BUZZ_QUEUE_SIZE;
    // count stays at BUZZ_QUEUE_SIZE
  }
#ifndef UNIT_TEST
  portEXIT_CRITICAL(&_buzzQueueMux);
#endif
}

static void start_pattern(const ToneStep *pat) {
  LOG_D("[Buzzer] Start pattern: freq=%d, dur=%d", pat[0].freq_hz, pat[0].duration_ms);
  current_pattern = pat;
  current_step = 0;
  step_start_ms = millis();
  playing = true;

  // Attach pin to LEDC for playback (Arduino 3.x: ledcAttach replaces ledcSetup+ledcAttachPin)
  uint32_t initFreq = (pat[0].freq_hz > 0) ? (uint32_t)pat[0].freq_hz : 2000;
  ledcAttach(BUZZER_PIN, initFreq, BUZZER_PWM_RESOLUTION);

  // Start first step
  if (pat[0].duration_ms > 0) {
    int vol = AppState::getInstance().buzzerVolume;
    if (vol < 0) vol = 0;
    if (vol > 2) vol = 2;
    if (pat[0].freq_hz > 0) {
      ledcWriteTone(BUZZER_PIN, pat[0].freq_hz);
      ledcWrite(BUZZER_PIN, volume_duty[vol]);
    } else {
      // Silence gap
      ledcWrite(BUZZER_PIN, 0);
    }
  }
}

static void stop_buzzer() {
  LOG_D("[Buzzer] Pattern complete");
  ledcWrite(BUZZER_PIN, 0);
  ledcWriteTone(BUZZER_PIN, 0);
  // Detach pin from LEDC and drive LOW to eliminate residual PWM noise
  ledcDetach(BUZZER_PIN);
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
          ledcWriteTone(BUZZER_PIN, current_pattern[current_step].freq_hz);
          ledcWrite(BUZZER_PIN, volume_duty[vol]);
        } else {
          // Silence gap
          ledcWrite(BUZZER_PIN, 0);
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

  // Dequeue next pattern from circular queue (only when not currently playing)
  if (!playing) {
#ifndef UNIT_TEST
    portENTER_CRITICAL(&_buzzQueueMux);
#endif
    BuzzerPattern req = BUZZ_NONE;
    if (_buzzQueueCount > 0) {
      req = _buzzQueue[_buzzQueueTail];
      _buzzQueueTail = (_buzzQueueTail + 1) % BUZZ_QUEUE_SIZE;
      _buzzQueueCount--;
    }
#ifndef UNIT_TEST
    portEXIT_CRITICAL(&_buzzQueueMux);
#endif
    if (req != BUZZ_NONE && AppState::getInstance().buzzerEnabled) {
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
