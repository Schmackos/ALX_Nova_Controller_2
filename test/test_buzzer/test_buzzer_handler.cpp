#include <cstring>
#include <string>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Minimal inline stubs for buzzer_handler dependencies =====
// We re-implement the buzzer logic inline for unit testing without
// pulling in the full AppState singleton or ESP32 LEDC hardware.

// Simulated AppState fields
static bool test_buzzerEnabled = true;
static int test_buzzerVolume = 1;

// ===== Buzzer types (mirrored from buzzer_handler.h) =====
enum BuzzerPattern {
  BUZZ_NONE,
  BUZZ_TICK,
  BUZZ_CLICK,
  BUZZ_CONFIRM,
  BUZZ_BTN_SHORT,
  BUZZ_BTN_LONG,
  BUZZ_BTN_VERY_LONG,
  BUZZ_BTN_DOUBLE,
  BUZZ_BTN_TRIPLE,
  BUZZ_NAV,
  BUZZ_STARTUP,
  BUZZ_OTA_UPDATE,
};

struct ToneStep {
  uint16_t freq_hz;
  uint16_t duration_ms;
};

// Pattern definitions (same as buzzer_handler.cpp)
static const ToneStep pat_tick[] = {{1500, 8}, {0, 20}, {0, 0}};
static const ToneStep pat_click[] = {{2000, 30}, {0, 0}};
static const ToneStep pat_confirm[] = {{2000, 60}, {3000, 80}, {0, 0}};
static const ToneStep pat_btn_short[] = {{1500, 100}, {0, 0}};
static const ToneStep pat_btn_long[] = {
    {2000, 100}, {1500, 100}, {1000, 100}, {0, 0}};
static const ToneStep pat_btn_double[] = {
    {2000, 40}, {0, 60}, {2000, 40}, {0, 0}};
static const ToneStep pat_btn_triple[] = {
    {2000, 40}, {0, 50}, {2000, 40}, {0, 50}, {2000, 40}, {0, 0}};
static const ToneStep pat_nav[] = {{3000, 10}, {0, 0}};
static const ToneStep pat_startup[] = {
    {523, 120}, {0, 40}, {659, 120}, {0, 40}, {784, 120},
    {0, 40}, {1047, 150}, {0, 50}, {1319, 300}, {0, 0}};
static const ToneStep pat_btn_very_long[] = {
    {1000, 100}, {2000, 100}, {1000, 100},
    {2000, 100}, {1000, 100}, {2000, 100},
    {0, 0}};
static const ToneStep pat_ota_update[] = {
    {1175, 100}, {0, 30}, {880, 100}, {0, 30}, {698, 120},
    {0, 80}, {587, 100}, {0, 30}, {880, 200}, {0, 0}};

static const ToneStep *get_pattern(BuzzerPattern p) {
  switch (p) {
  case BUZZ_TICK:
    return pat_tick;
  case BUZZ_CLICK:
    return pat_click;
  case BUZZ_CONFIRM:
    return pat_confirm;
  case BUZZ_BTN_SHORT:
    return pat_btn_short;
  case BUZZ_BTN_LONG:
    return pat_btn_long;
  case BUZZ_BTN_VERY_LONG:
    return pat_btn_very_long;
  case BUZZ_BTN_DOUBLE:
    return pat_btn_double;
  case BUZZ_BTN_TRIPLE:
    return pat_btn_triple;
  case BUZZ_NAV:
    return pat_nav;
  case BUZZ_STARTUP:
    return pat_startup;
  case BUZZ_OTA_UPDATE:
    return pat_ota_update;
  default:
    return nullptr;
  }
}

static const uint8_t volume_duty[] = {25, 76, 153};

// ===== 3-slot circular queue =====
#define BUZZ_QUEUE_SIZE 3

static BuzzerPattern _buzzQueue[BUZZ_QUEUE_SIZE];
static int _buzzQueueHead  = 0;
static int _buzzQueueTail  = 0;
static int _buzzQueueCount = 0;
static uint32_t _buzzQueueDropped = 0;

// Sequencer state
static const ToneStep *current_pattern = nullptr;
static int current_step = 0;
static unsigned long step_start_ms = 0;
static bool playing = false;
static volatile bool _buzzer_tick_pending = false;
static volatile bool _buzzer_click_pending = false;

static void start_pattern(const ToneStep *pat) {
  current_pattern = pat;
  current_step = 0;
  step_start_ms = millis();
  playing = true;

  if (pat[0].duration_ms > 0) {
    int vol = test_buzzerVolume;
    if (vol < 0)
      vol = 0;
    if (vol > 2)
      vol = 2;
    if (pat[0].freq_hz > 0) {
      ledcWriteTone(1, pat[0].freq_hz);
      ledcWrite(1, volume_duty[vol]);
    } else {
      ledcWrite(1, 0);
    }
  }
}

static void stop_buzzer() {
  ledcWrite(1, 0);
  ledcWriteTone(1, 0);
  playing = false;
  current_pattern = nullptr;
}

// Enqueue a pattern; if full, drop oldest and enqueue new one
static void buzzer_play(BuzzerPattern pattern) {
  if (pattern == BUZZ_NONE) return;
  if (_buzzQueueCount < BUZZ_QUEUE_SIZE) {
    _buzzQueue[_buzzQueueHead] = pattern;
    _buzzQueueHead = (_buzzQueueHead + 1) % BUZZ_QUEUE_SIZE;
    _buzzQueueCount++;
  } else {
    // Queue full — drop oldest
    _buzzQueueDropped++;
    _buzzQueueTail = (_buzzQueueTail + 1) % BUZZ_QUEUE_SIZE;
    _buzzQueue[_buzzQueueHead] = pattern;
    _buzzQueueHead = (_buzzQueueHead + 1) % BUZZ_QUEUE_SIZE;
    // count stays at BUZZ_QUEUE_SIZE
  }
}

static void buzzer_update() {
  // Handle ISR-safe tick/click flags (bypass queue — direct play when idle)
  if (_buzzer_tick_pending) {
    _buzzer_tick_pending = false;
    if (test_buzzerEnabled && !playing) {
      start_pattern(pat_tick);
    }
  }
  if (_buzzer_click_pending) {
    _buzzer_click_pending = false;
    if (test_buzzerEnabled && !playing) {
      start_pattern(pat_click);
    }
  }

  // Dequeue next pattern only when not currently playing
  if (!playing) {
    BuzzerPattern req = BUZZ_NONE;
    if (_buzzQueueCount > 0) {
      req = _buzzQueue[_buzzQueueTail];
      _buzzQueueTail = (_buzzQueueTail + 1) % BUZZ_QUEUE_SIZE;
      _buzzQueueCount--;
    }
    if (req != BUZZ_NONE && test_buzzerEnabled) {
      const ToneStep *pat = get_pattern(req);
      if (pat) {
        start_pattern(pat);
      }
    }
  }

  if (!playing || current_pattern == nullptr)
    return;

  unsigned long elapsed = millis() - step_start_ms;
  if (elapsed >= current_pattern[current_step].duration_ms) {
    current_step++;
    if (current_pattern[current_step].duration_ms == 0) {
      stop_buzzer();
      return;
    }
    step_start_ms = millis();
    int vol = test_buzzerVolume;
    if (vol < 0)
      vol = 0;
    if (vol > 2)
      vol = 2;
    if (current_pattern[current_step].freq_hz > 0) {
      ledcWriteTone(1, current_pattern[current_step].freq_hz);
      ledcWrite(1, volume_duty[vol]);
    } else {
      ledcWrite(1, 0);
    }
  }
}

static void buzzer_play_blocking(BuzzerPattern pattern, uint16_t timeout_ms) {
  buzzer_play(pattern);
  unsigned long start = millis();
  while (millis() - start < timeout_ms) {
    buzzer_update();
    ArduinoMock::mockMillis++;
  }
  buzzer_update();
}

// ===== Test setUp/tearDown =====

void setUp(void) {
  ArduinoMock::reset();
  ArduinoMock::resetLedc();
  test_buzzerEnabled = true;
  test_buzzerVolume = 1;
  current_pattern = nullptr;
  current_step = 0;
  step_start_ms = 0;
  playing = false;
  _buzzer_tick_pending = false;
  _buzzer_click_pending = false;
  // Reset circular queue
  _buzzQueueHead    = 0;
  _buzzQueueTail    = 0;
  _buzzQueueCount   = 0;
  _buzzQueueDropped = 0;
  for (int i = 0; i < BUZZ_QUEUE_SIZE; i++) {
    _buzzQueue[i] = BUZZ_NONE;
  }
}

void tearDown(void) {}

// ===== Test Cases =====

// Test 1: Buzzer disabled skips all sounds
void test_buzzer_disabled_skips_all(void) {
  test_buzzerEnabled = false;
  buzzer_play(BUZZ_CLICK);
  buzzer_update();

  TEST_ASSERT_FALSE(playing);
  TEST_ASSERT_EQUAL(0, ArduinoMock::ledcWriteToneCount);
}

// Test 2: Buzzer enabled plays a pattern
void test_buzzer_enabled_plays_pattern(void) {
  test_buzzerEnabled = true;
  buzzer_play(BUZZ_CLICK);
  buzzer_update();

  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(1, ArduinoMock::ledcWriteToneCount);
  TEST_ASSERT_EQUAL(2000, (int)ArduinoMock::ledcLastFreq);
}

// Test 3: Volume levels map to correct PWM duty cycles
void test_volume_duty_low(void) {
  test_buzzerVolume = 0;
  buzzer_play(BUZZ_CLICK);
  buzzer_update();

  TEST_ASSERT_EQUAL(25, ArduinoMock::ledcLastDuty);
}

void test_volume_duty_medium(void) {
  test_buzzerVolume = 1;
  buzzer_play(BUZZ_CLICK);
  buzzer_update();

  TEST_ASSERT_EQUAL(76, ArduinoMock::ledcLastDuty);
}

void test_volume_duty_high(void) {
  test_buzzerVolume = 2;
  buzzer_play(BUZZ_CLICK);
  buzzer_update();

  TEST_ASSERT_EQUAL(153, ArduinoMock::ledcLastDuty);
}

// Test 4: ISR-safe functions set volatile flags
void test_isr_tick_flag(void) {
  _buzzer_tick_pending = true;
  buzzer_update();

  TEST_ASSERT_FALSE(_buzzer_tick_pending);
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(1500, (int)ArduinoMock::ledcLastFreq);
}

void test_isr_click_flag(void) {
  _buzzer_click_pending = true;
  buzzer_update();

  TEST_ASSERT_FALSE(_buzzer_click_pending);
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(2000, (int)ArduinoMock::ledcLastFreq);
}

// Test 5: Pattern sequencing advances correctly
void test_pattern_sequencing_confirm(void) {
  buzzer_play(BUZZ_CONFIRM);
  buzzer_update();

  // First step: 2000 Hz
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(2000, (int)ArduinoMock::ledcLastFreq);

  // Advance time past first step (60ms)
  ArduinoMock::mockMillis = 60;
  buzzer_update();

  // Second step: 3000 Hz
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(3000, (int)ArduinoMock::ledcLastFreq);

  // Advance time past second step (60 + 80 = 140ms)
  ArduinoMock::mockMillis = 140;
  buzzer_update();

  // Pattern should be done
  TEST_ASSERT_FALSE(playing);
}

// Test 6: Second pattern enqueued while playing starts after current finishes
// (Queue semantics: no override — patterns are FIFO)
void test_enqueued_pattern_plays_after_current(void) {
  buzzer_play(BUZZ_BTN_SHORT); // 1-step pattern: 1500 Hz, 100ms
  buzzer_update();
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(1500, (int)ArduinoMock::ledcLastFreq);

  // Enqueue a second pattern while first is playing
  buzzer_play(BUZZ_CLICK); // 1-step: 2000 Hz, 30ms
  // update while still playing (time not advanced) — dequeue should NOT happen
  buzzer_update();
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(1500, (int)ArduinoMock::ledcLastFreq); // still first pattern

  // Advance past first pattern (100ms)
  ArduinoMock::mockMillis = 100;
  buzzer_update(); // stops first pattern

  // Now idle — next update dequeues BUZZ_CLICK
  buzzer_update();
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(2000, (int)ArduinoMock::ledcLastFreq);
}

// Test 7: Silence gaps produce zero duty
void test_silence_gap_zero_duty(void) {
  buzzer_play(BUZZ_BTN_DOUBLE); // {2000,40},{0,60},{2000,40}
  buzzer_update();

  // First step: tone
  TEST_ASSERT_EQUAL(2000, (int)ArduinoMock::ledcLastFreq);
  TEST_ASSERT_EQUAL(76, ArduinoMock::ledcLastDuty); // Medium volume

  // Advance past first step (40ms)
  ArduinoMock::mockMillis = 40;
  buzzer_update();

  // Second step: silence gap — duty should be 0
  TEST_ASSERT_EQUAL(0u, ArduinoMock::ledcLastDuty);
}

// Test 8: OTA update pattern plays first tone (D6 = 1175 Hz)
void test_ota_update_pattern_plays(void) {
  buzzer_play(BUZZ_OTA_UPDATE);
  buzzer_update();

  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(1175, (int)ArduinoMock::ledcLastFreq);
}

// Test 9: OTA update pattern walks through all 9 steps correctly
void test_ota_update_pattern_sequencing(void) {
  buzzer_play(BUZZ_OTA_UPDATE);
  buzzer_update();

  // Step 0: D6 (1175 Hz, 100ms)
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(1175, (int)ArduinoMock::ledcLastFreq);

  // Step 1: silence (30ms)
  ArduinoMock::mockMillis = 100;
  buzzer_update();
  TEST_ASSERT_EQUAL(0u, ArduinoMock::ledcLastDuty);

  // Step 2: A5 (880 Hz, 100ms)
  ArduinoMock::mockMillis = 130;
  buzzer_update();
  TEST_ASSERT_EQUAL(880, (int)ArduinoMock::ledcLastFreq);

  // Step 3: silence (30ms)
  ArduinoMock::mockMillis = 230;
  buzzer_update();
  TEST_ASSERT_EQUAL(0u, ArduinoMock::ledcLastDuty);

  // Step 4: F5 (698 Hz, 120ms)
  ArduinoMock::mockMillis = 260;
  buzzer_update();
  TEST_ASSERT_EQUAL(698, (int)ArduinoMock::ledcLastFreq);

  // Step 5: silence (80ms)
  ArduinoMock::mockMillis = 380;
  buzzer_update();
  TEST_ASSERT_EQUAL(0u, ArduinoMock::ledcLastDuty);

  // Step 6: D5 (587 Hz, 100ms)
  ArduinoMock::mockMillis = 460;
  buzzer_update();
  TEST_ASSERT_EQUAL(587, (int)ArduinoMock::ledcLastFreq);

  // Step 7: silence (30ms)
  ArduinoMock::mockMillis = 560;
  buzzer_update();
  TEST_ASSERT_EQUAL(0u, ArduinoMock::ledcLastDuty);

  // Step 8: A5 (880 Hz, 200ms)
  ArduinoMock::mockMillis = 590;
  buzzer_update();
  TEST_ASSERT_EQUAL(880, (int)ArduinoMock::ledcLastFreq);

  // Pattern complete
  ArduinoMock::mockMillis = 790;
  buzzer_update();
  TEST_ASSERT_FALSE(playing);
}

// Test 10: OTA update with buzzer disabled silently skips
void test_ota_update_disabled_skips(void) {
  test_buzzerEnabled = false;
  buzzer_play(BUZZ_OTA_UPDATE);
  buzzer_update();

  TEST_ASSERT_FALSE(playing);
  TEST_ASSERT_EQUAL(0, ArduinoMock::ledcWriteToneCount);
}

// Test 11: Blocking playback completes within timeout
void test_blocking_playback_completes(void) {
  buzzer_play_blocking(BUZZ_CLICK, 100);

  // After blocking call, mock time should have advanced
  TEST_ASSERT_TRUE(ArduinoMock::mockMillis >= 100);
  // Pattern should have been started (at least one tone played)
  TEST_ASSERT_TRUE(ArduinoMock::ledcWriteToneCount > 0);
}

// Test 12: Blocking playback with disabled buzzer still advances time, no tones
void test_blocking_playback_disabled_no_sound(void) {
  test_buzzerEnabled = false;
  unsigned long startTime = ArduinoMock::mockMillis;
  buzzer_play_blocking(BUZZ_OTA_UPDATE, 100);

  // Time should have advanced
  TEST_ASSERT_TRUE(ArduinoMock::mockMillis >= startTime + 100);
  // No tones should have been played
  TEST_ASSERT_EQUAL(0, ArduinoMock::ledcWriteToneCount);
}

// ===== Queue Tests =====

// Test 13: Enqueue 3 patterns; verify they dequeue in FIFO order
void test_buzz_queue_3_in_order(void) {
  // Enqueue 3 patterns while not playing
  buzzer_play(BUZZ_BTN_SHORT);   // freq 1500
  buzzer_play(BUZZ_CLICK);       // freq 2000
  buzzer_play(BUZZ_NAV);         // freq 3000

  TEST_ASSERT_EQUAL(3, _buzzQueueCount);
  TEST_ASSERT_EQUAL(0u, _buzzQueueDropped);

  // First update: dequeues BUZZ_BTN_SHORT (1500 Hz)
  buzzer_update();
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(1500, (int)ArduinoMock::ledcLastFreq);

  // Advance past BTN_SHORT (100ms), stop it
  ArduinoMock::mockMillis = 100;
  buzzer_update(); // stop_buzzer called

  // Next update: dequeues BUZZ_CLICK (2000 Hz)
  buzzer_update();
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(2000, (int)ArduinoMock::ledcLastFreq);

  // Advance past CLICK (30ms)
  ArduinoMock::mockMillis = 130;
  buzzer_update(); // stop_buzzer

  // Next update: dequeues BUZZ_NAV (3000 Hz)
  buzzer_update();
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(3000, (int)ArduinoMock::ledcLastFreq);

  TEST_ASSERT_EQUAL(0, _buzzQueueCount);
}

// Test 14: 4th enqueue when full drops oldest; queue still has 3 with correct order
void test_buzz_queue_4th_drops_oldest(void) {
  // Fill queue: BTN_SHORT, CLICK, NAV
  buzzer_play(BUZZ_BTN_SHORT);   // slot 0 — will be dropped
  buzzer_play(BUZZ_CLICK);       // slot 1
  buzzer_play(BUZZ_NAV);         // slot 2

  TEST_ASSERT_EQUAL(3, _buzzQueueCount);
  TEST_ASSERT_EQUAL(0u, _buzzQueueDropped);

  // 4th enqueue — queue is full, oldest (BTN_SHORT) dropped
  buzzer_play(BUZZ_CONFIRM);
  TEST_ASSERT_EQUAL(3, _buzzQueueCount);
  TEST_ASSERT_EQUAL(1u, _buzzQueueDropped);

  // Dequeue order should be: CLICK, NAV, CONFIRM

  // First dequeue: CLICK (2000 Hz)
  buzzer_update();
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(2000, (int)ArduinoMock::ledcLastFreq);

  ArduinoMock::mockMillis = 30;
  buzzer_update(); // stop CLICK

  buzzer_update(); // dequeue NAV
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(3000, (int)ArduinoMock::ledcLastFreq);

  ArduinoMock::mockMillis = 40;
  buzzer_update(); // stop NAV

  buzzer_update(); // dequeue CONFIRM (2000 Hz first step)
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(2000, (int)ArduinoMock::ledcLastFreq);
}

// Test 15: Dequeue from empty queue returns BUZZ_NONE (no crash, no playback)
void test_buzz_queue_empty_returns_none(void) {
  TEST_ASSERT_EQUAL(0, _buzzQueueCount);

  // Update on empty queue should not start playback
  buzzer_update();

  TEST_ASSERT_FALSE(playing);
  TEST_ASSERT_EQUAL(0, ArduinoMock::ledcWriteToneCount);
  TEST_ASSERT_EQUAL(0, _buzzQueueCount);
}

// Test 16: Overflow increments _buzzQueueDropped
void test_buzz_queue_drop_counter(void) {
  TEST_ASSERT_EQUAL(0u, _buzzQueueDropped);

  // Fill queue
  buzzer_play(BUZZ_TICK);
  buzzer_play(BUZZ_TICK);
  buzzer_play(BUZZ_TICK);
  TEST_ASSERT_EQUAL(0u, _buzzQueueDropped);

  // Each additional call should drop one
  buzzer_play(BUZZ_CLICK);
  TEST_ASSERT_EQUAL(1u, _buzzQueueDropped);

  buzzer_play(BUZZ_CLICK);
  TEST_ASSERT_EQUAL(2u, _buzzQueueDropped);

  buzzer_play(BUZZ_NAV);
  TEST_ASSERT_EQUAL(3u, _buzzQueueDropped);

  // Queue count never exceeds BUZZ_QUEUE_SIZE
  TEST_ASSERT_EQUAL(BUZZ_QUEUE_SIZE, _buzzQueueCount);
}

// Test 17: Sequential play — simulate play->update->complete->update cycle
void test_buzz_queue_sequential_playback(void) {
  // Enqueue two short patterns
  buzzer_play(BUZZ_BTN_SHORT);  // 1500 Hz, 100ms
  buzzer_play(BUZZ_NAV);        // 3000 Hz, 10ms

  // --- Cycle 1: start BTN_SHORT ---
  buzzer_update();
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(1500, (int)ArduinoMock::ledcLastFreq);
  TEST_ASSERT_EQUAL(1, _buzzQueueCount); // NAV still queued

  // Advance 100ms — pattern finishes on next update
  ArduinoMock::mockMillis = 100;
  buzzer_update();
  TEST_ASSERT_FALSE(playing); // BTN_SHORT done

  // --- Cycle 2: dequeue and start NAV ---
  buzzer_update();
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(3000, (int)ArduinoMock::ledcLastFreq);
  TEST_ASSERT_EQUAL(0, _buzzQueueCount); // queue empty

  // Advance 10ms — NAV finishes
  ArduinoMock::mockMillis = 110;
  buzzer_update();
  TEST_ASSERT_FALSE(playing);

  // Queue fully drained
  TEST_ASSERT_EQUAL(0, _buzzQueueCount);
  TEST_ASSERT_EQUAL(0u, _buzzQueueDropped);
}

// ===== Main =====

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_buzzer_disabled_skips_all);
  RUN_TEST(test_buzzer_enabled_plays_pattern);
  RUN_TEST(test_volume_duty_low);
  RUN_TEST(test_volume_duty_medium);
  RUN_TEST(test_volume_duty_high);
  RUN_TEST(test_isr_tick_flag);
  RUN_TEST(test_isr_click_flag);
  RUN_TEST(test_pattern_sequencing_confirm);
  RUN_TEST(test_enqueued_pattern_plays_after_current);
  RUN_TEST(test_silence_gap_zero_duty);
  RUN_TEST(test_ota_update_pattern_plays);
  RUN_TEST(test_ota_update_pattern_sequencing);
  RUN_TEST(test_ota_update_disabled_skips);
  RUN_TEST(test_blocking_playback_completes);
  RUN_TEST(test_blocking_playback_disabled_no_sound);
  RUN_TEST(test_buzz_queue_3_in_order);
  RUN_TEST(test_buzz_queue_4th_drops_oldest);
  RUN_TEST(test_buzz_queue_empty_returns_none);
  RUN_TEST(test_buzz_queue_drop_counter);
  RUN_TEST(test_buzz_queue_sequential_playback);
  return UNITY_END();
}
