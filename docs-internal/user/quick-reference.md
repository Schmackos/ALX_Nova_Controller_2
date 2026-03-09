# Smart Sensing - Quick Reference Guide

## Input Field Behavior

### When Can I Edit Settings?

| Condition | Timer Duration | Voltage Threshold | Effect of Change |
|-----------|----------------|-------------------|------------------|
| Mode: ALWAYS_ON | ✅ Editable | ✅ Editable | Ready for next use |
| Mode: ALWAYS_OFF | ✅ Editable | ✅ Editable | Ready for next use |
| Mode: SMART_AUTO, Amp OFF | ✅ Editable | ✅ Editable | Will apply on next voltage detection |
| Mode: SMART_AUTO, Amp ON, Timer = 0 | ✅ Editable | ✅ Editable | Ready for next voltage detection |
| Mode: SMART_AUTO, Amp ON, Timer > 0 | ✅ Editable | ✅ Editable | **Timer restarts with new duration!** |

**All input fields are always editable!**

## Visual Indicators

### Input Fields (Always Editable)
```
┌─────────────────────┐
│ 15                  │ ← White background, always enabled
└─────────────────────┘
   Normal cursor - always ready for input
```

### While Editing (Focus)
```
┌─────────────────────┐
│ 15█                 │ ← White background, cursor visible
└─────────────────────┘
   WebSocket updates BLOCKED for this field
   Type freely without interference!
```

### While Not Editing (Blur)
```
┌─────────────────────┐
│ 15                  │ ← White background, no cursor
└─────────────────────┘
   WebSocket updates ACTIVE for this field
   Value auto-updates from ESP32
```

## Common Questions

### Q: Can I change the timer while it's running?
**A**: Yes! All input fields are always editable. Changing the timer while running will restart the countdown with the new duration.

### Q: Why does my input value keep changing back?
**A**: The WebSocket updates the field every second when it's NOT focused. Click in the field to edit, and WebSocket updates will be blocked for that field.

### Q: When does the countdown start?
**A**: Only when voltage is detected and crosses the threshold. The amplifier turns ON and the timer begins.

### Q: Can I change settings before voltage is detected?
**A**: Yes! All settings are always editable. Changes made before voltage detection will be used when the timer starts.

### Q: What happens if I change the timer while it's counting down?
**A**: The countdown immediately restarts with the new duration. For example, changing from 15 minutes to 5 minutes will restart the timer at 5:00.

### Q: Do my changes save immediately?
**A**: Changes are sent when you finish entering the value (click outside the input, press Enter, or Tab to next field). Not on every keystroke.

### Q: Will WebSocket overwrite what I'm typing?
**A**: No! While you're actively editing a field (field has focus), WebSocket updates are blocked for that field. When you click away, updates resume.

## State Diagram

```
┌──────────────────┐
│   SMART_AUTO     │
│   Mode Active    │
└────────┬─────────┘
         │
         v
┌──────────────────┐
│  Amplifier OFF   │
│  Timer = 0       │◄─────────┐
│  ✅ Inputs Open  │          │
└────────┬─────────┘          │
         │                    │
         │ Voltage Detected   │
         v                    │
┌──────────────────┐          │
│  Amplifier ON    │          │
│  Timer Counting  │          │
│  ✅ Inputs Open  │          │
│  (Can adjust!)   │          │
└────────┬─────────┘          │
         │                    │
         │ User changes       │
         │ timer duration?    │
         │        │           │
         │        ├─ Yes ─────┤ Timer restarts
         │        │           │ with new value
         │        └─ No       │
         │                    │
         │ Countdown...       │
         v                    │
┌──────────────────┐          │
│  Timer = 0       │          │
│  Amp turns OFF   │──────────┘
│  ✅ Inputs Open  │
└──────────────────┘
```

## Keyboard Shortcuts

- **Tab**: Move between fields (triggers save on previous field)
- **Enter**: Confirm input and send to ESP32
- **Esc**: Cancel editing (reverts to previous value)
- **Arrow Up/Down**: Increment/decrement number values

## Error Messages

| Error | Meaning | Solution |
|-------|---------|----------|
| "Timer duration must be between 1 and 60 minutes" | Invalid value entered | Enter a number between 1-60 |
| "Voltage threshold must be between 0.1 and 3.3 volts" | Invalid value entered | Enter a number between 0.1-3.3 |

**Note**: No errors for editing while timer is running - all adjustments are allowed!

## Tips

1. **Adjust anytime**: Don't worry about when to change settings - inputs are always editable
2. **Restart countdown**: Change timer duration while running to restart with new value
3. **Type freely**: Click in field and type - WebSocket won't interfere
4. **Use Enter key**: Press Enter to immediately submit your changes
5. **Watch the countdown**: See your timer adjustment take effect in real-time
6. **Dynamic tuning**: Experiment with different timer values even while system is running
