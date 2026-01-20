#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "config.h"

// Button state tracking class
class ButtonHandler {
public:
  int pin;
  bool currentState;
  bool lastState;
  bool pressed;
  unsigned long pressStartTime;
  unsigned long releaseTime;
  unsigned long lastDebounceTime;
  int clickCount;
  unsigned long lastClickTime;
  ButtonPressType detectedPress;
  bool longPressTriggered;
  bool veryLongPressTriggered;
  
  ButtonHandler(int buttonPin);
  void begin();
  ButtonPressType update();
  unsigned long getHoldDuration();
  bool isPressed();
};

#endif // BUTTON_HANDLER_H
