# Architecture

**Analysis Date:** 2026-03-22

## Pattern Overview

**Overall:** Layered embedded architecture with a Hardware Abstraction Layer (HAL) singleton, real-time audio pipeline on dedicated core, event-driven main loop with dirty-flag state synchronization, and multi-interface connectivity (WiFi, Ethernet, MQTT, WebSocket, HTTP REST).

**Key Characteristics:**
- Real-time audio pipeline (8-lane input -> per-input DSP -> 16x16 matrix -> per-output DSP -> 8-slot sink) runs on dedicated FreeRTOS Core 1 task at priority 3
- Pluggable HAL device framework with 3-tier discovery (I2C scan -> EEPROM probe -> manual), capability-based slot/lane assignment, and state callback system
- AppState domain decomposition (15 lightweight state headers in `src/state/`) with dirty-flag change detection and FreeRTOS event group signaling
- Event-driven main loop replacing `delay(5)` with `app_events_wait(5)` — wakes in <1 us on any state change, falls back to 5 ms tick when idle
- Strict core affinity: Core 1 reserved for audio (main loop + audio task only); Core 0 for GUI, MQTT, USB audio, OTA
- Cross-core synchronization via volatile flags, binary semaphores (I2S driver handshake), and deferred toggle queue (`HalCoordState`)

## Layers

**Hardware Abstraction Layer (HAL):**
- Purpose: Unified device lifecycle management (UNKNOWN -> DETECTED -> CONFIGURING -> AVAILABLE <-> UNAVAILABLE -> ERROR/REMOVED/MANUAL), 3-tier discovery, runtime pin/bus configuration persistence, multi-instance device support
- Location: `src/hal/` (83 files: 42 headers + 41 implementations)
- Contains: `HalDevice` base class (`src/hal/hal_device.h`), device manager (`src/hal/hal_device_manager.h/.cpp`, 24 slots max), driver registry (`src/hal/hal_driver_registry.h/.cpp`), device database (`src/hal/hal_device_db.h/.cpp`), pipeline bridge (`src/hal/hal_pipeline_bridge.h/.cpp`), EEPROM v3 parser (`src/hal/hal_eeprom_v3.h/.cpp`), ESS SABRE ADC base class (`src/hal/hal_ess_sabre_adc_base.h/.cpp`), TDM deinterleaver (`src/hal/hal_tdm_deinterleaver.h/.cpp`), 15+ concrete device drivers
- Depends on: AppState (dirty flags, `halCoord` toggle queue), Audio pipeline (sink/source registration), LittleFS (`/hal_config.json` persistence), I2C/GPIO/I2S hardware
- Used by: Audio pipeline bridge (capability-based slot/lane assignment), `src/main.cpp` (boot init), REST API (`src/hal/hal_api.h/.cpp`), WebSocket broadcasts

**Audio Pipeline (Real-Time, Core 1):**
- Purpose: Stream processing engine: reads from HAL-registered input sources -> applies per-input DSP -> routes through 16x16 matrix -> applies per-output DSP -> writes to HAL-registered sinks. Float32 [-1.0, +1.0] internally. DMA buffers (16 x 2KB = 32KB) pre-allocated from internal SRAM at boot
- Location: `src/audio_pipeline.h/.cpp` (1139 lines)
- Contains: 16x16 routing matrix (`gain[out_ch][in_ch]`), input/output DSP bypass arrays, source/sink slot arrays (8 max each), noise gate fade state, DSP swap hold buffer
- Depends on: I2S audio HAL (`src/i2s_audio.h/.cpp`), DSP pipeline (`src/dsp_pipeline.h/.cpp`), output DSP (`src/output_dsp.h/.cpp`), AppState (bypass flags, `audioPaused`)
- Used by: FreeRTOS audio task (reads sources, applies DSP, writes sinks), HAL pipeline bridge (registers/removes sinks and sources)

**DSP Pipeline (Signal Processing, Core 1):**
- Purpose: Multi-stage audio filtering engine (biquad IIR, FIR, limiter, gain, delay, polarity, mute, compressor) with double-buffered stage config and glitch-free atomic swap
- Location: `src/dsp_pipeline.h/.cpp` (2408 lines), `src/output_dsp.h/.cpp` (878 lines per-output variant)
- Contains: Stage pool (~120 stages max), coefficient generators (RBJ Audio EQ Cookbook in `src/dsp_biquad_gen.h`), delay line PSRAM allocation with heap pre-flight check (40KB reserve)
- Depends on: ESP-DSP library (pre-built `libespressif__esp-dsp.a` for ESP32; `lib/esp_dsp_lite/` ANSI C fallback for native tests), `src/psram_alloc.h/.cpp` (PSRAM allocation wrapper)
- Used by: Audio pipeline task (inline DSP per lane/output), DSP API (`src/dsp_api.h/.cpp`)

**I2S Audio HAL:**
- Purpose: ADC/DAC driver abstraction for ESP32 I2S peripherals. Dual PCM1808 ADC on I2S_NUM_0 and I2S_NUM_1 (both masters, coordinated init). ES8311 codec on dedicated I2C bus. ESS SABRE expansion ADCs on I2C Bus 2
- Location: `src/i2s_audio.h/.cpp` (1637 lines), `src/hal/hal_i2s_bridge.h/.cpp`
- Contains: I2S config (pin overrides via HAL `_cachedAdcCfg[2]` + `_resolveI2sPin()`), dual ADC clock coordination, TDM frame parsing (4ch SABRE devices), sample rate management, FFT analysis buffers
- Depends on: HAL device manager (pin/bus config from `HalDeviceConfig`), hardware register maps (`src/drivers/*.h`)
- Used by: Audio pipeline (reads ADC frames, writes DAC frames)

**Network Layer:**
- **WiFi Manager** (`src/wifi_manager.h/.cpp`, 1556 lines): Multi-SSID client, AP mode, async connection with exponential backoff, sets `activeInterface = NET_WIFI` on connect/disconnect
- **Ethernet Manager** (`src/eth_manager.h/.cpp`): 100Mbps PHY on ESP32-P4, fallback to WiFi on unplugged, DHCP
- **MQTT** (split across 3 files):
  - `src/mqtt_handler.cpp` (1129 lines): Client lifecycle, broker config, callback dispatch (thread-safe via dirty flags)
  - `src/mqtt_publish.cpp` (951 lines): Change-detection statics (`prevMqtt*`), 20 Hz publish from dedicated Core 0 task
  - `src/mqtt_ha_discovery.cpp` (1914 lines): Home Assistant MQTT discovery protocol

**Web Interface:**
- **HTTP Server** (`src/main.cpp`, port 80): REST API endpoints registered via `registerXxxApiEndpoints()`, static asset serving (gzip-compressed), HTTP security headers via `src/http_security.h` (`X-Frame-Options: DENY`, `X-Content-Type-Options: nosniff`)
- **WebSocket Server** (`src/websocket_handler.h/.cpp`, port 81, 2623 lines): Real-time state broadcast, binary frames for waveform (`WS_BIN_WAVEFORM=0x01`) and spectrum (`WS_BIN_SPECTRUM=0x02`), token auth (16-slot pool, 60s TTL), adaptive rate limiting under heap pressure, deferred init state (spreads burst across 16 iterations)
- **Web Assets** (`web_src/` source -> auto-compiled to `src/web_pages_gz.cpp` via `tools/build_web_assets.js`): HTML shell, 7 CSS modules, 22 JS modules in concatenated scope

**Persistent Storage:**
- **Settings Manager** (`src/settings_manager.h/.cpp`, 1869 lines): JSON persistence (`/config.json`) with atomic write (tmp+rename), legacy `settings.txt` fallback on first boot only, export/import, factory reset. WiFi credentials survive LittleFS format via NVS
- **HAL Settings** (`src/hal/hal_settings.h/.cpp`): Per-device `HalDeviceConfig` persistence to `/hal_config.json`
- **Diagnostic Journal** (`src/diag_journal.h/.cpp`): Event ringbuffer (64 entries), diagnostic codes with severity (`src/diag_error_codes.h`, 100+ codes)
- **Crash Log** (`src/crash_log.h/.cpp`): Reset reason ringbuffer (`/crash_log.json`)

**GUI (Optional, Core 0):**
- Purpose: LVGL v9.4 + LovyanGFX on ST7735S 128x160 TFT (landscape 160x128). Guarded by `-D GUI_ENABLED`
- Location: `src/gui/` (22 files: manager, theme, input, navigation, 12 screen modules)
- Contains: FreeRTOS task on Core 0, ISR-driven rotary encoder (Gray code), screen stack with push/pop transitions, dark/light theme
- Screens: Desktop carousel, Home status, Control, WiFi, MQTT, Settings, Debug, Support, Boot animation, Keyboard, Value editor, Devices, DSP, Signal Generator, Menu

## Data Flow

**Boot Sequence (`src/main.cpp` setup()):**

1. CPU init, task watchdog reconfigure (30s, `idle_core_mask=0`)
2. LittleFS mount -> load settings (`/config.json`) -> apply debug log level
3. GUI init (optional, spawns `gui_task` on Core 0)
4. HAL builtin device registry (PCM5102A, ES8311, PCM1808 x2, 9 ESS SABRE ADCs, MCP4725, etc.)
5. HAL discovery: I2C bus scan -> EEPROM probe -> load saved config
6. DMA buffer pre-allocation (16 x 2KB = 32KB from internal SRAM, before WiFi connects)
7. I2S audio init (ADC2 first -> ADC1 second, both masters, uses HAL pin config)
8. Output DSP init (load presets from LittleFS)
9. HAL pipeline bridge sync (capability-based slot/lane assignment, state callback registration)
10. Peripheral device registration (NS4150B, display, encoder, buzzer, button, LED, relay, temp sensor, signal generator)
11. HTTP + WebSocket server startup, REST API endpoint registration
12. WiFi/Ethernet init (async, non-blocking)
13. MQTT task spawn on Core 0
14. Enter main loop (Core 1)

**Main Loop Data Flow (Core 1, `src/main.cpp` loop()):**

1. Wait up to 5ms on event group (`app_events_wait(5)`), wakes instantly on any dirty flag
2. Drain `HalCoordState` toggle queue: process deferred device enable/disable via `dac_activate_for_hal()` / `dac_deactivate_for_hal()`
3. WebSocket broadcast: serialize AppState changes (WiFi, audio, DSP, HAL, etc.) to JSON + binary frames for authenticated clients
4. Handle REST API requests (synchronous, via `WebServer` library callbacks)
5. Button/encoder input polling -> dispatch to handlers
6. Smart sensing update (signal detection, relay control, rate-matched to `audioUpdateRate`)
7. Periodic diagnostics: task monitor (5s), heap pressure check (30s), audio health
8. MQTT publish: handled independently by `mqtt_task` on Core 0 at 20 Hz
9. Loop back to event wait

**Audio Pipeline Data Flow (Core 1, Audio Task, `src/audio_pipeline.cpp`):**

1. I2S DMA interrupt fires (every 256 stereo frames at 48kHz ~ 5.3ms)
2. Audio task reads ADC frames via `i2s_read()` -> convert to float32 [-1.0, +1.0]
3. Check `appState.audio.paused` flag (set by DAC deinit before I2S uninstall)
4. Per-lane DSP: apply biquad stages (IIR/FIR) from double-buffered config
5. Routing matrix: sum weighted input channels -> 16 output channels via `gain[out][in]`
6. Per-output DSP: apply biquad/limiter/compressor/polarity/mute/gain
7. Sink dispatch: each of 8 sinks reads assigned output channels (slot-indexed, via `vTaskSuspendAll()`)
8. Sink processing: apply volume curve (`sink_apply_volume()`), mute ramp (`sink_apply_mute_ramp()` for click-free), convert float32 -> int32 left-justified (`sink_float_to_i2s_int32()`)
9. I2S write to all registered sinks (PCM5102A, ES8311, etc. via HAL)
10. VU metering: extract per-lane dBFS for display/WS broadcast
11. Yield 2 ticks, loop back

**State Management & Synchronization:**

- **AppState singleton** (`src/app_state.h`): 15 domain state headers composed into shell, dirty flags per domain
- **Dirty flag pattern**: Module sets flag + calls `app_events_signal(EVT_XXX)` -> main loop polls flag -> serializes to JSON/MQTT -> clears flag
- **Cross-core volatiles**: `appState.audio.paused` (bool), `appState.audio.taskPausedAck` (binary semaphore), `appState._mqttReconfigPending` (bool), `appState._pendingApToggle` (int8_t)
- **I2S handshake**: DAC deinit sets `paused=true`, waits `xSemaphoreTake(taskPausedAck, 50ms)`. Audio task gives semaphore when it observes flag and yields
- **Deferred toggle queue**: `appState.halCoord.requestDeviceToggle(halSlot, action)` enqueues enable/disable for any device type (capacity 8, same-slot dedup). Main loop drains via `hasPendingToggles()` / `pendingToggleAt(i)` / `clearPendingToggles()`

**Event Architecture** (`src/app_events.h`):

- 17 event bits assigned (bits 0-16), 7 spare (bits 17-23), bits 24-31 reserved by FreeRTOS
- `EVT_ANY = 0x00FFFFFF` — main loop wakes on any event
- Key events: `EVT_OTA`, `EVT_DISPLAY`, `EVT_BUZZER`, `EVT_SIGGEN`, `EVT_DSP_CONFIG`, `EVT_DAC`, `EVT_SETTINGS`, `EVT_HAL_DEVICE`, `EVT_CHANNEL_MAP`, `EVT_HEAP_PRESSURE`

## Key Abstractions

**AudioInputSource (`src/audio_input_source.h`):**
- Purpose: Pluggable input lane source (ADC, codec, signal generator, USB audio)
- Pattern: Slot-indexed lane array (8 max), each lane independently registered/cleared atomically via `vTaskSuspendAll()` on Core 1
- Registration: `audio_pipeline_set_source(lane, source)` returns `bool` (validates `lane*2+1 < MATRIX_SIZE`)
- Discovery: `audio_pipeline_get_source(lane)` for dynamic input enumeration

**AudioOutputSink (`src/audio_output_sink.h`):**
- Purpose: Pluggable output slot sink (DAC, USB host)
- Pattern: Slot-indexed sink array (8 max), HAL pipeline bridge (`src/hal/hal_pipeline_bridge.h/.cpp`) owns all sink lifecycle
- Registration: `audio_pipeline_set_sink(slot, sink)` returns `bool` (validates `firstChannel + channelCount <= MATRIX_SIZE`)
- Shared utilities: `src/sink_write_utils.h/.cpp` (volume, mute ramp, float->int32)

**HalDevice Base Class (`src/hal/hal_device.h`):**
- Purpose: Abstract device model with lifecycle states, init/deinit, runtime pin config, discovery metadata
- Pattern: Subclass per device type, register via `registerDevice()`, auto-init via HAL discovery or manual probe
- Lifecycle: UNKNOWN -> DETECTED -> CONFIGURING -> AVAILABLE <-> UNAVAILABLE -> ERROR / REMOVED / MANUAL
- State callback: `HalStateChangeCb` fires on every `_state` transition, registered by `hal_pipeline_bridge` at boot
- Config: `HalDeviceConfig _config` per instance (I2C/I2S/GPIO overrides persisted to `/hal_config.json`)

**HalEssSabreAdcBase (`src/hal/hal_ess_sabre_adc_base.h/.cpp`):**
- Purpose: Abstract base class for all 9 ESS SABRE ADC expansion drivers
- Contains: Shared I2C helpers (`_writeReg`, `_readReg`, `_writeReg16`), Wire selection, config override reading
- 2ch I2S subclasses: ES9822PRO, ES9826, ES9823PRO/MPRO, ES9821, ES9820
- 4ch TDM subclasses: ES9843PRO, ES9842PRO, ES9841, ES9840 (use `HalTdmDeinterleaver`)

**DspStage (`src/dsp_pipeline.h`):**
- Purpose: Single audio processing step (biquad, gain, delay, limiter, compressor, etc.)
- Pattern: Stage pool (~120 capacity), double-buffered active + pending config for glitch-free swap
- Access: `dsp_add_stage()` (rolls back on pool exhaustion), `dsp_swap_config()`, overflow telemetry

**HalCoordState (`src/state/hal_coord_state.h/.cpp`):**
- Purpose: Fixed-capacity deferred device toggle queue (capacity 8, same-slot dedup)
- API: `requestDeviceToggle(halSlot, action)` returns bool (false on overflow), `hasPendingToggles()`, `pendingToggleAt(i)`, `clearPendingToggles()`
- Telemetry: `_overflowCount` (lifetime) + `consumeOverflowFlag()` (one-shot, triggers `DIAG_HAL_TOGGLE_OVERFLOW`)

## Entry Points

**Firmware Setup (`src/main.cpp` setup()):**
- Location: `src/main.cpp`
- Triggers: Hardware reset, OTA update completion
- Responsibilities: Initialize all subsystems in sequence (LittleFS -> HAL -> DMA -> audio -> WiFi -> servers), spawn tasks, register REST API endpoints

**Main Loop (`src/main.cpp` loop()):**
- Location: `src/main.cpp`
- Triggers: Continuous on Core 1, wakes via `app_events_wait(5)`
- Responsibilities: Drain toggle queue, broadcast state changes via WebSocket, dispatch button/encoder input, update smart sensing, log diagnostics

**Audio Pipeline Task (`src/audio_pipeline.cpp`):**
- Spawned by `audio_pipeline_init()`, runs on Core 1 priority 3
- Triggers: I2S DMA interrupt every ~5.3ms (256 frames at 48kHz)
- Responsibilities: Read ADC frames, apply DSP chains, mix via matrix, write to sinks, compute VU meters

**MQTT Task (`src/mqtt_task.cpp`):**
- Spawned at boot, runs on Core 0 priority 2
- Triggers: 20 Hz poll timer, `_mqttReconfigPending` flag
- Responsibilities: Maintain broker connection, periodic publish of sensor state, respond to HA commands

**GUI Task (`src/gui/gui_manager.cpp`):**
- Spawned at boot (if `GUI_ENABLED`), runs on Core 0
- Triggers: LVGL tick timer, encoder ISR events
- Responsibilities: Render TFT display, handle rotary encoder input, screen navigation

**REST API Endpoints (registered in `src/main.cpp` via `registerXxxApiEndpoints()`):**
- HTTP server port 80, all guarded by auth and `appState.halSafeMode`
- Registration functions: `registerHalApiEndpoints()` (`src/hal/hal_api.cpp`), `registerDspApiEndpoints()` (`src/dsp_api.cpp`), `registerPipelineApiEndpoints()` (`src/pipeline_api.cpp`), `registerDacApiEndpoints()` (`src/dac_api.cpp`), `registerPsramApiEndpoints()` (`src/psram_api.cpp`)
- Key routes: `/api/hal/devices`, `/api/hal/scan`, `/api/audio/matrix`, `/api/dsp/config`, `/api/dac/*`, `/api/psram/status`, `/api/networks`, `/api/mqtt/settings`, `/api/settings`, `/api/ota/*`, `/api/ws-token`, `/api/diag/snapshot`

**WebSocket Handler (`src/websocket_handler.h/.cpp`):**
- Port 81, token auth required (from `GET /api/ws-token`, 60s TTL, 16-slot pool)
- JSON broadcasts (state sync), binary frames (waveform 0x01, spectrum 0x02)
- Per-client subscription: `_audioSubscribed[clientId]` gates high-frequency audio data
- Adaptive rate: halved under heap warning, suppressed under heap critical

## Error Handling

**Strategy:** Defensive nullability, graduated feature degradation, diagnostic logging with severity levels, no exceptions (embedded constraint). Return-value checking enforced at all callsites.

**Patterns:**

- **HAL device registration overflow**: `registerDevice()` returns -1 on capacity exhaustion (24/24 slots), caller logs warning, emits `DIAG_HAL_REGISTRY_FULL` (0x100F). Driver registry: `DIAG_HAL_REGISTRY_FULL` at 32 drivers; DB: `DIAG_HAL_DB_FULL` at 32 entries
- **Pipeline slot validation**: `set_source()` validates `lane*2+1 < MATRIX_SIZE`; `set_sink()` validates `firstChannel + channelCount <= MATRIX_SIZE`. Both return `bool` (false on validation/allocation failure). Compile-time `static_assert` enforces `MAX_INPUTS*2 <= MATRIX_SIZE` and `MAX_SINKS*2 <= MATRIX_SIZE`
- **DSP swap failure**: Pool exhausted during `dsp_add_stage()` -> rolls back, returns error. `dsp_swap_config()` callers check return value, respond HTTP 503 or log warning
- **I2S driver handshake**: DAC deinit sets `paused=true`, waits `xSemaphoreTake(taskPausedAck, 50ms)`. Audio task gives semaphore when safe. 50ms timeout prevents deadlock
- **Graduated heap pressure** (3 states, checked every 30s): Normal -> Warning @ 50KB free (WS binary rate halved, DSP stages warned) -> Critical @ 40KB free (DMA alloc refused, WS binary suppressed, DSP stages rejected, OTA skipped)
- **Graduated PSRAM pressure** (3 states): Normal -> Warning @ 1MB free -> Critical @ 512KB free (DSP delay/convolution refused)
- **WiFi SDIO conflict**: I2C Bus 0 (GPIO 48/54) skipped during HAL discovery when WiFi active via `hal_wifi_sdio_active()`, emits `DIAG_HAL_I2C_BUS_CONFLICT` (0x1101), scan API returns `partialScan` flag
- **Boot loop detection**: 3 consecutive crash boots -> `appState.halSafeMode=true` -> skip HAL init, WiFi+web only, emit `DIAG_SYS_BOOT_LOOP` (0x0101)
- **Toggle queue overflow**: `requestDeviceToggle()` returns false on capacity (8) exhaustion, all 6 callers check return value and `LOG_W`. REST endpoints return HTTP 503. Overflow emits `DIAG_HAL_TOGGLE_OVERFLOW`
- **DMA allocation failure**: `DIAG_AUDIO_DMA_ALLOC_FAIL` (0x200E) emitted, `AudioState.dmaAllocFailed` + bitmask track affected lanes/slots

## Cross-Cutting Concerns

**Logging:** Centralized `src/debug_serial.h` macros (`LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`) with module prefixes (`[ModuleName]`). Runtime level control via `applyDebugSerialLevel()`. WebSocket log forwarding via `broadcastLine()` sends separate `"module"` JSON field for frontend category filtering. Never log in ISR or audio task — blocks UART TX, starves DMA. Use dirty-flag pattern instead.

**Validation:** Input bounds checking on matrix dimensions (compile-time `static_assert` + runtime guards), GPIO number validation (rejects GPIO>54 via `HAL_GPIO_MAX`), I2C address range validation (0x08-0x77), pipeline slot capacity checks (8 max each).

**Authentication:** Web password PBKDF2-SHA256 (10k iterations) in `src/auth_handler.h/.cpp`. HttpOnly cookie. WS token pool (16 slots, 60s TTL) from `GET /api/ws-token`. Rate limiting on failed auth (HTTP 429). HTTP security headers (`X-Frame-Options: DENY`, `X-Content-Type-Options: nosniff`) via `src/http_security.h`.

**Synchronization:** `vTaskSuspendAll()` for atomic audio pipeline slot updates on Core 1. Binary semaphore for I2S driver handshake. Volatile flags for cross-core coordination (`_mqttReconfigPending`, `_pendingApToggle`, `audio.paused`). No locks in audio task (real-time constraint). FreeRTOS mutex for buzzer dual-core safety.

**Memory Management:** PSRAM-preferred allocations via unified `src/psram_alloc.h/.cpp` wrapper (auto fallback to SRAM with `heap_budget` recording and `DIAG_SYS_PSRAM_ALLOC_FAIL` emission). DMA buffers from internal SRAM only (`MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`). Per-subsystem tracking via `src/heap_budget.h/.cpp` (32 entries). Exposed via WS `hardwareStats` and REST `/api/psram/status`.

**Diagnostics:** Event-driven journal (`src/diag_journal.h/.cpp`, 64-entry ringbuffer), 100+ error codes (`src/diag_error_codes.h`), severity levels, event emission macro (`src/diag_event.h`). Health dashboard on web UI (`web_src/js/27a-health-dashboard.js`). Task monitor (`src/task_monitor.h/.cpp`) enumerates FreeRTOS tasks every 5s.

---

*Architecture analysis: 2026-03-22*
