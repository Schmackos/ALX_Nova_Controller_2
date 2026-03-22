# Codebase Structure

**Analysis Date:** 2026-03-22

## Directory Layout

```
ALX_Nova_Controller_2/
├── src/                    # Firmware source code (C++)
│   ├── main.cpp           # Entry point: setup() + loop()
│   ├── app_state.h        # AppState singleton (15 domain state headers)
│   ├── config.h           # Build config (pins, stack sizes, feature flags)
│   ├── audio_pipeline.h/.cpp  # 8→16→16 input/matrix/output audio routing
│   ├── i2s_audio.h/.cpp   # I2S ADC drivers, analysis buffers
│   ├── dsp_pipeline.h/.cpp # Double-buffered DSP stages (biquad, FIR, gain, limiter)
│   ├── output_dsp.h/.cpp  # Per-output DSP (mono processing)
│   ├── websocket_handler.cpp  # WS server (port 81), binary frame protocol
│   ├── auth_handler.cpp   # PBKDF2 auth, WS token pool
│   ├── http_security.h    # Security headers (X-Frame-Options, X-Content-Type-Options)
│   ├── mqtt_handler.cpp   # MQTT broker, callback dispatch
│   ├── mqtt_task.cpp      # FreeRTOS MQTT task (Core 0)
│   ├── mqtt_publish.cpp   # Change-detection statics, periodic publishes
│   ├── mqtt_ha_discovery.cpp  # Home Assistant discovery (binary_sensor for all devices)
│   ├── wifi_manager.cpp   # WiFi client + AP mode, scan, retry backoff
│   ├── settings_manager.cpp  # /config.json (primary) + settings.txt (fallback)
│   ├── ota_updater.cpp    # GitHub release check, SHA256 verification
│   ├── ota_certs.h        # Root CA certs (Sectigo, DigiCert)
│   ├── smart_sensing.cpp  # Voltage detection, auto-off timer, amplifier relay
│   ├── signal_generator.cpp  # Multi-waveform test signal (sine, square, noise, sweep)
│   ├── buzzer_handler.cpp # Piezo pattern sequencer, ISR-safe
│   ├── button_handler.cpp # Debounce, short/long/multi-click
│   ├── gui/               # LVGL GUI (ST7735S 128×160, Core 0)
│   │   ├── gui_manager.cpp   # Init, FreeRTOS task, sleep/wake
│   │   ├── gui_input.cpp     # Rotary encoder Gray code, button debounce
│   │   ├── gui_theme.cpp     # Orange accent, dark/light mode
│   │   ├── gui_navigation.cpp # Screen stack, push/pop, animations
│   │   └── screens/          # 8 screen implementations
│   ├── hal/               # Hardware Abstraction Layer (32-device registry)
│   │   ├── hal_device_manager.h/.cpp  # Lifecycle, discovery, pin tracking
│   │   ├── hal_device.h   # Base class, capability flags, state enum
│   │   ├── hal_discovery.h/.cpp  # 3-tier scan: I2C → EEPROM → manual
│   │   ├── hal_device_db.h/.cpp  # Built-in + LittleFS-persisted device configs
│   │   ├── hal_pipeline_bridge.h/.cpp  # State→sink/source registration, multi-sink/multi-source
│   │   ├── hal_api.h/.cpp # REST endpoints (13 routes, CRUD + discovery)
│   │   ├── hal_settings.h/.cpp  # /hal_config.json persistence
│   │   ├── hal_builtin_devices.h/.cpp  # HAL_REGISTER() macro, all driver factories
│   │   ├── hal_audio_device.h/.cpp  # Base for audio sources/sinks
│   │   ├── hal_pcm5102a.h/.cpp   # 2ch DAC (onboard, I2S)
│   │   ├── hal_es8311.h/.cpp     # Codec (onboard, I2C, Bus 1)
│   │   ├── hal_ns4150b.h/.cpp    # Class-D amp enable (GPIO 53)
│   │   ├── hal_relay.h/.cpp      # GPIO relay for amplifier
│   │   ├── hal_led.h/.cpp        # Status LED (GPIO 1)
│   │   ├── hal_button.h/.cpp     # Pushbutton (GPIO 36)
│   │   ├── hal_encoder.h/.cpp    # Rotary encoder (GPIO 32/33)
│   │   ├── hal_buzzer.h/.cpp     # Piezo buzzer (GPIO 45, PWM)
│   │   ├── hal_signal_gen.h/.cpp # Signal generator lane provider
│   │   ├── hal_display.h/.cpp    # TFT display
│   │   ├── hal_temp_sensor.h/.cpp # Chip temperature (IDF5)
│   │   ├── hal_eeprom_v3.h       # V3 mezzanine EEPROM format
│   │   ├── hal_es_sabre_adc_base.h/.cpp  # Abstract base for 9 ADC drivers
│   │   ├── hal_es9822pro.h/.cpp  # 2ch ADC (I2S, 16-bit vol)
│   │   ├── hal_es9826.h/.cpp     # 2ch ADC (I2S, 30dB gain)
│   │   ├── hal_es9823pro.h/.cpp  # 2ch ADC (I2S, 42dB gain, auto-detect PRO/MPRO)
│   │   ├── hal_es9821.h/.cpp     # 2ch ADC (I2S, no PGA)
│   │   ├── hal_es9820.h/.cpp     # 2ch ADC (I2S, 18dB gain)
│   │   ├── hal_es9843pro.h/.cpp  # 4ch ADC (TDM, 42dB gain per-channel)
│   │   ├── hal_es9842pro.h/.cpp  # 4ch ADC (TDM, 18dB gain)
│   │   ├── hal_es9841.h/.cpp     # 4ch ADC (TDM, 42dB gain, 0xFF=0dB)
│   │   ├── hal_es9840.h/.cpp     # 4ch ADC (TDM, 18dB gain)
│   │   ├── hal_ess_sabre_dac_base.h/.cpp  # Abstract base for 12 DAC drivers
│   │   ├── hal_es9038q2m.h/.cpp  # 2ch DAC (128dB, ultra-low power)
│   │   ├── hal_es9039q2m.h/.cpp  # 2ch DAC (130dB, HyperStream IV)
│   │   ├── hal_es9069q.h/.cpp    # 2ch DAC (130dB, MQA renderer)
│   │   ├── hal_es9033q.h/.cpp    # 2ch DAC (122dB, integrated line drivers)
│   │   ├── hal_es9020_dac.h/.cpp # 2ch DAC (122dB, APLL)
│   │   ├── hal_es9038pro.h/.cpp  # 8ch DAC (132dB, HyperStream II, 4 sink slots)
│   │   ├── hal_es9028pro.h/.cpp  # 8ch DAC (124dB, HyperStream II)
│   │   ├── hal_es9039pro.h/.cpp  # 8ch DAC (132dB, HyperStream IV, auto-detect PRO/MPRO)
│   │   ├── hal_es9027pro.h/.cpp  # 8ch DAC (124dB, HyperStream IV)
│   │   ├── hal_es9081.h/.cpp     # 8ch DAC (120dB, 40-QFN)
│   │   ├── hal_es9082.h/.cpp     # 8ch DAC (120dB, 48-QFN, ASP2/SIS)
│   │   ├── hal_es9017.h/.cpp     # 8ch DAC (120dB, ES9027PRO pin-compat)
│   │   ├── hal_tdm_interleaver.h/.cpp  # TDM frame combiner for 8ch DAC output
│   │   ├── hal_tdm_deinterleaver.h/.cpp # TDM frame splitter for 4ch ADC input
│   │   ├── hal_dsp_bridge.h/.cpp # DSP device wrapper for pipeline
│   │   ├── hal_driver_registry.h  # Driver factory type
│   │   ├── hal_custom_device.h/.cpp  # Manual device creation from REST API
│   │   └── hal_types.h    # Enums, structs, hal_init_descriptor() inline
│   ├── dac_hal.h/.cpp     # DAC state, enable/disable, toggle queue
│   ├── dac_api.h/.cpp     # REST endpoints (volume, mute, enable, capabilities)
│   ├── dac_eeprom.h/.cpp  # EEPROM diagnostics (CapacityError, HalModel)
│   ├── pipeline_api.h/.cpp # REST endpoints (matrix, sink DSP CRUD)
│   ├── psram_api.h/.cpp   # REST endpoint (PSRAM status, budget array)
│   ├── psram_alloc.h/.cpp # Unified PSRAM/SRAM wrapper, auto heap_budget recording
│   ├── heap_budget.h/.cpp # Per-subsystem allocation tracker (32 entries)
│   ├── dsp_api.h/.cpp     # REST endpoints (DSP import/export, stage CRUD)
│   ├── dsp_coefficients.h/.cpp  # RBJ biquad coefficient generation
│   ├── dsp_biquad_gen.h/c    # dsp_gen_* functions (renamed from dsps_)
│   ├── dsp_crossover.cpp  # Crossover presets (LR2/LR4/LR8, Butterworth)
│   ├── dsp_rew_parser.h/.cpp  # Equalizer APO, miniDSP, WAV IR import/export
│   ├── dsp_convolution.h/.cpp # FIR convolution engine (PSRAM-preferred)
│   ├── output_dsp.h/.cpp  # Per-output mono DSP (post-matrix/pre-sink)
│   ├── sink_write_utils.h/.cpp  # Shared sink utilities (volume, mute ramp, F32→I32S)
│   ├── audio_input_source.h  # Interface for input lane sources
│   ├── audio_output_sink.h   # Interface for output slot sinks
│   ├── app_events.h/.cpp  # Event group (24 usable bits, EVT_ANY)
│   ├── app_state.cpp      # AppState implementation (singleton, dirty flags)
│   ├── diag_journal.h/.cpp # DiagEvent ring buffer (256 max), auto-dedup
│   ├── diag_event.h       # DiagEvent struct, severity enum
│   ├── diag_error_codes.h # All diagnostic codes (0x10xx HAL, 0x20xx audio, etc.)
│   ├── debug_serial.h/.cpp # Log macros (LOG_D/I/W/E), level control
│   ├── crash_log.h/.cpp   # Stack trace recovery from RTC memory
│   ├── task_monitor.cpp   # FreeRTOS task enumeration (stack, priority, core)
│   ├── eth_manager.h/.cpp # Ethernet initialization (placeholder)
│   ├── globals.h          # extern declarations (mqttClient, server, webSocket)
│   ├── design_tokens.h    # Theme colors/sizes (extracted to CSS via tools/extract_tokens.js)
│   ├── strings.h          # Localized strings (18KB of UI text)
│   ├── utils.h            # Utility functions (version compare, JSON helpers)
│   ├── web_pages.cpp/.h   # Auto-generated (DO NOT EDIT): HTML asset embedding
│   ├── web_pages_gz.cpp   # Auto-generated: gzip-compressed assets
│   ├── login_page.h       # Embedded auth login HTML
│   ├── usb_audio.cpp      # TinyUSB UAC2 speaker device (guarded by USB_AUDIO_ENABLED)
│   ├── state/             # 15 domain state headers (composed in AppState)
│   │   ├── enums.h        # AppFSMState, FftWindowType, NetIfType
│   │   ├── general_state.h # tzOffset, dstOffset, darkMode, serialNumber
│   │   ├── ota_state.h    # Update info, release data
│   │   ├── audio_state.h  # ADC enabled, I2S metrics, sensing, DMA diagnostics
│   │   ├── dac_state.h    # TX underruns, EEPROM diagnostics (minimal, device state in HAL)
│   │   ├── dsp_state.h    # Enabled, bypass, presets, swap diagnostics
│   │   ├── display_state.h # Brightness, timeout, dim, backlight
│   │   ├── buzzer_state.h # Enabled, volume
│   │   ├── signal_gen_state.h # Waveform, freq, amp, PWM mode
│   │   ├── usb_audio_state.h # Connection, streaming, device name (guarded)
│   │   ├── wifi_state.h   # SSID, password, AP mode, connection state
│   │   ├── mqtt_state.h   # Broker config, connection, topics
│   │   ├── ethernet_state.h # IP config (placeholder)
│   │   ├── debug_state.h  # Debug mode, serial level, hw stats, heapCritical
│   │   └── hal_coord_state.h # Deferred toggle queue (capacity 8), overflow telemetry
│   └── drivers/           # Vendor chip register definitions
│       ├── ess_sabre_common.h   # ESS SABRE family constants
│       ├── es8311_regs.h  # Codec register map
│       ├── es9*_regs.h    # ESS SABRE register maps (9 ADC + 12 DAC)
│
├── test/                  # C++ Unit Tests (Unity framework, native platform)
│   ├── README.md          # Test infrastructure overview
│   ├── test_*/            # 105 test modules (each ~1-2 Unity tests)
│   │   ├── test_wifi/     # WiFi connection, scan, AP mode
│   │   ├── test_mqtt/     # MQTT client, payload serialization
│   │   ├── test_audio_pipeline/  # Matrix routing, bypass, gain
│   │   ├── test_dsp_pipeline/    # Biquad, FIR, coefficients
│   │   ├── test_hal_*/    # All 30+ HAL drivers (init, config, lifecycle)
│   │   ├── test_auth/     # PBKDF2, token pool, cookie validation
│   │   └── ...            # 100+ more (see CLAUDE.md for full list)
│   └── test_mocks/        # Mock implementations (Arduino, WiFi, MQTT, Preferences)
│
├── web_src/               # Web UI source (HTML/CSS/JS, auto-compiled to web_pages.cpp)
│   ├── index.html         # Shell (body injected at build time)
│   ├── css/               # Modular stylesheets (split by concern)
│   │   ├── 01-variables.css  # Design tokens, colors, sizes
│   │   ├── 02-layout.css     # Grid, flex, sidebar, tabs
│   │   ├── 03-components.css # Buttons, inputs, cards, modals
│   │   ├── 04-canvas.css     # Waveform/spectrum visualization
│   │   └── 05-responsive.css # Mobile breakpoints
│   ├── js/                # 28 JavaScript modules (concatenated, shared scope)
│   │   ├── 01-core.js           # HTTP/WS client, fetch wrapper, alert/confirm
│   │   ├── 02-ws-router.js      # WS message dispatcher (commandDone, broadcasts)
│   │   ├── 03-app-state.js      # Client-side state mirror
│   │   ├── 04-shared-audio.js   # Audio tab data, matrix, DSP shared logic
│   │   ├── 05-audio-tab.js      # Audio UI (matrix, routing, DSP editor)
│   │   ├── 06-peq-overlay.js    # PEQ curve editor (modal)
│   │   ├── 07-ui-core.js        # Common UI helpers (modal, toast, layout)
│   │   ├── 08-ui-status.js      # Status bar indicators
│   │   ├── 09-audio-viz.js      # Waveform + spectrum canvas rendering
│   │   ├── 13-signal-gen.js     # Signal generator control
│   │   ├── 15-hal-devices.js    # HAL device CRUD, discovery, config
│   │   ├── 15a-yaml-parser.js   # YAML preset parsing
│   │   ├── 20-wifi-network.js   # WiFi scan, connect, AP mode
│   │   ├── 21-mqtt-settings.js  # MQTT broker config, HA topics
│   │   ├── 22-settings.js       # Display, audio, factory reset
│   │   ├── 23-firmware-update.js # OTA version check, download progress
│   │   ├── 24-hardware-stats.js # Heap, PSRAM, task, pin telemetry
│   │   ├── 25-debug-console.js  # Serial log viewer (filtering, search)
│   │   ├── 26-support.js        # About, links, crash log viewer
│   │   ├── 27-auth.js           # Login, logout, password reset, token refresh
│   │   ├── 27a-health-dashboard.js # Health cards (audio, HAL, memory)
│   │   └── 28-init.js           # WS connect, initial state broadcast handler
│   ├── .eslintrc.json     # ESLint config (380+ globals for concatenated scope)
│
├── e2e/                   # Browser E2E Tests (Playwright, 113 tests across 22 specs)
│   ├── tests/             # Spec files (*.spec.js)
│   │   ├── auth.spec.js         # Login, cookie, token pool
│   │   ├── audio.spec.js        # Matrix, routing, DSP editing
│   │   ├── devices.spec.js      # HAL CRUD, discovery, config
│   │   ├── wifi.spec.js         # Scan, connect, AP mode toggle
│   │   ├── mqtt.spec.js         # Broker config, connection, subscriptions
│   │   └── ...                  # 17 more specs
│   ├── helpers/           # Reusable test utilities
│   │   ├── fixtures.js          # connectedPage fixture (auth + WS handshake)
│   │   ├── ws-helpers.js        # buildInitialState(), handleCommand(), binary frames
│   │   └── selectors.js         # DOM ID mappings from index.html
│   ├── fixtures/          # Pre-built test data
│   │   ├── ws-messages/         # 15 mock WS broadcast JSON samples
│   │   └── api-responses/       # 14 mock REST response JSON files
│   ├── mock-server/       # Express mock server (port 3000)
│   │   ├── server.js            # Main server setup, route mounting
│   │   ├── assembler.js         # Replicates build_web_assets.js HTML assembly
│   │   ├── ws-state.js          # Mock state singleton (reset per test)
│   │   └── routes/              # 12 Express route files (auth, WiFi, MQTT, HAL, etc.)
│   └── package.json       # Playwright deps (test runner)
│
├── docs-site/             # Docusaurus v3 documentation (public, deployed to GH Pages)
│   ├── docusaurus.config.js  # Mermaid, dark mode default, routeBasePath: 'docs'
│   ├── sidebars.js           # userSidebar (9 items) + devSidebar (17 items)
│   ├── docs/                 # 26 markdown pages
│   │   ├── user/             # 9 pages: Getting Started, Setup, WiFi, MQTT, etc.
│   │   └── developer/        # 17 pages: Architecture, API, HAL, Testing, DSP, etc.
│   ├── src/                  # Docusaurus components + landing page
│   │   ├── pages/index.js    # Hero landing + feature cards
│   │   └── css/              # tokens.css (auto-generated from src/design_tokens.h)
│   └── build/               # Generated static site (NOT committed)
│
├── docs-internal/         # Internal working docs (non-public)
│   ├── testing-architecture.md # Test infrastructure deep-dive
│   ├── architecture/       # 10 Mermaid diagrams (CI, tasks, test flow, etc.)
│   └── planning/           # Phase plans, design notes
│
├── tools/                 # Build & maintenance scripts
│   ├── build_web_assets.js     # Compiles web_src/ → src/web_pages.cpp
│   ├── extract_tokens.js       # Extracts src/design_tokens.h → CSS files
│   ├── update_certs.js         # Downloads root CAs → src/ota_certs.h
│   ├── extract_api.js          # Parses C++ source for API doc generation
│   ├── find_dups.js            # CI: Detects duplicate JS declarations
│   ├── check_missing_fns.js    # CI: Finds undefined function references
│   ├── generate_docs.js        # Claude API orchestrator (doc regen)
│   ├── detect_doc_changes.js   # Git diff → doc section mapping
│   └── doc-mapping.json        # Source file → documentation section mapping (65 entries)
│
├── lib/                   # Third-party libraries (vendored + overrides)
│   ├── WebSockets/        # WebSocket library (vendored, no longer from registry)
│   ├── esp_dsp_lite/      # ANSI C fallback for native tests (lib_ignore'd on ESP32)
│   └── ...                # Arduino standard libs (included via platformio.ini)
│
├── platformio.ini         # PlatformIO config (esp32-p4 board, compiler flags)
├── .github/workflows/     # CI/CD pipeline
│   ├── tests.yml          # 4 parallel gates (cpp-tests, cpp-lint, js-lint, e2e-tests)
│   └── docs.yml           # Doc regen trigger (on changes detected)
├── .githooks/pre-commit   # Pre-commit checks (find_dups, check_missing_fns, ESLint)
│
├── CLAUDE.md              # Project instructions (this file, checked into repo)
└── .planning/codebase/    # GSD analysis output (ARCHITECTURE.md, STRUCTURE.md, etc.)
```

## Directory Purposes

**src/**
- Purpose: All firmware source code (C++)
- Contains: Main loop, HAL devices, audio pipeline, web APIs, state management
- Key files: `main.cpp`, `app_state.h`, `audio_pipeline.h`, `hal/hal_device_manager.h`

**src/hal/**
- Purpose: Hardware Abstraction Layer — all device drivers and discovery
- Contains: 44 drivers (onboard + expansion), device registry, pin tracking, lifecycle callbacks
- Key files: `hal_device_manager.h`, `hal_discovery.h`, `hal_pipeline_bridge.h`

**src/gui/**
- Purpose: LVGL-based local display interface
- Contains: Screen implementations, navigation, input handling, theme management
- Key files: `gui_manager.cpp`, `gui_navigation.cpp`, screen files in `screens/`

**src/state/**
- Purpose: Domain-specific state structs (composed into AppState)
- Contains: 15 lightweight headers for WiFi, MQTT, Audio, DAC, DSP, etc.
- Key files: One `*_state.h` per domain

**test/**
- Purpose: C++ unit tests (105 modules, native platform)
- Contains: Test modules, mock implementations, test data fixtures
- Key files: Each `test_*/` directory has `test_main.cpp` or individual test `.cpp` files

**web_src/**
- Purpose: Web UI source (HTML, CSS, JavaScript)
- Contains: Single-page app with tab navigation, real-time visualization, REST API client
- **IMPORTANT:** Edit files here, NOT `src/web_pages.cpp` (auto-generated). After ANY edit, run `node tools/build_web_assets.js` before building firmware.
- Key files: `index.html`, `css/`, `js/` (28 modules)

**e2e/**
- Purpose: Playwright browser E2E tests (113 tests across 22 specs)
- Contains: Test specs, mock server, test helpers, fixture data
- Key files: `tests/*.spec.js`, `mock-server/server.js`, `helpers/`

**docs-site/**
- Purpose: Docusaurus v3 public documentation (26 pages)
- Contains: User guide, developer reference, landing page, design tokens
- Key files: `docs/user/` + `docs/developer/`, `docusaurus.config.js`

**tools/**
- Purpose: Build automation and code generation
- Contains: Web asset compilation, certificate updates, CI helper scripts
- Key files: `build_web_assets.js`, `extract_tokens.js`, `update_certs.js`

## Key File Locations

**Entry Points:**
- `src/main.cpp`: Firmware boot (setup/loop), HTTP route registration, FreeRTOS task spawning
- `web_src/index.html`: Web UI shell (body injected at compile-time)
- `e2e/tests/*.spec.js`: E2E test entry points

**Configuration:**
- `src/config.h`: Build flags, pin assignments, stack sizes, feature toggles
- `platformio.ini`: PlatformIO board config, compiler flags, library dependencies
- `web_src/.eslintrc.json`: JavaScript linting rules (380+ globals)
- `docusaurus.config.js`: Documentation site configuration

**Core Logic:**
- `src/app_state.h`: Global state singleton (15 domain structs)
- `src/audio_pipeline.h/.cpp`: Input lanes → matrix → output sinks
- `src/i2s_audio.h/.cpp`: ADC acquisition, analysis (RMS/VU/peak), diagnostics
- `src/dsp_pipeline.h/.cpp`: Double-buffered DSP stages, coefficient generation
- `src/hal/hal_device_manager.h/.cpp`: Device registry, lifecycle, discovery
- `src/hal/hal_pipeline_bridge.h/.cpp`: HAL ↔ audio pipeline integration
- `src/websocket_handler.cpp`: Real-time WebSocket protocol, binary frames

**Testing:**
- `test/test_*/`: 105 C++ unit test modules
- `e2e/tests/`: 22 Playwright spec files
- `test/test_mocks/`: Arduino/WiFi/MQTT mock implementations

## Naming Conventions

**Files:**
- C++ source: `module.cpp` / `module.h` (e.g., `audio_pipeline.cpp`)
- Drivers: `hal_<device>.h/.cpp` (e.g., `hal_es9038pro.h`)
- State: `<domain>_state.h` in `src/state/` (e.g., `audio_state.h`)
- Tests: `test_<module>/test_main.cpp` or `test_<module>.cpp` (e.g., `test_audio_pipeline/`)
- Web assets: `NN-description.{css,js}` with load order via prefix (e.g., `05-audio-tab.js`)

**Directories:**
- Subsystems: lowercase with underscores (e.g., `src/hal/`, `src/gui/`)
- Expanded names: `src/state/`, `src/drivers/` for file grouping

**Functions:**
- C: snake_case with module prefix (e.g., `audio_pipeline_set_source()`)
- C++: camelCase in classes (e.g., `HalDeviceManager::registerDevice()`)
- Callbacks: `on` + CamelCase (e.g., `onHalStateChange()`, `onWsEvent()`)

**Variables:**
- Public: snake_case (e.g., `fsmState`, `isReady`)
- Private: underscore prefix (e.g., `_devices`, `_currentCfg`)
- Global singletons: no prefix (e.g., `appState`, accessed via `getInstance()`)

**Types:**
- Structs: PascalCase (e.g., `AudioState`, `HalDevice`)
- Enums: SCREAMING_SNAKE_CASE (e.g., `STATE_IDLE`, `HAL_CAP_DAC_PATH`)
- Class templates: PascalCase + Template suffix (e.g., `AudioInputSource`)

## Where to Add New Code

**New Audio Feature (DSP stage, filter, analyzer):**
- Primary: `src/dsp_pipeline.h/.cpp` (stage registration) or `src/output_dsp.h/.cpp` (mono processing)
- Config/API: `src/dsp_api.cpp` (REST endpoints if needed)
- Tests: `test/test_dsp_*/` directory
- Example: Adding a compressor stage → add to DspStageType enum, implement apply function, register in dsp_add_stage()

**New HAL Device (expansion driver or onboard control):**
- Driver: `src/hal/hal_<device>.h/.cpp` — inherit from HalDevice or HalAudioDevice
- Registration: Entry in `hal_builtin_devices.cpp` using `HAL_REGISTER()` macro
- Tests: `test/test_hal_<device>/test_main.cpp`
- Docs: `docs-site/docs/developer/hal/` reference
- Example: New GPIO relay → HalRelay class, COMPATIBLE_STRING entry, test coverage

**New REST Endpoint:**
- Implementation: New handler function in relevant `src/*_api.cpp` file (or create new)
- Registration: `server.on("/api/path", HTTP_GET, handler)` in `main.cpp`
- Response: Use `sendGzipped()` for JSON (handles gzip + security headers)
- Auth: Call `auth_check_token(server)` before processing
- Tests: Add Playwright spec in `e2e/tests/` if UI depends on it

**New Web UI Tab:**
- HTML: Add button to sidebar/tab-bar in `web_src/index.html` with `data-tab="name"` + `onclick="switchTab('name')"`
- JavaScript: New module `web_src/js/NN-name.js` with global `initTabName()` function
- CSS: Add styles to `web_src/css/` (modular by concern)
- WS Handler: If real-time data needed, add case in `ws_router` (in `02-ws-router.js`)
- Tests: `e2e/tests/name.spec.js` verifying toggle/input behavior and WS/REST calls
- After ANY edit: Run `node tools/build_web_assets.js` before building

**New Configuration Setting:**
- State: Add field to relevant domain state struct in `src/state/` (e.g., `general_state.h`)
- Persistence: Add JSON key to `settings_manager.cpp` load/save
- UI: Form input in `web_src/` with `POST /api/settings` or domain-specific endpoint
- API: Optional REST getter/setter in domain `*_api.cpp`
- Example: New timezone offset → add to GeneralState, JSON serialize in settings_manager, UI input in settings.js

**Utilities/Helpers:**
- Shared helpers: `src/utils.h` (small utilities) or new `src/helper.h/.cpp` (larger modules)
- No global state in helpers — pass AppState or config as parameters
- Reusable JS: `web_src/js/01-core.js` (HTTP/WS primitives) or new utility module before consumer

**Tests:**
- C++ unit: `test/test_<module>/` directory with test fixtures + mocks
- E2E: `e2e/tests/feature.spec.js` using Playwright fixtures + selectors
- Before committing: Run `pio test -e native -v` (C++) and `npx playwright test` (E2E)

## Special Directories

**src/state/**
- Purpose: Domain-specific state structs (15 total), composed into AppState
- Generated: No — hand-maintained, one per domain
- Committed: Yes
- Usage: `appState.domain.field` (e.g., `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`)

**src/drivers/**
- Purpose: Vendor chip register definitions (ESS SABRE, ES8311)
- Generated: No — extracted from datasheets
- Committed: Yes
- Usage: Included by HAL drivers for low-level I2C register control

**src/hal/**
- Purpose: Hardware abstraction layer (drivers + lifecycle)
- Generated: No — hand-implemented per device
- Committed: Yes
- Key: All 44 drivers (onboard + expansion) live here; discovery + bridge orchestrate them

**web_src/**
- Purpose: Web UI source (DO NOT EDIT src/web_pages.cpp directly)
- Generated: No — this is the source
- Auto-Generated From: `build_web_assets.js` compiles web_src/ → src/web_pages.cpp (compiled into firmware)
- Committed: Yes
- **CRITICAL:** After ANY edit to web_src/, run `node tools/build_web_assets.js` before building firmware

**e2e/mock-server/**
- Purpose: Mock REST/WS server for E2E tests (no real hardware)
- Generated: No — hand-maintained
- Committed: Yes
- Usage: Replaces live firmware during Playwright tests; deterministic state + fixtures

**docs-site/**
- Purpose: Public documentation (Docusaurus v3)
- Generated: docs content auto-updated by `tools/generate_docs.js` on CI
- Committed: Yes (except build/ which is excluded)
- Deploy: `npm run build && npm run serve` locally, or GitHub Actions to gh-pages on merge

**logs/**
- Purpose: Build artifacts, test reports, serial captures
- Generated: Yes (all build/test output)
- Committed: No (.gitignore)
- Usage: Keep project root clean; save all `.log` files here

---

*Structure analysis: 2026-03-22*
