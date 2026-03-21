# Codebase Structure

**Analysis Date:** 2026-03-21

## Directory Layout

```
project-root/
├── src/                        # Firmware source (ESP32-P4 Arduino + FreeRTOS)
│   ├── main.cpp               # Entry point: setup(), Arduino loop, REST endpoints, HAL init
│   ├── app_state.h/.cpp       # AppState singleton (~80 lines, composition of 15 domain headers)
│   ├── app_events.h/.cpp      # Event group + dirty-flag signaling (24-bit, 16 bits assigned)
│   ├── config.h               # Pin definitions, build flags, FreeRTOS stack/priority config
│   ├── globals.h              # Global variable declarations (mqttClient, webSocket, resetButton)
│   ├── design_tokens.h        # Theme colors, typography (auto-synced to CSS via tools/extract_tokens.js)
│   │
│   ├── state/                 # Domain-specific state headers (15 total)
│   │   ├── enums.h            # AppFSMState, FftWindowType, NetIfType (shared enums)
│   │   ├── general_state.h    # GeneralState (timezone, DST, darkMode, certValidation, serial)
│   │   ├── audio_state.h      # AudioState (adcEnabled[], sensing fields, pipeline bypass)
│   │   ├── dac_state.h        # DacState (txUnderruns, eepromDiag only — device config in HalDeviceConfig)
│   │   ├── dsp_state.h        # DspSettingsState (enabled, bypass, presets, swap diagnostics)
│   │   ├── display_state.h    # DisplayState (backlight, brightness, dim timeout)
│   │   ├── buzzer_state.h     # BuzzerState (enabled, volume, pattern)
│   │   ├── wifi_state.h       # WifiState (SSID, password, AP mode, connection status)
│   │   ├── mqtt_state.h       # MqttState (broker, port, auth, connection state)
│   │   ├── ota_state.h        # OtaState + ReleaseInfo (version check, download progress)
│   │   ├── debug_state.h      # DebugState (debug mode, serial level, hw stats)
│   │   ├── ethernet_state.h   # EthernetState (connection status, IP)
│   │   ├── usb_audio_state.h  # UsbAudioState (guarded by USB_AUDIO_ENABLED)
│   │   ├── signal_gen_state.h # SignalGenState (waveform, frequency, amplitude)
│   │   └── hal_coord_state.h  # HalCoordState (deferred toggle queue, capacity 8, same-slot dedup)
│   │
│   ├── hal/                   # Hardware Abstraction Layer (15 files)
│   │   ├── hal_types.h        # Enums (HalDeviceType, HalDeviceState, HalDiscovery), structs (HalDeviceDescriptor, HalDeviceConfig), constants
│   │   ├── hal_device.h       # Abstract base class (probe, init, deinit, healthCheck, getInputSource, buildSink)
│   │   ├── hal_device_manager.h/.cpp  # Singleton managing up to 24 devices, pin tracking (56 pins), device registry
│   │   ├── hal_driver_registry.h      # Compatible string → factory function mapping
│   │   ├── hal_device_db.h/.cpp       # Builtin devices + LittleFS JSON persistence (/hal_config.json)
│   │   ├── hal_discovery.h/.cpp       # 3-tier discovery (I2C scan → EEPROM → manual config), skips Bus 0 on WiFi
│   │   ├── hal_pipeline_bridge.h/.cpp # Connects device lifecycle to audio pipeline (capability-based ordinal assignment)
│   │   ├── hal_audio_health_bridge.h/.cpp # Audio health status integration
│   │   ├── hal_i2s_bridge.h/.cpp      # I2S TX pin/clock resolution (respects HalDeviceConfig overrides)
│   │   ├── hal_settings.h/.cpp        # Per-device config persistence (load/save to /hal_config.json)
│   │   ├── hal_api.h/.cpp             # 13 REST endpoints (device CRUD, discovery, config, database)
│   │   ├── hal_audio_device.h         # Base class for audio devices (DAC, ADC, codec)
│   │   ├── hal_audio_interfaces.h     # Traits for I2C/I2S bus detection
│   │   ├── hal_eeprom_v3.h/.cpp       # AT24C02 EEPROM probe + string matching (Phase 3)
│   │   ├── hal_custom_device.h/.cpp   # User-defined device templates
│   │   ├── hal_init_result.h          # Return struct with error code + reason
│   │   │
│   │   ├── Builtin Audio Drivers:
│   │   │   ├── hal_pcm5102a.h/.cpp    # DAC (I2S, no I2C control)
│   │   │   ├── hal_es8311.h/.cpp      # Codec (I2C, dedicated bus 1)
│   │   │   ├── hal_pcm1808.h/.cpp     # ADC (I2S, dual instance support)
│   │   │   ├── hal_mcp4725.h/.cpp     # DAC (I2C, 12-bit, 2 instances)
│   │   │   └── hal_dsp_bridge.h/.cpp  # DSP processing as HAL pseudo-device
│   │   │
│   │   ├── Builtin GPIO/Misc Drivers:
│   │   │   ├── hal_ns4150b.h/.cpp     # Class-D amp (GPIO enable on pin 53)
│   │   │   ├── hal_relay.h/.cpp       # Amplifier relay (GPIO control)
│   │   │   ├── hal_temp_sensor.h/.cpp # Internal chip temp sensor
│   │   │   ├── hal_led.h/.cpp         # LED (GPIO low forever)
│   │   │   ├── hal_button.h/.cpp      # GPIO debounced button
│   │   │   ├── hal_encoder.h/.cpp     # Rotary encoder with Gray code (ISR-driven)
│   │   │   ├── hal_buzzer.h/.cpp      # Piezo with LEDC PWM
│   │   │   ├── hal_siggen.h/.cpp      # HalSignalGen — software ADC source registration
│   │   │   ├── hal_signal_gen.h/.cpp  # HalSigGen — PWM output (alx,signal-gen)
│   │   │   ├── hal_display.h/.cpp     # TFT display manager
│   │   │   └── hal_usb_audio.h/.cpp   # TinyUSB UAC2 audio device
│   │   │
│   │   └── hal_builtin_devices.h/.cpp # Device registry: compatible → factory function mapping (8 integrated drivers)
│   │
│   ├── Audio Pipeline:
│   │   ├── audio_pipeline.h/.cpp      # 8-lane input → 16×16 matrix → 8-slot output (FreeRTOS task on Core 1)
│   │   ├── audio_input_source.h       # Lane-indexed source descriptor (read callback, name, halSlot)
│   │   ├── audio_output_sink.h        # Slot-indexed sink descriptor (write callback, isReady, halSlot)
│   │   ├── i2s_audio.h/.cpp           # Dual I2S ADC (synchronized masters), DMA, FFT analysis, health check
│   │   ├── dsp_pipeline.h/.cpp        # Per-input DSP (biquad IIR, FIR, limiter, compressor)
│   │   ├── output_dsp.h/.cpp          # Per-output DSP (post-matrix)
│   │   ├── dsp_coefficients.h/.cpp    # RBJ Audio EQ Cookbook biquad coefficient generation
│   │   ├── dsp_biquad_gen.h           # Inline biquad gen (renamed from dsps_biquad_gen_* to avoid conflicts)
│   │   ├── dsp_crossover.h/.cpp       # Crossover presets (LR2/LR4/LR8, Butterworth)
│   │   ├── dsp_convolution.h/.cpp     # FIR convolution / impulse response
│   │   ├── dsp_rew_parser.h/.cpp      # Equalizer APO + miniDSP import/export, WAV IR loading
│   │   ├── dsp_api.h/.cpp             # REST API for DSP config (GET/POST biquads, presets, swap)
│   │   ├── pipeline_api.h/.cpp        # REST API for matrix routing + per-output DSP
│   │   ├── dac_api.h/.cpp             # REST API for DAC enable/disable, volume
│   │   ├── dac_hal.h/.cpp             # DAC subsystem bus utilities (volume curves, periodic logging via HAL sink loop)
│   │   ├── dac_eeprom.h/.cpp          # EEPROM diagnostics for DAC state
│   │   ├── dac_settings.h/.cpp        # DAC-specific settings persistence (filter mode, etc.)
│   │   ├── sink_write_utils.h/.cpp    # Shared float buffer utilities (volume ramp, float→I2S conversion)
│   │   ├── thd_measurement.h/.cpp     # THD+N measurement engine with signal generator
│   │   └── usb_audio.h/.cpp           # TinyUSB UAC2 speaker device (Core 0 task, guarded by USB_AUDIO_ENABLED)
│   │
│   ├── Networking & Connectivity:
│   │   ├── wifi_manager.h/.cpp        # WiFi client + AP mode, async connect, backoff
│   │   ├── mqtt_handler.h/.cpp        # MQTT broker connection, settings load/save, callback
│   │   ├── mqtt_publish.cpp           # Change-detection statics, periodic MQTT publishing (no header)
│   │   ├── mqtt_ha_discovery.cpp      # Home Assistant MQTT discovery implementation
│   │   ├── mqtt_task.h/.cpp           # Dedicated FreeRTOS task (Core 0, priority 2, 20 Hz poll)
│   │   └── eth_manager.h/.cpp         # 100Mbps Ethernet full-duplex
│   │
│   ├── Input & User Interface:
│   │   ├── button_handler.h/.cpp      # GPIO debouncing, short/long/very-long press, multi-click detection
│   │   ├── buzzer_handler.h/.cpp      # Piezo buzzer with multi-pattern sequencer, ISR-safe, FreeRTOS mutex
│   │   ├── signal_generator.h/.cpp    # Multi-waveform test signal (sine, square, noise, sweep), PWM output
│   │   └── smart_sensing.h/.cpp       # Voltage detection, auto-off timer, relay control via HAL
│   │
│   ├── Authentication & Web:
│   │   ├── auth_handler.h/.cpp        # PBKDF2-SHA256 (10k iter), HttpOnly cookies, WS tokens (60s TTL, 16-slot pool)
│   │   ├── websocket_handler.cpp      # WS server (port 81), token auth, binary frames (waveform, spectrum), JSON broadcasts
│   │   ├── web_pages.cpp              # Auto-generated from web_src/; do NOT edit manually
│   │   ├── web_pages_gz.cpp           # Auto-generated gzip-compressed HTML/CSS/JS
│   │   └── login_page.h               # Login form HTML (embedded)
│   │
│   ├── Settings & Persistence:
│   │   ├── settings_manager.h/.cpp    # /config.json (primary) + settings.txt (legacy fallback), atomic write via tmp+rename
│   │   ├── crash_log.h/.cpp           # MCU reset/crash reason logging to NVS
│   │   └── diag_journal.h/.cpp        # Circular event buffer with timestamps, error codes (portMUX guarded)
│   │
│   ├── Firmware Updates:
│   │   └── ota_updater.h/.cpp         # GitHub release checks, SHA256 verification, SSL validation
│   │
│   ├── Diagnostics & Monitoring:
│   │   ├── debug_serial.h/.cpp        # Log-level filtered output (LOG_D/I/W/E), WS forwarding, [ModuleName] prefix extraction
│   │   ├── diag_error_codes.h         # 20+ error reason codes
│   │   ├── diag_event.h               # Diagnostic event struct (timestamp, reason, level)
│   │   ├── task_monitor.h/.cpp        # FreeRTOS task enumeration (stack, priority, core affinity)
│   │   └── utils.h/.cpp               # Version comparison, string utilities
│   │
│   ├── GUI (LVGL on ST7735S):
│   │   ├── gui/gui_manager.h/.cpp     # Init, FreeRTOS task (Core 0), screen sleep/wake
│   │   ├── gui/gui_input.h/.cpp       # ISR rotary encoder (Gray code state machine)
│   │   ├── gui/gui_theme.h/.cpp       # Orange accent, dark/light mode (LVGL styles)
│   │   ├── gui/gui_navigation.h/.cpp  # Screen stack with push/pop, transition animations
│   │   ├── gui/lgfx_config.h          # LovyanGFX ST7735S config (128×160 memory, offset rotation)
│   │   └── gui/screens/               # 8 screen implementations
│   │       ├── scr_desktop.cpp        # Carousel home, status badges
│   │       ├── scr_home.cpp           # Main dashboard
│   │       ├── scr_control.cpp        # Audio/DSP controls
│   │       ├── scr_wifi.cpp           # WiFi config + scanning
│   │       ├── scr_mqtt.cpp           # MQTT broker settings
│   │       ├── scr_settings.cpp       # General settings
│   │       ├── scr_debug.cpp          # Debug info (HAL devices, task monitor, ADC levels)
│   │       ├── scr_support.cpp        # Links, documentation
│   │       ├── scr_boot.cpp           # Boot animation + password display
│   │       ├── scr_keyboard.cpp       # Text input UI
│   │       └── scr_value_editor.cpp   # Numeric input UI
│   │
│   ├── Drivers:
│   │   └── drivers/es8311_regs.h      # ES8311 register definitions + init sequences
│   │
│   └── Other Utilities:
│       ├── strings.h                  # String constants (error messages, labels)
│       └── globals.h                  # Global extern declarations
│
├── web_src/                    # Web UI source (auto-compiled, do NOT edit src/web_pages.cpp)
│   ├── index.html              # HTML shell (body content, no inline JS/CSS)
│   ├── css/                    # Split by concern (5 files)
│   │   ├── 00-tokens.css       # Auto-generated from src/design_tokens.h (theme colors, typography)
│   │   ├── 01-variables.css    # CSS custom properties (sizing, spacing)
│   │   ├── 02-layout.css       # Grid, flexbox, responsive breakpoints
│   │   ├── 03-components.css   # Button, input, toggle, card, panel styles
│   │   ├── 04-canvas.css       # Waveform/spectrum canvas sizing
│   │   └── 05-responsive.css   # Mobile media queries
│   │
│   ├── js/                     # 28 files concatenated in load order (single <script> block)
│   │   ├── 01-core.js          # WebSocket client (ws://, error handling, auto-reconnect)
│   │   ├── 02-ws-router.js     # WS message dispatch to handlers (audio, settings, etc.)
│   │   ├── 03-app-state.js     # Frontend state object (mirrors firmware AppState)
│   │   ├── 04-shared-audio.js  # numInputLanes, resizeAudioArrays(), dynamic ADC lookup
│   │   ├── 05-audio-tab.js     # HAL-driven channel strips, 8×16 matrix UI
│   │   ├── 06-canvas-helpers.js # Waveform/spectrum canvas utilities
│   │   ├── 06-peq-overlay.js   # Full-screen PEQ/crossover/compressor overlays + freq response graph
│   │   ├── 07-ui-core.js       # Tab switching, sidebar, layout
│   │   ├── 08-ui-status.js     # Status bar indicators (WiFi, MQTT, ADC, DAC)
│   │   ├── 09-audio-viz.js     # Real-time waveform + spectrum rendering (requestAnimationFrame)
│   │   ├── 13-signal-gen.js    # Signal generator waveform selector UI
│   │   ├── 15-hal-devices.js   # HAL device explorer (live discovery, config UI)
│   │   ├── 15a-yaml-parser.js  # Parse YAML device DB for friendly names
│   │   ├── 20-wifi-network.js  # WiFi scan + connect UI
│   │   ├── 21-mqtt-settings.js # MQTT broker config + HA discovery toggle
│   │   ├── 22-settings.js      # General settings (timezone, dark mode, backlight)
│   │   ├── 23-firmware-update.js # OTA version check + download progress
│   │   ├── 24-hardware-stats.js # CPU, memory, heap, uptime display
│   │   ├── 25-debug-console.js # Live serial log with category filtering, search, sticky localStorage
│   │   ├── 26-support.js       # Documentation links, GitHub, community
│   │   ├── 27-auth.js          # Login form, session management
│   │   ├── 27a-health-dashboard.js # Device grid, error counters, event log
│   │   └── 28-init.js          # Initialization: auth, WS connect, first state fetch
│   │
│   ├── .eslintrc.json          # Linter config (380 globals for concatenated scope)
│   └── index.html.mmd          # Docusaurus architecture diagram (HTML assembly flow)
│
├── test/                       # C++ unit tests (Unity framework, native platform)
│   ├── test_mocks/             # Mock implementations for Arduino, WiFi, MQTT, Preferences
│   │   ├── mock_arduino.h
│   │   ├── mock_wifi.h
│   │   ├── mock_mqtt.h
│   │   └── mock_preferences.h
│   │
│   └── test_*/                 # 70 test modules (one test per directory to avoid duplicate main)
│       ├── test_audio_pipeline/
│       ├── test_dsp/
│       ├── test_hal_core/
│       ├── test_hal_coord/     # HAL toggle queue (capacity 8, dedup, overflow)
│       ├── test_i2s_audio/     # I2S pin override resolution
│       ├── test_mqtt/
│       ├── test_wifi/
│       ├── test_auth/
│       ├── test_websocket/
│       ├── test_button/
│       ├── test_buzzer/
│       ├── test_hal_discovery/
│       ├── test_hal_pcm1808/
│       ├── test_hal_es8311/
│       ├── test_smart_sensing/
│       ├── test_ota/
│       ├── test_utils/
│       └── ... (70 total modules)
│
├── e2e/                        # Playwright browser tests (26 tests across 19 specs)
│   ├── tests/                  # Spec files
│   │   ├── auth.spec.js
│   │   ├── wifi.spec.js
│   │   ├── mqtt.spec.js
│   │   ├── audio-tab.spec.js
│   │   └── ... (19 total specs)
│   │
│   ├── fixtures/
│   │   ├── ws-messages/        # 15 hand-crafted WS broadcast fixtures (.json)
│   │   └── api-responses/      # 14 REST response fixtures (.json)
│   │
│   ├── helpers/
│   │   ├── fixtures.js         # connectedPage fixture (auth + WS setup)
│   │   ├── ws-helpers.js       # buildInitialState(), handleCommand(), binary frame builders
│   │   └── selectors.js        # Reusable DOM selectors matching web_src/ IDs
│   │
│   ├── mock-server/            # Express server (port 3000) for testing
│   │   ├── server.js           # Express app setup
│   │   ├── assembler.js        # HTML assembly from web_src/ (replicas tools/build_web_assets.js)
│   │   ├── ws-state.js         # Mock state singleton, reset between tests
│   │   └── routes/             # 12 Express route files matching firmware REST API
│   │
│   └── playwright.config.js    # Playwright configuration
│
├── lib/                        # Vendored/custom libraries
│   ├── WebSockets/             # Vendored WebSocket library (no longer from lib_deps)
│   ├── esp_dsp_lite/           # ANSI C ESP-DSP fallback for native tests
│   └── ... (other vendored libs)
│
├── tools/                      # Build & CI utilities
│   ├── build_web_assets.js     # Compile web_src/ → src/web_pages.cpp + src/web_pages_gz.cpp
│   ├── extract_tokens.js       # Extract design_tokens.h → CSS (both docs-site + web_src)
│   ├── extract_api.js          # Extract REST API + WS commands from C++ source (informational)
│   ├── find_dups.js            # Find duplicate JS declarations (pre-commit check)
│   ├── check_missing_fns.js    # Find undefined JS function references (pre-commit check)
│   ├── detect_doc_changes.js   # Git diff → doc sections for CI incremental generation
│   ├── generate_docs.js        # Claude API orchestrator for CI doc regeneration
│   ├── doc-mapping.json        # Source file → doc section mapping (65 entries)
│   ├── prompts/                # Writing style templates (api-reference, user-guide, developer-guide, hal-driver)
│   └── ... (other utilities)
│
├── docs-site/                  # Docusaurus v3 public documentation
│   ├── src/
│   │   ├── pages/
│   │   │   ├── index.js        # Hero landing page
│   │   │   └── ... (other pages)
│   │   └── css/
│   │       └── tokens.css      # Auto-generated from src/design_tokens.h
│   │
│   ├── docs/                   # 26 doc pages (9 user guide + 17 developer reference)
│   │   ├── user-guide/         # Getting started, setup, configuration, usage
│   │   └── developer/          # API reference, WebSocket, HAL, DSP, testing, architecture
│   │
│   ├── docusaurus.config.js    # Site config (Mermaid, search, dark mode default, /docs route)
│   ├── sidebars.js             # userSidebar + devSidebar
│   ├── package.json            # Docusaurus deps (v3.x)
│   └── static/                 # Static assets (favicon, icons)
│
├── docs-internal/              # Internal working documentation (separate from public site)
│   ├── planning/               # Phase plans, debt registry, test strategy
│   ├── architecture/           # 10 Mermaid diagrams (system, HAL lifecycle, boot, test infrastructure, CI)
│   └── ... (working notes)
│
├── .github/workflows/          # GitHub Actions CI/CD
│   ├── tests.yml               # 4 parallel quality gates (cpp-tests, cpp-lint, js-lint, e2e-tests)
│   ├── docs.yml                # Doc generation + Docusaurus build + deploy to gh-pages
│   └── release.yml             # Release workflow (same 4 gates)
│
├── .githooks/                  # Pre-commit checks
│   └── pre-commit              # find_dups, check_missing_fns, ESLint
│
├── platformio.ini              # PlatformIO configuration (ESP32-P4, native test env, build flags)
├── package.json                # Web UI deps (Playwright, ESLint; top-level, no monorepo)
├── CLAUDE.md                   # Project instructions (overview, build/test commands, architecture, conventions)
└── README.md                   # Project overview + quick start
```

## Directory Purposes

**src/**
- Firmware source code for ESP32-P4. Entry point is `main.cpp`. Organized into subsystems (audio, networking, HAL, DSP, UI, diagnostics). State is decomposed across `src/state/` (15 domain headers), composed into AppState singleton. HAL framework manages all hardware devices.

**src/state/**
- Domain-specific state headers (15 total). Each header contains one struct (e.g., WifiState, AudioState). Composed into AppState singleton. Decomposition reduces cross-domain coupling and improves code organization.

**src/hal/**
- Hardware Abstraction Layer: device base class, manager, discovery, driver registry, 8 builtin drivers, REST API, persistence, pipeline integration. HAL is the single source of truth for all devices.

**web_src/**
- Web UI source files. HTML, CSS (split by concern), and JS (28 files in load order, concatenated into single script block). Auto-compiled to `src/web_pages.cpp` via `tools/build_web_assets.js`. DO NOT edit `src/web_pages.cpp` manually.

**test/**
- C++ unit tests (Unity framework). 70 test modules, native platform (no hardware). One test per directory to avoid duplicate symbols. Mocks for Arduino, WiFi, MQTT, Preferences in `test/test_mocks/`.

**e2e/**
- Playwright browser tests (26 tests across 19 specs). Mock Express server with assembler and WS interception. Fixtures for WS messages and REST responses. Helpers for common selectors and operations.

**tools/**
- Build utilities: `build_web_assets.js` (compile web_src), `extract_tokens.js` (design tokens → CSS), pre-commit checks (find_dups, check_missing_fns), doc generation orchestrator.

**docs-site/**
- Public Docusaurus v3 documentation site (26 pages: 9 user guide, 17 developer reference). Design tokens synced from `src/design_tokens.h`. CI generates docs incrementally on changes.

**docs-internal/**
- Internal working documentation (separate from public site). Contains phase plans, debt registry, architecture diagrams, test strategy, topic files.

## Key File Locations

**Entry Points:**
- `src/main.cpp` — Arduino setup/loop, REST endpoint registration, HAL initialization, main event loop
- `src/main.cpp` line ~450 — `loop()` function: `app_events_wait(5)` with dirty-flag dispatch

**Core Application State:**
- `src/app_state.h` — AppState singleton (thin shell, composition of 15 domain headers)
- `src/state/*.h` — 15 domain-specific state structs (WifiState, AudioState, DspSettingsState, MqttState, etc.)
- `src/app_events.h/.cpp` — Event group + dirty-flag signaling (24-bit, main loop wake)

**HAL Device Framework:**
- `src/hal/hal_device.h` — Abstract base class for all devices (lifecycle: probe/init/deinit/healthCheck)
- `src/hal/hal_types.h` — Enums (HalDeviceType, HalDeviceState, HalDiscovery), structs (HalDeviceDescriptor, HalDeviceConfig), constants
- `src/hal/hal_device_manager.h/.cpp` — Singleton managing up to 24 devices
- `src/hal/hal_builtin_devices.h/.cpp` — Registry: compatible string → factory function (8 integrated drivers)
- `src/hal/hal_discovery.h/.cpp` — 3-tier discovery (I2C scan → EEPROM → manual)
- `src/hal/hal_pipeline_bridge.h/.cpp` — Connects device lifecycle to audio pipeline
- `src/hal/hal_api.h/.cpp` — 13 REST endpoints for device CRUD

**Audio Pipeline:**
- `src/audio_pipeline.h/.cpp` — 8-lane input → 16×16 matrix → 8-slot output (FreeRTOS Core 1 task)
- `src/i2s_audio.h/.cpp` — Dual I2S ADC (synchronized masters), DMA, FFT, health check
- `src/dsp_pipeline.h/.cpp` — Per-input DSP chains (biquad, limiter, gain, delay, polarity, mute)
- `src/output_dsp.h/.cpp` — Per-output DSP (post-matrix)
- `src/dsp_api.h/.cpp` — DSP configuration REST endpoints

**Networking & MQTT:**
- `src/wifi_manager.h/.cpp` — WiFi client + AP mode
- `src/mqtt_handler.h/.cpp` — MQTT broker connection, settings, callback
- `src/mqtt_task.h/.cpp` — Dedicated FreeRTOS task (Core 0, 20 Hz poll)
- `src/mqtt_publish.cpp` — Change-detection statics (no header), periodic MQTT publishes

**Web & REST API:**
- `src/main.cpp` — REST endpoint registration (status, settings, WiFi, MQTT, firmware)
- `src/auth_handler.h/.cpp` — PBKDF2-SHA256 (10k iter), HttpOnly cookies, WS tokens
- `src/websocket_handler.cpp` — WebSocket server (port 81), token auth, binary frames
- `web_src/index.html` — HTML shell (body, no inline CSS/JS)
- `web_src/js/*.js` — 28 files in load order (core, WS router, app state, UI modules)

**Configuration & Persistence:**
- `src/config.h` — Pin definitions, build flags, FreeRTOS stack/priority
- `src/settings_manager.h/.cpp` — `/config.json` (primary) + legacy `settings.txt` fallback
- `src/hal/hal_settings.h/.cpp` — Per-device HAL config persistence
- `src/design_tokens.h` — Theme colors, typography (auto-synced to CSS)

**Testing:**
- `test/test_*/` — 70 unit test modules (one test per directory)
- `e2e/tests/*.spec.js` — 19 Playwright specs (26 tests total)
- `platformio.ini` — Native test environment configuration

## Naming Conventions

**Files:**
- `*.h` — Header files (declarations, inline code, templates)
- `*.cpp` — Implementation files (definitions, large code)
- `*_state.h` — AppState domain headers in `src/state/`
- `hal_*.h/.cpp` — HAL subsystem files in `src/hal/`
- `test_*.cpp` — Unity test files in `test/test_*/`
- `*.spec.js` — Playwright test specs in `e2e/tests/`
- `*.js` — Web UI modules in `web_src/js/`, numbered by load order (01-28)

**Directories:**
- `src/state/` — Domain-specific AppState composition headers (15 total)
- `src/hal/` — Hardware abstraction layer (device manager, drivers, discovery)
- `src/gui/screens/` — LVGL screen implementations
- `web_src/css/` — CSS split by concern (00-05)
- `test/test_*/` — Unit test module directories
- `docs-site/docs/` — Docusaurus documentation pages
- `docs-internal/` — Internal working documentation

**Functions & Variables:**
- `camelCase` — Function names, variable names, method names
- `UPPER_SNAKE_CASE` — Macros, constants, enums
- `_leadingUnderscore` — File-local statics (e.g., `static int _pendingCount`)
- `[ModuleName]` — Log prefix in debug output (e.g., `[WiFi]`, `[MQTT]`, `[Audio]`)
- `appState.domain.field` — State access pattern (e.g., `appState.wifi.ssid`, `appState.audio.paused`)
- `halSlot` — Device index in HAL (0-23)
- `lane` — Input index in audio pipeline (0-7)
- `slot` — Output index in audio pipeline (0-7)

## Where to Add New Code

**New HAL Device:**
- Implementation: `src/hal/hal_<device_name>.h/.cpp`
- Registration: Update `src/hal/hal_builtin_devices.h` compatible string mapping
- Database entry: Update `src/hal/hal_device_db.h/.cpp` builtin list
- Tests: `test/test_hal_<device_name>/test_hal_<device_name>.cpp`
- Example: `src/hal/hal_ns4150b.h/.cpp` (amp enable GPIO control)

**New DSP Stage:**
- Module: `src/dsp_<stage_name>.h/.cpp`
- Integration: Add stage type to `DspStageType` enum in DSP pipeline
- Config struct: Add to stage config union
- REST endpoint: Update `src/dsp_api.h/.cpp`
- Tests: `test/test_dsp_<stage_name>/`

**New REST API Endpoint:**
- Registration: `main.cpp` line ~950 `server.on(path, handler)`
- Implementation: Inline in `main.cpp` or separate module (e.g., `src/hal_api.cpp`)
- WebSocket response: Add corresponding WS message handler in `web_src/js/02-ws-router.js`
- Tests: `e2e/tests/*.spec.js` (Playwright) or `test/test_api/` (C++ unit)

**New Web UI Tab:**
- HTML structure: `web_src/index.html` (button, tab ID, content panel)
- Styles: `web_src/css/03-components.css` + responsive in `05-responsive.css`
- JS module: `web_src/js/NN-<tab_name>.js` (follow numbering 01-28)
- State handler: Add to `web_src/js/02-ws-router.js` (WS message dispatcher)
- Selectors: Add to `e2e/helpers/selectors.js` if testing

**New Utility Function:**
- General helpers: `src/utils.h/.cpp`
- HAL helpers: `src/hal/hal_types.h` (static inline)
- Audio helpers: `src/sink_write_utils.h/.cpp` (float buffer conversion)
- Web helpers: `web_src/js/07-ui-core.js` or domain-specific module

**New Configuration Field:**
- AppState domain: Add to corresponding `src/state/<domain>.h` struct
- Persistence: Add to `/config.json` structure in `src/settings_manager.cpp`
- REST endpoint: Add to GET/PUT settings in `main.cpp`
- Web UI: Add input to `web_src/js/22-settings.js` + corresponding WS handler

**New Test:**
- Unit test: Create `test/test_<module>/` directory, add `test_<module>.cpp` with `setup()`, `test_*()` functions
- Browser test: Create `e2e/tests/<feature>.spec.js` using `connectedPage` fixture
- Mock data: Add to `e2e/fixtures/` if needed (ws-messages or api-responses)

## Special Directories

**src/gui/**
- Purpose: LVGL v9.4 GUI framework (runs on Core 0 FreeRTOS task)
- Generated: No (hand-written screen code)
- Committed: Yes

**src/hal/**
- Purpose: Hardware abstraction layer (sole device lifecycle manager)
- Generated: No (hand-written drivers)
- Committed: Yes

**web_src/**
- Purpose: Web UI source files (HTML, CSS, JS in load order)
- Generated: No (hand-written code)
- Committed: Yes
- Note: `src/web_pages.cpp` and `src/web_pages_gz.cpp` are GENERATED by `tools/build_web_assets.js` — do NOT commit edits to these files. Always edit `web_src/` source files instead.

**.pio/build/**
- Purpose: PlatformIO build artifacts (object files, linked firmware)
- Generated: Yes (from `pio run`)
- Committed: No

**test/test_mocks/**
- Purpose: Mock implementations of Arduino, WiFi, MQTT libraries for native tests
- Generated: No
- Committed: Yes

**e2e/mock-server/**
- Purpose: Express server + WS state for Playwright test environment
- Generated: No (except assembler.js mirrors build_web_assets.js)
- Committed: Yes

---

*Structure analysis: 2026-03-21*
