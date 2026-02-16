# Enhanced Button Detection Guide

## Overview
Your ESP32 project now includes a sophisticated button handler that can detect multiple types of button presses on a single GPIO pin.

## Detected Button Press Types

### 1. **Short Press** (< 0.5 seconds)
- **Action**: Prints status information to Serial
- **Use Case**: Quick status check
- **Output**: WiFi status, AP mode status, LED blinking state, firmware version

### 2. **Double Click** (2 quick presses within 400ms)
- **Action**: Toggles Access Point mode
- **Use Case**: Quick AP mode on/off without holding
- **Behavior**: Starts or stops the WiFi Access Point

### 3. **Triple Click** (3 quick presses within 400ms)
- **Action**: Toggles LED blinking
- **Use Case**: Enable/disable the blinking LED
- **Behavior**: Turns LED blinking on or off

### 4. **Long Press** (2 seconds)
- **Action**: Restarts the ESP32
- **Use Case**: Quick reboot without factory reset
- **Behavior**: Performs ESP.restart()

### 5. **Very Long Press** (10 seconds)
- **Action**: Factory reset
- **Use Case**: Complete device reset
- **Behavior**: Erases WiFi credentials, restores default settings
- **Visual Feedback**: LED blinks rapidly, progress sent via WebSocket

## Technical Details

### Timing Constants
```cpp
BTN_DEBOUNCE_TIME = 50ms         // Debounce delay
BTN_SHORT_PRESS_MAX = 500ms      // Maximum duration for short press
BTN_LONG_PRESS_MIN = 2000ms      // Minimum duration for long press
BTN_VERY_LONG_PRESS_MIN = 10000ms // Minimum duration for very long press
BTN_MULTI_CLICK_WINDOW = 400ms   // Time window between clicks
```

### Button Pin Configuration
- **Pin**: GPIO 15
- **Mode**: INPUT_PULLUP (button connects to GND when pressed)
- **Active State**: LOW

## ButtonHandler Class

The implementation uses a reusable `ButtonHandler` class with the following features:

### Key Methods
- `begin()` - Initialize the button pin
- `update()` - Must be called in loop(), returns detected press type
- `getHoldDuration()` - Returns current hold duration in milliseconds
- `isPressed()` - Returns true if button is currently pressed

### State Tracking
- Debouncing to prevent false triggers
- Click counting for multi-click detection
- Hold duration tracking for long press detection
- Edge detection for press and release events

## Visual Feedback

### During Very Long Press (Factory Reset)
- LED blinks rapidly (every 200ms)
- Progress updates sent to Serial every second
- WebSocket updates for web interface
- Cancellable by releasing before 10 seconds

## Example Usage

To add another button to your project:

```cpp
// Declare the button
const int MY_BUTTON_PIN = 14;
ButtonHandler myButton(MY_BUTTON_PIN);

// In setup()
myButton.begin();

// In loop()
ButtonPressType pressType = myButton.update();
switch (pressType) {
  case BTN_SHORT_PRESS:
    // Handle short press
    break;
  case BTN_DOUBLE_CLICK:
    // Handle double click
    break;
  case BTN_TRIPLE_CLICK:
    // Handle triple click
    break;
  case BTN_LONG_PRESS:
    // Handle long press
    break;
  case BTN_VERY_LONG_PRESS:
    // Handle very long press
    break;
}
```

## Extensibility

The `ButtonHandler` class can easily be extended to support:
- Quadruple clicks or more
- Custom hold durations
- Press-and-hold-then-click combinations
- Rhythmic press patterns
- Different debounce times per button

## Testing Tips

1. **Short Press**: Quick tap and release
2. **Double Click**: Two quick taps (like double-clicking a mouse)
3. **Triple Click**: Three rapid taps in succession
4. **Long Press**: Hold for 2 seconds, then release
5. **Very Long Press**: Hold for full 10 seconds (watch LED blink rapidly)

## Safety Features

- Debouncing prevents accidental triggers
- Factory reset requires full 10-second hold
- Visual and serial feedback during long operations
- Can cancel factory reset by releasing early
- All actions are logged to Serial monitor
