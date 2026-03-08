# Coding Conventions

**Analysis Date:** 2026-03-08

## Naming Patterns

**Files (C++):**
- Header + implementation pairs use the same snake_case base name: `hal_device_manager.h` / `hal_device_manager.cpp`
- HAL driver files are prefixed `hal_`: `hal_pcm5102a.h`, `hal_es8311.cpp`
- Module headers use the module name directly: `smart_sensing.h`, `mqtt_handler.h`
- Use `#pragma once` in all `.h` files (preferred over include guards, though legacy files use `#ifndef`)

**Files (JavaScript web front-end):**
- All JS files in `web_src/js/` are numbered with a two-digit prefix to control concatenation order: `01-core.js`, `06-peq-overlay.js`, `27a-health-dashboard.js`
- Numbered by load-order concern (01-09 = core/WS/state, 10-19 = feature tabs, 20-28 = settings/auth/init)
- Do NOT add files without choosing an appropriate numeric prefix

**Functions (C++):**
- Free functions use `snake_case`: `detectSignal()`, `handleSmartSensingGet()`, `mqttPublishPendingState()`
- Class methods use `camelCase`: `getDevice()`, `registerDevice()`, `healthCheck()`
- HAL lifecycle methods follow ESPHome naming: `probe()`, `init()`, `deinit()`, `healthCheck()`, `dumpConfig()`
- REST API handlers are named `handle<Feature><Method>`: `handleSmartSensingGet()`, `handleWifiScan()`
- Private members use `_camelCase` prefix: `_descriptor`, `_state`, `_ready`, `_initPriority`

**Functions (JavaScript):**
- `camelCase` throughout: `apiFetch()`, `buildInitialState()`, `routeWsMessage()`
- Event handlers prefixed with action verb: `handleCommand()`, `handleBinaryMessage()`, `handleHalDeviceState()`
- Renderers prefixed `render`: `renderHalDevices()`, `renderHealthDashboard()`

**Variables:**
- Local variables: `camelCase` in both C++ and JS
- Module-level file statics in C++: `snake_case` with `_` prefix (private) or no prefix (internal): `_smoothedAudioLevel`, `prevBroadcastMode`, `pendingConnection`
- Constants: `UPPER_SNAKE_CASE` for `#define` macros and `const` globals: `FIRMWARE_VERSION`, `DEFAULT_MQTT_PORT`, `WS_MIN_RECONNECT_DELAY`

**Types (C++):**
- Enums: `PascalCase` for the enum name, `UPPER_SNAKE_CASE` for values: `enum HalDeviceState`, `HAL_STATE_AVAILABLE`
- Structs: `PascalCase`: `HalDeviceConfig`, `WiFiConnectionRequest`, `HalBusRef`
- Classes: `PascalCase`: `HalDeviceManager`, `HalPcm5102a`, `DebugSerial`
- Typedefs: `PascalCase`: `HalStateChangeCb`, `HalDeviceCallback`

**Enums with `: type` suffix (scoped):**
- Use typed enums where integer width matters: `enum HalDeviceState : uint8_t`, `enum FftWindowType : uint8_t`

## Code Style

**Formatting:**
- No automated formatter configured (no `.clang-format`, no `.prettierrc` for C++)
- 4-space indentation in C++ source (tabs not used)
- Brace style: K&R variant — opening brace on same line as `if`/`for`, Allman for class/function bodies is mixed
- Max line length is not enforced; long lines appear in JSON-building blocks
- Blank lines separate logical sections; section headers use `// ===== Title =====` banners

**Linting (JavaScript only):**
- ESLint configured at `web_src/.eslintrc.json`
- Rules enforced: `no-undef` (error), `no-redeclare` (error), `eqeqeq` ("smart")
- `ecmaVersion: 2020`, `sourceType: "script"` (concatenated scope, not ES modules)
- All ~380 cross-file globals declared in the globals section of `.eslintrc.json`
- Run: `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`

**C++ static analysis:**
- cppcheck enforced in CI: `--enable=warning,style,performance --std=c++11`
- Excludes `src/gui/` (LVGL-heavy, too many third-party patterns)
- Run: `cppcheck --suppress=missingInclude --suppress=unusedFunction --error-exitcode=1 -i src/gui/ src/`

## Import / Include Organization

**C++ include order (observed in source files like `smart_sensing.cpp`):**
1. Module's own header first: `#include "smart_sensing.h"`
2. Application headers (quoted): `#include "app_state.h"`, `#include "debug_serial.h"`
3. Third-party library headers (angle brackets): `#include <ArduinoJson.h>`, `#include <LittleFS.h>`
4. Standard library headers (angle brackets): `#include <cmath>`

**Conditional compilation guards:**
- Always wrap hardware-specific code in feature guards:
  ```cpp
  #ifdef DAC_ENABLED
  // DAC code here
  #endif
  #ifdef GUI_ENABLED
  // LVGL code here
  #endif
  #ifdef USB_AUDIO_ENABLED
  // TinyUSB code here
  #endif
  ```
- Native test stubs: `#ifdef NATIVE_TEST` / `#else` / `#endif` pattern to switch between mock and real headers

**Path aliases:**
- None configured. Headers use relative paths from `src/`: `#include "hal/hal_device.h"`
- Test files use `../../src/` relative paths: `#include "../../src/hal/hal_types.h"`

## Error Handling

**C++ patterns:**
- HAL lifecycle methods return `HalInitResult` (defined in `src/hal/hal_init_result.h`) — use `hal_init_ok()` for success and `hal_init_fail(DIAG_CODE, "reason")` for failure. Never return raw `bool` from `init()`
- REST handlers build JSON responses: `doc["success"] = false; doc["error"] = "message";` — always include a `success` field
- Dangerous/fallible operations guarded with early return:
  ```cpp
  if (!client || broker.empty()) return false;
  ```
- No C++ exceptions used; ESP32 Arduino framework does not use exceptions

**JavaScript patterns:**
- `apiFetch()` wrapper handles `401` automatically (redirects to `/login`)
- Async errors caught with `try/catch` or `.catch()` chained on promises
- UI errors shown via `showToast('message', 'error')` (defined in `07-ui-core.js`)

## Logging

**Framework:** `DebugSerial` class in `src/debug_serial.h`, accessed via convenience macros

**Macros:**
```cpp
#define LOG_D(fmt, ...) DebugOut.debug(fmt, ##__VA_ARGS__)   // Debug: high-frequency detail
#define LOG_I(fmt, ...) DebugOut.info(fmt, ##__VA_ARGS__)    // Info: state transitions, start/stop
#define LOG_W(fmt, ...) DebugOut.warn(fmt, ##__VA_ARGS__)    // Warn: degraded state
#define LOG_E(fmt, ...) DebugOut.error(fmt, ##__VA_ARGS__)   // Error: failures, init errors
```

**Module prefix convention (mandatory):**
Every `LOG_*` call must begin with `[ModuleName]` to enable frontend category filtering:
```cpp
LOG_I("[HAL:PCM5102A] Initializing (sr=%luHz bits=%u xsmt=%d)", sampleRate, bits, paPin);
LOG_I("[Sensing] Mode changed: %s → %s", prevMode, newMode);
LOG_W("[WiFi] Disconnected, reason=%d", reason);
LOG_E("[MQTT] Publish failed: %s", topic);
```

**Module prefix table:**

| Module | Prefix |
|--------|--------|
| `smart_sensing` | `[Sensing]` |
| `i2s_audio` | `[Audio]` |
| `signal_generator` | `[SigGen]` |
| `buzzer_handler` | `[Buzzer]` |
| `wifi_manager` | `[WiFi]` |
| `mqtt_handler` | `[MQTT]` |
| `ota_updater` | `[OTA]` |
| `settings_manager` | `[Settings]` |
| `usb_audio` | `[USB Audio]` |
| HAL modules | `[HAL]`, `[HAL Discovery]`, `[HAL DB]`, `[HAL API]` |
| HAL drivers | `[HAL:<DeviceName>]` e.g. `[HAL:PCM5102A]` |
| `output_dsp` | `[OutputDSP]` |
| `gui_*` | `[GUI]` |

**When to log:**
- `LOG_I`: State transitions (start/stop, connect/disconnect, mode changes, health status changes). Use a `static prev` variable to avoid repeated identical logs
- `LOG_D`: High-frequency operational detail (per-pattern steps, parameter snapshots during active operation)
- `LOG_W`: Degraded but recoverable conditions
- `LOG_E`: Failures, init errors, unexpected states
- **Never log inside ISR paths or `audio_pipeline_task`** — `Serial.print` blocks when UART TX buffer fills, causing audio dropouts. Use dirty flags; main loop calls `audio_periodic_dump()` for output

## AppState and Dirty Flags

**Access pattern:**
- All application state lives in the `AppState` singleton: `AppState::getInstance()` or the `appState` macro
- New code writes `appState.memberName` directly (not the legacy `#define` macro aliases)
- After mutating state that needs WebSocket or MQTT propagation, call the matching event signal:
  ```cpp
  appState.someValue = newValue;
  app_events_signal(EVT_SOME_EVENT);  // wakes main loop immediately
  ```
- The `mqtt_task` and main loop independently consume dirty flags — do not call `publishMqtt*()` from any callback; set flags instead

**Thread safety rules:**
- `appState._mqttReconfigPending` and `appState._pendingApToggle` are `volatile` coordination flags for cross-core signalling
- For DAC toggle requests use validated setters only: `appState.requestDacToggle(int8_t)` and `appState.requestEs8311Toggle(int8_t)` — only accept -1, 0, 1
- Direct writes to `_pendingDacToggle` / `_pendingEs8311Toggle` are unsafe

## Module Design

**Header guards:**
- Prefer `#pragma once` (all HAL headers use it)
- Legacy modules use `#ifndef MODULE_NAME_H` guards

**Singleton pattern (C++):**
- Meyers singleton: `static ClassName& instance() { static ClassName inst; return inst; }`
- Used in: `AppState`, `HalDeviceManager`
- Delete copy/move constructors:
  ```cpp
  AppState(const AppState&) = delete;
  AppState& operator=(const AppState&) = delete;
  ```

**File splitting at ~1000 lines:**
- When a module grows beyond ~1000 lines, split by responsibility
- Example: `mqtt_handler.cpp` (~1120 lines) + `mqtt_publish.cpp` (publish functions + change-detection statics) + `mqtt_ha_discovery.cpp` (HA discovery)
- The split file keeps the same header; implementation is partitioned logically

**Feature flags guard entire modules:**
- HAL device implementations start with `#ifdef DAC_ENABLED` or `#ifdef USB_AUDIO_ENABLED`
- GUI code is inside `#ifdef GUI_ENABLED` blocks
- Test stubs for NATIVE_TEST immediately follow the hardware includes

## Comments

**Block headers:**
```cpp
// ===== Section Title =====
```
Used to visually separate major sections within long files.

**Inline explanations:**
- Comment non-obvious constants and magic numbers: `// 2^23 - 1`, `// 308ms time constant`
- Explain WHY, not WHAT: `// Arithmetic right shift by 8 recovers the 24-bit signed integer`

**Deprecation notices:**
```cpp
// DEPRECATED v1.14: flat fields — use adc[] array above. Kept for backward compat.
```

**JSDoc/TSDoc:** Not used — code comments are plain `//` or `/* */`.

## Commit Convention

Format: `<type>: <description>` (imperative, lowercase after colon)

| Type | When to use |
|------|-------------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation only |
| `refactor` | Code restructuring without behavior change |
| `test` | Add or update tests |
| `chore` | Maintenance, tooling |

**Rules:**
- Do NOT add `Co-Authored-By` trailers (no AI attribution)
- Branch into `main` or `develop` only

## Web Asset Build Process

After editing any file in `web_src/`:
1. Run `node tools/build_web_assets.js` to regenerate `src/web_pages.cpp` and `src/web_pages_gz.cpp`
2. Never edit `src/web_pages.cpp` or `src/web_pages_gz.cpp` directly — they are auto-generated and overwritten

Icons must use inline SVG paths from Material Design Icons (pictogrammers.com). No external CDN or icon font.
Pattern: `<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true"><path d="..."/></svg>`

---

*Convention analysis: 2026-03-08*
