# Codebase Structure

**Analysis Date:** 2026-03-09

## Directory Layout

```
ALX_Nova_Controller_2/
├── src/                    # Firmware source (C++/C, Arduino/ESP-IDF)
│   ├── hal/                # HAL device framework
│   ├── state/              # AppState domain headers
│   ├── gui/                # LVGL GUI (screens, manager, input, theme)
│   │   └── screens/        # Individual screen implementations
│   └── drivers/            # Low-level hardware drivers (ES8311, PCM5102)
├── test/                   # Unity unit tests (native platform)
│   ├── test_mocks/         # Mock headers (Arduino, WiFi, MQTT, Preferences, Wire, I2S)
│   └── test_<module>/      # One directory per test module
├── web_src/                # Embedded web frontend source (edit here)
│   ├── css/                # CSS split by concern
│   └── js/                 # JS modules in load order
├── e2e/                    # Playwright browser tests
│   ├── tests/              # Playwright spec files
│   ├── helpers/            # Shared fixtures and WS helpers
│   ├── fixtures/           # JSON fixtures for WS messages and API responses
│   │   ├── ws-messages/    # WebSocket broadcast fixtures
│   │   └── api-responses/  # REST API response fixtures
│   └── mock-server/        # Express mock server for E2E
│       └── routes/         # Route files matching firmware REST API
├── docs-site/              # Docusaurus v3 public documentation site
│   ├── docs/               # Markdown docs
│   │   ├── user/           # User-facing guides
│   │   └── developer/      # Developer guides, API docs, HAL guide
│   │       ├── api/        # REST API reference pages
│   │       └── hal/        # HAL driver guide pages
│   └── src/                # Docusaurus React components and custom CSS
├── docs-internal/          # Internal architecture docs (not published)
│   ├── architecture/       # Mermaid diagrams (*.mmd) + interconnect docs
│   ├── planning/           # Phase plans, improvement notes
│   ├── development/        # CI/CD, release process, test docs
│   ├── hardware/           # PCM1808, TFT hardware notes
│   └── archive/            # Completed change summaries
├── tools/                  # Node.js build and analysis scripts
├── lib/                    # Vendored libraries
│   ├── WebSockets/         # Vendored WebSockets library (not from lib_deps)
│   └── esp_dsp_lite/       # ANSI C ESP-DSP fallback (native tests only)
├── include/                # Global C++ headers (empty, reserved)
├── logs/                   # Build logs, test reports, serial captures
├── .planning/              # GSD planning documents
│   └── codebase/           # Codebase analysis (STACK.md, ARCHITECTURE.md, etc.)
├── .claude/                # Claude agents, commands, skills, plans
│   ├── agents/             # Agent skill definitions
│   ├── commands/           # Claude slash commands
│   └── plans/              # Named implementation plans
├── .github/
│   └── workflows/          # GitHub Actions CI/CD (tests.yml, release.yml, docs.yml)
├── .githooks/              # Pre-commit hooks (find_dups.js, check_missing_fns.js, ESLint)
├── platformio.ini          # PlatformIO build config (all environments)
├── partitions_ota.csv      # ESP32 flash partition table with OTA slots
├── CLAUDE.md               # Claude Code instructions (this project)
└── RELEASE_NOTES.md        # Firmware release changelog
```

---

## Directory Purposes

### `src/` — Firmware Source Root

All firmware C++ and C source files. Flat structure for subsystem modules; subdirectories only for grouped concerns (hal, state, gui, drivers).

**Entry point:** `src/main.cpp` (~1,451 lines) — `setup()` registers all routes, devices, and subsystems; `loop()` polls events and dispatches dirty flags.

**Top-level files by role:**

| File | Role |
|---|---|
| `main.cpp` | Entry point — `setup()` + `loop()` |
| `app_state.h/.cpp` | AppState singleton composition |
| `app_events.h/.cpp` | FreeRTOS event group (16 event bits) |
| `config.h` | Pin definitions, firmware version, constants |
| `globals.h` | `extern` declarations for global server/MQTT objects |
| `audio_pipeline.h/.cpp` | Core audio pipeline — 8-lane input, 16x16 matrix, 8-sink output |
| `audio_input_source.h` | `AudioInputSource` struct definition |
| `audio_output_sink.h` | `AudioOutputSink` struct definition |
| `i2s_audio.h/.cpp` | Dual PCM1808 I2S ADC HAL, FFT/waveform buffers, RMS/VU computation |
| `dsp_pipeline.h/.cpp` | Pre-matrix stereo DSP engine, 24 stage types, double-buffered |
| `dsp_biquad_gen.h/.c` | RBJ Audio EQ Cookbook biquad coefficient computation |
| `dsp_coefficients.h/.cpp` | DSP coefficient helpers |
| `dsp_crossover.h/.cpp` | Crossover presets (LR2/LR4/LR8, Butterworth) |
| `dsp_convolution.h/.cpp` | Partitioned convolution (room correction IR) |
| `dsp_rew_parser.h/.cpp` | Equalizer APO / miniDSP import, FIR, WAV IR |
| `dsp_api.h/.cpp` | REST API for DSP config CRUD |
| `output_dsp.h/.cpp` | Post-matrix per-output mono DSP engine |
| `pipeline_api.h/.cpp` | REST API for audio pipeline matrix |
| `smart_sensing.h/.cpp` | Voltage sensing, auto-off timer, amplifier relay control |
| `mqtt_handler.h/.cpp` | MQTT lifecycle, settings, callback dispatch |
| `mqtt_publish.cpp` | All `publishMqtt*()` functions + change-detection statics |
| `mqtt_ha_discovery.cpp` | Home Assistant MQTT discovery |
| `mqtt_task.h/.cpp` | Dedicated MQTT FreeRTOS task (Core 0) |
| `websocket_handler.h/.cpp` | WebSocket event handler + broadcast functions |
| `wifi_manager.h/.cpp` | Multi-network WiFi, AP mode, event handler |
| `eth_manager.h/.cpp` | 100Mbps Ethernet (ESP32-P4 onboard) |
| `ota_updater.h/.cpp` | GitHub release check, firmware download + SHA256 verify |
| `settings_manager.h/.cpp` | LittleFS JSON settings persistence |
| `auth_handler.h/.cpp` | PBKDF2-SHA256 auth, session tokens, WS tokens |
| `signal_generator.h/.cpp` | Multi-waveform signal generator (sine/square/noise/sweep) |
| `usb_audio.h/.cpp` | TinyUSB UAC2 speaker device (guarded by `USB_AUDIO_ENABLED`) |
| `button_handler.h/.cpp` | Debouncing, short/long/very-long press, multi-click |
| `buzzer_handler.h/.cpp` | Piezo buzzer patterns, ISR-safe, FreeRTOS mutex |
| `task_monitor.h/.cpp` | FreeRTOS task stack/priority enumeration |
| `debug_serial.h/.cpp` | `LOG_D/I/W/E` macros, WS log forwarding |
| `diag_journal.h/.cpp` | Hot ring buffer + LittleFS diagnostic journal |
| `diag_event.h` | `DiagEvent` struct (64 bytes fixed) |
| `diag_error_codes.h` | `DiagErrorCode` enum and subsystem names |
| `crash_log.h/.cpp` | LittleFS ring buffer of recent reset reasons |
| `dac_hal.h/.cpp` | DAC activation/deactivation logic |
| `dac_api.h/.cpp` | REST API for DAC control |
| `dac_eeprom.h/.cpp` | EEPROM read/write for DAC identification |
| `dac_registry.h/.cpp` | Runtime DAC device registration |
| `thd_measurement.h/.cpp` | THD measurement computation |
| `safe_snr_sfdr.c` | SNR/SFDR analysis (C, not C++) |
| `web_pages.h/.cpp` | Auto-generated: assembled HTML page (do not edit) |
| `web_pages_gz.cpp` | Auto-generated: gzip-compressed page (do not edit) |
| `login_page.h` | Auto-generated: login page |
| `strings.h` | Compile-time string constants |
| `design_tokens.h` | Design token constants |
| `utils.h/.cpp` | Utility functions (version compare, etc.) |
| `idf_component.yml` | ESP-IDF component manifest |

### `src/hal/` — HAL Device Framework

All HAL device drivers, manager, discovery, bridge, and supporting infrastructure.

**Core infrastructure:**
| File | Role |
|---|---|
| `hal_types.h` | All HAL enums, structs, constants (`HalDeviceState`, `HalDeviceDescriptor`, `HalDeviceConfig`, `HalBusType`, capabilities) |
| `hal_device.h` | `HalDevice` abstract base class (`probe`, `init`, `deinit`, `healthCheck`, `getInputSource`) |
| `hal_init_result.h` | `HalInitResult` struct (error code + reason) |
| `hal_device_manager.h/.cpp` | `HalDeviceManager` Meyers singleton (24 devices, pin tracking, lifecycle, retry) |
| `hal_driver_registry.h/.cpp` | Compatible string -> factory function map |
| `hal_builtin_devices.h/.cpp` | Registers all driver entries (`hal_register_builtins()`) |
| `hal_device_db.h/.cpp` | In-memory device database + LittleFS JSON persistence |
| `hal_discovery.h/.cpp` | 3-tier discovery: I2C scan -> EEPROM probe -> manual config |
| `hal_eeprom_v3.h/.cpp` | AT24C02 EEPROM v3 format parser (compatible string + CRC-16) |
| `hal_pipeline_bridge.h/.cpp` | HAL state change callback -> audio pipeline sync |
| `hal_settings.h/.cpp` | `hal_load_device_configs()`, `hal_save_device_configs()` for `/hal_config.json` |
| `hal_api.h/.cpp` | REST endpoints for HAL CRUD |
| `hal_audio_interfaces.h` | `HalAudioDacInterface`, `HalAudioAdcInterface`, `HalAudioCodecInterface` |
| `hal_audio_health_bridge.h/.cpp` | ADC health -> HAL state transitions, flap guard |
| `hal_audio_device.h` | HAL audio device base helpers |

**Audio device drivers:**
| File | Role |
|---|---|
| `hal_pcm5102a.h/.cpp` | PCM5102A I2S DAC driver (`ti,pcm5102a`) |
| `hal_pcm1808.h/.cpp` | PCM1808 I2S ADC driver (`ti,pcm1808`), `getInputSource()` override |
| `hal_es8311.h/.cpp` | ES8311 I2C+I2S codec driver (`evergrande,es8311`) |
| `hal_mcp4725.h/.cpp` | MCP4725 I2C DAC driver |
| `hal_siggen.h/.cpp` | `HalSigGen` — `alx,signal-gen` audio source (software ADC via factory) |
| `hal_signal_gen.h/.cpp` | `HalSignalGen` — generic signal-gen PWM init (HAL GPIO device) |
| `hal_usb_audio.h/.cpp` | `HalUsbAudio` — `alx,usb-audio` USB audio source (software ADC) |
| `hal_i2s_bridge.h/.cpp` | I2S bridge helpers |
| `hal_dac_adapter.h/.cpp` | `HalDacAdapter` — bridges HAL DAC devices to legacy DAC registry |
| `hal_dsp_bridge.h/.cpp` | HAL -> DSP pipeline bridge |

**Peripheral device drivers:**
| File | Role |
|---|---|
| `hal_ns4150b.h/.cpp` | NS4150B class-D amp GPIO enable/disable (GPIO 53) |
| `hal_temp_sensor.h/.cpp` | ESP32-P4 internal temperature sensor |
| `hal_display.h/.cpp` | TFT display HAL device |
| `hal_encoder.h/.cpp` | Rotary encoder HAL device (Gray code ISR) |
| `hal_buzzer.h/.cpp` | Buzzer HAL device (LEDC PWM) |
| `hal_button.h/.cpp` | Button HAL device |
| `hal_led.h/.cpp` | LED HAL device |
| `hal_relay.h/.cpp` | Relay HAL device |
| `hal_custom_device.h/.cpp` | User-defined custom device support |

### `src/state/` — AppState Domain Headers

14 compiled domain state headers (note: `hal_coord_state.h` reserved but not present yet).

| File | Domain | Key Fields |
|---|---|---|
| `enums.h` | Shared enums | `AppFSMState`, `FftWindowType`, `NetIfType` |
| `general_state.h` | General | `timezoneOffset`, `dstOffset`, `darkMode`, `enableCertValidation`, `deviceSerialNumber` |
| `audio_state.h` | Audio | `AdcState[]`, `I2sRuntimeMetrics`, `volatile bool paused`, `adcEnabled[]`, sensing fields |
| `dac_state.h` | DAC output | `filterMode`, `txUnderruns`, `pendingToggle`, `eepromDiag` |
| `dsp_state.h` | DSP pipeline | `enabled`, `bypass`, `presets`, swap diagnostics |
| `wifi_state.h` | WiFi | `ssid`, `password`, AP mode, connection state, multi-network list |
| `mqtt_state.h` | MQTT | `broker`, `port`, `user`, connection state, HA discovery config |
| `ota_state.h` | OTA | `updateAvailable`, `inProgress`, `justUpdated`, `previousFirmwareVersion`, `ReleaseInfo` |
| `display_state.h` | Display | backlight, timeout, dim settings |
| `buzzer_state.h` | Buzzer | `enabled`, `volume`, pattern state |
| `signal_gen_state.h` | Signal gen | `enabled`, `waveform`, `frequency`, `amplitude`, `channel`, `outputMode` |
| `usb_audio_state.h` | USB Audio | connection state, volume, mute |
| `ethernet_state.h` | Ethernet | connection state, IP, MAC |
| `debug_state.h` | Debug | `debugMode`, `serialLevel`, hw stats, `heapCritical` |

### `src/gui/` — LVGL GUI

| File | Role |
|---|---|
| `gui_manager.h/.cpp` | TFT init, LVGL tick, `gui_task` (Core 0), sleep/wake |
| `gui_input.h/.cpp` | ISR-driven Gray code encoder, `gui_input_init_pins()` |
| `gui_theme.h/.cpp` | Orange accent dark/light theme |
| `gui_navigation.h/.cpp` | Screen push/pop stack, transition animations |
| `gui_icons.h` | Inline SVG icon constants |
| `gui_config.h` | LVGL configuration includes |
| `lv_conf.h` | LVGL library configuration |
| `lgfx_config.h` | LovyanGFX ST7735S driver config (confirmed working pin config) |
| `screens/scr_boot_anim.h/.cpp` | Boot animation |
| `screens/scr_desktop.h/.cpp` | Desktop carousel |
| `screens/scr_home.h/.cpp` | Home status screen |
| `screens/scr_control.h/.cpp` | Control screen |
| `screens/scr_wifi.h/.cpp` | WiFi configuration screen |
| `screens/scr_mqtt.h/.cpp` | MQTT configuration screen |
| `screens/scr_settings.h/.cpp` | Settings screen |
| `screens/scr_debug.h/.cpp` | Debug screen |
| `screens/scr_dsp.h/.cpp` | DSP screen |
| `screens/scr_devices.h/.cpp` | HAL devices screen |
| `screens/scr_siggen.h/.cpp` | Signal generator screen |
| `screens/scr_support.h/.cpp` | Support/info screen |
| `screens/scr_menu.h/.cpp` | Menu overlay |
| `screens/scr_keyboard.h/.cpp` | On-screen keyboard |
| `screens/scr_value_edit.h/.cpp` | Value editor overlay |

### `src/drivers/` — Low-Level Hardware Drivers

| File | Role |
|---|---|
| `dac_es8311.h/.cpp` | ES8311 register-level driver |
| `dac_pcm5102.h/.cpp` | PCM5102A register-level driver |
| `es8311_regs.h` | ES8311 register map |

### `test/` — Unity Unit Tests (Native Platform)

Tests run on the host via PlatformIO `native` environment. `test_build_src = no` — tests do not compile `src/` directly; they include specific headers and use mocks.

**`test/test_mocks/`** — Mock headers that replace Arduino/ESP32 headers in native builds:
- `Arduino.h` — `millis()`, `analogRead()`, GPIO, serial stubs
- `WiFi.h` — WiFi connection stubs
- `PubSubClient.h` — MQTT client stub
- `Preferences.h` — NVS stub
- `Wire.h` — I2C stub
- `LittleFS.h` — filesystem stub
- `i2s_std_mock.h` — I2S driver mock
- `mbedtls/` — TLS mock headers

**Test module directories** (each gets its own `test_<module>/` directory):

Audio/DSP: `test_audio_pipeline`, `test_audio_diagnostics`, `test_audio_health_bridge`, `test_dsp`, `test_dsp_presets`, `test_dsp_rew`, `test_dsp_swap`, `test_output_dsp`, `test_fft`, `test_esp_dsp`, `test_vrms`, `test_i2s_audio`, `test_peq`, `test_pipeline_bounds`, `test_pipeline_output`, `test_sink_slot_api`, `test_deferred_toggle`

HAL: `test_hal_core`, `test_hal_bridge`, `test_hal_dsp_bridge`, `test_hal_discovery`, `test_hal_integration`, `test_hal_adapter`, `test_hal_eeprom_v3`, `test_hal_pcm5102a`, `test_hal_pcm1808`, `test_hal_es8311`, `test_hal_mcp4725`, `test_hal_siggen`, `test_hal_usb_audio`, `test_hal_custom_device`, `test_hal_multi_instance`, `test_hal_state_callback`, `test_hal_retry`, `test_hal_wire_mock`, `test_hal_buzzer`, `test_hal_button`, `test_hal_encoder`, `test_hal_ns4150b`

DAC: `test_dac_hal`, `test_dac_eeprom`, `test_dac_settings`

Core subsystems: `test_utils`, `test_auth`, `test_wifi`, `test_mqtt`, `test_settings`, `test_ota`, `test_ota_task`, `test_button`, `test_buzzer`, `test_websocket`, `test_websocket_messages`, `test_api`, `test_smart_sensing`, `test_signal_generator`, `test_task_monitor`, `test_usb_audio`, `test_crash_log`, `test_diag_journal`, `test_debug_mode`, `test_dim_timeout`, `test_evt_any`, `test_eth_manager`, `test_es8311`

GUI: `test_gui_home`, `test_gui_input`, `test_gui_navigation`, `test_pinout`

Hardware integration (not native): `idf4_pcm1808_test`, `idf5_dac_test`, `idf5_pcm1808_test`, `p4_hosted_update`

### `web_src/` — Embedded Web Frontend Source

**ALWAYS edit files here, then run `node tools/build_web_assets.js` to regenerate `src/web_pages.cpp` and `src/web_pages_gz.cpp`.**

| File | Role |
|---|---|
| `index.html` | HTML shell template (body content, no inline CSS/JS) |
| `css/00-tokens.css` | Design token variables |
| `css/01-variables.css` | CSS custom properties |
| `css/02-layout.css` | Layout structure |
| `css/03-components.css` | UI component styles |
| `css/04-canvas.css` | Audio visualization canvas styles |
| `css/05-responsive.css` | Responsive breakpoints |
| `css/06-health-dashboard.css` | Health dashboard styles |

JavaScript files are concatenated in filename order into a single `<script>` block. All `let`/`const` declarations share the same scope.

| File | Role |
|---|---|
| `js/01-core.js` | WebSocket connection management |
| `js/02-ws-router.js` | WS message type dispatch |
| `js/03-app-state.js` | Client-side state store |
| `js/04-shared-audio.js` | Dynamic `numInputLanes`, `resizeAudioArrays()`, visualization arrays |
| `js/05-audio-tab.js` | Unified Audio tab (Inputs/Matrix/Outputs/SigGen sub-views) |
| `js/06-canvas-helpers.js` | Canvas drawing utilities |
| `js/06-peq-overlay.js` | PEQ/crossover/compressor/limiter overlays + biquad math |
| `js/07-ui-core.js` | Tab switching, modals, notifications |
| `js/08-ui-status.js` | Status bar updates |
| `js/09-audio-viz.js` | Waveform/spectrum/VU rendering |
| `js/13-signal-gen.js` | Signal generator controls |
| `js/15-hal-devices.js` | HAL devices tab |
| `js/15a-yaml-parser.js` | YAML parser for HAL device config |
| `js/20-wifi-network.js` | WiFi configuration |
| `js/21-mqtt-settings.js` | MQTT settings |
| `js/22-settings.js` | General settings |
| `js/23-firmware-update.js` | OTA firmware update UI |
| `js/24-hardware-stats.js` | Hardware statistics display |
| `js/25-debug-console.js` | Debug console with category filters |
| `js/26-support.js` | Support/about tab |
| `js/27-auth.js` | Authentication UI |
| `js/27a-health-dashboard.js` | Health Dashboard (device grid, error counters, event log) |
| `js/28-init.js` | Application initialization |

### `e2e/` — Playwright Browser Tests

26 tests across 19 spec files. Tests verify web UI against a mock Express server — no real hardware.

| Path | Role |
|---|---|
| `playwright.config.js` | Playwright configuration |
| `package.json` | Node dependencies (Playwright, Express) |
| `tests/*.spec.js` | 19 spec files (auth, audio-inputs, audio-matrix, audio-outputs, audio-siggen, control-tab, dark-mode, debug-console, hal-devices, hardware-stats, mqtt, navigation, ota, peq-overlay, responsive, settings, support, wifi, auth-password) |
| `helpers/fixtures.js` | `connectedPage` Playwright fixture (session cookie + WS auth + initial state) |
| `helpers/ws-helpers.js` | `buildInitialState()`, `handleCommand()`, binary frame builders |
| `helpers/selectors.js` | Reusable DOM selectors matching `web_src/index.html` IDs |
| `fixtures/ws-messages/*.json` | 15 hand-crafted WS broadcast fixtures |
| `fixtures/api-responses/*.json` | 14 deterministic REST response fixtures |
| `mock-server/server.js` | Express server (port 3000) |
| `mock-server/assembler.js` | Replicates `tools/build_web_assets.js` HTML assembly |
| `mock-server/ws-state.js` | Deterministic mock state singleton, reset between tests |
| `mock-server/routes/auth.js` | Auth routes |
| `mock-server/routes/hal.js` | HAL routes |
| `mock-server/routes/wifi.js` | WiFi routes |
| `mock-server/routes/mqtt.js` | MQTT routes |
| `mock-server/routes/settings.js` | Settings routes |
| `mock-server/routes/ota.js` | OTA routes |
| `mock-server/routes/pipeline.js` | Pipeline matrix routes |
| `mock-server/routes/dsp.js` | DSP routes |
| `mock-server/routes/sensing.js` | Smart sensing routes |
| `mock-server/routes/siggen.js` | Signal generator routes |
| `mock-server/routes/diagnostics.js` | Diagnostics routes |
| `mock-server/routes/system.js` | System routes |

### `docs-site/` — Docusaurus v3 Public Documentation

| Path | Role |
|---|---|
| `docusaurus.config.js` | Docusaurus configuration |
| `sidebars.js` | Sidebar navigation structure |
| `docs/user/` | User-facing guides (getting-started, wifi, mqtt, ota, smart-sensing, etc.) |
| `docs/developer/overview.md` | Developer entry point |
| `docs/developer/architecture.md` | Architecture reference (Mermaid diagrams) |
| `docs/developer/audio-pipeline.md` | Audio pipeline deep-dive |
| `docs/developer/dsp-system.md` | DSP system reference |
| `docs/developer/websocket.md` | WebSocket protocol reference |
| `docs/developer/testing.md` | Testing guide |
| `docs/developer/build-setup.md` | Build and development setup |
| `docs/developer/contributing.md` | Contribution guide |
| `docs/developer/api/` | REST API reference pages (rest-main.md, rest-dac.md, rest-dsp.md, rest-hal.md, rest-pipeline.md) |
| `docs/developer/hal/overview.md` | HAL overview |
| `docs/developer/hal/device-lifecycle.md` | Device lifecycle reference |
| `docs/developer/hal/driver-guide.md` | Driver authoring guide |
| `docs/developer/hal/drivers.md` | Driver catalog |
| `src/pages/` | Docusaurus React pages |
| `src/css/` | Custom CSS overrides |
| `static/img/` | Static images |

### `docs-internal/` — Internal Architecture Docs

Not published. Referenced by MEMORY.md and Claude plans.

| Path | Role |
|---|---|
| `architecture/` | 10 Mermaid diagrams (system-architecture.mmd, hal-lifecycle.mmd, hal-pipeline-bridge.mmd, boot-sequence.mmd, event-architecture.mmd, sink-dispatch.mmd, test-infrastructure.mmd, ci-quality-gates.mmd, e2e-test-flow.mmd, test-coverage-map.mmd) + text docs |
| `testing-architecture.md` | E2E test architecture narrative |
| `planning/` | Active and archived implementation plans |
| `development/` | CI/CD, release process, test inventory |
| `hardware/` | PCM1808 integration, TFT/rotary feature notes |
| `archive/` | Completed phase summaries |
| `user/` | User doc source (moved to docs-site/docs/user) |

### `tools/` — Build and Analysis Scripts

| File | Role |
|---|---|
| `build_web_assets.js` | Assembles `web_src/` -> `src/web_pages.cpp` + `src/web_pages_gz.cpp`. Run after any `web_src/` edit. |
| `find_dups.js` | Finds duplicate `let`/`const` declarations across JS files (pre-commit + CI) |
| `check_missing_fns.js` | Finds JS function calls with no matching definition |
| `deep_check_fns.js` | Extended function reference checker |
| `detect_doc_changes.js` | Detects source changes that need doc updates |
| `extract_api.js` | Extracts REST API endpoints from source |
| `extract_tokens.js` | Extracts design tokens |
| `generate_docs.js` | Auto-generates documentation |
| `doc-mapping.json` | Source file -> documentation page mapping |
| `fix_riscv_toolchain.py` | PlatformIO pre-script: patches RISC-V toolchain path for ESP32-P4 |
| `check_hal_debug_contract.sh` | Shell script: validates HAL debug log contract |
| `prompts/` | Prompt templates for documentation generation |

### `lib/` — Vendored Libraries

| Path | Role |
|---|---|
| `WebSockets/` | Vendored WebSockets library — not pulled from lib_deps registry. Referenced in `platformio.ini` via `lib_ignore = WebSockets` on native env. |
| `esp_dsp_lite/` | ANSI C fallback for ESP-DSP (native tests only). `lib_ignore = esp_dsp_lite` in ESP32 env prevents link conflict with pre-built `.a`. |

### `.planning/codebase/` — GSD Codebase Analysis

| File | Contents |
|---|---|
| `ARCHITECTURE.md` | System architecture, layers, data flow, design patterns |
| `STRUCTURE.md` | This file — directory structure and file organization |
| `STACK.md` | Technology stack and build environment |
| `INTEGRATIONS.md` | External services and APIs |
| `CONVENTIONS.md` | Coding style, naming, import patterns |
| `TESTING.md` | Test frameworks, patterns, coverage |

### `.github/workflows/` — CI/CD Pipelines

| File | Role |
|---|---|
| `tests.yml` | 4 parallel quality gates: cpp-tests (Unity), cpp-lint (cppcheck), js-lint (ESLint + find_dups + check_missing_fns), e2e-tests (Playwright). Triggers on push/PR to main and develop. |
| `docs.yml` | Docusaurus documentation build/deploy |

---

## Naming Conventions

### Files
- Firmware C++ modules: `snake_case.h` / `snake_case.cpp` — e.g., `audio_pipeline.h`, `smart_sensing.cpp`
- HAL drivers: `hal_<device>.h/.cpp` — e.g., `hal_pcm5102a.h`, `hal_es8311.cpp`
- State headers: `<domain>_state.h` — e.g., `audio_state.h`, `wifi_state.h`
- GUI screens: `scr_<name>.h/.cpp` — e.g., `scr_home.h`, `scr_desktop.cpp`
- Test modules: `test_<module>/` directory — e.g., `test_hal_core/`
- E2E specs: `<feature>.spec.js` — e.g., `auth.spec.js`, `hal-devices.spec.js`
- JS modules: `<nn>[-<suffix>]-<name>.js` (numeric prefix controls load order)

### Classes and Types
- C++ classes: `PascalCase` — `HalDevice`, `HalDeviceManager`, `AppState`, `ButtonHandler`
- HAL device class names: `Hal<DeviceName>` — `HalPcm5102a`, `HalEs8311`, `HalBuzzer`
- C structs: `PascalCase` — `AudioInputSource`, `DiagEvent`, `HalDeviceDescriptor`
- State domain structs: `<Domain>State` — `AudioState`, `WifiState`, `DacState`
- Enums: `SCREAMING_SNAKE_CASE` for values, `PascalCase` for type — e.g., `enum HalDeviceState : uint8_t { HAL_STATE_UNKNOWN = 0, ... }`

### Functions
- C-style module functions: `snake_case` with module prefix — `audio_pipeline_set_source()`, `hal_discover_devices()`, `diag_emit()`
- HTTP handler functions: `handle<Action><Resource>()` — `handleWiFiSave()`, `handleSettingsGet()`
- WS broadcast functions: `send<Domain>State()` — `sendDacState()`, `sendHalDeviceState()`
- Dirty flag methods on AppState: `markXxxDirty()`, `isXxxDirty()`, `clearXxxDirty()`

### Constants and Macros
- Build flag constants: `SCREAMING_SNAKE_CASE` — `AUDIO_PIPELINE_MAX_INPUTS`, `HAL_MAX_DEVICES`
- HAL capability flags: `HAL_CAP_*` — `HAL_CAP_DAC_PATH`, `HAL_CAP_ADC_PATH`
- HAL I2C bus indices: `HAL_I2C_BUS_*` — `HAL_I2C_BUS_EXT`, `HAL_I2C_BUS_ONBOARD`
- Event bits: `EVT_*` — `EVT_DAC`, `EVT_CHANNEL_MAP`
- Log macros: `LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`

---

## Where to Add New Code

### New Firmware Subsystem Module
1. Create `src/<module>.h` and `src/<module>.cpp`
2. Add `#include "<module>.h"` in `src/main.cpp`
3. Call init function from `setup()`; call periodic functions from `loop()` or register as FreeRTOS task on Core 0
4. Add dirty flags + event bits if the module has state to broadcast
5. Create `test/test_<module>/test_<module>.cpp` with Unity tests
6. Add module prefix to logging: `LOG_I("[ModuleName] ...")`

### New HAL Device Driver
1. Create `src/hal/hal_<device>.h/.cpp` extending `HalDevice`
2. Implement: `probe()`, `init()`, `deinit()`, `dumpConfig()`, `healthCheck()`
3. Implement `getInputSource()` override if it produces audio
4. Register driver entry in `src/hal/hal_builtin_devices.cpp` via `hal_registry_register()`
5. Register device instance in `src/main.cpp` `setup()` with `registerDevice()`, `probe()`, `init()`
6. Create `test/test_hal_<device>/test_hal_<device>.cpp`

### New AppState Domain Field
1. Add field to the appropriate domain header in `src/state/<domain>_state.h`
2. If a new dirty flag is needed: add `markXxxDirty()`, `isXxxDirty()`, `clearXxxDirty()` to `src/app_state.h`
3. Add a new `EVT_*` bit in `src/app_events.h` if needed (bits 0-15 assigned, bits 16-23 spare)
4. Add corresponding WS broadcast in `src/websocket_handler.cpp`

### New REST API Endpoint
1. Add `server.on("/api/<path>", HTTP_<METHOD>, []() { ... })` in `src/main.cpp` `setup()`
2. Add matching route in `e2e/mock-server/routes/<category>.js`
3. Add fixture JSON in `e2e/fixtures/api-responses/` if needed
4. Add Playwright test in `e2e/tests/<feature>.spec.js`

### New Web UI Feature
1. Edit files in `web_src/css/` and `web_src/js/` only
2. Run `node tools/build_web_assets.js` to regenerate `src/web_pages.cpp` + `src/web_pages_gz.cpp`
3. Add new top-level JS declarations to `web_src/.eslintrc.json` globals
4. Add WS fixture to `e2e/fixtures/ws-messages/` if a new broadcast type is introduced
5. Update `e2e/helpers/ws-helpers.js` `buildInitialState()` and `handleCommand()` for new state
6. Add Playwright test in `e2e/tests/`
7. Run `node tools/find_dups.js` to verify no duplicate declarations

### New LVGL Screen
1. Create `src/gui/screens/scr_<name>.h/.cpp`
2. Register in `src/gui/gui_navigation.cpp`
3. Add navigation entry point from appropriate parent screen

### New Test Module
1. Create `test/test_<module>/` directory
2. Create `test/test_<module>/test_<module>.cpp` with `setUp()`, `tearDown()`, and `RUN_TEST()` calls
3. Each test file must have its own `main()` — no shared `main` across modules
4. Use mock headers from `test/test_mocks/` for platform-specific dependencies

---

## Special Directories and Files

### `src/web_pages.cpp` and `src/web_pages_gz.cpp`
Auto-generated by `node tools/build_web_assets.js`. Do not edit manually. Generated from `web_src/index.html` + `web_src/css/*.css` + `web_src/js/*.js` (sorted alphanumerically).

### `src/login_page.h`
Auto-generated by `tools/build_web_assets.js`. Contains login page as C string literal.

### `partitions_ota.csv`
Custom ESP32 flash partition table with dual OTA slots. Required for OTA firmware update.

### `platformio.ini`
Three environments:
- `esp32-p4` — primary build for Waveshare ESP32-P4-WiFi6-DEV-Kit (COM8)
- `native` — host-machine Unity test runner (gcc/MinGW)
- `p4_hosted_update` — hosted OTA update test build

### `logs/`
Save all build output, test reports, and serial captures here. Keep project root clean.

### `.githooks/pre-commit`
Runs before every commit: `find_dups.js`, `check_missing_fns.js`, ESLint on `web_src/js/`. Activate with: `git config core.hooksPath .githooks`

---

*Structure analysis: 2026-03-09*
