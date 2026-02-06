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
};

struct ToneStep {
  uint16_t freq_hz;
  uint16_t duration_ms;
};

// Pattern definitions (same as buzzer_handler.cpp)
static const ToneStep pat_tick[] = {{4000, 8}, {0, 0}};
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
  default:
    return nullptr;
  }
}

static const uint8_t volume_duty[] = {25, 76, 153};

// Sequencer state
static const ToneStep *current_pattern = nullptr;
static int current_step = 0;
static unsigned long step_start_ms = 0;
static bool playing = false;
static volatile BuzzerPattern pending_pattern = BUZZ_NONE;
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

static void buzzer_update() {
  if (_buzzer_tick_pending) {
    _buzzer_tick_pending = false;
    if (test_buzzerEnabled) {
      start_pattern(pat_tick);
    }
  }
  if (_buzzer_click_pending) {
    _buzzer_click_pending = false;
    if (test_buzzerEnabled) {
      start_pattern(pat_click);
    }
  }

  BuzzerPattern req = pending_pattern;
  if (req != BUZZ_NONE) {
    pending_pattern = BUZZ_NONE;
    if (test_buzzerEnabled) {
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

static void buzzer_play(BuzzerPattern pattern) { pending_pattern = pattern; }

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
  pending_pattern = BUZZ_NONE;
  _buzzer_tick_pending = false;
  _buzzer_click_pending = false;
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
  TEST_ASSERT_EQUAL(4000, (int)ArduinoMock::ledcLastFreq);
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

// Test 6: New pattern overrides current
void test_new_pattern_overrides(void) {
  buzzer_play(BUZZ_BTN_LONG); // 3-step pattern
  buzzer_update();
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(2000, (int)ArduinoMock::ledcLastFreq);

  // Override with tick
  buzzer_play(BUZZ_TICK);
  buzzer_update();
  TEST_ASSERT_TRUE(playing);
  TEST_ASSERT_EQUAL(4000, (int)ArduinoMock::ledcLastFreq);
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
  RUN_TEST(test_new_pattern_overrides);
  RUN_TEST(test_silence_gap_zero_duty);
  return UNITY_END();
}
