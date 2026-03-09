---
title: Web Interface
sidebar_position: 3
description: Using the ALX Nova web configuration interface.
---

# Web Interface

The ALX Nova serves a full web application directly from the device — no app download, no cloud account, no internet connection required. Open any modern browser on your phone or desktop and navigate to your controller's IP address.

:::info
The web server runs on **port 80**. If your browser adds `https://` automatically, remove it — the controller uses plain HTTP on your local network. The WebSocket for real-time updates uses **port 81** automatically in the background.
:::

---

## Accessing the Interface

1. Make sure your device or phone is on the same network as the ALX Nova.
2. Find the controller's IP address (check your router's DHCP client list, or look at the TFT display's Home screen).
3. Type that address into your browser — for example `http://192.168.1.45`.
4. Log in with your password. On a fresh device the one-time password is shown on the TFT display and on the serial console at boot.

:::tip
Bookmark the IP address for quick access. If your router supports mDNS/Bonjour, you may also be able to reach the controller by hostname.
:::

---

## Layout Overview

The interface is organised as a sidebar with named tabs on the left (desktop) or a hamburger menu (mobile). All tabs update in real time via WebSocket — you do not need to refresh the page to see current values.

A **dark/light mode** toggle is available in the top-right corner. Your preference is remembered between visits.

---

## Audio Tab

The Audio tab is the heart of the interface. It is divided into four sub-views reachable from the horizontal sub-navigation bar:

### Inputs

Each active input source appears as a **channel strip** showing:

- Source name (e.g. PCM1808 Left, Signal Generator, USB Audio)
- Real-time VU meter and dBFS reading
- Peak hold indicator
- A **DSP** button that opens the per-input DSP overlay

:::info
The number of input strips shown depends on how many input devices the HAL has initialised. If a device is not listed, check the HAL Devices section in System.
:::

### Routing Matrix

A visual grid maps input channels (rows) to output channels (columns). Click any cell to toggle that connection. The matrix supports up to 16×16 connections — an input can feed multiple outputs simultaneously, and an output can mix signals from several inputs.

:::tip
Changes to the matrix take effect immediately with no audio glitch. The current matrix is saved automatically and survives reboots.
:::

### Outputs

Each active output sink is shown as a channel strip with:

- Sink name (e.g. PCM5102A, ES8311 Codec)
- Real-time VU meter
- A **DSP** button that opens the per-output DSP overlay

### Signal Generator

A built-in test signal source can inject sine waves, square waves, pink noise, or frequency sweeps into any input lane. Controls include:

- Waveform type selector
- Frequency (Hz) and amplitude (dBFS) sliders
- Start / Stop toggle

---

## DSP Overlays

Pressing a **DSP** button on any input or output channel strip opens a full-screen overlay. All overlays share the same structure:

### Parametric Equaliser (PEQ)

- Up to 10 bands per channel
- Filter types: peaking, low shelf, high shelf, low-pass, high-pass, notch, all-pass
- Frequency, gain (dB), and Q controls per band
- Live frequency response graph updates as you drag handles

:::tip
You can import EQ curves directly from REW (Room EQ Wizard) or miniDSP exports using the **Import** button in the PEQ overlay.
:::

### Crossover

- Linkwitz-Riley 2nd, 4th, and 8th order presets
- Butterworth option
- Crossover frequency slider

### Compressor & Limiter

- Threshold, ratio, attack, and release controls
- Hard limiter ceiling for output protection

### Additional Processing

Per-channel controls for delay (0–100 ms), gain trim, polarity invert, and mute are available as toggle buttons below the PEQ graph.

---

## Settings Tab

### Appearance

Toggle between **Light** and **Dark** mode. The TFT display theme tracks this setting too.

### Backup and Restore

- **Export Settings** — downloads a JSON file containing all device configuration. Store this before a firmware update or factory reset.
- **Import Settings** — upload a previously exported file to restore configuration. The controller reboots to apply the settings.

:::warning
Importing settings overwrites everything including WiFi credentials and MQTT configuration. Make sure you are importing the right file.
:::

### Device Actions

- **Reboot** — soft-restarts the controller without erasing any settings.
- **Factory Reset** — erases all settings and returns to factory defaults. See [Troubleshooting](./troubleshooting.md) for the full reset procedure.

---

## WiFi Tab

The WiFi tab lets you manage network connections from the browser. See the dedicated [WiFi Configuration](./wifi-configuration.md) page for a full walkthrough.

At a glance:

- Current connection status, SSID, signal strength, and IP address
- Saved network list with priority order
- Scan and add new networks
- Access point (AP) enable/disable toggle and SSID/password fields
- Ethernet status (if a cable is connected)

---

## MQTT Tab

Configure the MQTT broker and Home Assistant integration here. See [MQTT & Home Assistant](./mqtt-home-assistant.md) for full details.

At a glance:

- MQTT enabled/disabled toggle
- Broker address, port, username, and password
- Base topic prefix
- Home Assistant auto-discovery toggle
- Live connection status indicator

---

## System Section

The System section groups hardware monitoring and advanced device management.

### Hardware Stats

Refreshed every 2 seconds:

- CPU usage (both cores)
- Free heap memory and PSRAM
- Uptime
- Internal chip temperature

### HAL Devices

A live list of every device managed by the Hardware Abstraction Layer. For each device you can see:

- Current lifecycle state (Available, Unavailable, Error, etc.)
- Device type and compatible string
- I2C/I2S pin assignments
- **Rescan** button to re-run device discovery
- **Reinitialise** button to recover a faulted device

:::info
Normally you will not need to use this panel. It is most useful when adding expansion hardware or diagnosing a device that failed to initialise.
:::

### OTA Updates

The OTA controls live here. See [OTA Updates](./ota-updates.md) for a complete walkthrough.

- Current firmware version
- Last check timestamp
- **Check for Updates** button
- Auto-update toggle
- Manual firmware upload area

---

## Debug Tab

The Debug tab is primarily for developers and advanced users diagnosing problems.

### Debug Console

A real-time log stream from the controller with:

- Log level filter chips (Debug, Info, Warning, Error)
- Module/category filter chips — each `[ModuleName]` prefix gets its own chip
- Text search with highlighted matches
- Entry count badges (red for errors, amber for warnings)
- Relative and absolute timestamp toggle
- All filter choices are remembered between sessions

### Health Dashboard

An at-a-glance view of all audio and HAL health indicators:

- Per-device health state grid
- Error counter per subsystem
- Recent diagnostic event log with timestamps and correlation IDs

:::tip
If the controller is behaving unexpectedly, open the Debug Console and switch the filter to **Warning** or **Error** to see only the relevant messages.
:::
