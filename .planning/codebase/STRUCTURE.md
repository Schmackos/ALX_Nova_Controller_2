# Codebase Structure

_Last updated: 2026-03-21_

## Directory Layout

```
ALX_Nova_Controller_2/
├── src/                        # Firmware source (ESP32-P4)
│   ├── main.cpp                # Entry point: setup(), loop(), REST API registration
│   ├── config.h                # All compile-time constants (pins, stack sizes, FIRMWARE_VERSION)
│   ├── globals.h               # Shared extern declarations
│   ├── app_state.h/.cpp        # AppState singleton (~80 lines, thin composition shell)
│   ├── app_events.h/.cpp       # FreeRTOS event group (16 bits assigned, EVT_ANY=0x00FFFFFF)
│   ├── design_tokens.h         # Design system tokens (colours, spacing) — source of truth
│   ├── state/                  # 15 domain-specific state headers (decomposed from AppState)
│   │   ├── enums.h             # AppFSMState, FftWindowType, NetIfType
│   │   ├── general_state.h     # GeneralState (timezone, darkMode, certValidation)
│   │   ├── audio_state.h       # AudioState (ADC channels, I2S metrics, audioPaused volatile)
│   │   ├── dac_state.h         # DacState (txUnderruns, EepromDiag only)
│   │   ├── dsp_state.h         # DspSettingsState (enabled, bypass, presets, swap diags)
│   │   ├── wifi_state.h        # WifiState (SSID, password, AP mode, connection)
│   │   ├── mqtt_state.h        # MqttState (broker config, connection)
│   │   ├── hal_coord_state.h   # HalCoordState (deferred device toggle queue, capacity 8)
│   │   ├── ota_state.h         # OtaState + ReleaseInfo struct
│   │   ├── display_state.h     # DisplayState
│   │   ├── buzzer_state.h      # BuzzerState
│   │   ├── signal_gen_state.h  # SignalGenState
│   │   ├── usb_audio_state.h   # UsbAudioState (guarded by USB_AUDIO_ENABLED)
│   │   ├── ethernet_state.h    # EthernetState
│   │   └── debug_state.h       # DebugState (debug mode, serial level, heapCritical)
│   ├── hal/                    # Hardware Abstraction Layer
│   │   ├── hal_types.h         # Core types: HalDeviceConfig, HalCapability, HAL_MAX_DEVICES=24
│   │   ├── hal_device.h        # HalDevice base class (lifecycle states, _ready, _state)
│   │   ├── hal_audio_device.h  # HalAudioDevice subclass (buildSink virtual)
│   │   ├── hal_audio_interfaces.h  # AudioInputSource, AudioOutputSink interfaces
│   │   ├── hal_device_manager.h/.cpp   # Singleton: 24 devices, 56 pins, priority init
│   │   ├── hal_pipeline_bridge.h/.cpp  # Connects HAL lifecycle to audio pipeline
│   │   ├── hal_driver_registry.h/.cpp  # compatible-string → factory function mapping
│   │   ├── hal_builtin_devices.h/.cpp  # Registers all built-in drivers at boot
│   │   ├── hal_device_db.h/.cpp        # In-memory device DB + LittleFS JSON persistence
│   │   ├── hal_discovery.h/.cpp        # 3-tier: I2C scan → EEPROM probe → manual
│   │   ├── hal_settings.h/.cpp         # /hal_config.json load/save
│   │   ├── hal_api.h/.cpp              # 13 REST endpoints for HAL CRUD
│   │   ├── hal_eeprom_v3.h/.cpp        # EEPROM v3 compatible-string matching
│   │   ├── hal_init_result.h           # HalInitResult struct (.success field)
│   │   ├── hal_custom_device.h/.cpp    # User-defined device schemas
│   │   ├── hal_i2s_bridge.h/.cpp       # I2S TX bridge (HAL DAC → I2S output)
│   │   ├── hal_audio_health_bridge.h/.cpp  # Pipeline health → HAL diagnostics
│   │   ├── hal_dsp_bridge.h/.cpp       # DSP pipeline integration
│   │   ├── hal_relay.h/.cpp            # GPIO relay (generic,relay-amp compatible)
│   │   ├── hal_temp_sensor.h/.cpp      # ESP32-P4 internal temp sensor
│   │   ├── hal_ns4150b.h/.cpp          # NS4150B class-D amp (GPIO 53)
│   │   ├── hal_pcm5102a.h/.cpp         # PCM5102A I2S DAC driver
│   │   ├── hal_pcm1808.h/.cpp          # PCM1808 I2S ADC driver (dual)
│   │   ├── hal_es8311.h/.cpp           # ES8311 codec driver (I2C bus 1)
│   │   ├── hal_mcp4725.h/.cpp          # MCP4725 I2C DAC driver
│   │   ├── hal_siggen.h/.cpp           # alx,signal-gen (audio source, dynamic lane)
│   │   ├── hal_signal_gen.h/.cpp       # generic,signal-gen (GPIO stub)
│   │   ├── hal_usb_audio.h/.cpp        # alx,usb-audio UAC2 speaker (guarded)
│   │   ├── hal_button.h/.cpp           # Button HAL device
│   │   ├── hal_buzzer.h/.cpp           # Buzzer HAL device
│   │   ├── hal_encoder.h/.cpp          # Rotary encoder HAL device
│   │   ├── hal_display.h/.cpp          # Display HAL device
│   │   └── hal_led.h/.cpp              # LED HAL device
│   ├── gui/                    # LVGL GUI (Core 0, guarded by GUI_ENABLED)
│   │   ├── gui_manager.h/.cpp  # Init, FreeRTOS task, screen sleep/wake
│   │   ├── gui_input.h/.cpp    # ISR-driven rotary encoder (Gray code)
│   │   ├── gui_theme.h/.cpp    # Orange accent theme, dark/light mode
│   │   ├── gui_navigation.h/.cpp  # Screen stack with push/pop + transitions
│   │   ├── lgfx_config.h       # LovyanGFX ST7735S config (confirmed working)
│   │   └── screens/            # Individual screen implementations
│   │       ├── scr_boot_anim.h/.cpp
│   │       ├── scr_desktop.h/.cpp   # Carousel home
│   │       ├── scr_home.h/.cpp      # Status dashboard
│   │       ├── scr_control.h/.cpp   # Volume/DSP controls
│   │       ├── scr_wifi.h/.cpp
│   │       ├── scr_mqtt.h/.cpp
│   │       ├── scr_settings.h/.cpp
│   │       ├── scr_debug.h/.cpp
│   │       ├── scr_dsp.h/.cpp
│   │       ├── scr_siggen.h/.cpp
│   │       ├── scr_devices.h/.cpp
│   │       ├── scr_support.h/.cpp
│   │       ├── scr_menu.h/.cpp
│   │       ├── scr_keyboard.h/.cpp  # On-screen keyboard
│   │       └── scr_value_edit.h/.cpp
│   ├── drivers/                # Low-level C drivers
│   │   └── dsp_biquad_gen.c    # RBJ EQ biquad coefficient computation
│   ├── safe_snr_sfdr.c         # SNR/SFDR measurement (ESP-DSP safe wrappers)
│   ├── idf_component.yml       # IDF component manifest
│   │
│   # Core firmware modules (flat in src/):
│   ├── audio_pipeline.h/.cpp       # 8-lane → DSP → 16×16 matrix → 8-sink dispatch
│   ├── audio_input_source.h        # AudioInputSource struct
│   ├── audio_output_sink.h         # AudioOutputSink struct
│   ├── i2s_audio.h/.cpp            # Dual PCM1808 I2S ADC init, FFT, analysis
│   ├── dsp_pipeline.h/.cpp         # 4-ch biquad IIR/FIR DSP, double-buffered swap
│   ├── dsp_biquad_gen.h            # biquad gen function declarations
│   ├── dsp_coefficients.h/.cpp     # Coefficient computation wrappers
│   ├── dsp_crossover.h/.cpp        # Crossover presets (LR2/4/8, Butterworth)
│   ├── dsp_rew_parser.h/.cpp       # Equalizer APO + miniDSP import/export
│   ├── dsp_convolution.h/.cpp      # FIR convolution/IR processing
│   ├── dsp_api.h/.cpp              # REST API for DSP CRUD + LittleFS persistence
│   ├── output_dsp.h/.cpp           # Per-output mono DSP (post-matrix/pre-sink)
│   ├── pipeline_api.h/.cpp         # REST API for matrix CRUD + output DSP config
│   ├── dac_hal.h/.cpp              # I2S TX management, volume curves, periodic logging
│   ├── dac_api.h/.cpp              # REST API for DAC state, volume, enable/disable
│   ├── dac_eeprom.h/.cpp           # DAC EEPROM diagnostics
│   ├── sink_write_utils.h/.cpp     # Shared float buffer utils for all sink drivers
│   ├── signal_generator.h/.cpp     # Multi-waveform test signal generator
│   ├── thd_measurement.h/.cpp      # THD+N measurement (guarded by DSP_ENABLED)
│   ├── smart_sensing.h/.cpp        # Voltage detection, auto-off, relay control
│   ├── wifi_manager.h/.cpp         # Multi-network WiFi client, AP mode, retry/backoff
│   ├── mqtt_handler.h/.cpp         # MQTT broker connection, settings, HA discovery
│   ├── mqtt_publish.cpp            # Change-detection statics, publish functions
│   ├── mqtt_ha_discovery.cpp       # Home Assistant MQTT discovery messages
│   ├── mqtt_task.h/.cpp            # Dedicated FreeRTOS task (Core 0, priority 2)
│   ├── ota_updater.h/.cpp          # GitHub release check, download, SHA256 verify
│   ├── settings_manager.h/.cpp     # /config.json + NVS persistence
│   ├── auth_handler.h/.cpp         # PBKDF2-SHA256 auth, WS token pool, rate limiting
│   ├── button_handler.h/.cpp       # Debouncing, short/long/multi-click detection
│   ├── buzzer_handler.h/.cpp       # Piezo sequencer, ISR-safe, FreeRTOS mutex
│   ├── websocket_handler.h/.cpp    # Real-time WS broadcasting (port 81)
│   ├── eth_manager.h/.cpp          # Ethernet 100Mbps Full Duplex
│   ├── usb_audio.h/.cpp            # TinyUSB UAC2 speaker (guarded by USB_AUDIO_ENABLED)
│   ├── debug_serial.h/.cpp         # LOG_D/I/W/E macros, WS log forwarding
│   ├── task_monitor.h/.cpp         # FreeRTOS task stack/priority enumeration
│   ├── crash_log.h/.cpp            # Crash log capture and persistence
│   ├── diag_journal.h/.cpp         # DiagEvent journal (circular buffer)
│   ├── diag_event.h                # DiagEvent struct
│   ├── diag_error_codes.h          # Error code constants
│   ├── login_page.h                # Embedded login HTML (gzip)
│   └── web_pages.cpp / web_pages_gz.cpp  # Auto-generated — DO NOT EDIT (from web_src/)
│
├── web_src/                    # Web UI source (edit here, NOT src/web_pages.cpp)
│   ├── index.html              # HTML shell (body content, no inline CSS/JS)
│   ├── css/                    # CSS split by concern
│   │   ├── 00-tokens.css       # Auto-generated from src/design_tokens.h
│   │   ├── 01-variables.css
│   │   ├── 02-layout.css
│   │   ├── 03-components.css
│   │   ├── 04-canvas.css
│   │   ├── 05-responsive.css
│   │   └── 06-health-dashboard.css
│   └── js/                     # JS modules (concatenated in filename order)
│       ├── 01-core.js          # WebSocket connection, reconnect
│       ├── 02-ws-router.js     # Message dispatch
│       ├── 03-app-state.js     # Client-side state
│       ├── 04-shared-audio.js  # numInputLanes, resizeAudioArrays()
│       ├── 05-audio-tab.js     # HAL-driven channel strips, 8×8 matrix UI
│       ├── 06-canvas-helpers.js
│       ├── 06-peq-overlay.js   # Full-screen PEQ/DSP overlays, freq response graph
│       ├── 07-ui-core.js
│       ├── 08-ui-status.js
│       ├── 09-audio-viz.js
│       ├── 13-signal-gen.js
│       ├── 15-hal-devices.js   # HAL device management UI
│       ├── 15a-yaml-parser.js
│       ├── 20-wifi-network.js
│       ├── 21-mqtt-settings.js
│       ├── 22-settings.js
│       ├── 23-firmware-update.js
│       ├── 24-hardware-stats.js
│       ├── 25-debug-console.js # Module chip filtering, search, entry badges
│       ├── 26-support.js
│       ├── 27-auth.js
│       ├── 27a-health-dashboard.js  # Health Dashboard (device grid, error counters, event log)
│       └── 28-init.js
│
├── test/                       # Unity C++ unit tests (native platform)
│   ├── test_mocks/             # Mock implementations (Arduino, WiFi, MQTT, Preferences, I2C)
│   ├── test_<module>/          # One directory per test module (~70 modules)
│   └── [idf4/idf5/p4 dirs]    # Hardware-specific integration test stubs
│
├── e2e/                        # Playwright browser tests
│   ├── tests/                  # 19 spec files (26 tests)
│   ├── mock-server/            # Express server (port 3000) + WS interception
│   │   ├── server.js           # Main Express server
│   │   ├── assembler.js        # HTML assembly from web_src/
│   │   ├── ws-state.js         # Deterministic mock state singleton
│   │   └── routes/             # 12 Express route files (matches firmware REST API)
│   ├── helpers/
│   │   ├── fixtures.js         # connectedPage fixture (session + WS auth)
│   │   ├── ws-helpers.js       # buildInitialState(), handleCommand(), binary frames
│   │   └── selectors.js        # Reusable DOM selectors
│   └── fixtures/
│       ├── ws-messages/        # 15 hand-crafted WS broadcast fixtures (JSON)
│       └── api-responses/      # 14 deterministic REST response fixtures (JSON)
│
├── docs-site/                  # Docusaurus v3 public documentation site
│   ├── docusaurus.config.js    # Site config (Mermaid, local search, dark mode default)
│   ├── sidebars.js             # userSidebar (9) + devSidebar (17)
│   ├── docs/
│   │   ├── developer/          # 17 developer reference pages
│   │   └── user/               # 9 user guide pages
│   └── src/
│       ├── css/tokens.css      # Auto-generated from src/design_tokens.h
│       └── pages/index.js      # Hero landing page
│
├── docs-internal/              # Internal working docs (not public)
│   ├── architecture/           # Mermaid diagrams (10 .mmd files)
│   ├── planning/               # Feature plans and debt registries
│   ├── development/            # Dev guides
│   ├── hardware/               # Hardware notes
│   └── user/                   # Internal user docs
│
├── lib/                        # Vendored libraries
│   ├── WebSockets/             # WebSocket server (vendored, not registry)
│   └── esp_dsp_lite/           # ANSI C DSP fallbacks for native tests only
│
├── tools/                      # Build and generation scripts (Node.js)
│   ├── build_web_assets.js     # Bundles web_src/ → src/web_pages.cpp + web_pages_gz.cpp
│   ├── extract_tokens.js       # design_tokens.h → CSS files (web + docs)
│   ├── extract_api.js          # REST endpoint extraction from C++ source
│   ├── generate_docs.js        # Claude API orchestrator for CI doc generation
│   ├── detect_doc_changes.js   # Git diff → section mapping for incremental updates
│   ├── find_dups.js            # Duplicate JS global declarations checker
│   ├── check_missing_fns.js    # Undefined function reference checker
│   ├── doc-mapping.json        # Source file → documentation section mapping (65 entries)
│   └── prompts/                # Writing style templates (api-ref, user-guide, hal-driver)
│
├── .planning/                  # GSD planning system
│   └── codebase/               # Codebase map documents
│
├── .github/workflows/          # CI/CD
│   ├── tests.yml               # 4 parallel quality gates (cpp, lint, js, e2e)
│   ├── release.yml             # Same 4 gates before release
│   └── docs.yml                # Doc detection → generation → gh-pages deploy
│
├── .githooks/
│   └── pre-commit              # find_dups + check_missing_fns + ESLint
│
├── platformio.ini              # PlatformIO config (esp32-p4 + native environments)
├── CLAUDE.md                   # AI coding instructions (this project)
└── logs/                       # Build output, test reports, serial captures
```

## Key File Locations

| Purpose | File/Location |
|---------|---------------|
| Firmware entry point | `src/main.cpp` |
| All compile-time constants | `src/config.h` |
| AppState singleton | `src/app_state.h` |
| Domain state headers | `src/state/*.h` (15 files) |
| HAL base class | `src/hal/hal_device.h` |
| HAL device manager | `src/hal/hal_device_manager.h` |
| HAL type definitions | `src/hal/hal_types.h` |
| Audio pipeline | `src/audio_pipeline.h/.cpp` |
| Web UI source | `web_src/` (edit here) |
| Web UI generated | `src/web_pages.cpp` (DO NOT EDIT) |
| Design tokens | `src/design_tokens.h` |
| Build web assets | `tools/build_web_assets.js` |
| Test mocks | `test/test_mocks/` |
| E2E mock server | `e2e/mock-server/server.js` |
| PlatformIO config | `platformio.ini` |
| CI/CD workflows | `.github/workflows/` |
| Pre-commit hooks | `.githooks/pre-commit` |

## Naming Conventions

### Files
- C++ modules: `snake_case.h` + `snake_case.cpp` (header + implementation pair)
- HAL drivers: `hal_<device>.h/.cpp` (e.g., `hal_pcm5102a.h`)
- State headers: `<domain>_state.h` (e.g., `wifi_state.h`)
- GUI screens: `scr_<name>.h/.cpp` (e.g., `scr_home.h`)
- Test modules: `test_<module>/` directory with `test_<module>.cpp` inside
- JS modules: `NN-name.js` (two-digit number prefix for load order)
- CSS files: `NN-name.css` (two-digit number prefix)

### Code
- Classes: `PascalCase` (e.g., `HalDeviceManager`, `AppState`)
- Functions: `snake_case` (e.g., `audio_pipeline_init()`)
- HAL compatible strings: `vendor,device` format (e.g., `"alx,signal-gen"`, `"generic,relay-amp"`)
- Constants/macros: `UPPER_SNAKE_CASE` (e.g., `HAL_MAX_DEVICES`, `FIRMWARE_VERSION`)
- State access: `appState.domain.field` (e.g., `appState.wifi.ssid`, `appState.audio.paused`)
- Event bits: `EVT_<NAME>` (e.g., `EVT_WIFI`, `EVT_MQTT`)
- Log prefixes: `[ModuleName]` in square brackets (e.g., `[HAL]`, `[Audio]`, `[WiFi]`)

### Test Naming
- Test files: `test_<module>.cpp` inside `test/test_<module>/` directory
- Test functions: `test_<what>_<condition>()` (e.g., `test_hal_core_high_gpio()`)
- setUp()/tearDown() per test file for state reset

## Module Boundaries

```
┌─────────────────────────────────────────────────────┐
│ main.cpp — orchestration, REST registration, FSM     │
├──────────────┬──────────────┬───────────────────────┤
│  HAL Layer   │  Audio Layer │  Network/Config Layer  │
│  src/hal/    │  audio_      │  wifi_manager          │
│  (24 devices)│  pipeline,   │  mqtt_handler/task     │
│              │  i2s_audio,  │  ota_updater           │
│              │  dsp_*,      │  settings_manager      │
│              │  output_dsp  │  auth_handler          │
├──────────────┴──────────────┴───────────────────────┤
│ AppState (src/app_state.h) — thin composition shell  │
│ 15 domain state headers (src/state/*.h)              │
├─────────────────────────────────────────────────────┤
│ GUI (src/gui/) — Core 0, LVGL, guarded GUI_ENABLED   │
├─────────────────────────────────────────────────────┤
│ Web UI (web_src/) — HTTP/80 + WebSocket/81           │
└─────────────────────────────────────────────────────┘
```

### Inter-module Communication
- **State**: via `appState.domain.field` (dirty flags + event bits)
- **HAL → Audio**: via `hal_pipeline_bridge` (state change callback → sink/source reg)
- **Audio → Main loop**: via `EVT_*` event bits + dirty flags
- **MQTT**: isolated on Core 0 via `mqtt_task`, communicates via `_mqttReconfigPending` flag
- **GUI**: reads AppState, writes dirty flags, runs on Core 0 isolated from audio Core 1
- **WebSocket**: real-time broadcast triggered by dirty flags in main loop (port 81)
- **HAL toggles**: via `appState.halCoord.requestDeviceToggle(slot, action)` → queue drained by main loop

### Critical Boundaries (DO NOT CROSS)
- **Core 1**: Only `loopTask` + `audio_pipeline_task`. No new tasks may be pinned here.
- **I2S driver**: Never reinstall without `appState.audio.paused` + semaphore handshake
- **HAL Bus 0 (GPIO 48/54)**: Never scan while WiFi active (SDIO conflict → MCU reset)
- **MCLK**: Never call `i2s_configure_adc1()` in audio task loop (MCLK must be continuous)
- **web_pages.cpp**: Never edit manually — always regenerate via `node tools/build_web_assets.js`
