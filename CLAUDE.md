# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 based intelligent amplifier controller (ALX Nova) with smart auto-sensing, WiFi management, MQTT/Home Assistant integration, OTA firmware updates, a web configuration interface, and an LVGL-based GUI on a ST7735S TFT display with rotary encoder input. Built with PlatformIO and the Arduino framework. Current firmware version is defined in `src/config.h` as `FIRMWARE_VERSION`.

## Build & Test Commands

```bash
# Build firmware for ESP32-S3
pio run

# Upload firmware to device
pio run --target upload

# Monitor serial output (9600 baud)
pio device monitor

# Run all unit tests (native platform, no hardware needed)
pio test -e native

# Run a specific test module
pio test -e native -f test_wifi
pio test -e native -f test_mqtt
pio test -e native -f test_auth

# Verbose test output
pio test -e native -v
```

Tests run on the `native` environment (host machine with gcc/MinGW) using the Unity framework (376 tests). Mock implementations of Arduino, WiFi, MQTT, and Preferences libraries live in `test/test_mocks/`. Test modules: `test_utils`, `test_auth`, `test_wifi`, `test_mqtt`, `test_settings`, `test_ota`, `test_ota_task`, `test_button`, `test_websocket`, `test_api`, `test_smart_sensing`, `test_buzzer`, `test_gui_home`, `test_gui_input`, `test_gui_navigation`, `test_pinout`, `test_i2s_audio`, `test_fft`, `test_signal_generator`, `test_audio_diagnostics`, `test_vrms`, `test_dim_timeout`.

## Architecture

### State Management — AppState Singleton
All application state lives in `src/app_state.h` as a singleton accessed via `AppState::getInstance()` or the `appState` macro. It uses **dirty flags** (e.g., `isAmplifierDirty()`, `isTimerDirty()`) to detect changes and minimize unnecessary WebSocket broadcasts and MQTT publishes.

Legacy code uses `#define` macros (e.g., `#define wifiSSID appState.wifiSSID`) to alias global variable names to AppState members. New code should use `appState.memberName` directly.

### FSM States
The application uses a finite state machine (`AppFSMState` in `app_state.h`): `STATE_IDLE`, `STATE_SIGNAL_DETECTED`, `STATE_AUTO_OFF_TIMER`, `STATE_WEB_CONFIG`, `STATE_OTA_UPDATE`, `STATE_ERROR`.

### Handler/Module Pattern
Each subsystem is a separate module in `src/`:
- **smart_sensing** — Voltage detection, auto-off timer, amplifier relay control
- **wifi_manager** — Multi-network WiFi client, AP mode, async connection with retry/backoff
- **mqtt_handler** — MQTT broker connection, Home Assistant discovery, heartbeat publishing
- **ota_updater** — GitHub release checking, firmware download with SHA256 verification
- **settings_manager** — NVS/Preferences persistence, export/import, factory reset
- **auth_handler** — Session token management, web password authentication
- **button_handler** — Debouncing, short/long/very-long press and multi-click detection
- **buzzer_handler** — Piezo buzzer with multi-pattern sequencer, ISR-safe encoder tick/click, volume control, FreeRTOS mutex for dual-core safety
- **i2s_audio** — Dual PCM1808 I2S ADC driver, RMS/dBFS, VU metering, peak hold, waveform downsampling, FFT spectrum analysis, audio health diagnostics
- **signal_generator** — Multi-waveform test signal generator (sine, square, noise, sweep), software injection + PWM output modes
- **websocket_handler** — Real-time state broadcasting to web clients (port 81)
- **web_pages** — Embedded HTML/CSS/JS served from the ESP32 (gzip-compressed in `web_pages_gz.cpp`). **IMPORTANT: After ANY edit to `web_pages.cpp`, you MUST run `node build_web_assets.js` to regenerate `web_pages_gz.cpp` before building firmware.** The ESP32 serves the gzipped version — without this step, frontend changes will not take effect.

### GUI (LVGL on TFT Display)
LVGL v9.4 + TFT_eSPI on ST7735S 128x160 (landscape 160x128). Runs on Core 1 via FreeRTOS `gui_task`. All GUI code is guarded by `-D GUI_ENABLED`.

Key GUI modules in `src/gui/`:
- **gui_manager** — Init, FreeRTOS task, screen sleep/wake, dashboard refresh
- **gui_input** — ISR-driven rotary encoder (Gray code state machine)
- **gui_theme** — Orange accent theme, dark/light mode
- **gui_navigation** — Screen stack with push/pop and transition animations
- **screens/** — Desktop carousel, Home status, Control, WiFi, MQTT, Settings, Debug, Support, Boot animations, Keyboard, Value editor

### Web Server
HTTP server on port 80 with REST API endpoints under `/api/`. WebSocket server on port 81 for real-time updates. API endpoints are registered in `main.cpp`.

### FreeRTOS Tasks
Concurrent tasks with configurable stack sizes and priorities defined in `src/config.h` (`TASK_STACK_SIZE_*`, `TASK_PRIORITY_*`). Main loop runs on Core 0; GUI task runs on Core 1. OTA update check and download run as one-shot tasks on Core 1 via `startOTACheckTask()` / `startOTADownloadTask()` in `src/ota_updater.cpp`. Cross-core communication uses dirty flags in AppState — GUI/OTA tasks set flags, main loop handles WebSocket/MQTT broadcasts.

## Pin Configuration

Defined as build flags in `platformio.ini` and with fallback defaults in `src/config.h`:
- Core: LED=2, Amplifier=4, VoltSense(ADC)=1, Reset button=15, Buzzer=8
- TFT: MOSI=11, SCLK=12, CS=10, DC=13, RST=14, BL=21
- Encoder: A=5, B=6, SW=7
- I2S Audio ADC1: BCK=16, DOUT=17, LRC=18, MCLK=3
- I2S Audio ADC2: DOUT2=19 (shares BCK/LRC/MCLK with ADC1)
- Signal Generator PWM: GPIO 38

## Testing Conventions

- Tests use Arrange-Act-Assert pattern
- Each test file has a `setUp()` that resets state
- Mock headers in `test/test_mocks/` simulate Arduino functions (`millis()`, `analogRead()`, GPIO), WiFi, MQTT (`PubSubClient`), and NVS (`Preferences`)
- The `native` environment compiles with `-D UNIT_TEST -D NATIVE_TEST` flags — use these for conditional compilation
- `test_build_src = no` in platformio.ini means tests don't compile `src/` directly; they include specific headers and use mocks
- Each test module must be in its own directory to avoid duplicate `main`/`setUp`/`tearDown` symbols

## Commit Convention

```
feat: Add new feature
fix: Fix bug
docs: Update documentation
refactor: Code refactoring
test: Add/update tests
chore: Maintenance tasks
```

## CI/CD

GitHub Actions (`.github/workflows/tests.yml`): runs all native tests, then builds ESP32-S3 firmware. Triggers on push/PR to `main` and `develop` branches. A separate `release.yml` workflow handles automated releases.

## Serial Debug Logging

All modules use `debug_serial.h` macros (`LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`) with consistent `[ModuleName]` prefixes:

| Module | Prefix | Notes |
|---|---|---|
| `smart_sensing` | `[Sensing]` | Mode changes, threshold, timer, amplifier state, ADC health transitions |
| `i2s_audio` | `[Audio]` | Init, sample rate changes, periodic dBFS dump (5s), ADC detection changes |
| `signal_generator` | `[SigGen]` | Init, start/stop, PWM duty, param changes while active |
| `buzzer_handler` | `[Buzzer]` | Init, pattern start/complete, play requests (excludes tick/click to avoid noise) |
| `wifi_manager` | `[WiFi]` | Connection attempts, AP mode, scan results |
| `mqtt_handler` | `[MQTT]` | Connect/disconnect, HA discovery, publish errors |
| `ota_updater` | `[OTA]` | Version checks, download progress, verification |
| `settings_manager` | `[Settings]` | Load/save operations |
| `button_handler` | — | Logged from `main.cpp` (11 LOG calls covering all press types) |
| `gui_*` | `[GUI]` | Navigation, screen transitions, theme changes |

When adding logging to new modules, follow these conventions:
- Use `LOG_I` for state transitions and significant events (start/stop, connect/disconnect, health changes)
- Use `LOG_D` for high-frequency operational details (pattern steps, param snapshots)
- Never log inside ISR paths — use flags and log from the main-loop or task context
- Log transitions, not repetitive state (use static `prev` variables to detect changes)

## Key Dependencies

- `ArduinoJson@^7.4.2` — JSON parsing throughout the codebase
- `WebSockets@^2.7.2` — WebSocket server for real-time UI updates
- `PubSubClient@^2.8` — MQTT client for Home Assistant integration
- `lvgl@^9.4` — GUI framework (guarded by `GUI_ENABLED`)
- `TFT_eSPI@^2.5.43` — TFT display driver for ST7735S
