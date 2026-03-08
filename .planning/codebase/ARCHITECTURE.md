# Architecture

**Analysis Date:** 2026-03-08

## Pattern Overview

**Overall:** Event-driven, dirty-flag-signaled embedded firmware with HAL-managed hardware, modular audio pipeline, and FreeRTOS multi-core task isolation.

**Key Characteristics:**
- All mutable application state lives in the `AppState` singleton (`src/app_state.h`), accessed globally via the `appState` macro.
- State mutations set typed dirty flags that atomically signal FreeRTOS event group bits, waking the main loop from `app_events_wait(5)` without polling.
- Hardware is managed exclusively by the HAL framework (`src/hal/`); the audio pipeline sees only abstract `AudioInputSource` / `AudioOutputSink` structs — never raw drivers.
- Core 1 is reserved exclusively for real-time audio (`audio_pipeline_task`). No new tasks may be pinned to Core 1.
- MQTT work (reconnect, publish, subscribe) runs on a dedicated FreeRTOS task on Core 0 to prevent blocking HTTP/WebSocket serving in the main loop.

## Layers

**Application State Layer:**
- Purpose: Central truth store for all runtime state. Provides typed getters/setters that set dirty flags and call `app_events_signal(EVT_*)`.
- Location: `src/app_state.h`, `src/app_state.cpp`
- Contains: FSM state enum (`AppFSMState`), WiFi/MQTT/OTA/audio/DAC/USB/sensing/display/debug fields, per-ADC structs (`AdcState`), cross-task coordination flags (`volatile _pendingDacToggle`, `volatile _mqttReconfigPending`).
- Depends on: `src/app_events.h`
- Used by: Every subsystem — main loop, MQTT task, GUI task, audio pipeline bridge, WebSocket handler, settings manager.

**Event Signaling Layer:**
- Purpose: FreeRTOS `EventGroupHandle_t` wrapped behind a thin API; allows main loop to sleep and wake instantly on any state change.
- Location: `src/app_events.h`, `src/app_events.cpp`
- Contains: 16 assigned event bit definitions (`EVT_OTA` through `EVT_CHANNEL_MAP`, bits 0-15). `EVT_ANY = 0x00FFFFFF` covers all 24 usable bits.
- Depends on: FreeRTOS `freertos/event_groups.h`
- Used by: Every dirty-flag setter in `AppState`; main loop calls `app_events_wait(5)`.

**HAL Framework Layer:**
- Purpose: Device lifecycle management, discovery, config persistence, and pin conflict prevention. The ONLY system that directly touches hardware drivers.
- Location: `src/hal/`
- Contains: `HalDevice` abstract base, `HalDeviceManager` singleton, `HalDeviceDescriptor`, `HalDeviceConfig`, `HalDeviceState` enum, `HalStateChangeCb`, driver implementations, discovery, DB, API.
- Depends on: Arduino IDF drivers, `src/app_state.h`, `src/audio_pipeline.h` (via bridge only).
- Used by: `main.cpp` (registers builtins), `hal_pipeline_bridge` (connects HAL to audio pipeline).

**HAL-Pipeline Bridge Layer:**
- Purpose: Translates HAL device state transitions into audio pipeline sink/source registration and deregistration. Capability-based ordinal counting assigns pipeline slots/lanes dynamically.
- Location: `src/hal/hal_pipeline_bridge.h`, `src/hal/hal_pipeline_bridge.cpp`
- Contains: `_halSlotToSinkSlot[]`, `_halSlotToAdcLane[]` lookup tables, forward/reverse-lookup APIs, correlation ID support.
- Depends on: `HalDeviceManager`, `audio_pipeline_*` API, `AppState`.
- Used by: `HalDeviceManager` state change callback chain, `main.cpp` (calls `hal_pipeline_sync()` at boot).

**Audio Pipeline Layer:**
- Purpose: Real-time audio processing — reads from registered input sources, applies per-input DSP, routes through 16×16 gain matrix, applies per-output DSP, writes to registered output sinks.
- Location: `src/audio_pipeline.h`, `src/audio_pipeline.cpp`
- Contains: Slot-indexed source (`AudioInputSource`) and sink (`AudioOutputSink`) registration, 16×16 routing matrix, DMA buffer management (lazy SRAM alloc), VU metering per source and sink.
- Depends on: `src/audio_input_source.h`, `src/audio_output_sink.h`, `src/dsp_pipeline.h`, `src/output_dsp.h`, FreeRTOS (Core 1 task).
- Used by: HAL drivers (PCM1808, ES8311, PCM5102A, SigGen, USB Audio) register sources/sinks; `websocket_handler` reads VU/waveform/spectrum via `i2s_audio_get_analysis()`.

**DSP Sub-Layer:**
- Purpose: Per-input biquad IIR/FIR chain (guarded by `DSP_ENABLED`) and per-output mono DSP engine post-matrix.
- Location: `src/dsp_pipeline.h/.cpp`, `src/output_dsp.h/.cpp`, `src/dsp_biquad_gen.h/.c`, `src/dsp_crossover.h/.cpp`, `src/dsp_rew_parser.h/.cpp`
- Contains: 31 stage types (`DspStageType`), double-buffered config with `audio_pipeline_notify_dsp_swap()`, PSRAM delay lines, RBJ cookbook coefficient computation.

**Web / API Layer:**
- Purpose: HTTP REST API (port 80) and WebSocket broadcast server (port 81). Handles authentication, settings changes, OTA, HAL management, audio control.
- Location: `src/main.cpp` (route registration), `src/websocket_handler.h/.cpp`, `src/auth_handler.h/.cpp`, `src/mqtt_handler.h/.cpp`, `src/mqtt_publish.cpp`, `src/mqtt_ha_discovery.cpp`, module `*_api.cpp` files.
- Contains: `WebServer server(80)`, `WebSocketsServer webSocket(81)`, all `server.on()` registrations, `send*()` broadcast functions, WS binary frame types `WS_BIN_WAVEFORM` / `WS_BIN_SPECTRUM`.
- Depends on: `AppState`, ArduinoJson, PubSubClient, WebSockets library (vendored in `lib/WebSockets/`).

**Settings/Persistence Layer:**
- Purpose: Dual-format settings persistence with safe atomic write, factory reset, legacy migration.
- Location: `src/settings_manager.h/.cpp`
- Contains: `/config.json` primary format (JSON v1+, atomic `config.json.tmp` → rename), legacy `settings.txt` / `mqtt_config.txt` fallback. WiFi credentials and selected NVS values survive LittleFS format. `saveSettingsDeferred()` debounces rapid saves.

**GUI Layer:**
- Purpose: LVGL v9.4 based TFT display UI on ST7735S 128×160. Runs as `gui_task` on Core 0.
- Location: `src/gui/` (guarded by `GUI_ENABLED`)
- Contains: `gui_manager`, `gui_input` (rotary encoder ISR), `gui_theme`, `gui_navigation` (screen stack), screens in `src/gui/screens/`.

**Diagnostics Layer:**
- Purpose: Structured diagnostic event journal with hot ring buffer (PSRAM, 32 entries) and LittleFS persistence (800 entries), crash log, health bridge.
- Location: `src/diag_journal.h/.cpp`, `src/diag_event.h`, `src/diag_error_codes.h`, `src/crash_log.h/.cpp`, `src/hal/hal_audio_health_bridge.h/.cpp`
- Contains: `diag_emit()` entry point, per-entry CRC32, `EVT_DIAG` dirty flag, boot loop detection (3 consecutive crashes → `halSafeMode`).

## Data Flow

**Audio Signal Path:**

1. PCM1808 ADC(s) write samples via I2S DMA — `i2s_audio_init()` starts `audio_pipeline_task` on Core 1.
2. `audio_pipeline_task` reads from each registered `AudioInputSource.read()` callback (PCM1808, SigGen, USB Audio).
3. Per-input DSP chain applied (`dsp_pipeline_process()`) on raw float32 samples.
4. 16×16 routing matrix mixes inputs to output channels (linear gains, persisted to LittleFS).
5. Per-output DSP chain applied (`output_dsp_process()`).
6. Output written to each registered `AudioOutputSink.write()` callback (PCM5102A, ES8311).
7. VU/waveform/spectrum updated in `AppState.audioAdc[]` — main loop reads via `i2s_audio_get_analysis()` for WS broadcast.

**State Change → WebSocket Broadcast:**

1. Any subsystem (MQTT callback, HTTP handler, GUI, audio task) calls a `mark*Dirty()` method on `AppState`.
2. `mark*Dirty()` sets the dirty bool and calls `app_events_signal(EVT_*)`.
3. Main loop wakes from `app_events_wait(5)`.
4. Main loop checks each dirty flag in sequence, calls corresponding `send*()` function in `websocket_handler`.
5. Dirty flag cleared. MQTT publishes happen independently in `mqtt_task`.

**HAL Device Lifecycle → Audio Pipeline:**

1. Device registered in `HalDeviceManager` at boot (`hal_register_builtins()`, `hal_load_auto_devices()`).
2. `HalDeviceManager::initAll()` runs in priority order (BUS=1000 → LATE=100).
3. On `probe()` success → `init()` → state transitions to `HAL_STATE_AVAILABLE`.
4. `HalStateChangeCb` fires → `hal_pipeline_state_change()` in bridge.
5. Bridge calls `audio_pipeline_set_sink(slot, sink)` for DAC devices or `audio_pipeline_set_source(lane, src)` for ADC devices (under `vTaskSuspendAll()`).
6. On health check failure → `UNAVAILABLE`: `_ready=false` only (auto-recovery). On ERROR/REMOVED/MANUAL → explicit `audio_pipeline_remove_sink(slot)`.

**State Management:**
- All dirty flags stored in `AppState` private members (e.g., `_displayDirty`, `_otaDirty`).
- Cross-task coordination uses `volatile` flags: `_mqttReconfigPending` (web UI → mqtt_task), `_pendingApToggle` (MQTT callback → main loop), `_pendingDacToggle`/`_pendingEs8311Toggle` (WS handler → main loop).
- Change-detection shadow fields (`prevMqtt*`) live as file-local statics in `src/mqtt_publish.cpp`, not in `AppState`.

## FreeRTOS Task Layout

| Task | Core | Priority | Stack | Purpose |
|---|---|---|---|---|
| `loopTask` (Arduino main) | 1 | 1 | default | HTTP serving, WS broadcast, dirty-flag dispatch, smart sensing, OTA scheduling, button handling |
| `audio_pipeline_task` | 1 | 3 | 12288 B | Real-time audio I2S read, DSP, matrix, sink write. Preempts loopTask, yields 2 ticks after DMA |
| `gui_task` | 0 | default | varies | LVGL tick + screen rendering. Guarded by `GUI_ENABLED` |
| `mqtt_task` | 0 | 2 | 4096 B | MQTT reconnect (1-3s blocking TCP), `mqttClient.loop()`, periodic publish at 20 Hz (50ms `vTaskDelay`) |
| `usb_audio_task` | 0 | 1 | 4096 B | TinyUSB UAC2 poll (100ms idle, 1ms streaming). Guarded by `USB_AUDIO_ENABLED` |
| OTA check task | 0 | low | 8192 B | One-shot: GitHub release fetch, SHA256 verify. Launched by `startOTACheckTask()` |
| OTA download task | 0 | low | 8192 B | One-shot: firmware download + flash write. Launched by `startOTADownloadTask()` |

**Core isolation rule:** Core 1 runs only `loopTask` + `audio_pipeline_task`. All other tasks are pinned to Core 0. Do not pin new tasks to Core 1.

## Key Abstractions

**AppState Singleton:**
- Purpose: Central truth store with dirty-flag notification.
- Location: `src/app_state.h`, `src/app_state.cpp`
- Pattern: Meyers singleton (`AppState::getInstance()`), aliased via `#define appState AppState::getInstance()`. Dirty-flag setters call `app_events_signal(EVT_*)`.

**HalDevice (Abstract Base):**
- Purpose: ESPHome-style device abstraction — `probe()`, `init()`, `deinit()`, `dumpConfig()`, `healthCheck()`, `getInputSource()`.
- Location: `src/hal/hal_device.h`
- Pattern: Subclassed by every driver (`HalPcm5102a`, `HalEs8311`, `HalPcm1808`, `HalSigGen`, `HalUsbAudio`, `HalNs4150b`, etc.). Hot-path state (`volatile _ready`, `volatile _state`) is read directly without virtual dispatch by the audio pipeline.

**AudioInputSource / AudioOutputSink:**
- Purpose: C structs with function pointers — zero-overhead callback interface between HAL drivers and audio pipeline.
- Location: `src/audio_input_source.h`, `src/audio_output_sink.h`
- Pattern: HAL drivers own the structs; bridge copies and injects `lane`/`halSlot` before calling `audio_pipeline_set_source()`/`audio_pipeline_set_sink()`. `0xFF` halSlot = not bound to HAL.

**DiagEvent / diag_emit():**
- Purpose: Structured diagnostic events with error codes, severity, correlation IDs, device slot, and message.
- Location: `src/diag_journal.h`, `src/diag_event.h`, `src/diag_error_codes.h`
- Pattern: `diag_emit(code, severity, slot, device, msg)` writes to spinlock-protected PSRAM ring buffer, sets `EVT_DIAG`, prints `[DIAG]` JSON line on serial.

## Entry Points

**`setup()` in `main.cpp`:**
- Location: `src/main.cpp:164`
- Responsibilities: TWDT reconfigure (30s), serial number init, LittleFS init, diag journal init, crash log record, settings load, USB audio init, GUI init (Core 0 task), HAL builtin registration, HAL DB init + auto-device load, I2S audio init, secondary DAC init, HAL pipeline sync, health bridge init, HTTP routes registration, WiFi init, Ethernet init, WebSocket start, MQTT task start, FSM set to `STATE_IDLE`.

**`loop()` in `main.cpp`:**
- Location: `src/main.cpp:946`
- Responsibilities: WDT feed, `app_events_wait(5)`, HTTP client handling, DNS (AP mode captive portal), WebSocket loop, WS auth timeout sweep, button handling, OTA scheduling, smart sensing, dirty-flag dispatch (one flag per subsystem), deferred save checks, HAL health audio bridge check, periodic hardware stats broadcast.

**`audio_pipeline_task` (Core 1):**
- Location: `src/audio_pipeline.cpp` (task body started by `audio_pipeline_init()`)
- Responsibilities: I2S DMA read per registered source, per-input DSP, 16×16 matrix mix, per-output DSP, sink write, VU/waveform/spectrum update. Never calls `Serial.print`. Observes `appState.audioPaused` and gives `audioTaskPausedAck` semaphore for safe I2S reinit.

## Error Handling

**Strategy:** Structured `HalInitResult` codes from HAL device `init()` failures; non-blocking retry with timestamp-based backoff (up to 3 retries); `HAL_STATE_ERROR` after exhaustion. `diag_emit()` records all transitions.

**Patterns:**
- HAL retry: `HalRetryState.count`/`nextRetryMs` — `healthCheckAll()` drives retries without blocking main loop.
- Boot loop guard: 3 consecutive crash boots set `appState.halSafeMode = true` → HAL device registration skipped.
- Heap guard: `heapCritical = true` when `ESP.getMaxAllocHeap() < 40KB` — OTA skipped, DSP PSRAM fallback pre-flight check.
- I2S reinit handshake: `audioPaused` + binary semaphore `audioTaskPausedAck` prevents race between DAC deinit and pipeline task.
- Audio health bridge: ADC lane health (`AudioHealthStatus`) drives HAL state via flap guard (>2 transitions in 30s → `HAL_STATE_ERROR`, `DIAG_HAL_DEVICE_FLAPPING`).

## Cross-Cutting Concerns

**Logging:** `src/debug_serial.h` macros `LOG_D`/`LOG_I`/`LOG_W`/`LOG_E` with `[ModuleName]` prefix. Runtime level control via `appState.debugSerialLevel`. WS log forwarding via `broadcastLine()` with module field extracted from prefix. No logging from ISR or audio task context.

**Validation:** AppState validated setters for unsafe toggle operations: `requestDacToggle(int8_t)` and `requestEs8311Toggle(int8_t)` accept only -1/0/1. HAL config CRUD validated in `hal_api.cpp` before calling `setConfig()`.

**Authentication:** PBKDF2-SHA256 (10,000 iterations, 16-byte salt). Session cookie with `HttpOnly` flag. WS connections authenticated via short-lived token from `GET /api/ws-token` (60s TTL, 16-slot pool, single-use). Login rate limiting via `_nextLoginAllowedMs` (non-blocking HTTP 429 with `Retry-After`).

**Build Feature Flags:**
- `DAC_ENABLED` — HAL framework, DAC/codec drivers, pipeline bridge, HAL API
- `DSP_ENABLED` — DSP pipeline, output DSP, DSP API
- `GUI_ENABLED` — LVGL GUI, TFT display, rotary encoder
- `USB_AUDIO_ENABLED` — TinyUSB UAC2 speaker device
- `NATIVE_TEST` / `UNIT_TEST` — compile without Arduino/FreeRTOS, use mocks in `test/test_mocks/`

---

*Architecture analysis: 2026-03-08*
