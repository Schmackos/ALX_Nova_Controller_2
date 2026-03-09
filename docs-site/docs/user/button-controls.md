---
title: Button Controls
sidebar_position: 8
description: Physical button and rotary encoder controls on the ALX Nova.
---

# Button Controls

ALX Nova has two physical control interfaces: a **multi-function push button** on GPIO 46 for system-level actions, and a **rotary encoder** (EC11) for navigating and adjusting settings on the TFT display.

---

## Multi-Function Button (GPIO 46)

A single button provides access to several functions depending on how you press it. The button uses a pull-up resistor and is active when pressed to ground — there is no need to hold it down firmly; a gentle click is enough.

:::tip
Watch the TFT display while pressing the button. It shows progress feedback during long-press actions and confirms which action was registered.
:::

### Quick Reference

| Press Type | Timing | Action |
|---|---|---|
| Short press | Less than 0.5 seconds | Toggle the amplifier relay on/off |
| Long press | Hold for 2 seconds | Restart the controller |
| Very long press | Hold for 10 seconds | Factory reset |
| Double-click | Two presses within 0.4 seconds | Toggle AP (access point) mode |
| Triple-click | Three presses within 0.4 seconds | Toggle the amplifier relay |

---

### Short Press — Toggle Amplifier

A quick tap toggles the amplifier relay. If the amplifier is off it turns on; if it is on it turns off. This is the same action as the Amplifier switch in the web interface.

:::info
In **Smart Auto** sensing mode, a short press manually overrides the automatic state. The timer continues to run normally after the manual toggle.
:::

---

### Long Press — Restart

Hold the button for at least 2 seconds (but less than 10 seconds), then release. The controller performs a soft restart — all settings are preserved. This is equivalent to clicking **Reboot** in the web interface.

During the long press, the TFT display shows a progress indicator so you can see the hold duration building up.

---

### Very Long Press — Factory Reset

:::danger
This action erases all settings including WiFi credentials, MQTT configuration, DSP presets, and the web password. The controller returns to factory defaults and restarts in AP mode. This cannot be undone.
:::

1. Hold the button for the full **10 seconds**.
2. While holding, the TFT display shows a countdown and the buzzer sounds a repeating pattern.
3. After 10 seconds the reset executes and the controller reboots.

You can **cancel** the factory reset by releasing the button before the 10 seconds are up. The controller will perform a regular restart instead (long press) or nothing if you release before 2 seconds.

---

### Double-Click — Toggle Access Point

Two quick presses within 0.4 seconds toggle the WiFi access point on or off.

- If the AP is currently off, this turns it on. The SSID (`ALX-XXXXXXXXXXXX`) becomes visible in your device's WiFi list.
- If the AP is currently on, this turns it off.

This is the fastest way to regain access to the web interface when you do not know the controller's current IP address on your home network. Enable the AP, connect to it, and browse to `http://192.168.4.1`.

:::tip
Double-clicking to enable AP mode does not disconnect the controller from your home network. Both connections are active simultaneously, so you can use the AP to reach the web interface while it stays connected to your router for MQTT and OTA.
:::

---

### Triple-Click — Toggle Amplifier Relay

Three presses within 0.4 seconds also toggle the amplifier relay, as an alternative to the short press. This gesture is provided for hardware setups where the button response timing makes a single short press ambiguous.

---

## Debounce and Click Detection

The button handler uses a 50 ms debounce window to filter electrical noise, so very rapid accidental contacts are ignored. The multi-click detection window is 400 ms — clicks within that window are counted together and registered as a double or triple click.

---

## Rotary Encoder (EC11)

The EC11 rotary encoder controls the LVGL-based TFT display interface. It has three inputs:

- **Rotate clockwise / counter-clockwise** — navigate menus, adjust values
- **Press (click)** — select / confirm

### Navigation

- On the desktop carousel, rotate to scroll between available screens (Home, Control, WiFi, MQTT, Settings, Debug, Signal Generator, Devices, Support).
- Press to enter the highlighted screen.
- Inside a screen, rotate to move focus between interactive elements (sliders, toggles, buttons, list items).
- Press to activate the focused element.

### Volume / Value Adjustment

When a slider or numeric field has focus (shown by the LVGL highlight border):

- Rotate clockwise to increase the value.
- Rotate counter-clockwise to decrease the value.
- The step size adapts to the parameter range — coarse steps for wide-range parameters, fine steps for precise ones.

### Back Navigation

Inside any screen, pressing the encoder button when the **Back** element is focused (or when no other element is focused) returns to the desktop carousel.

:::info
The encoder uses a Gray code state machine for clean, debounced detection. Even fast spinning is tracked accurately without double-counting steps.
:::

---

## Buzzer Feedback

Many button and encoder interactions trigger a short piezo buzzer tone as confirmation:

- Single click: short blip
- Encoder step: faint tick
- Action confirmed: ascending two-tone
- Factory reset in progress: repeating rapid pattern
- Boot complete: startup chime

Buzzer volume can be adjusted (or muted) from the **Settings** screen on the TFT display or from the Settings tab in the web interface.
