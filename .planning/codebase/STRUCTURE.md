# Codebase Structure

**Analysis Date:** 2026-03-22

## Directory Layout

```
ALX_Nova_Controller_2/
├── src/                    # Firmware source (C++ Arduino + FreeRTOS, ESP32-P4)
│   ├── main.cpp            # Boot sequence + main loop (Core 1, 1579 lines)
│   ├── config.h            # Build-time constants (pins, stack sizes, feature flags)
│   ├── app_state.h/.cpp    # AppState singleton (15 domain state headers)
│   ├── app_events.h/.cpp   # FreeRTOS event group (17 bits for state changes)
│   ├── http_security.h     # HTTP security headers (X-Frame-Options, X-Content-Type-Options)
│   ├── state/              # Domain-specific state headers (16 files)
│   ├── hal/                # Hardware Abstraction Layer (83 files)
│   ├── gui/                # LVGL GUI (22 files, guarded by GUI_ENABLED)
│   │   └── screens/        # 12 screen modules (home, control, settings, debug, etc.)
│   ├── drivers/            # Hardware register definitions (11 files)
│   ├── audio_pipeline.h/.cpp  # 8-lane → 16×16 matrix → 8-sink routing (1139 lines)
│   ├── dsp_pipeline.h/.cpp    # DSP filter chains (2408 lines)
│   ├── output_dsp.h/.cpp      # Per-output DSP (878 lines)
│   ├── i2s_audio.h/.cpp       # ADC/DAC I2S driver (1637 lines)
│   ├── websocket_handler.h/.cpp # Port 81 real-time broadcasts (2623 lines)
│   ├── wifi_manager.h/.cpp    # WiFi client + AP mode (1556 lines)
│   ├── mqtt_handler.h/.cpp    # MQTT client lifecycle (1129 lines)
│   ├── mqtt_publish.cpp       # 20 Hz publish loop (951 lines)
│   ├── mqtt_ha_discovery.cpp  # Home Assistant discovery (1914 lines)
│   ├── mqtt_task.h/.cpp       # FreeRTOS MQTT task (Core 0)
│   ├── settings_manager.h/.cpp # JSON persistence (1869 lines)
│   ├── auth_handler.h/.cpp    # PBKDF2-SHA256 auth + WS tokens (792 lines)
│   ├── dsp_api.h/.cpp         # REST DSP endpoints (1437 lines)
│   ├── pipeline_api.h/.cpp    # REST audio matrix endpoints (323 lines)
│   ├── dac_api.h/.cpp         # REST DAC endpoints (530 lines)
│   ├── psram_api.h/.cpp       # REST PSRAM status endpoint (60 lines)
│   ├── psram_alloc.h/.cpp     # Unified PSRAM allocation wrapper
│   ├── heap_budget.h/.cpp     # Per-subsystem allocation tracker (32 entries)
│   ├── sink_write_utils.h/.cpp # Float → int32 conversion, volume/mute
│   ├── web_pages.h/.cpp       # AUTO-GENERATED (do NOT edit)
│   ├── web_pages_gz.cpp       # AUTO-GENERATED gzip variant (do NOT edit)
│   └── [20+ more modules]    # smart_sensing, ota, buzzer, button, signal_gen, etc.
│
├── web_src/                # Web UI source (edit ONLY these files)
│   ├── index.html          # HTML shell (body content)
│   ├── .eslintrc.json      # ESLint config (380+ globals for concatenated scope)
│   ├── css/                # 7 CSS modules (tokens, variables, layout, components, canvas, responsive, health)
│   └── js/                 # 22 JS modules (concatenated in filename order)
│
├── test/                   # C++ unit tests (Unity framework, native platform)
│   ├── test_mocks/         # Mock implementations of Arduino/WiFi/MQTT/NVS
│   └── [94 test modules]   # One directory per module (~2316 tests)
│
├── e2e/                    # Playwright browser tests
│   ├── tests/              # 22 spec files (107 tests)
│   ├── fixtures/           # WS messages (15 JSON) + API responses (16 JSON)
│   ├── helpers/            # Playwright fixtures, selectors, WS helpers
│   ├── mock-server/        # Express server (port 3000) + 12 route files
│   └── playwright.config.js
│
├── lib/                    # Vendored libraries
│   ├── WebSockets/         # Vendored WebSocket library (not from registry)
│   └── esp_dsp_lite/       # ANSI C fallback for native tests (lib_ignored on ESP32)
│
├── docs-site/              # Public documentation (Docusaurus v3, 26 pages)
│   ├── docs/               # 9 user guide + 17 developer reference pages
│   ├── src/                # Docusaurus components + CSS (design tokens)
│   └── docusaurus.config.js
│
├── docs-internal/          # Internal working docs
│   ├── architecture/       # 13 Mermaid diagrams + analysis docs
│   ├── planning/           # Phase plans, debt registry, test strategy
│   └── archive/            # Completed implementation summaries
│
├── tools/                  # Build + CI automation (18 files)
│   ├── build_web_assets.js # Compile web_src/ → src/web_pages.cpp + web_pages_gz.cpp
│   ├── extract_tokens.js   # design_tokens.h → CSS for web UI + Docusaurus
│   ├── find_dups.js        # Duplicate JS declaration checker (pre-commit)
│   ├── check_missing_fns.js # Undefined function reference checker
│   └── generate_docs.js    # Claude API doc generation orchestrator
│
├── .github/workflows/      # CI/CD automation
│   ├── tests.yml           # 4 parallel quality gates
│   ├── release.yml         # Same 4 gates before release
│   └── docs.yml            # Doc generation + deploy to gh-pages
│
├── .planning/codebase/     # GSD codebase analysis documents
│   ├── ARCHITECTURE.md     # Pattern, layers, data flow, abstractions
│   ├── STRUCTURE.md        # Directory layout, key locations (THIS FILE)
│   ├── CONVENTIONS.md      # Coding conventions
│   ├── TESTING.md          # Testing patterns
│   ├── STACK.md            # Technology stack
│   ├── INTEGRATIONS.md     # External integrations
│   └── CONCERNS.md         # Technical debt and issues
│
├── platformio.ini          # PlatformIO config (board=esp32-p4)
├── CLAUDE.md               # Project instructions for Claude Code
└── .githooks/pre-commit    # Pre-commit hook (find_dups + check_missing_fns + ESLint)
```

## Directory Purposes

**src/**
- Purpose: All firmware source code (C++ Arduino + FreeRTOS on ESP32-P4)
- Contains: Main loop, HAL framework, audio pipeline, DSP engine, network managers, web server, GUI, authentication, diagnostics
- Key files: `main.cpp` (boot + loop, 1579 lines), `app_state.h` (state singleton), `config.h` (all build-time constants), `audio_pipeline.h/.cpp` (routing engine)

**src/hal/**
- Purpose: Hardware Abstraction Layer — unified device model, discovery, lifecycle, persistence
- Contains: Device manager (24 slots), pipeline bridge, driver registry (32 max), device database (32 max), 15+ concrete hardware drivers, ESS SABRE ADC base class, TDM deinterleaver, custom device support
- Key files: `hal_device_manager.h/.cpp` (registry, 387 lines), `hal_pipeline_bridge.h/.cpp` (slot/lane assignment, 655 lines), `hal_discovery.h/.cpp` (3-tier auto-detection, 238 lines), `hal_api.h/.cpp` (REST endpoints, 451 lines)
- Driver files: `hal_pcm5102a`, `hal_es8311`, `hal_pcm1808`, `hal_ns4150b`, `hal_mcp4725`, `hal_relay`, `hal_buzzer`, `hal_button`, `hal_encoder`, `hal_display`, `hal_led`, `hal_siggen`, `hal_signal_gen`, `hal_temp_sensor`, `hal_usb_audio`, `hal_custom_device`
- ESS SABRE ADCs: `hal_ess_sabre_adc_base` (abstract base), `hal_es9822pro`, `hal_es9826`, `hal_es9823pro`, `hal_es9821`, `hal_es9820` (2ch I2S), `hal_es9843pro`, `hal_es9842pro`, `hal_es9841`, `hal_es9840` (4ch TDM)

**src/state/**
- Purpose: Domain-specific application state (decomposed from monolithic AppState)
- Contains: 16 files (15 headers + 1 implementation for `hal_coord_state.cpp`)
- Files: `enums.h`, `general_state.h`, `ota_state.h`, `audio_state.h`, `dac_state.h`, `dsp_state.h`, `display_state.h`, `buzzer_state.h`, `signal_gen_state.h`, `usb_audio_state.h`, `wifi_state.h`, `mqtt_state.h`, `ethernet_state.h`, `debug_state.h`, `hal_coord_state.h/.cpp`
- Pattern: Each header defines one domain struct, composed into `AppState` shell. Access via `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`, `appState.halCoord.requestDeviceToggle()`

**src/gui/**
- Purpose: LVGL-based TFT display interface (Core 0 FreeRTOS task, guarded by `GUI_ENABLED`)
- Contains: 22 files — display manager, theme, input handling, navigation stack, 12 screen modules
- Key files: `gui_manager.h/.cpp` (task lifecycle), `gui_input.h/.cpp` (encoder Gray code ISR), `gui_navigation.h/.cpp` (screen stack), `gui_theme.h/.cpp` (orange accent), `gui_config.h`, `gui_icons.h`, `lgfx_config.h`, `lv_conf.h`
- Screens (`gui/screens/`): `scr_boot_anim`, `scr_home`, `scr_control`, `scr_desktop`, `scr_devices`, `scr_dsp`, `scr_wifi`, `scr_mqtt`, `scr_settings`, `scr_debug`, `scr_siggen`, `scr_support`, `scr_keyboard`, `scr_value_edit`, `scr_menu`

**src/drivers/**
- Purpose: Hardware register definitions (read-only constants, no implementation)
- Contains: 11 header files — PCM1808, ES8311, ESS SABRE common, plus per-chip register maps for all 9 ESS ADCs
- Files: `es8311_regs.h`, `ess_sabre_common.h`, `es9822pro_regs.h`, `es9843pro_regs.h`, `es9826_regs.h`, `es9823pro_regs.h`, `es9821_regs.h`, `es9820_regs.h`, `es9842pro_regs.h`, `es9841_regs.h`, `es9840_regs.h`

**web_src/**
- Purpose: Web UI source files — edit ONLY these, auto-compiled to `src/web_pages.cpp` + `src/web_pages_gz.cpp`
- CSS modules (7): `00-tokens.css` (auto-generated from `design_tokens.h`), `01-variables.css`, `02-layout.css`, `03-components.css`, `04-canvas.css`, `05-responsive.css`, `06-health-dashboard.css`
- JS modules (22, concatenated in filename order): `01-core.js` (WS client), `02-ws-router.js`, `03-app-state.js`, `04-shared-audio.js`, `05-audio-tab.js`, `06-canvas-helpers.js`, `06-peq-overlay.js`, `07-ui-core.js`, `08-ui-status.js`, `09-audio-viz.js`, `13-signal-gen.js`, `15-hal-devices.js`, `15a-yaml-parser.js`, `20-wifi-network.js`, `21-mqtt-settings.js`, `22-settings.js`, `23-firmware-update.js`, `24-hardware-stats.js`, `25-debug-console.js`, `26-support.js`, `27-auth.js`, `27a-health-dashboard.js`, `28-init.js`
- CRITICAL: Run `node tools/build_web_assets.js` after ANY edit to `web_src/` before building firmware

**test/**
- Purpose: C++ unit tests (Unity framework, native platform, no hardware needed)
- Contains: 94 test module directories + `test_mocks/` (mock Arduino/WiFi/MQTT/NVS libraries)
- Pattern: One directory per module (`test/test_<module>/test_<module>.cpp`) to avoid duplicate `main`/`setUp`/`tearDown` symbols
- Run: `pio test -e native -v` (~2316 tests across 87+ modules)
- Notable: 3 new test modules on current branch: `test_hal_probe_retry/`, `test_http_security/`, `test_ws_adaptive_rate/`

**e2e/**
- Purpose: Playwright browser end-to-end tests (mock Express server, no hardware needed)
- Contains: 22 spec files (107 tests), mock server with 12 route files, 31 fixture JSON files
- Key files: `mock-server/server.js` (Express port 3000), `mock-server/assembler.js` (HTML assembly), `mock-server/ws-state.js` (deterministic state), `helpers/fixtures.js` (connectedPage fixture), `helpers/selectors.js` (DOM selectors), `helpers/ws-helpers.js` (WS builders)
- Run: `cd e2e && npx playwright test`

**lib/**
- Purpose: Vendored libraries (not from PlatformIO registry)
- `WebSockets/`: Vendored WebSocket server library
- `esp_dsp_lite/`: ANSI C ESP-DSP fallback for native tests only (`lib_ignore`d on ESP32 builds)

**docs-site/**
- Purpose: Public documentation site (Docusaurus v3, deployed to GitHub Pages)
- Contains: 26 pages (9 user guide + 17 developer reference), design token CSS, Mermaid diagrams
- Build: `cd docs-site && npm run build && npm run serve`

**docs-internal/**
- Purpose: Internal working documents (not in public site)
- `architecture/`: 13 Mermaid diagrams + analysis docs (system, boot, HAL lifecycle, pipeline, events, etc.)
- `planning/`: Phase plans, debt registry, test strategy
- `archive/`: Completed implementation summaries

**tools/**
- Purpose: Build automation, linting, code generation (18 files)
- Build: `build_web_assets.js` (MUST run after `web_src/` edits), `extract_tokens.js` (design tokens -> CSS)
- Linting: `find_dups.js` (duplicate JS declarations), `check_missing_fns.js` (undefined function refs), `deep_check_fns.js`
- Docs: `generate_docs.js` (Claude API orchestrator), `detect_doc_changes.js`, `doc-mapping.json` (65 entries), `extract_api.js`
- Misc: `update_certs.js`, `fix_riscv_toolchain.py`, `check_hal_debug_contract.sh`, `diagram-validation.js`, `check_mapping_coverage.js`

## Key File Locations

**Entry Points:**
- `src/main.cpp`: Firmware boot (`setup()`) and main loop (`loop()`) — 1579 lines
- `platformio.ini`: PlatformIO build config (board=esp32-p4, build flags, upload port COM8)
- `tools/build_web_assets.js`: Web UI compilation (web_src/ -> src/web_pages.cpp + web_pages_gz.cpp)

**Configuration:**
- `src/config.h`: All build-time constants — pin assignments, stack sizes, buffer sizes, feature flags (`DSP_ENABLED`, `DAC_ENABLED`, `GUI_ENABLED`, `USB_AUDIO_ENABLED`), heap thresholds, firmware version
- `src/design_tokens.h`: Theme colors/sizes — source for TFT, web UI (`00-tokens.css`), and docs site
- `platformio.ini`: Board config, build flags, lib deps, upload/monitor settings
- `.github/workflows/tests.yml`: CI quality gates configuration

**Core Audio:**
- `src/audio_pipeline.h/.cpp`: 8-lane input -> 16x16 matrix -> 8-sink routing engine
- `src/dsp_pipeline.h/.cpp`: DSP stage pool, biquad IIR/FIR, limiter, compressor, delay
- `src/output_dsp.h/.cpp`: Per-output DSP (post-matrix, pre-sink)
- `src/i2s_audio.h/.cpp`: ADC/DAC I2S peripheral control, dual master coordination
- `src/audio_input_source.h`: Input lane pluggable source interface
- `src/audio_output_sink.h`: Output sink pluggable interface
- `src/sink_write_utils.h/.cpp`: Shared sink utilities (volume, mute ramp, float->int32)

**State Management:**
- `src/app_state.h/.cpp`: AppState singleton shell (composes 15 domain headers)
- `src/app_events.h/.cpp`: FreeRTOS event group (17 bits, `EVT_ANY = 0x00FFFFFF`)
- `src/state/*.h`: Domain headers — each defines one struct for one concern

**HAL Framework:**
- `src/hal/hal_device.h`: Base device class (lifecycle, config, state callback)
- `src/hal/hal_device_manager.h/.cpp`: Device registry (24 slots, pin tracking)
- `src/hal/hal_pipeline_bridge.h/.cpp`: Links HAL to audio pipeline (capability-based)
- `src/hal/hal_discovery.h/.cpp`: 3-tier auto-detection (I2C -> EEPROM -> manual)
- `src/hal/hal_builtin_devices.h/.cpp`: Driver registry (compatible string -> factory)
- `src/hal/hal_ess_sabre_adc_base.h/.cpp`: Shared ESS SABRE ADC base class
- `src/hal/hal_api.h/.cpp`: REST endpoints for HAL CRUD

**Network:**
- `src/wifi_manager.h/.cpp`: WiFi client + AP mode
- `src/eth_manager.h/.cpp`: Ethernet (100Mbps)
- `src/mqtt_handler.h/.cpp` + `src/mqtt_publish.cpp` + `src/mqtt_ha_discovery.cpp`: MQTT stack
- `src/mqtt_task.h/.cpp`: Dedicated MQTT FreeRTOS task

**Web Server:**
- `src/websocket_handler.h/.cpp`: Real-time WS server (port 81)
- `src/auth_handler.h/.cpp`: PBKDF2-SHA256 auth, WS token pool, rate limiting
- `src/http_security.h`: Security headers applied to all HTTP responses
- `src/web_pages.h/.cpp`: AUTO-GENERATED from web_src/ (do not edit)

**REST APIs:**
- `src/dsp_api.h/.cpp`: DSP config CRUD (1437 lines)
- `src/pipeline_api.h/.cpp`: Audio matrix + per-output DSP (323 lines)
- `src/dac_api.h/.cpp`: DAC state/volume/enable (530 lines)
- `src/hal/hal_api.h/.cpp`: HAL device CRUD (451 lines)
- `src/psram_api.h/.cpp`: PSRAM health status (60 lines)

**Diagnostics:**
- `src/diag_journal.h/.cpp`: Event ringbuffer (64 entries)
- `src/diag_error_codes.h`: Error code enum (100+ codes)
- `src/diag_event.h`: Emission macro
- `src/heap_budget.h/.cpp`: Per-subsystem allocation tracker (32 entries)
- `src/psram_alloc.h/.cpp`: Unified PSRAM allocation wrapper with SRAM fallback
- `src/task_monitor.h/.cpp`: FreeRTOS task enumeration
- `src/crash_log.h/.cpp`: Reset reason ringbuffer

**Persistence (LittleFS):**
- `/config.json`: Settings (WiFi, MQTT, sample rate, etc.) — atomic write via tmp+rename
- `/hal_config.json`: Per-device HAL config (pin overrides, enable state, gain)
- `/hal_auto_devices.json`: Auto-discovered add-on devices (EEPROM metadata)
- `/crash_log.json`: Reset reason ringbuffer
- `/diag_journal.json`: Diagnostic event ringbuffer

**Testing:**
- `test/test_mocks/`: Mock Arduino/WiFi/MQTT/NVS headers
- `test/test_<module>/test_<module>.cpp`: One directory per test module
- `e2e/tests/*.spec.js`: Playwright browser test specs
- `e2e/mock-server/`: Express server replicating firmware REST API
- `e2e/fixtures/`: Hand-crafted test data (WS messages + API responses)

## Naming Conventions

**Files:**
- C++ source: `module_name.h` / `module_name.cpp` (lowercase, underscores)
- HAL drivers: `hal_<device_type>.h/.cpp` (e.g., `hal_es9822pro.h/.cpp`)
- HAL ESS SABRE ADCs: `hal_es<XXXX>.h/.cpp` (chip model number)
- State headers: `src/state/<domain>_state.h` (e.g., `audio_state.h`, `wifi_state.h`)
- Driver registers: `src/drivers/<chip>_regs.h` (e.g., `es9822pro_regs.h`)
- Test modules: `test/test_<module>/test_<module>.cpp` (one per directory)
- Web CSS: `NN-feature.css` (numeric prefix for load order, 00-06)
- Web JS: `NN-feature.js` (numeric prefix for load order, 01-28)
- GUI screens: `src/gui/screens/scr_<name>.h/.cpp` (e.g., `scr_home.h`)

**Directories:**
- Feature domain: `src/<feature>/` (e.g., `src/hal/`, `src/gui/`, `src/state/`, `src/drivers/`)
- Test suite: `test/test_<module>/` (one per test module, avoids symbol conflicts)
- Build artifacts: `.pio/build/` (git-ignored)
- Generated docs: `.planning/codebase/` (committed)

**Functions:**
- Module functions: `snake_case` with module prefix — `audio_pipeline_set_matrix_gain()`, `hal_device_manager_register()`, `dsp_add_stage()`
- Init functions: `<module>_init()` — `audio_pipeline_init()`, `gui_init()`, `mqtt_task_start()`
- API registration: `register<Module>ApiEndpoints()` — `registerHalApiEndpoints()`, `registerDspApiEndpoints()`
- Handler callbacks: `<module>Event()` or `<module>Callback()` — `webSocketEvent()`, `mqttCallback()`
- ISR handlers: `<module>_isr()` — `button_isr()`

**Variables:**
- Global state: `appState` (macro for `AppState::getInstance()`), domain access `appState.wifi.ssid`
- File-local statics: `_leadingUnderscore` (private to translation unit)
- Class members: `_leadingUnderscore` (private/protected)
- Constants: `UPPER_SNAKE_CASE` — `HAL_MAX_DEVICES`, `HEAP_WARNING_THRESHOLD`

**Types:**
- Struct: `PascalCase` — `AudioInputSource`, `HalDeviceConfig`, `DspStage`, `HeapBudgetEntry`
- Enum: `PascalCase` — `AppFSMState`, `HalDeviceState`, `FftWindowType`
- Class: `PascalCase` with `Hal` prefix for HAL — `HalPcm5102a`, `HalDevice`, `HalEssSabreAdcBase`
- State structs: `<Domain>State` — `WifiState`, `AudioState`, `DebugState`

## Where to Add New Code

**New Firmware Feature (Audio DSP, Signal Processing):**
- Primary code: `src/<feature_name>.h/.cpp`
- State: Create `src/state/<feature>_state.h`, include in `src/app_state.h`, add dirty flag setter/getter + event bit in `src/app_events.h` (7 spare bits available)
- Tests: Create `test/test_<feature>/test_<feature>.cpp` (Unity framework)
- REST API: Create `src/<feature>_api.h/.cpp`, register via `register<Feature>ApiEndpoints()` in `src/main.cpp`

**New HAL Device Driver (ADC, DAC, Codec, Sensor):**
- Driver class: `src/hal/hal_<device>.h/.cpp` (subclass `HalDevice`, implement `init()`/`deinit()`)
- For ESS SABRE ADC: subclass `HalEssSabreAdcBase`, add register map to `src/drivers/<chip>_regs.h`
- For 4ch TDM device: use `HalTdmDeinterleaver` (see `hal_es9843pro.cpp` pattern)
- Register: Add factory function to `src/hal/hal_builtin_devices.cpp`
- If audio device: implement `AudioOutputSink` or `AudioInputSource`, HAL pipeline bridge handles registration automatically via state callback
- Tests: `test/test_hal_<device>/test_hal_<device>.cpp`
- Documentation: `docs-site/docs/developer/hal/`

**New Web UI Tab or Feature:**
- HTML: Add `<div id="tabName">` container to `web_src/index.html`
- CSS: Create `web_src/css/NN-<feature>.css`
- JS: Create `web_src/js/NN-<feature>.js` (pick number that places it in correct load order)
- WS messages: Add handler in `web_src/js/02-ws-router.js`
- Globals: Add new top-level declarations to `web_src/.eslintrc.json` globals
- E2E test: Add `e2e/tests/<feature>.spec.js` + fixtures in `e2e/fixtures/`
- CRITICAL: Run `node tools/build_web_assets.js` after edits, THEN build firmware

**New REST API Endpoint:**
- Implementation: Create `src/<feature>_api.h/.cpp` with `register<Feature>ApiEndpoints()`
- Auth: Use auth guard pattern from existing endpoints (see `src/dac_api.cpp`)
- Security headers: Call `http_add_security_headers()` from `src/http_security.h`
- Register: Add `register<Feature>ApiEndpoints()` call in `src/main.cpp` setup()
- E2E mock: Add route file `e2e/mock-server/routes/<feature>.js`, add fixture JSON

**New Network Integration (WiFi, MQTT, External API):**
- Implementation: `src/<integration>.h/.cpp`
- If concurrent: Spawn FreeRTOS task on Core 0 (never Core 1), coordinate via dirty flags
- State: Add domain header to `src/state/`, add event bit, include in `src/app_state.h`
- Tests: `test/test_<integration>/` with mocked network calls

**New GUI Screen:**
- Screen module: `src/gui/screens/scr_<name>.h/.cpp`
- Navigation: Link via `gui_push_screen()` / `gui_pop_screen()` in `src/gui/gui_navigation.cpp`
- Guard: All GUI code must be inside `#ifdef GUI_ENABLED`
- Tests: `test/test_gui_<name>/` with LVGL object tree verification

**New Utility Function:**
- Shared helpers: `src/utils.h/.cpp`
- Module-specific: inline in the module's header
- Avoid: Creating new utility files unless >200 lines

**New Build-Time Configuration:**
- Constants: Add to `src/config.h` with descriptive comments
- Feature flags: Use `#ifdef FEATURE_ENABLED` / `#endif` to gate entire modules
- PlatformIO flags: Add `-D FLAG_NAME` to `platformio.ini` build_flags section

## Special Directories

**`.pio/build/`**
- Purpose: PlatformIO build artifacts (object files, firmware binary, test executables)
- Generated: Yes (by `pio run`, `pio test`)
- Committed: No (git-ignored)

**`.planning/codebase/`**
- Purpose: GSD (Claude Code) codebase analysis documents
- Generated: Yes (by `/gsd:map-codebase`)
- Committed: Yes (checked in for context preservation across conversations)

**`logs/`**
- Purpose: Build output, test reports, serial captures (keeps project root clean)
- Generated: Yes
- Committed: No (git-ignored)

**`.github/workflows/`**
- Purpose: CI/CD automation (GitHub Actions)
- Generated: No (hand-authored)
- Contains: `tests.yml` (4 parallel quality gates on push/PR), `release.yml` (same gates before release), `docs.yml` (doc generation + deploy)

**`.githooks/`**
- Purpose: Pre-commit checks (find_dups + check_missing_fns + ESLint on web_src/js/)
- Activate: `git config core.hooksPath .githooks`
- Committed: Yes

**`.claude/`**
- Purpose: Claude Code configuration (agents, commands, skills, plans, reviews)
- Contains: Agent definitions in `.claude/agents/`, custom commands in `.claude/commands/`
- Committed: Partially (agents and commands yes, skills vary)

---

*Structure analysis: 2026-03-22*
