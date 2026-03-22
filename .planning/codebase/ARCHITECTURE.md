# Architecture

**Analysis Date:** 2026-03-23

## Pattern Overview

**Overall:** Layered event-driven modular microservice architecture with domain-specific state composition.

**Key Characteristics:**
- **Singleton AppState** — Centralized decomposed state across 15 domain-specific headers in `src/state/`, reducing DEBT-4. State changes trigger dirty flags and FreeRTOS event group signals
- **Hardware Abstraction Layer (HAL)** — 3-tier device discovery (I2C bus scan → EEPROM probe → manual config), lifecycle state machine (UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ⇄ UNAVAILABLE → ERROR/REMOVED/MANUAL), capability-based audio pipeline integration
- **Event-Driven Core** — Main loop replaced delay(5) with `app_events_wait(5)` using FreeRTOS event group; wakes <1 µs on state change or 5ms tick fallback
- **Dual-Core Audio Pipeline** — Core 1 exclusively reserved for audio (loopTask priority 1 + audio_pipeline_task priority 3). Core 0 handles GUI, MQTT, OTA, USB Audio
- **Deferred Command Queueing** — HAL device toggles use `requestDeviceToggle(halSlot, action)` queue in `HalCoordState` (capacity 8, main loop drains per entry)
- **Modular REST + WebSocket API** — 13 HAL endpoints, 12 DSP endpoints, 4 pipeline endpoints, plus generic endpoints (auth, settings, OTA, diagnostics) registered in `main.cpp`
- **Capability-Based Discovery** — Devices advertise features via 16-bit capability flags (`HAL_CAP_HW_VOLUME`, `HAL_CAP_MQA`, etc.), enabling capability-aware API responses and UI filtering

## Layers

**HAL Device Layer:**
- Purpose: Hardware abstraction for audio devices (ADCs, DACs, codecs, relays, LEDs), sensor interface, discovery, lifecycle management
- Location: `src/hal/` (30 drivers, 57 files total)
- Contains: Device base class `HalDevice`, manager singleton `HalDeviceManager`, 3-tier discovery `HalDiscovery`, EEPROM v3 matcher, built-in + custom device registries, REST API endpoints
- Depends on: I2S port API, EEPROM v3, I2C Bus 0/1/2 backend
- Used by: Audio pipeline bridge (sink/source registration), smart sensing (relay via `findByCompatible("generic,relay-amp")`), HAL API handlers

**Audio Processing Layer (Core 1):**
- Purpose: Real-time audio I/O, DSP, routing, analysis
- Location: `src/audio_pipeline.cpp/.h`, `src/i2s_audio.cpp/.h`, `src/dsp_pipeline.cpp/.h`, `src/output_dsp.cpp/.h`
- Contains: 8-lane input → per-input DSP → 32×32 matrix mixer → per-output DSP → 16-slot sink dispatch. Port-generic I2S driver for 3 configurable ports (STD/TDM, TX/RX). Float32 internal. Analysis metrics (RMS/VU/peak/dBFS, 256-sample waveform, 1024-point FFT)
- Depends on: HAL sinks/sources, DSP coefficient generator (RBJ EQ Cookbook), ESP-DSP pre-built library (S3 assembly, PSRAM allocation wrapper)
- Used by: HAL pipeline bridge (source/sink callbacks), smart sensing (signal detection), WebSocket audio streaming

**State Management Layer:**
- Purpose: Decomposed domain-specific state (WiFi, MQTT, audio, DSP, display, debug, HAL coordination)
- Location: `src/state/` (15 files) composed into `src/app_state.h` singleton
- Contains: `WifiState`, `MqttState`, `AudioState`, `DspSettingsState`, `DisplayState`, `BuzzerState`, `SignalGenState`, `DebugState`, `GeneralState`, `OtaState`, `EthernetState`, `DacState`, `UsbAudioState`, `HalCoordState`, shared `enums.h`
- Depends on: Nothing (stateless headers)
- Used by: All modules via `AppState::getInstance()` or `appState` macro; WebSocket broadcasts read state for JSON serialization; MQTT publish reads state for Home Assistant discovery

**Event Coordination Layer:**
- Purpose: Cross-task signaling, dirty flags, deferred command queueing
- Location: `src/app_events.h/.cpp` (24-bit event group), `src/state/hal_coord_state.h/.cpp` (8-slot toggle queue)
- Contains: `EVT_AUDIO`, `EVT_WIFI`, `EVT_MQTT`, `EVT_DSP`, etc. (16 bits assigned, 8 spare); HAL toggle queue with overflow telemetry
- Depends on: FreeRTOS event group API
- Used by: All dirty-flag setters (trigger event signals), main loop (app_events_wait), MQTT task, GUI task

**Integration Layer (REST + WebSocket):**
- Purpose: HTTP/WebSocket API, command dispatch, client auth, real-time state broadcasting
- Location: `src/websocket_*.cpp` (4 files: command, broadcast, auth, cpu_monitor), `src/*_api.cpp` files (12 endpoint files), `src/auth_handler.cpp`, `src/http_security.h`
- Contains: 13 HAL endpoints, 12 DSP endpoints, 4 pipeline endpoints, 3 diagnostics endpoints, I2S config endpoints, settings CRUD, OTA check, WiFi/MQTT CRUD. WS: 17 state broadcast functions, binary waveform/spectrum streaming (adaptive rate 20-500ms per client), token-based auth (60s TTL, 16-slot pool)
- Depends on: ArduinoJson for serialization, WebSocketsServer (vendored v2.7.3), WebServer, AppState for reads
- Used by: Web UI on port 80/81

**WiFi + MQTT Layer:**
- Purpose: Network connectivity, settings persistence, Home Assistant discovery
- Location: `src/wifi_manager.cpp/.h`, `src/mqtt_handler.cpp/.h`, `src/mqtt_task.cpp`, `src/mqtt_publish.cpp`, `src/mqtt_ha_discovery.cpp`, `src/eth_manager.cpp/.h`
- Contains: Multi-network client + AP mode (via WiFi library), MQTT broker auto-connect + PBKDF2 auth + Hall Availability sensor discovery + generic device binary_sensor publication, Ethernet manager (100Mbps), change-detection statics in mqtt_publish.cpp + mqtt_ha_discovery.cpp
- Depends on: WiFi library, PubSubClient, NVS (WiFi credentials via Preferences), `appState.wifi/mqtt` state
- Used by: Main loop (dirty flag dispatch to mqtt_task), Settings manager (persistence), Smart sensing (amplifier state publish)

**Firmware Update Layer:**
- Purpose: OTA checking, download, SHA256 verification, atomic install
- Location: `src/ota_updater.cpp/.h`, `src/ota_certs.h`, `tools/update_certs.js`
- Contains: GitHub release API poller, firmware.bin SHA256 verification (Sectigo R46/E46, DigiCert G2 root certs), binary download with progress callback, atomic Update.begin/write/end
- Depends on: HTTPClient, WiFiClientSecure, Arduino Update API, LittleFS for SHA file storage
- Used by: Main loop via `startOTACheckTask()` (Core 0 one-shot task), REST endpoint `POST /api/ota/check`

**Settings Persistence Layer:**
- Purpose: Load/save application state to LittleFS, export/import with versioning
- Location: `src/settings_manager.cpp/.h`
- Contains: `/config.json` atomic write (tmp+rename), legacy `settings.txt` fallback, export v2.0 includes HAL devices + custom schemas + DSP + pipeline matrix, version-aware import with preview UI
- Depends on: ArduinoJson, LittleFS, NVS (WiFi credentials separate), HAL config persistence via `hal_settings.cpp`
- Used by: Main loop (boot-time load, factory reset), Web UI settings tab, HAL device config CRUD

**Smart Sensing Layer:**
- Purpose: Voltage threshold detection (auto-off timer), amplifier relay control, signal detection
- Location: `src/smart_sensing.cpp/.h`
- Contains: RMS analysis at configurable update rate, threshold trigger, amplifier relay via HAL or GPIO, auto-off countdown, cross-module dirty flags
- Depends on: Audio analysis metrics from i2s_audio.cpp, HAL relay driver lookup, `appState.audio` state
- Used by: Main loop (polling), WebSocket broadcasts (sensing state)

**Signal Generation Layer (HAL-managed):**
- Purpose: Multi-waveform test signal (sine, square, noise, sweep), software injection into lane 0, PWM output option
- Location: `src/signal_generator.cpp/.h`, `src/hal/hal_signal_gen.h/.cpp`, `src/siggen_api.cpp/.h`
- Contains: Waveform tables, frequency/amplitude/rate control, PWM on GPIO 47 (via MCPWM), lane injection toggle via `HalSigGen` device
- Depends on: Audio pipeline input source registration, MCPWM driver (ESP32 LEDC replacement)
- Used by: Web UI signal generator tab, Siggen REST endpoint

**Debug + Diagnostics Layer:**
- Purpose: Serial logging with runtime level control, diagnostic event journaling (health metrics), task monitoring
- Location: `src/debug_serial.cpp/.h`, `src/diag_journal.cpp/.h`, `src/task_monitor.cpp/.h`, `src/diag_api.cpp/.h`
- Contains: Log level filtering (D/I/W/E), color codes, `[ModuleName]` prefix extraction, WS log forwarding; DiagEvent journal (up to 128 entries, wrapping ringbuffer, 8 severity levels, 16-bit error codes); FreeRTOS task enumeration (stack/priority/core affinity), heap pressure transitions (3 states: normal/warning/critical)
- Depends on: Serial, FreeRTOS API, ArduinoJson for WS forwarding
- Used by: All modules (LOG_D/I/W/E macros), Web UI debug console, Health Dashboard

**GUI Layer (Core 0, LVGL 9.4):**
- Purpose: ST7735S 160×128 TFT display management, rotary encoder input, screen stack navigation
- Location: `src/gui/` (9 files: manager, input, theme, navigation, screens/*), guarded by `GUI_ENABLED`
- Contains: LVGL theme with orange accent, dark/light mode, rotary encoder Gray code state machine, screen stack with push/pop/transitions, 10+ desktop screens (Home, Control, WiFi, MQTT, Settings, Debug, Boot animations)
- Depends on: LVGL 9.4, LovyanGFX driver for ST7735S, `appState.display` state
- Used by: Main loop (FreeRTOS gui_task on Core 0), Button handler (rotary encoder ISR feedback)

**USB Audio Layer (Core 0, TinyUSB UAC2):**
- Purpose: High-speed USB speaker device via native USB OTG, sample rate auto-negotiation
- Location: `src/usb_audio.cpp/.h`, guarded by `USB_AUDIO_ENABLED`
- Contains: UAC2 class (speaker device only), lock-free ring buffer (1024 frames, PSRAM), FreeRTOS task, host volume/mute tracking, `HalUsbAudio` device driver
- Depends on: TinyUSB 0.20.1, audio pipeline source registration
- Used by: HAL pipeline bridge (dynamic source at boot or rescan)

## Data Flow

**Audio Signal Path:**

1. **Input Capture** — `audio_pipeline_task` (Core 1) calls `i2s_port_read()` per configured ADC source
2. **Per-Input DSP** — Each of 8 lanes has optional biquad/FIR/gain/limiter chain (double-buffered, glitch-free swaps)
3. **Analysis** — 256-sample waveform buffer + 1024-point FFT (RFFT via ESP-DSP) → per-ADC RMS/VU/peak/dBFS stored in `AudioAnalysis` struct
4. **Matrix Mixing** — 32×32 coefficient matrix multiplies input lanes to output channels (slot-indexed, under `vTaskSuspendAll()`)
5. **Per-Output DSP** — Each of 16 outputs has independent biquad/compressor/limiter chain
6. **Sink Dispatch** — 16 slot indices map to HAL sinks; each sink fetches float samples and converts via `sink_apply_volume()` → `sink_apply_mute_ramp()` → `sink_float_to_i2s_int32()` (left-justified I2S)
7. **DMA Output** — `i2s_port_write()` enqueues to I2S peripheral for immediate transmission

**State Change Propagation:**

1. **Module sets dirty flag** — e.g., WiFi manager sets `isWifiDirty(true)`
2. **Dirty flag setter calls event signal** — `app_events_signal(EVT_WIFI)` via `<flag_setter>_signal()` wrapper
3. **Main loop wakes immediately** — `app_events_wait(5)` returns <1 µs instead of 5 ms
4. **Main loop checks flag and broadcasts** — `wsAnyClientAuthenticated()` guard, then `sendWifiState()` serializes JSON
5. **WebSocket server sends to all connected clients** — Each client's WS task receives frame on port 81
6. **Browser JavaScript updates DOM** — Listener functions in `web_src/js/` update UI elements

**HAL Device Lifecycle:**

1. **Boot: hal_device_manager.initAll()** — Priority-sorted (BUS=1000 → LATE=100), each device's `init()` runs under `vTaskSuspendAll()`
2. **State transitions** — UNKNOWN → DETECTED (post-discovery) → CONFIGURING → AVAILABLE → [UNAVAILABLE ↔ AVAILABLE] → ERROR/REMOVED/MANUAL
3. **Sink/Source registration** — HAL pipeline bridge listens on state change callback; calls `audio_pipeline_set_sink()`/`audio_pipeline_set_source()` per capability
4. **Runtime recovery** — `hal_device_manager.healthCheckAll()` (30s periodic) retries UNAVAILABLE/ERROR devices up to 3 times with backoff; exhaustion → `DIAG_HAL_PROBE_RETRY_OK` or fault count increment

**HTTP Request → Command Flow:**

1. **Browser sends WS command** — e.g., `{"cmd":"dsp","action":"addStage","channel":0,"type":"PEQ","freq":1000,"gain":3,"q":1}`
2. **webSocketEvent()** dispatches via `webSocketCommand.cpp` lookup table
3. **Command handler** — e.g., `cmd_dsp_add_stage()` reads params, calls `dsp_add_stage(channel, ...)`, sets dirty flag on success
4. **Main loop** — Detects `isDspDirty()`, calls `sendDspState()` → JSON serialization → binary frame if `WS_SEND_DSPSTATS_BINARY`
5. **Client receives broadcast** — JavaScript `ws.onmessage` parses JSON, updates `appState` object, triggers UI update

**State Management — AudioState Example:**

```
AudioState {
  enabled: bool                    // Audio subsystem active
  adcEnabled[8]: bool             // Per-lane input enable
  adcDetected[8]: bool            // Lane detection result
  adcCount: int                   // How many ADCs currently detected
  adcGain[8]: float               // Software gain per lane (dB)
  analysis: AudioAnalysis          // Current RMS/VU/peak/FFT
  diagnostics: AudioDiagnostics    // I2S errors, DC offset, noise floor
  dmaAllocFailed: bool            // DMA buffer pre-allocation failed
  audioUpdateRate: int            // 50-1000 Hz (rate-matches smart sensing)
  audioPaused: volatile bool      // Cross-core pause flag (handshake with semaphore)
  taskPausedAck: SemaphoreHandle  // Binary semaphore for I2S driver safety
}
```

**MQTT Publish Loop (separate task, Core 0):**

1. `mqtt_task.cpp` runs at 20 Hz on dedicated Core 0 task (priority 2)
2. Polls `appState._mqttReconfigPending` for broker setting changes
3. Checks dirty flags in `mqtt_publish.cpp` shadow statics (`prevMqtt*` fields)
4. Publishes deltas to broker on state changes
5. Home Assistant discovery: `mqtt_ha_discovery.cpp` publishes binary_sensor availability + generic HAL device entities

## Key Abstractions

**HalDevice (Base Class):**
- Purpose: Abstract interface for all hardware devices (DACs, ADCs, codecs, relays, buttons, encoders)
- Examples: `HalPcm5102a`, `HalEs9039Pro`, `HalNs4150b`, `HalRelay`, `HalTemperature`
- Pattern: Virtual `init()`, `probe()`, `getCapabilities()`, `deinit()`. Built-in devices inherit and override. Custom devices use JSON schema. HAL_REGISTER macro reduces registration to one-liner
- State: `_state` enum (lifecycle), `_ready` volatile bool (lock-free pipeline reads), `_descriptor` (name, type, compatible), `_lastError[48]` (failure reason for UI)

**AudioPipeline (Floating-Point Audio Router):**
- Purpose: Thread-safe mixing engine on Core 1, slot-indexed sink/source dispatch
- Examples: 8 input lanes, 32-channel matrix, 16 output sinks (DACs, USB, etc.)
- Pattern: `audio_pipeline_set_source(lane, source)` registers input; `audio_pipeline_set_sink(slot, sink)` registers output; `audio_pipeline_set_matrix_gain(out, in, gain)` routes audio
- Validation: Compile-time static_assert prevents `MAX_INPUTS*2 > MATRIX_SIZE`; runtime bounds check on `set_source()` and `set_sink()` returns bool

**DspPipeline (Per-Channel DSP Chain):**
- Purpose: Double-buffered DSP stage sequencer (biquads, FIR, limiter, compressor, gain, polarity, mute)
- Examples: PEQ (parametric EQ), crossover presets, FIR convolution/room correction, speaker protection
- Pattern: `dsp_add_stage(channel, type, params)` appends to config, `dsp_swap_config()` atomically swaps on audio task wakeup for glitch-free updates
- Pool management: Stages allocated from fixed pool (256 entries); exhaustion returns false + logs warning; config import skips failed stages gracefully

**HalDiscovery (3-Tier Device Detection):**
- Purpose: Auto-detect and register devices at boot, rescan at runtime
- Examples: I2C bus scan (0x08-0x77), EEPROM probe (0x50-0x57 addresses), manual registration
- Pattern: Tier 1 (I2C scan) → Tier 2 (EEPROM v3 match) → Tier 3 (driver factory function). Bus 0 SDIO-aware (skip if WiFi active). Probe retry with backoff (2 attempts, 50ms spacing)
- Guard: `hal_wifi_sdio_active()` checks `connectSuccess || connecting || activeInterface == NET_WIFI`; emits `DIAG_HAL_I2C_BUS_CONFLICT` on skip

**WebSocketServer (Real-Time Broadcasting):**
- Purpose: JSON + binary frame dispatch to multiple connected clients, token-based auth
- Examples: `sendDspState()` (JSON DSP config), `sendAudioData()` (binary waveform 0x01 + spectrum 0x02)
- Pattern: Client auth via `GET /api/ws-token` (16-slot pool, 60s TTL); WS frames carry 4-byte token in first dword of JSON payload; auth tracked in `wsAuthStatus[clientNum]`
- Adaptive rate: Binary frame skip factor scales 1→8 based on authenticated client count; heap pressure gates (`heapCritical`) suppress all binary data

**HalCoordState (Deferred Device Toggle Queue):**
- Purpose: Thread-safe queuing of device enable/disable commands from REST/MQTT for main loop execution
- Examples: `requestDeviceToggle(halSlot, HAL_ACTION_ENABLE)` enqueues; main loop polls `hasPendingToggles()` and calls `dac_activate_for_hal(halSlot)`
- Pattern: 8-slot FIFO queue, same-slot deduplication (later request overwrites earlier), overflow counter (emits `DIAG_HAL_TOGGLE_OVERFLOW` diagnostic)
- All 6 callers check return value; REST endpoints return HTTP 503 on overflow

**AppState Singleton (Decomposed Domain State):**
- Purpose: Centralized mutable state with per-domain dirty flags and event signaling
- Examples: `appState.wifi.ssid`, `appState.audio.adcGain[0]`, `appState.dsp.enabled`
- Pattern: 15 domain headers in `src/state/` composed into `AppState` class; dirty-flag setters call `app_events_signal()` helper; main loop drains events and broadcasts to WS
- Backward compat: Legacy `#define wifiSSID appState.wifiSSID` aliases redirect old code to new nested access

## Entry Points

**Firmware Bootup (main.cpp setup()):**
- Location: `src/main.cpp` lines 150-300
- Triggers: ESP32-P4 power-on, watch-dog timer reset, brownout recovery
- Responsibilities: Initialize serial (115200 baud), LittleFS, Preferences, crash log recovery, TWDT reconfigure (30s), GPIO setup (LED, button, buzzer, encoder), I2S + audio pipeline init, HAL discovery + device init, WiFi/MQTT setup, WebSocket server start, GUI init, MQTT task spawn, app_events init, settings load from /config.json, OTA cert setup
- Returns: Never (Arduino loop runs indefinitely)

**Main Loop (Arduino loop()):**
- Location: `src/main.cpp` lines 430+
- Triggers: Continuously (every ~5 ms or immediately on event signal)
- Responsibilities: Check app events via `app_events_wait(5)` (replaced hardcoded delay(5)); poll WiFi/MQTT state; dispatch WebSocket broadcasts (sendDspState, sendHardwareStats, etc.); drain HAL coordinate toggle queue; run smart sensing (signal detect, amplifier relay); check heap pressure (heap_budget), PSRAM pressure (fallback/failure counts), task monitor (5s periodic dump); poll button/rotary encoder; drain diagnostic journal broadcasts; OTA update checks; GUI task (Core 0); Serial debug log forwarding to WS clients
- Never blocks: All subsystems use dirty flags + event signaling instead of polling loops

**WebSocket Event Handler (webSocketEvent):**
- Location: `src/websocket_command.cpp` lines 1+
- Triggers: WebSocket connect (type WStype_CONNECTED), disconnect (WStype_DISCONNECTED), text frame (WStype_TEXT)
- Responsibilities: Auth token validation on connection, command dispatch via lookup table (cmd_wifi_scan, cmd_dsp_add_stage, cmd_settings_save, etc.), JSON parsing with ArduinoJson, parameter validation, error response on auth failure or invalid params
- Pattern: Each command sets dirty flag on success, main loop broadcasts state back to all clients

**HTTP Request Handler (WebServer):**
- Location: `src/main.cpp` registerEndpoints() section; individual handlers in `src/*_api.cpp`
- Triggers: HTTP GET/POST/PUT/DELETE on port 80
- Responsibilities: Parse URL query params + JSON body, validate auth cookie (or return 401), execute command (read HAL state, modify settings, trigger OTA check), return JSON response with status
- All responses wrapped by `server_send()` helper to add `X-Frame-Options: DENY`, `X-Content-Type-Options: nosniff` headers

**HAL Device Init (during boot, per device):**
- Location: Each driver's `init()` method, e.g., `src/hal/hal_es9039pro.cpp`
- Triggers: `hal_device_manager.initAll()` during `setup()`, priority-sorted (BUS devices first, LATE devices last)
- Responsibilities: I2C register writes (volume, filter, gain), EEPROM read (firmware version check), I2S port configuration (sample rate, format, MCLK multiple), GPIO claim (via `HalDeviceManager::claimPin()`), state transition CONFIGURING → AVAILABLE
- Guard: Under `vTaskSuspendAll()` to prevent audio pipeline from reading uninitialized state
- Failure: Returns `HalInitResult` with error code; manager logs, transitions to ERROR state, schedules retry

**Audio Pipeline Task (Core 1, audio_pipeline_task):**
- Location: `src/audio_pipeline.cpp` (FreeRTOS task spawned by `audio_pipeline_init()`)
- Triggers: DMA completion interrupt (I2S0 RX) → runs ~512 samples per 8 ms frame at 48 kHz
- Responsibilities: Fetch samples via `i2s_port_read()`; apply per-input DSP; route through matrix mixer (under `vTaskSuspendAll()`); apply per-output DSP; call per-sink `buildSink()` callbacks; write via `i2s_port_write()`; update `AudioAnalysis` metrics; check `audioPlayAused` flag (DAC toggle safety); yield 2 ticks to let main loop run
- Never logs or calls Serial.print (blocks DMA); sets dirty flags and returns
- Timing: Total frame ~1.2 ms on ESP32-P4 @ 48 kHz

**MQTT Task (Core 0, mqtt_task):**
- Location: `src/mqtt_task.cpp` (spawned at boot)
- Triggers: Periodic 20 Hz loop
- Responsibilities: Reconnect on `_mqttReconfigPending` flag; call `mqttClient.loop()` for inbound subscriptions; poll dirty flags in `mqtt_publish.cpp` and publish deltas; periodic Home Assistant discovery publishes (5s); binary_sensor availability for HAL devices
- Decoupled from main loop: Main loop only sets dirty flags; MQTT task reads and publishes independently

## Error Handling

**Strategy:** Layered with escalating recovery:

1. **Probe-time detection** — HAL discovery retries up to 3 times with backoff (50ms) on I2C timeout; emits `DIAG_HAL_PROBE_RETRY_OK` on success or logs error code
2. **Init-time recovery** — Device init failure → logs `[HAL]` message, transitions to ERROR state, schedules retry
3. **Runtime self-healing** — Health check (30s) retries UNAVAILABLE/ERROR devices non-blocking; exhaustion increments NVS fault counter
4. **Audio task safety** — `audioPlayAused` flag + semaphore handshake prevents race on I2S driver uninstall
5. **REST/WebSocket error response** — Command handlers validate inputs, return HTTP status (400 bad request, 401 auth, 503 service unavailable), JSON error detail

**Patterns:**

- **I2C SDIO conflict detection** — `hal_wifi_sdio_active()` checks connectivity state; skips Bus 0 scan and emits `DIAG_HAL_I2C_BUS_CONFLICT`
- **Heap pressure cascades** — 3 states (normal/warning/critical) with graduated feature degradation (WS binary suppressed, DSP stages refused, OTA skipped)
- **Matrix bounds validation** — `audio_pipeline_set_source()` and `audio_pipeline_set_sink()` check `lane*2+1 < MATRIX_SIZE` and return bool; config import skips failed stages
- **DMA allocation pre-flight** — Eagerly allocate DMA buffers at boot (16 × 2KB = 32KB SRAM) before WiFi connects; if failure, set `AudioState.dmaAllocFailed` bit and emit `DIAG_AUDIO_DMA_ALLOC_FAIL`

## Cross-Cutting Concerns

**Logging:**
- Framework: `debug_serial.h` macros (LOG_D, LOG_I, LOG_W, LOG_E) with `[ModuleName]` prefix
- Forwarding: WS log forwarding in `debug_serial.cpp` streams lines to authenticated clients; Debug Console tab filters by module category
- Guard: Never log from ISR or audio task (blocks Serial TX → DMA starvation); use dirty-flag pattern instead

**Validation:**
- JSON: ArduinoJson with bounds checking; REST endpoints validate `Content-Type: application/json`
- Matrix: Compile-time `static_assert` on `MAX_INPUTS*2 <= MATRIX_SIZE`; runtime slot bounds check in `set_source()` / `set_sink()`
- Auth: Cookie HttpOnly + Secure flags; WS token 60s TTL in 16-slot pool; rate limiting on `/api/auth/login` returns HTTP 429

**Authentication:**
- Web UI: PBKDF2-SHA256 password hash (50k iterations, `p2:` format), stored in `/config.json`, auto-migrated from legacy `p1:` (10k) on login
- WebSocket: Token-based (60s TTL, 16-slot pool); fetched via `GET /api/ws-token`, validated on WS connect
- REST: Session cookie (HttpOnly, Secure) set on login; checked on every request; login returns 401 if invalid

---

*Architecture analysis: 2026-03-23*
