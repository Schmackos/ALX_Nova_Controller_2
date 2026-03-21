# Codebase Structure

**Analysis Date:** 2026-03-21

## Directory Layout

```
[project-root]/
├── src/                          # Main firmware codebase (C++)
│   ├── state/                    # Domain-specific state headers (15 files)
│   ├── hal/                      # Hardware Abstraction Layer (50+ files)
│   ├── gui/                      # LVGL GUI on Core 0 (15 files + 15 screens)
│   ├── drivers/                  # Chip register definitions (1 file: ES8311)
│   ├── main.cpp                  # Arduino setup() / loop() entry point
│   ├── app_state.h/.cpp          # AppState singleton (~80 lines)
│   ├── app_events.h/.cpp         # FreeRTOS event group signaling
│   ├── config.h                  # Build flags, constants, pin definitions
│   ├── audio_pipeline.h/.cpp     # Core real-time lane/matrix/sink dispatch
│   ├── dsp_pipeline.h/.cpp       # 24-stage multi-channel DSP (optional, -DDSP_ENABLED)
│   ├── output_dsp.h/.cpp         # Per-output post-matrix DSP
│   ├── i2s_audio.h/.cpp          # I2S driver, ADC/DAC init, FFT analysis
│   ├── dac_hal.h/.cpp            # DAC state queries (bus-utility only)
│   ├── websocket_handler.cpp     # WS server, state broadcast, auth, binary frames
│   ├── wifi_manager.cpp          # WiFi client + AP mode, SDIO guard
│   ├── mqtt_handler.cpp          # MQTT connect/config load
│   ├── mqtt_publish.cpp          # Change-detection statics, periodic publish
│   ├── mqtt_ha_discovery.cpp     # Home Assistant discovery payloads
│   ├── mqtt_task.cpp             # Dedicated FreeRTOS task on Core 0
│   ├── settings_manager.cpp      # JSON config I/O, atomic writes
│   ├── auth_handler.cpp          # PBKDF2 auth, token pool, rate limiting
│   ├── smart_sensing.cpp         # Signal detection, amplifier relay, auto-off
│   ├── signal_generator.cpp      # Multi-waveform test signal, PWM/ADC injection
│   ├── button_handler.cpp        # Debounce + press pattern detection
│   ├── buzzer_handler.cpp        # Tone sequencer, ISR-safe LEDC
│   ├── usb_audio.cpp             # TinyUSB UAC2 speaker (guarded by -DUSB_AUDIO_ENABLED)
│   ├── ota_updater.cpp           # GitHub release check, SHA256 verify, streaming download
│   ├── heap_budget.h/.cpp        # Per-subsystem allocation tracker (32 entries)
│   ├── heap_monitor.cpp          # 3-state heap pressure (normal/warning/critical)
│   ├── diag_journal.h/.cpp       # 800-entry persistent error code ring buffer
│   ├── diag_event.h              # Error code constants
│   ├── crash_log.h/.cpp          # Reboot reason capture via NVS
│   ├── task_monitor.cpp          # FreeRTOS task enumeration (5s timer)
│   ├── thd_measurement.h/.cpp    # THD+N measurement engine
│   ├── dsp_convolution.h/.cpp    # FIR convolution/IR processing
│   ├── pipeline_api.h/.cpp       # REST API: matrix CRUD, output DSP
│   ├── dac_api.h/.cpp            # REST API: DAC state, volume, enable/disable
│   ├── dsp_api.h/.cpp            # REST API: DSP config CRUD (1600+ lines)
│   ├── debug_serial.h/.cpp       # Log-level filtering, WS forwarding
│   ├── web_pages.cpp             # Auto-generated HTML/CSS/JS (563KB gzip)
│   ├── web_pages_gz.cpp          # Gzip-compressed assets (636KB)
│   ├── websocket_handler.h       # WS header for external declarations
│   ├── globals.h                 # Global extern declarations (cleanly isolated)
│   ├── utils.h/.cpp              # Misc utility functions
│   ├── strings.h                 # Static string constants (localization-ready)
│   └── (35 more header files)     # Auxiliary state, types, HAL declarations
│
├── web_src/                      # Web UI source (concatenated into web_pages.cpp)
│   ├── index.html                # HTML shell + body content
│   ├── css/                      # 01-05-*.css (variables, layout, components, canvas, responsive)
│   ├── js/                       # 01-28-*.js (core, state, UI, audio, DSP, WiFi, settings, system)
│   └── .eslintrc.json            # ESLint config (380 globals for concatenated scope)
│
├── test/                         # C++ unit tests (Unity framework)
│   ├── test_mocks/               # Mock Arduino, WiFi, MQTT, Preferences, NVS
│   ├── test_<module>/            # 75 test modules (one per test file to avoid duplicate main)
│   │   └── test_<module>.cpp
│   └── (no compilation of src/ — tests #include specific headers + use mocks)
│
├── e2e/                          # Playwright browser tests (26 tests, 19 specs)
│   ├── tests/                    # Spec files: auth, wifi, mqtt, audio, dsp, settings, etc.
│   ├── mock-server/              # Express server (port 3000) + WS interception
│   ├── helpers/                  # Fixtures, selectors, WS helpers
│   ├── fixtures/                 # Hand-crafted WS/API response JSON
│   └── playwright.config.js      # Browser launcher, base URL, timeout
│
├── docs-site/                    # Docusaurus v3 public documentation (26 pages)
│   ├── docs/                     # User Guide (9 pages) + Developer Reference (17 pages)
│   ├── src/                      # Docusaurus config + landing page
│   ├── sidebars.js               # Menu structure (userSidebar, devSidebar)
│   └── package.json              # Build scripts: build, serve, deploy
│
├── docs-internal/                # Internal working docs (not on public site)
│   ├── planning/                 # Implementation plans, debt registry, topic files
│   └── architecture/             # Mermaid diagrams (system, HAL, boot, test infrastructure)
│
├── tools/                        # Build & maintenance scripts
│   ├── build_web_assets.js       # Concatenate web_src/ into web_pages.cpp (run before firmware build)
│   ├── find_dups.js              # Detect duplicate JS declarations (pre-commit hook)
│   ├── check_missing_fns.js      # Undefined function references (pre-commit hook)
│   ├── extract_tokens.js         # Design token extraction (design_tokens.h → CSS)
│   ├── extract_api.js            # API documentation extraction (informational)
│   ├── generate_docs.js          # CI doc regeneration via Claude API
│   ├── detect_doc_changes.js     # Git diff → section mapping
│   └── doc-mapping.json          # Source file → documentation mapping (65 entries)
│
├── .github/workflows/            # GitHub Actions CI/CD
│   ├── tests.yml                 # Quality gates (4 parallel: cpp-tests, cpp-lint, js-lint, e2e-tests)
│   ├── docs.yml                  # Doc detection → generation → build → deploy
│   └── release.yml               # Pre-release gates + publish
│
├── lib/                          # PlatformIO dependency vendoring
│   ├── WebSockets/               # Vendored WebSocketsServer (no longer from lib_deps registry)
│   ├── esp_dsp_lite/             # ANSI C ESP-DSP fallback (lib_ignore on ESP32 builds)
│   └── (other archived dependencies)
│
├── .githooks/                    # Pre-commit hooks
│   └── pre-commit                # Run find_dups + check_missing_fns + ESLint
│
├── platformio.ini                # PlatformIO project config (native + esp32-p4 envs)
├── .editorconfig                 # Code formatting (indent size, line endings)
├── CLAUDE.md                     # Project instructions for Claude Code (THIS FILE)
└── README.md                     # Public project overview
```

## Directory Purposes

**src/state/:**
- Purpose: Domain-specific immutable state composition (replaces monolithic AppState)
- Contains: 15 header files defining structs (GeneralState, AudioState, WifiState, DspState, etc.)
- Key files: `enums.h` (shared FSM + window enums), `general_state.h`, `audio_state.h`, `hal_coord_state.h` (device toggle queue)
- Pattern: Each domain includes only headers it needs. Cross-core volatile semantics on individual fields. No locks (Core 1 audio task reads lock-free)

**src/hal/:**
- Purpose: Hardware Abstraction Layer — unified device driver interface
- Contains: Device base class, manager, discovery, 50+ builtin drivers, config persistence, pipeline bridge
- Key files: `hal_device.h` (abstract base), `hal_device_manager.h/.cpp` (lifecycle owner), `hal_discovery.h/.cpp` (3-tier bus scan), `hal_pipeline_bridge.h/.cpp` (state→pipeline mapper), `hal_builtin_devices.cpp` (driver registry)
- Driver files: `hal_pcm5102a`, `hal_es8311`, `hal_pcm1808`, `hal_mcp4725`, `hal_ns4150b`, `hal_relay`, `hal_button`, `hal_encoder`, `hal_buzzer`, `hal_led`, `hal_display`, `hal_siggen`, `hal_usb_audio`, `hal_temp_sensor`, `hal_eeprom_v3`

**src/gui/:**
- Purpose: LVGL 9.4 TFT display interface (Core 0 exclusive, 160×128 landscape ST7735S)
- Contains: Manager, input handler (Gray code rotary), theme, navigation, 15 screen implementations
- Key files: `gui_manager.h/.cpp` (FreeRTOS task init, screen sleep/wake), `gui_input.cpp` (encoder ISR), `gui_navigation.cpp` (screen stack + transitions), `gui_theme.cpp` (orange accent, dark/light mode)
- Screens: desktop (carousel), home (status), control, wifi, mqtt, settings, debug, support, keyboard, value editor, boot animation
- Guarded by: `-DGUI_ENABLED` build flag

**web_src/:**
- Purpose: Source files for embedded web UI (auto-concatenated → gzip → web_pages.cpp/web_pages_gz.cpp)
- Contains: HTML shell, CSS modules (variables, layout, components, canvas, responsive), JS modules (core, WS router, UI, audio, DSP, WiFi, settings, system)
- Pattern: All JS files concatenated in filename order into single `<script>` block (shared scope). NO duplicate `let`/`const`. Check with `node tools/find_dups.js`
- Build: After ANY edit to web_src/, run `node tools/build_web_assets.js` to regenerate web_pages.cpp + web_pages_gz.cpp before firmware build. NEVER edit web_pages.cpp directly

**test/:**
- Purpose: C++ unit tests (Unity framework, native platform — no hardware needed)
- Contains: Mock headers (Arduino, WiFi, MQTT, Preferences) + 75 test modules (1730+ tests)
- Pattern: Each test file in its own directory to avoid duplicate main/setUp/tearDown symbols. `test_build_src = no` in platformio.ini (tests #include specific headers, not entire src/)
- Run: `pio test -e native` (all tests), `pio test -e native -f test_<module> -v` (verbose single module)

**e2e/:**
- Purpose: Browser-based end-to-end tests (Playwright, mock Express server, 26 tests across 19 specs)
- Contains: Test specs, mock server (port 3000) with WS interception, fixtures, helpers, selectors
- Key files: `mock-server/server.js` (Express + route handlers), `mock-server/ws-state.js` (deterministic mock state), `helpers/fixtures.js` (connectedPage fixture), `helpers/ws-helpers.js` (buildInitialState, handleCommand)
- Run: `cd e2e && npm install && npx playwright test` (all tests), `npx playwright test --headed` (visible browser)

**docs-site/:**
- Purpose: Public Docusaurus v3 documentation (26 pages: 9 user guide + 17 developer reference)
- Contains: MDX source pages, CSS (design tokens), landing page
- Build: `npm install && npm run build && npm run serve` (local dev), deployed to GitHub Pages
- Critical: Escape curly braces in markdown tables: `\{variable\}` (code blocks safe)
- Token sync: `node tools/extract_tokens.js` after changing `src/design_tokens.h`

**docs-internal/:**
- Purpose: Internal working documentation (separate from public site)
- Contains: Implementation plans (`.claude/plans/`), topic files (hal.md, ota.md, hardware.md), Mermaid diagrams (architecture/)
- Not committed to git directly (`.claude/plans/` managed by GSD orchestrator)

**tools/:**
- Purpose: Build automation + maintenance utilities
- Key scripts:
  - `build_web_assets.js` — Mandatory after editing web_src/
  - `find_dups.js`, `check_missing_fns.js` — Pre-commit hooks
  - `extract_tokens.js` — Design token pipeline
  - `generate_docs.js` — CI doc automation (requires ANTHROPIC_API_KEY secret)
  - `doc-mapping.json` — 65 source file → doc section mappings

## Key File Locations

**Entry Points:**
- `src/main.cpp` — Arduino `setup()` / `loop()` (1500 lines). Singleton creation, task spawning, REST endpoint registration
- `src/audio_pipeline.cpp` — `audio_pipeline_task()` FreeRTOS task (Core 1 exclusive). Real-time sample I/O → matrix → DSP → sink dispatch
- `src/mqtt_task.cpp` — Dedicated MQTT task (Core 0). Broker reconnect, 20Hz publish loop
- `src/websocket_handler.cpp::webSocketEvent()` — WS event dispatcher (connect/disconnect/message)

**Configuration:**
- `src/config.h` — Build constants (pin definitions, DSP/USB/GUI feature flags, thresholds, stack sizes)
- `src/app_state.h` — Singleton declaration (orchestrates 15 domain state headers)
- `platformio.ini` — PlatformIO project config (envs: native for testing, esp32-p4 for firmware)

**Core Logic:**
- `src/audio_pipeline.h/.cpp` — Lane→matrix→sink dispatcher (2200 lines)
- `src/dsp_pipeline.h/.cpp` — 24-stage DSP engine (3300 lines, optional)
- `src/hal/hal_device_manager.h/.cpp` — Device lifecycle + registry (550 lines)
- `src/hal/hal_discovery.h/.cpp` — 3-tier I2C/EEPROM/manual discovery (520 lines)
- `src/hal/hal_pipeline_bridge.h/.cpp` — HAL state → audio pipeline mapper (380 lines)
- `src/i2s_audio.h/.cpp` — Dual PCM1808 I2S ADC driver + FFT analysis (1850 lines)

**Networking:**
- `src/websocket_handler.cpp` — WS server, state broadcast, binary frames (2800 lines)
- `src/wifi_manager.cpp` — WiFi client/AP mode, SDIO conflict guard (1800 lines)
- `src/mqtt_handler.cpp` — MQTT connect, HA discovery (1800 lines)
- `src/mqtt_publish.cpp` — Change-detection statics, periodic publish (1200 lines)
- `src/mqtt_task.cpp` — Dedicated FreeRTOS task (40 lines)

**State & Settings:**
- `src/settings_manager.cpp` — JSON config I/O, atomic writes, factory reset (2200 lines)
- `src/auth_handler.cpp` — PBKDF2 auth, WebSocket tokens, rate limiting (650 lines)
- `src/state/hal_coord_state.h` — Device toggle queue (inline + spinlock in cpp)
- `src/app_events.h/.cpp` — FreeRTOS event group signaling

**Testing:**
- `test/` — 75 test modules (1730+ tests), all with mock implementations
- `e2e/` — 26 Playwright browser tests, mock Express server

## Naming Conventions

**Files:**
- Module names: snake_case + `.h/.cpp` pairs (e.g., `audio_pipeline.h`, `websocket_handler.cpp`)
- HAL drivers: `hal_<device_name>.h/.cpp` (e.g., `hal_pcm5102a.cpp`, `hal_ns4150b.cpp`)
- State domain headers: `src/state/<domain>_state.h` (e.g., `audio_state.h`, `wifi_state.h`)
- GUI screens: `scr_<screen_name>.h/.cpp` (e.g., `scr_home.cpp`, `scr_dsp.cpp`)
- Test modules: `test_<module>/test_<module>.cpp` (one per directory to avoid duplicate main)
- Auto-generated: `web_pages.cpp`, `web_pages_gz.cpp` (from `tools/build_web_assets.js` — NEVER edit manually)

**Directories:**
- Subsystems: lowercase (src/state, src/hal, src/gui, src/drivers)
- Web UI: `web_src/css/`, `web_src/js/` with numeric prefixes (01-28) for load order
- Builds: `.pio/build/` (PlatformIO build output, `.gitignored`)

**Functions:**
- Public module API: snake_case (e.g., `audio_pipeline_set_sink()`, `hal_discovery_scan()`)
- HAL device methods: camelCase in class (e.g., `probe()`, `init()`, `buildSink()`)
- Handler callbacks: descriptive names with `_callback` or `_handler` suffix (e.g., `webSocketEvent()`, `hal_pipeline_state_change()`)
- REST endpoint handlers: `handle<EndpointName>()` or `<operation><Resource>Handler()` (e.g., `handleSmartSensingGet()`)

**Variables:**
- File-static: `_prefix` (e.g., `_wsAuthCount`, `_pendingInitState`)
- Member fields: `_underscore` prefix in classes (e.g., `_descriptor`, `_ready`, `_state`)
- Globals in headers: `extern Type varName;` (declared in headers, defined in one .cpp)
- Constants: `UPPER_CASE` (e.g., `HEAP_WARNING_THRESHOLD`, `MAX_WS_CLIENTS`)

**Types:**
- Classes: PascalCase (e.g., `AppState`, `HalDevice`, `HalDeviceManager`)
- Structs: PascalCase (e.g., `AudioInputSource`, `DspBiquadParams`, `HalDeviceDescriptor`)
- Enums: PascalCase values (e.g., `HAL_STATE_UNKNOWN`, `EVT_WIFI`, `DSP_BIQUAD_PEQ`)
- Typedef pointers: usually avoided (prefer class pointers directly)

## Where to Add New Code

**New Feature (Audio Processing):**
- Primary code: `src/dsp_api.cpp` (REST endpoints), `src/dsp_pipeline.h/.cpp` (stage implementation)
- Tests: `test_dsp_<feature>/` (new test module)
- Web UI: `web_src/js/06-peq-overlay.js` (if DSP control overlay) or new tab in `web_src/js/05-audio-tab.js`
- Configuration: Add to `DspSettingsState` in `src/state/dsp_state.h` if persistent state needed
- MQTT: Add to `mqtt_ha_discovery.cpp` if Home Assistant integration required

**New HAL Device Driver:**
- Implementation: `src/hal/hal_<device_name>.h/.cpp` (inherit from `HalDevice`, implement `probe()`, `init()`, `deinit()`, `healthCheck()`)
- Registration: Add factory function to `src/hal/hal_builtin_devices.cpp` with compatible string matching
- Discovery: Already handled by `hal_discovery.h/.cpp` (EEPROM v3 + I2C bus scan)
- Pipeline integration: Override `getInputSource()` (for ADC) or `buildSink()` (for DAC)
- Tests: `test_hal_<device_name>/` with mock I2C/GPIO
- REST API: Already generated by `hal_api.cpp` (enumerate, config, reinit)

**New Component/Module:**
- Standalone module: Create `src/<module_name>.h/.cpp` pair
- Include pattern: Add includes in `src/main.cpp` (conditional on build flag if optional)
- State: If persistent config, add struct to appropriate domain state header in `src/state/`
- HTTP handler: Register endpoint in `src/main.cpp` → `server.on()` chain
- WebSocket broadcast: Add dirty flag methods to `AppState`, broadcast logic in `websocket_handler.cpp`
- Testing: `test_<module>/test_<module>.cpp` with mocks as needed

**Utility Functions:**
- Shared helpers: `src/utils.h/.cpp` (general purpose) or domain-specific header
- Math/DSP utilities: `src/dsp_biquad_gen.h/.c` (RBJ Audio EQ Cookbook functions)
- Audio sink utilities: `src/sink_write_utils.h/.cpp` (volume, mute ramp, float→I2S conversion)
- String constants: `src/strings.h` (static strings, localization-ready)

**Web UI Component:**
- New tab: Add `<button class="tab-btn" data-tab="tabName">Label</button>` to `web_src/index.html` body, create tab div with `id="tab-tabName"`
- New JS module: Create `web_src/js/NN-name.js` with numeric prefix in load order (must run `tools/build_web_assets.js` after)
- New CSS: Add to appropriate `web_src/css/NN-*.css` file by concern (variables, layout, components, canvas, responsive)
- Icons: Use inline SVG with Material Design Icons (MDI) paths from pictogrammers.com
- Test: Add Playwright spec in `e2e/tests/` with navigation + assertion tests

## Special Directories

**src/state/:**
- Purpose: Domain-specific immutable state composition
- Generated: No (hand-edited headers)
- Committed: Yes
- Pattern: 15 lightweight headers (one per domain), composed into AppState singleton. No circular deps, no locks

**.pio/:**
- Purpose: PlatformIO build artifacts (toolchain, dependencies, object files)
- Generated: Yes (auto-created by PlatformIO)
- Committed: No (.gitignored)
- Size: ~2GB for full native + esp32-p4 builds

**web_src/ → src/web_pages.cpp:**
- Purpose: Source → compiled pipeline (edit web_src/, NOT web_pages.cpp)
- Generated: Yes (by `tools/build_web_assets.js` from web_src/ concatenation + gzip)
- Committed: Yes (generated files checked in, auto-updated by pre-commit hook in future)
- Gotcha: NEVER edit web_pages.cpp directly — changes will be overwritten on next build

**test/test_mocks/:**
- Purpose: Mock implementations of Arduino, WiFi, MQTT, Preferences libraries
- Generated: No (hand-maintained)
- Committed: Yes
- Usage: Tests #include these instead of real libraries when compiling for `native` environment

**e2e/mock-server/:**
- Purpose: Express server + WebSocket interception for browser tests
- Generated: No (hand-written Node.js + Playwright fixtures)
- Committed: Yes
- Startup: Automatically launched by Playwright before test suite runs

**docs-site/docs/:**
- Purpose: MDX source for Docusaurus site
- Generated: Partially (some pages auto-generated by `tools/generate_docs.js` via Claude API)
- Committed: Yes
- Deployment: Built to static HTML → deployed to GitHub Pages (gh-pages branch)

---

*Structure analysis: 2026-03-21*
