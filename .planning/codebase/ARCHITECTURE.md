# Architecture

**Analysis Date:** 2026-03-10

## Pattern Overview

**Overall:** Embedded event-driven monolith with HAL device framework

**Key Characteristics:**
- Arduino `setup()`/`loop()` main structure on FreeRTOS, targeting ESP32-P4
- Singleton AppState with dirty-flag event-driven change propagation
- Hardware Abstraction Layer (HAL) with ESPHome-inspired device lifecycle (probe/init/deinit/healthCheck)
- Slot-indexed audio pipeline with N-input M-output routing matrix
- Multi-core architecture: Core 1 reserved for audio, Core 0 for everything else
- REST API + WebSocket server for web UI, MQTT for Home Assistant integration
- Conditional compilation via build flags: `DAC_ENABLED`, `DSP_ENABLED`, `GUI_ENABLED`, `USB_AUDIO_ENABLED`

## Layers

**Configuration & State Layer:**
- Purpose: Centralized application state, constants, pin definitions, event signaling
- Location: `src/app_state.h`, `src/config.h`, `src/app_events.h`, `src/state/`
- Contains: AppState singleton (composed of 15 domain state structs), dirty flags, FSM state, event bit definitions, pin/task constants
- Depends on: Nothing (leaf layer)
- Used by: Every module in the system

**HAL Device Layer:**
- Purpose: Hardware abstraction for all devices (DAC, ADC, codec, amp, sensor, GPIO peripherals)
- Location: `src/hal/`
- Contains: Device base class (`hal_device.h`), device manager singleton (`hal_device_manager.h/.cpp`), driver registry (`hal_driver_registry.h/.cpp`), discovery engine (`hal_discovery.h/.cpp`), device database (`hal_device_db.h/.cpp`), individual drivers (`hal_pcm5102a`, `hal_es8311`, `hal_pcm1808`, `hal_ns4150b`, `hal_buzzer`, `hal_button`, `hal_encoder`, `hal_led`, `hal_relay`, `hal_signal_gen`, `hal_siggen`, `hal_usb_audio`, `hal_mcp4725`, `hal_temp_sensor`, `hal_display`, `hal_custom_device`), pipeline bridge (`hal_pipeline_bridge.h/.cpp`), audio health bridge (`hal_audio_health_bridge.h/.cpp`), REST API (`hal_api.h/.cpp`), settings persistence (`hal_settings.h/.cpp`), EEPROM v3 protocol (`hal_eeprom_v3.h/.cpp`)
- Depends on: Configuration & State Layer, Audio Pipeline (for sink/source registration via bridge)
- Used by: Main loop (device registration, health checks), Audio Pipeline (sink/source dispatch), Web/WS/MQTT (device state queries)

**Audio Pipeline Layer:**
- Purpose: Real-time audio processing: input capture, DSP, matrix routing, output dispatch
- Location: `src/audio_pipeline.h/.cpp`, `src/audio_input_source.h`, `src/audio_output_sink.h`, `src/i2s_audio.h/.cpp`, `src/dsp_pipeline.h/.cpp`, `src/output_dsp.h/.cpp`, `src/dsp_biquad_gen.h/.c`, `src/dsp_coefficients.h/.cpp`, `src/dsp_crossover.h/.cpp`, `src/dsp_rew_parser.h/.cpp`, `src/dsp_convolution.h/.cpp`, `src/signal_generator.h/.cpp`, `src/usb_audio.h/.cpp`, `src/sink_write_utils.h/.cpp`
- Contains: 8-lane input source management, 4-channel biquad/FIR DSP with double-buffered config swap, 16x16 routing matrix, 8-slot output sink dispatch, VU metering, FFT analysis, signal generator, USB Audio UAC2 device
- Depends on: Configuration & State (sample rates, bypass flags), HAL layer (source/sink structs provided by HAL drivers)
- Used by: HAL Pipeline Bridge (registers sources/sinks), WebSocket handler (VU/spectrum data), DSP API (config management)

**DAC Subsystem Layer:**
- Purpose: DAC device activation/deactivation, volume/mute control, I2S TX bridge, EEPROM diagnostics
- Location: `src/dac_hal.h/.cpp`, `src/dac_api.h/.cpp`, `src/dac_eeprom.h/.cpp`
- Contains: DAC lifecycle management (`dac_activate_for_hal`, `dac_deactivate_for_hal`), slot-indexed volume/mute, REST API endpoints, EEPROM hardware ID reading
- Depends on: HAL Device Layer, Audio Pipeline Layer (sink registration), I2S audio
- Used by: HAL Pipeline Bridge (device activation), Main loop (deferred toggle processing), Web API

**Smart Sensing & Control Layer:**
- Purpose: Audio signal detection, auto-off timer, amplifier relay control
- Location: `src/smart_sensing.h/.cpp`
- Contains: Signal detection with configurable threshold, FSM state transitions (IDLE/SIGNAL_DETECTED/AUTO_OFF_TIMER), amplifier relay control
- Depends on: Configuration & State (sensing mode, threshold, timer duration), Audio Pipeline (VU levels)
- Used by: Main loop (periodic update call)

**Network & Communication Layer:**
- Purpose: WiFi management, MQTT broker integration with Home Assistant, Ethernet
- Location: `src/wifi_manager.h/.cpp`, `src/mqtt_handler.h/.cpp`, `src/mqtt_publish.cpp`, `src/mqtt_ha_discovery.cpp`, `src/mqtt_task.h/.cpp`, `src/eth_manager.h/.cpp`
- Contains: Multi-network WiFi with AP mode, MQTT lifecycle (separate FreeRTOS task on Core 0), Home Assistant auto-discovery, Ethernet 100Mbps
- Depends on: Configuration & State (WiFi/MQTT credentials), Globals (PubSubClient, WiFiClient)
- Used by: Main loop (WiFi monitoring, AP toggle), WebSocket handler (connection status)

**Web Server & API Layer:**
- Purpose: HTTP REST API, WebSocket real-time state broadcasting, authentication
- Location: `src/main.cpp` (route registration), `src/websocket_handler.h/.cpp`, `src/auth_handler.h/.cpp`, `src/web_pages.h/.cpp`, `src/web_pages_gz.cpp`, `src/login_page.h`, `src/dsp_api.h/.cpp`, `src/pipeline_api.h/.cpp`
- Contains: All `server.on()` route registrations (in `main.cpp`), WebSocket event handler, binary audio streaming, state broadcasting functions, session auth with PBKDF2, WS token auth (16-slot pool, 60s TTL)
- Depends on: Every subsystem (reads state for broadcasting, dispatches commands to handlers)
- Used by: Web frontend clients, external API consumers

**GUI Layer:**
- Purpose: LVGL-based TFT display with rotary encoder navigation
- Location: `src/gui/`
- Contains: GUI manager (`gui_manager.h/.cpp`), input handling (`gui_input.h/.cpp`), theme (`gui_theme.h/.cpp`), navigation stack (`gui_navigation.h/.cpp`), 14 screen modules in `src/gui/screens/`
- Depends on: Configuration & State (display settings, all subsystem states), LVGL library
- Used by: Dedicated `gui_task` FreeRTOS task on Core 0

**Diagnostics Layer:**
- Purpose: Structured event logging, crash detection, task monitoring
- Location: `src/diag_event.h`, `src/diag_error_codes.h`, `src/diag_journal.h/.cpp`, `src/crash_log.h/.cpp`, `src/task_monitor.h/.cpp`, `src/debug_serial.h/.cpp`
- Contains: 64-byte binary DiagEvent journal (LittleFS-persisted ring buffer), error code taxonomy, crash log ring buffer, FreeRTOS task stack/timing monitoring, level-filtered serial output with WS forwarding
- Depends on: Configuration & State (debug flags), LittleFS
- Used by: All modules (emit diagnostic events), Main loop (periodic flush, monitoring), WebSocket (broadcast events)

**Persistence Layer:**
- Purpose: Settings storage, config migration, NVS key-value storage
- Location: `src/settings_manager.h/.cpp`, `src/hal/hal_settings.h/.cpp`
- Contains: Dual-format persistence (`/config.json` primary, legacy `.txt` migration), HAL config persistence (`/hal_config.json`), deferred save with debouncing, factory reset, export/import
- Depends on: LittleFS, NVS Preferences
- Used by: Main loop (deferred save checks), Setup (load on boot), API handlers (save on config change)

**Web Frontend Layer:**
- Purpose: Single-page web UI for device configuration and monitoring
- Location: `web_src/` (source), `src/web_pages.cpp` + `src/web_pages_gz.cpp` (generated)
- Contains: Concatenated JS modules (22 files in load order), CSS split by concern (7 files), HTML shell
- Depends on: WebSocket connection to firmware, REST API endpoints
- Used by: End users via browser

## Data Flow

**Audio Signal Path (real-time, Core 1):**

1. HAL PCM1808 drivers provide `AudioInputSource` structs with `read()` callbacks
2. `hal_pipeline_bridge` registers sources at dynamically assigned lanes via `audio_pipeline_set_source()`
3. `audio_pipeline_task` (Core 1, priority 3) reads from all active input sources
4. Per-input DSP processing via `dsp_process_buffer_float()` (double-buffered config, glitch-free swap)
5. 16x16 routing matrix applies gain coefficients to route input channels to output channels
6. Per-output DSP via `output_dsp_process()` (biquad, gain, limiter, compressor)
7. Slot-indexed sink dispatch: iterates `AUDIO_OUT_MAX_SINKS` (8), calls `sink->write()` for each ready, non-muted sink
8. VU metering computed per-source and per-sink after each buffer cycle

**State Change Propagation (cross-core):**

1. Any module sets a dirty flag on AppState (e.g., `appState.markDacDirty()`)
2. Dirty flag setter calls `app_events_signal(EVT_XXX)` which sets a bit in the FreeRTOS event group
3. Main loop's `app_events_wait(5)` wakes immediately on any event bit (or 5ms timeout when idle)
4. Main loop checks dirty flags, calls corresponding `send*()` WebSocket broadcast function
5. MQTT task independently checks state at 20Hz and publishes changes from its own shadow-field change detection

**HAL Device Lifecycle:**

1. Boot: `hal_register_builtins()` creates driver instances and calls `registerDevice()` on HalDeviceManager
2. Discovery: `hal_discover_devices()` scans I2C buses, probes EEPROMs, matches compatible strings to driver registry
3. Init: `HalDeviceManager::initAll()` sorts by priority (BUS=1000 first, LATE=100 last), calls `probe()` then `init()`
4. State callback: Every `_state` transition fires `HalStateChangeCb` registered by `hal_pipeline_bridge`
5. Bridge action: AVAILABLE triggers `buildSink()` + `audio_pipeline_set_sink()` (DAC) or `audio_pipeline_set_source()` (ADC); ERROR/REMOVED triggers removal
6. Health: `healthCheckAll()` every 30s; 3 consecutive failures + retry exhaustion transitions to ERROR with NVS fault counter

**Web UI Command Flow:**

1. Browser sends WebSocket text message (JSON command) or HTTP POST (REST API)
2. `webSocketEvent()` or `server.on()` handler processes the command
3. Handler updates AppState fields and calls `markXxxDirty()`
4. Main loop detects dirty flag, broadcasts new state to all authenticated WS clients
5. For device toggles: handler calls `appState.halCoord.requestDeviceToggle(halSlot, action)` which queues the toggle; main loop drains the queue and calls `hal_pipeline_activate_device()` / `hal_pipeline_deactivate_device()`

**State Management:**
- Singleton `AppState` (access via `appState` macro) composed of 15 domain state structs
- Cross-core fields use `volatile` (e.g., `audioPaused`, `_ready`)
- Dirty flags + FreeRTOS event group for change notification
- Binary semaphore `audioTaskPausedAck` for deterministic I2S driver reinstall handshake
- `vTaskSuspendAll()` for atomic sink/source slot updates (both tasks on Core 1)

## Key Abstractions

**HalDevice (abstract base class):**
- Purpose: Uniform interface for all hardware devices regardless of bus type
- Examples: `src/hal/hal_pcm5102a.h/.cpp`, `src/hal/hal_es8311.h/.cpp`, `src/hal/hal_pcm1808.h/.cpp`, `src/hal/hal_buzzer.h/.cpp`, `src/hal/hal_encoder.h/.cpp`
- Pattern: ESPHome-inspired lifecycle: `probe()` -> `init()` -> `healthCheck()` -> `deinit()`. Volatile `_ready` and `_state` for lock-free cross-core reads. Virtual `getInputSource()` for ADC devices, virtual `buildSink()` for DAC devices.

**HalAudioDevice (extends HalDevice):**
- Purpose: Audio-specific device interface (volume, mute, sample rate, filter mode)
- Examples: `src/hal/hal_pcm5102a.h/.cpp`, `src/hal/hal_es8311.h/.cpp`
- Pattern: Adds `configure()`, `setVolume()`, `setMute()`, `setFilterMode()`, `buildSink()` override

**AudioInputSource (C struct):**
- Purpose: Uniform input interface for the audio pipeline
- Examples: PCM1808 ADC, signal generator (`HalSigGen`), USB Audio (`HalUsbAudio`)
- Pattern: Function pointer callbacks: `read()`, `isActive()`, `getSampleRate()`. Includes VU metering fields, HAL slot binding, hardware ADC flag for noise gating.

**AudioOutputSink (C struct):**
- Purpose: Uniform output interface for the audio pipeline
- Examples: PCM5102A DAC sink, ES8311 codec sink
- Pattern: Function pointer callbacks: `write()`, `isReady()`. Includes volume gain, mute flag, VU metering fields, HAL slot binding, opaque context pointer.

**DspState / DspStage (double-buffered config):**
- Purpose: Real-time DSP processing with glitch-free config updates
- Examples: `src/dsp_pipeline.h/.cpp`
- Pattern: Two DspState instances (active read by audio task, inactive modified by API). `dsp_swap_config()` atomically swaps pointers. Stage union holds parameters for 30+ DSP effect types. Pool-allocated FIR taps and delay lines.

**DiagEvent (64-byte struct):**
- Purpose: Structured diagnostic logging with severity, error codes, correlation IDs
- Examples: `src/diag_event.h`, `src/diag_journal.h/.cpp`
- Pattern: Binary-serializable fixed-size struct. Ring buffer in PSRAM (hot) + LittleFS (persistent). `diag_emit()` writes to journal and signals `EVT_DIAG`.

## Entry Points

**Arduino setup():**
- Location: `src/main.cpp:165`
- Triggers: Power-on / reset
- Responsibilities: Watchdog config, LittleFS init, crash log, settings load, HAL device registration + discovery, I2S audio init, HAL pipeline sync, peripheral registration (NS4150B, buzzer, button, encoder, display, LED, relay, signal gen), WiFi/Ethernet init, web server + WS start, MQTT task start, event group init

**Arduino loop():**
- Location: `src/main.cpp:957`
- Triggers: Continuously after setup() on Core 1 (loopTask, priority 1)
- Responsibilities: Watchdog feed, event wait (5ms), HTTP/WS/DNS service, WiFi monitoring, button handling, OTA check/update, smart sensing, dirty flag dispatch to WS broadcasts, deferred saves, HAL health checks, audio health bridge, heap monitoring, HW stats broadcast, audio data streaming, buzzer processing

**audio_pipeline_task:**
- Location: `src/audio_pipeline.cpp` (created by `audio_pipeline_init()`)
- Triggers: FreeRTOS task on Core 1, priority 3
- Responsibilities: I2S DMA read from all active input sources, per-input DSP, matrix routing, per-output DSP, sink dispatch, VU metering. Yields 2 ticks after each buffer cycle.

**mqtt_task:**
- Location: `src/mqtt_task.cpp` (created by `mqtt_task_init()`)
- Triggers: FreeRTOS task on Core 0, priority 2, polls at 20Hz (50ms vTaskDelay)
- Responsibilities: MQTT reconnect (blocking TCP, never stalls main loop), `mqttClient.loop()`, periodic HA state publishes, heartbeat, reconfiguration on `_mqttReconfigPending`

**gui_task:**
- Location: `src/gui/gui_manager.cpp` (created by `gui_init()`)
- Triggers: FreeRTOS task on Core 0, priority 1
- Responsibilities: LVGL timer handler, encoder input polling, screen refresh, buzzer processing (primary path)

**webSocketEvent():**
- Location: `src/websocket_handler.cpp`
- Triggers: WebSocket message from client (text or binary)
- Responsibilities: Authentication validation (token-based), command dispatch, state updates, client disconnect handling

## Error Handling

**Strategy:** Fail-safe with self-healing retry + diagnostic event emission

**Patterns:**
- HAL devices: 3-retry with exponential backoff before transitioning to ERROR state. NVS fault counter survives reboots. Boot loop detection (3 consecutive crashes) enters safe mode (WiFi+web only, no HAL init).
- Audio pipeline: `audioPaused` flag + binary semaphore handshake prevents I2S read during driver reinstall. Noise gate uses `isHardwareAdc` flag. DSP swap failure returns false with `LOG_W` (no crash).
- Network: WiFi auto-reconnect with backoff (1s -> 60s max). MQTT reconnect runs in dedicated task to avoid blocking HTTP/WS.
- Persistence: Atomic write via temp file + rename. Legacy format migration on first boot. `halSafeMode` skips HAL init on boot loop detection.
- OTA: SHA256 verification of firmware download. Skips when heap is critical. Defers when amplifier is in use.
- Heap: 40KB reserve threshold monitored every 30s. `heapCritical` flag suppresses allocations (DSP delay lines, OTA checks).
- Logging: Level-filtered via `LOG_D`/`LOG_I`/`LOG_W`/`LOG_E`. Never log inside ISR or audio task (use dirty flags). WebSocket forwarding for remote debug.

## Cross-Cutting Concerns

**Logging:**
- `DebugSerial` class in `src/debug_serial.h/.cpp` with `LOG_D`/`LOG_I`/`LOG_W`/`LOG_E` macros
- Level-filtered at runtime via `applyDebugSerialLevel()`
- WebSocket forwarding: `broadcastLine()` sends JSON with extracted `[ModuleName]` prefix for frontend category filtering
- All modules use consistent `[ModuleName]` prefixes (e.g., `[Sensing]`, `[Audio]`, `[HAL]`, `[MQTT]`)

**Validation:**
- REST API: JSON deserialization check, range validation on numeric fields, `requireAuth()` guard on all protected endpoints
- HAL: GPIO bounds checking (`HAL_GPIO_MAX=54`), pin claim/release tracking, I2C bus conflict avoidance (Bus 0 SDIO)
- Audio: Sample count bounds, buffer size validation, matrix index bounds (16x16)
- WS auth: Token-based with 60s TTL, 16-slot pool, single-use tokens

**Authentication:**
- PBKDF2-SHA256 password hashing (10,000 iterations, random 16-byte salt)
- Session token via HttpOnly cookie
- WebSocket: short-lived token from `GET /api/ws-token` (60s TTL, single-use)
- Login rate limiting: non-blocking timestamp check, HTTP 429 with Retry-After
- First-boot random password displayed on TFT and serial

**Concurrency:**
- Core 1 reserved for audio (loopTask + audio_pipeline_task only)
- `vTaskSuspendAll()` for atomic sink/source slot updates
- `volatile` fields for cross-core reads (`_ready`, `_state`, `audioPaused`)
- Binary semaphore for I2S driver reinstall handshake
- FreeRTOS mutex in buzzer handler for dual-core safety
- MQTT on dedicated task (Core 0) with dirty-flag coordination to main loop
- Event group with 16 assigned bits (8 spare) for main loop wake

---

*Architecture analysis: 2026-03-10*
