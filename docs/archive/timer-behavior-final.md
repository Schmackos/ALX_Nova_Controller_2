# Smart Sensing Timer - Final Behavior

## Core Concept

The timer has **two states**:
1. **Display Value** (`timerRemaining`) - Always shows the configured duration
2. **Countdown Active** - Only when amplifier is ON

## Key Rule

**Timer only counts down when `amplifierState == true`**

Even if `timerRemaining > 0`, the countdown is PAUSED when amplifier is OFF.

## Behavior Breakdown

### Scenario 1: Initial Setup (Amplifier OFF)

```
Action: Switch to SMART_AUTO mode
Result: timerRemaining = 900 sec (15 minutes default)
Display: Shows "15:00"
Countdown: PAUSED (amplifier OFF)
```

### Scenario 2: Adjust Timer Before Voltage Detection

```
Action: User changes timer to 10 minutes
Result: timerRemaining = 600 sec
Display: Shows "10:00"
Countdown: STILL PAUSED (amplifier still OFF)
```

### Scenario 3: Voltage Detected

```
Event: Voltage crosses threshold
Result: amplifierState = true, timerRemaining stays at 600 sec
Display: Shows "10:00" and starts counting down
Countdown: ACTIVE (9:59, 9:58, 9:57...)
```

### Scenario 4: Adjust Timer While Countdown Active

```
Action: User changes timer to 5 minutes while it's at 8:23
Result: timerRemaining = 300 sec (resets to 5:00)
Display: Shows "5:00" and continues counting down
Countdown: ACTIVE (continues from new value: 4:59, 4:58...)
```

### Scenario 5: Timer Expires

```
Event: timerRemaining reaches 0
Result: amplifierState = false (turns OFF)
Display: Shows "0:00"
Countdown: STOPPED
```

### Scenario 6: Adjust Timer After Expiration

```
Action: User changes timer to 20 minutes
Result: timerRemaining = 1200 sec
Display: Shows "20:00"
Countdown: PAUSED (amplifier is OFF)
Note: Will start counting when next voltage is detected
```

## Code Logic

### Timer Update (handleSmartSensingUpdate)

```cpp
if (currentMode == SMART_AUTO) {
  // ALWAYS update timerRemaining to new duration
  timerRemaining = timerDuration * 60;
  
  if (amplifierState) {
    // Amplifier ON: countdown is active, restart timer
    lastTimerUpdate = millis();
    Serial.println("Countdown active");
  } else {
    // Amplifier OFF: countdown is paused, just show new duration
    Serial.println("Countdown will start when voltage detected");
  }
}
```

### Timer Countdown (updateSmartSensingLogic)

```cpp
// Only decrement if BOTH conditions are true:
// 1. amplifierState == true
// 2. timerRemaining > 0
if (amplifierState && timerRemaining > 0 && (millis() - lastTimerUpdate >= 1000)) {
  lastTimerUpdate = millis();
  timerRemaining--;  // Countdown happens here
  
  if (timerRemaining == 0) {
    setAmplifierState(false);  // Turn OFF when timer expires
  }
}
```

## User Experience Summary

| Amplifier State | Timer Remaining | What User Sees | Countdown? |
|-----------------|-----------------|----------------|------------|
| OFF | 900 sec | "15:00" (static) | No |
| OFF → ON (voltage) | 900 sec | "15:00" → "14:59"... | Yes |
| ON | 820 sec | "13:40" → "13:39"... | Yes |
| ON (user adjusts to 5 min) | 300 sec | Jumps to "5:00" → "4:59"... | Yes |
| ON → OFF (expired) | 0 sec | "0:00" (static) | No |

## Benefits

1. **No Wasted Countdown**: Timer doesn't count down when amplifier is OFF
2. **Visual Feedback**: User can see configured duration even when paused
3. **Instant Updates**: Changing duration updates display immediately
4. **Smart Resume**: If voltage detected again, countdown starts from configured duration
5. **Flexible Adjustment**: Can change at any time, effect is immediate

## Testing Checklist

- [ ] Switch to SMART_AUTO → Timer shows duration but doesn't count
- [ ] Adjust timer while OFF → Display updates, no countdown
- [ ] Voltage detected → Timer starts counting from configured value
- [ ] Adjust timer while ON → Timer resets and continues counting
- [ ] Timer expires → Amplifier turns OFF, countdown stops
- [ ] Adjust timer after expiration → Display updates, countdown paused until next voltage

## Visual Indicator Suggestion

Consider adding a visual indicator in the UI:
- ⏸️ **Paused icon** when amplifierState = false
- ▶️ **Play icon** when amplifierState = true and counting
- ⏹️ **Stop icon** when timerRemaining = 0
