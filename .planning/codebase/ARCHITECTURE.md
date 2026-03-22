# Architecture

**Analysis Date:** 2026-03-22

## Pattern Overview

**Overall:** Modular layered architecture with Hardware Abstraction Layer (HAL) as the sole device manager, Finite State Machine (FSM) driven main loop, and domain-specific state composition.

**Key Characteristics:**
- **Hardware Abstraction Layer (HAL)** — 32-device registry with lifecycle management (UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ⇄ UNAVAILABLE → ERROR/REMOVED/MANUAL). Single source of truth for all hardware.
- **Domain-Driven State** — 15 lightweight state headers composed into AppState singleton. Each domain (WiFi, MQTT, Audio, DAC, etc.) has dedicated state struct accessed via `appState.domain.field`.
- **Dirty Flags + Event Signaling** — Every state mutation sets a dirty flag and signals an event bit, enabling the main loop to wake immediately on change and fall back to 5ms tick when idle.
- **FreeRTOS Cross-Core Coordination** — Core 0 runs WiFi/MQTT/GUI tasks. Core 1 is audio-exclusive: main loop (priority 1) + audio pipeline task (priority 3). State changes use atomic flags + binary semaphore handshake.
- **Audio Pipeline: 8-lane input → per-lane DSP → 32x32 matrix → per-output DSP → 16-slot sink dispatch** — Float32 internally, HAL-managed sources/sinks registered dynamically at runtime.

## Layers

**Web Client (Browser)**
- Purpose: User interface and real-time monitoring
- Location: `web_src/`
- Contains: HTML, CSS (modular by concern), JavaScript modules (28 files)
- Depends on: HTTP (port 80) for assets, WebSocket (port 81) for state sync
- Used by: Web browser on desktop/mobile/tablet

**HTTP API Server**
- Purpose: RESTful endpoints for system control and configuration
- Location: `src/main.cpp` (route handlers), `src/*/api.cpp` (domain-specific endpoints)
- Contains: HTTP handlers, JSON serialization, auth gates, response formatting
- Depends on: WebServer (port 80), AppState, HAL, domain modules
- Used by: Web client, external tools via REST

**WebSocket Broadcast Server**
- Purpose: Real-time state synchronization to connected web clients
- Location: `src/websocket_handler.cpp`
- Contains: Binary frame protocol (waveform 0x01, spectrum 0x02), JSON state broadcasts, token-based auth (60s TTL, 16-slot pool)
- Depends on: AppState dirty flags, audio analysis buffers, HAL device status
- Used by: Web client for live updates (adaptive rate: skip 1/2/4/6/8 based on client count and heap pressure)

**Domain-Specific Handlers**
- Purpose: Business logic for each subsystem
- Location: `src/{module}_handler.cpp` or `src/{module}.cpp`
- Contains: WiFi connection, MQTT broker sync, OTA updates, settings persistence, signal generation, smart sensing
- Depends on: AppState, WebServer, PubSubClient, HAL
- Used by: Main loop, FreeRTOS tasks on Core 0

**Audio Pipeline (Core 1 Real-Time)**
- Purpose: Continuous audio I/O with DSP processing
- Location: `src/audio_pipeline.cpp`, `src/i2s_audio.cpp`, `src/dsp_pipeline.cpp`
- Contains: 8 input lanes, 16x16 routing matrix, 8 output sinks, per-lane + per-output DSP
- Depends on: I2S hardware, DMA buffers (pre-allocated at boot), HAL for source/sink registration
- Used by: DAC HAL drivers, HAL pipeline bridge, output DSP
- **Core exclusivity**: Audio task never yields to main loop; main loop never calls i2s_read()

**HAL Device Manager (Core 0, Main Loop)**
- Purpose: Lifecycle and state management for all hardware
- Location: `src/hal/hal_device_manager.h/.cpp`
- Contains: 32-slot device registry, pin tracking (56 GPIO), priority-sorted init, retry logic
- Depends on: Device registry, pin allocation, config persistence
- Used by: HAL discovery, HAL API, HAL pipeline bridge, main loop health checks

**HAL Pipeline Bridge**
- Purpose: Connects HAL device state changes to audio pipeline
- Location: `src/hal/hal_pipeline_bridge.h/.cpp`
- Contains: Source/sink registration on device lifecycle changes, capability-based ordinal counting, multi-source/multi-sink support
- Depends on: HAL device manager, audio pipeline, state callbacks
- Used by: HAL device manager (state change callback), main loop (indirectly via device transitions)

**HAL Device Drivers (23 Built-in + 21 Expansion)**
- Purpose: Hardware-specific register control and protocol handling
- Location: `src/hal/hal_*.h/.cpp` (onboard: PCM5102A, ES8311, NS4150B, etc.)
- Contains: Initialization, I2C/I2S config, volume control, mute, filter selection, capability flags
- Depends on: Wire (I2C), I2S hardware, config overrides, HAL base classes
- Used by: HAL device manager, audio pipeline bridge, REST API

**Persistence Layer**
- Purpose: LittleFS configuration storage and NVS for critical state
- Location: `src/settings_manager.cpp`, `src/hal/hal_settings.cpp`, `src/dac_eeprom.cpp`
- Contains: Atomic JSON writes (/config.json), legacy fallback (settings.txt), per-device HAL config (/hal_config.json), DAC EEPROM diagnostics
- Depends on: LittleFS, NVS (Preferences), ArduinoJson
- Used by: Settings manager, HAL device manager, DAC module on boot

**Diagnostic Journal**
- Purpose: Event logging and health telemetry
- Location: `src/diag_journal.cpp`
- Contains: Ring buffer of DiagEvent structs (code, severity, timestamp), emission helpers with auto-deduplication
- Depends on: AppState dirty flags, heap/PSRAM pressure tracking
- Used by: All modules via `diag_emit()`, WebSocket broadcasts, REST endpoints

**GUI (LVGL on TFT, Core 0)**
- Purpose: Local interface on ST7735S 128x160 display
- Location: `src/gui/`
- Contains: 8 screens (Home, Control, WiFi, MQTT, Settings, Debug, Support, Boot), input handling (encoder/button), theme management
- Depends on: LVGL v9.4, LovyanGFX, AppState, HAL devices, signal changes via app_events
- Used by: End user on device

## Data Flow

**Signal Detection & Auto-Off Flow:**

1. **Sensing Task (main loop):** `smart_sensing_update()` reads ADC1 + ADC2 via `audio_pipeline_get_lane_vu_*()` (non-blocking read of pipeline analysis)
2. **Threshold Check:** Compare combined RMS to `appState.audio.adcThreshold` (user-configurable 0-100, default 10)
3. **State Transitions:** ADC rising above threshold → `STATE_SIGNAL_DETECTED`; falling below + timer expires → `STATE_AUTO_OFF_TIMER` → `STATE_IDLE`
4. **Amplifier Control:** `setAmplifierState()` routes through `HalRelay::setEnabled()` (if available), falls back to direct GPIO 27
5. **WS Broadcast:** State change → dirty flag → main loop broadcasts sensing mode + amplifier state to web clients

**Audio Input Path:**

1. **I2S ADC Hardware:** ESP32-P4 dual PCM1808 (I2S0 ADC1 master: GPIO 20/21/22/23, I2S1 ADC2 slave data-only: GPIO 25)
2. **Pipeline Lane Assignment:** HAL discovery registers PCM1808 → `audio_pipeline_set_source(lane, &source)` → DMA ISR fills frame buffers on Core 1
3. **Analysis:** `i2s_audio_read()` computes RMS/VU/peak per ADC, aggregates to `AudioAnalysis` struct (read-only by main loop)
4. **Per-Lane DSP:** Optional per-input processing (biquad, gain, etc.) applied before matrix
5. **Matrix Routing:** 16x16 gain matrix mixes all inputs → 16 output channels (typically 8 mono + 8 spare)
6. **Per-Output DSP:** Mono processing (PEQ, limiter, compressor) on each output
7. **Sink Dispatch:** Audio pipeline calls `sink->write(samples, channelCount, sampleRate)` for each registered sink (HAL DAC drivers fill I2S TX buffers)

**HAL Device Lifecycle (State Callback Flow):**

1. **Discovery:** `POST /api/hal/scan` → `hal_discovery_scan()` enumerates I2C buses 0/1/2, EEPROM probe, device DB lookup
2. **Auto-Init (Optional):** `appState.halAutoDiscovery=true` → main loop calls `HalDeviceManager::initAll()` on discovered devices (priority-sorted)
3. **Init:** Device calls `init()` → I2C register write, GPIO claim, config override read → transitions UNKNOWN → DETECTED → CONFIGURING
4. **Availability Check:** Device `postInit()` verifies communication → AVAILABLE state
5. **State Change Callback:** `HalDeviceManager` fires `HalStateChangeCb` → `hal_pipeline_bridge::onHalStateChange()` →
   - UNAVAILABLE: Sets sink/source `_ready=false` (stays registered, audio pipeline skips writes)
   - MANUAL/ERROR/REMOVED: Calls `audio_pipeline_remove_sink(slot)` / `audio_pipeline_remove_source(lane)`
   - AVAILABLE: Calls `audio_pipeline_set_sink(slot, &sink)` / `audio_pipeline_set_source(lane, &source)`
6. **WS Broadcast:** Dirty flag set → main loop broadcasts device status, channel map, capabilities

**HAL Multi-Instance & Multi-Sink/Multi-Source:**

- **Dual ADC + Expansion ADC:** PCM1808 (lanes 0,1) + ES9843PRO (4ch TDM, registers as lanes 2-3 via `HalTdmDeinterleaver`)
- **Dual DAC:** PCM5102A (output sinks slot 0) + ES9038PRO (8ch, sinks slots 1-4, via `HalTdmInterleaver`)
- **Ordinal Assignment:** Capability-based slot/lane counting in `hal_pipeline_bridge`:
  - `HAL_CAP_ADC_PATH` → input lane (0,2,4,6)
  - `HAL_CAP_DAC_PATH` → output sink slot (0,1,2,4,8)
  - Multi-source device: `getInputSourceCount()` returns 2 (ES9843PRO stereo pairs)
  - Multi-sink device: `getSinkCount()` returns 4 (ES9038PRO 4×stereo)

**DSP Configuration Swap (Glitch-Free):**

1. **Web UI:** User imports EQ profile → REST `POST /api/dsp/config` → `dsp_api.cpp` queues deferred save
2. **Config Swap:** `dsp_swap_config()` on Core 0 atomically swaps `_currentCfg` ↔ `_nextCfg` pointers
3. **Audio Task Coordination:** Before swap, `audio_pipeline_notify_dsp_swap()` arms PSRAM hold buffer
4. **Glitch Prevention:** If swap gap occurs, pipeline uses last-good-frame from hold buffer (≤4ms latency)

**Settings Persistence & Boot Flow:**

1. **Boot:** `setup()` reads `/config.json` → `settings_manager::loadSettings()` (ArduinoJson deserialization)
2. **Fallback:** If JSON corrupted, reads legacy `settings.txt` (PBKDF2 hashes, MQTT config, display settings)
3. **WiFi Credentials:** Stored in NVS (survives LittleFS format)
4. **HAL Config:** Per-device bus/pin/register overrides in `/hal_config.json` (loaded by `hal_settings.cpp`)
5. **Atomic Write:** On save, writes to tmp file, renames (atomic on most filesystems) to prevent corruption on power-loss

**State Management across Cores:**

- **Core 0 Tasks:** Main loop, WiFi, MQTT, GUI, OTA
- **Core 1 Tasks:** Audio pipeline (real-time), main Arduino loop (low-priority, 1ms tick)
- **AppState Dirty Flags:** Set by Core 0 tasks → main loop reads on next iteration → broadcasts to WS
- **Volatile Flags:** `appState.audio.audioPaused` (Core 0 DAC toggle waits on Core 1 ack via binary semaphore)
- **Deferred Device Toggle Queue:** `HalCoordState.requestDeviceToggle(slot, action)` → main loop calls `dac_activate_for_hal()` / `dac_deactivate_for_hal()`

## Key Abstractions

**HalDevice (Base Class):**
- Purpose: Polymorphic interface for all hardware
- Examples: `hal_pcm5102a.cpp`, `hal_es8311.cpp`, `hal_es9038pro.cpp`
- Pattern: Virtual `init()`, `postInit()`, `getCapabilities()`, `getState()` override per driver. Lifecycle state machine enforced by HalDeviceManager.

**AudioInputSource & AudioOutputSink:**
- Purpose: Abstraction for pipeline source/sink registration without tight coupling to HAL
- Examples: `audio_input_source.h`, `audio_output_sink.h`
- Pattern: Source has read callback `bool (*read)()`, sink has write callback `void (*write)()`. HAL drivers wrap these in `HalAudioDevice` subclass.

**DspStage (DSP Pipeline Module):**
- Purpose: Pluggable DSP processing (biquad, FIR, limiter, compressor, delay)
- Examples: `dsp_pipeline.h` defines `DspStage` struct with config pointer + apply function
- Pattern: Double-buffered config (current ↔ next), atomic swap on glitch-free boundaries

**HalCoordState (Cross-Core Toggle Queue):**
- Purpose: Deferred device toggle requests from REST handlers to main loop
- Pattern: Fixed-size queue (capacity 8), same-slot deduplication, overflow telemetry + HTTP 503 on failure

**DiagEvent (Diagnostic Emission):**
- Purpose: Structured event logging for health/error tracking
- Pattern: Ring buffer (max 256 events), auto-deduplication by (code, deviceSlot) tuple, severity levels (INFO/WARN/ERROR/CRIT)

## Entry Points

**Firmware Boot:**
- Location: `src/main.cpp` → `setup()` (runs once) → `loop()` (repeats every ~1-5ms)
- Triggers: Power-on reset, OTA update complete
- Responsibilities: Initialize HAL, load settings, spawn FreeRTOS tasks, configure WiFi/MQTT, start audio pipeline

**HTTP Request Handler:**
- Location: `src/main.cpp` → `server.on("/api/...", handler)` (60+ routes)
- Triggers: GET/POST/PUT/DELETE from web client
- Responsibilities: Validate auth, parse JSON, update state, queue deferred operations (HAL scan, DSP save, device toggle), send JSON response

**WebSocket Message Handler:**
- Location: `src/websocket_handler.cpp` → `onWsEvent()`
- Triggers: WebSocket binary/text frames from authenticated client
- Responsibilities: Parse command (JSON), update state, broadcast confirmation (if needed)

**FreeRTOS Tasks (Core 0):**
- **WiFi Task:** `wifi_manager.cpp` → Connection, scan, AP mode toggle
- **MQTT Task:** `mqtt_task.cpp` → Broker reconnect, publish at 20Hz, HA discovery
- **GUI Task:** `gui/gui_manager.cpp` → Screen rendering, input handling
- **OTA Tasks:** `ota_updater.cpp` → Version check, download, verification (spawned on-demand)

**Audio Pipeline Task (Core 1):**
- Location: `src/audio_pipeline.cpp` → FreeRTOS task created by `audio_pipeline_init()`
- Triggers: DMA ISR every 4.66ms (48kHz, 256-sample buffer)
- Responsibilities: Mix inputs, route matrix, apply output DSP, dispatch sinks, compute timing metrics, manage heap pressure gates

**I2S DMA ISR (Core 1):**
- Location: Installed by `i2s_audio_init()` via ESP-IDF driver
- Triggers: Every 4.66ms (256-sample frame)
- Responsibilities: Fill I2S RX buffers (ADC), consume I2S TX buffers (DAC), signal `audio_pipeline_task` via semaphore

## Error Handling

**Strategy:** Layered approach with dirty flags for async recovery.

**Patterns:**
- **I2C Bus Conflict:** `hal_wifi_sdio_active()` checks WiFi state before scanning Bus 0 (GPIO 48/54 shared with SDIO). Emits `DIAG_HAL_I2C_BUS_CONFLICT` (0x1101), returns `partialScan=true` to REST client.
- **I2C Timeout Retry:** On addresses with error codes 4/5, retries up to 2 times with 50ms backoff. Emits `DIAG_HAL_PROBE_RETRY_OK` (0x1105) on recovery.
- **Device Init Failure:** HAL device transitions ERROR state, sets `_ready=false`, increments fault count in NVS. Main loop retries periodically (non-blocking backoff).
- **Heap Pressure:** Graduated 3-state: Normal (≥50KB free) → Warning → Critical (<40KB). Critical blocks DSP allocation, OTA checks, binary WS data.
- **PSRAM Pressure:** Similar 3-state: Normal (≥1MB free) → Warning → Critical (<512KB). Critical blocks DSP delay/convolution allocations.
- **DAC Toggle Overflow:** Deferred toggle queue capacity 8. On overflow: increment `_overflowCount`, emit `DIAG_HAL_TOGGLE_OVERFLOW` (0x100E), return HTTP 503.
- **MQTT Socket Timeout:** TCP timeout capped at 5s in `setupMqtt()` via `mqttWifiClient.setTimeout()` to prevent 15-30s blocking on unreachable brokers.
- **OTA Certificate Validation:** Root CAs in `src/ota_certs.h` (Sectigo R46/E46, DigiCert G2). Update via `node tools/update_certs.js`.

## Cross-Cutting Concerns

**Logging:** All modules use `debug_serial.h` macros (`LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`) with consistent `[ModuleName]` prefixes. Level set via REST `/api/debug/serial-level` and boot flag `DEFAULT_DEBUG_SERIAL_LEVEL`. Forwarded to web client via binary WS frames. Core 1 (audio task) never logs directly — uses dirty flag pattern instead.

**Validation:** ArduinoJson `safeJson()` wrapper on 32+ call sites. WebSocket `validateWsMessage()` for binary frame parsing. REST endpoints check auth token before processing.

**Authentication:** PBKDF2-SHA256 (50k iterations, `p2:` format). Legacy `p1:` (10k) auto-migrated on login. First-boot random password displayed on TFT/serial. WS token 60s TTL, 16-slot pool. Cookie `HttpOnly`.

**Security Headers:** Every HTTP response adds `X-Frame-Options: DENY` and `X-Content-Type-Options: nosniff` via `http_add_security_headers()` (called from `sendGzipped()`, auth, API, 404 handlers).

---

*Architecture analysis: 2026-03-22*
