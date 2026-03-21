# Codebase Structure

**Analysis Date:** 2026-03-21

## Directory Layout

```
ALX_Nova_Controller_2/
├── src/                    # Firmware (C++ Arduino + FreeRTOS)
│   ├── main.cpp            # Boot sequence + main loop on Core 1
│   ├── config.h            # Build-time constants (pins, stack sizes, feature flags)
│   ├── app_state.h         # AppState singleton (15 domain state headers)
│   ├── app_events.h        # FreeRTOS event group (17 bits for state changes)
│   ├── state/              # Domain-specific state headers (15 files)
│   ├── audio_pipeline.h/.cpp # Real-time audio routing (8-lane → 16×16 matrix → 8-sink)
│   ├── i2s_audio.h/.cpp    # ADC/DAC I2S driver (dual PCM1808, ES8311, TDM parsing)
│   ├── dsp_pipeline.h/.cpp # DSP filter chains (biquad, FIR, limiter, compressor)
│   ├── output_dsp.h/.cpp   # Per-output DSP (post-matrix processing)
│   ├── smart_sensing.h/.cpp # Signal detection + relay control
│   ├── websocket_handler.cpp # Port 81 real-time broadcasts (2565 lines)
│   ├── wifi_manager.h/.cpp # WiFi client + AP mode (1556 lines)
│   ├── eth_manager.h/.cpp  # Ethernet (100Mbps on ESP32-P4)
│   ├── mqtt_handler.cpp    # MQTT client lifecycle (1128 lines)
│   ├── mqtt_publish.cpp    # 20 Hz publish loop (change-detection statics)
│   ├── mqtt_ha_discovery.cpp # Home Assistant discovery (1897 lines)
│   ├── mqtt_task.cpp       # FreeRTOS MQTT task (Core 0, priority 2)
│   ├── auth_handler.h/.cpp # Web password + WS token auth (PBKDF2-SHA256)
│   ├── settings_manager.h/.cpp # JSON persistence (/config.json)
│   ├── ota_updater.h/.cpp  # GitHub release checking + SHA256 verification
│   ├── button_handler.h/.cpp # Debounce + press detection
│   ├── buzzer_handler.h/.cpp # Piezo patterns + volume control
│   ├── signal_generator.h/.cpp # Sine/square/noise/sweep test signals
│   ├── usb_audio.h/.cpp    # TinyUSB UAC2 speaker device (guarded by USB_AUDIO_ENABLED)
│   ├── hal/                # Hardware Abstraction Layer (24 files)
│   │   ├── hal_device_manager.h/.cpp # Device registry (24 slots), lifecycle states
│   │   ├── hal_pipeline_bridge.h/.cpp # Links HAL devices to audio pipeline (slot/lane assignment)
│   │   ├── hal_driver_registry.h/.cpp # Compatible string → factory function mapping
│   │   ├── hal_device_db.h/.cpp # In-memory device database + LittleFS persistence
│   │   ├── hal_discovery.h/.cpp # 3-tier: I2C scan → EEPROM → manual config
│   │   ├── hal_settings.h/.cpp # Per-device config persistence (/hal_config.json)
│   │   ├── hal_api.h/.cpp  # REST endpoints for HAL CRUD
│   │   ├── hal_builtin_devices.h/.cpp # PCM5102A, ES8311, ES9822PRO, ES9843PRO, etc.
│   │   ├── hal_es9822pro.h/.cpp # ESS 2-ch SABRE ADC (expansion mezzanine)
│   │   ├── hal_es9843pro.h/.cpp # ESS 4-ch SABRE ADC (TDM deinterleaver)
│   │   ├── hal_tdm_deinterleaver.h/.cpp # Splits 4-slot TDM → 2 stereo pairs
│   │   ├── hal_relay.h/.cpp # GPIO amplifier relay control
│   │   ├── hal_ns4150b.h/.cpp # NS4150B class-D amp driver
│   │   ├── hal_buzzer.h/.cpp # Piezo via HAL (dynamic GPIO)
│   │   ├── hal_button.h/.cpp # Push button via HAL (dynamic GPIO)
│   │   ├── hal_encoder.h/.cpp # Rotary encoder via HAL (dynamic GPIO)
│   │   ├── hal_display.h/.cpp # TFT display via HAL (dynamic SPI pins)
│   │   ├── hal_led.h/.cpp  # Status LED via HAL (dynamic GPIO)
│   │   ├── hal_signal_gen.h/.cpp # Software signal generator source
│   │   ├── hal_temp_sensor.h/.cpp # ESP32-P4 internal temp sensor
│   │   ├── hal_custom_device.h/.cpp # User-defined device templates (YAML)
│   │   ├── hal_audio_device.h # Audio sink interface (abstract)
│   │   ├── hal_types.h     # Device states, capability flags, error codes
│   │   ├── hal_init_result.h # Init status structs
│   │   ├── hal_audio_health_bridge.h/.cpp # Audio health → HAL diagnostics
│   │   ├── hal_i2s_bridge.h/.cpp # I2S TX bridge (HAL DAC ↔ pipeline)
│   │   ├── hal_dsp_bridge.h/.cpp # DSP routing (input → DSP lane mapping)
│   │   └── hal_eeprom_v3.h/.cpp # EEPROM string matching for auto-discovery
│   ├── gui/                # LVGL GUI (Core 0, guarded by GUI_ENABLED)
│   │   ├── gui_manager.h/.cpp # Init + FreeRTOS task + screen sleep/wake
│   │   ├── gui_theme.h     # Orange accent + dark/light mode
│   │   ├── gui_input.h/.cpp # ISR rotary encoder (Gray code)
│   │   ├── gui_navigation.h/.cpp # Screen stack + transitions
│   │   └── screens/        # 10+ screen modules (home, control, settings, debug, etc.)
│   ├── dsp_pipeline.h      # DSP stage pool + config structures
│   ├── dsp_biquad_gen.h    # RBJ Audio EQ Cookbook coefficient computation
│   ├── dsp_coefficients.h  # DSP coefficient helpers
│   ├── dsp_crossover.h     # Crossover presets (LR2/LR4/LR8)
│   ├── dsp_convolution.h/.cpp # FIR convolution + IR processing
│   ├── dsp_rew_parser.h/.cpp # Equalizer APO + miniDSP import/export
│   ├── dsp_api.h/.cpp      # REST DSP endpoints (1441 lines)
│   ├── pipeline_api.h/.cpp # REST audio matrix + per-output DSP CRUD
│   ├── dac_api.h/.cpp      # REST DAC state/volume/enable endpoints
│   ├── dac_hal.h/.cpp      # DAC bus utilities (I2S TX, volume curves, logging)
│   ├── dac_eeprom.h/.cpp   # Legacy EEPROM format v2 (not used, docs only)
│   ├── heap_budget.h/.cpp  # Per-subsystem allocation tracker (32 entries)
│   ├── task_monitor.h/.cpp # FreeRTOS task enumeration + stack usage
│   ├── thd_measurement.h   # THD+N measurement engine
│   ├── diag_journal.h/.cpp # Event ringbuffer (64 entries) + serial logging
│   ├── diag_event.h        # Diagnostic event emission macro
│   ├── diag_error_codes.h  # Error code enum (100+ codes)
│   ├── debug_serial.h      # Logging macros (LOG_D/I/W/E) + serial level control
│   ├── audio_input_source.h # Input lane pluggable source interface
│   ├── audio_output_sink.h # Output sink pluggable interface
│   ├── sink_write_utils.h/.cpp # Shared float→int32 conversion + volume/mute
│   ├── web_pages.cpp       # Auto-generated HTML/CSS/JS (~12.7k lines, do NOT edit)
│   ├── web_pages_gz.cpp    # Gzip-compressed variant (~6.5k lines, do NOT edit)
│   ├── login_page.h        # First-boot password generation UI
│   ├── crash_log.h/.cpp    # Reset reason ringbuffer
│   ├── design_tokens.h     # Theme colors/sizes (design system source)
│   ├── drivers/            # Device register definitions
│   │   ├── pcm1808_regs.h  # PCM1808 ADC register map
│   │   ├── es8311_regs.h   # ES8311 codec register map
│   │   ├── es9822pro_regs.h # ES9822PRO SABRE ADC register map
│   │   └── es9843pro_regs.h # ES9843PRO SABRE ADC register map
│   ├── strings.h           # String constants + error messages
│   ├── globals.h           # External variables (WebServer, PubSubClient, HAL ptrs)
│   └── utils.h/.cpp        # Helper functions (version compare, time utils)
│
├── web_src/                # Web UI source (edit only these, NOT src/web_pages.cpp)
│   ├── index.html          # HTML shell (body content, no inline CSS/JS)
│   ├── css/                # CSS modules (split by concern)
│   │   ├── 00-tokens.css   # Auto-generated design token vars
│   │   ├── 01-variables.css # CSS custom properties (colors, sizes)
│   │   ├── 02-layout.css   # Grid/flex, page structure
│   │   ├── 03-components.css # Button, input, card styles
│   │   ├── 04-canvas.css   # Waveform + spectrum canvas rendering
│   │   ├── 05-responsive.css # Mobile/tablet breakpoints
│   │   └── 06-health-dashboard.css # Device grid + event log
│   └── js/                 # JS modules (concatenated in filename order)
│       ├── 01-core.js      # WebSocket client + message dispatch
│       ├── 02-ws-router.js # Handler routing (WS message → DOM update)
│       ├── 03-app-state.js # Client-side state mirror
│       ├── 04-shared-audio.js # Audio constants (16×16 matrix, HAL device discovery)
│       ├── 05-audio-tab.js # HAL-driven input/output channel strips + matrix UI
│       ├── 06-canvas-helpers.js # Waveform/spectrum drawing
│       ├── 06-peq-overlay.js # Full-screen PEQ/crossover/compressor overlays + freq response
│       ├── 07-ui-core.js   # Tab switching, sidebar, layout control
│       ├── 08-ui-status.js # System status card + indicators
│       ├── 09-audio-viz.js # Real-time waveform + spectrum visualization
│       ├── 13-signal-gen.js # Signal generator UI (waveform selection)
│       ├── 15-hal-devices.js # HAL device list + status grid
│       ├── 15a-yaml-parser.js # YAML config editor for custom devices
│       ├── 20-wifi-network.js # WiFi scan + connection UI
│       ├── 21-mqtt-settings.js # MQTT broker config + test
│       ├── 22-settings.js  # General settings (time, theme, safety)
│       ├── 23-firmware-update.js # OTA check + changelog modal
│       ├── 24-hardware-stats.js # CPU usage, heap pressure, task monitor
│       ├── 25-debug-console.js # Filtered serial log display (module category chips)
│       ├── 27a-health-dashboard.js # Device grid, error counters, event log
│       └── 28-auth.js      # Login/logout, session management
│
├── test/                   # C++ unit tests (Unity framework, native platform)
│   ├── test_mocks/         # Mock implementations of Arduino/WiFi/MQTT/NVS
│   ├── test_utils/         # Utility functions test
│   ├── test_auth/          # Auth handler test (75 tests)
│   ├── test_wifi/          # WiFi manager test
│   ├── test_mqtt/          # MQTT handler test
│   ├── test_audio_pipeline/ # Audio matrix routing (24 new bounds tests in v1.12.3)
│   ├── test_dsp_pipeline/  # DSP stage pool + swap
│   ├── test_hal_*/         # 25+ HAL-specific test directories (device lifecycle, discovery, bridge, etc.)
│   └── ... (75+ test modules, ~1866 total tests)
│
├── e2e/                    # Playwright browser tests
│   ├── tests/              # 20 spec files (44 tests total, mock server + WS interception)
│   ├── fixtures/           # Hand-crafted test data (WS messages, API responses)
│   ├── helpers/            # Playwright utilities (fixtures, selectors, WS helpers)
│   ├── mock-server/        # Express server (port 3000) replicating firmware REST API
│   └── package.json        # Playwright + ESLint deps
│
├── docs-site/              # Docusaurus documentation site (public)
│   ├── docs/               # 26 pages (9 user guide + 17 developer reference)
│   ├── src/                # Docusaurus components + CSS
│   └── docusaurus.config.js # Site config (Mermaid, design token vars, dark mode)
│
├── docs-internal/          # Internal working docs (not in public site)
│   ├── planning/           # Phase plans, debt registry, test strategy
│   └── architecture/       # Mermaid diagrams (10 files)
│
├── tools/                  # Build + CI automation
│   ├── build_web_assets.js # Compile web_src/ → src/web_pages.cpp
│   ├── extract_tokens.js   # Design tokens → CSS for web + docs
│   ├── extract_api.js      # API extraction from C++ (informational)
│   ├── generate_docs.js    # Claude API orchestrator for doc generation
│   ├── find_dups.js        # Check for duplicate JS declarations (pre-commit)
│   ├── check_missing_fns.js # Check undefined function references
│   └── doc-mapping.json    # Source file → documentation section mapping
│
├── platformio.ini          # PlatformIO config (board=esp32-p4, build flags)
├── CLAUDE.md               # Project instructions for Claude (THIS FILE)
├── .planning/codebase/     # GSD codebase analysis documents (you are here)
│   ├── ARCHITECTURE.md     # Pattern, layers, data flow, abstractions, entry points
│   ├── STRUCTURE.md        # Directory layout, key locations, file naming (THIS FILE)
│   ├── CONVENTIONS.md      # Coding conventions (optional, not written yet)
│   ├── TESTING.md          # Testing patterns (optional, not written yet)
│   ├── STACK.md            # Technology stack (optional, not written yet)
│   ├── INTEGRATIONS.md     # External integrations (optional, not written yet)
│   └── CONCERNS.md         # Technical debt and issues (optional, not written yet)
│
└── .github/workflows/      # CI/CD automation
    ├── tests.yml           # 4 parallel gates (cpp-tests, cpp-lint, js-lint, e2e-tests)
    ├── docs.yml            # Doc generation + Docusaurus deploy to gh-pages
    └── release.yml         # Same 4 gates before release
```

## Directory Purposes

**src/**
- Purpose: All firmware source code (C++ Arduino + FreeRTOS on ESP32-P4)
- Contains: Main loop, HAL framework, audio pipeline, network managers, web server, GUI, testing mocks
- Key files: `main.cpp` (boot + loop), `app_state.h` (state singleton), `audio_pipeline.h` (routing engine)

**src/hal/**
- Purpose: Hardware Abstraction Layer — unified device model, discovery, lifecycle, persistence
- Contains: Device manager, bridge, driver registry, device database, 12+ hardware drivers, settings persistence
- Key files: `hal_device_manager.h/.cpp` (registry), `hal_pipeline_bridge.h/.cpp` (slot/lane assignment), `hal_discovery.h/.cpp` (3-tier auto-detection)

**src/state/**
- Purpose: Domain-specific application state (decomposed from DEBT-4)
- Contains: 15 lightweight headers: enums, general, audio, wifi, mqtt, debug, dac, dsp, etc.
- Pattern: Each header defines one domain struct, composed into AppState shell. Access via `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`, etc.

**src/gui/**
- Purpose: LVGL-based TFT display interface (Core 0 FreeRTOS task)
- Contains: Display manager, theme, input handling, navigation stack, 10+ screen modules
- Key files: `gui_manager.h/.cpp` (task lifecycle), `gui_input.h/.cpp` (encoder Gray code), `screens/` (individual screens)

**src/drivers/**
- Purpose: Hardware register definitions (documentation + constants)
- Contains: PCM1808, ES8311, ES9822PRO, ES9843PRO register maps
- Pattern: Read-only header files (no implementation), referenced by HAL drivers

**web_src/**
- Purpose: Web UI source files (edit ONLY these, auto-compile to src/web_pages.cpp)
- Contains: HTML shell, 6 CSS modules (tokens, layout, components, canvas, responsive, health), 20 JS modules
- Key rule: NEVER edit `src/web_pages.cpp` directly. Always edit `web_src/` then run `node tools/build_web_assets.js` before building

**test/**
- Purpose: C++ unit tests (Unity framework, native platform)
- Contains: 75+ test modules (1866 total tests), mock implementations of Arduino/WiFi/MQTT/NVS libraries
- Pattern: Each test in own directory (`test_utils/`, `test_auth/`, etc.) to avoid duplicate `main`/`setUp`/`tearDown` symbols
- Run: `pio test -e native -v`

**e2e/**
- Purpose: Browser end-to-end tests (Playwright, mock Express server)
- Contains: 20 test specs (44 tests), mock server routes, test fixtures, helper utilities
- Pattern: Mock server on port 3000 replicates firmware REST API + WS interception, no real hardware needed
- Run: `cd e2e && npx playwright test`

**docs-site/**
- Purpose: Public documentation site (Docusaurus v3, deployed to GitHub Pages)
- Contains: 26 pages (user guide + developer reference), design token CSS, Mermaid diagrams
- Pattern: MDX compatibility (escape `\{variable\}` in tables), dark mode default, local search
- Build: `cd docs-site && npm run build && npm run serve`

**docs-internal/**
- Purpose: Internal working documents (not in public site)
- Contains: Phase plans, debt registry, test strategy, architecture diagrams (10 Mermaid files)
- Pattern: Separate from Docusaurus, may be replaced by it over time

**tools/**
- Purpose: Build automation + code generation
- Contains: Web asset compiler, design token extractor, API extractor, doc generator, linters
- Key: `build_web_assets.js` MUST be run after ANY edit to `web_src/` files before building firmware

## Key File Locations

**Entry Points:**
- `src/main.cpp`: Firmware boot (`setup()`) and main loop (`loop()`)
- `platformio.ini`: Build config (board, flags, upload port)
- `tools/build_web_assets.js`: Web UI compilation (HTML/CSS/JS → gzip)

**Configuration:**
- `src/config.h`: Build-time constants (pins, stack sizes, feature flags like DSP_ENABLED, DAC_ENABLED, GUI_ENABLED, USB_AUDIO_ENABLED)
- `platformio.ini`: PlatformIO board config (esp32-p4), upload port (COM8), build flags
- `.github/workflows/tests.yml`: CI quality gates (4 parallel: cpp-tests, cpp-lint, js-lint, e2e-tests)

**Core Logic:**
- `src/audio_pipeline.h/.cpp`: Audio routing engine (8-lane input → 16×16 matrix → 8-sink output)
- `src/dsp_pipeline.h/.cpp`: DSP stage pool + filter chains (biquad IIR/FIR, limiter, etc.)
- `src/i2s_audio.h/.cpp`: ADC/DAC I2S peripheral control (dual PCM1808 master coordination)
- `src/websocket_handler.cpp`: Real-time WS broadcasts + binary frame handling
- `src/main.cpp` (lines 500+): Main loop (state serialization, event dispatch, periodic diagnostics)

**State Management:**
- `src/app_state.h`: AppState singleton (15 domain state headers, dirty flags)
- `src/state/`: Domain headers (general, audio, wifi, mqtt, debug, dac, dsp, etc.)

**HAL Framework:**
- `src/hal/hal_device_manager.h/.cpp`: Device registry (24 slots), lifecycle states
- `src/hal/hal_pipeline_bridge.h/.cpp`: Links HAL to audio pipeline (capability-based slot/lane assignment)
- `src/hal/hal_builtin_devices.h/.cpp`: Registered drivers (PCM5102A, ES8311, ES9822PRO, ES9843PRO, etc.)
- `src/hal/hal_discovery.h/.cpp`: I2C scan + EEPROM probing + manual config

**Testing:**
- `test/test_mocks/`: Arduino/WiFi/MQTT/NVS mock headers
- `test/test_audio_pipeline/`: Matrix routing tests (24 new bounds tests v1.12.3)
- `test/test_hal_*/`: 25+ HAL device test modules
- `e2e/tests/`: Playwright specs
- `e2e/mock-server/`: Express server replicating firmware REST API

**Web UI:**
- `web_src/index.html`: HTML shell
- `web_src/css/`: CSS modules (00-tokens through 06-health-dashboard)
- `web_src/js/`: JS modules (01-core through 28-auth, concatenated in order)
- `src/web_pages.cpp`: AUTO-GENERATED (do not edit directly)

**Persistence:**
- `/config.json` (LittleFS): Settings (WiFi SSID, MQTT broker, sample rate, etc.)
- `/hal_config.json` (LittleFS): Per-device HAL config (pin overrides, enable state, gain)
- `/hal_auto_devices.json` (LittleFS): Auto-discovered add-on devices (ES9822PRO, ES9843PRO EEPROM metadata)
- `/crash_log.json` (LittleFS): Reset reason ringbuffer
- `/diag_journal.json` (LittleFS): Diagnostic event ringbuffer (64 entries)

## Naming Conventions

**Files:**
- C++ header: `module_name.h` (lowercase, underscores)
- C++ implementation: `module_name.cpp` (lowercase, underscores)
- HAL drivers: `hal_device_type.h/.cpp` (e.g., `hal_es9822pro.h/.cpp`)
- HAL state: `hal_*_state.h` (e.g., `hal_coord_state.h`)
- Test: `test_<module>/test_<module>.cpp` (one test per directory to avoid symbol conflicts)
- Web: `NN-feature.js` / `NN-feature.css` (numeric prefix for load order)
- State: `src/state/<domain>_state.h` (e.g., `audio_state.h`, `wifi_state.h`)

**Directories:**
- Feature domain: `src/<feature>/` (e.g., `src/hal/`, `src/gui/`)
- Test suite: `test/test_<module>/` (one per test module)
- Build artifact: `build/` (ignored by git)
- Documentation: `docs-site/` (public), `docs-internal/` (private)

**Functions:**
- Module functions: `snake_case` with module prefix, e.g., `audio_pipeline_set_matrix_gain()`, `hal_device_manager_register()`
- Handler callbacks: Module prefix + `_handler` or `_callback`, e.g., `webSocketEvent()`, `mqttCallback()`
- ISR handlers: Module prefix + `_isr`, e.g., `button_isr()`

**Variables:**
- Global state: `appState` (AppState singleton macro), domain access `appState.wifi.ssid`
- File-local statics: `_leadingUnderscore` (private to translation unit)
- Class members: `_leadingUnderscore` (private), `_name` (protected)
- Module globals: `gModulePrefix_variableName` (discouraged; prefer AppState)

**Types:**
- Struct: `PascalCase`, e.g., `AudioInputSource`, `HalDeviceConfig`
- Enum: `PascalCase`, e.g., `AppFSMState`, `HalDeviceState`
- Class: `PascalCase` with `Hal` prefix for HAL classes, e.g., `HalPcm5102a`, `HalDevice`

## Where to Add New Code

**New Firmware Feature (Audio DSP, Signal Processing):**
- Primary code: `src/feature_name.h/.cpp` (e.g., `src/thd_measurement.h/.cpp`)
- State: Add struct to new domain header in `src/state/feature_state.h`, include in `src/app_state.h`, add dirty flag setter/getter
- Tests: Create `test/test_feature_name/` directory with `test_feature_name.cpp` (Unity framework, native platform)
- REST API: Add endpoints in new `src/feature_api.h/.cpp`, register via `registerFeatureApiEndpoints()` call in `main.cpp`

**New Network Integration (WiFi, MQTT, External API):**
- Implementation: `src/integration_name.h/.cpp` (e.g., `src/homekit_manager.h/.cpp`)
- Manager task (if concurrent): Spawn FreeRTOS task on Core 0 with appropriate priority, coordinate with main loop via dirty flags
- WS broadcast: Add domain state header, include in AppState, implement change detection in new module (or in websocket_handler.cpp)
- Tests: `test/test_integration_name/` with mocked network calls

**New HAL Device Driver (ADC, DAC, Codec, Sensor):**
- Driver class: `src/hal/hal_device_type.h/.cpp` (subclass HalDevice, implement init/deinit, lifecycle states)
- Register: Call `HalDeviceManager::instance().registerDevice(device, HAL_DISC_BUILTIN)` in `main.cpp` or `hal_builtin_devices.cpp`
- Sink/Source: If audio device, implement `AudioOutputSink` or `AudioInputSource`, register via `hal_pipeline_bridge` callback
- Tests: `test/test_hal_device_type/` with device state transitions, init success/failure paths
- Documentation: Add driver guide to `docs-site/docs/developer/hal/`

**New Web UI Tab or Feature:**
- HTML: Add `<div id="tabName">` container to `web_src/index.html` body
- CSS: Create `web_src/css/NN-feature-name.css` with component styles
- JS: Create `web_src/js/NN-feature-name.js` module with event handlers + DOM builders
- WS messages: Add handler in `web_src/js/02-ws-router.js` for new message type
- Test fixture: Add sample WS message JSON to `e2e/fixtures/ws-messages/`
- Test spec: Add `.spec.js` to `e2e/tests/` with Playwright navigation + element verification
- **Critical**: Run `node tools/build_web_assets.js` after web_src edits, THEN build firmware

**New Utility Function:**
- Shared helpers: `src/utils.h/.cpp` (version compare, time utils, string conversion)
- Module-specific: `src/feature_utils.h` (inline or small helpers used by one feature)
- Avoid: Creating new utility files unless >200 lines of code

**New GUI Screen (TFT Display):**
- Screen module: `src/gui/screens/screen_name.h/.cpp` (LVGL objects + callbacks)
- Register: Call `gui_add_screen()` in `gui_manager.cpp` (or define in screen_name.cpp and include)
- Navigation: Link from screen stack via `gui_push_screen()`/`gui_pop_screen()`
- Tests: Create `test/test_gui_screen_name/` with LVGL object tree verification

**New Compile-Time Configuration:**
- Add to: `src/config.h` with descriptive comments (or `platformio.ini` build flags section)
- Guard features: Use `#ifdef FEATURE_ENABLED` / `#endif` to gate entire modules
- Examples: `DAC_ENABLED`, `GUI_ENABLED`, `USB_AUDIO_ENABLED`, `DSP_ENABLED`

## Special Directories

**build/**
- Purpose: PlatformIO build artifacts (object files, firmware binary, test executables)
- Generated: Yes (by `pio run`, `pio test`)
- Committed: No (git-ignored)

**.planning/codebase/**
- Purpose: GSD (Claude Code) codebase analysis documents
- Generated: Yes (by `/gsd:map-codebase` command)
- Committed: Yes (checked in for context preservation)

**logs/**
- Purpose: Build output, test reports, serial captures (keep project root clean)
- Generated: Yes (save all `.log` files here)
- Committed: No (git-ignored)

**.github/workflows/**
- Purpose: CI/CD automation (GitHub Actions)
- Generated: No (checked in)
- Triggered: On push/PR to main/develop, or manual via workflow_dispatch

---

*Structure analysis: 2026-03-21*
