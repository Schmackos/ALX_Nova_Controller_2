# Codebase Structure

**Analysis Date:** 2026-03-08

## Directory Layout

```
ALX_Nova_Controller_2/
├── src/                    # Firmware source (all .cpp/.h modules)
│   ├── hal/                # HAL framework (device manager, drivers, bridge, API)
│   │   └── screens/        # (not here — screens are under src/gui/screens/)
│   ├── gui/                # LVGL GUI modules (guarded by GUI_ENABLED)
│   │   └── screens/        # Per-screen .cpp/.h pairs
│   └── drivers/            # Low-level DAC chip drivers (ES8311, PCM5102A)
├── test/                   # Unit tests (Unity, native platform)
│   ├── test_mocks/         # Mock implementations of Arduino/WiFi/MQTT/FreeRTOS APIs
│   └── test_<module>/      # One directory per test module
├── lib/                    # Vendored/local libraries
│   ├── WebSockets/         # Vendored WebSockets library (not from lib_deps registry)
│   └── esp_dsp_lite/       # ANSI C DSP fallback for native tests (lib_ignore on ESP32)
├── web_src/                # Web UI source (edit here, NOT src/web_pages.cpp)
│   ├── css/                # CSS split: 01-05-*.css by concern
│   └── js/                 # JS modules: 01-28-*.js in load order
├── e2e/                    # Playwright browser tests + mock server
│   ├── mock-server/        # Express mock (routes/, ws-state.js, assembler.js)
│   ├── tests/              # 19 Playwright spec files
│   ├── helpers/            # fixtures.js, ws-helpers.js, selectors.js
│   └── fixtures/           # ws-messages/*.json, api-responses/*.json
├── docs/                   # Architecture diagrams and planning docs
│   └── architecture/       # 10 Mermaid .mmd diagrams
├── tools/                  # Build scripts and static analysis tools
├── .github/                # GitHub Actions CI workflows
├── .githooks/              # Pre-commit hooks (find_dups, check_missing_fns, ESLint)
├── .planning/              # GSD planning documents
│   └── codebase/           # Codebase analysis docs (this directory)
├── logs/                   # Build output, test reports, serial captures
├── include/                # Global headers (empty in this project)
├── partitions_ota.csv      # OTA partition table (4MB ota_0 + 4MB ota_1)
├── platformio.ini          # PlatformIO build configuration (3 envs: esp32-p4, native, p4_hosted_update)
└── CLAUDE.md               # Project instructions for Claude Code
```

## Directory Purposes

**`src/`:**
- Purpose: All firmware C/C++ source. Each subsystem is a `.h`/`.cpp` pair.
- Key files: `main.cpp` (setup/loop, route registration), `app_state.h` (singleton state), `app_events.h` (FreeRTOS event bits), `config.h` (pin defs, constants, firmware version).

**`src/hal/`:**
- Purpose: Hardware Abstraction Layer — device manager, driver implementations, discovery, persistence, REST API.
- Key files:
  - `hal_types.h` — all enums (`HalDeviceState`, `HalDeviceType`, capability flags) and structs (`HalDeviceDescriptor`, `HalDeviceConfig`)
  - `hal_device.h` — `HalDevice` abstract base class with ESPHome-style lifecycle
  - `hal_device_manager.h/.cpp` — singleton managing up to 24 devices (`HAL_MAX_DEVICES=24`)
  - `hal_pipeline_bridge.h/.cpp` — connects HAL lifecycle to audio pipeline slot/lane assignments
  - `hal_builtin_devices.h/.cpp` — driver factory registry (compatible string → constructor)
  - `hal_device_db.h/.cpp` — in-memory device database + LittleFS persistence
  - `hal_discovery.h/.cpp` — 3-tier I2C scan / EEPROM probe / manual config discovery
  - `hal_api.h/.cpp` — REST endpoints for HAL CRUD
  - `hal_audio_health_bridge.h/.cpp` — audio ADC health → HAL state transitions with flap guard
  - Driver files: `hal_pcm5102a.h/.cpp`, `hal_es8311.h/.cpp`, `hal_pcm1808.h/.cpp`, `hal_ns4150b.h/.cpp`, `hal_temp_sensor.h/.cpp`, `hal_siggen.h/.cpp`, `hal_usb_audio.h/.cpp`, `hal_mcp4725.h/.cpp`, `hal_dac_adapter.h/.cpp`, `hal_dsp_bridge.h/.cpp`, `hal_signal_gen.h/.cpp`
  - Peripheral drivers: `hal_display.h/.cpp`, `hal_encoder.h/.cpp`, `hal_buzzer.h/.cpp`, `hal_led.h/.cpp`, `hal_relay.h/.cpp`, `hal_button.h/.cpp`
  - `hal_custom_device.h/.cpp` — user-configured devices without EEPROM
  - `hal_settings.h/.cpp` — HAL config persistence to `/hal_config.json`
  - `hal_eeprom_v3.h/.cpp` — EEPROM v3 format reader (ALXD header, device ID, capabilities)
  - `hal_i2s_bridge.h/.cpp` — I2S pin configuration bridge

**`src/gui/`:**
- Purpose: LVGL GUI system on ST7735S 128×160 TFT. All files guarded by `GUI_ENABLED`.
- Key files: `gui_manager.h/.cpp` (task, sleep/wake), `gui_input.h/.cpp` (Gray code rotary encoder ISR), `gui_theme.h/.cpp` (orange accent), `gui_navigation.h/.cpp` (screen stack), `lgfx_config.h` (LovyanGFX display config), `lv_conf.h` (LVGL config), `gui_config.h` (layout constants), `gui_icons.h` (MDI SVG paths).
- Screens: `src/gui/screens/scr_boot_anim`, `scr_desktop`, `scr_home`, `scr_control`, `scr_wifi`, `scr_mqtt`, `scr_settings`, `scr_debug`, `scr_devices`, `scr_dsp`, `scr_siggen`, `scr_support`, `scr_keyboard`, `scr_value_edit`, `scr_menu`.

**`src/drivers/`:**
- Purpose: Low-level hardware register drivers used by HAL driver classes.
- Key files: `dac_es8311.h/.cpp` (ES8311 I2C register map), `dac_pcm5102.h/.cpp` (PCM5102A config), `es8311_regs.h` (register definitions).

**`test/`:**
- Purpose: Unity C++ unit tests for the native platform (no hardware required).
- Key directory: `test/test_mocks/` — mock implementations of `Arduino.h`, `WiFi.h`, `PubSubClient.h`, `Preferences.h`, `LittleFS.h`, `esp_task_wdt.h`, FreeRTOS primitives.
- Each test module: one subdirectory (`test_<module>/`) with a `test_<module>.cpp` file containing `setUp()`, `tearDown()`, and test functions.

**`lib/`:**
- `lib/WebSockets/` — vendored WebSockets library (not from registry; fixes native build compatibility).
- `lib/esp_dsp_lite/` — ANSI C DSP fallback for native tests. Ignored on ESP32 via `lib_ignore = esp_dsp_lite` in `platformio.ini`.

**`web_src/`:**
- Purpose: Web UI source. ALWAYS edit here — `src/web_pages.cpp` and `src/web_pages_gz.cpp` are auto-generated.
- After editing: run `node tools/build_web_assets.js` to regenerate compressed assets.
- `web_src/index.html` — HTML shell (body content only, no inline CSS/JS).
- `web_src/css/` — CSS split into 5 files by concern (variables, layout, components, canvas, responsive).
- `web_src/js/` — JS modules in filename-order load sequence (01–28), concatenated into single `<script>` block. All files share the same global scope.

**`e2e/`:**
- Purpose: Playwright browser tests running against a mock Express server (no real hardware).
- `e2e/mock-server/server.js` — Express server on port 3000.
- `e2e/mock-server/routes/` — 12 route files matching firmware REST API.
- `e2e/fixtures/ws-messages/` — 15 hand-crafted WS broadcast JSON fixtures.
- `e2e/fixtures/api-responses/` — 14 deterministic REST response fixtures.
- `e2e/helpers/selectors.js` — reusable DOM selectors (update here when element IDs change).

**`docs/architecture/`:**
- 10 Mermaid `.mmd` diagrams: `system-architecture`, `hal-lifecycle`, `hal-pipeline-bridge`, `boot-sequence`, `event-architecture`, `sink-dispatch`, `test-infrastructure`, `ci-quality-gates`, `e2e-test-flow`, `test-coverage-map`.

**`tools/`:**
- `build_web_assets.js` — assembles and gzip-compresses web UI into `src/web_pages.cpp`/`src/web_pages_gz.cpp`.
- `find_dups.js` — detects duplicate `let`/`const` across JS files.
- `check_missing_fns.js` — detects undefined function references in JS.
- `fix_riscv_toolchain.py` — PlatformIO pre-script for RISC-V toolchain path.

## Key File Locations

**Entry Points:**
- `src/main.cpp` — `setup()` (firmware init, route registration) and `loop()` (event dispatch, dirty-flag handling).
- `src/app_state.h` — `AppState` class definition with all state fields and dirty-flag accessors.
- `src/app_events.h` — FreeRTOS event bit definitions (`EVT_OTA` through `EVT_CHANNEL_MAP`).

**Configuration:**
- `src/config.h` — firmware version (`FIRMWARE_VERSION`), pin constants, task stack sizes/priorities, system constants.
- `platformio.ini` — build flags (pin overrides, feature flags `DSP_ENABLED`/`DAC_ENABLED`/`GUI_ENABLED`/`USB_AUDIO_ENABLED`), library deps, upload port COM8.
- `partitions_ota.csv` — flash partition layout (4MB ota_0 + 4MB ota_1).

**Core Logic:**
- `src/audio_pipeline.h/.cpp` — real-time audio path, source/sink registration, 16×16 matrix.
- `src/audio_input_source.h` — `AudioInputSource` C struct (function pointer interface).
- `src/audio_output_sink.h` — `AudioOutputSink` C struct (function pointer interface).
- `src/hal/hal_types.h` — all HAL enums, constants, and structs.
- `src/hal/hal_device.h` — `HalDevice` abstract base class.
- `src/mqtt_handler.cpp`, `src/mqtt_publish.cpp`, `src/mqtt_ha_discovery.cpp` — MQTT 3-file split.
- `src/websocket_handler.h/.cpp` — WS event handler, all `send*()` broadcast functions.
- `src/smart_sensing.h/.cpp` — signal detection, auto-off timer, amplifier relay control.
- `src/settings_manager.h/.cpp` — JSON settings persistence (primary `/config.json`).
- `src/auth_handler.h/.cpp` — PBKDF2-SHA256 password auth, session tokens, WS token pool.

**Testing:**
- `test/test_mocks/` — all mock headers.
- Each test module: `test/<module_name>/test_<module_name>.cpp`.
- `e2e/tests/` — 19 Playwright spec files.
- `e2e/helpers/fixtures.js` — `connectedPage` Playwright fixture (auth + WS + initial state).

**Auto-Generated (Do Not Edit):**
- `src/web_pages.cpp` — uncompressed HTML page (generated by `tools/build_web_assets.js`).
- `src/web_pages_gz.cpp` — gzip-compressed page (generated by `tools/build_web_assets.js`).

## Naming Conventions

**Files:**
- Module pair: `<subsystem_name>.h` + `<subsystem_name>.cpp` (e.g., `smart_sensing.h`, `smart_sensing.cpp`).
- HAL drivers: `hal_<device>.h/.cpp` (e.g., `hal_pcm5102a.h/.cpp`).
- GUI screens: `scr_<name>.h/.cpp` (e.g., `scr_home.h/.cpp`).
- Test modules: `test/test_<module>/test_<module>.cpp`.
- Web JS modules: `NN-<description>.js` (two-digit prefix controls concatenation order).
- Web CSS files: `NN-<description>.css` (two-digit prefix controls load order).

**Symbols:**
- C++ classes: `PascalCase` (e.g., `AppState`, `HalDeviceManager`, `HalPcm5102a`).
- C functions / module APIs: `snake_case` prefixed by module (e.g., `audio_pipeline_set_sink()`, `hal_discover_devices()`).
- AppState dirty-flag methods: `mark*Dirty()`, `is*Dirty()`, `clear*Dirty()`.
- Constants/macros: `UPPER_SNAKE_CASE` (e.g., `HAL_MAX_DEVICES`, `EVT_OTA`, `AUDIO_PIPELINE_MAX_INPUTS`).
- Enum values: `UPPER_SNAKE_CASE` (e.g., `HAL_STATE_AVAILABLE`, `DSP_BIQUAD_PEQ`).

**Directories:**
- Flat under `src/` except `hal/` and `gui/` which are subsystem namespaces.
- Test modules mirror the module name: `test/test_smart_sensing/` for `src/smart_sensing.h`.

## Where to Add New Code

**New Firmware Module:**
- Implementation: `src/<module_name>.h` + `src/<module_name>.cpp`.
- Test module: `test/test_<module_name>/test_<module_name>.cpp`.
- Include in `src/main.cpp` under the appropriate `#ifdef` guard if needed.
- Add `LOG_*` calls with `[ModuleName]` prefix following the debug serial convention.

**New HAL Driver:**
- Implementation: `src/hal/hal_<device>.h/.cpp` subclassing `HalDevice`.
- Register factory in `src/hal/hal_builtin_devices.cpp` (`hal_register_builtins()`).
- Add DB entry in `src/hal/hal_device_db.cpp` if device should appear in presets.
- Add test module: `test/test_hal_<device>/test_hal_<device>.cpp`.

**New GUI Screen:**
- Implementation: `src/gui/screens/scr_<name>.h/.cpp`.
- Register in `src/gui/gui_navigation.cpp` screen stack.
- Add a desktop icon tile in `src/gui/screens/scr_desktop.cpp` if user-accessible.

**New Web UI Feature:**
- Edit source files in `web_src/js/` (create new `NN-<feature>.js` or extend existing module).
- Run `node tools/build_web_assets.js` after every web_src change before building firmware.
- Add Playwright test in `e2e/tests/<feature>.spec.js`.
- Update `e2e/helpers/selectors.js` with new DOM selectors.
- Add globals to `web_src/.eslintrc.json` for any new top-level `let`/`const` declarations.

**New REST API Endpoint:**
- Register route in `src/main.cpp` (or in a `register*ApiEndpoints()` function called from `main.cpp`).
- Add mock route in `e2e/mock-server/routes/` matching the firmware endpoint.
- Add fixture in `e2e/fixtures/api-responses/` for deterministic mock responses.

**New WebSocket Message Type:**
- Add `send*()` function to `src/websocket_handler.h/.cpp`.
- Call it from the dirty-flag dispatch block in `loop()`.
- Add fixture in `e2e/fixtures/ws-messages/`.
- Update `e2e/helpers/ws-helpers.js` (`buildInitialState()`, `handleCommand()`).
- Update `e2e/mock-server/ws-state.js` for new state fields.

**New FreeRTOS Task:**
- Pin to Core 0 only — Core 1 is reserved for `loopTask` + `audio_pipeline_task`.
- Use `xTaskCreatePinnedToCore()` with an entry from `TASK_STACK_SIZE_*` / `TASK_PRIORITY_*` in `src/config.h`.
- Register with TWDT: `esp_task_wdt_add(NULL)` at task entry, `esp_task_wdt_reset()` in each iteration.

**New AppState Field:**
- Add field to `AppState` in `src/app_state.h`.
- If the field needs WS broadcast, add a dirty-flag trio (`mark*Dirty()` / `is*Dirty()` / `clear*Dirty()`) and a new `EVT_*` bit in `src/app_events.h` (bits 0-15 assigned, bits 16-23 spare).
- Add a corresponding `send*()` call in the dirty-flag dispatch block in `loop()`.
- Add MQTT publish if appropriate in `src/mqtt_publish.cpp`.

## Special Directories

**`.planning/codebase/`:**
- Purpose: GSD codebase analysis documents consumed by `/gsd:plan-phase` and `/gsd:execute-phase`.
- Generated: Yes (by `/gsd:map-codebase`).
- Committed: Yes.

**`.pio/`:**
- Purpose: PlatformIO build cache and artifacts.
- Generated: Yes.
- Committed: No (in `.gitignore`).

**`logs/`:**
- Purpose: Build output, test reports, serial captures. Keeps the project root clean.
- Generated: Yes.
- Committed: No (captured content only).

**`lib/WebSockets/`:**
- Purpose: Vendored WebSockets library — not from the PlatformIO registry. Provides fix for native build compatibility.
- Generated: No (manually vendored).
- Committed: Yes.

**`lib/esp_dsp_lite/`:**
- Purpose: ANSI C DSP fallback for native test builds. Ignored on ESP32 (`lib_ignore = esp_dsp_lite` in `platformio.ini`).
- Generated: No.
- Committed: Yes.

**`src/web_pages.cpp` and `src/web_pages_gz.cpp`:**
- Purpose: Auto-generated C arrays of the assembled and gzip-compressed web UI.
- Generated: Yes (by `node tools/build_web_assets.js`).
- Committed: Yes (so firmware builds without requiring the Node.js toolchain).
- **Never edit directly.** Always edit `web_src/` files and regenerate.

---

*Structure analysis: 2026-03-08*
