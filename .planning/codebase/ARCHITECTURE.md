# Architecture

**Analysis Date:** 2026-03-09

## System Overview

ALX Nova Controller 2 is a dual-core ESP32-P4 embedded firmware. The system runs Arduino framework on top of ESP-IDF 5. Core 1 is exclusively reserved for real-time audio. Core 0 runs all service tasks (GUI, MQTT, USB Audio, OTA).

The overall pattern is **event-driven layered embedded firmware** with a central singleton state store, a hardware abstraction layer, a modular audio pipeline, and a web+WebSocket frontend all on a single MCU.

**Key Design Rules:**
- Core 1 is reserved for `loopTask` (priority 1) and `audio_pipeline_task` (priority 3). No new tasks may be pinned to Core 1.
- The main loop never calls MQTT functions directly. All MQTT work runs inside `mqtt_task` on Core 0.
- GUI, OTA, and USB Audio tasks run on Core 0.
- Cross-core communication uses dirty flags in `AppState` and the `app_events` FreeRTOS event group.
- Audio pipeline callbacks (`AudioInputSource.read`, `AudioOutputSink.write`) must never block or allocate.

---

## Layers

### 1. Hardware Layer (HAL Framework)
- **Purpose:** Abstract all physical hardware devices behind a unified lifecycle and interface
- **Location:** `src/hal/`
- **Contains:** Device drivers, device manager singleton, discovery, pipeline bridge, driver registry, EEPROM v3, health monitoring
- **Key types:** `HalDevice` (abstract base), `HalDeviceManager` (singleton, up to 24 devices), `HalDeviceDescriptor`, `HalDeviceConfig`, `HalDeviceState` enum
- **Depends on:** `src/config.h`, `src/app_events.h`, `src/audio_pipeline.h`, `src/diag_journal.h`
- **Used by:** `src/main.cpp` (device registration at boot), `src/hal/hal_pipeline_bridge.cpp` (lifecycle to pipeline sync)

### 2. Audio Pipeline Layer
- **Purpose:** Real-time digital audio processing on Core 1
- **Location:** `src/audio_pipeline.h/.cpp`, `src/i2s_audio.h/.cpp`, `src/dsp_pipeline.h/.cpp`, `src/output_dsp.h/.cpp`
- **Contains:** 8-lane input, per-input DSP, 16x16 routing matrix, per-output DSP, 8-slot sink dispatch
- **Key types:** `AudioInputSource` (struct with callbacks: `read`, `isActive`, `getSampleRate`), `AudioOutputSink` (struct with callbacks: `write`, `isReady`)
- **Depends on:** HAL layer for device registration; I2S driver for DMA buffers
- **Used by:** `audio_pipeline_task` (Core 1), HAL pipeline bridge

### 3. Application State Layer
- **Purpose:** Centralized application state with dirty-flag-driven change detection
- **Location:** `src/app_state.h/.cpp`, `src/state/` (15 domain headers), `src/app_events.h/.cpp`
- **Contains:** `AppState` singleton, 15 domain sub-structs, 16 event bits, dirty-flag setters/clearers
- **Depends on:** Domain state headers, `app_events.h`
- **Used by:** All modules — accessed via the `appState` macro (`AppState::getInstance()`)

### 4. Service/Subsystem Layer
- **Purpose:** Business logic — sensing, MQTT, WiFi, OTA, auth, settings, signal gen, DSP config, diagnostics
- **Location:** `src/` flat files: `smart_sensing.cpp`, `mqtt_handler.cpp`, `mqtt_publish.cpp`, `mqtt_ha_discovery.cpp`, `mqtt_task.cpp`, `wifi_manager.cpp`, `ota_updater.cpp`, `settings_manager.cpp`, `auth_handler.cpp`, `signal_generator.cpp`, `usb_audio.cpp`, `task_monitor.cpp`, `crash_log.cpp`, `diag_journal.cpp`, `debug_serial.cpp`
- **Depends on:** `AppState`, HAL layer, audio pipeline
- **Used by:** `src/main.cpp` (setup + loop dispatch), FreeRTOS tasks

### 5. Web Interface Layer
- **Purpose:** HTTP REST API + WebSocket real-time state broadcasting to browser clients
- **Location:** `src/main.cpp` (route registration), `src/websocket_handler.h/.cpp`, `src/web_pages.cpp` / `src/web_pages_gz.cpp` (auto-generated), `web_src/` (edit source here)
- **Contains:** WebServer port 80, WebSocketsServer port 81, gzip-compressed HTML/CSS/JS
- **Depends on:** `AppState`, auth handler, all subsystem modules
- **Used by:** Browser clients

### 6. GUI Layer
- **Purpose:** LVGL 9.4 graphical interface on ST7735S 128x160 TFT display
- **Location:** `src/gui/` — `gui_manager.h/.cpp`, `gui_input.h/.cpp`, `gui_theme.h/.cpp`, `gui_navigation.h/.cpp`, `src/gui/screens/`
- **Contains:** `gui_task` (Core 0), 14 screens, rotary encoder ISR input, LovyanGFX driver config
- **Depends on:** `AppState`, HAL devices (display, encoder), buzzer
- **Used by:** `src/main.cpp` (calls `gui_init()` once in `setup()`)

---

## Core Subsystems

### Audio Pipeline (`src/audio_pipeline.h/.cpp`)

The audio pipeline runs as a dedicated FreeRTOS task (`audio_pipeline_task`, Core 1, priority 3).

**Signal Chain:**
```
AudioInputSource[0..7]  (PCM1808 ADC / SigGen / USB Audio)
  -> per-lane pre-matrix DSP (dsp_pipeline, optional, double-buffered)
     -> 16x16 routing matrix (linear gains, float32)
        -> per-output post-matrix DSP (output_dsp, mono, double-buffered)
           -> AudioOutputSink[0..7]  (PCM5102A / ES8311 / MCP4725)
```

**Key constants defined in `src/audio_pipeline.h`:**
- `AUDIO_PIPELINE_MAX_INPUTS = 8` (lanes 0-7)
- `AUDIO_PIPELINE_MAX_OUTPUTS = 8`
- `AUDIO_PIPELINE_MATRIX_SIZE = 16` (16x16 matrix; top-left 8x8 in typical use)
- `AUDIO_OUT_MAX_SINKS = 8`

**Slot-indexed source/sink API:** All source and sink registration uses explicit slot indices. `audio_pipeline_set_source(lane, src)` and `audio_pipeline_set_sink(slot, sink)` use `vTaskSuspendAll()` for atomicity. The HAL pipeline bridge owns all registration via this API.

**Input data format:** DMA raw buffers captured via `<driver/i2s_std.h>` (IDF5); converted to `float32 [-1.0, +1.0]` internally; converted back to `int32` at sink write.

**Lazy SRAM allocation:** DMA raw buffers and float working buffers are allocated on `set_source` / `set_sink`, not statically.

**Noise gate:** Uses `AudioInputSource.isHardwareAdc` flag to gate only physical ADC lanes, not SigGen or USB Audio.

**VU metering:** Per-source VU for all active input lanes via `audio_pipeline_get_lane_vu_l/r(lane)`. Per-sink VU updated in `pipeline_write_output()`.

**Read-only accessor:** `audio_pipeline_get_source(lane)` returns registered source info — used by `sendAudioChannelMap()` for dynamic input discovery.

**DSP swap safety:** `audio_pipeline_notify_dsp_swap()` is called before `_swapRequested` is set to arm a PSRAM hold buffer, preventing audio glitches during double-buffered config swap.

**Matrix persistence:** Loaded/saved via `audio_pipeline_load_matrix()` / `audio_pipeline_save_matrix()` to `/pipeline_matrix.json`. Old 8x8 matrices are placed in the top-left corner of the 16x16 matrix on load (backward-compat migration).

### HAL Framework (`src/hal/`)

**Device Lifecycle State Machine:**
```
UNKNOWN -> DETECTED -> CONFIGURING -> AVAILABLE <-> UNAVAILABLE -> ERROR / REMOVED / MANUAL
```

`volatile bool _ready` and `volatile HalDeviceState _state` on `HalDevice` base class enable lock-free reads from the audio pipeline task without virtual dispatch.

**HalDeviceManager** (`src/hal/hal_device_manager.h/.cpp`): Meyers singleton managing up to `HAL_MAX_DEVICES=24` devices. Tracks 24-pin GPIO claims (`claimPin`/`releasePin`). Stores per-device `HalDeviceConfig` persisted to `/hal_config.json`. Fires `HalStateChangeCb` on every `_state` transition. Non-blocking self-healing retry with `HalRetryState` (3 retries, timestamp-based, fault counter in NVS).

**HAL Pipeline Bridge** (`src/hal/hal_pipeline_bridge.h/.cpp`): Registered as the state change callback in `HalDeviceManager`. On `AVAILABLE`: calls `audio_pipeline_set_sink()` for `HAL_CAP_DAC_PATH` devices, `audio_pipeline_set_source()` for `HAL_CAP_ADC_PATH` devices (via `dev->getInputSource()`). On `UNAVAILABLE`: sets `_ready=false` only (auto-recovery). On `ERROR`/`REMOVED`/`MANUAL`: calls `audio_pipeline_remove_sink()` or `audio_pipeline_remove_source()`. Maintains `_halSlotToSinkSlot[]` and `_halSlotToAdcLane[]` mapping tables. Capability-based ordinal counting assigns slots and lanes dynamically.

**HAL Driver Registry** (`src/hal/hal_driver_registry.h`): Maps compatible strings (`"vendor,model"`) to factory functions (`HalDeviceFactory`). Registered via `hal_registry_register()` in `hal_builtin_devices.cpp`.

**HAL Discovery** (`src/hal/hal_discovery.h/.cpp`): 3-tier device discovery: I2C bus scan (0x08-0x77) → EEPROM probe (AT24C02 at 0x50-0x57 with v3 compatible string) → manual config from `/hal_config.json`. Skips I2C Bus 0 (GPIO 48/54) when WiFi is active (SDIO conflict on ESP32-P4).

**EEPROM v3** (`src/hal/hal_eeprom_v3.h/.cpp`): Extends 256-byte AT24C02 layout: compatible string at offset 0x5E (32 bytes), CRC-16/CCITT at 0x7E.

**HAL Device Database** (`src/hal/hal_device_db.h/.cpp`): In-memory device database with builtin entries plus LittleFS JSON persistence (`/hal_auto_devices.json`).

**Registered onboard builtin devices (registered in `src/main.cpp` `setup()`):**
- `ti,pcm5102a` — I2S DAC, primary output (`HAL_CAP_DAC_PATH`)
- `evergrande,es8311` — I2C+I2S codec (`HAL_CAP_CODEC`)
- `ti,pcm1808` (x2) — I2S ADC inputs (`HAL_CAP_ADC_PATH`), dual-master clock topology
- `ti,ns4150b` — GPIO class-D amplifier enable on GPIO 53 (`HalNs4150b`)
- ESP32-P4 internal temperature sensor (`HalTempSensor`, `CONFIG_IDF_TARGET_ESP32P4` guard)
- Display: `HalDisplay` (TFT SPI)
- Encoder: `HalEncoder` (rotary encoder with Gray code ISR)
- Buzzer: `HalBuzzer` (PWM buzzer)
- LED: `HalLed`
- Relay: `HalRelay` (amplifier relay GPIO)
- Button: `HalButton` (factory reset button)
- Signal gen: `HalSignalGen` (PWM mode signal generator)

**HAL Audio Interfaces** (`src/hal/hal_audio_interfaces.h`): `HalAudioDacInterface`, `HalAudioAdcInterface`, `HalAudioCodecInterface` — ESPHome-style component/platform layering. Device drivers implement these on top of `HalDevice`.

**Note: `initAll()` is never called.** Devices are explicitly `probe()` then `init()`'d after `registerDevice()` in `setup()`. This gives `main.cpp` control over the exact init order.

### AppState — Decomposed Singleton (`src/app_state.h`, `src/state/`)

Central state store. Reduced from 553 lines to ~80 lines by decomposing into 15 domain headers.

Access via `appState` macro: `#define appState AppState::getInstance()`

**Domain sub-structs composed in `AppState`:**
| Field | Type | Header |
|---|---|---|
| `appState.wifi` | `WifiState` | `src/state/wifi_state.h` |
| `appState.mqtt` | `MqttState` | `src/state/mqtt_state.h` |
| `appState.audio` | `AudioState` | `src/state/audio_state.h` |
| `appState.dac` | `DacState` | `src/state/dac_state.h` |
| `appState.dsp` | `DspSettingsState` | `src/state/dsp_state.h` |
| `appState.general` | `GeneralState` | `src/state/general_state.h` |
| `appState.debug` | `DebugState` | `src/state/debug_state.h` |
| `appState.ota` | `OtaState` | `src/state/ota_state.h` |
| `appState.display` | `DisplayState` | `src/state/display_state.h` |
| `appState.buzzer` | `BuzzerState` | `src/state/buzzer_state.h` |
| `appState.sigGen` | `SignalGenState` | `src/state/signal_gen_state.h` |
| `appState.ethernet` | `EthernetState` | `src/state/ethernet_state.h` |
| `appState.usbAudio` | `UsbAudioState` | `src/state/usb_audio_state.h` |
| `appState.fsmState` | `AppFSMState` | `src/state/enums.h` |

**Cross-domain coordination fields** (remain in `AppState` shell):
- `volatile bool _mqttReconfigPending` — set by HTTP handler; `mqtt_task` reconnects when set
- `volatile int8_t _pendingApToggle` — set by MQTT callback; main loop executes WiFi mode change
- `volatile bool _halScanInProgress` — guards concurrent scan requests

**Dirty flag + event pattern:** Every state mutation calls a dirty setter which also signals the FreeRTOS event group:
```cpp
void markDacDirty() { _dacDirty = true; app_events_signal(EVT_DAC); }
```

**FSM States** (`src/state/enums.h`): `STATE_IDLE`, `STATE_SIGNAL_DETECTED`, `STATE_AUTO_OFF_TIMER`, `STATE_WEB_CONFIG`, `STATE_OTA_UPDATE`, `STATE_ERROR`.

**Cross-core volatile field:** `appState.audio.paused` — `volatile bool`, written Core 0, read Core 1. `appState.audio.taskPausedAck` — binary semaphore for deterministic I2S driver pause handshake during DAC deinit/reinit.

**Event bits** (`src/app_events.h`): 16 bits assigned (bits 0-15), 8 spare. `EVT_ANY = 0x00FFFFFF` covers all 24 usable FreeRTOS event group bits (bits 24-31 reserved by FreeRTOS).

### MQTT Subsystem

Split across 4 files to keep each file focused:
- `src/mqtt_handler.cpp` (~1,129 lines): Core lifecycle, settings load/save, callback dispatch, HTTP API handlers
- `src/mqtt_publish.cpp`: All `publishMqtt*()` functions; change-detection `prevMqtt*` file-local statics live here (not in AppState)
- `src/mqtt_ha_discovery.cpp` (~1,898 lines): Home Assistant MQTT discovery message generation
- `src/mqtt_task.cpp`: Dedicated FreeRTOS task (Core 0, priority 2). Owns `mqttClient.loop()`, reconnect (1-3s blocking TCP when broker unreachable), periodic publish at 20 Hz. Polls `appState._mqttReconfigPending`.

`mqttCallback()` is thread-safe: no direct WebSocket, LittleFS, or WiFi calls — all side effects use dirty flags or `_pendingApToggle`.

Per-lane MQTT topics use dynamic iteration over `activeInputCount` with worst-case health aggregation across all active ADC lanes.

### Web Server and WebSocket (`src/main.cpp`, `src/websocket_handler.h/.cpp`)

- HTTP server on port 80 (`WebServer server(80)`) — all routes registered in `src/main.cpp` `setup()`
- WebSocket server on port 81 (`WebSocketsServer webSocket(81)`) — real-time state broadcasting
- Binary WS frames for audio: `WS_BIN_WAVEFORM` (0x01), `WS_BIN_SPECTRUM` (0x02); audio levels remain JSON
- WS authentication: client fetches short-lived token from `GET /api/ws-token` (60s TTL, 16-slot pool), sends it as first WS message
- `_wsAuthCount` file-static counter in `websocket_handler.cpp`; `wsAnyClientAuthenticated()` public accessor
- All heavy broadcast functions check `if (!wsAnyClientAuthenticated()) return;` before building JSON
- `drainPendingInitState()` called each loop iteration to send initial state to newly authenticated clients

### GUI Layer (`src/gui/`)

- `gui_task` runs on Core 0, initialized via `gui_init()` in `setup()`
- LVGL v9.4 + LovyanGFX on ST7735S 128x160 (landscape 160x128)
- Rotary encoder input via ISR-driven Gray code state machine in `gui_input.cpp`
- Screen navigation via push/pop stack with transition animations (`gui_navigation.cpp`)
- All GUI code guarded by `-D GUI_ENABLED`
- Synchronous SPI (no DMA on ESP32-P4 — GDMA completion not wired in LovyanGFX for P4)

**Screens** (`src/gui/screens/`):
`scr_boot_anim`, `scr_desktop`, `scr_home`, `scr_control`, `scr_wifi`, `scr_mqtt`, `scr_settings`, `scr_debug`, `scr_dsp`, `scr_devices`, `scr_siggen`, `scr_support`, `scr_menu`, `scr_keyboard`, `scr_value_edit`

### DSP Subsystem (`src/dsp_pipeline.h/.cpp`, `src/output_dsp.h/.cpp`, `src/dsp_biquad_gen.h/.c`)

- **Pre-matrix DSP** (`dsp_pipeline`): 4-channel stereo engine, up to `DSP_MAX_STAGES=24` stages. Double-buffered config with glitch-free atomic swap. Stage types: biquad IIR (LPF/HPF/BPF/PEQ/shelves/allpass/Linkwitz/first-order), FIR, limiter, gain, delay, polarity, mute, compressor, noise gate, tone control, stereo width, loudness, bass enhancement, multi-band compressor, speaker protection, convolution, decimation.
- **Post-matrix DSP** (`output_dsp`): Per-output mono engine, 8 channels x 12 stages. Subset of stage types: biquad, gain, limiter, mute, polarity, compressor. Double-buffered config.
- **Coefficients** (`src/dsp_biquad_gen.h/.c`): RBJ Audio EQ Cookbook implementation, renamed `dsp_gen_*` prefix to avoid symbol conflicts with pre-built ESP-DSP.
- **ESP-DSP** (pre-built `libespressif__esp-dsp.a`): S3 assembly-optimized biquad IIR, Radix-4 FFT, window functions, vector math. Used on ESP32 only. Native tests use `lib/esp_dsp_lite/` ANSI C fallback.
- **Delay lines** use `ps_calloc()` (PSRAM) when available, guarded by 40KB heap reserve pre-flight check.
- **DSP swap failure** standardized via `dsp_log_swap_failure()` inline helper in `src/dsp_pipeline.h`. HTTP endpoints return HTTP 503 on swap failure.

### Diagnostics (`src/diag_journal.h/.cpp`, `src/diag_event.h`, `src/diag_error_codes.h`)

- `DiagEvent` struct (64 bytes fixed): seq (NVS-backed monotonic), bootId, timestamp, heapFree, code (`DiagErrorCode`), corrId (correlation group), severity, slot (HAL device slot), device[16], message[24]
- `diag_emit()`: writes to 32-entry PSRAM hot ring buffer, signals `EVT_DIAG`, prints `[DIAG]` JSON on serial. Thread-safe via portMUX spinlock (~1 µs critical section). Not ISR-safe.
- `diag_journal_flush()`: persists WARN+ entries to `/diag_journal.bin` on LittleFS, CRC-32 per entry. Called every 60s and on shutdown.
- Correlation IDs via `hal_pipeline_begin_correlation()` / `hal_pipeline_end_correlation()` for grouping multi-step events.
- **HAL Audio Health Bridge** (`src/hal/hal_audio_health_bridge.h/.cpp`): Runs every 5s in main loop. Reads `AudioHealthStatus` per ADC lane, reverse-looks up owning HAL device slot, drives `AVAILABLE <-> UNAVAILABLE` transitions. Flap guard: >2 transitions in 30s -> `ERROR` + `DIAG_HAL_DEVICE_FLAPPING`.

### Smart Sensing (`src/smart_sensing.h/.cpp`)

Audio-level-based signal detection using I2S ADC data. Controls amplifier relay GPIO. Implements auto-off timer FSM. Detection rate matches `appState.audio.updateRate`. Dynamically scaled smoothing alpha to maintain ~308ms time constant.

### Authentication (`src/auth_handler.h/.cpp`)

- PBKDF2-SHA256 password hashing (10,000 iterations, random 16-byte salt). Stored as `"p1:<saltHex>:<keyHex>"`.
- Legacy unsalted SHA256 hashes auto-migrate on next successful login.
- First-boot random password (10 chars, ~57-bit entropy) printed on TFT and serial. Regenerated on factory reset.
- Session cookie with `HttpOnly` flag.
- WS tokens: `GET /api/ws-token` -> 60s TTL, 16-slot pool, single-use.
- Login rate limiting: failed attempts set `_nextLoginAllowedMs`; excess returns HTTP 429 with `Retry-After`.

### OTA Updater (`src/ota_updater.h/.cpp`)

- GitHub releases API polling, SHA256 firmware verification, auto-update with configurable countdown.
- OTA runs as one-shot tasks on Core 0 (`startOTACheckTask()`, `startOTADownloadTask()`).
- Periodic check with exponential backoff (5min -> 60min on failures).
- Auto-update skipped when amplifier is in use (`appState.audio.amplifierState`).
- Uses Mozilla CA bundle via `ESP32CertBundle` for TLS.
- First OTA check delayed 30s after boot to let Ethernet DHCP + DNS stabilize.

### USB Audio (`src/usb_audio.h/.cpp`)

TinyUSB UAC2 speaker device on native USB OTG (GPIO 19/20 on ESP32-P4). SPSC lock-free ring buffer (1024 frames, PSRAM). HAL-managed via `HalUsbAudio` (`alx,usb-audio`, `HAL_DEV_ADC`). FreeRTOS task on Core 0 with adaptive poll rate (100ms idle, 1ms streaming). Guarded by `-D USB_AUDIO_ENABLED`.

---

## FreeRTOS Task Map

| Task | Core | Priority | Module |
|---|---|---|---|
| `loopTask` (Arduino `loop()`) | Core 1 | 1 | `src/main.cpp` |
| `audio_pipeline_task` | Core 1 | 3 | `src/audio_pipeline.cpp` |
| `gui_task` | Core 0 | default | `src/gui/gui_manager.cpp` |
| `mqtt_task` | Core 0 | 2 | `src/mqtt_task.cpp` |
| `usb_audio_task` | Core 0 | 1 | `src/usb_audio.cpp` |
| OTA check task | Core 0 | one-shot | `src/ota_updater.cpp` |
| OTA download task | Core 0 | one-shot | `src/ota_updater.cpp` |

`audio_pipeline_task` (priority 3) preempts `loopTask` (priority 1) during DMA processing, then yields 2 ticks. Audio task is the highest-priority task on Core 1.

---

## Data Flow

### Inbound Audio (ADC -> Pipeline -> DAC)

1. I2S DMA interrupts fill hardware ring buffers (PCM1808 x2 via `<driver/i2s_std.h>`)
2. `audio_pipeline_task` calls `AudioInputSource.read()` per active lane — returns int32 DMA frames
3. `pipeline_to_float()` converts int32 -> float32; noise gate applied to hardware ADC lanes
4. Pre-matrix per-lane DSP applied (`dsp_pipeline`, double-buffered config)
5. 16x16 routing matrix applied (float gains)
6. Post-matrix per-output DSP applied (`output_dsp`, mono)
7. Float32 -> int32 conversion; `AudioOutputSink.write()` called per active sink

### Web UI State Update Flow

1. Browser WS client authenticates (token from `GET /api/ws-token`)
2. `drainPendingInitState()` sends all current state as JSON frames on client connect
3. Subsystem changes state -> calls dirty setter -> `app_events_signal(EVT_XXX)` fires
4. `app_events_wait(5)` in main loop wakes immediately
5. Main loop checks each dirty flag, calls matching `sendXxxState()` WS broadcast function
6. Broadcast function checks `wsAnyClientAuthenticated()` before serializing JSON

### MQTT Publish Flow

1. `mqtt_task` polls at 20 Hz (50ms `vTaskDelay`)
2. Calls `mqttPublishPendingState()` — compares `appState.*` against `prevMqtt*` statics in `mqtt_publish.cpp`
3. Publishes changed topics via `mqttClient.publish()`
4. Calls `mqttPublishHeartbeat()` periodically

### HAL Device Lifecycle -> Audio Pipeline Flow

1. `hal_register_builtins()` populates driver registry with factory functions
2. `hal_pipeline_sync()` at boot registers the state change callback
3. Each device: `registerDevice()` -> `probe()` -> `init()` (explicit per device in `setup()`)
4. `init()` success -> HAL transitions `_state = AVAILABLE`, fires `HalStateChangeCb`
5. Bridge receives callback -> `hal_pipeline_on_device_available()` -> `audio_pipeline_set_sink()` or `audio_pipeline_set_source()`
6. `healthCheckAll()` called periodically from main loop; health failures drive `UNAVAILABLE`/`ERROR` -> bridge removes from pipeline

### Settings Persistence

- **Primary:** `/config.json` — JSON v1 format `{ "version": 1, "settings": {...}, "mqtt": {...} }`, atomic write via `.tmp` -> rename
- **Fallback:** `/settings.txt`, `/mqtt_config.txt` — legacy, only read when JSON v1 absent; auto-migrated on first boot
- **WiFi credentials:** NVS (`Preferences`) — survive LittleFS format and factory reset
- **HAL device configs:** `/hal_config.json` — persisted by `hal_save_device_configs()`
- **Audio routing matrix:** `/pipeline_matrix.json` — `audio_pipeline_load_matrix()` / `audio_pipeline_save_matrix()`
- **DSP config:** `/dsp_config.json` — debounced 2s save via `dsp_check_debounced_save()`
- **Diagnostic journal:** `/diag_journal.bin` — binary, CRC-32 per entry, flushed every 60s
- **Smart sensing settings:** part of `/config.json`
- **Signal gen settings:** part of `/config.json`

---

## Key Design Patterns

### Dirty Flag + FreeRTOS Event Group
Every state change calls a dirty setter which also signals the event group:
```cpp
void markDacDirty() { _dacDirty = true; app_events_signal(EVT_DAC); }
```
Main loop wakes immediately (`app_events_wait(5)`) and dispatches WS broadcasts:
```cpp
if (appState.isDacDirty()) { sendDacState(); appState.clearDacDirty(); }
```
The main loop is the sole consumer (`pdTRUE` clears on wait). `mqtt_task` does NOT use this group (avoids fan-out race).

### Slot-Indexed Source/Sink API
All audio pipeline registrations use explicit slot indices assigned by the HAL pipeline bridge via capability-based ordinal counting. Forward and reverse lookup APIs in `src/hal/hal_pipeline_bridge.h`:
- `hal_pipeline_get_sink_slot(halSlot)` — forward
- `hal_pipeline_get_slot_for_sink(sinkSlot)` — reverse
- `hal_pipeline_get_input_lane(halSlot)` — forward
- `hal_pipeline_get_slot_for_adc_lane(lane)` — reverse

### Double-Buffered DSP Config (Glitch-Free Swap)
`dsp_pipeline` and `output_dsp` maintain active + pending config buffers. Writes go to pending; a flag triggers atomic swap at next inter-buffer boundary. `audio_pipeline_notify_dsp_swap()` arms a PSRAM hold buffer to cover the swap gap.

### Deferred Device Toggle
DAC-path HAL devices cannot be directly `deinit()`'d from HTTP/MQTT handlers (audio task race). All toggle requests write to `appState.dac.pendingToggle` via `requestDeviceToggle(halSlot, action)`. Main loop processes the toggle safely each iteration.

### WS Broadcast Client Guard
`_wsAuthCount` counter in `src/websocket_handler.cpp` tracks authenticated clients. All expensive JSON broadcast functions check `wsAnyClientAuthenticated()` before building JSON. Direct response functions (e.g., `sendDisplayState`) are not guarded.

### HAL Compatible String Matching
Device identity uses ESPHome-style `"vendor,model"` compatible strings (e.g., `"ti,pcm5102a"`, `"alx,signal-gen"`). Driver registry lookup is by exact compatible string match; EEPROM v3 field provides the string for discovered devices.

### Cross-Core Volatile Semantics
`volatile bool _ready` and `volatile HalDeviceState _state` on `HalDevice` allow lock-free reads from `audio_pipeline_task` (Core 1) without virtual dispatch. `appState.audio.paused` is `volatile bool`. On ESP32-P4 (RISC-V), volatile applies to the field directly even within a containing struct.

### I2S Dual-Master Topology (PCM1808)
Both PCM1808 ADCs are configured as I2S master. I2S_NUM_0 (ADC1) outputs BCK/WS/MCLK clocks. I2S_NUM_1 (ADC2) has `BCK=I2S_PIN_NO_CHANGE`, data-only. ADC2 data pin uses `INPUT_PULLDOWN` to prevent floating-noise false positives. Init order: ADC2 first, ADC1 second.

### Logging
`debug_serial.h` macros (`LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`) with `[ModuleName]` prefixes. `DebugSerial` class forwards to both serial and WebSocket. Never log from `audio_pipeline_task` or ISRs — use dirty-flag + `audio_periodic_dump()` pattern (task sets flag; main loop calls dump function).

---

## Error Handling

**Strategy:** Non-blocking, diagnostic-emitting, self-healing.

- HAL device failures -> `diag_emit()` -> hot ring buffer -> `EVT_DIAG` -> WS/MQTT broadcast
- HAL retry state: 3 retries with timestamp-based backoff, fault counter persisted in NVS
- 3 consecutive crash boots -> safe mode (`appState.halSafeMode = true`); skip HAL init, WiFi+web only
- DSP swap failures -> `dsp_log_swap_failure()` inline helper; HTTP endpoints return HTTP 503
- `heapCritical` flag set when `ESP.getMaxAllocHeap() < 40KB` — guards TLS allocations, OTA
- Audio health bridge drives ADC HAL state transitions; flap guard prevents cascading ERROR churn
- `appState.audio.taskPausedAck` binary semaphore for deterministic I2S driver pause before DAC deinit

---

*Architecture analysis: 2026-03-09*
