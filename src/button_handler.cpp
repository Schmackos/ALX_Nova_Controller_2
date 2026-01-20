#include "button_handler.h"

ButtonHandler::ButtonHandler(int buttonPin) : pin(buttonPin) {
  currentState = HIGH;
  lastState = HIGH;
  pressed = false;
  pressStartTime = 0;
  releaseTime = 0;
  lastDebounceTime = 0;
  clickCount = 0;
  lastClickTime = 0;
  detectedPress = BTN_NONE;
  longPressTriggered = false;
  veryLongPressTriggered = false;
}

void ButtonHandler::begin() {
  pinMode(pin, INPUT_PULLUP);
}

ButtonPressType ButtonHandler::update() {
  detectedPress = BTN_NONE;
  bool reading = digitalRead(pin);
  
  // Debouncing
  if (reading != lastState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > BTN_DEBOUNCE_TIME) {
    if (reading != currentState) {
      currentState = reading;
      
      // Button pressed (LOW because of INPUT_PULLUP)
      if (currentState == LOW && !pressed) {
        pressed = true;
        pressStartTime = millis();
        longPressTriggered = false;
        veryLongPressTriggered = false;
      }
      // Button released
      else if (currentState == HIGH && pressed) {
        pressed = false;
        releaseTime = millis();
        unsigned long pressDuration = releaseTime - pressStartTime;
        
        // Check if it was a very long press (already triggered)
        if (veryLongPressTriggered) {
          // Already handled in the hold detection below
          clickCount = 0; // Reset click counting
        }
        // Check if it was a long press (already triggered)
        else if (longPressTriggered) {
          // Already handled in the hold detection below
          clickCount = 0; // Reset click counting
        }
        // Short press - could be part of multi-click
        else if (pressDuration < BTN_SHORT_PRESS_MAX) {
          clickCount++;
          lastClickTime = releaseTime;
        }
      }
    }
  }
  
  // Check for long press while button is held
  if (pressed && !veryLongPressTriggered && !longPressTriggered) {
    unsigned long holdDuration = millis() - pressStartTime;
    
    if (holdDuration >= BTN_VERY_LONG_PRESS_MIN) {
      veryLongPressTriggered = true;
      detectedPress = BTN_VERY_LONG_PRESS;
      clickCount = 0;
    }
    else if (holdDuration >= BTN_LONG_PRESS_MIN) {
      longPressTriggered = true;
      detectedPress = BTN_LONG_PRESS;
      clickCount = 0;
    }
  }
  
  // Check for multi-click timeout
  if (clickCount > 0 && !pressed) {
    if (millis() - lastClickTime > BTN_MULTI_CLICK_WINDOW) {
      // Multi-click sequence complete
      if (clickCount == 1) {
        detectedPress = BTN_SHORT_PRESS;
      } else if (clickCount == 2) {
        detectedPress = BTN_DOUBLE_CLICK;
      } else if (clickCount >= 3) {
        detectedPress = BTN_TRIPLE_CLICK;
      }
      clickCount = 0;
    }
  }
  
  lastState = reading;
  return detectedPress;
}

// Get current hold duration (useful for progress indicators)
unsigned long ButtonHandler::getHoldDuration() {
  if (pressed) {
    return millis() - pressStartTime;
  }
  return 0;
}

// Check if button is currently pressed
bool ButtonHandler::isPressed() {
  return pressed;
}
