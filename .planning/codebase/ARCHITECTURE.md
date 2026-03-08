# Architecture

**Analysis Date:** 2026-03-08

## Pattern Overview

**Overall:** Event-driven, dirty-flag-based singleton state with layered module pattern on a FreeRTOS dual-core firmware.

**Key Characteristics:**
- All application state lives in a single singleton `AppState` accessed via `appState` macro (`src/app_state.h`)
- Change propagation uses dirty flags + FreeRTOS event group bits; the main loop wakes in <1 µs on any state change (`app_events_wait(5)`) instead of `delay(5)`
- Core 1 is exclusively reserved for audio: `loopTask` (Arduino main loop) + `audio_pipeline_task`. No new tasks may be pinned to Core 1
- HAL is the single source of truth for all hardware devices — audio pipeline sources/sinks are registered and removed exclusively through `hal_pipeline_bridge`
- Compile-time feature flags gate major subsystems: `DAC_ENABLED`, `DSP_ENABLED`, `GUI_ENABLED`, `USB_AUDIO_ENABLED`

---

## AppState Singleton

**File:** `src/app_state.h` (553 lines), `src/app_state.cpp`

**Access pattern:**
```cpp
// Preferred — use appState macro everywhere
appState.mqttEnabled = true;
appState.markDacDirty();   // sets _dacDirty + signals EVT_DAC

// Singleton accessor (used inside AppState methods)
AppState::getInstance()
```

**Dirty flag pattern — every subsystem follows this template:**
```cpp
// In app_state.h
void markXxxDirty() { _xxxDirty = true; app_events_signal(EVT_XXX); }
bool isXxxDirty() const { return _xxxDirty; }
void clearXxxDirty() { _xxxDirty = false; }

// In main loop
if (appState.isXxxDirty()) {
    sendXxxState();           // broadcast to WS clients
    appState.clearXxxDirty();
}
```

**Active dirty flags (16 assigned, 8 spare in 24-bit event group):**

| Flag | Event Bit | Subsystem |
|------|-----------|-----------|
| `_otaDirty` | `EVT_OTA` (bit 0) | OTA task → main loop WS broadcast |
| `_displayDirty` | `EVT_DISPLAY` (bit 1) | GUI backlight/timeout → WS/MQTT |
| `_buzzerDirty` | `EVT_BUZZER` (bit 2) | GUI/API buzzer change → WS |
| `_sigGenDirty` | `EVT_SIGGEN` (bit 3) | Signal generator state → WS/MQTT |
| `_dspConfigDirty` | `EVT_DSP_CONFIG` (bit 4) | DSP pipeline config → WS |
| `_dacDirty` | `EVT_DAC` (bit 5) | DAC output state → WS/MQTT |
| `_eepromDirty` | `EVT_EEPROM` (bit 6) | EEPROM scan → WS |
| `_usbAudioDirty` | `EVT_USB_AUDIO` (bit 7) | USB audio state → WS |
| `_usbAudioVuDirty` | `EVT_USB_VU` (bit 8) | USB audio VU (high-freq) → WS |
| `_settingsDirty` | `EVT_SETTINGS` (bit 9) | General settings → WS/MQTT |
| `_adcEnabledDirty` | `EVT_ADC_ENABLED` (bit 10) | ADC lane enable → WS/MQTT |
| `_ethernetDirty` | `EVT_ETHERNET` (bit 11) | Ethernet link state → WS |
| `_diagJournalDirty` | `EVT_DIAG` (bit 12) | Diagnostic event → WS |
| `_dacSettingsDirty` | `EVT_DAC_SETTINGS` (bit 13) | DAC config → WS |
| `_halDeviceDirty` | `EVT_HAL_DEVICE` (bit 14) | HAL device state → WS |
| `_channelMapDirty` | `EVT_CHANNEL_MAP` (bit 15) | Audio channel map → WS |

**Cross-task coordination flags (volatile, not dirty flags):**
```cpp
volatile bool _mqttReconfigPending;   // web UI broker change → mqtt_task reconnects
volatile int8_t _pendingApToggle;     // MQTT cmd → main loop executes WiFi mode switch
volatile int8_t _pendingDacToggle;    // WS cmd → main loop executes DAC init/deinit
volatile int8_t _pendingEs8311Toggle; // WS cmd → main loop executes ES8311 init/deinit
```

**FSM states (`AppFSMState`):**
```cpp
enum AppFSMState {
    STATE_IDLE, STATE_SIGNAL_DETECTED, STATE_AUTO_OFF_TIMER,
    STATE_WEB_CONFIG, STATE_OTA_UPDATE, STATE_ERROR
};
```

---

## FreeRTOS Task Layout

**Core assignment is fixed. Core 1 = audio-only.**

| Task | Core | Priority | Stack | Notes |
|------|------|----------|-------|-------|
| `loopTask` (Arduino main) | Core 1 | 1 | default | HTTP, WS, button, OTA check, dirty-flag dispatch |
| `audio_pipeline_task` | Core 1 | 3 | `TASK_STACK_SIZE_AUDIO` | I2S DMA read, DSP, matrix, sink write. Preempts loopTask |
| `gui_task` | Core 0 | — | — | LVGL rendering, encoder input. Guarded by `GUI_ENABLED` |
| `mqtt_task` | Core 0 | 2 | `TASK_STACK_SIZE_MQTT` | Reconnect (blocking TCP), `mqttClient.loop()`, publish at 20 Hz |
| `usb_audio_task` | Core 0 | 1 | — | TinyUSB UAC2 poll, adaptive rate (100ms idle / 1ms streaming) |
| OTA check task | Core 0 | — | — | One-shot, `startOTACheckTask()` |
| OTA download task | Core 0 | — | — | One-shot, `startOTADownloadTask()` |

**Key rules:**
- MQTT reconnect (`mqttReconnect()`, 1–3 s blocking TCP) runs inside `mqtt_task` only — never in the main loop
- `audio_pipeline_task` never calls `Serial.print` (blocks UART TX → audio dropouts). Use dirty flag → `audio_periodic_dump()` from main loop
- `mqtt_task` polls at 20 Hz (`vTaskDelay(50 ms)`) and does NOT wait on the event group (avoids race with main loop's single-consumer `pdTRUE` clear)

**I2S DAC deinit handshake (audioPaused semaphore):**
```
DAC deinit:  set audioPaused=true → xSemaphoreTake(audioTaskPausedAck, 50ms) → i2s_driver_uninstall()
Audio task:  if (audioPaused) { xSemaphoreGive(audioTaskPausedAck); yield; }
```

---

## Audio Pipeline Architecture

**File:** `src/audio_pipeline.h/.cpp`

**Signal flow:**
```
[8 × AudioInputSource]
    PCM1808 ADC1 (lane 0, HAL)
    PCM1808 ADC2 (lane 1, HAL)
    SigGen       (lane N, HalSigGen)
    USB Audio    (lane N, HalUsbAudio)
    └─ read() callback → int32 DMA buffer (SRAM)
        ↓
[Int32 → Float32 conversion] + [Noise gate] + [Per-input DSP (dsp_pipeline)]
        ↓
[16×16 Routing Matrix]  (matrix gain[out_ch][in_ch], bypass flag)
        ↓
[Per-output DSP (output_dsp)]  — biquad, gain, limiter, compressor, mute
        ↓
[8 × AudioOutputSink]
    PCM5102A (slot 0, HAL)
    ES8311   (slot 1, HAL)
    └─ write() callback → int32 I2S TX buffer
```

**Buffer types:**
- DMA raw buffers: `int32_t*` in internal SRAM (`heap_caps_malloc(MALLOC_CAP_INTERNAL)`)
- Float working buffers: `float*` in PSRAM (`ps_calloc` or `heap_caps_malloc(MALLOC_CAP_SPIRAM)`)
- 256 stereo frames per DMA buffer (`I2S_DMA_BUF_LEN`)

**Pipeline dimensions (from `src/config.h`):**
- `AUDIO_PIPELINE_MAX_INPUTS` = 8 lanes
- `AUDIO_PIPELINE_MATRIX_SIZE` = 16 channels
- `AUDIO_OUT_MAX_SINKS` = 8 slots

**Input source API (slot-indexed, preferred):**
```cpp
audio_pipeline_set_source(int lane, const AudioInputSource *src);
audio_pipeline_remove_source(int lane);
// Both use vTaskSuspendAll() for atomic update across both Core 1 tasks
```

**Sink API (slot-indexed, preferred):**
```cpp
audio_pipeline_set_sink(int slot, const AudioOutputSink *sink);
audio_pipeline_remove_sink(int slot);
```

---

## HAL Framework

**Files:** `src/hal/`

**Device lifecycle:** `UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ⇄ UNAVAILABLE → ERROR / REMOVED / MANUAL`

**Key components:**

| Component | File | Role |
|-----------|------|------|
| `HalDevice` (abstract base) | `hal/hal_device.h` | Lifecycle interface: `probe()`, `init()`, `deinit()`, `healthCheck()`, `getInputSource()` |
| `HalDeviceManager` (singleton) | `hal/hal_device_manager.h/.cpp` | Registers up to 24 devices, priority-sorted init, pin claim tracking, state change callback |
| `HalPipelineBridge` | `hal/hal_pipeline_bridge.h/.cpp` | Connects HAL lifecycle to audio pipeline. Owns source/sink registration and removal |
| `HalDeviceDb` | `hal/hal_device_db.h/.cpp` | In-memory device database + LittleFS JSON persistence (`/hal_config.json`) |
| `HalDiscovery` | `hal/hal_discovery.h/.cpp` | 3-tier: I2C bus scan → EEPROM probe → manual config |
| `HalApi` | `hal/hal_api.h/.cpp` | REST endpoints for HAL CRUD |
| `HalBuiltinDevices` | `hal/hal_builtin_devices.h/.cpp` | Driver registry: compatible string → factory function |
| `HalAudioHealthBridge` | `hal/hal_audio_health_bridge.h/.cpp` | Feeds ADC audio health status (AUDIO_HW_FAULT etc.) back into HAL state transitions |

**State change callback chain:**
```
HalDeviceManager._stateChangeCb
    → hal_pipeline_state_change()   (registered by hal_pipeline_sync() at boot)
        → on_device_available()     AVAILABLE: set_sink() or set_source()
        → on_device_unavailable()   UNAVAILABLE: _ready=false only (auto-recovery)
        → on_device_removed()       ERROR/MANUAL/REMOVED: remove_sink() / remove_source()
```

**Init priority ordering (higher = earlier):**
```
HAL_PRIORITY_BUS      = 1000  (I2C/I2S/SPI bus controllers)
HAL_PRIORITY_IO       = 900   (GPIO expanders)
HAL_PRIORITY_HARDWARE = 800   (Audio codecs: PCM5102A, ES8311, PCM1808)
HAL_PRIORITY_DATA     = 600   (Pipeline consumers)
HAL_PRIORITY_LATE     = 100   (Diagnostics, temp sensor)
```

**Registered onboard devices at boot:**

| Compatible String | Type | Notes |
|-------------------|------|-------|
| `ti,pcm5102a` | `HAL_DEV_DAC` | I2S primary DAC |
| `everest,es8311` | `HAL_DEV_CODEC` | I2C + I2S codec, PA via GPIO 53 |
| `ti,pcm1808` | `HAL_DEV_ADC` | I2S ADC x2 (instanceId 0 and 1) |
| `alx,ns4150b` | `HAL_DEV_AMP` | GPIO 53 class-D amp enable |
| `alx,temp-sensor` | `HAL_DEV_SENSOR` | ESP32-P4 internal temp |
| `alx,signal-gen` | `HAL_DEV_ADC` | Software ADC, HAL-managed lane |
| `alx,usb-audio` | `HAL_DEV_ADC` | TinyUSB UAC2, HAL-managed lane |
| Various peripheral | `HAL_DEV_GPIO` | Display, encoder, buzzer, LED, relay, button |

**Multi-instance support:**
```cpp
// HalDeviceDescriptor fields:
uint8_t instanceId;    // Auto-assigned, 0-based per compatible string
uint8_t maxInstances;  // Max concurrent instances (0 = type default)

// Manager API:
uint8_t countByCompatible(const char* compatible);
HalDevice* findByCompatible(const char* compatible, uint8_t instanceId);
```

---

## MQTT Architecture

**Files (3-file split):**

| File | Lines | Role |
|------|-------|------|
| `src/mqtt_handler.cpp` | ~1127 | Core lifecycle: connect, `setupMqtt()`, `mqttReconnect()`, `mqttCallback()`, HTTP API handlers |
| `src/mqtt_publish.cpp` | ~938 | All `publishMqtt*()` functions + `prevMqtt*` shadow statics for change detection |
| `src/mqtt_ha_discovery.cpp` | ~1888 | Home Assistant discovery message generation |
| `src/mqtt_task.cpp` | 49 | FreeRTOS task: reconnect + `mqttClient.loop()` + publish at 20 Hz |

**Thread safety rules in `mqttCallback()`:**
- No direct WebSocket calls
- No LittleFS calls
- No WiFi calls
- All side-effects go through dirty flags or `_pendingApToggle`

**Change detection shadow fields (file-local statics in `mqtt_publish.cpp`):**
```cpp
// Examples — not in AppState
static bool prevMqttConnected;
static float prevAudioLevel;
static bool prevAmplifierState;
// etc.
```

---

## Web Server and WebSocket

**HTTP Server (port 80):** `WebServer server(80)` — defined in `src/main.cpp`, routes registered in `setup()`
**WebSocket Server (port 81):** `WebSocketsServer webSocket(81)` — event handler `webSocketEvent()` in `src/websocket_handler.cpp`

**Authentication flow:**
1. `POST /api/auth/login` → sets HttpOnly session cookie
2. `GET /api/ws-token` → returns 16-char one-time token (60s TTL, 16-slot pool)
3. WS client connects → sends token in first auth message → server validates → `wsAuthStatus[num]=true`

**Binary WebSocket message types (audio streaming):**
```cpp
#define WS_BIN_WAVEFORM 0x01  // [type:1][adc:1][samples:256] = 258 bytes
#define WS_BIN_SPECTRUM 0x02  // [type:1][adc:1][freq:f32LE][bands:16×f32LE] = 70 bytes
```

**REST API endpoint groups registered in `setup()`:**
- Auth: `/api/auth/*`, `/api/ws-token`
- WiFi: `/api/wifi*`, `/api/networks`, `/api/wifistatus`
- MQTT: `/api/mqtt`
- Settings: `/api/settings`, `/api/export`, `/api/import`
- Smart Sensing: `/api/smartsensing`
- DSP: registered via `registerDspApiEndpoints()` (`src/dsp_api.cpp`)
- DAC/HAL: registered via `registerDacApiEndpoints()`, `registerHalApiEndpoints()`, `registerPipelineApiEndpoints()`
- OTA: `/api/ota*`, `/api/releases`, `/api/firmware/upload`
- Signal Generator: `/api/signalgenerator`
- Pipeline: `/api/pipeline/matrix`, `/api/inputnames`

---

## GUI Architecture

**Files:** `src/gui/`

**Framework:** LVGL v9.4 + LovyanGFX on ST7735S 128×160 (landscape 160×128)

**Task:** `gui_task` pinned to Core 0 (`TASK_CORE_GUI=0`)

**Module breakdown:**

| Module | File | Role |
|--------|------|------|
| `gui_manager` | `gui/gui_manager.h/.cpp` | Init, FreeRTOS task, sleep/wake, dashboard refresh |
| `gui_input` | `gui/gui_input.h/.cpp` | ISR-driven rotary encoder (Gray code state machine) |
| `gui_theme` | `gui/gui_theme.h/.cpp` | Orange accent theme, dark/light mode |
| `gui_navigation` | `gui/gui_navigation.h/.cpp` | Screen stack with push/pop and transition animations |
| Screens | `gui/screens/scr_*.h/.cpp` | Individual screen implementations (see STRUCTURE.md) |

---

## DSP Pipeline

**Files:** `src/dsp_pipeline.h/.cpp`, `src/output_dsp.h/.cpp`, `src/dsp_api.h/.cpp`

**Pre-matrix DSP (`dsp_pipeline`):** Applied per input lane before the routing matrix. 4 channels (L1, R1, L2, R2). Up to 24 stages per channel: biquad IIR, FIR, limiter, gain, delay, polarity, mute, compressor. Double-buffered config with `dsp_swap_config()` for glitch-free updates.

**Post-matrix DSP (`output_dsp`):** Applied per mono output channel (8 channels) after the matrix, before sink write. Stages: biquad, gain, limiter, compressor, polarity, mute. Up to 12 stages per channel.

**ESP-DSP library:** On ESP32, uses pre-built `libespressif__esp-dsp.a` (S3 assembly-optimized biquad, FFT, vector math). Native tests use `lib/esp_dsp_lite/` (ANSI C fallback, `lib_ignore`d on ESP32).

---

## Settings Persistence

**Primary:** `/config.json` (LittleFS) — JSON format `{ "version": 1, "settings": {...}, "mqtt": {...} }`. Atomic write via `config.json.tmp` → rename.

**Fallback (legacy):** `/settings.txt` and `/mqtt_config.txt` — read on first boot if JSON absent, auto-migrated.

**HAL config:** `/hal_config.json` — per-device `HalDeviceConfig` structs.

**Matrix config:** LittleFS (via `audio_pipeline_save_matrix()` / `audio_pipeline_load_matrix()`)

**DSP config:** LittleFS (via `dsp_api.cpp`, debounced 2s save)

**NVS (Preferences):** WiFi credentials, serial number, crash log reset reason

---

## Error Handling Strategy

**Approach:** Non-panicking, self-healing where possible. Error state communicated via dirty flags.

**Patterns:**
- HAL device failures: retry up to 3× with backoff → ERROR state → `diag_emit()` → WS broadcast
- Flap guard: >2 AVAILABLE↔UNAVAILABLE transitions in 30s → ERROR + `DIAG_HAL_DEVICE_FLAPPING`
- Boot loop detection: 3 consecutive crash resets → `halSafeMode=true` (skips HAL init, WiFi+web only)
- Heap critical: `ESP.getMaxAllocHeap() < 40KB` → `heapCritical=true` → OTA check skipped, DSP allocations blocked
- WiFi: exponential backoff (`wifiBackoffDelay`, max 60s), auto-reconnect via WiFi event handler

---

## Cross-Cutting Concerns

**Logging:** `debug_serial.h` macros (`LOG_D`/`LOG_I`/`LOG_W`/`LOG_E`). `DebugSerial` class forwards to both Serial and WebSocket. Module prefix `[ModuleName]` extracted for frontend category filtering.

**Validation (inputs):** HTTP API handlers validate JSON fields inline. Auth uses PBKDF2-SHA256 (10,000 iterations). Login rate limiting: failed attempts set `_nextLoginAllowedMs`; excess requests return HTTP 429 immediately.

**Diagnostics:** `diag_journal_init()` → `diag_emit()` → ring buffer (32 entries, PSRAM) → periodic flush to `/diag_journal.bin`. `EVT_DIAG` signals main loop to broadcast via WS.

---

*Architecture analysis: 2026-03-08*
