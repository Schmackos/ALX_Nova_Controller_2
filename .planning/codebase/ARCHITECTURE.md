# Architecture

**Analysis Date:** 2026-03-21

## Pattern Overview

**Overall:** Event-driven, multi-layer modular architecture with Hardware Abstraction Layer (HAL) as single source of truth for device management. State is decomposed into 15 lightweight domain-specific headers, composed into a thin AppState singleton. FreeRTOS Core 1 reserved exclusively for audio, Core 0 for GUI/MQTT/USB. Entry point via Arduino framework (`setup()` / `loop()` in `main.cpp`).

**Key Characteristics:**
- State decomposition: 15 domain headers in `src/state/` (general, audio, wifi, mqtt, dsp, display, buzzer, ota, ethernet, debug, hal_coord, etc.) composed into AppState singleton
- HAL-centric: All devices (DAC, ADC, codec, GPIO, relays, sensors) are HAL-managed with unified lifecycle (UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ↔ UNAVAILABLE → ERROR/REMOVED/MANUAL)
- Event signaling: Dirty flags + event group (`EVT_*` bits in `src/app_events.h`) wake the main loop immediately on state change, fallback to 5 ms tick when idle
- Capability-based slot assignment: Audio pipeline uses 8-lane input, 16×16 routing matrix, 8-slot output dispatch — all managed by HAL-Pipeline Bridge via `HAL_CAP_ADC_PATH` / `HAL_CAP_DAC_PATH` ordinal counting
- Cross-core thread safety: Volatile fields on AppState and HalCoordState, portMUX spinlocks for toggle queue, binary semaphore for I2S pause handshake
- Modular subsystems: WiFi, MQTT (dedicated task on Core 0), OTA, settings, signal generator, smart sensing, audio pipeline, DSP, display (LVGL on Core 0), USB audio (TinyUSB UAC2)

## Layers

**Firmware Initialization (Core 1 Main Loop):**
- Purpose: Arduino setup/loop entry point, event-driven main loop, REST API dispatch, WebSocket broadcasts
- Location: `src/main.cpp`
- Contains: Server instances (port 80 HTTP, port 81 WebSocket), button handlers, periodic status logging, debug stats, task monitor, LED management
- Depends on: All subsystems via headers
- Used by: Hardware initialization, REST endpoint registration

**Application State Layer:**
- Purpose: Single source of truth for all application state, decomposed across 15 domain headers
- Location: `src/app_state.h` (singleton), `src/state/*.h` (15 domain headers)
- Contains: Thin AppState shell (~80 lines) referencing domain structs (WifiState, AudioState, DspSettingsState, MqttState, HalCoordState, DisplayState, BuzzerState, DebugState, OtaState, EthernetState, UsbAudioState, SignalGenState, GeneralState, EthernetState, DacState)
- Depends on: `src/app_events.h` (dirty flag event signaling)
- Used by: All subsystems access via `appState.domain.field` (e.g., `appState.wifi.ssid`, `appState.audio.paused`)

**Event Signaling Layer:**
- Purpose: Wake main loop on state change; idle when no changes pending
- Location: `src/app_events.h/.cpp`
- Contains: Event group (24 usable bits, bits 0-15 assigned), dirty-flag setter pattern (setter calls `app_events_signal(EVT_*)`)
- Depends on: FreeRTOS
- Used by: All state setters, main loop `app_events_wait(5)` with fallback to 5 ms tick

**HAL Device Framework (Hardware Abstraction Layer):**
- Purpose: Unified device lifecycle, discovery, configuration, and capability-based slot assignment
- Location: `src/hal/` (15 files for device manager, discovery, builtin drivers, API, pipeline bridge, etc.)
- Contains:
  - Device base class (`hal_device.h`): probe(), init(), deinit(), healthCheck(), getInputSource(), buildSink()
  - Device manager (`hal_device_manager.h/.cpp`): Singleton managing up to 24 devices (HAL_MAX_DEVICES=24), pin tracking (HAL_MAX_PINS=56), device state callbacks
  - Device database (`hal_device_db.h/.cpp`): Builtin devices (PCM5102A, ES8311, PCM1808, NS4150B, TempSensor, SigGen, USB Audio, MCP4725) + LittleFS JSON persistence
  - Discovery (`hal_discovery.h/.cpp`): 3-tier (I2C bus scan → EEPROM probe → manual config), skips Bus 0 when WiFi active (SDIO conflict)
  - HAL-Pipeline Bridge (`hal_pipeline_bridge.h/.cpp`): Connects device lifecycle to audio pipeline; capability-based ordinal assignment (DAC_PATH → sink slot, ADC_PATH → input lane)
  - REST API (`hal_api.h/.cpp`): 13 endpoints (GET/PUT/DELETE devices, discovery, configuration)
  - Builtin drivers: 8 integrated drivers (PCM5102A DAC, ES8311 codec, PCM1808 ADC, NS4150B amp, TempSensor, SigGen, USB Audio, MCP4725)
- Depends on: `src/hal/hal_types.h` (enums, structs, constants), I2C/I2S drivers
- Used by: Audio pipeline for source/sink registration, main loop for device lifecycle handling

**Audio Pipeline Layer:**
- Purpose: Multi-lane signal routing with per-input DSP, matrix mixing, per-output DSP, and HAL-managed sink dispatch
- Location: `src/audio_pipeline.h/.cpp`
- Contains: 8-lane input → per-lane DSP bypass → 16×16 matrix → per-output DSP bypass → 8-slot sink dispatch; float32 [-1.0, +1.0] internal; FreeRTOS task on Core 1 priority 3
- Depends on: HAL (dynamic source/sink registration), DSP pipeline (per-input/output), audio I/O (i2s_audio, usb_audio)
- Used by: Main loop (matrix config), DSP modules (data flow), output DSP (post-matrix processing)

**I2S Audio Layer (Dual I2S ADC with Synchronized Masters):**
- Purpose: Hardware audio input from two synchronized I2S master-mode ADCs (PCM1808 x2); drives audio pipeline task
- Location: `src/i2s_audio.h/.cpp`
- Contains: Dual I2S master init (ADC2 first, ADC1 second with clock output), DMA buffers, FFT analysis, RMS/VU/peak/health computation
- Depends on: ESP32 I2S driver, `lib/esp_dsp_lite/` for native FFT fallback
- Used by: Audio pipeline (feeds input lanes), analysis UI (waveform/spectrum)

**DSP Pipeline Layer:**
- Purpose: Real-time audio DSP processing: biquad IIR, FIR, limiter, gain, delay, polarity, mute, compressor with atomic glitch-free config swap
- Location: `src/dsp_pipeline.h/.cpp`, `src/output_dsp.h/.cpp` (per-output), `src/dsp_coefficients.h/.cpp` (RBJ biquad gen)
- Contains: Per-input DSP engine + per-output DSP engine; double-buffered config; PSRAM-backed delay lines; pre-built `libespressif__esp-dsp.a` (S3 assembly)
- Depends on: `src/dsp_convolution.h/.cpp` (FIR), `src/dsp_crossover.h/.cpp` (crossover presets), `src/dsp_rew_parser.h/.cpp` (REW/EQ APO import)
- Used by: Audio pipeline (signal path), DSP API (configuration), THD measurement (test signal)

**HAL Coordination State (Device Toggle Queue):**
- Purpose: Deferred cross-core-safe device enable/disable requests; prevents silent loss of concurrent toggles
- Location: `src/state/hal_coord_state.h`
- Contains: Fixed-size queue (capacity 8, same-slot dedup), `requestDeviceToggle(halSlot, action)` with portMUX spinlock guard
- Depends on: FreeRTOS (portMUX)
- Used by: Main loop drain loop, REST handlers (queuing device changes)

**Networking & Connectivity:**
- **WiFi**: `src/wifi_manager.h/.cpp` — multi-network client, AP mode, async connection with backoff
- **MQTT**: Split across 3 files:
  - `src/mqtt_handler.h/.cpp` — broker connection, settings load/save, discovery
  - `src/mqtt_publish.cpp` — periodic publishing with change-detection statics
  - `src/mqtt_ha_discovery.cpp` — Home Assistant MQTT discovery
  - Dedicated task: `src/mqtt_task.cpp` (Core 0, priority 2, polls 20 Hz independent of main loop)
- **Ethernet**: `src/eth_manager.h/.cpp` — 100Mbps full-duplex

**Authentication & API:**
- Purpose: Web password auth (PBKDF2-SHA256, 10k iterations), short-lived WebSocket tokens, REST API rate limiting
- Location: `src/auth_handler.h/.cpp`
- Contains: First-boot random password (logged to serial/TFT), HttpOnly cookies, WS token pool (16 slots, 60s TTL), HTTP 429 rate limiting
- Depends on: `mbedtls` (PBKDF2), `<WebServer.h>`, `<WebSocketsServer.h>`
- Used by: Main loop (endpoint auth), WebSocket handler (token validation)

**Settings & Persistence:**
- Purpose: Dual-format configuration: `/config.json` (primary, atomic via tmp+rename) with legacy `settings.txt` fallback
- Location: `src/settings_manager.h/.cpp`
- Contains: Export/import, factory reset, WiFi credentials via NVS (survives LittleFS format)
- Depends on: `<LittleFS.h>`, `<Preferences.h>`, `<ArduinoJson.h>`
- Used by: Main loop at boot, MQTT handler (on broker change), HTTP API

**OTA (Over-The-Air) Updates:**
- Purpose: GitHub release version checking, firmware download with SHA256 verification, atomic `Update.begin()`
- Location: `src/ota_updater.h/.cpp`
- Contains: Version comparison, SSL cert validation (Mozilla bundle), transient retry logic, progress logging
- Depends on: `<Update.h>`, `<HTTPClient.h>`, `<WiFiClientSecure.h>`
- Used by: Main loop (periodic checks), REST API (`/api/fw-update`)

**Smart Sensing & Amplifier Control:**
- Purpose: Voltage detection via ADC, auto-off timer, amplifier relay control via HAL
- Location: `src/smart_sensing.h/.cpp`
- Contains: ADC smoothing with dynamic alpha (rate-matched to `appState.audioUpdateRate`), relay via `findByCompatible("generic,relay-amp")` with GPIO fallback
- Depends on: HAL (device lookup), audio analysis
- Used by: Main loop (periodic 5 ms checks), relay configuration

**Signal Generator:**
- Purpose: Multi-waveform test signal generation (sine, square, noise, sweep) with hardware PWM output and software audio injection
- Location: `src/signal_generator.h/.cpp`
- Contains: Waveform synthesis, PWM duty cycle control, software injection via HAL-assigned dynamic lane
- Depends on: MCPWM driver, audio pipeline
- Used by: DSP testing, THD measurement, main loop control

**Display & GUI (LVGL on ST7735S):**
- Purpose: 160×128 TFT interface with LVGL v9.4, screen stack navigation, responsive input
- Location: `src/gui/` (20+ files: manager, input, theme, navigation, 8 screens)
- Contains: FreeRTOS task on Core 0 (gui_task), ISR-driven rotary encoder (Gray code state machine), screen sleep/wake, dark/light mode
- Depends on: `LovyanGFX` (SPI display driver), `lvgl@9.4`
- Used by: Main loop (HID input dispatch)

**USB Audio (TinyUSB UAC2 Speaker Device):**
- Purpose: Native USB audio input (PCM16, HS only) on ESP32-P4 USB OTG; registers as HAL audio source
- Location: `src/usb_audio.h/.cpp`
- Contains: FreeRTOS task on Core 0, lock-free ring buffer (1024 frames, PSRAM), dynamic lane registration, host volume/mute subscriptions
- Depends on: TinyUSB 0.20.1, PSRAM
- Used by: Audio pipeline (lane registration), HAL discovery (dynamic)

**WebSocket Handler (Real-Time UI Updates):**
- Purpose: Binary frame streaming (waveform, spectrum) and JSON state broadcasts to connected clients
- Location: `src/websocket_handler.cpp`
- Contains: Token-based auth (60s TTL), binary frame builders (`WS_BIN_WAVEFORM=0x01`, `WS_BIN_SPECTRUM=0x02`), module-category filtering (debug console), `wsAnyClientAuthenticated()` guard
- Depends on: `<WebSocketsServer.h>`, auth_handler, debug_serial
- Used by: Main loop (periodic broadcasts), debug console (live logs)

**Diagnostics & Monitoring:**
- **Debug Serial**: `src/debug_serial.h/.cpp` — log-level filtered output with WebSocket forwarding, module prefix extraction
- **Diagnostics Journal**: `src/diag_journal.h/.cpp` — circular event buffer with timestamps, error codes, diagnostic rules
- **Task Monitor**: `src/task_monitor.h/.cpp` — FreeRTOS enumeration (stack, priority, core affinity), 5s periodic dump
- **Health Bridge**: `src/hal/hal_audio_health_bridge.h/.cpp` — integrates audio pipeline health with HAL device diagnostics

**REST API Endpoints:**
Organized across 5 modules:
- `main.cpp`: Core endpoints (GET `/`, status, settings, WiFi, MQTT, firmware)
- `hal_api.cpp`: 13 HAL endpoints (device CRUD, discovery, config, database)
- `dsp_api.cpp`: DSP configuration (GET/POST biquads, FIR, presets, swap)
- `pipeline_api.cpp`: Matrix routing, per-output DSP, sink mute/volume
- `dac_api.cpp`: DAC state, enable/disable, volume

**Web UI Frontend (HTML/CSS/JS):**
- Purpose: Single-page application with modular JS concatenation (28 files in load order)
- Location: `web_src/` (auto-compiled to `src/web_pages.cpp` via `tools/build_web_assets.js`)
- Contains: Core WS/routing, app state, audio/DSP UI, HAL devices, WiFi/MQTT/settings, firmware update, debug console, health dashboard
- Key files: `01-core.js` (WS client), `03-app-state.js`, `04-shared-audio.js`, `05-audio-tab.js`, `06-peq-overlay.js`, `15-hal-devices.js`, `27a-health-dashboard.js`
- Uses: Material Design Icons (inline SVG, offline), localStorage (sticky filters), Canvas for waveform/spectrum visualization

## Data Flow

**Boot Sequence:**
1. `setup()` → HAL device registration (onboard + discovered) → discovery scan → `initAll()` sorted by priority
2. HAL-Pipeline Bridge listens for state changes, registers sources/sinks into audio pipeline
3. Audio pipeline task starts (Core 1), I2S ADC streams begin
4. MQTT task starts (Core 0), WiFi/MQTT connect in background
5. Main loop enters event-driven idle (waits on event group, 5 ms fallback)

**Audio Data Flow (Per-Frame, ~10 ms @ 48 kHz):**
1. I2S ADC DMA fills buffer → PCM samples
2. Audio pipeline task reads from all registered input sources (HAL devices + USB audio)
3. Per-lane DSP: biquad chains (double-buffered)
4. 16×16 matrix mixing (slot-indexed per output)
5. Per-output DSP: additional chains
6. Sink write: volume ramp (click-free), float→I2S conversion, DMA to DACs
7. Periodic analysis: FFT (spectrum), RMS/VU (metering), health checks

**State Change Broadcast (Main Loop):**
1. Any subsystem sets state → dirty flag + `app_events_signal(EVT_*)`
2. Main loop wakes immediately (event group)
3. Checks all dirty flags, builds JSON payload
4. Broadcasts to WebSocket clients (guarded by `wsAnyClientAuthenticated()`)
5. MQTT publishes independently at 20 Hz (mqtt_task on Core 0)

**Device Toggle Flow (HAL Coordination):**
1. REST handler calls `appState.halCoord.requestDeviceToggle(halSlot, action)` (thread-safe with portMUX)
2. Main loop detects pending toggles: `halCoord.hasPendingToggles()`
3. Drains queue: call `dac_activate_for_hal()` / `dac_deactivate_for_hal()` per entry
4. Device lifecycle: probe() → init() → _state=AVAILABLE → buildSink() or getInputSource() registered
5. HAL state change → `HalStateChangeCb` fires → bridge removes/registers pipeline sinks/sources

**State Management Shadow Fields:**
- Change detection for MQTT publishes: `prevMqtt*` statics in `mqtt_publish.cpp`
- Sensing broadcast tracking: `prevBroadcast*` in `smart_sensing.cpp`
- I2S pin config cache: `_cachedAdcCfg[2]` + `_cachedAdcCfgValid[2]` statics in `i2s_audio.cpp`

## Key Abstractions

**AudioInputSource (Lane-Indexed Source):**
- Purpose: Represents an input signal producer (ADC, USB, signal generator)
- Files: `src/audio_input_source.h`
- Pattern: Struct with `read` callback (samples), `name` (device name), `lane` (pipeline index), `halSlot` (device origin)
- Used by: Audio pipeline slot indexing, HAL dynamic lane discovery

**AudioOutputSink (Slot-Indexed Sink):**
- Purpose: Represents an output signal consumer (DAC, USB speaker, analyzer)
- Files: `src/audio_output_sink.h`
- Pattern: Struct with `write` callback (samples), `isReady` callback (health check), `name`, `slot` (pipeline index), `halSlot`
- Used by: Audio pipeline routing, HAL-routed driver initialization

**HalDevice (Lifecycle Base Class):**
- Purpose: Abstract interface for all HAL-managed devices
- Files: `src/hal/hal_device.h`
- Pattern: ESPHome-inspired lifecycle (probe, init, deinit, healthCheck); capability flags; override getInputSource() / buildSink() for audio devices
- Used by: Device registry, discovery, pipeline bridge (type-agnostic lifecycle)

**HalDeviceConfig (Persistence Struct):**
- Purpose: Per-device runtime configuration (bus overrides, pin mappings, sample rate preference, volume, mute)
- Files: `src/hal/hal_types.h`
- Pattern: Loaded from `/hal_config.json` at boot; supports I2C/I2S/GPIO pin overrides; persisted on config change
- Used by: Device manager (device-specific init), I2S audio (pin override resolution), REST API (config CRUD)

**Event Group (Dirty Flags + Wake):**
- Purpose: Efficient main loop wake-on-change with idle fallback
- Files: `src/app_events.h/.cpp`
- Pattern: 24-bit FreeRTOS event group; setter macros call `app_events_signal(EVT_*)`; main loop: `app_events_wait(5)`
- Used by: All state setters, main loop idle strategy

**Toggle Queue (HalCoordState):**
- Purpose: Deferred device enable/disable with same-slot dedup, cross-core thread-safe
- Files: `src/state/hal_coord_state.h`
- Pattern: Fixed-size array (capacity 8), `requestDeviceToggle(halSlot, action)` with portMUX guard
- Used by: REST handlers (device control), main loop drain loop

## Entry Points

**Arduino Setup/Loop (Core 1 Main):**
- Location: `src/main.cpp`
- Triggers: Hardware power-on → Arduino framework
- Responsibilities:
  - Initialize all subsystems (HAL, MQTT task, GUI task, OTA, audio)
  - Register REST API endpoints
  - Enter event-driven loop: `app_events_wait(5)` with dirty-flag dispatch

**HTTP Server Endpoints (Port 80):**
- Registered in `main.cpp` via `server.on(path, handler)`
- Responsibilities: Web UI serving, REST API dispatch, auth checking (except login), form handling
- Example: `GET /` → web_pages gzipped HTML, `POST /api/fw-update` → OTA trigger

**WebSocket Handler (Port 81):**
- Entry: `src/websocket_handler.cpp` event callback `webSocket.onEvent()`
- Triggers: Client connect, message RX, disconnect
- Responsibilities: Token validation, WS command dispatch, binary frame streaming

**REST API Endpoints:**
- Core: `/api/status`, `/api/settings`, `/api/wifi`, `/api/mqtt`
- HAL: `/api/hal/devices`, `/api/hal/scan`, `/api/hal/db`
- DSP: `/api/dsp/*`
- Pipeline: `/api/pipeline/*`, `/api/dac/*`
- Auth: `/api/login`, `/api/logout`, `/api/ws-token`

**FreeRTOS Tasks (Core 0):**
- **mqtt_task**: Dedicated MQTT reconnect + publish loop (20 Hz poll)
- **gui_task**: LVGL screen updates, HID input dispatch
- **usb_audio_task**: USB DMA polling and ring buffer management
- **OTA tasks**: Spawned on-demand for version check and firmware download

**HAL Device Lifecycle:**
- Entry: `HalDeviceManager::probeAndInit(slot)` (called at boot and post-scan)
- Triggers: Boot sequence, `POST /api/hal/scan` REST endpoint, user config change
- Responsibilities: I2C/GPIO probe, driver lookup, init() call, state transition, callback firing

## Error Handling

**Strategy:** Retry-with-backoff for transient failures (I2C, MQTT, OTA), graceful degradation for permanent (missing device, init failure).

**Patterns:**
- **HAL Health Check**: 30s periodic timer calls `healthCheck()` on all devices; 3 consecutive failures → STATE_ERROR
- **MQTT Backoff**: Exponential backoff (1s → 32s) on connection failure; reset on successful connect
- **I2S Pause Handshake**: Audio task respects `appState.audio.paused` flag, acks via binary semaphore before `i2s_driver_uninstall()`
- **DSP Config Swap**: Double-buffered with PSRAM hold buffer during swap gap (prevents glitch)
- **Settings Load**: Primary `/config.json` read; fallback to legacy `settings.txt` on first boot only

**Error Codes:** `src/diag_error_codes.h` defines 20+ error reasons (I2C ACK fail, driver not found, init returned false, etc.). Logged to diagnostics journal with timestamps.

## Cross-Cutting Concerns

**Logging:** All modules use `debug_serial.h` macros (`LOG_D/I/W/E`) with `[ModuleName]` prefixes. Runtime level control via `applyDebugSerialLevel()`. WebSocket relay enabled when clients connected.

**Validation:** Input bounds-checking on DSP coefficients, sample rates, GPIO numbers. Audio arrays sized dynamically via `resizeAudioArrays(numInputLanes)` on HAL discovery (moved from hardcoded NUM_ADCS).

**Authentication:** PBKDF2-SHA256 (10k iterations) on first boot; HttpOnly cookies for HTTP; short-lived WS tokens (60s TTL, 16-slot pool) for real-time endpoints.

**Thread Safety:**
- AppState volatile fields (audio.paused, debug.heapCritical)
- HalCoordState.requestDeviceToggle() guarded by portMUX spinlock
- Audio pipeline slot updates use `vTaskSuspendAll()` for atomicity
- Dirty flags trigger immediate main loop wake (no polling latency)

---

*Architecture analysis: 2026-03-21*
