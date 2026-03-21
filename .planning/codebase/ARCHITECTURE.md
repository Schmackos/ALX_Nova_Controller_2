# Architecture

**Analysis Date:** 2026-03-21

## Pattern Overview

**Overall:** Layered architecture with Hardware Abstraction Layer (HAL) singleton managing devices, real-time audio pipeline with DSP chains, and event-driven state synchronization across WiFi, MQTT, and WebSocket interfaces.

**Key Characteristics:**
- Real-time audio pipeline (8-lane input → 16×16 matrix → 8-slot sink output) runs on dedicated FreeRTOS Core 1 task
- Pluggable HAL device framework with capability-based slot/lane assignment and state callback system
- AppState domain decomposition (15 lightweight state headers) with dirty-flag change detection
- Event-driven main loop waking immediately on state changes via `app_events_wait()`
- Cross-core synchronization via atomic flags and binary semaphores (I2S driver handshake)

## Layers

**Hardware Abstraction Layer (HAL):**
- Purpose: Unified device lifecycle management (UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ⇄ UNAVAILABLE → ERROR/REMOVED/MANUAL), discovery via I2C bus scan + EEPROM probing, runtime pin/bus configuration persistence, multi-instance device support
- Location: `src/hal/` (24 files covering device manager, bridge, builtin drivers, custom devices, discovery, persistence)
- Contains: `HalDevice` base class, driver registry, device database, pipeline bridge, EEPROM v3 parser
- Depends on: AppState (dirty flags), Audio pipeline (sink/source registration), LittleFS (config persistence), I2C/GPIO hardware
- Used by: Audio pipeline bridge (capability-based slot/lane assignment), main.cpp (boot init sequence), REST API endpoints, WebSocket broadcasts

**Audio Pipeline (Real-Time):**
- Purpose: Stream processing engine: reads from HAL-registered input sources → applies per-input DSP → routes through 16×16 matrix → applies per-output DSP → writes to HAL-registered sinks. All sample processing float32 [-1.0, +1.0] in RAM buffers allocated from PSRAM for audio data, internal SRAM for DMA
- Location: `src/audio_pipeline.h/.cpp` (1088 lines)
- Contains: 16×16 routing matrix (gain[out_ch][in_ch]), input/output DSP bypass arrays, source/sink slot arrays (8 max each), noise gate fade state, DSP swap hold buffer
- Depends on: I2S audio HAL (DMA buffers, sample rate), DSP pipeline (per-lane/per-output filter chains), AppState (bypass flags)
- Used by: Main loop (writes frames), FreeRTOS audio task (reads sources, applies DSP, writes sinks)

**DSP Pipeline (Signal Processing):**
- Purpose: Multi-stage audio filtering engine (biquad IIR, FIR, limiter, gain, delay, polarity, mute, compressor) with double-buffered stage config and glitch-free atomic swap. Runs on Core 1 inside audio task
- Location: `src/dsp_pipeline.h/.cpp` (2363 lines), `src/output_dsp.h/.cpp` (per-output variant)
- Contains: Stage pool (~120 stages max), coefficient generators (RBJ Audio EQ Cookbook), delay line PSRAM allocation with heap pre-flight check
- Depends on: ESP-DSP library (assembly-optimized on ESP32-S3, lite C fallback on native), audio pipeline (stage insertion points)
- Used by: Audio pipeline task (inline DSP processing per lane/output)

**I2S Audio HAL:**
- Purpose: ADC/DAC driver abstraction for ESP32 I2S peripherals (dual PCM1808 ADC on I2S_NUM_0 and I2S_NUM_1, ES8311 codec, ES9822PRO/ES9843PRO expansion ADCs on separate I2S bus)
- Location: `src/i2s_audio.h/.cpp` (1647 lines), `src/hal/hal_i2s_bridge.cpp`
- Contains: I2S config (pin overrides via HAL), dual ADC clock coordination (both masters), TDM frame parsing (ES9843PRO), sample rate management, FFT analysis buffers
- Depends on: HAL device manager (pin/bus config from HalDeviceConfig), hardware drivers (PCM1808 register control, ES8311 I2C commands)
- Used by: Audio pipeline (reads ADC frames, writes DAC frames)

**Smart Sensing Module:**
- Purpose: Voltage detection (smoothed with rate-matched alpha), auto-off timer, amplifier relay control via GPIO or HAL relay device
- Location: `src/smart_sensing.h/.cpp`, `src/hal/hal_relay.h/.cpp`
- Contains: Detection state machine (NO_SIGNAL → SIGNAL_PRESENT → AUTO_OFF_TIMER), configurable threshold/timeout
- Depends on: AppState audio state (signal presence flags), HAL (relay device lookup), GPIO (direct relay control fallback)
- Used by: Main loop (periodic ~100ms update), amplifier control subsystem

**Network Managers:**
- **WiFi Manager** (`src/wifi_manager.h/.cpp`, 1556 lines): Multi-SSID client, AP mode, async connection with exponential backoff, sets `activeInterface = NET_WIFI` on connect/disconnect
- **Ethernet Manager** (`src/eth_manager.h/.cpp`): 100Mbps phy on ESP32-P4, fallback to WiFi on unplugged, IP assignment, DHCP
- **MQTT Handler** (split across 3 files):
  - `src/mqtt_handler.cpp` (1128 lines): Client lifecycle, broker config, callback dispatch
  - `src/mqtt_publish.cpp` (949 lines): Change-detection statics (`prevMqtt*`), 20 Hz publish task on Core 0
  - `src/mqtt_ha_discovery.cpp` (1897 lines): Home Assistant MQTT discovery protocol implementation

**Web Interface:**
- **HTTP Server** (`src/main.cpp`, port 80): REST API endpoints, static HTML/CSS/JS asset serving, compression via gzip
- **WebSocket Server** (`src/websocket_handler.cpp`, port 81): Real-time state broadcast (8 output channels with per-client subscription), binary frame support for audio waveforms (WS_BIN_WAVEFORM=0x01) and spectrum (WS_BIN_SPECTRUM=0x02), token auth with 60s TTL
- **Web Assets** (`web_src/` source, auto-compiled to `src/web_pages_gz.cpp`): HTML shell, CSS modules (tokens, layout, components, canvas, responsive), 28 JS modules in concatenated scope

**Persistent Storage:**
- **Settings Manager** (`src/settings_manager.h/.cpp`): JSON persistence (`/config.json`) with atomic write (tmp+rename), legacy `settings.txt` fallback, export/import/factory reset
- **HAL Settings** (`src/hal/hal_settings.h/.cpp`): Per-device `HalDeviceConfig` persistence (`/hal_config.json`) with pin/bus overrides
- **Diagnostic Journal** (`src/diag_journal.h/.cpp`): Event ringbuffer (64 entries), diagnostic codes with severity, event correlation IDs

## Data Flow

**Boot Sequence (Main Loop):**

1. Firmware init: CPU, memory, task watchdog
2. LittleFS mount → Load settings → Apply debug log level
3. GUI init (optional, TFT+encoder on Core 0 via FreeRTOS task)
4. HAL builtin device registry (PCM5102A, ES8311, etc.)
5. I2S audio init (ADC1+ADC2 dual master, uses HAL pin config)
6. Output DSP init (load all presets from LittleFS)
7. HAL pipeline bridge sync (capability-based slot/lane assignment)
8. Peripheral device registration (NS4150B amp, display, encoder, buzzer, button, LED, relay, temp sensor)
9. HTTP+WebSocket server startup
10. WiFi/Ethernet init (async, non-blocking)
11. MQTT task spawn on Core 0
12. Enter main loop on Core 1 (audio task spawned separately)

**Main Loop Data Flow (Core 1):**

1. Wait up to 5ms on event group (`app_events_wait(5)`), waking instantly on any dirty flag
2. WebSocket broadcast: if any authenticated client, serialize AppState changes (WiFi, audio, DSP, etc.) to JSON + binary frames
3. MQTT publish: handled independently by `mqtt_task` on Core 0 at 20 Hz (reads same dirty flags, publishes to broker)
4. Handle REST API requests (synchronous, via WebServer library callbacks)
5. Button/encoder input polling → dispatch to handlers
6. Smart sensing update (signal detection, relay control)
7. Periodic diagnostics (task monitor, heap budget, audio health check)
8. Loop back to event wait

**Audio Pipeline Data Flow (Core 1, Audio Task):**

1. I2S DMA interrupt fires (every 256 stereo frames at 48kHz ≈ 5.3ms)
2. Audio task reads ADC frames via `i2s_read()` → convert to float32 [-1.0, +1.0]
3. Check `appState.audio.paused` flag (set by DAC deinit before I2S uninstall)
4. Per-lane DSP: apply biquad stages (IIR/FIR) from double-buffered config
5. Routing matrix: sum weighted input channels → 16 output channels
6. Per-output DSP: apply biquad/limiter/compressor/etc.
7. Sink dispatch: each of 8 sinks reads its assigned output channels (slot-indexed)
8. Sink processing: apply volume curve, mute ramp (click-free), convert float32 → int32 left-justified for DAC
9. I2S write to all registered sinks (PCM5102A, ES8311, etc. via HAL)
10. VU metering: extract per-lane dBFS for display/WS broadcast
11. Yield 2 ticks, loop back

**State Management & Synchronization:**

- **AppState singleton** (`src/app_state.h`): 15 domain-specific state headers composed into shell, dirty flags for each domain (`isBuzzerDirty()`, `isDspConfigDirty()`, etc.)
- **Dirty flag pattern**: Task sets flag + calls `app_events_signal(EVT_XXX)`, main loop polls flag, serializes to JSON/MQTT, clears flag
- **Cross-core volatiles**: `appState.audio.paused` (bool), `appState.audio.taskPausedAck` (binary semaphore), `appState._mqttReconfigPending` (bool), `appState._pendingApToggle` (int8_t)
- **I2S handshake**: DAC deinit sets `paused=true`, waits `xSemaphoreTake(taskPausedAck, 50ms)`. Audio task observes flag, gives semaphore when yielding

## Key Abstractions

**AudioInputSource:**
- Purpose: Pluggable input lane source (ADC, codec, signal generator, USB audio, etc.)
- Examples: `src/audio_input_source.h` struct with `read()` callback, HAL drivers register via `audio_pipeline_set_source(lane, source)`
- Pattern: Slot-indexed lane array (8 max), each lane independently registered/cleared atomically via `vTaskSuspendAll()`

**AudioOutputSink:**
- Purpose: Pluggable output slot sink (DAC, USB host, etc.)
- Examples: `src/audio_output_sink.h` struct with `write()` callback, HAL drivers register via `audio_pipeline_set_sink(slot, sink)`
- Pattern: Slot-indexed sink array (8 max), HAL-Pipeline Bridge owns all sink lifecycle

**HalDevice (Base Class):**
- Purpose: Abstract device model with lifecycle states, init/deinit, runtime pin config, discovery metadata
- Location: `src/hal/hal_device.h`
- Pattern: Subclass per device type (HalPcm5102a, HalEs8311, HalEs9822pro, etc.), register via `registerDevice()`, auto-init via HAL discovery or manual `probe()/init()`

**HalDeviceConfig:**
- Purpose: Persistent runtime config per device (pin/bus overrides, gain, enable state, etc.)
- Pattern: Serialized to `/hal_config.json`, loaded on boot, runtime changes queued for save
- Stored in: Each HalDevice instance has `HalDeviceConfig _config` field

**DspStage:**
- Purpose: Single audio processing step (biquad, gain, delay, etc.) with coefficient set
- Pattern: Stage pool (~120 capacity), double-buffered active config + pending config for glitch-free swap
- Accessed via: `dsp_add_stage()`, `dsp_get_stage()`, `dsp_swap_config()` with overflow telemetry

## Entry Points

**Firmware Setup (`src/main.cpp` setup()):**
- Location: `src/main.cpp` lines 140-500
- Triggers: Hardware reset, OTA update completion
- Responsibilities: Initialize all subsystems in sequence (LittleFS → HAL → audio → WiFi → servers), spawn tasks, register REST API endpoints

**Main Loop (`src/main.cpp` loop()):**
- Location: `src/main.cpp` lines 500+
- Triggers: Continuous on Core 1, wakes via `app_events_wait(5)`
- Responsibilities: Broadcast state changes via WebSocket, dispatch button/encoder input, update smart sensing, log diagnostics

**Audio Pipeline Task (`src/audio_pipeline.cpp`):**
- Location: Spawned by `audio_pipeline_init()`, runs on Core 1 priority 3
- Triggers: I2S DMA interrupt every ~5.3ms
- Responsibilities: Read ADC frames, apply DSP, mix via matrix, write to sinks, compute VU meters

**MQTT Task (`src/mqtt_task.cpp`):**
- Location: Spawned at boot, runs on Core 0 priority 2
- Triggers: 20 Hz poll timer, `_mqttReconfigPending` flag
- Responsibilities: Maintain broker connection, periodic publish of sensor state, respond to HA commands

**REST API Endpoints (`src/main.cpp`, registered via `registerXxxApiEndpoints()`):**
- HTTP server (port 80) routes to handlers:
  - WiFi: `GET /api/networks`, `POST /api/network/connect`, etc.
  - MQTT: `GET /api/mqtt/settings`, `PUT /api/mqtt/settings`
  - HAL: `GET /api/hal/devices`, `POST /api/hal/scan`, `PUT /api/hal/devices/{id}`, `DELETE /api/hal/devices/{id}`
  - Audio: `GET /api/audio/matrix`, `PUT /api/audio/matrix`, per-output DSP CRUD
  - DSP: `GET /api/dsp/presets`, `POST /api/dsp/config`, `PUT /api/dsp/swap`
- All endpoints guard on `appState.halSafeMode` (skip audio/HAL if boot loop detected)

**WebSocket Handler (`src/websocket_handler.cpp`):**
- Location: Port 81, token auth required
- Messages: JSON broadcasts (state sync), binary frames (waveform 0x01, spectrum 0x02)
- Subscription model: Per-client flag `_audioSubscribed[clientId]` gates high-frequency audio data
- Deferred init state: `_pendingInitState[clientId]` spreads auth-success burst across 16 main-loop iterations to prevent WiFi TX saturation

## Error Handling

**Strategy:** Defensive nullability, feature degradation, diagnostic logging with severity levels, no exceptions (Arduino/embedded constraint)

**Patterns:**

- **HAL device registration overflow**: `registerDevice()` returns -1 on capacity exhaustion (24/24 slots), caller logs warning, emits `DIAG_HAL_REGISTRY_FULL` (0x100F)
- **DSP swap failure**: If pool exhausted during `dsp_swap_config()`, all 42 callers check return value, respond with HTTP 503 or log warning, leave DSP unchanged
- **I2S driver uninstall**: DAC deinit path sets `appState.audio.paused=true`, waits binary semaphore, audio task gives semaphore when safe. Timeout 50ms prevents deadlock if audio task hung
- **Heap memory pressure**: 3-state graduated (normal → warning @ 50KB free → critical @ 40KB free), features shed at each level (WS binary rate halved, DMA alloc refused, DSP stages rejected, OTA skipped)
- **WiFi SDIO conflict**: I2C Bus 0 (GPIO 48/54) skipped during HAL discovery when WiFi active, `hal_wifi_sdio_active()` checks `connectSuccess || connecting || activeInterface == NET_WIFI`, emits `DIAG_HAL_I2C_BUS_CONFLICT` (0x1101)
- **Boot loop detection**: 3 consecutive crash boots → `appState.halSafeMode=true` → skip HAL init, WiFi+web only, emit `DIAG_SYS_BOOT_LOOP` (0x0101)

## Cross-Cutting Concerns

**Logging:** Centralized `debug_serial.h` macros (`LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`) with module prefixes (`[ModuleName]`). Runtime level control via `applyDebugSerialLevel()`. WebSocket log forwarding via `broadcastLine()` (sends separate `"module"` JSON field for category filtering). Never log in ISR or audio task (blocks UART TX, starves DMA)

**Validation:** Input bounds checking on matrix dimensions (compile-time `static_assert` + runtime guards), GPIO number validation (rejects GPIO>54), I2C address range validation (0x08-0x77)

**Authentication:** Web password PBKDF2-SHA256 (10k iterations), HttpOnly cookie, WS token pool (16 slots, 60s TTL), rate limiting on failed auth (HTTP 429)

**Synchronization:** FreeRTOS `vTaskSuspendAll()` for atomic audio pipeline slot updates, binary semaphore for I2S handshake, volatile flags for cross-core coordination, no locks in audio task (real-time constraint)

---

*Architecture analysis: 2026-03-21*
