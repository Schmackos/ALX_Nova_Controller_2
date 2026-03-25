# Architecture

**Analysis Date:** 2026-03-25

## System Overview

ESP32-P4 audio platform firmware using PlatformIO + Arduino framework (ESP-IDF 5 based). The system is a layered, event-driven architecture centered on a dual-core audio pipeline with a Hardware Abstraction Layer (HAL) managing device lifecycles. The main loop on Core 0 handles networking, UI, settings, and orchestration. Core 1 is exclusively reserved for real-time audio processing.

**Architecture Style:** Layered event-driven singleton with domain-specific state composition, deferred command queueing, and capability-based hardware abstraction.

**Key Characteristics:**
- Singleton `AppState` composed from 15 domain-specific state headers, accessed via `appState.domain.field`
- HAL device manager with 3-tier discovery (I2C scan, EEPROM probe, manual config), lifecycle state machine, and topological dependency-sorted initialization
- FreeRTOS event group (24 usable bits, 17 assigned) for zero-latency dirty-flag wake
- Lock-free atomic sentinel pattern for cross-core sink/source registration (replaced `vTaskSuspendAll`)
- Polyphase ASRC engine for per-lane sample rate conversion with DSD/DoP bypass
- Device dependency graph with Kahn's BFS topological sort in `initAll()`
- Clock quality diagnostics via virtual `getClockStatus()` on DPLL-capable devices

## Component Diagram

```
                          ┌─────────────────────────────────────┐
                          │          Web Browser / MQTT          │
                          └─────────────┬───────────────────────┘
                                        │ HTTP:80 / WS:81 / MQTT
                          ┌─────────────▼───────────────────────┐
                          │     Integration Layer (Core 0)       │
                          │  REST API · WebSocket · MQTT · Auth  │
                          └──────┬──────────────┬───────────────┘
                                 │              │
               ┌─────────────────▼──┐    ┌──────▼──────────────────┐
               │  State Management  │    │   HAL Device Layer       │
               │  AppState singleton│◄───│  Discovery · Lifecycle   │
               │  15 domain headers │    │  32-slot device manager  │
               │  Dirty flags+events│    │  Dependency graph        │
               └─────────┬─────────┘    │  Clock diagnostics       │
                         │              │  Power management         │
                         │              └──────┬───────────────────┘
                         │                     │ sink/source registration
               ┌─────────▼─────────────────────▼───────────────────┐
               │          Audio Processing Layer (Core 1)           │
               │  8-lane input → ASRC → per-input DSP → 32×32      │
               │  matrix → per-output DSP → 16-slot sink dispatch   │
               │  DoP DSD detection · Format negotiation            │
               └───────────────────────────────────────────────────┘
                         │                     │
               ┌─────────▼─────────┐  ┌───────▼───────────────────┐
               │  I2S Port Layer   │  │  DSP Engine               │
               │  3 ports (STD/TDM)│  │  4ch biquad/FIR/limiter   │
               │  TX/RX any pins   │  │  + 8ch output DSP         │
               └───────────────────┘  └───────────────────────────┘
```

## Layers

**HAL Device Layer:**
- Purpose: Hardware abstraction, device discovery, lifecycle management, dependency resolution, clock diagnostics, power management
- Location: `src/hal/` (86 files)
- Contains: `HalDevice` base class (`src/hal/hal_device.h`), `HalDeviceManager` singleton (`src/hal/hal_device_manager.h/.cpp`), discovery engine (`src/hal/hal_discovery.h/.cpp`), driver registry (`src/hal/hal_driver_registry.h/.cpp`), pipeline bridge (`src/hal/hal_pipeline_bridge.h/.cpp`), builtin devices (`src/hal/hal_builtin_devices.h/.cpp`), I2C bus abstraction (`src/hal/hal_i2c_bus.h/.cpp`), device database (`src/hal/hal_device_db.h/.cpp`), custom devices (`src/hal/hal_custom_device.h/.cpp`), REST API (`src/hal/hal_api.h/.cpp`), EEPROM API (`src/hal/hal_eeprom_api.h/.cpp`), settings persistence (`src/hal/hal_settings.h/.cpp`)
- Driver families: ESS SABRE DAC base (`src/hal/hal_ess_sabre_dac_base.h/.cpp`), ESS SABRE ADC base (`src/hal/hal_ess_sabre_adc_base.h/.cpp`), Cirrus DAC base (`src/hal/hal_cirrus_dac_base.h/.cpp`)
- Generic driver patterns: Pattern A `hal_ess_adc_2ch`, Pattern B `hal_ess_adc_4ch`, Pattern C `hal_ess_dac_2ch`/`hal_cirrus_dac_2ch`, Pattern D `hal_ess_dac_8ch`
- Peripheral drivers: `hal_ns4150b.cpp` (amp), `hal_pcm1808.cpp` (ADC), `hal_pcm5102a.cpp` (DAC), `hal_es8311.cpp` (codec), `hal_mcp4725.cpp` (DAC trim), `hal_temp_sensor.cpp`, `hal_display.cpp`, `hal_encoder.cpp`, `hal_buzzer.cpp`, `hal_led.cpp`, `hal_relay.cpp`, `hal_button.cpp`, `hal_siggen.cpp`, `hal_signal_gen.cpp`, `hal_usb_audio.cpp`
- TDM support: `hal_tdm_deinterleaver.h/.cpp`, `hal_tdm_interleaver.h/.cpp`
- Depends on: I2C bus backend, I2S port API, EEPROM v3
- Used by: Audio pipeline bridge, smart sensing, HAL API handlers, health check

**Audio Processing Layer (Core 1):**
- Purpose: Real-time audio I/O, ASRC, DSP, matrix routing, VU metering, DoP DSD detection
- Location: `src/audio_pipeline.h/.cpp` (pipeline core), `src/asrc.h/.cpp` (polyphase ASRC), `src/audio_input_source.h` (input interface), `src/audio_output_sink.h` (output interface), `src/i2s_audio.h/.cpp` (I2S driver), `src/i2s_port_api.cpp` (REST)
- Contains: 8-lane input with per-lane ASRC (160 phases x 32 taps polyphase FIR), per-input DSP (4ch biquad/FIR/limiter/gain/delay/compressor), 32x32 matrix mixer, per-output DSP (8ch post-matrix), 16-slot sink dispatch. Float32 internal processing. DoP DSD detection with 3-buffer confirm/clear hysteresis. Format negotiation via `audio_pipeline_check_format()`. Lock-free atomic sentinel pattern for cross-core sink/source operations
- Depends on: HAL sinks/sources (via pipeline bridge), DSP engine, ESP-DSP library, PSRAM allocator
- Used by: HAL pipeline bridge, smart sensing, WebSocket audio streaming

**DSP Engine:**
- Purpose: Multi-stage signal processing (biquad IIR, FIR convolution, limiter, compressor, delay, gain, crossover)
- Location: `src/dsp_pipeline.h/.cpp` (4ch pre-matrix DSP), `src/output_dsp.h/.cpp` (8ch post-matrix per-output DSP), `src/dsp_biquad_gen.c/.h` (RBJ EQ Cookbook coefficient generator), `src/dsp_coefficients.cpp/.h`, `src/dsp_convolution.cpp/.h`, `src/dsp_crossover.cpp/.h`, `src/dsp_rew_parser.cpp/.h`
- Contains: 24-stage per-channel processing (10 PEQ + 14 chain), preset management (32 slots), CPU load monitoring with auto-bypass, REW import parser
- Depends on: ESP-DSP pre-built library (`libespressif__esp-dsp.a`), PSRAM for FIR/delay buffers
- Used by: Audio pipeline (called in pipeline_run_dsp and pipeline_write_output stages)

**State Management Layer:**
- Purpose: Centralized decomposed domain-specific state with dirty-flag change detection
- Location: `src/state/` (17 files) composed into `src/app_state.h` singleton
- Contains: `WifiState`, `MqttState`, `AudioState` (incl. format negotiation, ASRC, DSD flags), `DspSettingsState`, `DisplayState`, `BuzzerState`, `SignalGenState`, `DebugState`, `GeneralState`, `OtaState`, `EthernetState`, `DacState`, `UsbAudioState`, `HalCoordState` (deferred toggle queue with spinlock), `HealthCheckState`, shared `enums.h`
- Access pattern: `appState.domain.field` via `#define appState AppState::getInstance()`
- Depends on: Nothing (pure state headers, no logic dependencies)
- Used by: All modules; WebSocket broadcasts serialize state to JSON; MQTT publish for HA discovery

**Event Coordination Layer:**
- Purpose: Cross-task signaling, event-driven wake, deferred command queueing
- Location: `src/app_events.h/.cpp` (FreeRTOS event group), `src/state/hal_coord_state.h/.cpp` (8-slot toggle queue)
- Contains: 17 event bits assigned (bits 0-18, with 5+13 freed), 7 spare (19-23). `EVT_FORMAT_CHANGE` (bit 18) signals sample rate mismatch or DSD detection. HAL toggle queue with overflow telemetry and portMUX spinlock protection
- Depends on: FreeRTOS event group API
- Used by: All dirty-flag setters trigger `app_events_signal(EVT_XXX)`, main loop calls `app_events_wait(5)`, MQTT task, GUI task

**Integration Layer (REST + WebSocket + MQTT):**
- Purpose: External API surface, real-time state broadcasting, command dispatch, authentication
- Location: `src/main.cpp` (route registration), `src/websocket_command.cpp` (WS command handler), `src/websocket_broadcast.cpp` (state broadcasts), `src/websocket_auth.cpp` (WS token auth), `src/websocket_cpu_monitor.cpp`, `src/mqtt_handler.cpp` (MQTT connection/subscribe), `src/mqtt_publish.cpp` (MQTT publish), `src/mqtt_ha_discovery.cpp` (Home Assistant auto-discovery), `src/mqtt_task.cpp` (Core 0, 20Hz), `src/auth_handler.cpp` (PBKDF2-SHA256)
- API modules: `src/hal/hal_api.cpp` (16 HAL endpoints), `src/dsp_api.cpp` (DSP endpoints), `src/pipeline_api.cpp` (pipeline endpoints), `src/diag_api.cpp`, `src/siggen_api.cpp`, `src/i2s_port_api.cpp`, `src/psram_api.cpp`, `src/health_check_api.cpp`
- Contains: REST versioning (all endpoints at both `/api/<path>` and `/api/v1/<path>` via `server_on_versioned()`), WebSocket port 81 (token auth, 16 clients max, binary VU/waveform/spectrum), MQTT with TLS support, Home Assistant auto-discovery
- Security: `src/http_security.h` (`server_send()` wrapper with X-Frame-Options/X-Content-Type-Options, `sanitize_filename()`, `json_response()` envelope), `src/auth_handler.cpp` (PBKDF2-SHA256, rate limiting), `src/websocket_auth.cpp` (token-based WS auth)
- Depends on: AppState, WebServer (port 80), WebSocketsServer (port 81), PubSubClient, ArduinoJson
- Used by: Web UI, Home Assistant, any HTTP/MQTT client

**Web Frontend:**
- Purpose: Single-page web UI served from flash (gzipped), communicates via WebSocket + REST
- Location: `web_src/` (source), `src/web_pages.cpp`/`src/web_pages_gz.cpp` (generated, DO NOT EDIT)
- Contains: 25 JS files (`web_src/js/01-core.js` through `29-i2s-ports.js`), CSS in `web_src/css/`, HTML template `web_src/index.html`. All icons inline SVG from Material Design Icons
- Build: `node tools/build_web_assets.js` concatenates JS/CSS into HTML, then generates C++ byte arrays
- Key JS modules: `01-core.js` (apiFetch with auto `/api/` to `/api/v1/` rewrite), `02-ws-router.js` (WS message dispatch), `03-app-state.js` (client state), `15-hal-devices.js` (HAL device UI, dependency badges, clock status)
- Depends on: WebSocket on port 81, REST API on port 80

**GUI Layer (TFT):**
- Purpose: Physical display interface on ST7735S 128x160 TFT via LovyanGFX + LVGL v9.4
- Location: `src/gui/` (config, theme, navigation, input), `src/gui/screens/` (15 screen implementations)
- Contains: `gui_manager.cpp/.h` (task management), `gui_navigation.cpp/.h`, `gui_theme.cpp/.h`, `gui_input.cpp/.h`, `lgfx_config.h` (LovyanGFX ST7735S setup), `lv_conf.h`
- Screens: `scr_home`, `scr_desktop`, `scr_control`, `scr_devices`, `scr_dsp`, `scr_siggen`, `scr_wifi`, `scr_mqtt`, `scr_settings`, `scr_debug`, `scr_support`, `scr_boot_anim`, `scr_menu`, `scr_keyboard`, `scr_value_edit`
- Guarded by: `-D GUI_ENABLED` build flag
- Runs on: Core 0 via `gui_task`

**Diagnostics Layer:**
- Purpose: Health monitoring, diagnostic event journal, error code taxonomy, crash log
- Location: `src/health_check.h/.cpp` (10 categories), `src/health_check_api.cpp`, `src/diag_journal.h/.cpp` (ring buffer), `src/diag_event.h` (event types), `src/diag_error_codes.h` (error taxonomy), `src/diag_api.cpp` (REST), `src/crash_log.h/.cpp` (boot crash ring), `src/hal/hal_audio_health_bridge.h/.cpp` (HAL-audio health integration)
- Contains: 2-phase health check (immediate at boot: heap/storage/DMA; deferred after 30s: I2C/HAL/I2S/network/MQTT/tasks/audio/clock), 32-entry in-memory hot ring + 800-entry persistent LittleFS journal, 5-second staleness guard, serial trigger
- Health categories: heap, storage, DMA, I2C buses, HAL devices, I2S ports, network, MQTT, tasks, audio, clock
- Depends on: AppState, HAL device manager (for device health), LittleFS (for persistent journal)

**Network Layer:**
- Purpose: WiFi, Ethernet, OTA updates
- Location: `src/wifi_manager.cpp/.h`, `src/eth_manager.cpp/.h`, `src/ota_updater.cpp/.h`, `src/ota_certs.h`
- Contains: Multi-network WiFi with AP mode (captive portal), Ethernet with static/DHCP config and config revert timer, OTA with SHA256 verification and backoff-aware retry, Mozilla certificate bundle for TLS
- Depends on: WiFi, HTTPClient, WiFiClientSecure, LittleFS

## Data Flow

**Audio Signal Path (Real-Time, Core 1):**

1. `pipeline_read_inputs()` — Read I2S data from up to 8 input sources (PCM1808 ADCs, USB Audio, Signal Generator) into per-lane int32 buffers
2. `pipeline_to_float()` — Convert int32 to float32, apply input gain, compute VU meters
3. DoP DSD detection — Check for alternating 0x05/0xFA markers in top byte; set `isDsd` flag with 3-buffer hysteresis
4. `pipeline_resample_inputs()` — Run polyphase ASRC on lanes with sample rate mismatch (skip DSD lanes)
5. `pipeline_run_dsp()` — Apply per-input DSP stages (biquad, FIR, limiter, compressor, delay) via `dsp_pipeline.cpp` (skip DSD lanes)
6. `pipeline_mix_matrix()` — 32x32 matrix mixer routes any input channel to any output channel with per-crosspoint gain
7. `pipeline_write_output()` — Apply per-output DSP, convert float32 back to int32, dispatch to up to 16 output sinks (PCM5102A, ES8311, expansion DACs)

**Device Lifecycle (Core 0, main loop):**

1. `hal_register_builtins()` — Register 14 onboard device instances with driver registry
2. `hal_db_init()` — Load device database (44 entries, 26 expansion devices)
3. `hal_load_device_configs()` — Load persisted per-device JSON configs from LittleFS
4. `hal_load_custom_devices()` — Instantiate user-defined tier 1-3 custom devices
5. `HalDeviceManager::initAll()` — Topological sort (Kahn's BFS) by dependency graph, then priority-sorted `probe()` + `init()` for each device
6. `hal_wire_builtin_dependencies()` — Wire static inter-device dependencies (e.g., DAC depends on I2S master clock from ADC)
7. `hal_pipeline_sync()` — Register state change callback, scan AVAILABLE devices, wire audio sinks/sources

**State Change Broadcast (Core 0, main loop):**

1. Any module sets `appState.markXxxDirty()` which sets dirty flag + calls `app_events_signal(EVT_XXX)`
2. Main loop wakes from `app_events_wait(5)` with <1us latency
3. Main loop checks each dirty flag: `if (appState.isXxxDirty()) { sendXxxState(); appState.clearXxxDirty(); }`
4. `sendXxxState()` serializes state to JSON, broadcasts to all authenticated WebSocket clients
5. MQTT publish runs on separate task (Core 0, 20Hz) reading same dirty flags

**HAL Device Toggle (Cross-Context):**

1. WebSocket command or REST API handler calls `appState.halCoord.requestDeviceToggle(halSlot, action)` (spinlock-protected)
2. Main loop detects `hasPendingToggles()`, drains all entries per tick
3. For enable: calls `hal_pipeline_activate_device(halSlot)` which runs `buildSink()` + `audio_pipeline_set_sink()`
4. For disable: calls `hal_pipeline_deactivate_device(halSlot)` which takes audio pause semaphore, removes sink, deinits device

**Format Negotiation:**

1. `audio_pipeline_check_format()` called from main loop, compares source sample rates against sink rates
2. On mismatch: sets `appState.audio.rateMismatch`, emits `DIAG_AUDIO_RATE_MISMATCH`, signals `EVT_FORMAT_CHANGE`
3. Main loop calls `audio_pipeline_set_lane_src(lane, srcRate, dstRate)` to configure ASRC for mismatched lanes
4. ASRC engine (`asrc.cpp`) activates polyphase resampling; passthrough when `srcRate == dstRate`
5. DSD lanes are always bypassed by ASRC (DoP bitstream must not be filtered)

## Key Design Patterns

**Atomic Sentinel Pattern (Lock-Free Cross-Core):**
- Used in: `audio_pipeline_set_sink()`, `audio_pipeline_set_source()`, `audio_pipeline_remove_sink()`, `audio_pipeline_remove_source()` in `src/audio_pipeline.cpp`
- Pattern: Null the sentinel field (function pointer: `write` or `read`) with `__ATOMIC_RELEASE`, copy body fields, then store non-null sentinel with `__ATOMIC_RELEASE`. Audio task on Core 1 checks sentinel with `__ATOMIC_ACQUIRE` before accessing other fields
- Purpose: Replaced `vTaskSuspendAll()` for zero-latency cross-core sink/source registration
- Rule: Never use raw `_ready =` on HalDevice; always use `setReady()` with `__ATOMIC_RELEASE`/`__ATOMIC_ACQUIRE`

**Dirty Flag + Event Group Wake:**
- Used in: All AppState dirty-flag setters (17 flags, 17 event bits)
- Pattern: `markXxxDirty()` sets bool flag + calls `app_events_signal(EVT_XXX)`. Main loop calls `app_events_wait(5)` which returns immediately on any event or after 5ms timeout. Loop checks each dirty flag and clears after processing
- Purpose: Zero-latency wake on state changes while avoiding busy polling

**Deferred Command Queue:**
- Used in: `src/state/hal_coord_state.h` (`HalCoordState`)
- Pattern: Thread-safe spinlock-guarded queue (capacity 8) with same-slot deduplication. HTTP/WS handlers enqueue, main loop drains all entries per tick. Overflow counter + one-shot flag for diagnostic emission
- Purpose: Safe cross-context device lifecycle operations (HTTP handler -> main loop execution)

**HAL Device Lifecycle State Machine:**
- States: `UNKNOWN` -> `DETECTED` -> `CONFIGURING` -> `AVAILABLE` <-> `UNAVAILABLE` -> `ERROR` / `REMOVED` / `DISABLED`
- Self-healing: 3 retry attempts with timestamp-based non-blocking backoff before permanent `ERROR`
- NVS fault persistence: Fault counters survive reboots, cleared on explicit field service
- State change callback: `hal_pipeline_bridge` registers callback at boot for automatic sink/source management

**Topological Sort with Priority Tiebreaker:**
- Used in: `HalDeviceManager::initAll()` in `src/hal/hal_device_manager.cpp`
- Pattern: Kahn's BFS using `_dependsOn` bitmask per device. Devices with zero in-degree are sorted by `initPriority` (descending). When a device completes init, its dependents' in-degree is decremented
- Purpose: Ensure I2S clock master initializes before clock-dependent DACs, relay after codec, etc.

**Generic Driver Pattern (Template Method):**
- 4 base classes cover 26 expansion devices:
  - `HalEssAdc2ch` (`src/hal/hal_ess_adc_2ch.h/.cpp`) — 5 ESS 2ch ADC models
  - `HalEssAdc4ch` (`src/hal/hal_ess_adc_4ch.h/.cpp`) — 4 ESS 4ch ADC models (TDM)
  - `HalEssDac2ch` (`src/hal/hal_ess_dac_2ch.h/.cpp`) — 8 ESS 2ch DAC models
  - `HalCirrusDac2ch` (`src/hal/hal_cirrus_dac_2ch.h/.cpp`) — 5 Cirrus Logic 2ch DAC models
  - `HalEssDac8ch` (`src/hal/hal_ess_dac_8ch.h/.cpp`) — 4 ESS 8ch DAC models
- Shared base classes: `HalEssSabreDacBase` (`src/hal/hal_ess_sabre_dac_base.h/.cpp`), `HalEssSabreAdcBase` (`src/hal/hal_ess_sabre_adc_base.h/.cpp`), `HalCirrusDacBase` (`src/hal/hal_cirrus_dac_base.h/.cpp`)

**Polyphase ASRC Engine:**
- Used in: `src/asrc.h/.cpp`, called from `pipeline_resample_inputs()` in `src/audio_pipeline.cpp`
- Pattern: 160 phases x 32 taps = 5120 coefficients in flash. Per-lane state (fractional phase accumulator + history ring buffer) in PSRAM via `psram_alloc()`. Passthrough when `srcRate == dstRate` (zero overhead)
- Supported ratios: 44.1k->48k, 48k->44.1k, 88.2k->48k, 96k->48k, 176.4k->48k, 192k->48k
- Memory: ~1.5KB PSRAM per active lane

## State Management

**AppState Singleton** (`src/app_state.h`):
- Access: `appState.domain.field` via `#define appState AppState::getInstance()`
- 15 domain-specific state structs composed into singleton:
  - `wifi` (`WifiState`) — SSID, password, AP mode, connection state
  - `general` (`GeneralState`) — device name, serial number
  - `ethernet` (`EthernetState`) — link state, IP config
  - `ota` (`OtaState`) — update state, auto-update, countdown
  - `audio` (`AudioState`) — ADC state, VU meters, sample rate, format negotiation, ASRC status, DSD flags
  - `debug` (`DebugState`) — log level, heap stats
  - `halCoord` (`HalCoordState`) — deferred device toggle queue
  - `healthCheck` (`HealthCheckState`) — last check timestamp, overall status
  - `mqtt` (`MqttState`) — connection state, broker config
  - `display` (`DisplayState`) — backlight, timeout, brightness
  - `buzzer` (`BuzzerState`) — enabled, volume
  - `sigGen` (`SignalGenState`) — frequency, waveform, amplitude
  - `dsp` (`DspSettingsState`) — enabled, bypass, preset, CPU metrics (guarded by `DSP_ENABLED`)
  - `usbAudio` (`UsbAudioState`) — connection, format, VU (guarded by `USB_AUDIO_ENABLED`)
  - `dac` (`DacState`) — EEPROM state (guarded by `DAC_ENABLED`)

**Dirty Flag Convention:**
- Every stateful domain has a dirty flag pattern: `markXxxDirty()`, `isXxxDirty()`, `clearXxxDirty()`
- `markXxxDirty()` always also calls `app_events_signal(EVT_XXX)` for immediate wake
- Main loop checks + clears dirty flags sequentially; broadcasts state to WS/MQTT clients

## Concurrency Model

**Core 1 (Audio — NEVER add non-audio tasks):**
- `loopTask` (Arduino main loop, priority 1) — runs `loop()` function
- `audio_pipeline_task` (priority 3, higher than loopTask) — real-time audio I/O and processing

**Core 0 (Everything else):**
- `gui_task` — LVGL + LovyanGFX display updates (guarded by `GUI_ENABLED`)
- `mqtt_task` (priority 2, 20Hz) — MQTT publish/subscribe
- `usb_audio_task` — TinyUSB UAC2 device (guarded by `USB_AUDIO_ENABLED`)
- OTA tasks — background firmware download/install

**Cross-Core Safety Rules:**
1. Never log (Serial.print) in ISR or audio task — use dirty-flag pattern
2. Use `audio_pipeline_request_pause(timeout_ms)` / `audio_pipeline_resume()` for I2S driver teardown — never set `appState.audio.paused` directly
3. Use `setReady(bool)` / `isReady()` with `__ATOMIC_RELEASE`/`__ATOMIC_ACQUIRE` for HalDevice ready state — never raw `_ready =`
4. Sink/source registration uses atomic sentinel pattern — no scheduler suspend needed
5. `HalCoordState` toggle queue uses portMUX spinlock for HTTP handler -> main loop queueing
6. MCLK must remain continuous — never call `i2s_configure_adc1()` from audio task loop

**I2S Driver Safety Protocol:**
```
audio_pipeline_request_pause(50)   // Blocks until audio task acknowledges (binary semaphore)
// ... teardown/reinstall I2S driver ...
audio_pipeline_resume()            // Clears paused flag, audio task resumes
```

## Error Handling Strategy

**HAL Self-Healing:**
- `healthCheckAll()` runs every 30s on Core 0
- Failed health check -> `HAL_STATE_UNAVAILABLE`, `_ready=false`
- Non-blocking timestamp-based retry with exponential backoff (up to 3 retries)
- 3 consecutive failures -> `HAL_STATE_ERROR`, persistent NVS fault counter incremented
- User can disable/re-enable via REST/WS to reset retry state

**Boot Loop Protection:**
- `src/crash_log.h/.cpp` — Ring buffer of recent reset reasons in LittleFS
- 3 consecutive crash boots -> `appState.halSafeMode = true` -> skip HAL init, WiFi+web only
- Emits `DIAG_SYS_BOOT_LOOP` critical diagnostic

**Diagnostic Journal:**
- `src/diag_journal.h/.cpp` — 32-entry in-memory hot ring (PSRAM) + 800-entry persistent ring (LittleFS)
- Severity levels: INFO, WARN, ERROR, CRIT
- Flushed to LittleFS every 60s (WARN+ entries)
- Accessible via REST `/api/diag/events` and WS broadcast

**Health Check System:**
- `src/health_check.h/.cpp` — 10 categories, 32 max check items
- 2-phase execution: immediate (heap/storage/DMA) at boot, deferred (I2C/HAL/I2S/network/MQTT/tasks/audio/clock) after 30s
- 5-second staleness guard prevents redundant runs
- Results: PASS, WARN, FAIL, SKIP per item
- Clock health: queries `getClockStatus()` on devices with `HAL_CAP_DPLL`

**Heap Safety:**
- Warning at 50KB free internal SRAM, critical at 40KB
- PSRAM warning at 1MB, critical at 512KB
- WiFi RX needs ~40KB free internal SRAM — below this, HTTP/WS packets silently dropped
- All PSRAM allocations via `psram_alloc()` wrapper (PSRAM-first, SRAM fallback capped at 64KB)

## Security Architecture

**Authentication:**
- `src/auth_handler.cpp` — PBKDF2-SHA256 password hashing (50,000 iterations)
- Session-based HTTP auth with Cookie or X-Session-ID header
- Rate limiting on login attempts
- WebSocket token auth (`/api/ws-token` -> token -> WS auth message)

**HTTP Security:**
- `src/http_security.h` — `server_send()` wrapper injects X-Frame-Options: DENY and X-Content-Type-Options: nosniff on all responses
- `sanitize_filename()` — Path traversal prevention for user-provided filenames
- `json_response()` — Structured JSON envelope with character escaping

**API Versioning:**
- All endpoints registered at both `/api/<path>` and `/api/v1/<path>` via `server_on_versioned()` macro in `src/main.cpp`
- Frontend `apiFetch()` in `web_src/js/01-core.js` auto-rewrites `/api/` to `/api/v1/`

## Entry Points

**Firmware Entry:**
- `src/main.cpp` — `setup()` and `loop()` (Arduino entry points)
- `setup()` initializes all subsystems in dependency order: Serial -> WDT -> LittleFS -> Crash Log -> Settings -> USB Audio -> GUI -> DSP -> HAL -> I2S -> Pipeline -> Network -> Auth -> Routes -> WiFi -> MQTT
- `loop()` is the event-driven main loop: `app_events_wait(5)` -> health check -> server.handleClient -> webSocket.loop -> dirty flag drain -> deferred saves -> audio broadcast

**Audio Task Entry:**
- `src/audio_pipeline.cpp` — `audio_pipeline_task()` pinned to Core 1, priority 3
- Continuous loop: read inputs -> float conversion -> DoP detection -> ASRC -> DSP -> matrix -> output DSP -> sink dispatch

**REST API Entry:**
- Route registration in `src/main.cpp` lines ~425-860
- HAL API: `src/hal/hal_api.cpp` (16 endpoints)
- DSP API: `src/dsp_api.cpp`
- Pipeline API: `src/pipeline_api.cpp`

**WebSocket Entry:**
- `src/websocket_command.cpp` — `webSocketEvent()` handler for incoming messages
- `src/websocket_broadcast.cpp` — All `sendXxxState()` broadcast functions

## Cross-Cutting Concerns

**Logging:**
- `src/debug_serial.h` — `LOG_D`, `LOG_I`, `LOG_W`, `LOG_E` macros with configurable level
- Never log in ISR or audio task — use dirty-flag pattern with main-loop emission
- Log transitions, not repetitive state (use static `prev` variables)

**Validation:**
- `hal_validate_config()` in `src/hal/hal_types.h` — Validates all HalDeviceConfig fields before `mgr.setConfig()`
- Input sanitization in REST handlers via ArduinoJson parsing
- `sanitize_filename()` for user file paths

**Persistence:**
- Settings: JSON files on LittleFS (`/settings.json`, `/hal_config.json`, `/hal_auto_devices.json`)
- HAL fault counters: NVS (Preferences) namespace
- Diagnostic journal: Binary ring buffer on LittleFS (`/diag_journal.bin`)
- Crash log: LittleFS ring buffer
- DSP presets: LittleFS JSON files

**PSRAM Allocation:**
- `src/psram_alloc.h/.cpp` — PSRAM-first with SRAM fallback (64KB cap)
- DMA buffers: 16x2KB internal SRAM, pre-allocated at boot
- ASRC history buffers: PSRAM (~1.5KB per active lane)
- DSP FIR/delay buffers: PSRAM

---

*Architecture analysis: 2026-03-25*
