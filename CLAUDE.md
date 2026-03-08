# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-P4 based intelligent amplifier controller (ALX Nova) with smart auto-sensing, WiFi management, MQTT/Home Assistant integration, OTA firmware updates, a web configuration interface, and an LVGL-based GUI on a ST7735S TFT display with rotary encoder input. Built with PlatformIO and the Arduino framework. Current firmware version is defined in `src/config.h` as `FIRMWARE_VERSION`.

**Target board**: Waveshare ESP32-P4-WiFi6-DEV-Kit. PlatformIO board config: `esp32-p4`. Upload/monitor port: COM8.

## Build & Test Commands

```bash
# Build firmware for ESP32-P4
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

Tests run on the `native` environment (host machine with gcc/MinGW) using the Unity framework (1556 tests). Mock implementations of Arduino, WiFi, MQTT, and Preferences libraries live in `test/test_mocks/`. Test modules: `test_utils`, `test_auth`, `test_wifi`, `test_mqtt`, `test_settings`, `test_ota`, `test_ota_task`, `test_button`, `test_websocket`, `test_api`, `test_smart_sensing`, `test_buzzer`, `test_gui_home`, `test_gui_input`, `test_gui_navigation`, `test_pinout`, `test_i2s_audio`, `test_fft`, `test_signal_generator`, `test_audio_diagnostics`, `test_vrms`, `test_dim_timeout`, `test_debug_mode`, `test_dsp`, `test_dsp_rew`, `test_crash_log`, `test_task_monitor`, `test_esp_dsp`, `test_usb_audio`, `test_hal_device`, `test_hal_manager`, `test_hal_registry`, `test_hal_db`, `test_output_dsp`, `test_dac_settings`, `test_evt_any`, `test_sink_slot_api`, `test_hal_state_callback`, `test_hal_bridge`, `test_deferred_toggle`, `test_hal_multi_instance`, `test_hal_siggen`, `test_hal_usb_audio`.

## Architecture

### State Management — AppState Singleton
All application state lives in `src/app_state.h` as a singleton accessed via `AppState::getInstance()` or the `appState` macro. It uses **dirty flags** (e.g., `isBuzzerDirty()`, `isDisplayDirty()`, `isOTADirty()`) to detect changes and minimize unnecessary WebSocket broadcasts and MQTT publishes. Every dirty-flag setter also calls `app_events_signal(EVT_XXX)` (defined in `src/app_events.h`) so the main loop can replace `delay(5)` with `app_events_wait(5)` — waking immediately on any state change and falling back to a 5 ms tick when idle. The event group uses 24 usable bits (`EVT_ANY = 0x00FFFFFF`; bits 24-31 are reserved by FreeRTOS). Currently 16 event bits are assigned (bits 0-15) with 8 spare.

Legacy code uses `#define` macros (e.g., `#define wifiSSID appState.wifiSSID`) to alias global variable names to AppState members. New code should use `appState.memberName` directly.

Change-detection shadow fields (`prevMqtt*`) have been extracted from AppState into file-local statics in `mqtt_publish.cpp`. Similarly, `prevBroadcast*` sensing fields live in `smart_sensing.cpp`. DAC/ES8311 deferred toggles use validated setters: `appState.requestDacToggle(int8_t)` and `appState.requestEs8311Toggle(int8_t)` — only accept -1, 0, 1; direct writes to `_pendingDacToggle` / `_pendingEs8311Toggle` are unsafe.

### FSM States
The application uses a finite state machine (`AppFSMState` in `app_state.h`): `STATE_IDLE`, `STATE_SIGNAL_DETECTED`, `STATE_AUTO_OFF_TIMER`, `STATE_WEB_CONFIG`, `STATE_OTA_UPDATE`, `STATE_ERROR`.

### Handler/Module Pattern
Each subsystem is a separate module in `src/`:
- **smart_sensing** — Voltage detection, auto-off timer, amplifier relay control. `detectSignal()` rate matches `appState.audioUpdateRate` (not hardcoded) with dynamically scaled smoothing alpha to maintain ~308ms time constant
- **wifi_manager** — Multi-network WiFi client, AP mode, async connection with retry/backoff
- **mqtt_handler** — MQTT broker connection, settings load/save, callback dispatch, HTTP API handlers. Split across 3 files: `mqtt_handler.cpp` (core lifecycle ~1120 lines), `mqtt_publish.cpp` (all `publishMqtt*()` functions + change-detection statics), `mqtt_ha_discovery.cpp` (HA discovery ~1880 lines). `mqttCallback()` is thread-safe: no direct WebSocket, LittleFS, or WiFi calls — all side-effects use dirty flags or coordination flags (`_pendingApToggle`). Periodic publish logic exposed as `mqttPublishPendingState()` + `mqttPublishHeartbeat()` (called by `mqtt_task`). Change-detection `prevMqtt*` statics live in `mqtt_publish.cpp` (not in AppState)
- **mqtt_task** — Dedicated FreeRTOS task (Core 0, priority 2) that owns MQTT reconnect and publish. `mqttReconnect()` (1–3 s blocking TCP connect when broker unreachable) runs entirely inside this task so HTTP/WebSocket serving on the main loop is never stalled. Checks `appState._mqttReconfigPending` to reconnect when web UI saves new broker settings. Polls at 20 Hz (50 ms `vTaskDelay`); does **not** wait on the event group (avoids fan-out race with the main loop's single-consumer `pdTRUE` clear)
- **ota_updater** — GitHub release checking, firmware download with SHA256 verification
- **settings_manager** — Dual-format persistence: `/config.json` (primary, JSON with `{ "version": 1, "settings": {...}, "mqtt": {...} }`) with safe atomic write (`config.json.tmp` → rename). Falls back to legacy `settings.txt` / `mqtt_config.txt` if JSON absent, auto-migrating on first boot. WiFi credentials and selected NVS settings survive LittleFS format. Export/import, factory reset.
- **auth_handler** — Session token management, web password authentication. Password hashed with PBKDF2-SHA256 (10,000 iterations, random 16-byte salt, stored as `"p1:<saltHex>:<keyHex>"`). Legacy unsalted SHA256 hashes auto-migrate on next successful login. First-boot random password (10 chars, ~57-bit entropy) displayed on TFT and serial; regenerated on factory reset. Login rate limiting is non-blocking: failed attempts set `_nextLoginAllowedMs`, excess requests return HTTP 429 with `Retry-After` header immediately. Cookie uses `HttpOnly` flag. WS connection authenticated via short-lived token from `GET /api/ws-token` (60s TTL, 16-slot pool).
- **button_handler** — Debouncing, short/long/very-long press and multi-click detection
- **buzzer_handler** — Piezo buzzer with multi-pattern sequencer, ISR-safe encoder tick/click, volume control, FreeRTOS mutex for dual-core safety
- **audio_pipeline** — Modular audio pipeline: 8-lane input (HAL-managed via `getInputSource()` — ADC, SigGen, USB Audio all registered dynamically) → per-input DSP → 16×16 routing matrix → per-output DSP → slot-indexed sink dispatch (8 slots). Float32 [-1.0,+1.0] internally; int32↔float only at edges. DMA raw buffers in internal SRAM; float working buffers in PSRAM. Slot-indexed APIs: `audio_pipeline_set_sink(slot, sink)` / `audio_pipeline_remove_sink(slot)` and `audio_pipeline_set_source(lane, src)` / `audio_pipeline_remove_source(lane)` under `vTaskSuspendAll()` (both tasks on Core 1). Read-only accessor `audio_pipeline_get_source(lane)` returns registered source info (used by `sendAudioChannelMap()` for dynamic input discovery). Dispatch iterates all `AUDIO_OUT_MAX_SINKS` (8) slots, skipping NULL/not-ready/muted. `AudioOutputSink.halSlot` (0xFF = unbound) enables O(1) HAL device lookup. Generic per-source VU metering for all active input lanes. **Never calls `i2s_configure_adc1()` in the task loop** — MCLK must remain continuous for PCM1808 PLL stability. Lives in `src/audio_pipeline.h/.cpp`.
- **i2s_audio** — Dual PCM1808 I2S ADC HAL: driver init/config, I2S TX bridge, FFT/waveform buffers, pure computation functions (RMS, VU, peak, quantize, downsample, FFT bands, health status). `i2s_audio_init()` delegates audio task creation to `audio_pipeline_init()`. Analysis shared state written by pipeline, read by consumers via `i2s_audio_get_analysis()`.
- **dsp_pipeline** — 4-channel audio DSP engine: biquad IIR, FIR, limiter, gain, delay, polarity, mute, compressor. Double-buffered config with glitch-free swap. ESP32 uses pre-built `libespressif__esp-dsp.a` (S3 assembly-optimized); native tests use `lib/esp_dsp_lite/` (ANSI C fallback, `lib_ignore`d on ESP32). Delay lines use PSRAM (`ps_calloc`) when available, with heap pre-flight check (40KB reserve) on fallback. `dsp_add_stage()` rolls back on pool exhaustion; config imports skip failed stages
- **dsp_coefficients** — RBJ Audio EQ Cookbook biquad coefficient computation via `dsp_gen_*` functions in `src/dsp_biquad_gen.h/.c` (renamed from `dsps_biquad_gen_*` to avoid symbol conflicts with pre-built ESP-DSP)
- **dsp_crossover** — Crossover presets (LR2/LR4/LR8, Butterworth), bass management
- **dsp_rew_parser** — Equalizer APO + miniDSP import/export, FIR text, WAV IR loading
- **dsp_api** — REST API endpoints for DSP config CRUD, persistence (LittleFS), debounced save
- **signal_generator** — Multi-waveform test signal generator (sine, square, noise, sweep), software injection (HAL-assigned dynamic lane via `HalSigGen`) + PWM output modes
- **task_monitor** — FreeRTOS task enumeration via `xTaskGetNext`, stack usage, priority, core affinity. Runs on a dedicated 5s timer in main loop (decoupled from HW stats broadcast). Only scans stack watermarks for known app tasks. Opt-in via `debugTaskMonitor` (default off). Uses `<esp_private/freertos_debug.h>` + `TaskIterator_t` (`task_snapshot.h` was deprecated in IDF5)
- **usb_audio** — TinyUSB UAC2 speaker device on native USB OTG (GPIO 19/20). HAL-managed via `HalUsbAudio` (compatible `alx,usb-audio`, type `HAL_DEV_ADC`). Custom audio class driver registered via `usbd_app_driver_get_cb()` weak function. SPSC lock-free ring buffer (1024 frames, PSRAM). Format conversion: USB PCM16/PCM24 → left-justified int32. Host volume/mute applied inline in source read callback. FreeRTOS task on Core 0 with adaptive poll rate (100ms idle, 1ms streaming). Guarded by `-D USB_AUDIO_ENABLED`. Requires `build_unflags = -DARDUINO_USB_MODE -DARDUINO_USB_CDC_ON_BOOT` in platformio.ini
- **debug_serial** — Log-level filtered serial output (`LOG_D`/`LOG_I`/`LOG_W`/`LOG_E`/`LOG_NONE`), runtime level control via `applyDebugSerialLevel()`, WebSocket log forwarding. `broadcastLine()` sends `"module"` as a separate JSON field extracted from the `[ModuleName]` prefix, enabling frontend category filtering
- **hal_device_manager** — Singleton managing up to 16 HAL devices with 24-pin tracking. Priority-sorted init (BUS=1000→LATE=100). Pin claim/release system prevents GPIO conflicts. Per-device `HalDeviceConfig` with I2C/I2S/GPIO overrides persisted to `/hal_config.json`. State change callback (`HalStateChangeCb`) fires on every `_state` transition — registered by `hal_pipeline_bridge` at boot. Lives in `src/hal/hal_device_manager.h/.cpp`
- **hal_pipeline_bridge** — Connects HAL device lifecycle to the audio pipeline. Owns both sink and source lifecycle: `on_device_available()` calls `audio_pipeline_set_sink()` for outputs and `audio_pipeline_set_source()` for inputs (via `dev->getInputSource()`), `on_device_removed()` calls `audio_pipeline_remove_sink()` / `audio_pipeline_remove_source()`. Sets `appState.adcEnabled[lane]` for input devices. Capability-based ordinal counting assigns slots/lanes dynamically (`HAL_CAP_DAC_PATH` → output slot, `HAL_CAP_ADC_PATH` → input lane). Hybrid transient policy: UNAVAILABLE sets `_ready=false` only (auto-recovery via volatile flag); MANUAL/ERROR/REMOVED explicitly remove the sink/source. Mapping tables `_halSlotToSinkSlot[]` and `_halSlotToAdcLane[]` track HAL-to-pipeline bindings. Lives in `src/hal/hal_pipeline_bridge.h/.cpp`
- **hal_device_db** — In-memory device database with builtin entries (PCM5102A, ES8311, PCM1808, NS4150B, TempSensor, SigGen, USB Audio) plus LittleFS JSON persistence. EEPROM v3 compatible string matching. Lives in `src/hal/hal_device_db.h/.cpp`
- **hal_discovery** — 3-tier device discovery: I2C bus scan → EEPROM probe → manual config. Bus scan covers address range 0x08–0x77. Skips Bus 0 (GPIO 48/54) when WiFi active (SDIO conflict); Bus 1 (ONBOARD) and Bus 2 (EXPANSION) always safe. Post-boot rescan via `POST /api/hal/scan` is mutex-guarded to avoid collision with ES8311 driver mid-transaction. Lives in `src/hal/hal_discovery.h/.cpp`
- **hal_api** — REST endpoints for HAL device CRUD: `GET /api/hal/devices`, `POST /api/hal/scan`, `PUT /api/hal/devices` (config update), `DELETE /api/hal/devices` (remove), `POST /api/hal/devices/reinit`, `GET /api/hal/db/presets`. Lives in `src/hal/hal_api.h/.cpp`
- **hal_builtin_devices** — Driver registry with compatible string → factory function mapping. Registers PCM5102A, ES8311, PCM1808, DSP bridge, NS4150B, TempSensor, SigGen (`alx,signal-gen`), USB Audio (`alx,usb-audio`), MCP4725 drivers. Lives in `src/hal/hal_builtin_devices.h/.cpp`
- **hal_temp_sensor** — ESP32-P4 internal chip temperature sensor using IDF5 `<driver/temperature_sensor.h>`. Range -10 to +80°C. Guarded by `CONFIG_IDF_TARGET_ESP32P4`. Lives in `src/hal/hal_temp_sensor.h/.cpp`
- **hal_ns4150b** — NS4150B class-D amplifier driver. GPIO-controlled enable/disable on GPIO 53 (shared with ES8311 PA pin). Lives in `src/hal/hal_ns4150b.h/.cpp`
- **output_dsp** — Per-output mono DSP engine applied post-matrix/pre-sink. Biquad, gain, limiter, compressor, polarity, mute stages. Double-buffered config with glitch-free atomic swap. Lives in `src/output_dsp.h/.cpp`
- **websocket_handler** — Real-time state broadcasting to web clients (port 81). WS connections authenticated via token fetched from `GET /api/ws-token` on `onopen` — client sends token in first auth message, server validates against 16-slot pool (60s TTL, single-use). Audio waveform and spectrum data use binary WebSocket frames (`sendBIN`) for efficiency; audio levels remain JSON. Binary message types defined as `WS_BIN_WAVEFORM` (0x01) and `WS_BIN_SPECTRUM` (0x02) in `websocket_handler.h`. `sendAudioChannelMap()` dynamically discovers registered input sources via `audio_pipeline_get_source()` and enriches with HAL device metadata via `hal_pipeline_get_slot_for_adc_lane()` — only active sources appear in broadcast
- **web_pages** — Embedded HTML/CSS/JS served from the ESP32 (gzip-compressed in `web_pages_gz.cpp`). **IMPORTANT: Edit source files in `web_src/` — NOT `src/web_pages.cpp` (auto-generated). After ANY edit to `web_src/` files, run `node tools/build_web_assets.js` to regenerate `src/web_pages.cpp` and `src/web_pages_gz.cpp` before building firmware.**
  - `web_src/index.html` — HTML shell (body content, no inline CSS/JS)
  - `web_src/css/01-05-*.css` — CSS split by concern (variables, layout, components, canvas, responsive)
  - `web_src/js/01-28-*.js` — JS modules in load order (core, state, UI, audio, DSP, WiFi, settings, system)
  - `src/web_pages.cpp` and `src/web_pages_gz.cpp` are both auto-generated — do not edit manually
  - Debug Console: module/category chip filtering (auto-populated), search/highlight, entry count badges (red=errors, amber=warnings), sticky filters (localStorage), relative/absolute timestamp toggle. Card positioned below Debug Controls

### GUI (LVGL on TFT Display)
LVGL v9.4 + LovyanGFX on ST7735S 128x160 (landscape 160x128). Runs on **Core 0** via FreeRTOS `gui_task` (`TASK_CORE_GUI=0`) — moved off Core 1 to keep Core 1 exclusively for audio. All GUI code is guarded by `-D GUI_ENABLED`.

Key GUI modules in `src/gui/`:
- **gui_manager** — Init, FreeRTOS task, screen sleep/wake, dashboard refresh
- **gui_input** — ISR-driven rotary encoder (Gray code state machine)
- **gui_theme** — Orange accent theme, dark/light mode
- **gui_navigation** — Screen stack with push/pop and transition animations
- **screens/** — Desktop carousel, Home status, Control, WiFi, MQTT, Settings, Debug, Support, Boot animations, Keyboard, Value editor

### Web Server
HTTP server on port 80 with REST API endpoints under `/api/`. WebSocket server on port 81 for real-time updates. API endpoints are registered in `main.cpp`.

### HAL Framework (Hardware Abstraction Layer)
Device model in `src/hal/` with lifecycle management, discovery, and configuration. Guarded by `-D DAC_ENABLED`.

**Device lifecycle**: UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ⇄ UNAVAILABLE → ERROR / REMOVED / MANUAL. Volatile `_ready` flag and `_state` enum enable lock-free reads from the audio pipeline on Core 1. State changes fire `HalStateChangeCb` → `hal_pipeline_bridge` → slot-indexed sink set/remove + dirty flags.

**Registered onboard devices**: PCM5102A (DAC, I2S), ES8311 (Codec, I2C bus 1), PCM1808 x2 (ADC, I2S), NS4150B (Amp, GPIO 53), Chip Temperature Sensor (Internal), Signal Generator (`alx,signal-gen`, software ADC), USB Audio (`alx,usb-audio`, software ADC, guarded by `USB_AUDIO_ENABLED`), MCP4725 (DAC, I2C).

**REST API**: `GET /api/hal/devices` (list), `POST /api/hal/scan` (rescan with 409 guard), `PUT /api/hal/devices` (config update), `DELETE /api/hal/devices` (remove), `POST /api/hal/devices/reinit`, `GET /api/hal/db/presets`. Config persisted to `/hal_config.json`.

**I2C bus architecture** (ESP32-P4):
- Bus 0 (EXT): GPIO 48/54 — **shares SDIO with WiFi**, never scan when WiFi active
- Bus 1 (ONBOARD): GPIO 7/8 — ES8311 dedicated, always safe
- Bus 2 (EXPANSION): GPIO 28/29 — always safe

### FreeRTOS Tasks
Concurrent tasks with configurable stack sizes and priorities defined in `src/config.h` (`TASK_STACK_SIZE_*`, `TASK_PRIORITY_*`, `I2S_DMA_BUF_COUNT`/`I2S_DMA_BUF_LEN`).

**Core assignment** (Core 1 is reserved for audio):
- **Core 1**: `loopTask` (Arduino main loop, priority 1) + `audio_pipeline_task` (priority 3, `TASK_CORE_AUDIO`). The audio task preempts the main loop during DMA processing then yields 2 ticks. No new tasks may be pinned to Core 1.
- **Core 0**: `gui_task` (`TASK_CORE_GUI=0`), `mqtt_task` (priority 2), `usb_audio_task` (priority 1), one-shot OTA tasks (`startOTACheckTask()` / `startOTADownloadTask()`).

**Main loop idle strategy**: `delay(5)` has been replaced with `app_events_wait(5)` (see `src/app_events.h`). The loop wakes in <1 µs whenever any dirty flag is set, or falls back to a 5 ms tick when idle — preserving all periodic timers unchanged.

**MQTT**: The main loop no longer calls `mqttLoop()` or any `publishMqtt*()` function. All MQTT work (reconnect, `mqttClient.loop()`, periodic HA publishes) runs inside `mqtt_task` on Core 0. The main loop dispatches dirty flags to WS broadcasts only; MQTT publishes happen independently at 20 Hz.

Cross-core communication uses dirty flags in AppState — GUI/OTA/MQTT tasks set flags, main loop handles WebSocket broadcasts. Two additional coordination flags in AppState: `volatile bool _mqttReconfigPending` (web UI broker change → mqtt_task reconnects) and `volatile int8_t _pendingApToggle` (MQTT command → main loop executes WiFi mode change).

**I2S driver safety**: The DAC module may uninstall/reinstall the I2S_NUM_0 driver at runtime (e.g., toggling DAC on/off). To prevent crashes with `audio_pipeline_task` calling `i2s_read()` concurrently, `appState.audioPaused` is set before `i2s_driver_uninstall()`. A binary semaphore `appState.audioTaskPausedAck` provides deterministic handshake: dac deinit sets `audioPaused=true` then `xSemaphoreTake(audioTaskPausedAck, 50ms)`; audio task gives the semaphore when it observes the flag and yields. This replaces the previous volatile + `vTaskDelay(40ms)` guess.

### Heap Safety & PSRAM
The Waveshare ESP32-P4-WiFi6-DEV-Kit has PSRAM. DSP delay lines are allocated via `ps_calloc()` when PSRAM is available, falling back to regular `calloc()` with a pre-flight heap check (blocks allocation if free heap would drop below 40KB reserve). The `heapCritical` flag is set when `ESP.getMaxAllocHeap() < 40KB`, monitored every 30s in the main loop.

**Critical lesson**: WiFi RX buffers are dynamically allocated from internal SRAM heap. If free heap drops below ~40KB, incoming packets (ping, HTTP, WebSocket) are silently dropped while outgoing (MQTT publish) still works. Always ensure DSP/audio allocations use PSRAM or are guarded by heap checks.

## Pin Configuration

Defined as build flags in `platformio.ini` and with fallback defaults in `src/config.h`:
- Core: LED=1, Amplifier=27, Reset button=46, Buzzer=45
- TFT: MOSI=2, SCLK=3, CS=4, DC=5, RST=6, BL=26
- Encoder: A=32, B=33, SW=36
- I2S Audio ADC1: BCK=20, DOUT=23, LRC=21, MCLK=22
- I2S Audio ADC2: DOUT2=25 (shares BCK/LRC/MCLK with ADC1)
- I2S DAC TX: DOUT=24 (full-duplex on I2S0), DAC I2C: SDA=48, SCL=54
- Signal Generator PWM: GPIO 47
- ES8311 Onboard DAC I2C: SDA=7, SCL=8 (dedicated onboard bus, PA=53)
- NS4150B Amp Enable: GPIO 53 (shared with ES8311 PA pin)
- I2C Bus 2 (Expansion): SDA=28, SCL=29
- USB Audio: native USB OTG on P4 (TinyUSB UAC2 speaker device)

**I2C Bus 0 (GPIO 48/54) SDIO conflict**: These pins are shared with the ESP32-C6 WiFi SDIO interface. I2C transactions while WiFi is active cause `sdmmc_send_cmd` errors and MCU reset. HAL discovery skips this bus when WiFi is connected.

### Dual I2S Configuration (Both Masters)

Both PCM1808 ADCs share BCK/LRC/MCLK clock lines. **Both I2S peripherals are configured as master RX** — I2S_NUM_0 (ADC1) outputs BCK/WS/MCLK clocks, while I2S_NUM_1 (ADC2) has NO clock output (only data_in on GPIO9). Both derive from the same 160MHz D2CLK with identical divider chains, giving frequency-locked BCK.

**Why not slave mode**: I2S slave mode has intractable DMA issues on ESP32 — the legacy driver always calculates `bclk_div = 4` (below the hardware minimum of 8), and the LL layer hard-codes the clock source regardless of APLL settings, making register overrides ineffective. Both master mode with coordinated init is the confirmed-working approach.

Init order in `i2s_audio_init()`:
1. **ADC2 first** — `i2s_configure_adc2()` installs master driver with only `data_in_num = GPIO25` (BCK/WS/MCK = `I2S_PIN_NO_CHANGE`)
2. **ADC1 second** — `i2s_configure_adc1()` installs master driver with all pins (BCK=20, WS=21, MCLK=22, DOUT=23)

DOUT2 uses `INPUT_PULLDOWN` so an unconnected pin reads zeros (→ NO_DATA) instead of floating noise (→ false OK). No GPIO matrix reconnection is needed since I2S1 uses internal clocking.

## Testing Conventions

### C++ Unit Tests (Unity, native platform)
- Tests use Arrange-Act-Assert pattern
- Each test file has a `setUp()` that resets state
- Mock headers in `test/test_mocks/` simulate Arduino functions (`millis()`, `analogRead()`, GPIO), WiFi, MQTT (`PubSubClient`), and NVS (`Preferences`)
- The `native` environment compiles with `-D UNIT_TEST -D NATIVE_TEST` flags — use these for conditional compilation
- `test_build_src = no` in platformio.ini means tests don't compile `src/` directly; they include specific headers and use mocks
- Each test module must be in its own directory to avoid duplicate `main`/`setUp`/`tearDown` symbols

### Browser / E2E Tests (Playwright)
Playwright browser tests live in `e2e/tests/` (26 tests across 19 specs). They verify the web UI against a mock Express server + Playwright `routeWebSocket()` interception — no real hardware needed. Full architecture: `docs/testing-architecture.md`. Diagrams: `docs/architecture/test-infrastructure.mmd`, `e2e-test-flow.mmd`, `test-coverage-map.mmd`. Plan: `docs/planning/test-strategy.md`.

```bash
cd e2e
npm install                              # First time only
npx playwright install --with-deps chromium  # First time only
npx playwright test                      # Run all 26 browser tests
npx playwright test tests/auth.spec.js   # Run single spec
npx playwright test --headed             # Run with visible browser
npx playwright test --debug              # Debug mode with inspector
```

**Test infrastructure:**
- `e2e/mock-server/server.js` — Express server (port 3000) assembling HTML from `web_src/`
- `e2e/mock-server/assembler.js` — Replicates `tools/build_web_assets.js` HTML assembly
- `e2e/mock-server/ws-state.js` — Deterministic mock state singleton, reset between tests
- `e2e/mock-server/routes/*.js` — 12 Express route files matching firmware REST API
- `e2e/helpers/fixtures.js` — `connectedPage` fixture: session cookie + WS auth + initial state broadcasts
- `e2e/helpers/ws-helpers.js` — `buildInitialState()`, `handleCommand()`, binary frame builders
- `e2e/helpers/selectors.js` — Reusable DOM selectors matching `web_src/index.html` IDs
- `e2e/fixtures/ws-messages/*.json` — 15 hand-crafted WS broadcast fixtures
- `e2e/fixtures/api-responses/*.json` — 14 deterministic REST response fixtures

**Key Playwright patterns:**
- WS interception: `page.routeWebSocket(/.*:81/, handler)` — uses `onMessage`/`onClose` (capital M/C)
- Tab navigation in tests: `page.evaluate(() => switchTab('tabName'))` — avoids scroll issues with sidebar clicks
- CSS-hidden checkboxes: use `toBeChecked()` not `toBeVisible()` for toggle inputs styled with `label.switch`
- Strict mode: use `.first()` when a selector might match multiple elements

### Mandatory Test Coverage Rules

**Every code change MUST keep tests green.** Before completing any task:

1. **C++ firmware changes** (`src/`):
   - Run `pio test -e native -v` or use the `firmware-test-runner` agent
   - New modules need a test file in `test/test_<module>/`
   - Changed function signatures → update affected tests

2. **Web UI changes** (`web_src/`):
   - Run `cd e2e && npx playwright test` or use the `test-engineer` agent
   - New toggle/button/dropdown → add test verifying it sends the correct WS command
   - New WS broadcast type → add fixture JSON + test verifying DOM updates
   - New tab or section → add navigation + element presence tests
   - Changed element IDs → update `e2e/helpers/selectors.js` + affected specs
   - Removed features → remove corresponding tests + fixtures
   - New top-level JS declarations → add to `web_src/.eslintrc.json` globals

3. **WebSocket protocol changes** (`src/websocket_handler.cpp`):
   - Update `e2e/fixtures/ws-messages/` with new/changed message fixtures
   - Update `e2e/helpers/ws-helpers.js` `buildInitialState()` and `handleCommand()`
   - Update `e2e/mock-server/ws-state.js` if new state fields are added
   - Add Playwright test verifying the frontend handles the new message type

4. **REST API changes** (`src/main.cpp`, `src/hal/hal_api.cpp`):
   - Update matching route in `e2e/mock-server/routes/*.js`
   - Update `e2e/fixtures/api-responses/` with new/changed response fixtures
   - Add Playwright test if the UI depends on the new endpoint

### Agent Workflow for Test Maintenance

**Always verify tests after code changes.** Use specialised agents in parallel:

| Change Type | Agent(s) to Use | What They Do |
|---|---|---|
| C++ firmware only | `firmware-test-runner` | Runs `pio test -e native -v`, diagnoses failures |
| Web UI only | `test-engineer` or `test-writer` | Runs Playwright, fixes selectors, adds coverage |
| Both firmware + UI | Launch **both** agents in parallel | Full coverage verification |
| New HAL driver | `hal-driver-scaffold` → `firmware-test-runner` | Scaffold creates test module automatically |
| New web feature | `web-feature-scaffold` → `test-engineer` | Scaffold creates DOM, then add E2E tests |
| Bug investigation | `debugger` or `debug` agent | Root cause analysis with test reproduction |

**Parallel execution pattern** — when changes span firmware and web UI, launch both in a single message:
```
Agent 1 (firmware-test-runner): "Run pio test -e native -v and report results"
Agent 2 (test-engineer): "Run cd e2e && npx playwright test and fix any failures"
```

### Static Analysis (CI-enforced)
- **ESLint** (`web_src/.eslintrc.json`): Lints all JS files with 380 globals for concatenated scope. Rules: `no-undef`, `no-redeclare`, `eqeqeq`. Run: `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`
- **cppcheck**: C++ static analysis on `src/` (excluding `src/gui/`). Run in CI only
- **find_dups.js** + **check_missing_fns.js**: Duplicate/missing declaration checks. Run: `node tools/find_dups.js && node tools/check_missing_fns.js`

### Quality Gates (CI/CD)
All 4 gates must pass before firmware build proceeds (parallel execution):
1. `cpp-tests` — `pio test -e native -v` (1,556 Unity tests across 63 modules)
2. `cpp-lint` — cppcheck on `src/`
3. `js-lint` — find_dups + check_missing_fns + ESLint
4. `e2e-tests` — Playwright browser tests (26 tests across 19 specs)

See `docs/architecture/ci-quality-gates.mmd` for the pipeline flow diagram.

### Pre-commit Hooks
`.githooks/pre-commit` runs fast checks before every commit:
1. `node tools/find_dups.js` — duplicate JS declarations
2. `node tools/check_missing_fns.js` — undefined function references
3. ESLint on `web_src/js/`

Activate: `git config core.hooksPath .githooks`

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

GitHub Actions (`.github/workflows/tests.yml`): 4 parallel quality gates (cpp-tests, cpp-lint, js-lint, e2e-tests) must all pass before firmware build. Triggers on push/PR to `main` and `develop` branches. A separate `release.yml` workflow runs the same 4 gates before release. Pipeline diagram: `docs/architecture/ci-quality-gates.mmd`. Playwright HTML report uploaded as artifact on failure (14-day retention).

## Serial Debug Logging

All modules use `debug_serial.h` macros (`LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`) with consistent `[ModuleName]` prefixes:

| Module | Prefix | Notes |
|---|---|---|
| `smart_sensing` | `[Sensing]` | Mode changes, threshold, timer, amplifier state, ADC health transitions |
| `i2s_audio` | `[Audio]` | Init, sample rate changes, ADC detection changes. Periodic dump via `audio_periodic_dump()` from main loop |
| `signal_generator` | `[SigGen]` | Init, start/stop, PWM duty, param changes while active |
| `buzzer_handler` | `[Buzzer]` | Init, pattern start/complete, play requests (excludes tick/click to avoid noise) |
| `wifi_manager` | `[WiFi]` | Connection attempts, AP mode, scan results |
| `mqtt_handler` | `[MQTT]` | Connect/disconnect, HA discovery, publish errors |
| `ota_updater` | `[OTA]` | Version checks, download progress, verification |
| `settings_manager` | `[Settings]` | Load/save operations |
| `usb_audio` | `[USB Audio]` | Init, connect/disconnect, streaming start/stop, host volume/mute changes |
| `button_handler` | — | Logged from `main.cpp` (11 LOG calls covering all press types) |
| `gui_*` | `[GUI]` | Navigation, screen transitions, theme changes |
| `hal_*` modules | `[HAL]`, `[HAL Discovery]`, `[HAL DB]`, `[HAL API]` | Device lifecycle, discovery, config save/load |
| `output_dsp` | `[OutputDSP]` | Per-output DSP stage add/remove |

When adding logging to new modules, follow these conventions:
- Use `LOG_I` for state transitions and significant events (start/stop, connect/disconnect, health changes)
- Use `LOG_D` for high-frequency operational details (pattern steps, param snapshots)
- Never log inside ISR paths or real-time FreeRTOS tasks (e.g., `audio_pipeline_task`) — `Serial.print` blocks when UART TX buffer fills, starving DMA and causing audio dropouts. Use dirty-flag pattern: task sets flag, main loop calls `audio_periodic_dump()` for actual Serial/WS output
- Log transitions, not repetitive state (use static `prev` variables to detect changes)
- **Log files**: Save all `.log` files (build output, test reports, serial captures) to the `logs/` directory — keep the project root clean

## Icons

All icons in the web UI **must** use inline SVG paths sourced from [Material Design Icons (MDI)](https://pictogrammers.com/library/mdi/). No external icon CDN or font library is loaded — the page is self-contained and must work offline.

**Standard pattern** (copy the SVG path from pictogrammers.com → select icon → SVG tab):

```html
<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true">
  <path d="<paste MDI path here>"/>
</svg>
```

- Use `fill="currentColor"` so the icon inherits its colour from CSS (`color` property)
- Set explicit `width`/`height` to match the surrounding context (18 px for inline text buttons, 24 px for standalone buttons)
- Always add `aria-hidden="true"` on decorative icons; add `aria-label` on icon-only interactive elements
- When generating icons in JavaScript strings, quote SVG attributes with double quotes and the outer JS string with single quotes (the two don't conflict)

**Reference icons already in use:**

| Icon | MDI name | Used for |
|------|----------|---------|
| ⓘ outline circle | `mdi-information-outline` | Release notes links |
| ✕ | `mdi-close` | Close/dismiss buttons, clear search |
| 💾 | `mdi-content-save` | Save preset button |
| ▲ | `mdi-chevron-up` | Sort ascending, move item up |
| ▼ | `mdi-chevron-down` | Sort descending, move item down |
| ✏ | `mdi-pencil` | Rename/edit action |
| ⚠ triangle | `mdi-alert` | Warning banners and modal titles |
| ✔ circle | `mdi-check-circle` | WiFi connection success status |
| ✕ circle | `mdi-close-circle` | WiFi connection error status |
| 🗑 | `mdi-delete` | Delete DSP stage / preset |

## Key Dependencies

- `ArduinoJson@^7.4.2` — JSON parsing throughout the codebase
- `WebSockets@^2.7.2` — WebSocket server for real-time UI updates
- `PubSubClient@^2.8` — MQTT client for Home Assistant integration
- `lvgl@^9.4` — GUI framework (guarded by `GUI_ENABLED`)
- `LovyanGFX@^1.2.0` — TFT display driver for ST7735S (replaced TFT_eSPI)
- `arduinoFFT@^2.0` — FFT spectrum analysis (**native tests only**; ESP32 uses pre-built ESP-DSP FFT)
- **ESP-DSP pre-built library** (`libespressif__esp-dsp.a`) — S3 assembly-optimized biquad IIR, FIR, Radix-4 FFT, window functions, vector math (mulc/mul/add), dot product, SNR/SFDR analysis. Include paths added via `-I` flags in `platformio.ini`. `lib/esp_dsp_lite/` provides ANSI C fallbacks for native tests only (`lib_ignore = esp_dsp_lite` in ESP32 envs)
