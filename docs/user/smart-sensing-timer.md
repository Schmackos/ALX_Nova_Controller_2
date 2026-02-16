# Smart Sensing Timer Behavior

## Overview
The Smart Sensing feature has been updated with the following key behaviors:
1. Timer countdown only starts when voltage is detected and the amplifier turns ON
2. Timer duration and voltage threshold inputs are **always editable**
3. Input fields won't be overwritten by WebSocket updates while you're editing them
4. Adjusting timer duration while timer is running will restart the countdown with the new value

## Timer Behavior

### Mode: SMART_AUTO

#### Initial State
- When switching to SMART_AUTO mode, `timerRemaining` is set to `timerDuration * 60`
- Timer DISPLAYS the full duration but is NOT counting down yet
- Countdown will start when voltage is detected

#### Voltage Detection (Amplifier OFF → ON)
- When voltage crosses the threshold (rising edge):
  - Amplifier turns ON
  - Timer starts: `timerRemaining = timerDuration * 60` (converts minutes to seconds)
  - Countdown begins immediately

#### Timer Countdown
- Timer decrements by 1 every second (only when `amplifierState == true AND timerRemaining > 0`)
- Countdown is PAUSED when amplifier is OFF (even if timerRemaining > 0)
- Countdown RESUMES when voltage is detected and amplifier turns ON
- When timer reaches 0:
  - Amplifier turns OFF
  - Countdown stops

#### Changing Timer Duration via Webpage

**Case 1: Amplifier is OFF (no voltage detected, countdown paused)**
- Input fields are **always enabled** and editable
- Updating `timerDuration` updates the displayed time remaining
- `timerRemaining` is set to `timerDuration * 60` but does NOT count down
- Countdown will start when voltage is detected and amplifier turns ON
- Log message: "Timer duration changed to: X minutes (countdown will start when voltage detected)"

**Case 2: Amplifier is ON (countdown active)**
- Input fields are **always enabled** and editable
- Updating `timerDuration` updates `timerRemaining` to the new value
- `timerRemaining` is set to `timerDuration * 60`
- Countdown continues/restarts from the new duration
- Log message: "Timer duration changed to: X minutes (countdown active)"

**Case 3: Editing Protection**
- While you're actively typing in an input field (field has focus), WebSocket updates won't overwrite your changes
- When you finish editing (press Enter, Tab, or click away), the value is sent to ESP32
- Input fields update from WebSocket only when you're NOT editing them

### Mode: ALWAYS_ON
- Amplifier is always ON
- Timer is disabled (`timerRemaining = 0`)
- No countdown occurs

### Mode: ALWAYS_OFF
- Amplifier is always OFF
- Timer is disabled (`timerRemaining = 0`)
- No countdown occurs

## Example Timeline

```
Time    Event                          timerRemaining  amplifierState  Countdown?
-----   ---------------------------    --------------  --------------  ----------
0:00    Switch to SMART_AUTO           900 sec (15m)   OFF             PAUSED
0:10    Set timer to 5 min             300 sec (5m)    OFF             PAUSED
0:20    Set timer to 15 min            900 sec (15m)   OFF             PAUSED
0:30    Voltage detected (rising)      900 sec (15m)   ON              ACTIVE
0:31    Timer counting down            899 sec         ON              ACTIVE
0:32    Timer counting down            898 sec         ON              ACTIVE
1:30    Adjust timer to 10 min         600 sec (10m)   ON              ACTIVE (restarted)
1:31    Timer counting down            599 sec         ON              ACTIVE
11:30   Timer expires                  0 sec           OFF (auto)      STOPPED
11:31   Set timer to 20 min            1200 sec (20m)  OFF             PAUSED
11:50   Voltage detected again         1200 sec (20m)  ON              ACTIVE
11:51   Timer counting down            1199 sec        ON              ACTIVE
```

## Benefits

1. **Smart Countdown**: Timer only counts down when amplifier is ON (voltage detected)
2. **Visual Feedback**: Display shows configured duration even when not counting down
3. **No Wasted Time**: Countdown paused when amplifier OFF, resumes when ON
4. **Flexible Adjustment**: Can change timer duration at any time
5. **Immediate Display Update**: Changing duration updates display instantly
6. **Countdown Only When Active**: Timer decrements only when amplifier is ON
7. **No Input Interference**: WebSocket updates won't overwrite your input while you're typing
8. **Always Editable**: No disabled/locked input fields - adjust settings whenever needed

## API Response

When calling `/api/smartsensing` endpoint:

```json
{
  "success": true,
  "mode": "smart_auto",
  "timerDuration": 15,        // Minutes (user-set)
  "timerRemaining": 0,        // Seconds (0 if amplifier OFF, counting down if ON)
  "amplifierState": false,    // true = ON, false = OFF
  "voltageThreshold": 1.0,    // Volts
  "voltageReading": 0.45,     // Current reading
  "voltageDetected": false    // true if reading >= threshold
}
```

## Serial Monitor Messages

```
Mode changed to: smart_auto
SMART_AUTO mode activated - timer will start when voltage is detected
Timer duration set to: 15 minutes

[... waiting for voltage ...]

Smart Auto: Voltage detected above threshold - timer started
Amplifier state changed to: ON

[... timer counting down ...]
[... user changes timer duration to 10 minutes while running ...]

Timer duration changed to: 10 minutes (timer restarted)
Timer duration set to: 10 minutes

[... timer continues from 10 minutes ...]

Smart Auto: Timer expired, turning amplifier OFF
Amplifier state changed to: OFF
```

## WebSocket Updates

The system broadcasts Smart Sensing state every second via WebSocket, allowing the webpage to display:
- Current timer remaining (in real-time)
- Amplifier status
- Voltage reading and detection status
- **Smart input field updates** that don't interfere with user editing

This ensures the UI always reflects the actual state of the system.

## UI Behavior Details

### Input Field States

**Timer Duration Input (`timerDuration`)**
- **Always enabled** - can be edited at any time
- WebSocket updates the value automatically when you're NOT editing
- While focused (typing), WebSocket updates are blocked to prevent overwriting your input
- When you finish editing (blur/Enter/Tab), your new value is sent to ESP32
- Adjusting while timer is running restarts the countdown

**Voltage Threshold Input (`voltageThreshold`)**
- **Always enabled** - can be edited at any time
- Same focus-aware update behavior as timer duration
- Changes take effect immediately
- Can be adjusted even while timer is running

### User Experience Flow

1. **Setup Phase** (Amplifier OFF)
   - User adjusts timer duration (e.g., 15 minutes)
   - User adjusts voltage threshold (e.g., 1.0V)
   - Inputs are always editable
   - Changes apply when user finishes editing (blur/Enter/Tab)

2. **Voltage Detection** (Amplifier turns ON)
   - Voltage crosses threshold
   - Timer starts counting down
   - Input fields remain editable
   - User can see real-time countdown

3. **Active Countdown** (Timer running)
   - User can view timer counting down in real-time
   - Input fields remain editable - can adjust timer or voltage
   - Clicking in an input field stops WebSocket updates to that field
   - Adjusting timer duration restarts the countdown
   - When user clicks away, WebSocket updates resume

4. **Timer Completion** (Amplifier turns OFF)
   - Timer reaches 0
   - Amplifier shuts off automatically
   - Input fields remain editable
   - Ready for next detection event

### Focus-Aware Updates

The system tracks which input fields you're currently editing:
- **While focused** (actively typing): WebSocket won't update that specific field
- **While blurred** (not editing): WebSocket updates the field normally
- This prevents the frustrating experience of your typing being overwritten

**Example:**
```
User clicks in timer field → inputFocusState.timerDuration = true
WebSocket sends update → Skips timer field (user is editing)
User presses Enter → Value sent to ESP32, field blurs
Field blurs → inputFocusState.timerDuration = false
WebSocket sends update → Updates timer field (user finished editing)
```
