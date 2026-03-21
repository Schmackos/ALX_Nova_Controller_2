---
title: Developer Overview
sidebar_position: 1
description: Developer overview of the ALX Nova Controller 2 firmware and architecture.
---

# Developer Overview

ALX Nova Controller 2 is an ESP32-P4 based intelligent amplifier controller. It combines real-time audio processing, HAL-managed hardware abstraction, a web configuration interface, MQTT/Home Assistant integration, OTA firmware updates, and an LVGL-based TFT GUI — all built with PlatformIO and the Arduino framework on top of ESP-IDF 5.

This page is an entry point for developers contributing to or integrating with the firmware. It covers the tech stack, repository layout, and a map of all major subsystems.

:::info
The current firmware version is defined in `src/config.h` as `FIRMWARE_VERSION`. The target board is the **Waveshare ESP32-P4-WiFi6-DEV-Kit** (`esp32-p4` in PlatformIO). Upload and monitor port defaults to **COM8**.
:::

---

## Tech Stack

| Layer | Technology | Notes |
|---|---|---|
| MCU | ESP32-P4 (RISC-V, dual-core) | 400 MHz, 32 MB PSRAM, 16 MB Flash |
| Framework | Arduino (ESP-IDF 5 underneath) | via pioarduino platform |
| Build system | PlatformIO | `platformio.ini` defines all environments |
| RTOS | FreeRTOS | Multi-core task isolation; Core 1 reserved for audio |
| Display GUI | LVGL v9.4 + LovyanGFX | ST7735S 128x160 TFT, landscape orientation |
| Audio DSP | ESP-DSP (pre-built `.a`) | Biquad IIR, FIR, FFT, vector math |
| Persistence | LittleFS + NVS (Preferences) | `/config.json` primary; NVS for WiFi credentials |
| Web UI | Vanilla HTML/CSS/JS (gzip-embedded) | Assembled from `web_src/` by `tools/build_web_assets.js` |
| Messaging | WebSocket (port 81) + MQTT | Real-time state; Home Assistant discovery |
| Auth | PBKDF2-SHA256 (10,000 iter) | HttpOnly session cookie; WS token pool |
| OTA | GitHub releases + SHA256 | Dual OTA partitions (4 MB each) |
| Unit tests | Unity framework, native platform | 2,316 tests across 87 modules, no hardware needed |
| E2E tests | Playwright + Express mock server | 107 browser tests across 22 specs |

---

## Repository Layout

```
ALX_Nova_Controller_2/
├── src/                    # Firmware source (C++)
│   ├── state/              # AppState domain headers (15 lightweight structs)
│   ├── hal/                # Hardware Abstraction Layer
│   ├── gui/                # LVGL GUI (guarded by GUI_ENABLED)
│   │   └── screens/        # Individual LVGL screens
│   ├── drivers/            # Low-level peripheral drivers (ES8311, PCM5102)
│   ├── main.cpp            # setup() + loop() entry points
│   ├── app_state.h/.cpp    # AppState singleton shell (composes domain structs)
│   ├── app_events.h/.cpp   # FreeRTOS event group (16 EVT_* bits)
│   ├── config.h            # Board pins, FIRMWARE_VERSION, task config
│   └── ...                 # Module source files
│
├── test/                   # C++ unit tests (PlatformIO native env)
│   ├── test_mocks/         # Arduino, WiFi, MQTT, Preferences mock headers
│   └── test_<module>/      # One directory per test module
│
├── web_src/                # Web UI source (edit here, not src/web_pages.cpp)
│   ├── index.html          # HTML shell
│   ├── css/                # CSS split by concern (01-05-*.css)
│   └── js/                 # JS modules in load order (01-28-*.js)
│
├── e2e/                    # Playwright browser tests
│   ├── tests/              # Test specs (*.spec.js)
│   ├── mock-server/        # Express server + WS mock
│   ├── helpers/            # Fixtures, selectors, WS helpers
│   └── fixtures/           # JSON WS messages and API responses
│
├── lib/                    # Vendored libraries
│   ├── WebSockets/         # WebSocket server (vendored, not from registry)
│   └── esp_dsp_lite/       # ANSI C DSP fallback for native tests
│
├── tools/                  # Build and analysis scripts
│   ├── build_web_assets.js # Assembles web_src/ → src/web_pages.cpp
│   ├── find_dups.js        # Detects duplicate JS declarations
│   └── check_missing_fns.js # Detects undefined JS function references
│
├── .githooks/              # Pre-commit hooks
│   └── pre-commit          # Runs find_dups, check_missing_fns, ESLint
│
├── .github/workflows/      # CI/CD (tests.yml, release.yml)
├── platformio.ini          # PlatformIO build configuration
└── CLAUDE.md               # Full project reference for AI-assisted development
```

---

## Key Subsystems

Each subsystem is implemented as a pair of `.h`/`.cpp` files in `src/`. The table below gives a one-line description of every major module.

### Application Core

| Module | Files | Description |
|---|---|---|
| `app_state` | `src/app_state.h/.cpp` | Singleton composing 15 domain state structs; dirty-flag setters wake the main loop |
| `app_events` | `src/app_events.h/.cpp` | FreeRTOS event group wrapping 16 `EVT_*` bits for cross-task wakeup |
| `main` | `src/main.cpp` | `setup()` init sequence and `loop()` dirty-flag dispatch |
| `config` | `src/config.h` | Pin definitions, `FIRMWARE_VERSION`, task stack/priority constants |
| `settings_manager` | `src/settings_manager.h/.cpp` | Dual-format persistence (`/config.json` primary, legacy fallback, atomic write) |
| `auth_handler` | `src/auth_handler.h/.cpp` | PBKDF2 session auth, WS token pool, rate limiting |
| `debug_serial` | `src/debug_serial.h/.cpp` | `LOG_D/I/W/E` macros, WS log forwarding, runtime level control |
| `crash_log` | `src/crash_log.h/.cpp` | Boot-loop detection (3 crashes → safe mode), crash persistence |
| `task_monitor` | `src/task_monitor.h/.cpp` | FreeRTOS task stack watermark enumeration |

### Audio Pipeline

| Module | Files | Description |
|---|---|---|
| `audio_pipeline` | `src/audio_pipeline.h/.cpp` | 8-lane input → 16×16 matrix → 8-slot output; Core 1 real-time task |
| `i2s_audio` | `src/i2s_audio.h/.cpp` | Dual PCM1808 I2S ADC driver; FFT/waveform/RMS/VU analysis |
| `dsp_pipeline` | `src/dsp_pipeline.h/.cpp` | Per-input biquad IIR/FIR/limiter/compressor chain; double-buffered swap |
| `output_dsp` | `src/output_dsp.h/.cpp` | Per-output mono DSP engine applied post-matrix, pre-sink |
| `dsp_coefficients` | `src/dsp_biquad_gen.h` | RBJ Audio EQ Cookbook coefficient computation |
| `dsp_crossover` | `src/dsp_crossover.h/.cpp` | LR2/LR4/LR8 and Butterworth crossover presets |
| `dsp_rew_parser` | `src/dsp_rew_parser.h/.cpp` | Equalizer APO / miniDSP import, FIR WAV loading |
| `dsp_api` | `src/dsp_api.h/.cpp` | REST CRUD for DSP config; LittleFS persistence |
| `pipeline_api` | `src/pipeline_api.h/.cpp` | REST API for audio pipeline matrix CRUD and per-output DSP config |
| `dac_api` | `src/dac_api.h/.cpp` | REST API for DAC state, capabilities, volume, and enable/disable |
| `sink_write_utils` | `src/sink_write_utils.h/.cpp` | Shared float buffer utilities for all HAL sink drivers (volume, mute ramp, I2S conversion) |
| `signal_generator` | `src/signal_generator.h/.cpp` | Sine/square/noise/sweep generator; HAL-assigned lane |
| `usb_audio` | `src/usb_audio.h/.cpp` | TinyUSB UAC2 speaker device; SPSC ring buffer; guarded by `USB_AUDIO_ENABLED` |

### HAL Framework (`src/hal/`)

| Module | Files | Description |
|---|---|---|
| `hal_device_manager` | `hal_device_manager.h/.cpp` | Singleton managing up to 24 devices; priority init; pin conflict prevention |
| `hal_pipeline_bridge` | `hal_pipeline_bridge.h/.cpp` | Translates HAL state transitions into audio pipeline source/sink registration |
| `hal_discovery` | `hal_discovery.h/.cpp` | 3-tier discovery: I2C bus scan → EEPROM probe → manual config |
| `hal_device_db` | `hal_device_db.h/.cpp` | In-memory device database with LittleFS JSON persistence |
| `hal_builtin_devices` | `hal_builtin_devices.h/.cpp` | Compatible-string → factory function driver registry |
| `hal_api` | `hal_api.h/.cpp` | REST endpoints for HAL device CRUD (`/api/hal/`) |
| `hal_es8311` | `hal_es8311.h/.cpp` | ES8311 codec driver (I2C bus 1, I2S2 TX) |
| `hal_pcm5102a` | `hal_pcm5102a.h/.cpp` | PCM5102A DAC driver (I2S) |
| `hal_pcm1808` | `hal_pcm1808.h/.cpp` | PCM1808 ADC driver (dual I2S master) |
| `hal_ns4150b` | `hal_ns4150b.h/.cpp` | NS4150B class-D amp enable/disable |
| `hal_relay` | `hal_relay.h/.cpp` | GPIO relay control for amplifier; used by smart_sensing via `findByCompatible` |
| `hal_siggen` | `hal_siggen.h/.cpp` | Signal generator as HAL ADC device (`alx,signal-gen`) |
| `hal_usb_audio` | `hal_usb_audio.h/.cpp` | USB Audio as HAL ADC device (`alx,usb-audio`) |
| `hal_mcp4725` | `hal_mcp4725.h/.cpp` | MCP4725 12-bit DAC (I2C) |
| `hal_temp_sensor` | `hal_temp_sensor.h/.cpp` | ESP32-P4 internal temperature sensor (IDF5 driver) |
| `hal_eeprom_v3` | `hal_eeprom_v3.h/.cpp` | EEPROM v3 device identity probe |
| `hal_dsp_bridge` | `hal_dsp_bridge.h/.cpp` | HAL-side DSP metrics and VU level accessor |
| `hal_ess_sabre_adc_base` | `hal_ess_sabre_adc_base.h/.cpp` | Abstract base class for ESS SABRE ADC family drivers (shared I2C helpers, config overrides) |
| `hal_es9822pro` | `hal_es9822pro.h/.cpp` | ESS ES9822PRO 2-channel SABRE ADC (125 dB, PGA 0–18 dB, HPF) |
| `hal_es9843pro` | `hal_es9843pro.h/.cpp` | ESS ES9843PRO 4-channel TDM SABRE ADC (125 dB, PGA 0–42 dB) |
| `hal_es9826` | `hal_es9826.h/.cpp` | ESS ES9826 2-channel SABRE ADC (123 dB, PGA 0–30 dB in 3 dB steps) |
| `hal_es9823pro` | `hal_es9823pro.h/.cpp` | ESS ES9823PRO / ES9823MPRO 2-channel SABRE ADC (128 dB, chip-ID variant detection) |
| `hal_es9821` | `hal_es9821.h/.cpp` | ESS ES9821 2-channel SABRE ADC (120 dB, no PGA) |
| `hal_es9820` | `hal_es9820.h/.cpp` | ESS ES9820 2-channel SABRE ADC (116 dB, PGA 0–18 dB) |
| `hal_es9842pro` | `hal_es9842pro.h/.cpp` | ESS ES9842PRO 4-channel TDM SABRE ADC (122 dB, PGA 0–18 dB) |
| `hal_es9840` | `hal_es9840.h/.cpp` | ESS ES9840 4-channel TDM SABRE ADC (116 dB, identical register map to ES9842PRO) |
| `hal_es9841` | `hal_es9841.h/.cpp` | ESS ES9841 4-channel TDM SABRE ADC (122 dB, PGA 0–42 dB, 8-bit per-channel volume) |
| `hal_tdm_deinterleaver` | `hal_tdm_deinterleaver.h/.cpp` | TDM frame splitter for 4-channel ADCs; ping-pong buffers, 2 concurrent instances |

### Networking & Communication

| Module | Files | Description |
|---|---|---|
| `wifi_manager` | `src/wifi_manager.h/.cpp` | Multi-network client, AP mode, async retry/backoff |
| `eth_manager` | `src/eth_manager.h/.cpp` | 100 Mbps Ethernet via ESP32-P4 EMAC |
| `mqtt_handler` | `src/mqtt_handler.h/.cpp` | MQTT lifecycle, settings, callback dispatch |
| `mqtt_publish` | `src/mqtt_publish.cpp` | All `publishMqtt*()` functions; change-detection shadow fields |
| `mqtt_ha_discovery` | `src/mqtt_ha_discovery.cpp` | Home Assistant MQTT discovery (~1880 lines) |
| `mqtt_task` | `src/mqtt_task.h/.cpp` | Dedicated Core 0 task: reconnect + publish at 20 Hz |
| `websocket_handler` | `src/websocket_handler.h/.cpp` | WS broadcast server port 81; binary audio frames |
| `ota_updater` | `src/ota_updater.h/.cpp` | GitHub release check, firmware download, SHA256 verify |

### GUI (`src/gui/`)

| Module | Files | Description |
|---|---|---|
| `gui_manager` | `gui_manager.h/.cpp` | LVGL init, Core 0 FreeRTOS task, screen sleep/wake |
| `gui_input` | `gui_input.h/.cpp` | ISR-driven rotary encoder (Gray code state machine) |
| `gui_theme` | `gui_theme.h/.cpp` | Orange accent theme, dark/light mode |
| `gui_navigation` | `gui_navigation.h/.cpp` | Screen stack with push/pop and transition animations |
| `screens/` | `screens/*.h/.cpp` | Individual LVGL screens (desktop, home, control, WiFi, MQTT, DSP, settings, debug, keyboard, value editor, boot animation) |

### Diagnostics

| Module | Files | Description |
|---|---|---|
| `diag_journal` | `src/diag_journal.h/.cpp` | PSRAM ring buffer (32 entries) + LittleFS journal (800 entries) |
| `diag_event` | `src/diag_event.h` | `DiagEvent` struct with severity, error code, correlation ID |
| `diag_error_codes` | `src/diag_error_codes.h` | All `DIAG_*` error code constants |
| `hal_audio_health_bridge` | `src/hal/hal_audio_health_bridge.h/.cpp` | Maps ADC health transitions to HAL state with flap guard |

### Memory Management

| Module | Files | Description |
|---|---|---|
| `psram_alloc` | `src/psram_alloc.h/.cpp` | Unified PSRAM allocation wrapper with SRAM fallback, diagnostic emission, and heap budget recording |
| `psram_api` | `src/psram_api.h/.cpp` | REST endpoint `GET /api/psram/status` for PSRAM health and per-subsystem allocation breakdown |
| `heap_budget` | `src/heap_budget.h/.cpp` | Per-subsystem allocation tracker (32 entries, label/bytes/isPsram); exposed via WS and REST |

---

## AppState Domain Decomposition

Application state is decomposed across 15 lightweight domain structs in `src/state/`, composed into the `AppState` singleton:

```cpp
// Access pattern examples
appState.wifi.ssid           // WifiState
appState.audio.adcEnabled[i] // AudioState
appState.dac.txUnderruns         // DacState (minimal: underruns + EEPROM diag)
appState.halCoord.requestDeviceToggle(slot, 1)  // HalCoordState (device toggle queue)
appState.dsp.enabled         // DspSettingsState
appState.general.darkMode    // GeneralState
appState.mqtt.brokerHost     // MqttState
```

The `appState` macro resolves to `AppState::getInstance()`. Dirty flags and event signaling remain on the AppState shell for backward compatibility.

---

## Feature Flags

Three build flags gate optional subsystems. All three are enabled in the `esp32-p4` PlatformIO environment.

| Flag | Subsystem Enabled |
|---|---|
| `-D DAC_ENABLED` | HAL framework, all DAC/ADC drivers, pipeline bridge, HAL REST API |
| `-D GUI_ENABLED` | LVGL GUI, TFT display driver, rotary encoder |
| `-D USB_AUDIO_ENABLED` | TinyUSB UAC2 speaker device, USB audio HAL driver |
| `-D DSP_ENABLED` | DSP pipeline, output DSP, DSP REST API |

The `native` test environment enables `UNIT_TEST` and `NATIVE_TEST`, which swap hardware drivers for mock implementations in `test/test_mocks/`.
