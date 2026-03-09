---
title: Smart Sensing
sidebar_position: 4
description: Configure automatic signal detection, amplifier control, and auto-off timers.
---

# Smart Sensing

Smart Sensing is the heart of what makes the ALX Nova a truly automatic amplifier controller. Instead of manually switching your amplifier on and off, the controller listens to your audio inputs, detects whether music or audio is present, and manages your amplifier relay accordingly.

---

## What Smart Sensing Does

The ALX Nova continuously analyses the audio arriving at its ADC inputs (up to 8 independent input lanes). It measures the signal level in dBFS (decibels relative to full scale) using the precision PCM1808 I2S ADC. When that level crosses a configurable threshold, the controller fires the amplifier relay (GPIO 27) to switch your amplifier on.

When the signal falls below the threshold and stays there for the duration of the auto-off timer, the relay opens again and your amplifier powers down.

This means:
- Your amplifier turns on the moment you start playing music — no manual intervention needed.
- It turns off automatically after a period of silence, saving power and reducing wear on your amplifier.
- You have full control over sensitivity (the threshold) and patience (the timer duration).

:::info Signal measurement uses I2S audio, not a voltage pin
On the ESP32-P4 hardware, signal detection is performed by reading the digital audio levels from the PCM1808 I2S ADC — not from an analogue voltage sense pin. The threshold is expressed in dBFS (a logarithmic audio level scale), which maps naturally to real-world audio levels.
:::

---

## The Three Sensing Modes

Open the **Control** tab in the web interface to choose a mode. The currently active mode is always shown at the top of the Control tab along with the live amplifier state.

### Always On

The amplifier relay stays energised continuously regardless of whether any audio signal is present. The timer is disabled.

**Use this mode when:**
- You want the amplifier on at all times and prefer to manage it manually or via MQTT.
- You are testing your audio setup and do not want automatic shutdowns interrupting you.
- The amplifier has its own standby management and you just want the relay permanently closed.

### Always Off

The amplifier relay stays open continuously. The amplifier will not turn on regardless of signal.

**Use this mode when:**
- You are temporarily shutting down the audio system and want to ensure it stays off.
- You are performing maintenance and need the amplifier safely de-energised.
- You want to override automation and keep everything off remotely via MQTT or the web interface.

### Smart Auto

This is the fully automatic mode. The controller monitors the audio level on all active input lanes and manages the amplifier relay based on your threshold and timer settings.

**Use this mode for normal day-to-day operation.** It gives you a set-and-forget experience — start playing music and the amplifier turns on; stop for long enough and it turns off.

---

## How the Threshold Works

In Smart Auto mode, the controller constantly measures the audio level of incoming signals in dBFS. The threshold is the level at which the controller considers "signal present."

- When the measured level rises **above** the threshold, the amplifier turns on and the countdown timer starts.
- When the measured level falls **below** the threshold, the timer continues counting down. If the signal rises again before the timer reaches zero, the timer resets.
- When the timer reaches zero with no signal, the amplifier turns off.

The default threshold is **-60 dBFS**, which is suitable for typical line-level audio sources. The range is -96 dBFS (extremely sensitive — responds to near-silence) to 0 dBFS (requires full-scale signal, practically never triggers).

:::tip How to choose the right threshold
Start with the default -60 dBFS and observe the behaviour:
- If the amplifier triggers on background hiss or cable noise when no music is playing, raise the threshold (for example, try -50 dBFS or -40 dBFS).
- If the amplifier does not turn on reliably for quiet passages or low-level sources, lower the threshold (for example, try -70 dBFS).

The live level meter in the Control tab shows the current measured dBFS level in real time, which makes it easy to find a good threshold value.
:::

:::warning Very sensitive settings can trigger on noise
Setting the threshold very low (such as -90 dBFS) may cause the amplifier to turn on from electrical noise alone when no audio source is connected. If you experience phantom triggering, raise the threshold until it is above your noise floor.
:::

---

## How the Auto-Off Timer Works

The auto-off timer controls how long the controller waits after the signal drops below the threshold before turning the amplifier off. It is expressed in minutes and defaults to **15 minutes**.

Here is the exact behaviour:

1. **Signal detected:** Audio level exceeds the threshold. Amplifier turns on. Timer is loaded with the full configured duration and begins counting down.
2. **Signal continues:** As long as the audio level stays above the threshold, the timer resets to full on every detection pass. The countdown effectively pauses while music is playing.
3. **Signal drops:** Audio level falls below the threshold. The countdown continues downward in real time.
4. **Signal returns:** If audio is detected again before the timer reaches zero, the timer resets to full and the amplifier stays on.
5. **Timer expires:** The countdown reaches zero with no signal detected. The amplifier relay opens and the amplifier turns off.
6. **Standby state:** The timer is paused at zero. The system waits for the next detection event. When audio is detected again, the full timer duration is reloaded and the cycle begins again.

:::info The timer only counts down when the amplifier is on
If you switch to Smart Auto mode while the amplifier is off (because no signal was detected at startup), the timer display shows the configured duration but is not counting. It will only start counting once the signal is first detected and the amplifier turns on.
:::

---

## Configuring via the Web Interface

All Smart Sensing settings are on the **Control** tab.

1. Open the web interface in your browser (for example, http://192.168.1.42).
2. Click the **Control** tab.
3. Under **Sensing Mode**, select **Smart Auto**, **Always On**, or **Always Off** from the dropdown. The change takes effect immediately.
4. In the **Threshold** field, enter your desired dBFS value. The live level meter nearby shows the current signal level to help you choose.
5. In the **Timer Duration** field, enter how many minutes of silence should pass before the amplifier turns off (1–60 minutes).
6. Changes to the threshold and timer take effect as soon as you finish typing and move focus away from the field (or press Enter). No separate save button is needed.

:::tip Adjusting while the timer is running
You can change the timer duration at any time, even while the countdown is active. If the amplifier is currently on and counting down, the timer immediately resets to the new duration and continues from there. This is handy for extending the timeout on the fly without switching modes.
:::

:::info Live updates while you type
The web interface updates its displayed values from the controller every second via a live connection. To prevent your typing from being overwritten mid-entry, updates to a field are paused while you have that field focused. The field resumes updating normally once you click away.
:::

---

## Configuring via MQTT

If MQTT is configured, you can read and control Smart Sensing remotely using the following topics. Replace `{prefix}` with your configured MQTT base topic (configurable in the MQTT tab; defaults to `ALX/{serial-number}`).

| Topic | Direction | Description |
|---|---|---|
| `{prefix}/state` | Published by controller | Current amplifier state (`ON` or `OFF`) |
| `{prefix}/set` | Send to controller | Set amplifier state (`ON` or `OFF`) |
| `{prefix}/mode` | Published by controller | Current sensing mode (`always_on`, `always_off`, `smart_auto`) |
| `{prefix}/mode/set` | Send to controller | Set sensing mode (`always_on`, `always_off`, `smart_auto`) |
| `{prefix}/availability` | Published by controller | Controller online status (`online` or `offline`) |

**Switching modes via MQTT:**
- Send `smart_auto` to `{prefix}/mode/set` to enable automatic detection.
- Send `always_on` to `{prefix}/mode/set` to force the amplifier on.
- Send `always_off` to `{prefix}/mode/set` to force the amplifier off.

**Overriding the amplifier state in Smart Auto mode:**
- Send `ON` or `OFF` to `{prefix}/set` to directly control the relay. This works in any mode.

:::tip Home Assistant integration
When Home Assistant Discovery is enabled in the MQTT tab, the ALX Nova automatically registers itself as a device in Home Assistant. You will find it under Settings > Devices and Services > MQTT. From there you can add it to dashboards, create automations, and see its state in the HA energy monitor.
:::

---

## Display and Indicator Feedback

If your ALX Nova has a TFT display fitted, the Home screen shows the current state at a glance:

- **Sensing mode:** The active mode (Always On, Always Off, or Smart Auto) is shown on the Home screen.
- **Amplifier state:** A visual indicator shows whether the amplifier relay is currently on or off.
- **Timer countdown:** When Smart Auto mode is active and the timer is counting down, the remaining time is displayed in real time.
- **Audio level:** A live VU-style level indicator shows the current signal level so you can see exactly what the controller is measuring.

The **LED** (GPIO 1) reflects overall device state. It does not blink by default — steady on means the device is running normally.

---

## Tips for Optimal Configuration

**Matching the threshold to your source**

Different audio sources have very different output levels. A streaming device or phone might output at a moderate level, while a turntable with a phono stage may be much lower or higher. Use the live level meter in the Control tab while the source is playing to see where the signal lands, then set the threshold a few dBFS below that level.

**Choosing an appropriate timer duration**

Consider how you typically use your audio setup:
- For background music during work or meals, 15–20 minutes is usually comfortable. If there is a natural break mid-session, the amplifier will not turn off prematurely.
- For focused listening sessions with intentional pauses, 5–10 minutes gives you a tighter experience.
- For home cinema use where there are long quiet passages in films, set the timer to 30 minutes or more to avoid the amplifier cycling during dialogue scenes.

**Multi-source setups**

The ALX Nova monitors all active input lanes simultaneously for signal detection. If you have multiple sources connected (for example, a streaming device and a TV), the amplifier turns on if any of them is producing signal. You do not need to select an active input — the controller aggregates across all lanes.

:::tip Starting fresh with settings
If you want to experiment with different configurations, the Settings tab allows you to export your current settings as a backup first. If the new settings do not work well, you can restore the export to get back to where you started.
:::
