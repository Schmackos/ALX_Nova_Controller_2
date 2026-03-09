# Codebase Structure

**Analysis Date:** 2026-03-09

## Directory Layout

```
ALX_Nova_Controller_2/
├── src/                        # Firmware source (ESP32-P4, Arduino/IDF5)
│   ├── main.cpp                # setup() + loop() — all HTTP routes registered here
│   ├── config.h                # Pin definitions, task config, feature flags (FIRMWARE_VERSION)
│   ├── app_state.h/.cpp        # AppState singleton shell (~80 lines), dirty flags, event signals
│   ├── app_events.h/.cpp       # FreeRTOS event group (16 bits assigned)
│   ├── globals.h               # extern declarations for global objects (server, webSocket, mqttClient)
│   ├── state/                  # 15 domain-specific state headers (AppState decomposition)
│   ├── hal/                    # HAL framework — device lifecycle, discovery, drivers
│   ├── gui/                    # LVGL TFT GUI (guarded by GUI_ENABLED)
│   ├── drivers/                # Low-level chip drivers (ES8311, PCM5102A)
│   └── [module].h/.cpp         # Feature modules (one .h + .cpp per subsystem)
├── test/                       # C++ Unity unit tests (native platform)
│   ├── test_mocks/             # Mock headers: Arduino, WiFi, PubSubClient, Preferences, I2S
│   └── test_<module>/          # One directory per test module (70 total)
├── web_src/                    # Web UI source (edit here, never src/web_pages.cpp)
│   ├── index.html              # HTML shell
│   ├── css/                    # 7 CSS files (00-tokens → 06-health-dashboard)
│   └── js/                     # 20 JS modules (01-core → 28-init), shared concatenated scope
├── e2e/                        # Playwright browser tests
│   ├── tests/                  # 19 spec files
│   ├── fixtures/               # WS message fixtures + API response fixtures
│   ├── helpers/                # fixtures.js, selectors.js, ws-helpers.js
│   └── mock-server/            # Express server + 12 route files (mirrors firmware REST API)
├── docs-internal/              # Internal working documents (architecture, planning, hardware)
├── docs-site/                  # Docusaurus v3 public documentation site
├── tools/                      # Build tooling (build_web_assets.js, find_dups.js, etc.)
├── lib/                        # Vendored libraries (WebSockets, esp_dsp_lite)
├── include/                    # PlatformIO global includes
├── logs/                       # Build output and test reports (gitignored)
├── .planning/                  # GSD planning documents
├── .githooks/                  # pre-commit hooks (find_dups, check_missing_fns, ESLint)
├── platformio.ini              # Build environments: esp32-p4, p4_hosted_update, native
├── partitions_ota.csv          # Flash partition layout with OTA support
└── CLAUDE.md                   # Project instructions for Claude
```

## Directory Purposes

**`src/state/` — AppState domain headers (15 total):**
- Purpose: Domain-specific state structs, composed into AppState singleton
- Contains: Plain C++ structs with in-class defaults, no methods except validated setters
- Key files:
  - `src/state/enums.h` — `AppFSMState`, `FftWindowType`, `NetIfType`
  - `src/state/audio_state.h` — `AudioState` with `AdcState[8]`, I2S metrics, pipeline bypass arrays, `volatile bool paused`
  - `src/state/dac_state.h` — `DacState` with `PendingDeviceToggle`, `EepromDiag`
  - `src/state/dsp_state.h` — `DspSettingsState`
  - `src/state/wifi_state.h` — `WifiState`
  - `src/state/mqtt_state.h` — `MqttState`
  - `src/state/debug_state.h` — `DebugState`
  - `src/state/general_state.h` — `GeneralState` (timezone, darkMode, deviceSerialNumber)
  - `src/state/ota_state.h`, `src/state/ethernet_state.h`, `src/state/display_state.h`, `src/state/buzzer_state.h`, `src/state/signal_gen_state.h`, `src/state/usb_audio_state.h`

**`src/hal/` — HAL framework:**
- Purpose: Device lifecycle management, discovery, driver registry, persistence
- Contains: 60+ files covering all device types
- Key files:
  - `src/hal/hal_device.h` — `HalDevice` abstract base with `probe()`, `init()`, `deinit()`, `healthCheck()`, `getInputSource()`
  - `src/hal/hal_types.h` — All enums, structs, constants (`HAL_MAX_DEVICES=24`, `HalDeviceConfig`, capability flags)
  - `src/hal/hal_device_manager.h/.cpp` — Meyers singleton, 24-slot registry, pin claim, state-change callback
  - `src/hal/hal_driver_registry.h/.cpp` — Compatible string → factory function map (`HAL_MAX_DRIVERS=16`)
  - `src/hal/hal_builtin_devices.h/.cpp` — `hal_register_builtins()` registers PCM5102A, ES8311, PCM1808×2, DSP bridge, NS4150B, TempSensor, SigGen, USB Audio, MCP4725
  - `src/hal/hal_pipeline_bridge.h/.cpp` — HAL state → audio pipeline slot/lane mapping
  - `src/hal/hal_discovery.h/.cpp` — 3-tier discovery (I2C scan, EEPROM probe, manual config)
  - `src/hal/hal_api.h/.cpp` — REST endpoints for HAL CRUD
  - `src/hal/hal_settings.h/.cpp` — `/hal_config.json` persistence
  - `src/hal/hal_device_db.h/.cpp` — In-memory device database + LittleFS persistence
  - `src/hal/hal_eeprom_v3.h/.cpp` — EEPROM v3 read/write (AT24C02 at 0x50-0x57)
  - `src/hal/hal_audio_health_bridge.h/.cpp` — ADC health → HAL state driver with flap guard
  - Driver files: `hal_pcm5102a.h/.cpp`, `hal_pcm1808.h/.cpp`, `hal_es8311.h/.cpp`, `hal_buzzer.h/.cpp`, `hal_button.h/.cpp`, `hal_encoder.h/.cpp`, `hal_ns4150b.h/.cpp`, `hal_siggen.h/.cpp`, `hal_signal_gen.h/.cpp`, `hal_usb_audio.h/.cpp`, `hal_mcp4725.h/.cpp`, `hal_temp_sensor.h/.cpp`, `hal_dac_adapter.h/.cpp`, `hal_dsp_bridge.h/.cpp`, `hal_display.h/.cpp`, `hal_led.h/.cpp`, `hal_relay.h/.cpp`, `hal_custom_device.h/.cpp`, `hal_i2s_bridge.h/.cpp`, `hal_audio_device.h`, `hal_audio_interfaces.h`, `hal_init_result.h`

**`src/gui/` — LVGL GUI (guarded by `GUI_ENABLED`):**
- Purpose: TFT display interface on ST7735S 160×128 via LovyanGFX
- Contains: Manager, navigation, input, theme, config, 16 screens
- Key files:
  - `src/gui/gui_manager.h/.cpp` — `gui_init()`, FreeRTOS `gui_task` on Core 0
  - `src/gui/gui_input.h/.cpp` — ISR-driven rotary encoder (Gray code state machine)
  - `src/gui/gui_navigation.h/.cpp` — Screen stack with push/pop, transition animations
  - `src/gui/gui_theme.h/.cpp` — Orange accent theme, dark/light mode
  - `src/gui/lgfx_config.h` — LovyanGFX SPI config (synchronous, no DMA on P4)
  - `src/gui/lv_conf.h` — LVGL v9.4 configuration
  - `src/gui/screens/` — 16 screen implementations: `scr_boot_anim`, `scr_desktop`, `scr_home`, `scr_control`, `scr_wifi`, `scr_mqtt`, `scr_settings`, `scr_debug`, `scr_support`, `scr_keyboard`, `scr_value_edit`, `scr_dsp`, `scr_menu`, `scr_siggen`, `scr_devices`

**`src/drivers/` — Low-level chip drivers:**
- Purpose: Register-level driver code for specific chips
- Key files: `src/drivers/dac_es8311.h/.cpp`, `src/drivers/dac_pcm5102.h/.cpp`, `src/drivers/es8311_regs.h`

**`test/test_mocks/` — Native test mock headers:**
- Purpose: Stubs for platform-specific APIs not available on host
- Contains: `Arduino.h`, WiFi mock, `PubSubClient` mock, `Preferences` mock, I2S stubs, LittleFS stub, FreeRTOS stubs, `mbedtls/` directory

**`test/test_<module>/` — One directory per test module:**
- Purpose: Each directory compiles independently with its own `main()`
- Contains: Exactly one `test_<module>.cpp` per directory
- Examples: `test/test_hal_core/`, `test/test_audio_pipeline/`, `test/test_dsp/`, `test/test_hal_bridge/`

**`web_src/` — Web UI source:**
- Purpose: Human-editable source that gets assembled and gzip-compressed into `src/web_pages_gz.cpp`
- Contains: `index.html` (HTML shell), `css/` (7 files in load order), `js/` (20 modules in load order)
- Key JS modules:
  - `web_src/js/01-core.js` — WebSocket connection, reconnect logic
  - `web_src/js/02-ws-router.js` — Message type dispatch
  - `web_src/js/03-app-state.js` — Client-side state mirroring
  - `web_src/js/04-shared-audio.js` — Dynamic `numInputLanes`, `resizeAudioArrays()`
  - `web_src/js/05-audio-tab.js` — Unified Audio tab (Inputs, Matrix, Outputs, SigGen sub-views)
  - `web_src/js/06-peq-overlay.js` — Full-screen PEQ/DSP overlays + frequency response graph
  - `web_src/js/15-hal-devices.js` — HAL devices panel
  - `web_src/js/27a-health-dashboard.js` — Health Dashboard (device grid, error counters, event log)
  - `web_src/js/28-init.js` — Page initialization and startup sequence

**`e2e/` — Playwright browser tests:**
- Purpose: End-to-end browser tests against mock Express server (no hardware needed)
- Key directories:
  - `e2e/tests/` — 19 spec files (auth, audio, WiFi, MQTT, settings, HAL, debug, OTA, etc.)
  - `e2e/fixtures/ws-messages/` — 15 hand-crafted WebSocket broadcast JSON fixtures
  - `e2e/fixtures/api-responses/` — 14 deterministic REST response fixtures
  - `e2e/mock-server/` — Express server (`server.js`), HTML assembler, 12 route files, WS state singleton
  - `e2e/helpers/` — `fixtures.js` (connectedPage with session + auth), `selectors.js`, `ws-helpers.js`

**`tools/` — Build and validation scripts:**
- Purpose: Code generation, quality checks, documentation tooling
- Key files:
  - `tools/build_web_assets.js` — Assembles `web_src/` → `src/web_pages.cpp` + `src/web_pages_gz.cpp` (run after any `web_src/` edit)
  - `tools/find_dups.js` — Detects duplicate `let`/`const` declarations across concatenated JS scope
  - `tools/check_missing_fns.js` — Detects undefined function references in JS
  - `tools/generate_docs.js`, `tools/extract_api.js`, `tools/extract_tokens.js` — Documentation generation
  - `tools/detect_doc_changes.js` — Used by `.github/workflows/docs.yml`
  - `tools/fix_riscv_toolchain.py` — PlatformIO pre-script for ESP32-P4 RISC-V toolchain

**`docs-internal/` — Internal working documentation:**
- Purpose: Architecture diagrams, planning documents, hardware notes, development guides
- Key subdirectories: `architecture/` (10 Mermaid diagrams), `planning/` (feature plans), `development/` (CI/CD guides), `hardware/`, `archive/`, `user/`

**`docs-site/` — Docusaurus v3 public documentation:**
- Purpose: Public-facing user and developer documentation site
- Key files: `docusaurus.config.js`, `sidebars.js`, `docs/` (user/ + developer/ sections)

**`lib/` — Vendored libraries:**
- Purpose: Libraries not available via PlatformIO registry or with local modifications
- Contains: `lib/WebSockets/` (vendored WebSocketsServer, ignored in native env), `lib/esp_dsp_lite/` (ANSI C DSP fallback for native tests, `lib_ignore`d in ESP32 env)

## Key File Locations

**Entry Points:**
- `src/main.cpp` — `setup()` and `loop()`, all HTTP route registrations, HAL device instantiation
- `src/config.h` — `FIRMWARE_VERSION`, all pin definitions, task priorities, pipeline dimensions
- `platformio.ini` — Build environments and all `-D` feature flags

**State Management:**
- `src/app_state.h` — AppState singleton shell + all dirty flags + event signaling methods
- `src/app_events.h` — All 16 event bit definitions + `app_events_wait()`
- `src/state/` — 15 domain headers (one per subsystem)

**Audio Pipeline Core:**
- `src/audio_pipeline.h/.cpp` — Pipeline init, matrix, source/sink slot API
- `src/audio_input_source.h` — `AudioInputSource` struct definition
- `src/audio_output_sink.h` — `AudioOutputSink` struct definition
- `src/i2s_audio.h/.cpp` — PCM1808 I2S ADC init, FFT/waveform analysis

**HAL Core:**
- `src/hal/hal_device.h` — `HalDevice` abstract base
- `src/hal/hal_types.h` — All HAL types, enums, `HalDeviceConfig`
- `src/hal/hal_device_manager.h/.cpp` — Singleton device registry
- `src/hal/hal_pipeline_bridge.h/.cpp` — HAL ↔ audio pipeline connector
- `src/hal/hal_builtin_devices.cpp` — `hal_register_builtins()` factory registrations

**Web UI Build:**
- `web_src/index.html` — HTML shell (edit here)
- `web_src/js/*.js` — JS modules (edit here)
- `web_src/css/*.css` — CSS files (edit here)
- `src/web_pages.cpp` — Auto-generated (do not edit)
- `src/web_pages_gz.cpp` — Auto-generated gzip (do not edit)
- `tools/build_web_assets.js` — Run after any `web_src/` change

**Testing:**
- `test/test_mocks/` — All mock headers for native builds
- `platformio.ini` `[env:native]` — Test environment with `-D UNIT_TEST -D NATIVE_TEST`

**Persistence Files (LittleFS, runtime):**
- `/config.json` — Primary settings (JSON v1+ format)
- `/hal_config.json` — Per-device HAL configuration overrides
- `/hal_auto_devices.json` — Auto-provisioned add-on devices
- `/dsp_config.json` — DSP pipeline configuration + presets
- `/matrix.json` — 16×16 routing matrix persistence
- `/diag_journal.bin` — Structured diagnostic event log (CRC32-guarded)

## Naming Conventions

**Files:**
- Source modules: `snake_case.h` / `snake_case.cpp` (e.g., `smart_sensing.cpp`, `mqtt_handler.cpp`)
- HAL drivers: `hal_<device_name>.h/.cpp` (e.g., `hal_pcm1808.h`, `hal_buzzer.cpp`)
- State headers: `<domain>_state.h` in `src/state/` (e.g., `audio_state.h`, `dac_state.h`)
- GUI screens: `scr_<name>.h/.cpp` in `src/gui/screens/` (e.g., `scr_home.cpp`)
- Test modules: `test_<module>/test_<module>.cpp` (e.g., `test_hal_core/test_hal_core.cpp`)
- Web JS modules: `NN-<name>.js` with two-digit numeric prefix for load order
- Web CSS: `NN-<concern>.css` with two-digit numeric prefix for load order

**Classes and Structs:**
- HAL device classes: `Hal` prefix + PascalCase (e.g., `HalPcm1808`, `HalBuzzer`, `HalEncoder`)
- State structs: PascalCase + `State` suffix (e.g., `AudioState`, `WifiState`, `DspSettingsState`)
- HAL types: `Hal` prefix + PascalCase (e.g., `HalDeviceConfig`, `HalDeviceManager`)
- Audio interfaces: C structs named `AudioInputSource`, `AudioOutputSink`

**Functions:**
- Module-scoped C functions: `<module>_<verb>` (e.g., `hal_discover_devices()`, `audio_pipeline_set_sink()`)
- HTTP handlers: `handle<Action>` (e.g., `handleLogin()`, `handleWiFiScan()`)
- WebSocket broadcast: `send<Domain>State()` (e.g., `sendHalDeviceState()`, `sendDacState()`)
- AppState dirty flags: `mark<Domain>Dirty()` / `is<Domain>Dirty()` / `clear<Domain>Dirty()`

**Defines:**
- Feature gates: `<FEATURE>_ENABLED` (e.g., `DAC_ENABLED`, `DSP_ENABLED`, `GUI_ENABLED`, `USB_AUDIO_ENABLED`)
- Pin defaults: `<SIGNAL>_PIN` all caps (e.g., `I2S_BCK_PIN`, `BUZZER_PIN`)
- HAL capability flags: `HAL_CAP_<NAME>` (e.g., `HAL_CAP_DAC_PATH`, `HAL_CAP_ADC_PATH`)
- Event bits: `EVT_<DOMAIN>` (e.g., `EVT_DAC`, `EVT_HAL_DEVICE`, `EVT_CHANNEL_MAP`)

## Where to Add New Code

**New HAL Device Driver:**
- Implementation: `src/hal/hal_<device>.h/.cpp` (inherit `HalDevice`, implement all 5 lifecycle methods)
- Registration: Add factory entry in `src/hal/hal_builtin_devices.cpp` → `hal_register_builtins()`
- Tests: `test/test_hal_<device>/test_hal_<device>.cpp` (follow `test/test_hal_pcm1808/` pattern)
- For audio input devices: override `getInputSource()` returning a pre-baked `AudioInputSource*`

**New Firmware Feature Module:**
- Implementation: `src/<feature>.h` + `src/<feature>.cpp`
- State: Add domain header `src/state/<feature>_state.h`, add member to `AppState` shell, add dirty-flag methods
- HTTP routes: Register in `src/main.cpp` `setup()` block (or create `registerXxxApiEndpoints()`)
- WS broadcast: Add `sendXxxState()` function in `src/websocket_handler.cpp`
- Tests: `test/test_<feature>/test_<feature>.cpp`

**New Web UI Feature:**
- HTML: Add elements to `web_src/index.html`
- JS: Add/edit modules in `web_src/js/` (check for duplicate `let`/`const` with `node tools/find_dups.js`)
- CSS: Add to appropriate `web_src/css/NN-*.css` file or new file
- After any edit: Run `node tools/build_web_assets.js` before building firmware
- E2E test: Add spec in `e2e/tests/<feature>.spec.js`; add fixtures in `e2e/fixtures/`; update `e2e/mock-server/routes/` if new REST endpoints added

**New State Domain:**
- Create `src/state/<domain>_state.h` with plain struct + in-class defaults
- Add member `DomainState domain;` to AppState in `src/app_state.h`
- Add dirty-flag methods in `src/app_state.h` + assign event bit in `src/app_events.h`

**New Test Module:**
- Create directory `test/test_<module>/`
- Create `test/test_<module>/test_<module>.cpp` with `setUp()`, `tearDown()`, `RUN_TEST()` calls
- Include needed headers from `test/test_mocks/` and `src/`
- Add module name to the list in `CLAUDE.md` after verification

**Utility Functions:**
- General utilities: `src/utils.h/.cpp`
- Design tokens (shared constants): `src/design_tokens.h`

## Special Directories

**`.planning/codebase/`:**
- Purpose: GSD codebase analysis documents (this file and peers)
- Generated: By `/gsd:map-codebase` command
- Committed: Yes

**`.planning/`:**
- Purpose: Phase plans, checklists, orchestration state for GSD workflow
- Generated: By GSD commands
- Committed: Yes

**`logs/`:**
- Purpose: Build output, test reports, serial captures
- Generated: By build tools and test runs
- Committed: No (gitignored)

**`src/web_pages.cpp` + `src/web_pages_gz.cpp`:**
- Purpose: Auto-generated from `web_src/` by `tools/build_web_assets.js`
- Generated: Yes — run `node tools/build_web_assets.js` after any `web_src/` edit
- Committed: Yes (pre-built to avoid Node.js requirement on CI)

**`.pio/`:**
- Purpose: PlatformIO build cache, toolchain downloads
- Generated: Yes
- Committed: No

---

*Structure analysis: 2026-03-09*
