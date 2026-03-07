# Architecture

**Analysis Date:** 2026-03-07

## Pattern Overview

**Overall:** Event-driven embedded firmware with modular subsystem handlers

**Key Characteristics:**
- Singleton `AppState` holds all shared state; modules communicate via dirty flags + FreeRTOS event group, not direct calls
- Main loop on Core 1 (`loopTask`) is the sole consumer of the FreeRTOS event group; `app_events_wait(5)` replaces `delay(5)` so it wakes immediately when any dirty flag is set
- Audio pipeline runs on a dedicated FreeRTOS task (Core 1, priority 3); all other non-audio tasks are pinned to Core 0
- Feature modules are conditionally compiled via build flags: `DSP_ENABLED`, `DAC_ENABLED`, `GUI_ENABLED`, `USB_AUDIO_ENABLED`

## Layers

**Application State Layer:**
- Purpose: Single source of truth for all runtime state; dirty flags signal the main loop
- Location: `src/app_state.h`, `src/app_state.cpp`, `src/app_events.h`, `src/app_events.cpp`
- Contains: `AppState` singleton (Meyers), `AppFSMState` enum, dirty flag accessors, cross-task volatile coordination flags
- Pattern: Every setter calls both `_flag = true` and `app_events_signal(EVT_XXX)`. Consumers call `clearXxxDirty()` after broadcasting. 16 event bits assigned (EVT_OTA, EVT_DISPLAY, EVT_BUZZER, EVT_DAC, etc.)

**HAL Framework Layer:**
- Purpose: Hardware abstraction for all physical devices; manages lifecycle, discovery, configuration persistence
- Location: `src/hal/`
- Contains: `HalDevice` abstract base, `HalDeviceManager` singleton, driver implementations, pipeline bridge, device database
- Depends on: `src/app_state.h`, `src/audio_pipeline.h`, `src/diag_journal.h`
- Used by: `src/main.cpp`, `src/audio_pipeline.cpp`, `src/dac_hal.cpp`
- Device lifecycle: `UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ⇄ UNAVAILABLE → ERROR / REMOVED / MANUAL`
- State change fires `HalStateChangeCb` → `hal_pipeline_bridge` → audio pipeline sink set/remove

**Audio Processing Layer:**
- Purpose: Real-time audio pipeline: 4-lane input → per-input DSP → 8×8 routing matrix → per-output DSP → slot-indexed sink dispatch
- Location: `src/audio_pipeline.h/.cpp`, `src/i2s_audio.h/.cpp`, `src/dsp_pipeline.h/.cpp`, `src/output_dsp.h/.cpp`
- Contains: DMA buffer management, float32 pipeline, routing matrix, VU/RMS metering, DSP stage chains
- Internal representation: float32 [-1.0, +1.0]; int32↔float conversion only at DMA edges
- DMA raw buffers: internal SRAM (DMA cannot access PSRAM). Float working buffers: PSRAM via `ps_calloc`

**Subsystem Module Layer:**
- Purpose: Independent feature modules; each owns its own state and exposes a narrow C API
- Location: `src/` (one `.h/.cpp` pair per module)
- Key modules: `smart_sensing`, `wifi_manager`, `mqtt_handler`, `mqtt_task`, `ota_updater`, `settings_manager`, `auth_handler`, `button_handler`, `buzzer_handler`, `signal_generator`, `usb_audio`, `eth_manager`, `crash_log`, `task_monitor`, `diag_journal`
- Pattern: Modules read/write `appState` members and call `appState.markXxxDirty()` to notify the main loop

**Web / Network Layer:**
- Purpose: HTTP REST API (port 80) and WebSocket real-time updates (port 81)
- Location: `src/main.cpp` (route registration), `src/websocket_handler.h/.cpp`, `src/auth_handler.h/.cpp`
- Contains: `WebServer server(80)`, `WebSocketsServer webSocket(81)`, gzip-served HTML, REST endpoints, WS broadcast functions
- Authentication: session cookie (`requireAuth()` guard on all protected routes)
- Web UI source: `web_src/` (never edit `src/web_pages.cpp` directly)

**GUI Layer (guarded by `GUI_ENABLED`):**
- Purpose: LVGL v9.4 TFT display (ST7735S 160×128) with rotary encoder input, running on Core 0
- Location: `src/gui/`
- Contains: `gui_manager` (task + sleep/wake), `gui_input` (ISR Gray-code encoder), `gui_theme`, `gui_navigation` (screen stack), `src/gui/screens/` (14 screen implementations)
- Depends on: `AppState` for data; sets dirty flags to propagate changes to WS/MQTT

## Data Flow

**Audio Signal Path:**

1. I2S DMA interrupt fills `_rawBuf[lane]` (int32, internal SRAM) for each PCM1808 ADC lane
2. `audio_pipeline_task` (Core 1) converts raw int32 → float32 into `_laneL/R[lane]` (PSRAM)
3. Per-lane DSP chain applied (`dsp_pipeline`) when `DSP_ENABLED` and not bypassed
4. Float32 lanes fed into 8×8 routing matrix; output channels accumulated into `_outCh[ch]` (PSRAM)
5. Per-output DSP chain applied (`output_dsp`) when `DSP_ENABLED`
6. Float32 output channels converted back to int32 in `_dacBuf` (internal SRAM)
7. Pipeline iterates `_sinks[]` array, calls `sink->write()` for each AVAILABLE, non-muted sink
8. VU/RMS metering updated on `AudioInputSource.vuL/R` and `AudioOutputSink.vuL/R` after each read/write

**State Change / Dirty Flag Flow:**

1. Any subsystem (ISR, MQTT callback, HTTP handler, GUI task, OTA task) sets a field on `appState` and calls `appState.markXxxDirty()` / `app_events_signal(EVT_XXX)`
2. Main loop wakes from `app_events_wait(5)` (< 1 µs latency on event; 5 ms max idle)
3. Main loop checks each `appState.isXxxDirty()` and calls the matching `sendXxxState()` WebSocket broadcast
4. After broadcast, main loop calls `appState.clearXxxDirty()`
5. `mqtt_task` independently polls `appState` at 20 Hz for MQTT publishes (does NOT use the event group)

**HAL Device Lifecycle → Audio Pipeline:**

1. HAL driver calls `hal_state_set_state(slot, newState)` in `HalDeviceManager`
2. `HalDeviceManager` fires `_stateChangeCb(slot, old, new)` → `hal_pipeline_state_change()`
3. `hal_pipeline_bridge` dispatches to `on_device_available`, `on_device_unavailable`, or `on_device_removed`
4. For DAC/CODEC devices: `audio_pipeline_set_sink(sinkSlot, sink)` or `audio_pipeline_remove_sink(sinkSlot)` under `vTaskSuspendAll()`
5. For ADC devices: `appState.adcEnabled[lane]` toggled + `appState.markAdcEnabledDirty()`
6. UNAVAILABLE → `sink->_ready = false` only (transient, auto-recovers). ERROR/REMOVED → explicit sink removal

**OTA Update Flow:**

1. Main loop calls `startOTACheckTask()` every 5 minutes (Core 0 one-shot task)
2. OTA task checks GitHub releases API via HTTPS; sets `appState.updateAvailable` and `appState.markOTADirty()`
3. Main loop wakes on `EVT_OTA`, calls `broadcastUpdateStatus()` to WS clients
4. On manual or auto-trigger: `startOTADownloadTask()` downloads firmware, verifies SHA256, calls `Update.apply()`
5. Post-update boot: `checkAndClearOTASuccessFlag()` detects flash and sets `appState.justUpdated`

## Key Abstractions

**AppState:**
- Purpose: Global singleton holding all shared mutable state with dirty-flag notification
- Examples: `src/app_state.h` line 68 (`class AppState`), `src/app_state.cpp`
- Pattern: `appState.markXxxDirty()` signals `EVT_XXX` to the FreeRTOS event group; main loop drains flags

**HalDevice (abstract base):**
- Purpose: Polymorphic device driver contract — `probe()`, `init()`, `deinit()`, `healthCheck()`, `dumpConfig()`
- Examples: `src/hal/hal_device.h`, concrete drivers: `src/hal/hal_pcm5102a.h/.cpp`, `src/hal/hal_es8311.h/.cpp`, `src/hal/hal_pcm1808.h/.cpp`
- Pattern: `volatile _ready` and `volatile _state` enable lock-free reads from the audio task on Core 1

**AudioInputSource / AudioOutputSink (C structs):**
- Purpose: Interface between audio drivers and the pipeline; function pointers for `read()` / `write()` / `isReady()`
- Examples: `src/audio_input_source.h`, `src/audio_output_sink.h`
- Pattern: Slot-indexed array in pipeline; `halSlot` field enables O(1) HAL device reverse lookup

**AppFSMState:**
- Purpose: Top-level application finite state machine
- States: `STATE_IDLE → STATE_SIGNAL_DETECTED → STATE_AUTO_OFF_TIMER → STATE_WEB_CONFIG / STATE_OTA_UPDATE / STATE_ERROR`
- Location: `src/app_state.h` (enum) + `src/app_state.cpp` (`setFSMState()`)

**DiagEvent / DiagJournal:**
- Purpose: Structured diagnostic event capture with 32-entry hot ring (PSRAM) + 800-entry LittleFS persistent log
- Examples: `src/diag_journal.h/.cpp`, `src/diag_event.h`, `src/diag_error_codes.h`
- Pattern: `diag_emit(code, severity, slot, device, msg)` from any context (not ISR); `diag_journal_flush()` called periodically from main loop

## Entry Points

**Firmware Boot (`setup()`):**
- Location: `src/main.cpp` line 161
- Responsibilities: TWDT reconfigure, serial number init, LittleFS mount, crash log, settings load, GUI init, HAL framework init, I2S audio init, DAC/ES8311 init, HTTP routes registration, WiFi connect, WebSocket start, MQTT task launch, event group init

**Main Loop (`loop()`):**
- Location: `src/main.cpp` line 816
- Triggers: `app_events_wait(5)` — wakes on any event bit or 5 ms timeout
- Responsibilities: HTTP client serving, WebSocket loop, WiFi check, button handling, OTA check, smart sensing, dirty flag dispatch (WS broadcasts), deferred saves, DAC toggle execution, HAL health check, audio data broadcast

**Audio Pipeline Task:**
- Location: `src/audio_pipeline.cpp` (`audio_pipeline_task`, created by `audio_pipeline_init()`)
- Core: 1, Priority: 3 (preempts main loop during DMA processing)
- Responsibilities: I2S DMA read, float conversion, DSP, matrix, output DSP, sink dispatch, VU metering

**MQTT Task:**
- Location: `src/mqtt_task.cpp`
- Core: 0, Priority: 2
- Responsibilities: MQTT reconnect (blocking TCP), `mqttClient.loop()`, periodic HA publishes at 20 Hz. Never calls WebSocket, LittleFS, or WiFi functions.

**GUI Task (Core 0, `GUI_ENABLED`):**
- Location: `src/gui/gui_manager.cpp`
- Responsibilities: LVGL tick, display refresh, screen transitions, sleep/wake based on `appState.backlightOn`

## Error Handling

**Strategy:** `HalInitResult` struct returned by `HalDevice::init()` carries error code and reason string. Self-healing retry logic in `HalDeviceManager::healthCheckAll()` — up to 3 non-blocking retries with timestamp-based backoff. After 3 failures: device transitions to `HAL_STATE_ERROR` and `_faultCount[slot]` incremented (NVS-backed).

**Patterns:**
- HAL devices: `HalInitResult` with typed `DiagErrorCode` + reason string; `diag_emit()` on every state transition
- Audio pipeline: `i2s_audio_is_healthy()` returns `AudioHealthStatus` enum; `hal_audio_health_bridge` drives HAL state transitions
- HTTP handlers: JSON `{"success":false,"message":"..."}` with appropriate HTTP status codes
- Main loop: `heapCritical` flag (`ESP.getMaxAllocHeap() < 40KB`) blocks OTA and heavy allocations

## Cross-Cutting Concerns

**Logging:** `src/debug_serial.h` macros `LOG_D/I/W/E` with `[ModuleName]` prefix. `DebugOut` forwards to Serial and WebSocket simultaneously. Level controlled at runtime via `appState.debugSerialLevel`. Never log from ISR or audio task — use dirty-flag pattern.

**Validation:** HTTP handler level (JSON deserialize check + field validation). HAL layer: `probe()` before `init()`. DSP: pool exhaustion rolls back stage add.

**Authentication:** Session token cookie (`auth_handler.h`). `requireAuth()` guard on every protected HTTP route. WebSocket clients get 5s auth window after connect; unauthenticated clients disconnected on timeout.

**Persistence:** Settings → NVS via `Preferences`. DSP config → LittleFS JSON. HAL config → `/hal_config.json`. Matrix routing → `/matrix_config.json`. Crash log → LittleFS ring. Diagnostic journal → `/diag_journal.bin`.

**PSRAM Strategy:** Float audio working buffers (`_laneL/R`, `_outCh`) and DSP delay lines allocated via `ps_calloc()`. Heap pre-flight check (40KB reserve) on fallback. `heapCritical` flag monitored every 30s.

---

*Architecture analysis: 2026-03-07*
