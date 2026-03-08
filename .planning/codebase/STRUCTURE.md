# Codebase Structure

**Analysis Date:** 2026-03-08

## Directory Layout

```
ALX_Nova_Controller_2/
├── src/                    # Firmware source (all C++/C)
│   ├── hal/                # HAL framework (hardware abstraction layer)
│   ├── gui/                # LVGL GUI subsystem
│   │   └── screens/        # Individual TFT screen implementations
│   └── drivers/            # Low-level codec drivers (ES8311, PCM5102A)
├── test/                   # Unity C++ unit tests (native platform)
│   ├── test_mocks/         # Mock headers for Arduino, WiFi, MQTT, NVS
│   └── test_<module>/      # One directory per test module
├── web_src/                # Web UI source (DO NOT edit auto-generated files)
│   ├── css/                # CSS split by concern (01–06 prefix, load order)
│   └── js/                 # JS modules (01–28 prefix, load order)
├── e2e/                    # Playwright browser tests + mock server
│   ├── tests/              # Playwright test specs (19 spec files)
│   ├── helpers/            # Shared fixtures, WS helpers, selectors
│   ├── fixtures/           # Hand-crafted JSON WS messages + API responses
│   │   ├── ws-messages/    # 15 WS broadcast fixture files
│   │   └── api-responses/  # 14 REST response fixture files
│   └── mock-server/        # Express server + route files
│       └── routes/         # 12 route files matching firmware REST API
├── lib/                    # Vendored libraries
│   ├── WebSockets/         # WebSockets@2.7.2 (local copy)
│   └── esp_dsp_lite/       # ANSI C DSP fallback (native tests only)
├── docs/                   # Documentation
│   └── architecture/       # Mermaid architecture diagrams (*.mmd)
├── tools/                  # Build scripts
│   ├── build_web_assets.js # Assembles web_src/ → src/web_pages.cpp + web_pages_gz.cpp
│   ├── find_dups.js        # Detect duplicate JS declarations
│   └── check_missing_fns.js# Detect undefined function references
├── logs/                   # Build output, test reports, serial captures
├── .github/workflows/      # CI/CD (tests.yml, release.yml)
├── .githooks/              # pre-commit hook (find_dups + ESLint)
├── .planning/codebase/     # GSD analysis documents
├── platformio.ini          # PlatformIO build configuration
├── partitions_ota.csv      # Flash partition table (OTA A/B, 4MB each)
├── CLAUDE.md               # Project instructions for Claude Code
└── web_src/.eslintrc.json  # ESLint config for concatenated JS scope
```

---

## Directory Purposes

### `src/` — Firmware Source

**Purpose:** All ESP32-P4 firmware C++/C source files.

**Module naming:** `<feature_name>.h` + `<feature_name>.cpp`. Public API declared in `.h`, implementation in `.cpp`.

**Key files:**
- `src/main.cpp` (1441 lines) — `setup()` + `loop()`. Registers all HTTP routes, initializes all subsystems, owns the main event loop with dirty-flag dispatch
- `src/app_state.h` (553 lines) + `src/app_state.cpp` — AppState singleton, all dirty flags, FSM enum, cross-task volatile flags
- `src/app_events.h` — FreeRTOS event group bit definitions (16 bits used, `EVT_ANY = 0x00FFFFFF`)
- `src/config.h` — Firmware version, pin definitions, task/DSP/pipeline constants (all overridable via `platformio.ini` build flags)

### `src/hal/` — HAL Framework

**Purpose:** Hardware Abstraction Layer. Every managed hardware device has a driver here.

**Key files:**
- `src/hal/hal_types.h` — All HAL enums, structs, constants (`HalDeviceState`, `HalDeviceType`, `HalDeviceDescriptor`, `HalDeviceConfig`, capability bit flags)
- `src/hal/hal_device.h` — `HalDevice` abstract base class
- `src/hal/hal_device_manager.h/.cpp` — Singleton managing up to 24 devices
- `src/hal/hal_pipeline_bridge.h/.cpp` — State callback → audio pipeline source/sink lifecycle
- `src/hal/hal_builtin_devices.h/.cpp` — Driver registry (compatible string → factory function)
- `src/hal/hal_device_db.h/.cpp` — In-memory device database + `/hal_config.json` persistence
- `src/hal/hal_discovery.h/.cpp` — 3-tier discovery (I2C scan → EEPROM → manual)
- `src/hal/hal_api.h/.cpp` — REST endpoints (`GET/POST/PUT/DELETE /api/hal/devices`)
- `src/hal/hal_audio_health_bridge.h/.cpp` — Feeds ADC health status into HAL state machine

**Per-device driver files (examples):**
- `src/hal/hal_pcm5102a.h/.cpp` — PCM5102A I2S DAC driver
- `src/hal/hal_es8311.h/.cpp` — ES8311 codec driver (I2C + I2S)
- `src/hal/hal_pcm1808.h/.cpp` — PCM1808 I2S ADC driver
- `src/hal/hal_siggen.h/.cpp` — `HalSigGen` software ADC (signal generator)
- `src/hal/hal_usb_audio.h/.cpp` — `HalUsbAudio` software ADC (TinyUSB UAC2)
- `src/hal/hal_ns4150b.h/.cpp` — NS4150B class-D amp (GPIO control)
- `src/hal/hal_temp_sensor.h/.cpp` — ESP32-P4 internal chip temperature

### `src/gui/` — GUI Subsystem

**Purpose:** LVGL v9.4 TFT display interface running on Core 0 via `gui_task`. Guarded by `-D GUI_ENABLED`.

**Key files:**
- `src/gui/gui_manager.h/.cpp` — Init, FreeRTOS task, sleep/wake, brightness
- `src/gui/gui_navigation.h/.cpp` — Screen stack with push/pop transitions
- `src/gui/gui_input.h/.cpp` — ISR-driven rotary encoder (Gray code)
- `src/gui/gui_theme.h/.cpp` — Orange accent theme, dark/light mode
- `src/gui/lgfx_config.h` — LovyanGFX ST7735S display configuration
- `src/gui/lv_conf.h` — LVGL compile-time configuration
- `src/gui/gui_icons.h` — Icon definitions

**Screen files in `src/gui/`:**
- `scr_boot_anim.h/.cpp` — Boot animation
- `scr_home.h/.cpp` — Home/status screen (signal level, amplifier state)
- `scr_control.h/.cpp` — Amplifier control
- `scr_desktop.h/.cpp` — Desktop carousel (entry point)
- `scr_wifi.h/.cpp` — WiFi configuration
- `scr_mqtt.h/.cpp` — MQTT settings
- `scr_settings.h/.cpp` — General settings
- `scr_debug.h/.cpp` — Debug information
- `scr_devices.h/.cpp` — HAL device list
- `scr_dsp.h/.cpp` — DSP controls
- `scr_siggen.h/.cpp` — Signal generator
- `scr_menu.h/.cpp` — Menu navigation
- `scr_keyboard.h/.cpp` — On-screen keyboard
- `scr_value_edit.h/.cpp` — Numeric value editor
- `scr_support.h/.cpp` — Support/about screen

### `src/drivers/` — Low-Level Codec Drivers

**Purpose:** Hardware register-level driver code for audio codecs.
- `src/drivers/dac_es8311.h/.cpp` — ES8311 I2C register control
- `src/drivers/dac_pcm5102.h/.cpp` — PCM5102A driver
- `src/drivers/es8311_regs.h` — ES8311 register map

### `test/` — Unit Tests

**Purpose:** Unity C++ tests compiled for the `native` environment (host PC, no hardware needed). One directory per module.

**Test infrastructure:**
- `test/test_mocks/` — Mock headers: `Arduino.h`, `WiFi.h`, `PubSubClient.h`, `Preferences.h`, `mbedtls/`
- Each `test/test_<module>/` contains exactly one `main.cpp` (or `test_<module>.cpp`) with Unity test runner

**Naming:** `test_<module>/` directory maps to `pio test -e native -f test_<module>` command.

### `web_src/` — Web UI Source

**Purpose:** Source files for the embedded web UI. NEVER edit `src/web_pages.cpp` or `src/web_pages_gz.cpp` — they are auto-generated.

**CSS files (concatenated in filename order):**
- `web_src/css/01-variables.css` — CSS custom properties, design tokens
- `web_src/css/02-layout.css` — Page layout, tab structure, sidebar
- `web_src/css/03-components.css` — Button, card, form component styles
- `web_src/css/04-canvas.css` — Audio visualization canvas styles
- `web_src/css/05-responsive.css` — Media queries
- `web_src/css/06-health-dashboard.css` — Health dashboard grid and event log

**JS files (concatenated in filename order, shared scope):**
- `web_src/js/01-core.js` — WebSocket connection management
- `web_src/js/02-ws-router.js` — WS message dispatch router
- `web_src/js/03-app-state.js` — Client-side state management
- `web_src/js/04-shared-audio.js` — Shared audio utilities
- `web_src/js/05-audio-tab.js` — Audio tab: channel strips, 8×8 matrix UI, sub-views
- `web_src/js/06-peq-overlay.js` — Full-screen PEQ/DSP overlays, frequency response graph
- `web_src/js/06-canvas-helpers.js` — Canvas drawing utilities
- `web_src/js/07-ui-core.js` — Tab switching, modal management
- `web_src/js/08-ui-status.js` — Status bar, connection indicator
- `web_src/js/09-audio-viz.js` — VU meter, waveform, spectrum visualization
- `web_src/js/13-signal-gen.js` — Signal generator UI
- `web_src/js/15-hal-devices.js` — HAL device list and management UI
- `web_src/js/15a-yaml-parser.js` — YAML parser for device presets
- `web_src/js/20-wifi-network.js` — WiFi network management UI
- `web_src/js/21-mqtt-settings.js` — MQTT configuration UI
- `web_src/js/22-settings.js` — General settings UI
- `web_src/js/23-firmware-update.js` — OTA firmware update UI
- `web_src/js/24-hardware-stats.js` — Hardware statistics display
- `web_src/js/25-debug-console.js` — Debug console with module filtering
- `web_src/js/26-support.js` — Support/about UI
- `web_src/js/27-auth.js` — Authentication UI
- `web_src/js/27a-health-dashboard.js` — Health dashboard (device grid, error counters, event log)
- `web_src/js/28-init.js` — Page initialization, startup sequence

### `e2e/` — End-to-End Browser Tests

**Purpose:** Playwright tests verifying web UI against a mock Express server. No hardware needed.

- `e2e/tests/` — 19 Playwright spec files (26 tests total)
- `e2e/helpers/fixtures.js` — `connectedPage` fixture: session cookie + WS auth + initial state
- `e2e/helpers/ws-helpers.js` — `buildInitialState()`, `handleCommand()`, binary frame builders
- `e2e/helpers/selectors.js` — Reusable DOM selectors matching `web_src/index.html` IDs
- `e2e/mock-server/server.js` — Express server (port 3000) assembling HTML from `web_src/`
- `e2e/mock-server/assembler.js` — Replicates `tools/build_web_assets.js` HTML assembly
- `e2e/mock-server/ws-state.js` — Deterministic mock state singleton, reset between tests
- `e2e/mock-server/routes/` — 12 route files matching firmware REST API

---

## Key File Locations

**Entry Points:**
- `src/main.cpp` — `setup()` and `loop()` (Arduino entry points)

**Configuration:**
- `src/config.h` — All firmware constants, pin defaults, version string (`FIRMWARE_VERSION`)
- `platformio.ini` — Build flags, board, libraries, environments
- `partitions_ota.csv` — Flash layout (OTA A/B partitions)
- `web_src/.eslintrc.json` — ESLint config (380 globals for shared JS scope)

**Core State:**
- `src/app_state.h` — AppState class, dirty flags, FSM states, all subsystem state
- `src/app_events.h` — FreeRTOS event bit definitions

**Audio Pipeline:**
- `src/audio_pipeline.h/.cpp` — Main pipeline task and routing logic
- `src/audio_input_source.h` — `AudioInputSource` struct (callback-based source interface)
- `src/audio_output_sink.h` — `AudioOutputSink` struct (callback-based sink interface)
- `src/i2s_audio.h/.cpp` — I2S ADC init, analysis structs (`AudioAnalysis`, `AudioDiagnostics`)

**HAL System:**
- `src/hal/hal_types.h` — All types, enums, structs (start here for HAL work)
- `src/hal/hal_device.h` — Abstract base class for all HAL drivers
- `src/hal/hal_device_manager.h` — Singleton manager (registration, lookup, lifecycle)

**Web Assets (auto-generated — never edit manually):**
- `src/web_pages.cpp` — Uncompressed HTML string
- `src/web_pages_gz.cpp` — Gzip-compressed HTML bytes

**Testing:**
- `test/test_mocks/` — Mock headers for all Arduino/ESP32 API dependencies

---

## Naming Conventions

**Files:**
- Firmware modules: `snake_case.h` + `snake_case.cpp` (e.g., `smart_sensing.h`, `audio_pipeline.cpp`)
- HAL driver files: `hal_<device>.h/.cpp` (e.g., `hal_pcm5102a.h`, `hal_es8311.cpp`)
- GUI screens: `scr_<name>.h/.cpp` (e.g., `scr_home.h`, `scr_settings.cpp`)
- Test modules: `test_<module>/` directory (e.g., `test/test_hal_bridge/`)
- Web JS: `<NN>[-<letter>]-<name>.js` with 2-digit load-order prefix (e.g., `05-audio-tab.js`, `27a-health-dashboard.js`)
- Web CSS: `<NN>-<name>.css` with 2-digit load-order prefix (e.g., `01-variables.css`)

**Classes:**
- HAL drivers: `Hal<DeviceName>` (e.g., `HalPcm5102a`, `HalEs8311`, `HalSigGen`)
- Singleton managers: `HalDeviceManager`, `AppState`
- GUI screens: `scr_<name>_create()` / `scr_<name>_destroy()` free functions

**Functions:**
- Module functions: `<module>_<verb>()` snake_case (e.g., `buzzer_play()`, `siggen_apply_params()`)
- WebSocket broadcast functions: `send<Name>State()` (e.g., `sendDacState()`, `sendAudioGraphState()`)
- HTTP handlers: `handle<Name>()` (e.g., `handleLogin()`, `handleSmartSensingUpdate()`)
- HAL API: `hal_<verb>_<noun>()` (e.g., `hal_pipeline_sync()`, `hal_register_builtins()`)

**Constants:**
- Build flags / config defines: `UPPER_CASE` (e.g., `AUDIO_PIPELINE_MAX_INPUTS`, `HAL_MAX_DEVICES`)
- Event bits: `EVT_<NAME>` (e.g., `EVT_OTA`, `EVT_DAC`)
- HAL capabilities: `HAL_CAP_<NAME>` (e.g., `HAL_CAP_DAC_PATH`, `HAL_CAP_ADC_PATH`)
- HAL states: `HAL_STATE_<NAME>` (e.g., `HAL_STATE_AVAILABLE`)
- HAL priorities: `HAL_PRIORITY_<NAME>` (e.g., `HAL_PRIORITY_HARDWARE`)

---

## Where to Add New Code

### New Firmware Module

1. Create `src/<module_name>.h` + `src/<module_name>.cpp`
2. Declare public API in `.h`; implement in `.cpp`
3. Include in `src/main.cpp`; initialize in `setup()` and call from `loop()` if periodic
4. Create `test/test_<module_name>/` with a Unity test file
5. If module has state → add dirty flag to `AppState` and event bit to `app_events.h`
6. Add `LOG_<LEVEL>("[ModuleName] ...")` calls using `debug_serial.h` macros

### New HAL Device Driver

1. Create `src/hal/hal_<device>.h` + `src/hal/hal_<device>.cpp` extending `HalDevice`
2. Implement all virtual methods: `probe()`, `init()`, `deinit()`, `dumpConfig()`, `healthCheck()`
3. If ADC: implement `getInputSource()` returning `AudioInputSource*`
4. Register in `src/hal/hal_builtin_devices.cpp` via driver registry (compatible string → factory)
5. Instantiate and `registerDevice()` in `src/main.cpp` `setup()` under `#ifdef DAC_ENABLED`
6. Create `test/test_hal_<device>/` test module
7. Add compatible string entry to `src/hal/hal_device_db.cpp` builtins table

### New Web UI Feature

1. Add HTML elements to `web_src/index.html` (body only — no inline CSS/JS)
2. Add CSS to appropriate `web_src/css/0N-*.css` file by concern
3. Add JS to new or existing `web_src/js/NN-*.js` file (maintain load order)
4. If new WS message type: add fixture to `e2e/fixtures/ws-messages/`, update `e2e/helpers/ws-helpers.js`
5. If new REST endpoint: add route to matching `e2e/mock-server/routes/*.js`
6. Run `node tools/build_web_assets.js` to regenerate `src/web_pages.cpp` and `src/web_pages_gz.cpp`
7. Add/update Playwright test in `e2e/tests/`
8. Run `node tools/find_dups.js` to verify no duplicate JS declarations

### New REST API Endpoint

1. Register handler in `src/main.cpp` `setup()` (or in appropriate `register*Endpoints()` function)
2. Implement handler function in relevant module (e.g., `src/settings_manager.cpp`)
3. Add matching route in `e2e/mock-server/routes/` for E2E test coverage
4. Add API response fixture in `e2e/fixtures/api-responses/`

### New Utility Function

- Shared firmware helpers: `src/utils.h/.cpp`
- HAL-specific helpers: inline in relevant `src/hal/*.cpp` or add to `src/hal/hal_types.h`

---

## Special Directories

**`.planning/codebase/`:**
- Purpose: GSD analysis documents (ARCHITECTURE.md, STRUCTURE.md, STACK.md, etc.)
- Generated: Yes (by GSD map-codebase)
- Committed: Yes

**`logs/`:**
- Purpose: Build output, test reports, serial captures — keep project root clean
- Generated: Yes
- Committed: Partial (`.gitignore` excludes most log files)

**`src/web_pages.cpp` and `src/web_pages_gz.cpp`:**
- Purpose: Auto-generated from `web_src/` by `node tools/build_web_assets.js`
- Generated: Yes — NEVER edit manually
- Committed: Yes (checked in as generated artifacts)

**`lib/esp_dsp_lite/`:**
- Purpose: ANSI C fallback for ESP-DSP functions used in native unit tests
- Generated: No
- Committed: Yes (`lib_ignore = esp_dsp_lite` in `[env:esp32-p4]` so it is excluded from firmware builds)

**`.pio/`:**
- Purpose: PlatformIO build artifacts
- Generated: Yes
- Committed: No (`.gitignore`d)

---

## Module Boundaries and Include Rules

- `src/main.cpp` is the only file that includes everything — all other modules include only what they need
- HAL modules include `hal_types.h` (shared types) and `hal_device.h` (base class) — not each other
- Audio pipeline (`audio_pipeline.cpp`) reads `volatile _ready` on `HalDevice` directly for lock-free hot-path access — no virtual dispatch in audio task
- `app_state.h` must not include heavy Arduino library headers in `NATIVE_TEST` builds — stub classes are provided inline
- Web page sources in `web_src/` are completely independent of firmware C++ — concatenated by `tools/build_web_assets.js` at build time

---

*Structure analysis: 2026-03-08*
