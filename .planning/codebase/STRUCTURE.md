# Codebase Structure

**Analysis Date:** 2026-03-07

## Directory Layout

```
ALX_Nova_Controller_2/
├── src/                     # All firmware source (C++/C)
│   ├── hal/                 # Hardware abstraction layer subsystem
│   ├── gui/                 # LVGL GUI subsystem (GUI_ENABLED)
│   │   └── screens/         # Individual LVGL screen implementations
│   └── drivers/             # Low-level chip register drivers
├── web_src/                 # Web UI source — EDIT HERE, not src/web_pages.cpp
│   ├── js/                  # JavaScript modules (concatenated in filename order)
│   └── css/                 # CSS files (concatenated in filename order)
├── test/                    # Unity unit tests (native platform)
│   ├── test_mocks/          # Mock headers for Arduino/WiFi/MQTT/NVS/FreeRTOS
│   └── test_*/              # One directory per test module
├── e2e/                     # Playwright browser E2E tests
│   ├── tests/               # Playwright spec files (.spec.js)
│   ├── helpers/             # Shared Playwright fixtures and selectors
│   ├── fixtures/            # Static WS message and REST API response fixtures
│   │   ├── ws-messages/     # JSON fixtures for WebSocket broadcast messages
│   │   └── api-responses/   # JSON fixtures for REST API responses
│   └── mock-server/         # Express dev server assembling web_src/ for tests
│       └── routes/          # 12 Express route files mirroring firmware REST API
├── lib/                     # Local libraries
│   └── esp_dsp_lite/        # ANSI C fallback DSP for native tests (lib_ignore on ESP32)
├── tools/                   # Build and validation scripts (Node.js + Python)
├── docs/                    # Architecture diagrams, planning notes, hardware docs
│   ├── architecture/        # Mermaid diagrams (.mmd) for system, HAL, CI, tests
│   ├── planning/            # Feature planning documents
│   ├── development/         # Dev guides and testing architecture
│   └── hardware/            # Pin diagrams, board datasheets
├── .github/workflows/       # GitHub Actions CI (tests.yml, release.yml)
├── .githooks/               # Pre-commit hooks (ESLint, find_dups, check_missing_fns)
├── logs/                    # Build output, test reports, serial captures
├── .planning/codebase/      # GSD codebase analysis documents
├── platformio.ini           # PlatformIO build configuration (3 envs)
├── partitions_ota.csv       # Flash partition table (4MB ota_0 + 4MB ota_1)
└── CLAUDE.md                # AI guidance document (build commands, conventions)
```

## Directory Purposes

**`src/`:**
- Purpose: All firmware C++/C source. Each subsystem is a `.h/.cpp` pair.
- Contains: Main entry point, AppState singleton, all module implementations, auto-generated web pages
- Key files:
  - `src/main.cpp` — `setup()` + `loop()`, all HTTP route registrations, global server/client instances
  - `src/app_state.h/.cpp` — `AppState` singleton, all shared mutable state, dirty flags
  - `src/app_events.h/.cpp` — FreeRTOS event group wrapper, event bit definitions
  - `src/config.h` — Firmware version, all pin definitions, task/DSP/audio constants
  - `src/audio_pipeline.h/.cpp` — Audio pipeline FreeRTOS task, routing matrix, sink dispatch
  - `src/i2s_audio.h/.cpp` — Dual PCM1808 I2S ADC HAL, FFT/waveform buffers
  - `src/dsp_pipeline.h/.cpp` — Per-input biquad/FIR/gain/delay/compressor DSP engine
  - `src/output_dsp.h/.cpp` — Per-output mono DSP post-matrix
  - `src/mqtt_task.h/.cpp` — Dedicated MQTT FreeRTOS task (Core 0)
  - `src/websocket_handler.h/.cpp` — WS event handler, all broadcast functions
  - `src/web_pages.h/.cpp` — Auto-generated; contains inline HTML + gzip arrays (do not edit)
  - `src/web_pages_gz.h/.cpp` — Auto-generated gzip variant (do not edit)

**`src/hal/`:**
- Purpose: Hardware abstraction layer — device model, lifecycle, discovery, drivers
- Key files:
  - `src/hal/hal_types.h` — All HAL enums and structs (`HalDeviceType`, `HalDeviceState`, `HalDeviceConfig`, etc.)
  - `src/hal/hal_device.h` — `HalDevice` abstract base class
  - `src/hal/hal_device_manager.h/.cpp` — `HalDeviceManager` singleton (up to 16 devices, 24-pin tracking)
  - `src/hal/hal_pipeline_bridge.h/.cpp` — State change callback → audio pipeline sink management
  - `src/hal/hal_audio_health_bridge.h/.cpp` — ADC health → HAL state transitions
  - `src/hal/hal_device_db.h/.cpp` — In-memory device database + LittleFS JSON persistence
  - `src/hal/hal_discovery.h/.cpp` — 3-tier discovery (I2C scan → EEPROM → manual)
  - `src/hal/hal_builtin_devices.h/.cpp` — Driver registry: compatible string → factory function
  - `src/hal/hal_api.h/.cpp` — REST endpoints for HAL device CRUD
  - `src/hal/hal_settings.h/.cpp` — HAL config persistence to `/hal_config.json`
  - `src/hal/hal_init_result.h` — `HalInitResult` struct (error code + reason string)
  - Drivers: `hal_pcm5102a`, `hal_es8311`, `hal_pcm1808`, `hal_ns4150b`, `hal_temp_sensor`, `hal_mcp4725`, `hal_dac_adapter`, `hal_i2s_bridge`, `hal_dsp_bridge`, `hal_display`, `hal_encoder`, `hal_buzzer`, `hal_led`, `hal_relay`, `hal_button`, `hal_signal_gen`, `hal_custom_device`

**`src/gui/`:**
- Purpose: LVGL GUI on ST7735S 160×128 TFT (guarded by `GUI_ENABLED`)
- Key files:
  - `src/gui/gui_manager.h/.cpp` — `gui_init()`, `gui_task` FreeRTOS task, sleep/wake
  - `src/gui/gui_input.h/.cpp` — ISR-driven rotary encoder (Gray code state machine)
  - `src/gui/gui_navigation.h/.cpp` — Screen stack push/pop + transition animations
  - `src/gui/gui_theme.h/.cpp` — Orange accent dark/light theme
  - `src/gui/lgfx_config.h` — LovyanGFX ST7735S display config (confirmed working settings)
  - `src/gui/lv_conf.h` — LVGL v9.4 configuration
  - `src/gui/screens/` — 14 screen pairs: `scr_home`, `scr_desktop`, `scr_control`, `scr_wifi`, `scr_mqtt`, `scr_settings`, `scr_debug`, `scr_dsp`, `scr_devices`, `scr_siggen`, `scr_support`, `scr_boot_anim`, `scr_keyboard`, `scr_value_edit`, `scr_menu`

**`src/drivers/`:**
- Purpose: Low-level register-level chip drivers (not HAL-managed)
- Key files: `dac_es8311.h/.cpp`, `dac_pcm5102.h/.cpp`, `es8311_regs.h`

**`web_src/`:**
- Purpose: Web UI source files — **always edit here**, never in `src/web_pages.cpp`
- Build: Run `node tools/build_web_assets.js` after any edit to regenerate firmware files
- Key files:
  - `web_src/index.html` — HTML shell (all CSS/JS injected by build tool)
  - `web_src/js/01-core.js` — WebSocket connection management
  - `web_src/js/02-ws-router.js` — Incoming WS message dispatch
  - `web_src/js/03-app-state.js` — Client-side state mirror
  - `web_src/js/04-shared-audio.js` — Shared audio utilities
  - `web_src/js/05-audio-tab.js` — Unified Audio tab (Inputs/Matrix/Outputs/SigGen)
  - `web_src/js/06-peq-overlay.js` — PEQ/crossover/compressor/limiter overlays
  - `web_src/js/15-hal-devices.js` — HAL device management UI
  - `web_src/js/28-init.js` — Application bootstrap
  - `web_src/css/01-variables.css` through `05-responsive.css` — CSS layers
  - `web_src/.eslintrc.json` — ESLint config with 380 globals for concatenated scope

**`test/`:**
- Purpose: Unity unit tests for native platform (no hardware needed)
- Key files:
  - `test/test_mocks/` — Mock headers: `Arduino.h`, `WiFi.h`, `PubSubClient.h`, `Preferences.h`, `FreeRTOS.h`, `LittleFS.h`
  - `test/test_*/` — One directory per module; each has its own `main()` / `setUp()` / `tearDown()`

**`e2e/`:**
- Purpose: Playwright browser E2E tests verifying the web UI against a mock server
- Key files:
  - `e2e/mock-server/server.js` — Express server (port 3000) serving web_src/ via assembler
  - `e2e/mock-server/assembler.js` — Replicates `tools/build_web_assets.js` HTML assembly
  - `e2e/mock-server/ws-state.js` — Deterministic mock state singleton
  - `e2e/mock-server/routes/` — 12 Express route files (auth, wifi, mqtt, settings, hal, dsp, etc.)
  - `e2e/helpers/fixtures.js` — `connectedPage` Playwright fixture (session + WS auth + state)
  - `e2e/helpers/ws-helpers.js` — `buildInitialState()`, `handleCommand()`, binary frame builders
  - `e2e/helpers/selectors.js` — Reusable DOM selectors matching `web_src/index.html` IDs
  - `e2e/tests/*.spec.js` — 26 tests across 19 spec files

**`tools/`:**
- Purpose: Build scripts and static analysis tools (Node.js + Python)
- Key files:
  - `tools/build_web_assets.js` — Assembles and gzip-compresses `web_src/` into firmware C arrays
  - `tools/find_dups.js` — Detects duplicate `let`/`const` declarations across JS files
  - `tools/check_missing_fns.js` — Detects undefined function references across JS files
  - `tools/fix_riscv_toolchain.py` — Pre-build script: patches RISC-V toolchain for ESP32-P4
  - `tools/patch_websockets.py` — Pre-build script: patches WebSockets library for ESP32-P4

**`lib/esp_dsp_lite/`:**
- Purpose: ANSI C fallback DSP for native test builds (no ESP-DSP assembly)
- Contains: `include/`, `src/` with portable biquad IIR, FFT, window functions
- Note: Explicitly `lib_ignore`d on the `esp32-p4` environment (uses pre-built `.a` instead)

## Key File Locations

**Entry Points:**
- `src/main.cpp` — `setup()` (boot) and `loop()` (main event loop)
- `src/audio_pipeline.cpp` — `audio_pipeline_task()` (real-time audio, Core 1)
- `src/mqtt_task.cpp` — `mqtt_task_fn()` (MQTT worker, Core 0)
- `src/gui/gui_manager.cpp` — `gui_task()` (LVGL display, Core 0)

**Configuration:**
- `platformio.ini` — All build environments, pin defines via `-D` flags, library dependencies
- `src/config.h` — Firmware version, compile-time constants, pin fallback defaults
- `partitions_ota.csv` — Flash partition layout (16 MB: boot, ota_0 4 MB, ota_1 4 MB, LittleFS 6 MB)

**Core Logic:**
- `src/app_state.h` — All shared state; dirty flag API is the primary inter-module channel
- `src/app_events.h` — Event bit definitions and `app_events_wait()` / `app_events_signal()` wrappers
- `src/hal/hal_types.h` — All HAL type definitions (enums, structs, constants)
- `src/hal/hal_device.h` — `HalDevice` virtual interface that all drivers implement

**Auto-generated (do not edit):**
- `src/web_pages.cpp` — Inline HTML page as C string literal
- `src/web_pages_gz.cpp` — Gzip-compressed HTML as byte array

## Naming Conventions

**Files:**
- Module pairs: `snake_case_name.h` + `snake_case_name.cpp` (e.g., `smart_sensing.h`)
- HAL drivers: `hal_<chipname>.h/.cpp` (e.g., `hal_pcm5102a.h`)
- HAL support: `hal_<function>.h/.cpp` (e.g., `hal_pipeline_bridge.h`)
- GUI screens: `scr_<screenname>.h/.cpp` (e.g., `scr_home.h`)
- Test modules: `test/test_<modulename>/` directory with `.cpp` file inside
- Web JS: `NN-description.js` where NN is load-order number (e.g., `01-core.js`)
- Web CSS: `NN-concern.css` (e.g., `03-components.css`)

**Classes and Types:**
- HAL device classes: `Hal` prefix + PascalCase (e.g., `HalPcm5102a`, `HalDeviceManager`)
- HAL enums: `HAL_` prefix + SCREAMING_SNAKE (e.g., `HAL_STATE_AVAILABLE`, `HAL_DEV_DAC`)
- App-level enums: PascalCase (e.g., `AppFSMState`, `SensingMode`)
- Struct types: PascalCase (e.g., `AudioOutputSink`, `HalDeviceConfig`)

**Functions:**
- Module API functions: `module_verb_noun()` snake_case (e.g., `audio_pipeline_set_sink()`, `mqtt_task_init()`)
- AppState setters: `set<FieldName>()` camelCase (e.g., `setBacklightOn()`)
- AppState dirty flags: `markXxxDirty()` / `isXxxDirty()` / `clearXxxDirty()` pattern
- WS broadcast functions: `sendXxxState()` or `broadcastXxx()` (e.g., `sendDacState()`)
- HTTP handlers: `handleXxx()` (e.g., `handleWiFiConfig()`)

**Constants:**
- Config constants: `SCREAMING_SNAKE_CASE` `#define` (e.g., `HAL_MAX_DEVICES`, `DSP_MAX_STAGES`)
- Event bits: `EVT_` prefix (e.g., `EVT_OTA`, `EVT_DAC`)
- HAL capabilities: `HAL_CAP_` prefix (e.g., `HAL_CAP_HW_VOLUME`)
- Sink slot indices: `AUDIO_SINK_SLOT_` prefix (e.g., `AUDIO_SINK_SLOT_PRIMARY`)

## Where to Add New Code

**New firmware module (C++ subsystem):**
- Implementation: `src/<module_name>.h` + `src/<module_name>.cpp`
- Register API in: `src/main.cpp` (`setup()` for init, HTTP route registration block, `loop()` for periodic logic)
- Add dirty flag: `src/app_events.h` (new `EVT_` bit) + `src/app_state.h` (dirty flag methods)
- Tests: `test/test_<module_name>/test_<module_name>.cpp` (new directory per module)

**New HAL device driver:**
- Implementation: `src/hal/hal_<chipname>.h/.cpp` (implement `HalDevice` virtual interface)
- Register driver: `src/hal/hal_builtin_devices.cpp` (`hal_register_builtins()`)
- Add DB entry: `src/hal/hal_device_db.cpp` (add to `_builtinEntries[]`)
- Tests: `test/test_hal_<chipname>/`

**New HTTP REST endpoint:**
- Add to: `src/main.cpp` (in the HTTP route registration block inside `setup()`)
- HAL endpoints: `src/hal/hal_api.cpp` + `src/hal/hal_api.h`
- DSP endpoints: `src/dsp_api.cpp`
- Pipeline endpoints: `src/pipeline_api.cpp`
- Mock route: `e2e/mock-server/routes/<group>.js` (matching Express route)
- Fixture: `e2e/fixtures/api-responses/<endpoint>.json`

**New GUI screen:**
- Implementation: `src/gui/screens/scr_<name>.h/.cpp`
- Register: `src/gui/gui_navigation.h/.cpp` (add to screen stack)

**New web UI feature:**
- JS: `web_src/js/<NN>-<description>.js` (pick load-order number)
- CSS: extend existing files in `web_src/css/` or add `web_src/css/06-<concern>.css`
- After editing: run `node tools/build_web_assets.js`
- Add globals: `web_src/.eslintrc.json` (any new top-level `let`/`const`/`function` declarations)
- Tests: new spec in `e2e/tests/<feature>.spec.js`

**Utilities:**
- Shared firmware helpers: `src/utils.h/.cpp`
- Shared GUI helpers: `src/gui/gui_icons.h`, `src/gui/gui_config.h`
- String literals / PROGMEM: `src/strings.h`
- Design tokens for GUI: `src/design_tokens.h`

## Special Directories

**`.planning/codebase/`:**
- Purpose: GSD codebase analysis documents
- Generated: Yes (by GSD map-codebase command)
- Committed: Yes

**`logs/`:**
- Purpose: Build output, test reports, serial captures — keep project root clean
- Generated: Yes (by build/test runs)
- Committed: Generally no (gitignored)

**`.pio/`:**
- Purpose: PlatformIO build artifacts and toolchain cache
- Generated: Yes
- Committed: No (gitignored)

**`e2e/node_modules/`:**
- Purpose: Playwright and Express dependencies
- Generated: Yes (npm install)
- Committed: No (gitignored)

**`e2e/test-results/`:**
- Purpose: Playwright HTML test reports
- Generated: Yes (test runs)
- Committed: No

---

*Structure analysis: 2026-03-07*
