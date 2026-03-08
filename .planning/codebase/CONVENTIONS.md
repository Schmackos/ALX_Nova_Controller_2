# Coding Conventions

**Analysis Date:** 2026-03-08

## Naming Patterns

**Files:**
- Source files: `snake_case.cpp` / `snake_case.h` (e.g., `smart_sensing.cpp`, `wifi_manager.h`)
- HAL files: same pattern under `src/hal/` subdirectory (e.g., `hal_device_manager.cpp`)
- Test files: `test_<module_name>.cpp` in `test/test_<module_name>/` directory
- Web source: `NN-purpose.js` / `NN-NN-purpose.css` with 2-digit load-order prefix (e.g., `01-core.js`, `02-ws-router.js`)
- Mock headers: `<LibraryName>.h` mirroring the real library name (e.g., `Arduino.h`, `Preferences.h`)

**Functions (C++):**
- Public module API: `camelCase` (e.g., `detectSignal()`, `handleSmartSensingGet()`, `loadMqttSettings()`)
- Private/static helpers: `camelCase` or `_camelCase` with underscore prefix for private statics (e.g., `_smoothedAudioLevel`)
- HAL virtual methods: `camelCase` matching ESPHome lifecycle pattern (`probe()`, `init()`, `deinit()`, `dumpConfig()`, `healthCheck()`)
- HTTP handlers: `handle<Feature><Verb>()` (e.g., `handleSmartSensingGet()`, `handleSmartSensingPost()`)

**Variables:**
- Local variables: `camelCase` (e.g., `modeStr`, `currentTime`)
- File-local statics: `camelCase` or `_camelCase` with leading underscore for module-private state (e.g., `_smoothedAudioLevel`, `_loginFailCount`, `_wsAuthCount`)
- Mock state namespaced in test files: `Namespace::memberName` (e.g., `ArduinoMock::mockMillis`, `TestAuthState::reset()`)
- AppState members: `camelCase` accessed via `appState.memberName`
- Coordination flags: `volatile` qualifier + descriptive name (e.g., `volatile bool _mqttReconfigPending`)

**Types / Enums:**
- Enum types: `PascalCase` (e.g., `AppFSMState`, `SensingMode`, `ButtonPressType`, `HalDeviceType`)
- Enum values: `UPPER_SNAKE_CASE` (e.g., `STATE_IDLE`, `ALWAYS_ON`, `HAL_DEV_DAC`, `LOG_DEBUG`)
- Structs: `PascalCase` (e.g., `HalDeviceDescriptor`, `HalDeviceConfig`, `HalBusRef`)
- Classes: `PascalCase` (e.g., `AppState`, `HalDevice`, `DebugSerial`)

**Constants / Defines:**
- Build-time defines: `UPPER_SNAKE_CASE` (e.g., `HAL_MAX_DEVICES`, `DSP_MAX_STAGES`, `LOG_PIN`)
- `const` values in `.h`: `UPPER_SNAKE_CASE` (e.g., `WEB_SERVER_PORT`, `BTN_DEBOUNCE_TIME`)
- Capability bit flags: `HAL_CAP_<FEATURE>` pattern (e.g., `HAL_CAP_HW_VOLUME`, `HAL_CAP_DAC_PATH`)

**JavaScript (web_src):**
- Functions: `camelCase` (e.g., `switchTab()`, `buildInitialState()`, `handleCommand()`)
- Variables: `camelCase`, declared at module scope (single concatenated JS scope)
- Constants: `UPPER_SNAKE_CASE` for true constants (e.g., `WS_MIN_RECONNECT_DELAY`, `DEBUG_MAX_LINES`)
- All globals must be declared in `web_src/.eslintrc.json` globals section

## Code Style

**Formatting:**
- No `.clangformat` or `.editorconfig` detected; style is enforced by convention
- 2-space indentation in C++ source files
- 4-space indentation in HAL files (e.g., `src/hal/hal_types.h`)
- Braces on same line for functions and control flow in module files
- HAL class files use same-line opening brace for methods

**Linting (C++):**
- Tool: `cppcheck` — CI-enforced on `src/` (excluding `src/gui/`)
- Flags: `--enable=warning,style,performance --suppress=missingInclude --suppress=unusedFunction --std=c++11 --error-exitcode=1 -i src/gui/`
- Run: `cppcheck --enable=warning,style,performance --suppress=missingInclude --suppress=unusedFunction --std=c++11 --error-exitcode=1 -i src/gui/ src/`

**Linting (JavaScript):**
- Tool: ESLint with config at `web_src/.eslintrc.json`
- Rules enforced: `no-undef` (error), `no-redeclare` (error), `eqeqeq` (smart)
- All JS files concatenated at build time — no module system, shared global scope
- Run: `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`

**Duplicate/reference checks (JavaScript):**
- `node tools/find_dups.js` — detects duplicate `let`/`const` declarations across all JS files
- `node tools/check_missing_fns.js` — detects undefined function references
- Both run in pre-commit hook and CI `js-lint` job

## Import / Include Organization

**C++ Header Order (observed pattern):**
1. Module's own header (e.g., `#include "smart_sensing.h"`)
2. Internal project headers in alphabetical order (e.g., `#include "app_state.h"`, `#include "config.h"`)
3. Arduino/framework headers (e.g., `#include <Arduino.h>`, `#include <ArduinoJson.h>`)
4. System/POSIX headers (e.g., `#include <cmath>`, `#include <string>`)

**Native test includes (inside `#ifdef NATIVE_TEST` guard):**
```cpp
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
// ... other mocks
#else
#include <Arduino.h>
#include <Preferences.h>
#endif
```

**HAL header style:** Use `#pragma once` (not `#ifndef` guards) in all `src/hal/` headers.

**JavaScript:** No `import`/`require` in `web_src/` — files are concatenated by `tools/build_web_assets.js` and load-ordered by filename prefix (`01-`, `02-`, etc.).

## Compile-Time Feature Guards

Feature-conditional code is wrapped with guards defined as build flags:
```cpp
#ifdef DSP_ENABLED
// DSP-specific code
#endif

#ifdef DAC_ENABLED
// DAC/HAL-specific code
#endif

#ifdef USB_AUDIO_ENABLED
// TinyUSB UAC2 code
#endif

#ifdef GUI_ENABLED
// LVGL/TFT code
#endif

#ifdef NATIVE_TEST
// Native test overrides (mock includes, stub statics)
#endif
```

Native test builds compile with `-D UNIT_TEST -D NATIVE_TEST` (both flags set).

## Error Handling

**HAL devices:** Return `HalInitResult` from `init()` — use `hal_init_ok()` on success, `hal_init_fail(DIAG_HAL_INIT_FAILED, "reason")` on failure. Never return raw bool from `init()`.

**JSON API handlers:** Set `doc["success"] = false` and include an error field when returning HTTP error responses.

**Preferences/NVS:** Always call `prefs.end()` after `prefs.begin()` — use scoped blocks or early returns before `end()` only when no writes remain.

**File I/O (LittleFS):** Check `!file || file.size() == 0` after `open()`. Close with `file.close()` before return. Use atomic write pattern for critical config: write to `.tmp` then rename.

**Cross-core flags:** Use `volatile` for all flags read/written from multiple FreeRTOS tasks (e.g., `volatile bool _mqttReconfigPending`, `volatile int8_t _pendingDacToggle`). Deferred toggle setters use validated accessors (`requestDacToggle(int8_t)` only accepts -1, 0, 1).

**AppState dirty flags:** Every state setter must call both the dirty flag setter AND `app_events_signal(EVT_XXX)`:
```cpp
appState.setSomeDirty();
app_events_signal(EVT_DISPLAY);
```

## Logging

**Framework:** `DebugSerial` class with `LOG_D` / `LOG_I` / `LOG_W` / `LOG_E` macros defined in `src/debug_serial.h`.

**Log levels:**
- `LOG_D(fmt, ...)` — DEBUG: high-frequency operational detail, rate-limited state snapshots
- `LOG_I(fmt, ...)` — INFO: state transitions, connect/disconnect events, significant lifecycle events
- `LOG_W(fmt, ...)` — WARN: unexpected conditions that don't stop operation
- `LOG_E(fmt, ...)` — ERROR: failures that degrade functionality

**Module prefix convention (always include in first argument):**
```cpp
LOG_I("[MQTT] Connected to broker");
LOG_D("[Sensing] Smoothed level: %.1f dBFS", level);
LOG_W("[WiFi] Scan skipped — Bus 0 SDIO conflict");
LOG_E("[HAL] init() failed for %s", compat);
```

**Module prefix table:**
| Module | Prefix |
|--------|--------|
| `smart_sensing` | `[Sensing]` |
| `i2s_audio` | `[Audio]` |
| `signal_generator` | `[SigGen]` |
| `buzzer_handler` | `[Buzzer]` |
| `wifi_manager` | `[WiFi]` |
| `mqtt_handler` / `mqtt_publish` | `[MQTT]` |
| `ota_updater` | `[OTA]` |
| `settings_manager` | `[Settings]` |
| `usb_audio` | `[USB Audio]` |
| `hal_*` | `[HAL]`, `[HAL Discovery]`, `[HAL DB]`, `[HAL API]` |
| `output_dsp` | `[OutputDSP]` |
| `gui_*` | `[GUI]` |

**Never log in ISR or real-time audio task paths.** Use dirty-flag pattern: task sets flag, main loop calls `audio_periodic_dump()` for Serial/WS output.

**Transition logging:** Use file-local `static prev` variables to detect state changes; log transitions not repeated state:
```cpp
static SensingMode prevMode = ALWAYS_ON;
if (appState.currentMode != prevMode) {
    LOG_I("[Sensing] Mode changed: %d -> %d", prevMode, appState.currentMode);
    prevMode = appState.currentMode;
}
```

## Comments

**Section banners:** Use `// ===== Section Name =====` for logical groupings within files.

**Function comments:** Use `//` line comments directly above the function for a single-line summary. No JSDoc/Doxygen on firmware functions.

**Complex logic:** Inline `//` comments on the same line or above the statement. Multi-line explanatory blocks use `//` prefix on each line.

**HAL headers:** Brief doc comment on each virtual method explaining the contract:
```cpp
// probe(): Non-destructive check — I2C ACK + chip ID verify
virtual bool probe() = 0;
```

**TODO/FIXME:** Acceptable for deferred work items. Run `grep -rn "TODO\|FIXME"` to audit.

## Function Design

**Size:** Modules follow handler pattern — functions handle one HTTP verb or one subsystem responsibility. Large modules (e.g., `mqtt_handler.cpp` ~1120 lines) split across multiple `.cpp` files (`mqtt_handler.cpp`, `mqtt_publish.cpp`, `mqtt_ha_discovery.cpp`).

**Parameters:** Prefer passing by `const String &` for string parameters. Use value types for primitives.

**Return Values:** Pure computation functions return values. Side-effect functions return `bool` for success/failure. HAL `init()` returns `HalInitResult` (not raw bool).

## Module Design

**Exports:** Declare public API in `.h` with function prototypes. Implement in `.cpp`. No inline implementations in headers except simple getters.

**File-local state:** Use `static` keyword for module-private variables at file scope. Group them near the top of `.cpp` below includes with a `// ===== Section =====` banner.

**AppState access:** Use `appState.memberName` directly (not legacy `#define` aliases). Singleton via `AppState::getInstance()` or the `appState` macro.

**Barrel files:** Not used. Headers included directly as needed.

## Commit Message Convention

```
feat: Add new feature
fix: Fix bug
docs: Update documentation
refactor: Code refactoring
test: Add/update tests
chore: Maintenance tasks
```

Do not add `Co-Authored-By` trailers to any commit message.

---

*Convention analysis: 2026-03-08*
