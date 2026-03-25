# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Your Ears deserve the best!

Open Source based audio ecosystem that lowers the threshold in the creation of Audio controlling/producing devices effortless and easy. Achieved through a modular platform with standardised add-on (certified) mezzanine expansion modules. The platform can be used for prototyping, but also for commercialised end-user devices.

The platform provides a standardised MCU with Hardware Abstraction layer (HAL) with hotpluggable configurable high level Components like ADC, DSP, DAC and others and specific SOCs coming from the likes of Everest Semi, Ti/BurrBrown and Analog Devices.

It will remove the need for complex proprietary tooling like SigmaStudio removing vendor lock-in/limitations and facilitate mixing and matching SOCs to achieve the desired outcome.

The platform enables the creation of custom tailored audio devices, removing a high bar of entry and investment.

The platform brings a worldwide community of tinkerers and leading audio enthusiasts together, enabling to join forces and embed expertise in a sustainable and re-usable way (reduce e-waste). Creating best in class expansion modules that work out of the box.

The audio platform has the ambition to be the go-to choice when either using or developing audio products.

Community created and certified "Works with" standardised add-on components can be sold through the ALX e-commerce store, where ALX will offer to offload the manufacturing and shipment process or let the community do this themselves. When sold through the ALX Store the creator will receive a percentage of the revenue. "Works with" certification is avaiable to both individual end-user oriented or large(r) scale commercialised, altough with support restriction due to its community nature. 

Certified commercial partners are permitted to use the dedicated 'Certified' badge, oriented on large(r) scale commercialised and dedicated support. These can be sold through the ALX e-commerce store or through affiliate links listed on the ALX Certified devices section or other proprietary channels.

Only certified partners are permitted to use the dedicated 'Works With' badge.

The platform provides a HAL for ADC, DSP and DACs (extenable beyond these). Also has an intelligent external amplifier controller (Relay control) with smart auto-sensing, WiFi management, API, MQTT/Home Assistant integration, OTA firmware updates, a web configuration interface LVGL-based GUI, push buttons, rotary encoder input and extensive documentation.

Built with PlatformIO and the Arduino framework (esp-idf based). Current firmware version is defined in `src/config.h` as `FIRMWARE_VERSION`.

**Target board**: Waveshare ESP32-P4-WiFi6-DEV-Kit. PlatformIO board config: `esp32-p4`. Upload/monitor port: COM8.

## Build & Test Commands

```bash
# Build firmware for ESP32-P4
pio run

# Upload firmware to device (PYTHONIOENCODING needed on Windows — esptool Unicode fix)
PYTHONIOENCODING=utf-8 pio run --target upload

# Monitor serial output (115200 baud)
pio device monitor

# Run all unit tests (native platform, no hardware needed)
pio test -e native

# Run a specific test module
pio test -e native -f test_wifi
pio test -e native -f test_mqtt
pio test -e native -f test_auth

# Verbose test output
pio test -e native -v

# Run on-device tests (requires connected ESP32-P4 + TEST_MODE firmware)
cd device_tests && pytest tests/ --device-port COM8 --device-ip <DEVICE_IP> -m "not slow" -v

# Run specific on-device test module
cd device_tests && pytest tests/test_hal_devices.py --device-port COM8 --device-ip <DEVICE_IP> -v
```

Tests run on the `native` environment (host machine with gcc/MinGW) using the Unity framework (~3780 C++ tests across 124 modules). E2E browser tests: 304 tests across 52 Playwright specs. On-device tests: 206 tests across 21 modules (see [On-Device Tests](#on-device-tests-pyserial--pytest)). Mock implementations of Arduino, WiFi, MQTT, and Preferences libraries live in `test/test_mocks/`. Test modules: `test_utils`, `test_auth`, `test_wifi`, `test_mqtt`, `test_settings`, `test_ota`, `test_ota_task`, `test_button`, `test_websocket`, `test_websocket_messages`, `test_api`, `test_smart_sensing`, `test_buzzer`, `test_gui_home`, `test_gui_input`, `test_gui_navigation`, `test_pinout`, `test_i2s_audio`, `test_fft`, `test_signal_generator`, `test_audio_diagnostics`, `test_audio_health_bridge`, `test_audio_pipeline`, `test_vrms`, `test_dim_timeout`, `test_debug_mode`, `test_dsp`, `test_dsp_rew`, `test_dsp_presets`, `test_dsp_swap`, `test_crash_log`, `test_task_monitor`, `test_esp_dsp`, `test_usb_audio`, `test_hal_core`, `test_hal_bridge`, `test_hal_coord`, `test_hal_dsp_bridge`, `test_hal_discovery`, `test_hal_integration`, `test_hal_eeprom_v3`, `test_hal_pcm5102a`, `test_hal_pcm1808`, `test_hal_probe_retry`, `test_hal_es8311`, `test_hal_mcp4725`, `test_hal_siggen`, `test_hal_usb_audio`, `test_hal_custom_device`, `test_hal_multi_instance`, `test_hal_state_callback`, `test_hal_retry`, `test_hal_wire_mock`, `test_hal_i2c_bus`, `test_hal_buzzer`, `test_hal_button`, `test_hal_encoder`, `test_hal_ns4150b`, `test_hal_adc_base`, `test_hal_es9822pro`, `test_hal_es9843pro`, `test_hal_tdm_deinterleaver`, `test_hal_es9826`, `test_hal_es9821`, `test_hal_es9823pro`, `test_hal_es9820`, `test_hal_es9842pro`, `test_hal_es9840`, `test_hal_es9841`, `test_hal_es9038q2m`, `test_hal_es9039q2m`, `test_hal_es9069q`, `test_hal_es9033q`, `test_hal_es9020_dac`, `test_hal_es9038pro`, `test_hal_es9028pro`, `test_hal_es9039pro`, `test_hal_es9027pro`, `test_hal_es9081`, `test_hal_es9082`, `test_hal_es9017`, `test_hal_tdm_interleaver`, `test_hal_cs43198`, `test_hal_cs43131`, `test_hal_cs4398`, `test_hal_cs4399`, `test_hal_cs43130`, `test_hal_cirrus_dac_2ch`, `test_output_dsp`, `test_dac_hal`, `test_dac_eeprom`, `test_dac_settings`, `test_diag_journal`, `test_peq`, `test_evt_any`, `test_sink_slot_api`, `test_sink_write_utils`, `test_deferred_toggle`, `test_pipeline_bounds`, `test_pipeline_output`, `test_matrix_bounds`, `test_eth_manager`, `test_es8311`, `test_heap_monitor`, `test_http_security`, `test_heap_budget`, `test_pipeline_dma_guard`, `test_psram_alloc`, `test_ws_adaptive_rate`, `test_strncpy_safety`, `test_hal_init_result`, `test_hal_atomic_write`, `test_settings_export_v2`, `test_health_check`, `test_dsp_audio`, `test_hal_advanced`.

## Architecture

### State Management — AppState Singleton (DEBT-4 Decomposed, 2026-03-09)
Application state is decomposed across 15 lightweight domain-specific headers under `src/state/`, composed into a thin `AppState` singleton (reduced from 553 to ~80 lines). Access via `AppState::getInstance()` or the `appState` macro. All state lives in `appState` — domain modules include only the headers they need.

**Domain State Headers** (15 total):
- `src/state/enums.h` — `AppFSMState`, `FftWindowType`, `NetIfType` (shared enums)
- `src/state/general_state.h` — `GeneralState` (timezoneOffset, dstOffset, darkMode, enableCertValidation, deviceSerialNumber)
- `src/state/ota_state.h` — `OtaState` + `ReleaseInfo` struct
- `src/state/audio_state.h` — `AudioState` with `AdcState[]`, `I2sRuntimeMetrics`, **volatile `bool audioPaused`** (cross-core), sensing fields, pipeline bypass arrays
- `src/state/dac_state.h` — `DacState` (minimal): `txUnderruns`, `EepromDiag` only. Device fields (enabled, volume, mute, filterMode) live in `HalDeviceConfig` via HAL manager. Toggle queue moved to `HalCoordState`
- `src/state/dsp_state.h` — `DspSettingsState` (enabled, bypass, presets, swap diagnostics)
- `src/state/display_state.h` — `DisplayState`
- `src/state/buzzer_state.h` — `BuzzerState`
- `src/state/signal_gen_state.h` — `SignalGenState`
- `src/state/usb_audio_state.h` — `UsbAudioState` (guarded by `USB_AUDIO_ENABLED`)
- `src/state/wifi_state.h` — `WifiState` (SSID, password, AP mode, connection)
- `src/state/mqtt_state.h` — `MqttState` (broker config, connection state, `useTls`, `verifyCert`)
- `src/state/ethernet_state.h` — `EthernetState`
- `src/state/debug_state.h` — `DebugState` (debug mode, serial level, hw stats, heapCritical)
- `src/state/hal_coord_state.h` — `HalCoordState`: deferred device toggle queue (capacity 8, same-slot dedup). `requestDeviceToggle(halSlot, action)` enqueues enable/disable for ANY device type. Main loop consumes via `hasPendingToggles()` / `pendingToggleAt(i)` / `clearPendingToggles()`. Overflow telemetry: `_overflowCount` (lifetime counter) + `consumeOverflowFlag()` (one-shot, triggers `DIAG_HAL_TOGGLE_OVERFLOW` diagnostic). All 6 callers check return value and LOG_W on failure; REST endpoints return HTTP 503

**Usage pattern**: Access domain state via nested composition: `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`, `appState.general.darkMode`, `appState.dsp.enabled`, `appState.halCoord.requestDeviceToggle()`, etc. DAC device state (enabled, volume, mute, filterMode) lives in `HalDeviceConfig` via HAL manager — not in DacState. Dirty flags and event signaling remain in AppState shell for backward compat.

**Cross-domain coordination**: AppState retains `volatile bool _mqttReconfigPending`, `volatile int8_t _pendingApToggle`, backoff delays, and `HalCoordState halCoord` (deferred device toggle queue) — inherently cross-cutting concerns unrelated to a single domain.

AppState uses **dirty flags** (e.g., `isBuzzerDirty()`, `isDisplayDirty()`, `isOTADirty()`) to detect changes and minimize unnecessary WebSocket broadcasts and MQTT publishes. Every dirty-flag setter also calls `app_events_signal(EVT_XXX)` (defined in `src/app_events.h`) so the main loop can replace `delay(5)` with `app_events_wait(5)` — waking immediately on any state change and falling back to a 5 ms tick when idle. The event group uses 24 usable bits (`EVT_ANY = 0x00FFFFFF`; bits 24-31 are reserved by FreeRTOS). Currently 14 event bits are assigned (bits 0-15, with bits 5 and 13 freed) with 10 spare.

Legacy code uses `#define` macros (e.g., `#define wifiSSID appState.wifiSSID`) to alias global variable names to AppState members. New code should use `appState.fieldName` directly; domain lookups use `appState.domain.fieldName` (e.g., `appState.wifi.ssid`).

Change-detection shadow fields (`prevMqtt*`) have been extracted from AppState into file-local statics in `mqtt_publish.cpp`. Similarly, `prevBroadcast*` sensing fields live in `smart_sensing.cpp`. Device toggles use `appState.halCoord.requestDeviceToggle(halSlot, action)` — works for ANY device type (DAC, ADC, codec). The main loop drains the queue calling `dac_activate_for_hal()` / `dac_deactivate_for_hal()` per entry.

### FSM States
The application uses a finite state machine (`AppFSMState` in `app_state.h`): `STATE_IDLE`, `STATE_SIGNAL_DETECTED`, `STATE_AUTO_OFF_TIMER`, `STATE_WEB_CONFIG`, `STATE_OTA_UPDATE`, `STATE_ERROR`.

### Handler/Module Pattern
Each subsystem is a separate module in `src/`:
- **smart_sensing** — Voltage detection, auto-off timer, amplifier relay control. `setAmplifierState()` routes through `HalRelay::setEnabled()` when available (via `findByCompatible("generic,relay-amp")`), falling back to direct GPIO. `detectSignal()` rate matches `appState.audioUpdateRate` (not hardcoded) with dynamically scaled smoothing alpha to maintain ~308ms time constant
- **wifi_manager** — Multi-network WiFi client, AP mode, async connection with retry/backoff
- **mqtt_handler** — MQTT broker connection, settings load/save, callback dispatch, HA discovery. Split across 3 files: `mqtt_handler.cpp`, `mqtt_publish.cpp` (change-detection statics), `mqtt_ha_discovery.cpp`. `mqttCallback()` is thread-safe: all side-effects use dirty flags. TCP socket timeout capped at `MQTT_SOCKET_TIMEOUT_MS` (5000ms) via `mqttWifiClient.setTimeout()` in `setupMqtt()` to prevent 15-30s blocking on unreachable brokers. **TLS support**: Optional `WiFiClientSecure` transport toggled via `appState.mqtt.useTls` — heap-guarded (50KB minimum, 65KB for cert validation), with `verifyCert` toggle for self-signed broker support. `setClient()` switches PubSubClient between plaintext and secure transports at runtime. Reuses `GITHUB_ROOT_CA` from `ota_certs.h` for certificate validation. `mqtt_ha_discovery.cpp` includes generic HAL device iteration publishing `binary_sensor` availability entities for expansion devices without dedicated HA entities, so new mezzanine modules appear in Home Assistant automatically. Lives in `src/mqtt_handler.cpp`
- **mqtt_task** — Dedicated FreeRTOS task (Core 0, priority 2) owning MQTT reconnect + publish. Polls at 20 Hz; does **not** wait on event group. Checks `appState._mqttReconfigPending` for broker setting changes. Lives in `src/mqtt_task.cpp`
- **ota_updater** — GitHub release checking, firmware download with SHA256 verification. Root CA certs in `src/ota_certs.h` (Sectigo R46/E46, DigiCert G2). Update certs via `node tools/update_certs.js`
- **settings_manager** — Dual-format persistence: `/config.json` (primary, atomic write via tmp+rename) with legacy `settings.txt` fallback on first boot only. Export v2.0 includes HAL device configs, custom device schemas, DSP global/channel/output settings, and pipeline matrix. Import is version-aware (v1 base settings only, v2 includes everything). Web UI shows import preview with section checklist before applying. Factory reset. WiFi credentials survive LittleFS format via NVS. Lives in `src/settings_manager.cpp`
- **auth_handler** — Web password auth with PBKDF2-SHA256 (50k iterations, `p2:` format). Backward-compatible with legacy `p1:` (10k) and SHA256 hashes — auto-migrated on login. `PBKDF2_ITERATIONS` constant in `config.h`. First-boot random password on TFT/serial. Rate limiting returns HTTP 429. Cookie `HttpOnly`. WS auth via short-lived token from `GET /api/ws-token` (60s TTL, 16-slot pool). **Password change requires current password** (verified via `verifyPassword()`); first-boot exempted via `isDefaultPassword()`. Returns HTTP 403 on wrong current password. On successful change: `clearAllSessions()` + `ws_disconnect_all_clients()` force re-auth everywhere. **Global rate limiting** via `rate_limit_check()` inside `requireAuth()` — all 35+ REST endpoints rate-limited at 30 req/sec per IP (WS unaffected). Lives in `src/auth_handler.cpp`
- **http_security** — HTTP security header helper. `server_send()` wrapper automatically adds `X-Frame-Options: DENY`, `X-Content-Type-Options: nosniff`, and `Content-Security-Policy` (blocks object/frame-src, restricts connect-src to self+ws/wss) to all HTTP responses. All handler files use `server_send()` instead of `server.send()`. `sendGzipped()` applies headers independently for compressed HTML. `sanitize_filename()` validates user-provided filenames (rejects `/`, `\`, `..`, null bytes; alphanumeric + hyphen + underscore only; max 32 chars) — used by DSP preset and custom device endpoints. `json_response(server, code, data, error)` standardized REST envelope helper — wraps responses in `{"success":bool,"data":...,"error":"..."}`. `data` must be pre-serialized JSON; `error` is JSON-escaped. Lives in `src/http_security.h`
- **button_handler** — Debouncing, short/long/very-long press and multi-click detection
- **buzzer_handler** — Piezo buzzer with multi-pattern sequencer, ISR-safe encoder tick/click, volume control, FreeRTOS mutex for dual-core safety
- **audio_pipeline** — 8-lane input → per-input DSP → 32×32 routing matrix → per-output DSP → 16-slot sink dispatch. Float32 [-1.0,+1.0] internally. HAL-managed sources/sinks registered dynamically. Slot-indexed APIs under `ScopedSchedulerSuspend` RAII guard (Core 1). `audio_pipeline_get_source(lane)` for dynamic input discovery. **Never calls `i2s_configure_adc1()` in the task loop** — MCLK must remain continuous for PCM1808 PLL stability. Compile-time `static_assert` enforces `MAX_INPUTS*2 <= MATRIX_SIZE` and `MAX_SINKS*2 <= MATRIX_SIZE`. `set_sink()` validates `firstChannel + channelCount <= MATRIX_SIZE`; `set_source()` validates `lane*2+1 < MATRIX_SIZE`. Both return `bool` (false on validation/allocation failure). DMA buffers (16 × 2KB = 32KB internal SRAM) are eagerly pre-allocated in `audio_pipeline_init()` via `heap_caps_calloc(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)` before WiFi connects. `DIAG_AUDIO_DMA_ALLOC_FAIL` (0x200E) emitted on failure; `AudioState.dmaAllocFailed` + bitmask track affected lanes/slots. HAL driver `buildSink()` methods guard `firstChannel` overflow. `PipelineTimingMetrics` includes end-to-end latency fields: `inputReadUs`, `perInputDspUs`, `sinkWriteUs`, `totalE2eUs` (full input-through-sink). `totalFrameUs` remains backward compatible (matrix+outputDSP+sink only). Lives in `src/audio_pipeline.h/.cpp`
- **i2s_audio** — Port-generic I2S driver managing 3 I2S ports via unified `I2sPortState _port[3]` array. Public API: `i2s_port_enable_tx/rx()`, `i2s_port_disable_tx/rx()`, `i2s_port_write()`, `i2s_port_read()`, `i2s_port_tdm_read()`, `i2s_port_get_info()` (returns `I2sPortInfo` struct with format, bitDepth, mclkMultiple). Per-port `I2sPortConfig` struct configures data format (Philips/MSB/PCM short, default Philips), bit depth (16/24/32, default 32), and MCLK multiple (128-1152, default 256). 24-bit + non-3x MCLK multiple emits LOG_W (IDF5 constraint). PCM1808 FMT pin driven to match configured format (LOW=Philips, HIGH=left-justified). All 3 ports are equally configurable for STD or TDM mode, any direction (TX/RX), any pins. 19 legacy device-specific wrapper functions removed (all callers migrated to port-generic API). `i2s_audio_init()` delegates audio task creation to `audio_pipeline_init()`. Analysis shared state written by pipeline, read by consumers via `i2s_audio_get_analysis()`. Lives in `src/i2s_audio.h/.cpp`
- **i2s_port_api** — REST endpoints for I2S port configuration: `GET /api/i2s/ports` (all ports or `?id=N` single port). Returns mode (STD/TDM/off), direction (TX/RX), pins, clock master status, sample rate. Registered via `registerI2sPortApiEndpoints()`. Lives in `src/i2s_port_api.h/.cpp`
- **dsp_pipeline** — 4-channel audio DSP engine: biquad IIR, FIR, limiter, gain, delay, polarity, mute, compressor. Double-buffered config with glitch-free swap. ESP32 uses pre-built `libespressif__esp-dsp.a` (S3 assembly-optimized); native tests use `lib/esp_dsp_lite/` (ANSI C fallback, `lib_ignore`d on ESP32). Delay lines use PSRAM (`ps_calloc`) when available, with heap pre-flight check (40KB reserve) on fallback. `dsp_add_stage()` rolls back on pool exhaustion; config imports skip failed stages
- **dsp_coefficients** — RBJ Audio EQ Cookbook biquad coefficient computation via `dsp_gen_*` functions in `src/dsp_biquad_gen.h/.c` (renamed from `dsps_biquad_gen_*` to avoid symbol conflicts with pre-built ESP-DSP)
- **dsp_crossover** — Crossover presets (LR2/LR4/LR8, Butterworth), bass management
- **dsp_rew_parser** — Equalizer APO + miniDSP import/export, FIR text, WAV IR loading
- **dsp_api** — REST API endpoints for DSP config CRUD, persistence (LittleFS), debounced save
- **signal_generator** — Multi-waveform test signal generator (sine, square, noise, sweep), software injection (HAL-assigned dynamic lane via `HalSigGen`) + PWM output modes
- **task_monitor** — FreeRTOS task enumeration (stack usage, priority, core affinity). 5s timer in main loop, opt-in via `debugTaskMonitor`. Lives in `src/task_monitor.cpp`
- **usb_audio** — TinyUSB UAC2 speaker device on native USB OTG. HAL-managed via `HalUsbAudio` (`alx,usb-audio`). Lock-free ring buffer (1024 frames, PSRAM). FreeRTOS task on Core 0. Guarded by `-D USB_AUDIO_ENABLED`. Lives in `src/usb_audio.cpp`
- **debug_serial** — Log-level filtered serial output (`LOG_D`/`LOG_I`/`LOG_W`/`LOG_E`/`LOG_NONE`), runtime level control via `applyDebugSerialLevel()`, WebSocket log forwarding. `broadcastLine()` sends `"module"` as a separate JSON field extracted from the `[ModuleName]` prefix, enabling frontend category filtering
- **hal_device_manager** — Singleton managing up to 32 HAL devices (`HAL_MAX_DEVICES=32`) with 56-pin tracking (`HAL_MAX_PINS=56`, covers all ESP32-P4 GPIOs 0–54). `HAL_GPIO_MAX=54` upper-bound validation rejects invalid GPIO numbers with `LOG_W`. Priority-sorted init (BUS=1000→LATE=100). Pin claim/release system prevents GPIO conflicts. Per-device `HalDeviceConfig` with I2C/I2S/GPIO overrides persisted to `/hal_config.json` (atomic write via tmp+rename). `HalDevice._lastError[48]` stores last init/probe failure reason — surfaced through REST API (`error` field), WebSocket (`errorReason` field + `faultCount` field), and web UI error banner. State change callback (`HalStateChangeCb`) fires on every `_state` transition — registered by `hal_pipeline_bridge` at boot. **Fault counter persistence**: per-device fault counters survive reboot via NVS namespace `"hal_faults"` (keys `"f00"`–`"f31"`). `loadFaultCounters()` restores at boot, `saveFaultCounter(slot)` persists on retry exhaustion, `clearFaultCounters()` resets all for field service. Lives in `src/hal/hal_device_manager.h/.cpp`
- **hal_pipeline_bridge** — Connects HAL device lifecycle to the audio pipeline. Owns sink + source registration/removal. Capability-based ordinal counting (`HAL_CAP_DAC_PATH` → output slot, `HAL_CAP_ADC_PATH` → input lane). Supports multi-source devices via `getInputSourceCount()`/`getInputSourceAt()` with `_halSlotAdcLaneCount[]` for consecutive-lane allocation (e.g., ES9843PRO registers 2 stereo pairs). Supports multi-sink devices via `getSinkCount()`/`buildSinkAt()` with `_halSlotSinkCount[]` for consecutive-sink-slot allocation (e.g., ES9038PRO registers 4 stereo pairs = 4 sink slots). Hybrid transient policy: UNAVAILABLE sets `_ready=false` only; DISABLED/ERROR/REMOVED removes sink/source. Lives in `src/hal/hal_pipeline_bridge.h/.cpp`
- **hal_device_db** — In-memory device database with builtin entries (PCM5102A, ES8311, PCM1808, NS4150B, TempSensor, SigGen, USB Audio, ES9822PRO, ES9843PRO) plus LittleFS JSON persistence. EEPROM v3 compatible string matching. Lives in `src/hal/hal_device_db.h/.cpp`
- **hal_discovery** — 3-tier device discovery: I2C bus scan → EEPROM probe → manual config. Bus scan covers address range 0x08–0x77. Skips Bus 0 (GPIO 48/54) when WiFi active (SDIO conflict) via `hal_wifi_sdio_active()` helper (checks `connectSuccess || connecting || activeInterface == NET_WIFI`); Bus 1 (ONBOARD) and Bus 2 (EXPANSION) always safe. Post-boot rescan via `POST /api/hal/scan` is **async** — returns 202 Accepted immediately and spawns a FreeRTOS task (Core 0, priority 1) for the actual scan. `Wire.setTimeout(200)` bounds per-address I2C probe to 200ms. `vTaskDelay(1)` every 8 addresses yields to the web server. Returns `partialScan` flag when Bus 0 is skipped. Emits `DIAG_HAL_I2C_BUS_CONFLICT` (0x1101) on Bus 0 skip. Targeted probe retry for I2C timeout addresses (error codes 4/5): up to `HAL_PROBE_RETRY_COUNT` (2) retries with `HAL_PROBE_RETRY_BACKOFF_MS` (50ms) increasing backoff. Emits `DIAG_HAL_PROBE_RETRY_OK` (0x1105) on retry success. Tracks unmatched I2C addresses (responded but no EEPROM/driver match) — exposed via `GET /api/hal/scan/unmatched` for the custom device creator UI. Lives in `src/hal/hal_discovery.h/.cpp`
- **hal_i2c_bus** — Singleton I2C bus abstraction (3 instances, one per bus). Per-bus recursive FreeRTOS mutex for thread safety. `HalI2cBus::get(busIndex)` returns bus reference. Provides `writeReg()`, `readReg()`, `writeReg16()`, `writeRegPaged()`, `readRegPaged()`, `probe()`, `writeBytes()`, `readBytes()`. SDIO guard via `isSdioBlocked()` for Bus 0. All HAL drivers and discovery use this instead of raw Wire. Lives in `src/hal/hal_i2c_bus.h/.cpp`
- **hal_custom_device** — User-defined device schemas loaded from `/hal/custom/*.json`. Three-tier support: Tier 1 (I2S passthrough, no I2C), Tier 2 (I2C init register sequence + I2S passthrough), Tier 3 (full C++ driver via hal-driver-scaffold). Real I2C probe with Bus 0 SDIO guard. Init sequence executes up to 32 `{reg,val}` pairs during `init()`. I2S audio passthrough via `buildSink()`/`getInputSource()` using `sink_write_utils.h`. Web UI creator modal with scan-assisted I2C address discovery and "Submit to ALX" community contribution flow. `POST /api/hal/devices/custom/create` for structured creation. Lives in `src/hal/hal_custom_device.h/.cpp`
- **hal_api** — REST endpoints for HAL device CRUD: `GET /api/hal/devices`, `POST /api/hal/scan`, `PUT /api/hal/devices` (config update), `DELETE /api/hal/devices` (remove), `POST /api/hal/devices/reinit`, `GET /api/hal/db`, `GET /api/hal/db/presets`, `POST /api/hal/devices/faults/clear` (reset all fault counters). All 16 endpoints are auth-guarded via `requireAuth()` (session cookie + 30 req/sec rate limit per IP). Registered via `registerHalApiEndpoints()`. Lives in `src/hal/hal_api.h/.cpp`
- **hal_builtin_devices** — Driver registry with compatible string → factory function mapping. Uses `HAL_REGISTER()` macro to reduce each registration to a one-liner. Registers PCM5102A, ES8311, PCM1808, DSP bridge, NS4150B, TempSensor, SigGen (`alx,signal-gen`), USB Audio (`alx,usb-audio`), MCP4725, HalLed (`generic,status-led`), HalRelay (`generic,relay-amp`), HalBuzzer, HalButton, HalEncoder, all 9 ESS SABRE ADC expansion drivers, 5 ESS SABRE 2ch DAC expansion drivers (Pattern C), 7 ESS SABRE 8ch DAC expansion drivers (Pattern D, +1 MPRO variant alias), and 5 Cirrus Logic 2ch DAC expansion drivers (Pattern C, `HalCirrusDac2ch` descriptor-driven). Lives in `src/hal/hal_builtin_devices.h/.cpp`
- **hal_ess_sabre_adc_base** — Abstract base class for all 9 ESS SABRE ADC family drivers. Delegates I2C to `HalI2cBus` via protected `_bus()` helper. Config override reading (`_applyConfigOverrides`), sample rate validation (`_validateSampleRate()`), and protected member fields. All 2ch I2S (Pattern A) and 4ch TDM (Pattern B) expansion ADC drivers inherit from this class. Lives in `src/hal/hal_ess_sabre_adc_base.h/.cpp`
- **hal_ess_adc_2ch** — Generic ESS SABRE 2-channel ADC driver (Pattern A). Replaces 5 individual drivers (ES9822PRO, ES9826, ES9823PRO/MPRO, ES9821, ES9820) with a single `HalEssAdc2ch` class driven by per-chip `EssAdc2chDescriptor` tables. 16-bit volume, per-chip gain type (NONE/2BIT/3BIT/NIBBLE), HPF, 8 filter presets. ES9823PRO/MPRO variant auto-detection via `altChipId`. Compatible: all 5 `ess,es98*` 2ch ADC strings. Lives in `src/hal/hal_ess_adc_2ch.h/.cpp`
- **hal_ess_adc_4ch** — Generic ESS SABRE 4-channel ADC driver (Pattern B). Replaces 4 individual drivers (ES9843PRO, ES9842PRO, ES9841, ES9840) with a single `HalEssAdc4ch` class driven by per-chip `EssAdc4chDescriptor` tables. 8-bit per-channel volume (normal or inverted encoding for ES9841), TDM output via `HalTdmDeinterleaver`, registers 2 AudioInputSources per device. Compatible: all 4 `ess,es98*` 4ch ADC strings. Lives in `src/hal/hal_ess_adc_4ch.h/.cpp`
- **hal_ess_sabre_dac_base** — Abstract base class for ESS SABRE DAC family expansion drivers. Delegates I2C to `HalI2cBus` via protected `_bus()` helper. Config override reading (`_applyConfigOverrides`), expansion I2S TX lifecycle via port-generic `i2s_port_enable_tx()`/`i2s_port_disable_tx()`, and shared `buildSink()` implementation with static dispatch table. All DAC drivers use `i2s_port_write()` with port from `HalDeviceConfig.i2sPort`. Lives in `src/hal/hal_ess_sabre_dac_base.h/.cpp`
- **hal_ess_dac_2ch** — Generic ESS SABRE 2-channel DAC driver (Pattern C). Replaces 5 individual drivers (ES9038Q2M, ES9039Q2M, ES9069Q, ES9033Q, ES9020) with a single `HalEssDac2ch` class driven by per-chip `EssDac2chDescriptor` tables. Chip-specific features (MQA, line driver, APLL) via generic `setFeatureEnabled()`/`isFeatureActive()`. Compatible: all 5 `ess,es90*` 2ch DAC strings. Lives in `src/hal/hal_ess_dac_2ch.h/.cpp`
- **hal_ess_dac_8ch** — Generic ESS SABRE 8-channel DAC driver (Pattern D). Replaces 7 individual drivers (ES9038PRO, ES9028PRO, ES9039PRO/MPRO, ES9027PRO, ES9081, ES9082, ES9017) with a single `HalEssDac8ch` class driven by per-chip `EssDac8chDescriptor` tables. 8-bit per-channel volume (regs 0x0F-0x16), 7 filter presets, DPLL, TDM input. Registers 4 AudioOutputSinks via `HalTdmInterleaver`. ES9039PRO/MPRO variant auto-detection. Compatible: all 7 `ess,es90*` 8ch DAC strings. Lives in `src/hal/hal_ess_dac_8ch.h/.cpp`
- **hal_cirrus_dac_base** — Abstract base class for Cirrus Logic MasterHIFI DAC expansion drivers. Delegates I2C to `HalI2cBus` via protected `_bus()` helper (8-bit and 16-bit paged registers). Config override reading (`_applyConfigOverrides`), expansion I2S TX lifecycle, and shared `buildSink()` implementation with static dispatch table. Lives in `src/hal/hal_cirrus_dac_base.h/.cpp`
- **hal_cirrus_dac_2ch** — Generic Cirrus Logic 2-channel DAC driver (Pattern C). Replaces 5 individual drivers (CS43198, CS43131, CS4398, CS4399, CS43130) with a single `HalCirrusDac2ch` class driven by per-chip `CirrusDac2chDescriptor` tables. Supports 8-bit (CS4398) and 16-bit paged (all others) register addressing via `CirrusRegType` enum. Optional `setHeadphoneAmpEnabled()` (CS43131, CS43130) and `setNosMode()` (CS4399, CS43130) return false on unsupported chips. Compatible: all 5 `cirrus,cs4*` strings. Lives in `src/hal/hal_cirrus_dac_2ch.h/.cpp`
- **hal_tdm_interleaver** — TDM frame combiner for multi-channel DACs. Ping-pong buffers combine 4 stereo sink pairs into 8-slot TDM frames for I2S TX via `i2s_port_write(_i2sPort, ...)`. "Last writer flushes" design (pair 3 triggers DMA write). Supports 2 concurrent instances. Thread-safe on Core 1. Lives in `src/hal/hal_tdm_interleaver.h/.cpp`
- **hal_tdm_deinterleaver** — TDM frame splitter for multi-channel ADCs. Ping-pong buffers split 4-slot TDM frames into stereo pairs for pipeline via `i2s_port_tdm_read(_i2sPort, ...)`. Supports 2 concurrent instances (keyed by I2S port index) for multi-4ch-device scenarios. Thread-safe on Core 1 via atomic write-index swap. Lives in `src/hal/hal_tdm_deinterleaver.h/.cpp`
- **hal_temp_sensor** — ESP32-P4 internal chip temperature sensor using IDF5 `<driver/temperature_sensor.h>`. Range -10 to +80°C. Guarded by `CONFIG_IDF_TARGET_ESP32P4`. Lives in `src/hal/hal_temp_sensor.h/.cpp`
- **hal_ns4150b** — NS4150B class-D amplifier driver. GPIO-controlled enable/disable on GPIO 53 (shared with ES8311 PA pin). Lives in `src/hal/hal_ns4150b.h/.cpp`
- **hal_relay** — GPIO relay control for amplifier. `setEnabled(bool)` API used by smart_sensing via `findByCompatible("generic,relay-amp")`. Lives in `src/hal/hal_relay.h/.cpp`
- **hal_eeprom** — Generic HAL EEPROM alias header. Wraps `dac_eeprom.h` for new code. `using HalEepromData = DacEepromData`. Lives in `src/hal/hal_eeprom.h`
- **hal_settings** — HAL config persistence: load/save per-device `HalDeviceConfig` to `/hal_config.json`. Lives in `src/hal/hal_settings.h/.cpp`
- **hal_audio_health_bridge** — Integrates audio pipeline health status with HAL device diagnostics. Lives in `src/hal/hal_audio_health_bridge.h/.cpp`
- **hal_i2s_bridge** — I2S TX bridge connecting HAL DAC devices to the I2S output path. Lives in `src/hal/hal_i2s_bridge.h/.cpp`
- **output_dsp** — Per-output mono DSP engine applied post-matrix/pre-sink. Biquad, gain, limiter, compressor, polarity, mute stages. Double-buffered config with glitch-free atomic swap. Lives in `src/output_dsp.h/.cpp`
- **thd_measurement** — THD+N measurement engine: starts signal generator at test frequency, averages multiple FFT frames, reports THD+N percentage/dB + per-harmonic levels. Guarded by `DSP_ENABLED`. Lives in `src/thd_measurement.h/.cpp`
- **dsp_convolution** — FIR convolution/IR processing engine with slot-based management. PSRAM allocation preferred. Lives in `src/dsp_convolution.h/.cpp`
- **pipeline_api** — REST API for audio pipeline matrix CRUD + per-output DSP config. Deferred matrix save with 2s debounce. Registered via `registerPipelineApiEndpoints()`. Lives in `src/pipeline_api.h/.cpp`
- **hal_eeprom_api** — REST endpoints for EEPROM programming and diagnostics: GET/POST /api/hal/eeprom, POST /api/hal/eeprom/erase, POST /api/hal/eeprom/scan, GET /api/hal/eeprom/presets. Auth-guarded. Registered via `registerHalEepromApiEndpoints()`. Lives in `src/hal/hal_eeprom_api.h/.cpp`
- **diag_api** — REST API for diagnostics: `GET /api/diagnostics`, `GET/DELETE /api/diagnostics/journal`, `GET /api/diag/snapshot`. Registered via `registerDiagApiEndpoints()`. Lives in `src/diag_api.h/.cpp`
- **siggen_api** — REST API for signal generator configuration: `GET/POST /api/signalgenerator`. Registered via `registerSignalGenApiEndpoints()`. Lives in `src/siggen_api.h/.cpp`
- **sink_write_utils** — Shared float buffer utilities for all HAL sink drivers: `sink_apply_volume()`, `sink_apply_mute_ramp()` (click-free ramping), `sink_float_to_i2s_int32()` (left-justified conversion for I2S DMA). Lives in `src/sink_write_utils.h/.cpp`
- **psram_alloc** — Unified PSRAM allocation wrapper with SRAM fallback. `psram_alloc(count, size, label)` attempts PSRAM first, falls back to SRAM with automatic `heap_budget` recording and `DIAG_SYS_PSRAM_ALLOC_FAIL` emission. SRAM fallback is capped at 64KB total (`PSRAM_SRAM_FALLBACK_MAX`) — refuses allocation and emits diagnostic when exceeded. `psram_free(ptr, label)` cleans up. `psram_get_stats()` returns lifetime fallback/failure counts and active byte totals. All ~10 PSRAM allocation sites across the codebase use this wrapper. Lives in `src/psram_alloc.h/.cpp`
- **psram_api** — REST endpoint `GET /api/psram/status` returning PSRAM health: total, free, usagePercent, fallback/failure counts, warning/critical flags, per-subsystem budget array. Auth-guarded. Registered via `registerPsramApiEndpoints()`. Lives in `src/psram_api.h/.cpp`
- **health_check** — On-device self-diagnostics: 9 check categories (system, I2C, HAL, I2S, network, MQTT, tasks, storage, audio). Two-phase boot: immediate checks at end of setup(), deferred checks 30s later via flag+poll. Serial output via `LOG_I`/`LOG_W` with `[Health]` prefix. REST: `GET /api/health` (auth required, rate limited). WS broadcast: `healthCheckState` (summary only). Serial command: `HEALTH_CHECK\n`. Lives in `src/health_check.h/.cpp`, REST in `src/health_check_api.cpp`
- **heap_budget** — Lightweight per-subsystem allocation tracker. Fixed-size array of 32 `HeapBudgetEntry` structs (label, bytes, isPsram). `heap_budget_record()` adds/updates, `heap_budget_remove()` removes entries. No dynamic allocation. Records audio pipeline, DSP, and other subsystem allocations. Exposed via WS `hardwareStats` and REST `/api/diag/snapshot`. Lives in `src/heap_budget.h/.cpp`
- **websocket subsystem** — Real-time state broadcasting to web clients (port 81, `MAX_WS_CLIENTS=16`). Split across 4 focused files sharing the public API in `websocket_handler.h`:
  - `src/websocket_command.cpp` — WS event handler (`webSocketEvent()`), command dispatch, deferred init state drain
  - `src/websocket_broadcast.cpp` — 16 state broadcast functions (`sendDspState()`, `sendHardwareStats()`, etc.) + audio data streaming
  - `src/websocket_auth.cpp` — Client authentication tracking, session validation, auth count recalibration
  - `src/websocket_cpu_monitor.cpp` — FreeRTOS idle hook CPU usage measurement
  - `src/websocket_internal.h` — Cross-file accessor functions for shared state
  Token-based auth (60s TTL, 16-slot pool). After auth, server sends `{"type":"protocolVersion","version":"<WS_PROTOCOL_VERSION>"}` (`WS_PROTOCOL_VERSION` constant in `config.h`, currently `"1.0"`). Incoming text messages capped at 4096 bytes (DoS prevention). Binary frames for waveform (`WS_BIN_WAVEFORM=0x01`) and spectrum (`WS_BIN_SPECTRUM=0x02`); JSON for state. Client-count adaptive binary rate: skip factor 1→8 combined with heap pressure gate. Uses vendored arduinoWebSockets v2.7.3 (`lib/WebSockets/`)
- **web_pages** — Embedded HTML/CSS/JS served from the ESP32 (gzip-compressed in `web_pages_gz.cpp`). **IMPORTANT: Edit source files in `web_src/` — NOT `src/web_pages.cpp` (auto-generated). After ANY edit to `web_src/` files, run `node tools/build_web_assets.js` to regenerate `src/web_pages.cpp` and `src/web_pages_gz.cpp` before building firmware.**
  - `web_src/index.html` — HTML shell (body content, no inline CSS/JS)
  - `web_src/css/01-05-*.css` — CSS split by concern (variables, layout, components, canvas, responsive)
  - `web_src/js/01-29-*.js` — JS modules in load order (core, state, UI, audio, DSP, WiFi, settings, system, I2S ports)
  - `src/web_pages.cpp` and `src/web_pages_gz.cpp` are both auto-generated — do not edit manually
  - `web_src/js/29-i2s-ports.js` — I2S port configuration cards on the Devices tab showing TX/RX mode, pins, clock master status, sample rate
  - Debug Console: module/category chip filtering (auto-populated), search/highlight, entry count badges (red=errors, amber=warnings), sticky filters (localStorage), relative/absolute timestamp toggle. Card positioned below Debug Controls

### GUI (LVGL on TFT Display)
LVGL v9.4 + LovyanGFX on ST7735S 128x160 (landscape 160x128). Runs on **Core 0** via FreeRTOS `gui_task` (`TASK_CORE_GUI=0`) — moved off Core 1 to keep Core 1 exclusively for audio. All GUI code is guarded by `-D GUI_ENABLED`.

Key GUI modules in `src/gui/`:
- **gui_manager** — Init, FreeRTOS task, screen sleep/wake, dashboard refresh
- **gui_input** — ISR-driven rotary encoder (Gray code state machine)
- **gui_theme** — Orange accent theme, dark/light mode
- **gui_navigation** — Screen stack with push/pop and transition animations
- **screens/** — Desktop carousel, Home status, Control, WiFi, MQTT, Settings, Debug, Support, Boot animations, Keyboard, Value editor

### Web Server
HTTP server on port 80 with REST API endpoints under `/api/`. WebSocket server on port 81 for real-time updates. API endpoints are registered in `main.cpp`. Includes `GET /api/i2s/ports` (all ports or `?id=N` single port) for I2S port status. Full REST API reference: `docs-site/docs/developer/api/`. WebSocket protocol: `docs-site/docs/developer/websocket.md`.

### HAL Framework (Hardware Abstraction Layer)
Device model in `src/hal/` with lifecycle management, discovery, and configuration. Guarded by `-D DAC_ENABLED`. Driver constructors use `hal_init_descriptor()` (inline helper in `hal_types.h`) to populate `_descriptor` in a single call. All string fields use `hal_safe_strcpy()` (guaranteed null termination) instead of raw `strncpy()`. The `HAL_REGISTER()` macro in `hal_builtin_devices.cpp` similarly reduces each driver registration to a one-liner.

**Device lifecycle**: UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ⇄ UNAVAILABLE → ERROR / REMOVED / DISABLED. Cross-core state uses atomic accessors: `setReady(bool)` / `isReady()` with `__ATOMIC_RELEASE`/`__ATOMIC_ACQUIRE` (RISC-V memory barriers on ESP32-P4). **All drivers must use `setReady()` — never raw `_ready =`**. Manager sets `_ready` BEFORE `_state = AVAILABLE` (ordering guarantee for callback). ERROR→ERROR transitions always fire callback (no skip). `deinit()` called before retry when CONFIGURING (double-init guard). State changes fire `HalStateChangeCb` → `hal_pipeline_bridge` → slot-indexed sink set/remove + dirty flags. Pipeline bridge verifies sink registration after activation (sets `ready=false` on DMA alloc failure).

**Config validation**: `hal_validate_config()` in `hal_types.h` validates all `HalDeviceConfig` fields before applying. `i2sPort` must be 0-2 or 255 (auto). `i2cBusIndex` must be 0-2. GPIO pins must be -1 or 0-54. `PUT /api/hal/devices` returns HTTP 422 with field-specific error on invalid config.

**Capability bit flags** (`uint16_t`, was `uint8_t`): `HAL_CAP_HW_VOLUME` (0), `HAL_CAP_FILTERS` (1), `HAL_CAP_MUTE` (2), `HAL_CAP_ADC_PATH` (3), `HAL_CAP_DAC_PATH` (4), `HAL_CAP_PGA_CONTROL` (5), `HAL_CAP_HPF_CONTROL` (6), `HAL_CAP_CODEC` (7), `HAL_CAP_MQA` (8), `HAL_CAP_LINE_DRIVER` (9), `HAL_CAP_APLL` (10), `HAL_CAP_DSD` (11), `HAL_CAP_HP_AMP` (12). Sample rate mask includes `HAL_RATE_384K` (bit 6) and `HAL_RATE_768K` (bit 7).

**Registered onboard devices**: PCM5102A (DAC, I2S), ES8311 (Codec, I2C bus 1), PCM1808 x2 (ADC, I2S), NS4150B (Amp, GPIO 53), Chip Temperature Sensor (Internal), Signal Generator (`alx,signal-gen`, software ADC), USB Audio (`alx,usb-audio`, software ADC, guarded by `USB_AUDIO_ENABLED`), MCP4725 (DAC, I2C), HalLed (`generic,status-led`, GPIO, `setOn(bool)` API), HalRelay (`generic,relay-amp`, GPIO, wired factory function).

**Registered expansion devices** (mezzanine add-on modules): Full ESS Technology SABRE ADC and DAC families plus Cirrus Logic MasterHIFI DAC family — 26 expansion devices total (9 ESS ADC + 12 ESS DAC + 5 Cirrus DAC) auto-discovered via EEPROM on I2C Bus 2. ADC + DAC mezzanines can be installed simultaneously (dual mezzanine, different EEPROM addresses + I2C control addresses). Discovery iterates all EEPROM addresses 0x50-0x57. ADC I2C addresses: 0x40 range. DAC I2C addresses: 0x48 range. All bus/port/pin assignments are fully configurable via `HalDeviceConfig` — no hardware is tied to a specific I2S or I2C port. HAL drivers use `i2s_port_*()` functions with port index from `HalDeviceConfig.i2sPort`. Expansion I2S TX (both STD and TDM modes) is fully implemented. Each mezzanine connector can host either an ADC or a DAC module (3+ expansion connectors, each with dedicated I2C bus).

**Expansion ADCs** (9 devices, base class `HalEssSabreAdcBase`): 2ch I2S (Pattern A): ES9822PRO, ES9826, ES9823PRO/ES9823MPRO, ES9821, ES9820. 4ch TDM (Pattern B, via `HalTdmDeinterleaver`): ES9843PRO, ES9842PRO, ES9841, ES9840.

**Expansion DACs — 2ch** (5 devices, Pattern C, base class `HalEssSabreDacBase`): ES9038Q2M (128dB, ultra-low power), ES9039Q2M (130dB, Hyperstream IV flagship), ES9069Q (130dB + MQA), ES9033Q (122dB, integrated line drivers), ES9020 (122dB, APLL clock recovery). All 2ch stereo with I2C volume/mute/filter control and `buildSink()` integration. Expansion I2S TX via `i2s_port_enable_tx()`/`i2s_port_write()` (fully implemented). Capability flags: `HAL_CAP_MQA` (ES9069Q), `HAL_CAP_LINE_DRIVER` (ES9033Q), `HAL_CAP_APLL` (ES9020).

**Expansion DACs — 8ch** (7 devices, Pattern D, base class `HalEssSabreDacBase` + `HalTdmInterleaver`): ES9038PRO (132dB, HyperStream II flagship), ES9028PRO (124dB, HyperStream II), ES9039PRO/ES9039MPRO (132dB, HyperStream IV flagship, auto-detect variant), ES9027PRO (124dB, HyperStream IV), ES9081 (120dB, 40-QFN), ES9082 (120dB, 48-QFN, ASP2/SIS), ES9017 (120dB, ES9027PRO pin-compatible). Each registers 4 stereo sink pairs via `getSinkCount()`/`buildSinkAt()` + `HalTdmInterleaver`. TDM output on expansion I2S TX via `i2s_port_write()` (fully implemented). Per-channel 8-bit volume (regs 0x0F-0x16), 7 filter presets, DPLL bandwidth.

**Expansion DACs — 2ch Cirrus Logic** (5 devices, Pattern C, generic driver `HalCirrusDac2ch`): CS43198 (130dBA, MasterHIFI 7-filter flagship, DSD256), CS43131 (127dB, DAC + integrated headphone amp, DSD256, `HAL_CAP_HP_AMP`), CS4398 (120dB, classic legacy, 8-bit regs, max 192kHz/24-bit), CS4399 (130dBA, 5-filter NOS mode, `setNosMode()`), CS43130 (130dB, HP amp + NOS mode + DSD128). All 2ch stereo with I2C control, 128-step volume (0.5dB), and `buildSink()` integration. I2C address 0x48 (CS4398: 0x4C), I2C Bus 2. Capability flags: `HAL_CAP_HP_AMP` (CS43131, CS43130), `HAL_CAP_DSD` (CS43198, CS43131, CS43130). CS43131/CS43130/CS4399 use 16-bit paged register addressing (`REG_16BIT_PAGED`); CS43198/CS4398 use 8-bit addressing (`REG_8BIT`). Per-chip behavior fully described by `CirrusDac2chDescriptor` tables — no per-chip subclasses. Lives in `src/hal/hal_cirrus_dac_base.h/.cpp` + `src/hal/hal_cirrus_dac_2ch.h/.cpp`.

Shared ESS family constants in `src/drivers/ess_sabre_common.h`. Per-device ESS register definitions in `src/drivers/es9*_regs.h` files. Mezzanine connector standard: `docs-site/docs/developer/hal/mezzanine-connector.md`.

**REST API**: 16 endpoints via `registerHalApiEndpoints()`. All endpoints require `requireAuth()` (session cookie + 30 req/sec rate limit per IP). Core CRUD: GET/POST/PUT/DELETE on `/api/hal/devices`. Discovery: `POST /api/hal/scan` (async — returns 202, 409 guard), `POST /api/hal/devices/reinit`. Database: `GET /api/hal/db`, `GET /api/hal/db/presets`. Settings: `GET|PUT /api/hal/settings` (auto-discovery toggle). Custom schemas: GET/POST/DELETE on `/api/hal/devices/custom`, `POST /api/hal/devices/custom/create`. Unmatched addresses: `GET /api/hal/scan/unmatched`. Fault management: `POST /api/hal/devices/faults/clear`. Config persisted to `/hal_config.json`. Full reference: `docs-site/docs/developer/api/rest-hal.md`.

**I2C bus architecture** (ESP32-P4, all access via `HalI2cBus::get(busIndex)`):
- Bus 0 (EXT): GPIO 48/54 → **Wire1** — shares SDIO with WiFi, never access when WiFi active
- Bus 1 (ONBOARD): GPIO 7/8 → **Wire** — ES8311 dedicated, always safe
- Bus 2 (EXPANSION): GPIO 28/29 → **Wire2** — always safe

Note: Bus 0 maps to Wire1 (not Wire) because Wire is already used by ES8311 on Bus 1.

Detailed HAL documentation: `docs-site/docs/developer/hal/` (overview, device lifecycle, driver guide, built-in drivers).

### FreeRTOS Tasks
Concurrent tasks with configurable stack sizes and priorities defined in `src/config.h` (`TASK_STACK_SIZE_*`, `TASK_PRIORITY_*`, `I2S_DMA_BUF_COUNT`/`I2S_DMA_BUF_LEN`).

**Core assignment** (Core 1 is reserved for audio):
- **Core 1**: `loopTask` (Arduino main loop, priority 1) + `audio_pipeline_task` (priority 3, `TASK_CORE_AUDIO`). The audio task preempts the main loop during DMA processing then yields 2 ticks. No new tasks may be pinned to Core 1.
- **Core 0**: `gui_task` (`TASK_CORE_GUI=0`), `mqtt_task` (priority 2), `usb_audio_task` (priority 1), one-shot OTA tasks (`startOTACheckTask()` / `startOTADownloadTask()`).

**Main loop idle strategy**: `delay(5)` has been replaced with `app_events_wait(5)` (see `src/app_events.h`). The loop wakes in <1 µs whenever any dirty flag is set, or falls back to a 5 ms tick when idle — preserving all periodic timers unchanged.

**MQTT**: The main loop no longer calls `mqttLoop()` or any `publishMqtt*()` function. All MQTT work (reconnect, `mqttClient.loop()`, periodic HA publishes) runs inside `mqtt_task` on Core 0. The main loop dispatches dirty flags to WS broadcasts only; MQTT publishes happen independently at 20 Hz.

Cross-core communication uses dirty flags in AppState — GUI/OTA/MQTT tasks set flags, main loop handles WebSocket broadcasts. Two additional coordination flags in AppState: `volatile bool _mqttReconfigPending` (web UI broker change → mqtt_task reconnects) and `volatile int8_t _pendingApToggle` (MQTT command → main loop executes WiFi mode change).

Architecture diagram with core/task layout: `docs-site/docs/developer/architecture.md`. Audio pipeline details: `docs-site/docs/developer/audio-pipeline.md`. DSP system: `docs-site/docs/developer/dsp-system.md`.

**I2S driver safety**: The DAC module may uninstall/reinstall the I2S_NUM_0 driver at runtime (e.g., toggling DAC on/off). To prevent crashes with `audio_pipeline_task` calling `i2s_read()` concurrently, callers MUST use `audio_pipeline_request_pause(timeout_ms)` before driver teardown and `audio_pipeline_resume()` after. The helper sets `appState.audio.paused=true`, then takes the binary semaphore `appState.audio.taskPausedAck` with timeout — guaranteeing the audio task has exited `i2s_read()`. All 5 callers (hal_pipeline_bridge, hal_settings ×2, i2s_audio, ota_updater) use these helpers. `i2s_audio_set_sample_rate()` has a `wasPaused` guard to prevent double semaphore take when called from a caller that already paused. **Never set `appState.audio.paused` directly** — always use the helpers.

### Heap Safety & PSRAM
The Waveshare ESP32-P4-WiFi6-DEV-Kit has PSRAM. All PSRAM-preferred allocations use the unified `psram_alloc()` wrapper (`src/psram_alloc.h/.cpp`) which attempts PSRAM first, falls back to SRAM with automatic `heap_budget` recording and `DIAG_SYS_PSRAM_ALLOC_FAIL` emission. DMA buffers (16 × 2KB = 32KB) are eagerly pre-allocated from internal SRAM at boot via `heap_caps_calloc(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)` — before WiFi connects. Actual worst-case footprint: ~330KB PSRAM + 32KB internal SRAM.

**Graduated internal heap pressure** (3 states, checked every 30s in main loop):
- **Normal**: `maxAllocBlock >= HEAP_WARNING_THRESHOLD (50KB)` — all features active
- **Warning** (`heapWarning`): `maxAllocBlock < 50KB` — WS binary rate halved, DSP `add_stage()` logs warning, `diag_emit(DIAG_SYS_HEAP_WARNING)` fired
- **Critical** (`heapCritical`): `maxAllocBlock < 40KB` — DMA buffer allocation refused, WS binary data suppressed, DSP stages refused, OTA checks skipped

**Graduated PSRAM pressure** (3 states, checked every 30s alongside internal heap):
- **Normal**: `freePsram >= PSRAM_WARNING_THRESHOLD (1MB)` — all features active
- **Warning** (`psramWarning`): `freePsram < 1MB` — `diag_emit(DIAG_SYS_PSRAM_WARNING)` fired, WS + Web UI show warning
- **Critical** (`psramCritical`): `freePsram < 512KB` — DSP delay/convolution allocations refused, `diag_emit(DIAG_SYS_PSRAM_WARNING)` with ERROR severity

Threshold constants defined in `config.h`: `HEAP_WARNING_THRESHOLD` (50000), `HEAP_CRITICAL_THRESHOLD` (40000), `PSRAM_WARNING_THRESHOLD` (1048576), `PSRAM_CRITICAL_THRESHOLD` (524288). Each transition emits a diagnostic event and signals `EVT_HEAP_PRESSURE`.

**PSRAM allocation tracking** (`psram_alloc.h/.cpp`): Unified wrapper used by all ~10 allocation sites. `psram_get_stats()` returns lifetime fallback/failure counts. REST: `GET /api/psram/status`. WebSocket `hardwareStats` includes `psramFallbackCount`, `psramWarning`, `psramCritical`. Web UI: budget breakdown table in Hardware Stats, health banner in Health Dashboard.

**Heap budget tracker** (`heap_budget.h/.cpp`): Records per-subsystem allocation size and PSRAM vs SRAM classification. `heap_budget_remove()` supports cleanup on free. Exposed via WebSocket `hardwareStats` (`heapBudget` array, `heapBudgetPsram`, `heapBudgetSram`) and REST `/api/diag/snapshot`.

**Critical lesson**: WiFi RX buffers are dynamically allocated from internal SRAM heap. If free heap drops below ~40KB, incoming packets (ping, HTTP, WebSocket) are silently dropped while outgoing (MQTT publish) still works. Always ensure DSP/audio allocations use PSRAM or are guarded by heap checks.

## Pin Configuration

Defined as build flags in `platformio.ini` and with fallback defaults in `src/config.h`:
- Core: LED=1, Amplifier=27, Reset button=46, Buzzer=45
- TFT: MOSI=2, SCLK=3, CS=4, DC=5, RST=6, BL=26
- Encoder: A=32, B=33, SW=36
- I2S Audio ADC1: BCK=20, DOUT=23, LRC=21, MCLK=22
- I2S Audio ADC2: DOUT2=25 (shares BCK/LRC/MCLK with ADC1)
- I2S DAC TX: DOUT=24 (full-duplex on I2S0), DAC I2C: SDA=48, SCL=54
- Signal Generator PWM: GPIO 47
- ES8311 Onboard DAC I2C: SDA=7, SCL=8 (dedicated onboard bus, PA=53)
- NS4150B Amp Enable: GPIO 53 (shared with ES8311 PA pin)
- I2C Bus 2 (Expansion): SDA=28, SCL=29
- USB Audio: native USB OTG on P4 (TinyUSB UAC2 speaker device)

**I2C Bus 0 (GPIO 48/54) SDIO conflict**: These pins are shared with the ESP32-C6 WiFi SDIO interface. I2C transactions while WiFi is active cause `sdmmc_send_cmd` errors and MCU reset. HAL discovery skips this bus when WiFi is connected.

### I2S Port Architecture (Port-Generic API)

All 3 ESP32-P4 I2S ports are managed through a unified `I2sPortState _port[3]` array in `i2s_audio.cpp`. Each port is independently configurable for STD or TDM mode, TX or RX direction, with any pin assignment — controlled via `HalDeviceConfig.i2sPort`.

**Port-generic public API**: `i2s_port_enable_tx/rx(port, cfg)`, `i2s_port_disable_tx/rx(port)`, `i2s_port_write(port, buf, len)`, `i2s_port_read(port, buf, len)`, `i2s_port_tdm_read(port, buf, len)`, `i2s_port_get_info(port)` (returns `I2sPortInfo` with mode, direction, pins, clock master, sample rate, format, bitDepth, mclkMultiple). Per-port `I2sPortConfig` struct configures data format (`I2S_FMT_PHILIPS`/`MSB`/`PCM_SHORT`, default Philips), bit depth (16/24/32, default 32), and MCLK multiple (128-1152, default 256).

**Default onboard configuration**: Both PCM1808 ADCs share BCK/LRC/MCLK clock lines. Both I2S peripherals are configured as master RX — I2S_NUM_0 (ADC1) outputs BCK/WS/MCLK clocks, while I2S_NUM_1 (ADC2) has NO clock output (only data_in on GPIO25). Both derive from the same 160MHz D2CLK with identical divider chains, giving frequency-locked BCK. Init order: ADC2 first (clock follower), ADC1 second (clock master).

**Expansion TX**: Fully implemented for both STD (2ch DACs) and TDM (8ch DACs) modes. HAL DAC drivers call `i2s_port_enable_tx()` during init and `i2s_port_write()` / TDM interleaver flush for audio output. The previously-stub expansion TX functions are now deprecated in favor of the port-generic API.

DOUT2 uses `INPUT_PULLDOWN` so an unconnected pin reads zeros (→ NO_DATA) instead of floating noise (→ false OK). No GPIO matrix reconnection is needed since I2S1 uses internal clocking.

## Testing Conventions

### C++ Unit Tests (Unity, native platform)
- Tests use Arrange-Act-Assert pattern
- Each test file has a `setUp()` that resets state
- Mock headers in `test/test_mocks/` simulate Arduino functions (`millis()`, `analogRead()`, GPIO), WiFi, MQTT (`PubSubClient`), and NVS (`Preferences`)
- The `native` environment compiles with `-D UNIT_TEST -D NATIVE_TEST` flags — use these for conditional compilation
- `test_build_src = no` in platformio.ini means tests don't compile `src/` directly; they include specific headers and use mocks
- Each test module must be in its own directory to avoid duplicate `main`/`setUp`/`tearDown` symbols

### Browser / E2E Tests (Playwright)
Playwright browser tests live in `e2e/tests/` (302 tests across 50 specs). They verify the web UI against a mock Express server + Playwright `routeWebSocket()` interception — no real hardware needed. Full architecture: `docs-internal/testing-architecture.md`. Diagrams: `docs-internal/architecture/test-infrastructure.mmd`, `e2e-test-flow.mmd`, `test-coverage-map.mmd`. Plan: `docs-internal/planning/test-strategy.md`.

**Page Object Model (POM)**: 19 POM classes in `e2e/pages/` wrap DOM interactions for each tab/modal. New tests should use POM methods; `selectors.js` remains the source of truth for element IDs. Key POMs: `BasePage` (navigation, WS capture, assertions), `DevicesPage`, `AudioPage`/sub-views, `SettingsPage`, `NetworkPage`.

**Test helpers**: `ws-assertions.js` (WS command capture/verification), `fixture-factories.js` (13 programmatic fixture builders), `a11y-helpers.js` (axe-core wrapper).

**Test tagging**: Tags in `test.describe` titles (`@smoke`, `@ws`, `@api`, `@visual`, `@a11y`, `@hal`, `@audio`, `@settings`, `@error`). Filter via `npm run test:smoke`, `test:hal`, etc.

```bash
cd e2e
npm install                              # First time only
npx playwright install --with-deps chromium  # First time only
npx playwright test                      # Run all 302 browser tests
npx playwright test tests/auth.spec.js   # Run single spec
npx playwright test --headed             # Run with visible browser
npx playwright test --debug              # Debug mode with inspector
```

**Test infrastructure:**
- `e2e/mock-server/server.js` — Express server (port 3000) assembling HTML from `web_src/`
- `e2e/mock-server/assembler.js` — Replicates `tools/build_web_assets.js` HTML assembly
- `e2e/mock-server/ws-state.js` — Deterministic mock state singleton, reset between tests
- `e2e/mock-server/routes/*.js` — 14 Express route files matching firmware REST API
- `e2e/helpers/fixtures.js` — `connectedPage` fixture: session cookie + WS auth + initial state broadcasts + WS capture
- `e2e/helpers/ws-helpers.js` — `buildInitialState()`, `handleCommand()` (37 command types), binary frame builders
- `e2e/helpers/ws-assertions.js` — `expectWsCommand()`, `captureApiCall()` for WS/REST verification
- `e2e/helpers/fixture-factories.js` — 13 programmatic fixture builders for per-test overrides
- `e2e/helpers/a11y-helpers.js` — `expectNoViolations()` axe-core accessibility wrapper
- `e2e/helpers/selectors.js` — Reusable DOM selectors matching `web_src/index.html` IDs
- `e2e/pages/*.js` — 19 Page Object Model classes (BasePage + one per tab/modal)
- `e2e/fixtures/ws-messages/*.json` — 21 hand-crafted WS broadcast fixtures
- `e2e/fixtures/api-responses/*.json` — 20 deterministic REST response fixtures

**Key Playwright patterns:**
- WS interception: `page.routeWebSocket(/.*:81/, handler)` — uses `onMessage`/`onClose` (capital M/C)
- WS capture: `page.wsCapture[]` collects all outbound WS commands; verify with `expectWsCommand(page, type, fields)`
- Tab navigation in tests: `page.evaluate(() => switchTab('tabName'))` — avoids scroll issues with sidebar clicks
- CSS-hidden checkboxes: use `toBeChecked()` not `toBeVisible()` for toggle inputs styled with `label.switch`
- Strict mode: use `.first()` when a selector might match multiple elements
- POM usage: `const DevicesPage = require('../pages/DevicesPage'); const devices = new DevicesPage(page); await devices.open()`
- Visual regression: `await expect(element).toHaveScreenshot('name.png')` — element-level, `animations: 'disabled'`
- Accessibility: `const AxeBuilder = require('@axe-core/playwright').default; await new AxeBuilder({page}).analyze()`
- Test tags: `test.describe('@hal @smoke ...', ...)` — filter with `npx playwright test --grep @smoke`
- API capture: `const cap = captureApiCall(page, '/api/hal/scan', 'POST'); await cap.expectCalled()`

### On-Device Tests (pyserial + pytest)
Device tests live in `device_tests/` and verify firmware on real ESP32-P4 hardware via serial + network. They use plain pyserial for serial capture and Python requests for REST API smoke tests. Failures auto-create GitHub issues via `gh` CLI when `--create-issues` is passed.

```bash
cd device_tests
pip install -r requirements.txt                                      # First time only
pytest tests/ --device-port COM8 --device-ip <DEVICE_IP> -v          # Run against connected device
pytest tests/ --device-port COM8 --device-ip <DEVICE_IP> -m "not slow" -v  # Skip slow tests
pytest tests/test_hal_advanced.py --device-port COM8 --device-ip <DEVICE_IP> -v  # Single module
pytest tests/ --create-issues                                        # Enable GitHub issue creation
```

Available markers: `boot`, `health`, `hal`, `audio`, `network`, `mqtt`, `settings`, `reboot`, `slow`, `ws`, `wifi`, `ethernet`, `ota`, `dsp`, `sync`, `perf`, `stress`. Filter: `pytest -m "hal" -v`.

### Mandatory Test Coverage Rules

**Every code change MUST keep tests green.** Before completing any task:

1. **C++ firmware changes** (`src/`):
   - Run `pio test -e native -v` or use the `firmware-test-runner` agent
   - New modules need a test file in `test/test_<module>/`
   - Changed function signatures → update affected tests

2. **Web UI changes** (`web_src/`):
   - Run `cd e2e && npx playwright test` or use the `test-engineer` agent
   - New toggle/button/dropdown → add test verifying it sends the correct WS command
   - New WS broadcast type → add fixture JSON + test verifying DOM updates
   - New tab or section → add navigation + element presence tests
   - Changed element IDs → update `e2e/helpers/selectors.js` + affected specs
   - Removed features → remove corresponding tests + fixtures
   - New top-level JS declarations → add to `web_src/.eslintrc.json` globals

3. **WebSocket protocol changes** (`src/websocket_command.cpp`, `src/websocket_broadcast.cpp`):
   - Update `e2e/fixtures/ws-messages/` with new/changed message fixtures
   - Update `e2e/helpers/ws-helpers.js` `buildInitialState()` and `handleCommand()`
   - Update `e2e/mock-server/ws-state.js` if new state fields are added
   - Add Playwright test verifying the frontend handles the new message type

4. **REST API changes** (`src/main.cpp`, `src/hal/hal_api.cpp`, `src/diag_api.cpp`, `src/siggen_api.cpp`):
   - Update matching route in `e2e/mock-server/routes/*.js`
   - Update `e2e/fixtures/api-responses/` with new/changed response fixtures
   - Add Playwright test if the UI depends on the new endpoint

5. **On-device health check changes** (`src/health_check.cpp`, `device_tests/`):
   - New HAL driver → add expected device to health check's onboard device list
   - New REST API endpoint → add to `device_tests/tests/test_network.py` smoke test list
   - New FreeRTOS task → add to health check's expected task list
   - New diagnostic subsystem → add health check category if it has a verifiable state
   - Changed WebSocket protocol → update `device_tests/tests/test_network.py` WS verification
   - New I2S port usage → update `device_tests/tests/test_audio.py` expected port config
   - New state in AppState → consider adding health check if it has a pass/fail threshold
   - New MQTT topic → update `device_tests/tests/test_mqtt.py` if broker-dependent

### Agent Workflow for Test Maintenance

**Always verify tests after code changes.** Use specialised agents in parallel:

| Change Type | Agent(s) to Use | What They Do |
|---|---|---|
| C++ firmware only | `firmware-test-runner` | Runs `pio test -e native -v`, diagnoses failures |
| Web UI only | `test-engineer` or `test-writer` | Runs Playwright, fixes selectors, adds coverage |
| Both firmware + UI | Launch **both** agents in parallel | Full coverage verification |
| New HAL driver | `hal-driver-scaffold` → `firmware-test-runner` | Scaffold creates test module automatically |
| New web feature | `web-feature-scaffold` → `test-engineer` | Scaffold creates DOM, then add E2E tests |
| Bug investigation | `debugger` or `debug` agent | Root cause analysis with test reproduction |
| New feature with device impact | `firmware-test-runner` + update `device_tests/` | Verify health check covers new hardware state |

**Parallel execution pattern** — when changes span firmware and web UI, launch both in a single message:
```
Agent 1 (firmware-test-runner): "Run pio test -e native -v and report results"
Agent 2 (test-engineer): "Run cd e2e && npx playwright test and fix any failures"
```

### Static Analysis (CI-enforced)
- **ESLint** (`web_src/.eslintrc.json`): Lints all JS files with 380 globals for concatenated scope. Rules: `no-undef`, `no-redeclare`, `eqeqeq`. Run: `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`
- **cppcheck**: C++ static analysis on `src/` (excluding `src/gui/`). Run in CI only
- **find_dups.js** + **check_missing_fns.js**: Duplicate/missing declaration checks. Run: `node tools/find_dups.js && node tools/check_missing_fns.js`

### Quality Gates (CI/CD)
All 4 gates must pass before firmware build proceeds (parallel execution):
1. `cpp-tests` — `pio test -e native -v` (~3493 Unity tests across 122 modules)
2. `cpp-lint` — cppcheck on `src/`
3. `js-lint` — find_dups + check_missing_fns + ESLint
4. `e2e-tests` — Playwright browser tests (302 tests across 50 specs)

See `docs-internal/architecture/ci-quality-gates.mmd` for the pipeline flow diagram.

### Pre-commit Hooks
`.githooks/pre-commit` runs fast checks before every commit:
1. `node tools/find_dups.js` — duplicate JS declarations
2. `node tools/check_missing_fns.js` — undefined function references
3. ESLint on `web_src/js/`

Activate: `git config core.hooksPath .githooks`

## Current Gotchas

Active warnings that prevent common bugs:

- **I2C Bus 0 SDIO conflict**: GPIO 48/54 shared with WiFi SDIO — I2C transactions cause MCU reset. All I2C access must go through `HalI2cBus::get(busIndex)` which handles the SDIO guard automatically via `isSdioBlocked()`. HAL discovery skips Bus 0 via `hal_wifi_sdio_active()` (checks `connectSuccess`, `connecting`, AND `activeInterface`). `wifi_manager.cpp` sets `activeInterface = NET_WIFI` on connect and immediately clears `connectSuccess` + `activeInterface` in the `ARDUINO_EVENT_WIFI_STA_DISCONNECTED` handler — no 20s timeout needed
- **No logging in ISR/audio task**: `Serial.print` blocks on UART TX buffer full, starving DMA. Use dirty-flag pattern: task sets flag, main loop does Serial/WS output
- **Slot-indexed sink removal only**: Never call `audio_pipeline_clear_sinks()` from deinit paths — use `audio_pipeline_remove_sink(slot)` to remove only the specific device
- **I2S driver reinstall handshake**: Use `audio_pipeline_request_pause()`/`audio_pipeline_resume()` — never set `appState.audio.paused` directly. See FreeRTOS Tasks section
- **HAL slot capacity**: 14/32 onboard slots at boot + up to 2 expansion (ADC+DAC) = 16/32 (`HAL_MAX_DEVICES=32`). Driver registry: 44/48 (`HAL_MAX_DRIVERS=48`). Device DB: 44/48 (`HAL_DB_MAX_ENTRIES=48`). Pin tracking: `HAL_MAX_PINS=56` with `HAL_GPIO_MAX=54` validation
- **Board config**: Use `board=esp32-p4` (360MHz, ES variant). `esp32-p4_r3` (400MHz, rev.300) crashes at bootloader on Waveshare ESP32-P4-WiFi6-DEV-Kit ECO2 silicon. The `board_upload.flash_size=16MB` override handles the 4MB default mismatch
- **Heap 40KB reserve**: WiFi RX buffers need ~40KB free internal SRAM. DSP/audio allocations must use PSRAM or be heap-guarded
- **MCLK continuity**: Never call `i2s_configure_adc1()` in the audio task loop — MCLK must remain continuous for PCM1808 PLL stability
- **Capacity constants when adding features**: When adding new HAL drivers, event bits, or pipeline slots, check and bump the corresponding capacity constant. Compile-time `static_assert` guards exist for HAL driver/DB capacity and pipeline sizes:
  | Feature Added | Constants to Check |
  |---|---|
  | New HAL driver (`HAL_REGISTER()`) | `HAL_MAX_DRIVERS` (48), `HAL_DB_MAX_ENTRIES` (48), `HAL_BUILTIN_DRIVER_COUNT` in `hal_builtin_devices.cpp` |
  | New event bit | `EVT_*` in `app_events.h` (24 usable bits, 16 assigned, 8 spare) |
  | New pipeline sink/source | `AUDIO_PIPELINE_MAX_OUTPUTS` (16), `AUDIO_PIPELINE_MATRIX_SIZE` (32) |
  | New HAL device instance at boot | `HAL_MAX_DEVICES` (32, 14 onboard + expansion) |

## Commit Convention

```
feat: Add new feature
fix: Fix bug
docs: Update documentation
refactor: Code refactoring
test: Add/update tests
chore: Maintenance tasks
```

**IMPORTANT**: Never add `Co-Authored-By` trailers (e.g., `Co-Authored-By: Claude ...<noreply@anthropic.com>`) to commit messages. Commits should not contain any AI attribution lines.

## CI/CD

GitHub Actions (`.github/workflows/tests.yml`): 4 parallel quality gates (cpp-tests, cpp-lint, js-lint, e2e-tests) must all pass before firmware build. Triggers on push/PR to `main` and `develop` branches. A separate `release.yml` workflow runs the same 4 gates before release. Pipeline diagram: `docs-internal/architecture/ci-quality-gates.mmd`. Playwright HTML report uploaded as artifact on failure (14-day retention).

When CI/doc-coverage gates fail after a merge, fix them immediately in the same session rather than leaving them for later.

## Documentation Site (Docusaurus v3)

Public documentation site in `docs-site/` built with Docusaurus v3, deployed to GitHub Pages. 26 documentation pages across User Guide (9) and Developer Reference (17).

```bash
# Local development
cd docs-site && npm install && npm run build && npm run serve

# Design token sync (run after changing src/design_tokens.h)
node tools/extract_tokens.js

# API extraction from C++ source (informational — used by generate_docs.js)
node tools/extract_api.js
```

**Key files:**
- `docs-site/docusaurus.config.js` — Site config (Mermaid, local search, dark mode default, `routeBasePath: 'docs'`)
- `docs-site/sidebars.js` — userSidebar (9 items) + devSidebar (17 items)
- `docs-site/src/css/tokens.css` — Auto-generated from `src/design_tokens.h` via `tools/extract_tokens.js`
- `docs-site/src/pages/index.js` — Hero landing page with feature cards
- `tools/generate_docs.js` — Claude API orchestrator for CI doc regeneration
- `tools/detect_doc_changes.js` — Git diff → section mapping for incremental updates
- `tools/doc-mapping.json` — Source file → documentation section mapping (65 entries)
- `tools/prompts/` — Writing style templates (api-reference, user-guide, developer-guide, hal-driver)
- `.github/workflows/docs.yml` — CI: detect changes → generate docs → build → deploy to gh-pages

**Design token pipeline:** `src/design_tokens.h` → `tools/extract_tokens.js` → CSS for both web UI (`web_src/css/00-tokens.css`) and Docusaurus (`docs-site/src/css/tokens.css`). Changing theme colours in `design_tokens.h` propagates to TFT, web UI, AND documentation site.

**MDX compatibility:** Docusaurus uses MDX, which interprets `{variable}` as JSX expressions. Always escape curly braces in markdown text/tables: `\{variable\}`. Code blocks (``` `) are safe.

**Internal working docs:** Moved to `docs-internal/` (renamed from `docs/`). These are separate from the public Docusaurus site and may be replaced by it over time.

## Serial Debug Logging

All modules use `debug_serial.h` macros (`LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`) with consistent `[ModuleName]` prefixes:

| Module | Prefix | Notes |
|---|---|---|
| `smart_sensing` | `[Sensing]` | Mode changes, threshold, timer, amplifier state, ADC health transitions |
| `i2s_audio` | `[Audio]` | Init, sample rate changes, ADC detection changes. Periodic dump via `audio_periodic_dump()` from main loop |
| `signal_generator` | `[SigGen]` | Init, start/stop, PWM duty, param changes while active |
| `buzzer_handler` | `[Buzzer]` | Init, pattern start/complete, play requests (excludes tick/click to avoid noise) |
| `wifi_manager` | `[WiFi]` | Connection attempts, AP mode, scan results |
| `mqtt_handler` | `[MQTT]` | Connect/disconnect, HA discovery, publish errors |
| `ota_updater` | `[OTA]` | Version checks, download progress, verification |
| `settings_manager` | `[Settings]` | Load/save operations |
| `usb_audio` | `[USB Audio]` | Init, connect/disconnect, streaming start/stop, host volume/mute changes |
| `button_handler` | — | Logged from `main.cpp` (11 LOG calls covering all press types) |
| `gui_*` | `[GUI]` | Navigation, screen transitions, theme changes |
| `hal_*` modules | `[HAL]`, `[HAL Discovery]`, `[HAL DB]`, `[HAL API]` | Device lifecycle, discovery, config save/load |
| `output_dsp` | `[OutputDSP]` | Per-output DSP stage add/remove |
| `health_check` | `[Health]` | Boot checks (immediate + deferred), periodic re-check (60s), serial command trigger, REST endpoint |

When adding logging to new modules, follow these conventions:
- Use `LOG_I` for state transitions and significant events (start/stop, connect/disconnect, health changes)
- Use `LOG_D` for high-frequency operational details (pattern steps, param snapshots)
- Never log inside ISR paths or real-time FreeRTOS tasks (e.g., `audio_pipeline_task`) — `Serial.print` blocks when UART TX buffer fills, starving DMA and causing audio dropouts. Use dirty-flag pattern: task sets flag, main loop calls `audio_periodic_dump()` for actual Serial/WS output
- Log transitions, not repetitive state (use static `prev` variables to detect changes)
- **Log files**: Save all `.log` files (build output, test reports, serial captures) to the `logs/` directory — keep the project root clean

## Icons

All icons in the web UI **must** use inline SVG paths sourced from [Material Design Icons (MDI)](https://pictogrammers.com/library/mdi/). No external icon CDN or font library is loaded — the page is self-contained and must work offline.

**Standard pattern** (copy the SVG path from pictogrammers.com → select icon → SVG tab):

```html
<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true">
  <path d="<paste MDI path here>"/>
</svg>
```

- Use `fill="currentColor"` so the icon inherits its colour from CSS (`color` property)
- Set explicit `width`/`height` to match the surrounding context (18 px for inline text buttons, 24 px for standalone buttons)
- Always add `aria-hidden="true"` on decorative icons; add `aria-label` on icon-only interactive elements
- When generating icons in JavaScript strings, quote SVG attributes with double quotes and the outer JS string with single quotes (the two don't conflict)

**Reference icons already in use:**

| Icon | MDI name | Used for |
|------|----------|---------|
| ⓘ outline circle | `mdi-information-outline` | Release notes links |
| ✕ | `mdi-close` | Close/dismiss buttons, clear search |
| 💾 | `mdi-content-save` | Save preset button |
| ▲ | `mdi-chevron-up` | Sort ascending, move item up |
| ▼ | `mdi-chevron-down` | Sort descending, move item down |
| ✏ | `mdi-pencil` | Rename/edit action |
| ⚠ triangle | `mdi-alert` | Warning banners and modal titles |
| ✔ circle | `mdi-check-circle` | WiFi connection success status |
| ✕ circle | `mdi-close-circle` | WiFi connection error status |
| 🗑 | `mdi-delete` | Delete DSP stage / preset |

## Key Dependencies

- `ArduinoJson@^7.4.2` — JSON parsing throughout the codebase
- `WebSockets` — WebSocket server for real-time UI updates (vendored in `lib/WebSockets/`, no longer from lib_deps registry)
- `PubSubClient@^2.8` — MQTT client for Home Assistant integration
- `lvgl@^9.4` — GUI framework (guarded by `GUI_ENABLED`)
- `LovyanGFX@^1.2.0` — TFT display driver for ST7735S (replaced TFT_eSPI)
- `arduinoFFT@^2.0` — FFT spectrum analysis (**native tests only**; ESP32 uses pre-built ESP-DSP FFT)
- **ESP-DSP pre-built library** (`libespressif__esp-dsp.a`) — S3 assembly-optimized biquad IIR, FIR, Radix-4 FFT, window functions, vector math (mulc/mul/add), dot product, SNR/SFDR analysis. Include paths added via `-I` flags in `platformio.ini`. `lib/esp_dsp_lite/` provides ANSI C fallbacks for native tests only (`lib_ignore = esp_dsp_lite` in ESP32 envs)

## Workflow Rules

After completing code fixes or concern mitigations, ALWAYS update concerns.md/MEMORY.md to mark items as resolved before proceeding to commit/push/merge.

## Git Operations

After completing any implementation or fix task, automatically proceed to commit, push, create PR, merge to main, and delete the feature branch. Show the final commit hash on main. Do not wait for the user to ask for git operations separately.

When performing git operations, always:
1. Confirm which branch you're on
2. Show the commit hash after pushing
3. Delete the remote feature branch after merge unless told otherwise

## Firmware / ESP32

Never modify `platformio.ini` board configuration unless explicitly asked. Treat `board=` and `board_build` settings as protected fields.

## Multi-Agent Workflow Rules

After multi-agent workflows, scan for and clean up any tool artifacts (firecrawl files, skill temp files, cache files) before committing.
