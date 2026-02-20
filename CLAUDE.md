# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 based intelligent amplifier controller (ALX Nova) with smart auto-sensing, WiFi management, MQTT/Home Assistant integration, OTA firmware updates, a web configuration interface, and an LVGL-based GUI on a ST7735S TFT display with rotary encoder input. Built with PlatformIO and the Arduino framework. Current firmware version is defined in `src/config.h` as `FIRMWARE_VERSION`.

**Target board**: Freenove ESP32-S3 WROOM (FNK0085) — CH343 USB-to-UART bridge. PlatformIO board config: `esp32-s3-devkitm-1`.

## Build & Test Commands

```bash
# Build firmware for ESP32-S3
pio run

# Upload firmware to device
pio run --target upload

# Monitor serial output (115200 baud)
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

Tests run on the `native` environment (host machine with gcc/MinGW) using the Unity framework (935 tests). Mock implementations of Arduino, WiFi, MQTT, and Preferences libraries live in `test/test_mocks/`. Test modules: `test_utils`, `test_auth`, `test_wifi`, `test_mqtt`, `test_settings`, `test_ota`, `test_ota_task`, `test_button`, `test_websocket`, `test_api`, `test_smart_sensing`, `test_buzzer`, `test_gui_home`, `test_gui_input`, `test_gui_navigation`, `test_pinout`, `test_i2s_audio`, `test_fft`, `test_signal_generator`, `test_audio_diagnostics`, `test_vrms`, `test_dim_timeout`, `test_debug_mode`, `test_dsp`, `test_dsp_presets`, `test_dsp_rew`, `test_dsp_swap`, `test_crash_log`, `test_task_monitor`, `test_esp_dsp`, `test_usb_audio`, `test_usb_auto_priority`, `test_usb_input`, `test_adc_sync`, `test_audio_quality`, `test_captive_portal`, `test_config`, `test_dac_eeprom`, `test_dac_hal`, `test_debug_serial`, `test_emergency_limiter`, `test_peq`, `test_stack_overflow`, `test_websocket_messages`, `test_wifi_roaming`, `test_wifi_watchdog`.

## Architecture

### State Management — AppState Singleton
All application state lives in `src/app_state.h` as a singleton accessed via `AppState::getInstance()` or the `appState` macro. It uses **dirty flags** (e.g., `isAmplifierDirty()`, `isTimerDirty()`) to detect changes and minimize unnecessary WebSocket broadcasts and MQTT publishes.

Legacy code uses `#define` macros (e.g., `#define wifiSSID appState.wifiSSID`) to alias global variable names to AppState members. New code should use `appState.memberName` directly.

### FSM States
The application uses a finite state machine (`AppFSMState` in `app_state.h`): `STATE_IDLE`, `STATE_SIGNAL_DETECTED`, `STATE_AUTO_OFF_TIMER`, `STATE_WEB_CONFIG`, `STATE_OTA_UPDATE`, `STATE_ERROR`.

### Handler/Module Pattern
Each subsystem is a separate module in `src/`:
- **smart_sensing** — Voltage detection, auto-off timer, amplifier relay control. `detectSignal()` rate matches `appState.audioUpdateRate` (not hardcoded) with dynamically scaled smoothing alpha to maintain ~308ms time constant
- **wifi_manager** — Multi-network WiFi client, AP mode, async connection with retry/backoff
- **mqtt_handler** — MQTT broker connection, Home Assistant discovery, heartbeat publishing
- **ota_updater** — GitHub release checking, firmware download with SHA256 verification
- **settings_manager** — NVS/Preferences persistence, export/import, factory reset
- **auth_handler** — Session token management, web password authentication
- **button_handler** — Debouncing, short/long/very-long press and multi-click detection
- **buzzer_handler** — Piezo buzzer with multi-pattern sequencer, ISR-safe encoder tick/click, volume control, FreeRTOS mutex for dual-core safety
- **i2s_audio** — Dual PCM1808 I2S ADC driver, RMS/dBFS, VU metering, peak hold, waveform downsampling, FFT spectrum analysis (Radix-4 ESP-DSP on ESP32, arduinoFFT on native), 6 selectable FFT window types, SNR/SFDR analysis, audio health diagnostics
- **dsp_pipeline** — 4-channel audio DSP engine: biquad IIR, FIR, limiter, gain, delay, polarity, mute, compressor. Double-buffered config with glitch-free swap. ESP32 uses pre-built `libespressif__esp-dsp.a` (S3 assembly-optimized); native tests use `lib/esp_dsp_lite/` (ANSI C fallback, `lib_ignore`d on ESP32). Delay lines use PSRAM (`ps_calloc`) when available, with heap pre-flight check (40KB reserve) on fallback. `dsp_add_stage()` rolls back on pool exhaustion; config imports skip failed stages
- **dsp_coefficients** — RBJ Audio EQ Cookbook biquad coefficient computation via `dsp_gen_*` functions in `src/dsp_biquad_gen.h/.c` (renamed from `dsps_biquad_gen_*` to avoid symbol conflicts with pre-built ESP-DSP)
- **dsp_crossover** — Crossover presets (LR2/LR4/LR8, Butterworth), bass management, 4x4 routing matrix (SIMD-accelerated via dsps_mulc/dsps_add)
- **dsp_rew_parser** — Equalizer APO + miniDSP import/export, FIR text, WAV IR loading
- **dsp_api** — REST API endpoints for DSP config CRUD, persistence (LittleFS), debounced save
- **signal_generator** — Multi-waveform test signal generator (sine, square, noise, sweep), software injection + PWM output modes
- **task_monitor** — FreeRTOS task enumeration via `pxTaskGetNext`, stack usage, priority, core affinity. Runs on a dedicated 5s timer in main loop (decoupled from HW stats broadcast). Only scans stack watermarks for known app tasks. Opt-in via `debugTaskMonitor` (default off). Uses ESP-IDF `task_snapshot.h` API (not `uxTaskGetSystemState` which is unavailable in pre-compiled Arduino FreeRTOS lib)
- **usb_audio** — TinyUSB UAC2 speaker device on native USB OTG (GPIO 19/20). Custom audio class driver registered via `usbd_app_driver_get_cb()` weak function. SPSC lock-free ring buffer (1024 frames, PSRAM). Format conversion: USB PCM16/PCM24 → left-justified int32. FreeRTOS task on Core 0 with adaptive poll rate (100ms idle, 1ms streaming). Guarded by `-D USB_AUDIO_ENABLED`. Requires `build_unflags = -DARDUINO_USB_MODE -DARDUINO_USB_CDC_ON_BOOT` in platformio.ini. **Streaming state**: TinyUSB routes standard SET_INTERFACE to `.control_xfer_cb()` for custom drivers (NOT to `.open()`). Alt 0 = idle, alt 1+ = streaming. `_lastDataMs` timestamp + 500ms safety-net timeout (`usb_audio_check_timeout()`, polled every loop) catch cases where host doesn't send SET_INTERFACE alt 0. Buffer overruns/underruns synced from ring buffer to AppState in main loop before each WS broadcast; periodic 1s poll during streaming catches incremental changes. Host volume: Windows tracks locally and does NOT send SET_CUR after device init — displayed value is best-effort only.
- **debug_serial** — Log-level filtered serial output (`LOG_D`/`LOG_I`/`LOG_W`/`LOG_E`/`LOG_NONE`), runtime level control via `applyDebugSerialLevel()`, WebSocket log forwarding
- **websocket_handler** — Real-time state broadcasting to web clients (port 81). Audio waveform and spectrum data use binary WebSocket frames (`sendBIN`) for efficiency; audio levels remain JSON. Binary message types defined as `WS_BIN_WAVEFORM` (0x01) and `WS_BIN_SPECTRUM` (0x02) in `websocket_handler.h`
- **web_pages** — Embedded HTML/CSS/JS served from the ESP32 (gzip-compressed in `web_pages_gz.cpp`). **IMPORTANT: After ANY edit to `web_pages.cpp`, you MUST run `node build_web_assets.js` to regenerate `web_pages_gz.cpp` before building firmware.** The ESP32 serves the gzipped version — without this step, frontend changes will not take effect.

### GUI (LVGL on TFT Display)
LVGL v9.4 + LovyanGFX on ST7735S 128x160 (landscape 160x128). DMA double-buffered flush. Runs on Core 1 via FreeRTOS `gui_task`. All GUI code is guarded by `-D GUI_ENABLED`.

Key GUI modules in `src/gui/`:
- **gui_manager** — Init, FreeRTOS task, screen sleep/wake, dashboard refresh
- **gui_input** — ISR-driven rotary encoder (Gray code state machine)
- **gui_theme** — Orange accent theme, dark/light mode
- **gui_navigation** — Screen stack with push/pop and transition animations
- **screens/** — Desktop carousel, Home status, Control, WiFi, MQTT, Settings, Debug, Support, Boot animations, Keyboard, Value editor

### Web Server
HTTP server on port 80 with REST API endpoints under `/api/`. WebSocket server on port 81 for real-time updates. API endpoints are registered in `main.cpp`.

### FreeRTOS Tasks
Concurrent tasks with configurable stack sizes and priorities defined in `src/config.h` (`TASK_STACK_SIZE_*`, `TASK_PRIORITY_*`, `I2S_DMA_BUF_COUNT`/`I2S_DMA_BUF_LEN`). Main loop runs on Core 0; GUI task and audio capture task (`audio_capture_task`, priority 3) run on Core 1 (`TASK_CORE_AUDIO`). USB audio task (`usb_audio`, priority 1) runs on Core 0 polling TinyUSB with adaptive timeout (100ms idle, 1ms streaming). OTA update check and download run as one-shot tasks on Core 1 via `startOTACheckTask()` / `startOTADownloadTask()` in `src/ota_updater.cpp`. Cross-core communication uses dirty flags in AppState — GUI/OTA tasks set flags, main loop handles WebSocket/MQTT broadcasts.

**I2S driver safety**: The DAC module may uninstall/reinstall the I2S_NUM_0 driver at runtime (e.g., toggling DAC on/off). To prevent crashes with `audio_capture_task` calling `i2s_read()` concurrently, the `appState.audioPaused` volatile flag is set before `i2s_driver_uninstall()` and cleared after reinstall. The audio task checks this flag each iteration and yields while paused.

### Heap Safety & PSRAM
The board has 8MB OPI PSRAM (Freenove ESP32-S3 WROOM N16R8). DSP delay lines are allocated via `ps_calloc()` when PSRAM is available, falling back to regular `calloc()` with a pre-flight heap check (blocks allocation if free heap would drop below 40KB reserve). The `heapCritical` flag is set when `ESP.getMaxAllocHeap() < 40KB`, monitored every 30s in the main loop.

**Critical lesson**: WiFi RX buffers are dynamically allocated from internal SRAM heap. If free heap drops below ~40KB, incoming packets (ping, HTTP, WebSocket) are silently dropped while outgoing (MQTT publish) still works. Always ensure DSP/audio allocations use PSRAM or are guarded by heap checks.

## Pin Configuration

Defined as build flags in `platformio.ini` and with fallback defaults in `src/config.h`:
- Core: LED=2, Amplifier=4, VoltSense(ADC)=1, Reset button=15, Buzzer=8
- TFT: MOSI=11, SCLK=12, CS=10, DC=13, RST=14, BL=21
- Encoder: A=5, B=6, SW=7
- I2S Audio ADC1: BCK=16, DOUT=17, LRC=18, MCLK=3
- I2S Audio ADC2: DOUT2=9 (shares BCK/LRC/MCLK with ADC1)
- I2S DAC TX: DOUT=40, BCK=16, LRC=18 (full-duplex on I2S0; BCK/LRC shared with ADC1)
- EEPROM I2C: SDA=41, SCL=42 (EEPROM only — not connected to any DAC)
- Signal Generator PWM: GPIO 38
- USB Audio: GPIO 19/20 (native USB D-/D+, used by TinyUSB UAC2 speaker device)

**ESP32-S3 GPIO 19/20 — USB Audio**: With `ARDUINO_USB_MODE=0`, these pins are used by TinyUSB for the UAC2 audio device. Serial still works via CH343/UART0 on the other USB port. Do NOT use GPIO 19 or 20 for I2S or other peripherals.

### Dual I2S Configuration (Both Masters)

Both PCM1808 ADCs share BCK/LRC/MCLK clock lines. **Both I2S peripherals are configured as master RX** — I2S_NUM_0 (ADC1) outputs BCK/WS/MCLK clocks, while I2S_NUM_1 (ADC2) has NO clock output (only data_in on GPIO9). Both derive from the same 160MHz D2CLK with identical divider chains, giving frequency-locked BCK.

**Why not slave mode**: ESP32-S3 I2S slave mode has intractable DMA issues — the legacy driver always calculates `bclk_div = 4` (below the hardware minimum of 8), and the LL layer hard-codes `rx_clk_sel = 2` (D2CLK) regardless of APLL settings, making register overrides ineffective. Three separate slave-mode fixes were attempted (pin routing, APLL, direct register override) — all failed with DMA timeout.

Init order in `i2s_audio_init()`:
1. **ADC2 first** — `i2s_configure_adc2()` installs master driver with only `data_in_num = GPIO9` (BCK/WS/MCK = `I2S_PIN_NO_CHANGE`)
2. **ADC1 second** — `i2s_configure_adc1()` installs master driver with all pins (BCK=16, WS=18, MCLK=3, DOUT=17)

DOUT2 uses `INPUT_PULLDOWN` so an unconnected pin reads zeros (→ NO_DATA) instead of floating noise (→ false OK). No GPIO matrix reconnection is needed since I2S1 uses internal clocking.

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

**IMPORTANT**: Never add `Co-Authored-By` trailers (e.g., `Co-Authored-By: Claude ...<noreply@anthropic.com>`) to commit messages. Commits should not contain any AI attribution lines.

## CI/CD

GitHub Actions (`.github/workflows/tests.yml`): runs all native tests, then builds ESP32-S3 firmware. Triggers on push/PR to `main` and `develop` branches. A separate `release.yml` workflow handles automated releases.

## Serial Debug Logging

All modules use `debug_serial.h` macros (`LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`) with consistent `[ModuleName]` prefixes:

| Module | Prefix | Notes |
|---|---|---|
| `smart_sensing` | `[Sensing]` | Mode changes, threshold, timer, amplifier state, ADC health transitions |
| `i2s_audio` | `[Audio]` | Init, sam       ple rate changes, ADC detection changes. Periodic dump via `audio_periodic_dump()` from main loop |
| `signal_generator` | `[SigGen]` | Init, start/stop, PWM duty, param changes while active |
| `buzzer_handler` | `[Buzzer]` | Init, pattern start/complete, play requests (excludes tick/click to avoid noise) |
| `wifi_manager` | `[WiFi]` | Connection attempts, AP mode, scan results |
| `mqtt_handler` | `[MQTT]` | Connect/disconnect, HA discovery, publish errors |
| `ota_updater` | `[OTA]` | Version checks, download progress, verification |
| `settings_manager` | `[Settings]` | Load/save operations |
| `usb_audio` | `[USB Audio]` | Init, connect/disconnect, streaming start/stop, host volume/mute changes |
| `button_handler` | — | Logged from `main.cpp` (11 LOG calls covering all press types) |
| `gui_*` | `[GUI]` | Navigation, screen transitions, theme changes |

When adding logging to new modules, follow these conventions:
- Use `LOG_I` for state transitions and significant events (start/stop, connect/disconnect, health changes)
- Use `LOG_D` for high-frequency operational details (pattern steps, param snapshots)
- Never log inside ISR paths or real-time FreeRTOS tasks (e.g., `audio_capture_task`) — `Serial.print` blocks when UART TX buffer fills, starving DMA and causing audio dropouts. Use dirty-flag pattern: task sets flag, main loop calls `audio_periodic_dump()` for actual Serial/WS output
- Log transitions, not repetitive state (use static `prev` variables to detect changes)

## Key Dependencies

- `ArduinoJson@^7.4.2` — JSON parsing throughout the codebase
- `WebSockets@^2.7.2` — WebSocket server for real-time UI updates
- `PubSubClient@^2.8` — MQTT client for Home Assistant integration
- `lvgl@^9.4` — GUI framework (guarded by `GUI_ENABLED`)
- `LovyanGFX@^1.2.0` — TFT display driver with DMA support for ST7735S (replaced TFT_eSPI)
- `arduinoFFT@^2.0` — FFT spectrum analysis (**native tests only**; ESP32 uses pre-built ESP-DSP FFT)
- **ESP-DSP pre-built library** (`libespressif__esp-dsp.a`) — S3 assembly-optimized biquad IIR, FIR, Radix-4 FFT, window functions, vector math (mulc/mul/add), dot product, SNR/SFDR analysis. Include paths added via `-I` flags in `platformio.ini`. `lib/esp_dsp_lite/` provides ANSI C fallbacks for native tests only (`lib_ignore = esp_dsp_lite` in ESP32 envs)
