# Codebase Structure

**Analysis Date:** 2026-03-23

## Directory Layout

```
ALX_Nova_Controller_2/
├── .github/
│   └── workflows/              # CI/CD pipelines (tests.yml, docs.yml, release.yml)
├── .planning/
│   └── codebase/              # GSD codebase mapping output (this directory)
├── docs-internal/              # Internal design docs (architecture diagrams, testing strategies)
├── docs-site/                  # Public Docusaurus v3 site (26 pages, 9 user + 17 dev)
├── e2e/                        # Browser E2E tests (Playwright, 113 tests, 22 specs)
│   ├── mock-server/           # Express mock API + WS state (assembler, routes, fixtures)
│   ├── helpers/               # WS message builders, selectors, fixtures
│   ├── tests/                 # 22 .spec.js files
│   └── fixtures/              # JSON response fixtures, WS message fixtures
├── include/                    # C++ include headers (3rd party, Arduino libs)
├── lib/                        # Vendored libraries
│   ├── WebSockets/            # arduinoWebSockets v2.7.3 (vendored, not from lib_deps)
│   ├── esp_dsp_lite/          # ANSI C ESP-DSP fallback for native tests (lib_ignore on ESP32)
│   └── ...                    # Other vendored deps
├── logs/                       # Build outputs, test reports, serial captures (auto-generated)
├── skills/                     # Claude agent skill definitions (.claude/commands)
├── src/                        # Main firmware source (107 files)
│   ├── hal/                   # Hardware Abstraction Layer (57 files, 30 drivers)
│   │   ├── hal_device_manager.h/.cpp       # Singleton device registry + lifecycle
│   │   ├── hal_discovery.h/.cpp            # 3-tier auto-detection (I2C, EEPROM, manual)
│   │   ├── hal_device_db.h/.cpp            # In-memory + LittleFS device database
│   │   ├── hal_api.h/.cpp                  # 13 REST endpoints (CRUD devices, scan, reinit)
│   │   ├── hal_pipeline_bridge.h/.cpp      # Lifecycle → audio sink/source registration
│   │   ├── hal_custom_device.h/.cpp        # User-defined device creator (3-tier support)
│   │   ├── hal_builtin_devices.h/.cpp      # Registry + HAL_REGISTER macro
│   │   ├── hal_audio_health_bridge.h/.cpp  # Audio diagnostics integration
│   │   ├── hal_eeprom_v3.h/.cpp            # EEPROM string matching (Zigbee Alliance)
│   │   ├── hal_settings.h/.cpp             # Per-device config persistence (/hal_config.json)
│   │   ├── hal_*_regs.h                    # Register definitions (ES9*, ES8311)
│   │   ├── hal_es9*.h/.cpp                 # 19 specific driver implementations
│   │   ├── hal_tdm_interleaver.h/.cpp      # 8ch DAC frame combiner (4 stereo pairs → TDM)
│   │   ├── hal_tdm_deinterleaver.h/.cpp    # 4ch ADC frame splitter (TDM → 2 stereo pairs)
│   │   ├── hal_*_base.h/.cpp               # Base classes (EssSabreAdcBase, EssSabreDacBase)
│   │   ├── hal_relay.h/.cpp                # GPIO relay control (amplifier enable/disable)
│   │   ├── hal_button.h/.cpp               # GPIO button driver (debounce, ISR)
│   │   ├── hal_encoder.h/.cpp              # Rotary encoder (Gray code state machine)
│   │   ├── hal_buzzer.h/.cpp               # Piezo buzzer patterns (MCPWM, mutex for dual-core)
│   │   ├── hal_signal_gen.h/.cpp           # Software signal generator (software ADC source)
│   │   ├── hal_display.h/.cpp              # TFT display enable (GPIO 26 backlight)
│   │   ├── hal_led.h/.cpp                  # GPIO status LED (generic,status-led compatible)
│   │   ├── hal_ns4150b.h/.cpp              # Class-D amplifier driver (GPIO 53)
│   │   ├── hal_temp_sensor.h/.cpp          # ESP32-P4 internal temperature sensor
│   │   ├── hal_button.h/.cpp               # Push button input
│   │   ├── hal_types.h                     # HalDevice base class, HalInitResult, capabilities
│   │   ├── hal_eeprom.h                    # Generic alias to dac_eeprom.h
│   │   ├── hal_audio_interfaces.h          # AudioInputSource, AudioOutputSink abstracts
│   │   ├── hal_audio_device.h              # HalAudioDevice with getSinkCount/buildSinkAt
│   │   └── ...
│   ├── state/                  # Decomposed domain state (16 files)
│   │   ├── app_state.h                     # AppState singleton (30 lines, composition shell)
│   │   ├── enums.h                         # AppFSMState, FftWindowType, NetIfType (shared)
│   │   ├── general_state.h                 # timezoneOffset, dstOffset, darkMode, serialNumber
│   │   ├── audio_state.h                   # adcEnabled[], analysis, diagnostics, audioPaused
│   │   ├── dac_state.h                     # txUnderruns, EepromDiag (device fields in HalDeviceConfig)
│   │   ├── dsp_state.h                     # enabled, bypass, presets, swap diagnostics
│   │   ├── display_state.h                 # backlight, brightness, timeout, darkMode
│   │   ├── buzzer_state.h                  # volume, mute
│   │   ├── wifi_state.h                    # ssid, password, apMode, connectionStatus
│   │   ├── mqtt_state.h                    # broker config, connection state
│   │   ├── ethernet_state.h                # dhcp, ip, gateway, dns
│   │   ├── debug_state.h                   # debugMode, serialLevel, hwStats, heapCritical
│   │   ├── ota_state.h                     # latestVersion, downloadProgress, state enum
│   │   ├── signal_gen_state.h              # frequency, amplitude, waveform type
│   │   ├── usb_audio_state.h               # connected, sample rate (guarded by USB_AUDIO_ENABLED)
│   │   ├── hal_coord_state.h/.cpp          # Deferred device toggle queue (8 slots, overflow telemetry)
│   │   └── ...
│   ├── gui/                    # LVGL 9.4 GUI (9 files, guarded by GUI_ENABLED)
│   │   ├── gui_manager.h/.cpp              # Init, FreeRTOS task, sleep/wake
│   │   ├── gui_input.h/.cpp                # Rotary encoder Gray code state machine
│   │   ├── gui_theme.h/.cpp                # Orange accent, dark/light mode
│   │   ├── gui_navigation.h/.cpp           # Screen stack, push/pop, transitions
│   │   ├── gui_config.h                    # Pin config, display dimensions
│   │   ├── gui_icons.h                     # Material Design Icons inline SVG paths
│   │   ├── screens/                        # 10+ screen implementations
│   │   └── ...
│   ├── drivers/                # IC register definitions (24 files, _regs.h)
│   │   ├── ess_sabre_common.h             # Shared ESS family constants
│   │   ├── es8311_regs.h                  # Onboard codec
│   │   ├── es9*.h                         # Expansion DAC/ADC register maps
│   │   └── ...
│   ├── app_state.h/.cpp                    # Singleton accessor, dirty flags, event signaling
│   ├── app_events.h/.cpp                   # FreeRTOS event group (24 bits, 16 assigned)
│   ├── audio_pipeline.h/.cpp               # Core 1 audio routing engine (8-lane → 32×32 matrix → 16-slot)
│   ├── audio_input_source.h                # AudioInputSource virtual interface
│   ├── audio_output_sink.h                 # AudioOutputSink virtual interface (for HAL integration)
│   ├── i2s_audio.h/.cpp                    # Port-generic I2S driver (3 ports, STD/TDM)
│   ├── i2s_port_api.h/.cpp                 # REST endpoint for I2S port config
│   ├── dsp_pipeline.h/.cpp                 # Per-channel DSP sequencer (biquad, FIR, limiter, etc.)
│   ├── dsp_api.h/.cpp                      # 12 DSP REST endpoints (CRUD stages, presets, save)
│   ├── dsp_biquad_gen.h/.c                 # RBJ EQ Cookbook biquad coeff generator
│   ├── dsp_coefficients.h/.cpp             # Biquad/crossover/FIR computation
│   ├── dsp_convolution.h/.cpp              # Partitioned convolution (room correction IR)
│   ├── dsp_crossover.h/.cpp                # Crossover presets (LR2/LR4/LR8)
│   ├── dsp_rew_parser.h/.cpp               # Equalizer APO + miniDSP import/export
│   ├── output_dsp.h/.cpp                   # Per-output mono DSP (post-matrix, pre-sink)
│   ├── psram_alloc.h/.cpp                  # Unified PSRAM wrapper (fallback to SRAM)
│   ├── psram_api.h/.cpp                    # REST endpoint for PSRAM health
│   ├── heap_budget.h/.cpp                  # Per-subsystem allocation tracker
│   ├── signal_generator.h/.cpp             # Multi-waveform test signal (sine, square, noise, sweep)
│   ├── siggen_api.h/.cpp                   # REST endpoint for signal generator config
│   ├── smart_sensing.h/.cpp                # Voltage threshold detection, auto-off timer, relay control
│   ├── usb_audio.h/.cpp                    # TinyUSB UAC2 speaker device (guarded by USB_AUDIO_ENABLED)
│   ├── main.cpp                            # Entry point, server setup, main loop, endpoint registration
│   ├── config.h                            # Build-time constants (task stacks, pins, thresholds)
│   ├── globals.h                           # Global forward declarations
│   ├── utils.h/.cpp                        # Utility functions
│   ├── strings.h                           # String constants, translations, error messages
│   ├── design_tokens.h                     # Color palette, fonts, spacing (auto-extracted to CSS)
│   ├── wifi_manager.h/.cpp                 # Multi-network WiFi client + AP mode
│   ├── eth_manager.h/.cpp                  # Ethernet 100Mbps manager
│   ├── mqtt_handler.h/.cpp                 # MQTT broker connection + callback dispatch
│   ├── mqtt_task.h/.cpp                    # Dedicated Core 0 MQTT task (20 Hz poll)
│   ├── mqtt_publish.cpp                    # Change-detection statics + publish logic
│   ├── mqtt_ha_discovery.cpp               # Home Assistant YAML discovery (binary sensors, generic devices)
│   ├── auth_handler.h/.cpp                 # PBKDF2-SHA256 password auth, rate limiting
│   ├── button_handler.h/.cpp               # Debounce, press detection, multi-click
│   ├── buzzer_handler.h/.cpp               # Piezo buzzer patterns, ISR-safe encoder tick
│   ├── ota_updater.h/.cpp                  # GitHub release check, firmware download, SHA256 verify
│   ├── ota_certs.h                         # Root CA certs (Sectigo, DigiCert) + update tool
│   ├── settings_manager.h/.cpp             # /config.json atomic write, export v2.0, import
│   ├── debug_serial.h/.cpp                 # LOG_D/I/W/E macros, WS forwarding, color codes
│   ├── diag_journal.h/.cpp                 # DiagEvent ringbuffer (128 entries, 16 severity levels)
│   ├── diag_api.h/.cpp                     # REST endpoints for diagnostics snapshot + journal
│   ├── diag_event.h                        # DiagEvent struct, error codes enum
│   ├── diag_error_codes.h                  # 16-bit error code definitions (0xHHLL: high=subsystem)
│   ├── crash_log.h/.cpp                    # Exception handler, panic log storage
│   ├── task_monitor.h/.cpp                 # FreeRTOS task enumeration (stack, priority, core)
│   ├── thd_measurement.h/.cpp              # THD+N measurement (signal gen + FFT analysis)
│   ├── websocket_handler.h                 # WS public API (13 broadcast functions + binary frame defs)
│   ├── websocket_command.cpp               # WS event handler, command dispatch
│   ├── websocket_broadcast.cpp             # 17 state broadcast functions (sendDspState, sendHardwareStats, etc.)
│   ├── websocket_auth.cpp                  # Client auth tracking, session validation
│   ├── websocket_cpu_monitor.cpp           # FreeRTOS idle hook CPU usage tracking
│   ├── websocket_internal.h                # Cross-file accessor functions
│   ├── pipeline_api.h/.cpp                 # 4 REST endpoints for audio matrix CRUD
│   ├── dac_api.h/.cpp                      # REST endpoint for DAC state + volume control
│   ├── http_security.h                     # server_send() wrapper (X-Frame-Options, X-Content-Type-Options)
│   ├── web_pages.h/.cpp                    # Embedded gzip HTML/CSS/JS (auto-generated)
│   ├── web_pages_gz.cpp                    # Gzip-compressed assets (auto-generated)
│   ├── i2s_audio.h                         # VU metering, audio analysis (RMS, peak, FFT)
│   ├── safe_snr_sfdr.c                     # SFDR (Spurious-Free Dynamic Range) calculation
│   ├── login_page.h                        # HTML login form (embedded)
│   └── idf_component.yml                   # ESP-IDF component descriptor
├── test/                       # C++ unit tests (110 test modules, 3050+ tests)
│   ├── test_audio_pipeline/                # Audio routing + matrix + DSP integration
│   ├── test_hal_*/                         # 40+ HAL device driver tests
│   ├── test_dsp_*/                         # 15+ DSP stage + pipeline tests
│   ├── test_wifi/                          # WiFi manager + connection state
│   ├── test_mqtt/                          # MQTT handler + subscriptions
│   ├── test_auth/                          # PBKDF2 password validation
│   ├── test_settings*/                     # Settings load/save, export/import
│   ├── test_websocket*/                    # WS command dispatch, binary frames
│   ├── test_ota*/                          # OTA check, download, SHA256 verify
│   ├── test_api/                           # REST endpoint integration
│   ├── test_button/                        # Debounce, press detection
│   ├── test_buzzer/                        # Pattern sequencer
│   ├── test_gui_*/                         # LVGL screen navigation, input handling
│   ├── test_usb_audio/                     # UAC2 class, ring buffer
│   ├── test_diag_journal/                  # Diagnostic event logging
│   ├── test_crash_log/                     # Exception handler
│   ├── test_mocks/                         # Mock implementations (Arduino, WiFi, MQTT, Preferences)
│   │   ├── Arduino.h                       # Serial, digitalWrite, millis(), analogRead()
│   │   ├── WiFi.h                          # WiFi client/server stubs
│   │   ├── PubSubClient.h                  # MQTT client mock
│   │   ├── Preferences.h                   # NVS key/value store mock
│   │   └── ...
│   ├── test_utils/                         # Shared test utilities (asserts, fixtures)
│   └── README.md                           # Test setup + run instructions
├── web_src/                    # Web UI source (JavaScript modules + CSS)
│   ├── index.html              # HTML shell (body content, no inline CSS/JS)
│   ├── .eslintrc.json          # ESLint config with 380 globals (concatenated scope)
│   ├── css/                    # Split CSS (concatenated by build_web_assets.js)
│   │   ├── 00-tokens.css       # Design tokens (auto-generated from design_tokens.h)
│   │   ├── 01-variables.css    # CSS custom properties
│   │   ├── 02-layout.css       # Grid, flexbox, spacing
│   │   ├── 03-components.css   # Cards, buttons, forms
│   │   ├── 04-canvas.css       # Audio visualizer styling
│   │   └── 05-responsive.css   # Mobile/tablet breakpoints
│   └── js/                     # JS modules (loaded in order, shared scope)
│       ├── 01-core.js          # Global state, helpers (escapeHtml, safeJson)
│       ├── 02-ws-router.js     # WS connection, message router
│       ├── 03-app-state.js     # Client-side AppState mirror
│       ├── 04-shared-audio.js  # Shared audio constants
│       ├── 05-audio-tab.js     # Audio input visualization
│       ├── 06-canvas-helpers.js # Canvas utilities (waveform, spectrum drawing)
│       ├── 06-peq-overlay.js   # Parametric EQ UI overlay
│       ├── 07-ui-core.js       # UI core (switchTab, dialogs)
│       ├── 08-ui-status.js     # Status bar, connection indicator
│       ├── 09-audio-viz.js     # Audio analysis visualization
│       ├── 13-signal-gen.js    # Signal generator control
│       ├── 15-hal-devices.js   # HAL device cards, discovery, custom creator
│       ├── 15a-yaml-parser.js  # Custom device YAML parser
│       ├── 19-ethernet.js      # Ethernet settings
│       ├── 20-wifi-network.js  # WiFi network management
│       ├── 21-mqtt-settings.js # MQTT broker config
│       ├── 22-settings.js      # General settings, factory reset, export/import
│       ├── 23-firmware-update.js # OTA update UI
│       ├── 24-hardware-stats.js # Hardware health dashboard
│       ├── 25-debug-console.js # Debug console with filters
│       ├── 26-support.js       # Support/about page
│       ├── 27-auth.js          # Login form, session management
│       ├── 27a-health-dashboard.js # Health status cards (WiFi, MQTT, Audio)
│       ├── 28-init.js          # App initialization
│       └── 29-i2s-ports.js     # I2S port configuration cards
├── tools/                      # Build + utility scripts
│   ├── build_web_assets.js     # Assembles web_src → src/web_pages.cpp + _gz.cpp (gzip)
│   ├── extract_tokens.js       # Extracts src/design_tokens.h → CSS variables
│   ├── extract_api.js          # Extracts C++ doxygen comments → API docs (informational)
│   ├── find_dups.js            # Detects duplicate JavaScript declarations (pre-commit)
│   ├── check_missing_fns.js    # Detects undefined JS function references (pre-commit)
│   ├── update_certs.js         # Downloads + embeds root CA certs into src/ota_certs.h
│   ├── generate_docs.js        # Claude API orchestrator for Docusaurus doc generation
│   ├── detect_doc_changes.js   # Git diff → section mapping for incremental doc updates
│   ├── doc-mapping.json        # Source file → documentation section mapping (65 entries)
│   ├── prompts/                # Writing style templates (api-reference, user-guide, etc.)
│   └── ...
├── platformio.ini              # PlatformIO configuration (boards, envs, build flags)
├── CLAUDE.md                   # Project instructions (this file — read by all Claude instances)
├── REVIEW.md                   # Code review checklist
├── RELEASE_NOTES.md            # User-facing release history
├── README.md                   # Project overview
└── .githooks/                  # Pre-commit hooks (find_dups, check_missing_fns, ESLint)
```

## Directory Purposes

**src/**
- Purpose: Main firmware source code
- Contains: 107 files (55 headers, 49 C++, 2 C), 3 subdirs (hal, state, gui, drivers)
- Key files: main.cpp (entry point), app_state.h (state singleton), audio_pipeline.cpp (Core 1 audio engine), hal_device_manager.cpp (device registry)

**src/hal/**
- Purpose: Hardware abstraction layer — device drivers, discovery, lifecycle, persistence
- Contains: 57 files (30 drivers, base classes, manager, discovery, API)
- Key files: hal_device_manager.h (singleton), hal_discovery.cpp (3-tier auto-detect), hal_pipeline_bridge.cpp (audio integration), hal_api.cpp (13 REST endpoints)

**src/state/**
- Purpose: Decomposed domain-specific state (15 lightweight headers)
- Contains: 15 headers + 1 cpp (hal_coord_state only mutable)
- Key files: app_state.h (singleton shell, 30 lines), audio_state.h (adcEnabled, analysis, diagnostics), dsp_state.h (enabled, bypass, presets)

**src/gui/**
- Purpose: LVGL 9.4 GUI on ST7735S TFT display (Core 0, guarded by GUI_ENABLED)
- Contains: 9 files (manager, input, theme, navigation, screens/)
- Key files: gui_manager.cpp (FreeRTOS task, screen manager), gui_input.cpp (rotary encoder state machine), screens/* (10+ screen implementations)

**test/**
- Purpose: C++ unit tests (native platform, 3050+ tests, 110 modules)
- Contains: 110 test directories + test_mocks/
- Key files: test_mocks/Arduino.h (millis, Serial stubs), test_mocks/WiFi.h (WiFi mock)
- Run: `pio test -e native` (2-3 min)

**web_src/**
- Purpose: Web UI source (JavaScript modules, CSS, HTML)
- Contains: 25+ JS files (concatenated in order), 5 CSS files (split by concern), index.html, .eslintrc.json
- Key files: 01-core.js (global helpers), 02-ws-router.js (WS connection), 15-hal-devices.js (device UI)
- Build: `node tools/build_web_assets.js` → src/web_pages.cpp + web_pages_gz.cpp (auto-generated, committed)

**e2e/**
- Purpose: Browser E2E tests (Playwright, 113 tests, 22 specs, no hardware needed)
- Contains: mock-server/ (Express + WS), helpers/, tests/, fixtures/
- Key files: mock-server/server.js (port 3000), helpers/fixtures.js (connectedPage fixture), ws-state.js (deterministic mock)
- Run: `cd e2e && npx playwright test` (30 sec)

**docs-site/**
- Purpose: Public Docusaurus v3 documentation site (26 pages, auto-deployed to GitHub Pages)
- Contains: Markdown pages (sidebars.js, docusaurus.config.js), src/ (custom pages, CSS)
- Key files: sidebars.js (navigation structure), src/css/tokens.css (auto-generated from design_tokens.h)
- Deploy: `npm run build && npm run serve` (local), CI auto-deploys on main branch

**docs-internal/**
- Purpose: Internal design documentation (Mermaid diagrams, testing architecture, architecture flows)
- Contains: Markdown design docs, Mermaid flowcharts
- Key files: architecture/*.mmd (10 diagrams), testing-architecture.md, planning/test-strategy.md

## Key File Locations

**Entry Points:**
- `src/main.cpp`: Firmware boot + main loop (setup + Arduino loop)
- `src/app_state.h`: AppState singleton accessor
- `src/audio_pipeline.cpp`: Core 1 audio task (FreeRTOS)
- `src/websocket_command.cpp`: WS event handler

**Configuration:**
- `src/config.h`: Build-time constants (task stacks, pins, thresholds, feature flags)
- `platformio.ini`: PlatformIO board config, build flags (DAC_ENABLED, DSP_ENABLED, USB_AUDIO_ENABLED, GUI_ENABLED)
- `src/design_tokens.h`: Color palette, fonts, spacing (auto-extracted to web UI CSS)

**Core Logic:**
- `src/audio_pipeline.cpp`: 8-lane audio router, matrix mixer, per-output DSP, 16-slot sink dispatch (1139 lines)
- `src/i2s_audio.cpp`: Port-generic I2S driver (3 ports, STD/TDM), per-ADC analysis metrics (2377 lines)
- `src/dsp_pipeline.cpp`: DSP stage sequencer, coefficient morphing, glitch-free swaps (2408 lines)
- `src/hal/hal_device_manager.cpp`: Device registry, lifecycle state machine, pin claim tracking (430 lines)
- `src/hal/hal_discovery.cpp`: 3-tier auto-detection, I2C SDIO guard, probe retry (430 lines)

**Testing:**
- `test/test_mocks/`: Arduino/WiFi/MQTT/Preferences mock implementations
- `test/test_audio_pipeline/`: Audio routing + matrix + DSP integration tests
- `test/test_hal_*/`: 40+ HAL driver test modules
- `test/test_dsp_*/`: 15+ DSP stage + pipeline tests
- `e2e/tests/`: 22 Playwright spec files
- `e2e/fixtures/`: JSON response + WS message fixtures

**Networking:**
- `src/wifi_manager.cpp`: WiFi client + AP mode, multi-network support
- `src/mqtt_handler.cpp`: MQTT broker connection, callback dispatch
- `src/mqtt_task.cpp`: Dedicated Core 0 task (20 Hz publish loop)
- `src/mqtt_ha_discovery.cpp`: Home Assistant YAML discovery (binary sensors, generic devices)

**Web Integration:**
- `src/websocket_handler.h`: Public WS API (broadcast function declarations)
- `src/websocket_command.cpp`: WS command dispatch lookup table (1467 lines)
- `src/websocket_broadcast.cpp`: 17 state broadcast functions (1121 lines)
- `src/auth_handler.cpp`: PBKDF2 password validation, rate limiting (776 lines)

## Naming Conventions

**Files:**
- `module_name.cpp` / `module_name.h` — Paired source/header (camelCase, lowercase)
- `src/hal/hal_device_type.cpp` — HAL driver (hal_ prefix, device type name)
- `src/drivers/*_regs.h` — Register definitions (chip_regs.h naming, no .cpp companion)
- `src/state/*_state.h` — Domain state headers (no .cpp, stateless, header-only)
- `web_src/js/NN-feature-name.js` — Web modules (numeric prefix for load order, hyphenated names)
- `test/test_module_name/` — Test directory (one directory per test module to avoid duplicate main/setUp/tearDown symbols)

**Directories:**
- `src/hal/` — Hardware Abstraction Layer drivers
- `src/state/` — Domain-specific state (stateless headers)
- `src/gui/` — LVGL GUI components + screens
- `src/drivers/` — IC register definitions (_regs.h files only, no drivers)
- `test/` — C++ unit tests (110 test modules, 1 per directory)
- `web_src/` — Web UI source (js/, css/, index.html)
- `e2e/` — Browser E2E tests (tests/, helpers/, fixtures/, mock-server/)

**Functions:**
- Firmware: `camelCase` for non-public, UPPER_SNAKE_CASE for macros, `PascalCase` for classes
- Web: `camelCase` for functions and variables, `PascalCase` for classes/constructors

**Variables:**
- State: Nested via AppState (`appState.wifi.ssid`, `appState.audio.adcGain[0]`)
- HAL: Device references via manager (`hal_device_manager.getDevice(slot)`, `findByCompatible("ess,es9039pro")`)
- Web: Global scope (concatenated JS, all files share scope; use local `let`/`const` to avoid pollution)

## Where to Add New Code

**New HAL Driver:**
- Implementation: `src/hal/hal_device_type.h/.cpp` (inherit from `HalDevice` or base class)
- Register: Add one-liner in `src/hal/hal_builtin_devices.cpp` using `HAL_REGISTER()` macro
- Tests: Create `test/test_hal_device_type/` directory with test_hal_device_type.cpp
- Config: Optional per-device config via `HalDeviceConfig` (I2C/I2S/GPIO overrides)

**New DSP Stage Type:**
- Enum: Add stage type to `DspStageType` in `src/dsp_pipeline.h`
- Processor: Implement stage logic in `src/dsp_pipeline.cpp` `dsp_process_channel()` function
- Tests: Add test case to `test/test_dsp/test_dsp.cpp`
- API: Handled automatically via existing DSP API endpoints

**New Web UI Feature:**
- HTML: Add elements to `web_src/index.html` (assign unique IDs for selectors)
- JavaScript: Create module in `web_src/js/NNN-feature-name.js` (pick numeric prefix for load order)
  - Use shared helpers from `01-core.js` (escapeHtml, safeJson, switchTab)
  - Connect to WS via `ws.send()` or REST via `fetch()`
  - Update client-side `appState` on messages received
- CSS: Add styles to relevant file in `web_src/css/` (or create new file with NN- prefix)
- Tests: Create new .spec.js in `e2e/tests/` (use `connectedPage` fixture for auth + WS init)
- Build: Run `node tools/build_web_assets.js` to regenerate `src/web_pages.cpp` + `src/web_pages_gz.cpp` before building firmware

**New REST API Endpoint:**
- Handler: Create in dedicated file `src/feature_api.cpp/.h` (follow existing patterns in `src/*_api.cpp`)
  - Use `server_send()` wrapper for HTTP security headers
  - Validate auth cookie via `isAuthenticatedRequest(server)`
  - Return JSON with `{"success":bool, "error":"..."}` pattern
- Registration: Call handler registration function in `main.cpp` within `setup()` (e.g., `registerFeatureApiEndpoints()`)
- Tests: Add test module `test/test_api/test_api.cpp` with endpoint mock + request/response validation
- E2E: Add route in `e2e/mock-server/routes/feature.js` + test in `e2e/tests/feature.spec.js`

**Utilities & Helpers:**
- Shared C++ helpers: `src/utils.h/.cpp`
- Shared Web helpers: `web_src/js/01-core.js` (escapeHtml, safeJson, etc.)
- HAL helpers: `src/hal/hal_types.h` (hal_init_descriptor, hal_safe_strcpy macros)

## Special Directories

**src/drivers/**
- Purpose: IC register definitions (_regs.h files only, no C++ implementations)
- Generated: No (hand-authored from datasheets)
- Committed: Yes
- Note: Used by hal_es8311.cpp, hal_es9*.cpp to define register addresses and bit fields

**web_src/js/**
- Purpose: JavaScript modules (concatenated by build_web_assets.js in filename order)
- Generated: No (hand-authored, numeric prefix determines load order)
- Committed: Yes
- Note: All files share global scope (use local let/const); ESLint enforces 380 globals

**src/web_pages.cpp + src/web_pages_gz.cpp**
- Purpose: Embedded HTML/CSS/JS (gzipped)
- Generated: Yes (by `node tools/build_web_assets.js` from web_src/)
- Committed: Yes (auto-generated but committed for offline builds)
- Rebuild: After ANY edit to `web_src/`, run `node tools/build_web_assets.js` before `pio run`

**src/state/**
- Purpose: Lightweight domain headers (no implementations, no cpp files except hal_coord_state)
- Generated: No
- Committed: Yes
- Note: Reduce size by composing only headers you need; new domain headers should be <100 lines

**.planning/codebase/**
- Purpose: GSD codebase mapping output (ARCHITECTURE.md, STRUCTURE.md, CONVENTIONS.md, TESTING.md, CONCERNS.md)
- Generated: Yes (by `/gsd:map-codebase` command)
- Committed: Yes
- Usage: Consumed by `/gsd:plan-phase` and `/gsd:execute-phase` commands

---

*Structure analysis: 2026-03-23*
