# Architecture

**Analysis Date:** 2026-03-21

## Pattern Overview

**Overall:** Event-driven real-time audio pipeline with Hardware Abstraction Layer (HAL) for modular device management.

**Key Characteristics:**
- **Multi-core audio pipeline**: 8-lane input → per-input DSP → 16×16 routing matrix → per-output DSP → 8-slot sink dispatch (Core 1 exclusive)
- **Domain-specific state composition**: 15 lightweight state headers in `src/state/` composed into thin `AppState` singleton (~80 lines)
- **Capability-based device discovery**: 3-tier I2C bus scan (EEPROM probe, chip ID verify) → capability-driven ordinal assignment (slots/lanes)
- **Deferred event signaling**: Dirty flags + `EVT_*` bits trigger main loop wakeup via FreeRTOS event group instead of fixed 5ms ticks
- **Cross-cutting coordination**: HAL toggle queue, MQTT async task on Core 0, PSRAM memory budgeting with 3-state heap pressure

## Layers

**I/O Layer (Hardware):**
- Purpose: Direct ESP32-P4 hardware interface (I2S ADC/DAC, I2C, GPIO, USB, PWM)
- Location: `src/i2s_audio.h/.cpp`, `src/hal/hal_*.cpp` (30+ driver files)
- Contains: Driver init/config, pin negotiation, interrupt handlers, register access
- Depends on: ESP-IDF, Arduino, PlatformIO build flags
- Used by: Audio pipeline (I2S read/write), HAL device manager (discovery/health)

**HAL (Device Management Layer):**
- Purpose: Unified API for all external devices — DAC, ADC, codecs, relays, encoders, buttons, displays, USB audio
- Location: `src/hal/` (50+ files)
- Contains: Device lifecycle state machine (UNKNOWN→DETECTED→CONFIGURING→AVAILABLE⇌UNAVAILABLE→ERROR/REMOVED), driver registry, discovery engine, EEPROM v3 config persistence
- Depends on: I/O layer (pin claim, bus negotiation), AppState (config), LittleFS (persistence)
- Used by: Audio pipeline (source/sink registration), settings (config import/export), REST API (`hal_api.cpp`)

**Audio Pipeline (Core 1):**
- Purpose: Real-time sample processing on Core 1 with deterministic latency
- Location: `src/audio_pipeline.h/.cpp` (~2200 lines), `src/audio_input_source.h`, `src/audio_output_sink.h`
- Contains: Lane-indexed sources, 16×16 matrix, output slots, sample I/O callbacks
- Depends on: HAL (device source/sink registration), DSP (per-lane/output filtering), I2S driver (DMA)
- Used by: Main loop (register sources/sinks via HAL), WebSocket (VU metering), REST API

**DSP Pipeline (Optional, Core 1 preemption):**
- Purpose: Multi-stage per-lane + per-output audio filtering (biquads, FIR, gain, limiting, compressor)
- Location: `src/dsp_pipeline.h/.cpp` (~3300 lines), `src/output_dsp.h/.cpp` (~1000 lines)
- Contains: 24 stage types (biquad PEQ/shelf/notch, FIR, convolution, compressor, limiter, noise gate, tone controls), double-buffered config swap, glitch-free morph
- Depends on: Audio pipeline (per-stage processing callbacks), ESP-DSP (assembly FFT/biquad on S3), memory budget (PSRAM delay lines)
- Used by: Audio pipeline (stage chain evaluation), WebSocket (PEQ overlay), REST API (`dsp_api.cpp`)

**Settings & State Management:**
- Purpose: Persistent configuration (JSON) + volatile runtime state (AppState singleton)
- Location: `src/app_state.h`, `src/state/*.h`, `src/settings_manager.cpp`, `src/hal/hal_settings.cpp`
- Contains: Config load/save, dirty-flag change detection, cross-task signaling
- Depends on: LittleFS, JSON parser (ArduinoJson)
- Used by: All subsystems (config), main loop (broadcast on dirty)

**Network Layer:**
- Purpose: WiFi/Ethernet connectivity, MQTT broker communication, HTTP/WebSocket servers
- Location: `src/wifi_manager.cpp`, `src/mqtt_handler.cpp`, `src/mqtt_task.cpp`, `src/websocket_handler.cpp`, `src/main.cpp`
- Contains: Connection state machines, MQTT QoS/subscriptions, REST API endpoint dispatch, WebSocket auth + binary frame marshaling
- Depends on: ESP-IDF (WiFi/Ethernet drivers), PubSubClient (MQTT), WebSocketsServer (library)
- Used by: Web UI (via HTTP/WS), Home Assistant (via MQTT), external systems

**Application Controller:**
- Purpose: Coordination, state broadcasting, event signaling
- Location: `src/main.cpp` (~1500 lines), `src/app_events.h`, `src/websocket_handler.cpp`
- Contains: FreeRTOS task creation, main loop (event-wait pattern), state change broadcasts
- Depends on: All subsystems
- Used by: Triggers all async operations (WiFi, MQTT, OTA, HAL scans)

**Presentation Layer (Optional):**
- Purpose: Embedded web UI (HTML/CSS/JS) + optional TFT display
- Location: `src/web_pages.cpp` (auto-generated from `web_src/`), `src/gui/*.cpp` (LVGL on Core 0)
- Contains: REST API consumer code, WebSocket message handlers, state visualization
- Depends on: HTTP server, WebSocket, LVGL (optional)
- Used by: End users + developers (debugging)

## Data Flow

**Audio Processing Cycle (Core 1, 8.33ms @ 48kHz):**

```
I2S DMA interrupt → audio_pipeline_task
  ├─ Wait on event group (EVT_AUDIO or 2-tick timeout)
  ├─ Read 256 samples from all I2S ports via i2s_read()
  │   └─ AudioInputSource::read_callback() → lane[0-7] float32[-1.0,+1.0]
  ├─ For each lane:
  │   └─ dsp_pipeline_process_channel() → 24-stage IIR/FIR/gain/compressor chain
  ├─ Routing matrix: 16 input channels → 16 output channels (per-cell gain)
  ├─ For each output:
  │   └─ output_dsp_process_channel() → per-output DSP (limiter, compressor, gain, mute)
  └─ For each sink slot:
      ├─ AudioOutputSink::write_callback()
      │   └─ sink_apply_volume() → sink_float_to_i2s_int32() → I2S TX DMA
      └─ Update VU metering (lane + output dBFS) via thread-safe snapshots
```

**State Broadcast Cycle (Main Loop, Core 0, event-triggered):**

```
Main Loop
  ├─ Wait on app_events_wait(5ms) — instant wakeup if any dirty flag set
  ├─ Check all dirty flags:
  │   ├─ WiFi state → WS broadcast + MQTT publish (via mqtt_task)
  │   ├─ ADC enabled → WS broadcast
  │   ├─ DSP config → WS broadcast (no MQTT unless explicit user save)
  │   ├─ HAL devices → WS broadcast (enumerate from HalDeviceManager)
  │   ├─ Audio channel map → WS broadcast (input lane + output sink names)
  │   └─ Buzzer/Display/Settings → similar patterns
  ├─ Drain HalCoordState toggle queue:
  │   └─ For each PendingDeviceToggle: dac_activate_for_hal() / dac_deactivate_for_hal()
  ├─ MQTT async publish (runs concurrently in mqtt_task on Core 0 @ 20Hz)
  │   └─ Change-detection statics (mqtt_publish.cpp) minimize broker traffic
  └─ OTA check/download (one-shot task spawned from REST handler)
```

**Device Lifecycle (HAL State Machine):**

```
UNKNOWN (post-registerDevice)
  ↓ probe() called by discovery engine
DETECTED (chip ID verified or EEPROM match)
  ↓ init() called by HalDeviceManager
CONFIGURING (internal use, ~1 frame)
  ↓ init() returns success
AVAILABLE (device ready, registered with audio pipeline)
  ⇌ UNAVAILABLE (transient: I2C NAK, register read fail)
  ↓ device removed or init fails
ERROR / REMOVED / MANUAL (terminal states → deregister from audio pipeline)
```

**State Management:**

- `AppState::getInstance()` — Singleton access, no locks (Core 1 audio task reads volatile fields lock-free)
- Domain-specific nested access: `appState.wifi.ssid`, `appState.audio.adcEnabled[lane]`, `appState.dsp.enabled`
- Dirty flags: `isWifiDirty()`, `isAdcEnabledDirty()`, `isDspDirty()` → main loop reads & clears
- Cross-core volatile semantics: Audio task writes `appState.audio.audioPaused`, main loop reads via binary semaphore `audioTaskPausedAck`

## Key Abstractions

**AudioInputSource (Lane Provider):**
- Purpose: Per-lane I2S read callback encapsulation
- Examples: PCM1808 ADC driver, USB Audio RX, Signal Generator, test fixture
- Pattern: Registered to lane 0-7 via `audio_pipeline_set_source(lane, &source)`. Pipeline calls `source->read(samples, ch_count)` per cycle. HAL bridge discovers count via `HalDeviceManager::countByCapability(HAL_CAP_ADC_PATH)`

**AudioOutputSink (Slot Writer):**
- Purpose: Per-slot I2S write callback + capability query
- Examples: PCM5102A (I2S), ES8311 (I2S + volume), MCP4725 (I2C), USB Audio TX
- Pattern: Built by `device->buildSink(slot, &sink)`. Pipeline writes `sink->write(samples)` per cycle. `isReady()` check gates underrun-free writes

**HalDevice (Driver Base):**
- Purpose: Abstract interface for all HAL-managed devices
- Examples: All 50+ driver classes inherit from HalDevice
- Pattern: Lifecycle methods `probe()`, `init()`, `deinit()`, `healthCheck()` + optional `getInputSource()`/`buildSink()`. State transitions emit `HalStateChangeCb`

**HalDeviceManager (Registry):**
- Purpose: Device lifecycle ownership + state coordination
- Location: `src/hal/hal_device_manager.h/.cpp`
- Pattern: Singleton instance manages up to 24 slots. Per-device `HalDeviceConfig` persisted to `/hal_config.json` (I2C/I2S/GPIO overrides). State change fires registered callback (e.g., bridge's `hal_pipeline_state_change()`)

**HalCoordState (Device Toggle Queue):**
- Purpose: Deferred cross-core device enable/disable without blocking
- Location: `src/state/hal_coord_state.h` (inline methods) + `src/state/hal_coord_state.cpp` (portMUX spinlock)
- Pattern: `requestDeviceToggle(halSlot, action)` enqueues request (capacity 8, same-slot dedup). Main loop drains via `hasPendingToggles()` → `pendingToggleAt(i)` → `clearPendingToggles()`. Overflow telemetry: `_overflowCount` (lifetime) + `consumeOverflowFlag()` (one-shot, triggers `DIAG_HAL_TOGGLE_OVERFLOW`)

**HalPipelineBridge (Device ↔ Pipeline):**
- Purpose: Maps HAL state transitions to audio pipeline registration/removal
- Location: `src/hal/hal_pipeline_bridge.h/.cpp`
- Pattern: Registered as `HalStateChangeCb` at boot. Receives state changes, decides action:
  - AVAILABLE → `audio_pipeline_set_sink()` / `markAdcEnabledDirty()`
  - UNAVAILABLE → set `_ready=false` (transient, no removal)
  - REMOVED/ERROR/MANUAL → `audio_pipeline_remove_sink()` / clear ADC lane
  - Capability-based ordinal: DAC devices map to sink slot = `HAL_CAP_DAC_PATH` ordinal; ADC devices map to lane = `HAL_CAP_ADC_PATH` ordinal

**HAL Discovery Engine:**
- Purpose: 3-tier device detection with SDIO conflict guard
- Location: `src/hal/hal_discovery.h/.cpp`
- Pattern: I2C bus scan (0x08–0x77), EEPROM v3 probe, manual config import. Skips Bus 0 when `hal_wifi_sdio_active()` returns true (checks `connectSuccess || connecting || activeInterface==NET_WIFI`). Emits `DIAG_HAL_I2C_BUS_CONFLICT` on Bus 0 skip. Scan API returns `partialScan` flag

**Dirty Flag Pattern (Change Detection):**
- Purpose: Minimize WebSocket/MQTT chatter by detecting state changes
- Pattern: `bool _xxxDirty` field + setter `markXxxDirty()` that also calls `app_events_signal(EVT_XXX)`. Main loop reads flag, broadcasts if set, clears flag
- Examples: WiFi state (EVT_WIFI), ADC enabled (EVT_ADC_ENABLED), DSP (EVT_DSP), Display (EVT_DISPLAY), Buzzer (EVT_BUZZER)

## Entry Points

**Firmware Entry:**
- Location: `src/main.cpp` → Arduino `setup()` / `loop()`
- Triggers: Board power-on → FreeRTOS scheduler → Arduino setup → app initialization loop
- Responsibilities: Singleton creation (AppState, HalDeviceManager), task spawning (audio, MQTT, GUI, USB), event group init, HAL discovery + pipeline sync

**WebSocket Event Handler:**
- Location: `src/websocket_handler.cpp::webSocketEvent()`
- Triggers: WebSocket client connect/disconnect/message
- Responsibilities: Auth token validation, message dispatch (command → REST handler), binary frame handling (waveform/spectrum), state broadcast dequeue

**REST API Handlers:**
- Location: `src/main.cpp` → `server.on()` + `server.handleClient()`
- Triggers: HTTP request to `/api/*` endpoints
- Responsibilities: JSON parsing, AppState mutation, HAL operations, response serialization. Deferred saves via dirty flags

**HAL Device Callback (State Change):**
- Location: `src/hal/hal_pipeline_bridge.cpp::hal_pipeline_state_change()`
- Triggers: HalDeviceManager sets `device->_state` → calls registered `HalStateChangeCb`
- Responsibilities: Update audio pipeline registration (sink set/remove), mark ADC lanes dirty, emit diagnostic events

**MQTT Task (Core 0):**
- Location: `src/mqtt_task.cpp` → FreeRTOS task spawned at boot
- Triggers: 50ms timer tick (20Hz poll rate)
- Responsibilities: MQTT client reconnect on broker config change, periodic publish of state snapshots, HA discovery. Never blocks main loop

**Audio Task (Core 1):**
- Location: `src/audio_pipeline.cpp` → FreeRTOS task spawned during pipeline init
- Triggers: I2S DMA interrupt → audio event group signal
- Responsibilities: Sample read/process/write, DSP apply, matrix routing, sink dispatch. Runs under `vTaskSuspendAll()` (Core 1) during slot mutations

## Error Handling

**Strategy:** Multi-level telemetry with fallback silence (no data > corrupt data)

**Patterns:**

1. **Diag Events (Error Codes):**
   - Central journal: `src/diag_journal.h/.cpp` (800-entry persistent ring + 32-entry hot PSRAM buffer)
   - Emitted via `diag_emit(code, [optional fields])` → stored with timestamp + correlation ID
   - Consumed by: REST API (snapshot export), WebSocket (real-time log), serial logging (opt-in level)
   - Examples: `DIAG_HAL_I2C_BUS_CONFLICT` (0x1101), `DIAG_HAL_TOGGLE_OVERFLOW` (0x100E), `DIAG_SYS_HEAP_WARNING` (0x0107)

2. **Allocation Failures:**
   - PSRAM→SRAM fallback: `heap_caps_calloc(size, MALLOC_CAP_SPIRAM)` with internal SRAM retry
   - On SRAM alloc fail: emit `DIAG_SYS_PSRAM_ALLOC_FAIL` (0x0109), reject DSP stage add/OTA download
   - Heap pressure thresholds: `HEAP_WARNING_THRESHOLD` (50KB) + `HEAP_CRITICAL_THRESHOLD` (40KB)

3. **Device Lifecycle Failures:**
   - I2C NAK during probe/health → state → UNAVAILABLE (transient, no removal)
   - Init failure with error code → state → ERROR (terminal, audio pipeline removes sink)
   - REST handler calls → return HTTP 503 if device toggle queue full, HTTP 400 on validation fail

4. **Audio Pipeline Underruns:**
   - Sink `isReady()` returns false → skip write (no garbage output)
   - Count underruns in `DacState.txUnderruns` (instrumentation only)
   - Trigger re-init on persistent underrun (HAL health check → UNAVAILABLE → re-AVAILABLE cycle)

5. **Interlock Safety:**
   - `appState.audio.audioPaused` + binary semaphore `audioTaskPausedAck` before I2S driver uninstall
   - Audio task yields when paused, gives semaphore → main loop continues deinit
   - 50ms timeout guard (prevents hang if audio task crashes)

## Cross-Cutting Concerns

**Logging:**
- Module-prefixed serial output via `LOG_D`/`LOG_I`/`LOG_W`/`LOG_E` macros in `src/debug_serial.h`
- Format: `[ModuleName] message` (prefix extracted for WebSocket category filtering)
- Real-time WebSocket forward: `broadcastLine()` sends module separately as JSON field
- Forbidden in ISR/audio task: `Serial.print` blocks on UART TX → starves DMA. Use dirty flags instead

**Validation:**
- JSON schema validation: ArduinoJson parsed objects + null checks + range bounds
- HAL pin validation: `HAL_GPIO_MAX=54` upper bound, `claimPin()` rejects invalid GPIO with LOG_W
- Matrix bounds: `static_assert(MAX_INPUTS*2 <= MATRIX_SIZE)` + runtime `firstChannel + channelCount <= MATRIX_SIZE` check in sink builders
- Deferred matrix save: 2s debounce timer (dirty flag set → main loop waits 2s idle → `settings_manager.cpp` writes `/audio_config.json`)

**Authentication:**
- Web password: PBKDF2-SHA256 10k iterations, salt stored in `/auth.json`
- Cookies: HttpOnly + Secure flags on HTTPS (TLS offload to reverse proxy in production)
- WebSocket tokens: 16-slot pool, 60s TTL, short-lived lookup from `GET /api/ws-token`
- Rate limiting: HTTP 429 on >5 auth failures per minute (per IP)

**Memory Management:**
- PSRAM for audio buffers (66KB) + DSP delay lines (~77KB): guarded by free-block checks
- Heap budget tracker: `src/heap_budget.h/.cpp` (32-entry subsystem allocation log)
- Feature shedding on `heapCritical`: DMA alloc refused, WS binary suppressed, DSP stages rejected, OTA skipped
- MCLK continuity: never call `i2s_configure_adc1()` in audio task loop (MCLK must remain continuous)

**Testing Coverage:**
- C++ unit tests: 1730+ tests via Unity framework on `native` environment
- Mock implementations: `test/test_mocks/` (Arduino, WiFi, MQTT, Preferences)
- E2E browser tests: 26 Playwright tests on mock Express server + WS interception
- CI/CD gates: cpp-tests, cpp-lint, js-lint, e2e-tests (all must pass before build)

---

*Architecture analysis: 2026-03-21*
