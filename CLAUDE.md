# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 based intelligent amplifier controller (ALX Nova) with smart auto-sensing, WiFi management, MQTT/Home Assistant integration, OTA firmware updates, and a web configuration interface. Built with PlatformIO and the Arduino framework. Current firmware version is defined in `src/config.h` as `FIRMWARE_VERSION`.

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

Tests run on the `native` environment (host machine with gcc/MinGW) using the Unity framework. Mock implementations of Arduino, WiFi, MQTT, and Preferences libraries live in `test/test_mocks/`. Test modules: `test_utils`, `test_auth`, `test_wifi`, `test_mqtt`, `test_settings`, `test_ota`, `test_button`, `test_websocket`, `test_api`, `test_smart_sensing`.

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
- **websocket_handler** — Real-time state broadcasting to web clients (port 81)
- **web_pages** — Embedded HTML/CSS/JS served from the ESP32 (gzip-compressed in `web_pages_gz.cpp`)

### Web Server
HTTP server on port 80 with REST API endpoints under `/api/`. WebSocket server on port 81 for real-time updates. API endpoints are registered in `main.cpp`.

### FreeRTOS Tasks
Concurrent tasks with configurable stack sizes and priorities defined in `src/config.h` (`TASK_STACK_SIZE_*`, `TASK_PRIORITY_*`). Task setup is in `src/tasks.h/cpp`.

## Pin Configuration

Defined as build flags in `platformio.ini` and with fallback defaults in `src/config.h`:
- LED: GPIO 2, Amplifier relay: GPIO 4, Voltage sense (ADC): GPIO 1, Reset button: GPIO 15

## Testing Conventions

- Tests use Arrange-Act-Assert pattern
- Each test file has a `setUp()` that resets state
- Mock headers in `test/test_mocks/` simulate Arduino functions (`millis()`, `analogRead()`, GPIO), WiFi, MQTT (`PubSubClient`), and NVS (`Preferences`)
- The `native` environment compiles with `-D UNIT_TEST -D NATIVE_TEST` flags — use these for conditional compilation
- `test_build_src = no` in platformio.ini means tests don't compile `src/` directly; they include specific headers and use mocks

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

## Key Dependencies

- `ArduinoJson@^7.4.2` — JSON parsing throughout the codebase
- `WebSockets@^2.7.2` — WebSocket server for real-time UI updates
- `PubSubClient@^2.8` — MQTT client for Home Assistant integration
