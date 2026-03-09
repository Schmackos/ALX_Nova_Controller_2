# Architecture

**Analysis Date:** 2026-03-09

## Pattern Overview

**Overall:** Event-driven, HAL-centric embedded controller with layered subsystems

**Key Characteristics:**
- HAL (Hardware Abstraction Layer) is the sole manager of all hardware devices — no direct hardware access outside HAL drivers
- AppState singleton decomposes into 15 domain-specific state headers, composed via thin shell; dirty flags + FreeRTOS event group replace polling
- Audio pipeline is data-flow: sources (ADC, SigGen, USB) → per-input DSP → 16×16 routing matrix → per-output DSP → slot-indexed sinks (DAC, codec)
- Dual-core design: Core 1 reserved exclusively for audio pipeline task + main Arduino loop; Core 0 handles GUI, MQTT, USB Audio
- Compile-time feature gating via `DAC_ENABLED`, `DSP_ENABLED`, `GUI_ENABLED`, `USB_AUDIO_ENABLED`

## Layers

**Application State Layer:**
- Purpose: Centralised shared state with dirty-flag change detection
- Location: `src/app_state.h`, `src/app_state.cpp`, `src/state/` (15 domain headers)
- Contains: All application state accessed by multiple subsystems; cross-core volatile flags; FSM state; event group signaling
- Depends on: `src/config.h`, `src/app_events.h`, all 15 domain headers
- Used by: Every module in the system
- Access pattern: `appState.domain.field` (e.g., `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`, `appState.dac.pendingToggle`)

**HAL Framework:**
- Purpose: Device lifecycle management, discovery, configuration persistence
- Location: `src/hal/`
- Contains: `HalDevice` abstract base, `HalDeviceManager` singleton (24 slots), driver registry, EEPROM v3 discovery, builtin device registrations, config persistence at `/hal_config.json`
- Depends on: `src/hal/hal_types.h`, platform I2C/I2S drivers, `AppState`
- Used by: Audio pipeline (via `hal_pipeline_bridge`), DSP bridge, main.cpp for device registration

**HAL-Pipeline Bridge:**
- Purpose: Connects HAL device lifecycle state transitions to the audio pipeline source/sink slots
- Location: `src/hal/hal_pipeline_bridge.h/.cpp`
- Contains: State-change callback registered with `HalDeviceManager`; capability-based ordinal counting for slot/lane assignment; mapping tables `_halSlotToSinkSlot[]` and `_halSlotToAdcLane[]`
- Depends on: `HalDeviceManager`, `audio_pipeline.h`, `AppState.audio`
- Used by: Main loop at boot via `hal_pipeline_sync()`

**Audio Pipeline:**
- Purpose: Real-time DSP processing and routing of audio from inputs to outputs
- Location: `src/audio_pipeline.h/.cpp`, `src/audio_input_source.h`, `src/audio_output_sink.h`
- Contains: 8-lane input sources, 16×16 float32 routing matrix, 8-slot output sinks, per-input DSP bypass, per-lane VU metering, noise gate
- Depends on: `i2s_audio.h`, `dsp_pipeline.h`, `output_dsp.h`, HAL devices (via registered callbacks)
- Used by: `audio_pipeline_task` on Core 1 (highest priority)

**DSP Engine:**
- Purpose: Multi-channel biquad IIR, FIR, limiter, compressor, delay, crossover
- Location: `src/dsp_pipeline.h/.cpp`, `src/output_dsp.h/.cpp`, `src/dsp_biquad_gen.h/.c`, `src/dsp_crossover.h/.cpp`, `src/dsp_rew_parser.h/.cpp`, `src/dsp_coefficients.h/.cpp`
- Contains: 4-channel pre-matrix DSP (per input pair), 8-channel post-matrix output DSP, double-buffered config with glitch-free swap, PSRAM delay lines
- Depends on: ESP-DSP pre-built `.a` (S3 assembly-optimised on ESP32); `lib/esp_dsp_lite/` (native tests)
- Used by: Audio pipeline task, `dsp_api.cpp` for REST endpoints

**Connectivity Layer:**
- Purpose: WiFi/Ethernet/MQTT/OTA communications
- Location: `src/wifi_manager.h/.cpp`, `src/eth_manager.h/.cpp`, `src/mqtt_handler.h/.cpp`, `src/mqtt_publish.cpp`, `src/mqtt_ha_discovery.cpp`, `src/mqtt_task.h/.cpp`, `src/ota_updater.h/.cpp`
- Contains: Multi-network WiFi client, AP mode, MQTT with Home Assistant discovery, OTA via GitHub Releases, MQTT task on Core 0
- Depends on: PubSubClient, WiFiClient, HTTPClient, AppState
- Used by: Main loop for WebSocket sync; dedicated `mqtt_task` on Core 0

**Web/WebSocket Layer:**
- Purpose: Browser-based configuration UI and real-time state streaming
- Location: `src/websocket_handler.h/.cpp`, `src/web_pages.h/.cpp`, `src/web_pages_gz.cpp`
- Contains: WebSocket server (port 81) with token-based auth, gzip-compressed HTML/JS/CSS served on port 80, binary audio frames (waveform `0x01`, spectrum `0x02`)
- Depends on: AppState dirty flags; `wsAnyClientAuthenticated()` guard for heavy broadcasts
- Used by: Main loop for client event dispatch

**GUI Layer (TFT display):**
- Purpose: Local display with LVGL v9.4 on ST7735S 160×128
- Location: `src/gui/` (gui_manager, gui_input, gui_navigation, gui_theme, lgfx_config.h, lv_conf.h, screens/)
- Contains: 16 screen implementations, rotary encoder ISR (Gray code), FreeRTOS `gui_task` on Core 0
- Depends on: LovyanGFX, LVGL, AppState (read-only from GUI task)
- Used by: `gui_init()` from main setup (guarded by `GUI_ENABLED`)

**Persistence Layer:**
- Purpose: Settings and configuration storage on LittleFS
- Location: `src/settings_manager.h/.cpp`, `src/hal/hal_settings.h/.cpp`
- Contains: `/config.json` (primary, atomic write via `.tmp` rename), `/hal_config.json`, `/hal_auto_devices.json`, `/dsp_config.json`, `/matrix.json`, `/diag_journal.bin`
- Depends on: ArduinoJson, LittleFS
- Used by: `loadSettings()` / `saveSettings()` called from main loop and HTTP handlers

**Diagnostics Layer:**
- Purpose: Structured event journal, error codes, health monitoring
- Location: `src/diag_journal.h/.cpp`, `src/diag_event.h`, `src/diag_error_codes.h`, `src/crash_log.h/.cpp`, `src/hal/hal_audio_health_bridge.h/.cpp`
- Contains: 32-entry PSRAM hot ring buffer, LittleFS persistence (800 entries), per-entry CRC32, boot-loop detection (3 consecutive crashes → safe mode), ADC health → HAL state bridge with flap guard
- Depends on: AppState dirty flags (`EVT_DIAG`), LittleFS
- Used by: Any module via `diag_emit()`, main loop for journal flush

## Data Flow

**Audio Input to Output:**
1. I2S DMA interrupts fill ring buffers for PCM1808 ADC1 (I2S_NUM_0, GPIO 20/21/22/23) and PCM1808 ADC2 (I2S_NUM_1, GPIO 25)
2. `audio_pipeline_task` (Core 1, priority 3) reads from registered `AudioInputSource` callbacks
3. Per-input noise gate applied (hardware ADC lanes only, via `isHardwareAdc` flag)
4. Per-input pre-matrix DSP (`dsp_pipeline`) applied to active lanes
5. 16×16 float32 routing matrix mixes input channels to output channels
6. Per-output post-matrix DSP (`output_dsp`) applied
7. `AudioOutputSink.write()` callbacks deliver `int32_t` frames to DAC drivers (PCM5102A via I2S_NUM_0 TX, ES8311 via I2S2)
8. VU metering updated on every source read and sink write

**Configuration Change (Web → Firmware State):**
1. Browser sends HTTP POST or WebSocket command
2. HTTP handler or `webSocketEvent()` updates `appState.domain.field`
3. Handler calls `appState.markXxxDirty()` → sets dirty flag + signals `EVT_XXX` event bit
4. Main loop wakes from `app_events_wait(5)` immediately
5. Main loop detects dirty flag, calls `sendXxxState()` to broadcast via WebSocket
6. MQTT task independently reads dirty flags at 20 Hz and publishes via `publishMqttXxx()`

**HAL Device Lifecycle:**
1. `hal_register_builtins()` registers compatible strings + factory functions in `hal_driver_registry`
2. Built-in devices explicitly `probe()` + `init()` in `setup()` sequence
3. Discovered devices: `hal_discover_devices()` → I2C scan → EEPROM probe → `hal_registry_find()` → factory → `registerDevice()` → `probe()` → `init()`
4. State transitions fire `HalStateChangeCb` → `hal_pipeline_state_change()` → `hal_pipeline_on_device_available()` / `on_device_removed()`
5. Bridge calls `audio_pipeline_set_sink(slot)` or `audio_pipeline_set_source(lane)` atomically under `vTaskSuspendAll()`

**State Management:**
- AppState is Meyers singleton accessed via `appState` macro
- 16 event bits assigned (bits 0-15), `EVT_ANY = 0x00FFFFFF` (24 usable)
- Dirty-flag + event-signal pattern: setter calls both `_xxxDirty = true` and `app_events_signal(EVT_XXX)`
- MQTT task polls independently (does NOT consume from event group — avoids fan-out race)
- Cross-core coordination: `volatile bool appState.audio.paused` + binary semaphore `audioTaskPausedAck` for DAC reinit handshake

## Key Abstractions

**HalDevice (abstract base):**
- Purpose: Uniform lifecycle interface for all hardware peripherals
- Examples: `src/hal/hal_pcm5102a.h`, `src/hal/hal_es8311.h`, `src/hal/hal_pcm1808.h`, `src/hal/hal_buzzer.h`, `src/hal/hal_encoder.h`, `src/hal/hal_ns4150b.h`
- Pattern: Inherits `HalDevice`; implements `probe()`, `init()`, `deinit()`, `dumpConfig()`, `healthCheck()`; optionally overrides `getInputSource()` for ADC devices; volatile `_ready` + `_state` for lock-free reads from audio task

**AudioInputSource (struct):**
- Purpose: Callback interface for any audio input registered with the pipeline
- Examples: `HalPcm1808::getInputSource()` returns pre-baked thunks for port 0/1; `HalSigGen` software source; `HalUsbAudio` USB source
- Pattern: `read()`, `isActive()`, `getSampleRate()` function pointers; `halSlot` for O(1) reverse-lookup; `isHardwareAdc` for noise gate gating

**AudioOutputSink (struct):**
- Purpose: Callback interface for any audio output registered with the pipeline
- Examples: PCM5102A I2S TX in `src/hal/hal_pcm5102a.cpp`, ES8311 in `src/hal/hal_es8311.cpp`
- Pattern: `write()`, `isReady()` function pointers; `firstChannel` + `channelCount` for matrix slice; `halSlot` for bridge reverse-lookup

**AppState Domain Headers (15 headers):**
- Purpose: Decomposed state structs — each domain module includes only what it needs
- Examples: `src/state/audio_state.h` (AdcState[], I2sRuntimeMetrics), `src/state/dac_state.h` (DacState, PendingDeviceToggle), `src/state/dsp_state.h` (DspSettingsState), `src/state/wifi_state.h` (WifiState)
- Pattern: Plain structs with in-class defaults; composed into `AppState` shell; `appState.domain.field` access

## Entry Points

**Firmware Boot (`setup()`):**
- Location: `src/main.cpp` line 166
- Triggers: Power-on or reset
- Responsibilities: TWDT config, serial number generation, LittleFS init, diagnostic journal, crash log, settings load, USB Audio init, GUI init (if enabled), output DSP init, HAL framework init (`hal_register_builtins()` → `hal_db_init()` → `hal_load_device_configs()` → `hal_provision_defaults()`), `i2s_audio_init()`, `hal_pipeline_sync()`, HTTP/WebSocket server registration, WiFi + Ethernet init, MQTT task start, event group init

**Main Loop (`loop()`):**
- Location: `src/main.cpp` line 958
- Triggers: Continuous (wakes from `app_events_wait(5)`)
- Responsibilities: WDT feed, HTTP request handling, WebSocket dispatch, WiFi reconnect, button handling, smart sensing FSM, dirty-flag → WebSocket broadcast, periodic health checks (HAL audio health, task monitor, heap monitor, deferred saves)

**Audio Pipeline Task:**
- Location: `src/audio_pipeline.cpp` (task function)
- Triggers: FreeRTOS task on Core 1, priority 3
- Responsibilities: DMA buffer reads from all registered input sources, DSP processing, matrix mixing, output DSP, sink writes, VU metering. Yields 2 ticks after DMA write to allow main loop on Core 1.

**MQTT Task:**
- Location: `src/mqtt_task.cpp`
- Triggers: FreeRTOS task on Core 0, priority 2, 50ms poll
- Responsibilities: `mqttClient.loop()`, reconnect on disconnect or `_mqttReconfigPending`, periodic HA state publish via `mqttPublishPendingState()` + `mqttPublishHeartbeat()`

**GUI Task:**
- Location: `src/gui/gui_manager.cpp`
- Triggers: FreeRTOS task on Core 0, `TASK_CORE_GUI=0`
- Responsibilities: `lv_timer_handler()`, encoder input polling, screen refresh, sleep/wake management

## Error Handling

**Strategy:** Multi-tier — HAL retry + flap guard, crash log ring buffer, boot-loop safe mode

**Patterns:**
- HAL device failures: `HalInitResult` struct returned from `init()`; non-blocking retry with timestamp backoff (max 3 retries → `HAL_STATE_ERROR`); NVS fault counter
- Audio health: `hal_audio_health_bridge` polls `AudioHealthStatus` per ADC lane every 5s; drives `HAL_STATE_AVAILABLE` ↔ `UNAVAILABLE`; flap guard prevents `ERROR` on transient issues
- Crash log: `crashlog_record()` in setup writes reset reason to LittleFS ring buffer; 3 consecutive crash boots → `appState.halSafeMode = true` (skips HAL init, WiFi+web only)
- DAC/ES8311 toggle: deferred via `appState.dac.requestDeviceToggle()` — main loop executes, not MQTT callback, to prevent audio task race
- I2S pause handshake: `appState.audio.paused = true` + `xSemaphoreTake(audioTaskPausedAck, 50ms)` before `i2s_driver_uninstall()`

## Cross-Cutting Concerns

**Logging:** `debug_serial.h` macros — `LOG_D/I/W/E` with `[ModuleName]` prefix; runtime level via `applyDebugSerialLevel()`; WebSocket forwarding with `module` JSON field for frontend filtering. Never log in ISR or audio task (use dirty-flag pattern).

**Validation:** AppState dirty-flag setters validate before accepting (e.g., `requestDeviceToggle()` rejects halSlot=0xFF or action outside -1..1). HAL discovery skips Bus 0 (GPIO 48/54) when WiFi active (SDIO conflict).

**Authentication:** Session cookie (`HttpOnly`), PBKDF2-SHA256 password hashing, WebSocket token pool (16 slots, 60s TTL, single-use from `GET /api/ws-token`), login rate limiting returning HTTP 429.

---

*Architecture analysis: 2026-03-09*
