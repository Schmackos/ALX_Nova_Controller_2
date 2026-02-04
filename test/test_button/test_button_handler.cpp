#include <string>
#include <unity.h>


#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#define BUTTON_PIN 5
#define DEBOUNCE_TIME 50
#define LONG_PRESS_TIME 3000
#define VERY_LONG_PRESS_TIME 10000
#define DOUBLE_CLICK_TIME 500

// Button state machine
enum ButtonState { IDLE, PRESSED, HELD, RELEASED };
enum ButtonEvent {
  NONE,
  SINGLE_CLICK,
  DOUBLE_CLICK,
  LONG_PRESS,
  VERY_LONG_PRESS
};

struct ButtonHandler {
  ButtonState currentState = IDLE;
  ButtonEvent lastEvent = NONE;
  unsigned long pressStartTime = 0;
  unsigned long lastReleaseTime = 0;
  int clickCount = 0;
};

ButtonHandler button;

namespace TestButtonState {
int mockButtonPin[50] = {0};

void reset() {
  button.currentState = IDLE;
  button.lastEvent = NONE;
  button.pressStartTime = 0;
  button.lastReleaseTime = 0;
  button.clickCount = 0;
  std::memset(mockButtonPin, 0, sizeof(mockButtonPin));
#ifdef NATIVE_TEST
  ArduinoMock::reset();
#endif
}
} // namespace TestButtonState

// ===== BUTTON HANDLER IMPLEMENTATIONS =====

void setButtonPin(int pin, int value) {
  if (pin < 50) {
    TestButtonState::mockButtonPin[pin] = value;
  }
}

int readButtonPin() { return TestButtonState::mockButtonPin[BUTTON_PIN]; }

bool isButtonPressed() {
  return readButtonPin() == LOW; // Button pulls LOW when pressed (active low)
}

void updateButtonState() {
  unsigned long currentTime = millis();
  bool pressed = isButtonPressed();

  switch (button.currentState) {
  case IDLE:
    if (pressed) {
      button.pressStartTime = currentTime;
      button.currentState = PRESSED;
    }
    break;

  case PRESSED:
    if (!pressed) {
      // Button released immediately - could be debounce or single click
      if (currentTime - button.pressStartTime < DEBOUNCE_TIME) {
        // Likely debounce, stay in PRESSED
      } else {
        // Valid press -> release, check for double click
        button.lastReleaseTime = currentTime;
        button.clickCount++;
        button.currentState = RELEASED;
      }
    } else if (currentTime - button.pressStartTime >= VERY_LONG_PRESS_TIME) {
      button.lastEvent = VERY_LONG_PRESS;
      button.currentState = HELD;
    } else if (currentTime - button.pressStartTime >= LONG_PRESS_TIME) {
      button.lastEvent = LONG_PRESS;
      button.currentState = HELD;
    }
    break;

  case RELEASED:
    // Wait for double-click window or timeout
    if (pressed) {
      // Second press within double-click window
      button.currentState = PRESSED;
      button.pressStartTime = currentTime;
    } else if (currentTime - button.lastReleaseTime >= DOUBLE_CLICK_TIME) {
      // Double-click window expired
      if (button.clickCount == 1) {
        button.lastEvent = SINGLE_CLICK;
      } else if (button.clickCount == 2) {
        button.lastEvent = DOUBLE_CLICK;
      }
      button.clickCount = 0;
      button.currentState = IDLE;
    }
    break;

  case HELD:
    if (!pressed) {
      // Released after long press
      button.currentState = IDLE;
      button.clickCount = 0;
    }
    break;
  }
}

ButtonEvent getLastEvent() { return button.lastEvent; }

void clearEventState() { button.lastEvent = NONE; }

// ===== Test Setup/Teardown =====

void setUp(void) { TestButtonState::reset(); }

void tearDown(void) {
  // Clean up after each test
}

// ===== Button State Tests =====

void test_button_press_detected(void) {
  // Simulate button press
  setButtonPin(BUTTON_PIN, LOW); // Press
  ArduinoMock::mockMillis = 0;

  updateButtonState();

  TEST_ASSERT_EQUAL(PRESSED, button.currentState);
  TEST_ASSERT_EQUAL(0, button.pressStartTime); // Should be updated to 0
}

void test_button_debouncing(void) {
  // Simulate button bounce
  setButtonPin(BUTTON_PIN, LOW); // Press
  ArduinoMock::mockMillis = 0;
  updateButtonState();

  // Release within debounce window
  setButtonPin(BUTTON_PIN, HIGH);
  ArduinoMock::mockMillis = 25; // Less than DEBOUNCE_TIME
  updateButtonState();

  // State should still be PRESSED (debounce)
  TEST_ASSERT_EQUAL(PRESSED, button.currentState);
}

void test_button_long_press(void) {
  // Press button
  setButtonPin(BUTTON_PIN, LOW);
  ArduinoMock::mockMillis = 0;
  updateButtonState();

  // Hold for 3+ seconds
  ArduinoMock::mockMillis = LONG_PRESS_TIME + 100;
  updateButtonState();

  TEST_ASSERT_EQUAL(HELD, button.currentState);
  TEST_ASSERT_EQUAL(LONG_PRESS, button.lastEvent);
}

void test_button_very_long_press(void) {
  // Press button
  setButtonPin(BUTTON_PIN, LOW);
  ArduinoMock::mockMillis = 0;
  updateButtonState();

  // Hold for 10+ seconds
  ArduinoMock::mockMillis = VERY_LONG_PRESS_TIME + 100;
  updateButtonState();

  TEST_ASSERT_EQUAL(HELD, button.currentState);
  TEST_ASSERT_EQUAL(VERY_LONG_PRESS, button.lastEvent);
}

void test_button_release_timing(void) {
  // Press and hold
  setButtonPin(BUTTON_PIN, LOW);
  ArduinoMock::mockMillis = 0;
  updateButtonState();

  // Hold for a bit
  ArduinoMock::mockMillis = 100;
  updateButtonState();

  // Release
  setButtonPin(BUTTON_PIN, HIGH);
  ArduinoMock::mockMillis = 150;
  updateButtonState();

  TEST_ASSERT_EQUAL(RELEASED, button.currentState);
}

void test_button_single_click(void) {
  // Press
  setButtonPin(BUTTON_PIN, LOW);
  ArduinoMock::mockMillis = 0;
  updateButtonState();

  // Release after debounce time but before long press
  setButtonPin(BUTTON_PIN, HIGH);
  ArduinoMock::mockMillis = 100;
  updateButtonState();

  TEST_ASSERT_EQUAL(RELEASED, button.currentState);
  TEST_ASSERT_EQUAL(1, button.clickCount);

  // Wait for double-click window to expire
  ArduinoMock::mockMillis = 100 + DOUBLE_CLICK_TIME + 100;
  updateButtonState();

  TEST_ASSERT_EQUAL(IDLE, button.currentState);
  TEST_ASSERT_EQUAL(SINGLE_CLICK, button.lastEvent);
}

void test_button_double_click(void) {
  // First press
  setButtonPin(BUTTON_PIN, LOW);
  ArduinoMock::mockMillis = 0;
  updateButtonState();

  // Release
  setButtonPin(BUTTON_PIN, HIGH);
  ArduinoMock::mockMillis = 100;
  updateButtonState();

  TEST_ASSERT_EQUAL(1, button.clickCount);

  // Second press within double-click window
  setButtonPin(BUTTON_PIN, LOW);
  ArduinoMock::mockMillis = 200;
  updateButtonState();

  TEST_ASSERT_EQUAL(PRESSED, button.currentState);
  TEST_ASSERT_EQUAL(1, button.clickCount); // Still counting first click

  // Release second press
  setButtonPin(BUTTON_PIN, HIGH);
  ArduinoMock::mockMillis = 300;
  updateButtonState();

  TEST_ASSERT_EQUAL(RELEASED, button.currentState);
  TEST_ASSERT_EQUAL(2, button.clickCount);
}

void test_button_state_transitions(void) {
  // IDLE -> PRESSED
  setButtonPin(BUTTON_PIN, LOW);
  ArduinoMock::mockMillis = 0;
  updateButtonState();
  TEST_ASSERT_EQUAL(PRESSED, button.currentState);

  // PRESSED -> RELEASED
  setButtonPin(BUTTON_PIN, HIGH);
  ArduinoMock::mockMillis = 100;
  updateButtonState();
  TEST_ASSERT_EQUAL(RELEASED, button.currentState);

  // RELEASED -> IDLE
  ArduinoMock::mockMillis = 100 + DOUBLE_CLICK_TIME + 100;
  updateButtonState();
  TEST_ASSERT_EQUAL(IDLE, button.currentState);
}

void test_button_active_low_logic(void) {
  // Button uses active-low logic (pressed = LOW)
  // Unpressed state
  setButtonPin(BUTTON_PIN, HIGH);
  TEST_ASSERT_FALSE(isButtonPressed());

  // Pressed state
  setButtonPin(BUTTON_PIN, LOW);
  TEST_ASSERT_TRUE(isButtonPressed());
}

void test_button_held_release(void) {
  // Press and trigger long press
  setButtonPin(BUTTON_PIN, LOW);
  ArduinoMock::mockMillis = 0;
  updateButtonState();

  ArduinoMock::mockMillis = LONG_PRESS_TIME + 100;
  updateButtonState();

  TEST_ASSERT_EQUAL(HELD, button.currentState);

  // Release
  setButtonPin(BUTTON_PIN, HIGH);
  ArduinoMock::mockMillis = LONG_PRESS_TIME + 200;
  updateButtonState();

  TEST_ASSERT_EQUAL(IDLE, button.currentState);
}

// ===== Test Runner =====

int runUnityTests(void) {
  UNITY_BEGIN();

  RUN_TEST(test_button_press_detected);
  RUN_TEST(test_button_debouncing);
  RUN_TEST(test_button_long_press);
  RUN_TEST(test_button_very_long_press);
  RUN_TEST(test_button_release_timing);
  RUN_TEST(test_button_single_click);
  RUN_TEST(test_button_double_click);
  RUN_TEST(test_button_state_transitions);
  RUN_TEST(test_button_active_low_logic);
  RUN_TEST(test_button_held_release);

  return UNITY_END();
}

// For native platform
#ifdef NATIVE_TEST
int main(void) { return runUnityTests(); }
#endif

// For Arduino platform
#ifndef NATIVE_TEST
void setup() {
  delay(2000);
  runUnityTests();
}

void loop() {
  // Do nothing
}
#endif
