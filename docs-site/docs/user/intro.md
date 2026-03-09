---
title: Introduction
sidebar_position: 1
description: Overview of the ALX Nova Controller 2 — ESP32-P4 intelligent amplifier controller.
---

# Welcome to ALX Nova

The ALX Nova Controller 2 is an intelligent amplifier controller built around the **Waveshare ESP32-P4-WiFi6-DEV-Kit**. It sits between your audio source and your power amplifier and makes your hi-fi system genuinely smart — automatically waking the amp when music is playing, putting it to sleep when silence returns, and giving you a rich set of controls through a web browser, Home Assistant, or the front-panel display.

Whether you run a stereo pair, a multi-zone setup with external DACs, or a home-theatre system, the ALX Nova is designed to stay out of your way until you need it.

---

## What ALX Nova Does

### Smart Auto-Sensing

ALX Nova listens to your audio inputs using a pair of high-quality PCM1808 I2S ADCs. When it hears a signal above your chosen threshold it turns the amplifier on. When the music stops, a configurable countdown timer runs in the background — and only after the silence has lasted that long does the amp switch off. Your system is always ready when you need it and never left running overnight by accident.

:::tip
The default threshold is –60 dBFS and the default timer is 15 minutes. Both can be tuned to match your source and listening habits from the web interface.
:::

### Hardware Abstraction Layer (Multi-Device Support)

ALX Nova uses a flexible device framework (the HAL) that can manage several audio devices on the same controller simultaneously:

- **PCM5102A** — external stereo I2S DAC
- **ES8311** — onboard codec with hardware volume control
- **PCM1808 x2** — dual stereo ADC inputs (up to 4 input channels)
- **MCP4725** — I2C DAC for analogue control voltage outputs
- Additional devices can be discovered and configured from the web UI without reflashing firmware

### DSP Engine

A 16×16 digital routing matrix connects up to 8 stereo input sources to up to 8 output sinks. Per-channel DSP processing is available on both the input and output sides:

- 10-band parametric equaliser (PEQ) with RBJ biquad filters
- Crossover presets (Linkwitz-Riley 2nd/4th/8th order, Butterworth)
- FIR convolution (up to 256 taps, supports REW and miniDSP import)
- Compressor, limiter, delay (up to 100 ms), gain, polarity invert, and mute per channel

### Web Interface

A self-contained web application is served directly from the controller on port 80. No cloud account, no app to install — just open a browser. The interface includes:

- Audio input channel strips with real-time VU meters
- Routing matrix for flexible signal routing
- Full DSP overlay panels (PEQ graph, compressor, crossover)
- WiFi, MQTT, System, and Debug tabs
- Dark and light themes with responsive layout for phones and desktops

### MQTT & Home Assistant

ALX Nova publishes its state to any MQTT broker and supports Home Assistant auto-discovery. Within seconds of saving your broker address, entities for amplifier control, sensing mode, timer, and audio levels appear in Home Assistant ready to use in automations and dashboards.

### OTA Firmware Updates

Updates are fetched directly from the project's GitHub Releases page over HTTPS. Every firmware binary is verified with SHA256 before installation. You can trigger an update manually from the web interface, or the controller checks automatically every 5 minutes when connected to the internet.

### TFT Display and Rotary Encoder

A 1.8-inch 160×128 ST7735S TFT display and an EC11 rotary encoder give you local control without opening a browser. A desktop carousel lets you navigate between status screens, and all key settings are reachable from the on-screen menus.

### Ethernet + WiFi

The ESP32-P4 supports both **WiFi 6** (via the onboard ESP32-C6 co-processor) and **100 Mbps Ethernet**. You can run the controller wired for stability and still use the WiFi access point for initial setup or mobile access.

---

## Who Is This For?

ALX Nova is aimed at audio enthusiasts who want:

- **Automation without compromise** — genuine I2S digital audio sensing, not a crude analogue voltage comparator
- **Integration with a smart home** — first-class Home Assistant support out of the box
- **Flexibility** — route any input to any output, add DSP to any path, hot-swap devices
- **Self-hosting** — everything runs locally on the device; no subscription, no cloud dependency

You do not need to know how to write firmware to use ALX Nova. The full feature set is available through the web interface and this documentation.

---

## Key Specifications at a Glance

| Feature | Detail |
|---|---|
| Processor | ESP32-P4 (dual-core 400 MHz) |
| Wireless | WiFi 6 (via ESP32-C6) + 100 Mbps Ethernet |
| Audio inputs | Up to 8 stereo lanes via HAL (2 onboard PCM1808 ADCs standard) |
| Audio outputs | Up to 8 stereo sinks via HAL |
| DSP matrix | 16×16 routing, 10-band PEQ + full DSP chain per channel |
| Web server | Port 80 (HTTP), Port 81 (WebSocket) |
| MQTT | Configurable broker, HA auto-discovery |
| OTA | HTTPS from GitHub Releases, SHA256 verified |
| Display | 160×128 ST7735S TFT with LVGL UI |
| Firmware version | 1.12.1 |

---

## Next Steps

- [Getting Started](./getting-started.md) — first power-on, connecting to WiFi, finding your device
- [Web Interface](./web-interface.md) — a tab-by-tab walkthrough of the browser UI
- [Button Controls](./button-controls.md) — what the physical button and encoder do
- [MQTT & Home Assistant](./mqtt-home-assistant.md) — smart home integration
