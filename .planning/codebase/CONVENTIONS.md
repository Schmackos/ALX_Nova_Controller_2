# Coding Conventions

**Analysis Date:** 2026-03-09

## Naming Patterns

**Files:**
- C++ source: `snake_case.cpp` / `snake_case.h` (e.g. `audio_pipeline.cpp`, `hal_device_manager.h`)
- HAL drivers: `hal_<device>.cpp` / `hal_<device>.h` (e.g. `hal_pcm5102a.cpp`, `hal_es8311.h`)
- Test files: `test_<module>.cpp` inside `test/test_<module>/` — one file per module directory
- Web JS: `<NN>-<kebab-name>.js` (numeric prefix controls concat order, e.g. `01-core.js`, `05-audio-tab.js`)
- Web CSS: `<NN>-<kebab-name>.css` (e.g. `00-tokens.css`, `03-components.css`)
- E2E specs: `<kebab-name>.spec.js` (e.g. `hal-devices.spec.js`, `audio-inputs.spec.js`)
- Fixtures: `<kebab-name>.json` in `e2e/fixtures/ws-messages/` and `e2e/fixtures/api-responses/`

**C++ functions:**
- Free functions: `snake_case` (e.g. `audio_pipeline_set_sink()`, `hal_registry_find()`)
- Class methods: `camelCase` (e.g. `registerDevice()`, `findByCompatible()`, `healthCheck()`)
- HAL lifecycle virtuals: `probe()`, `init()`, `deinit()`, `dumpConfig()`, `healthCheck()`, `getInputSource()`
- Test functions: `test_<what_it_tests>` (e.g. `test_register_and_get_device()`, `test_max_devices_limit()`)

**C++ variables:**
- Local variables: `camelCase` or `snake_case` (both used; prefer `camelCase` for new code)
- Private/protected class members: `_prefixedCamelCase` (e.g. `_ready`, `_state`, `_slot`, `_descriptor`)
- Static file-local variables: `_prefixedCamelCase` (e.g. `_smoothedAudioLevel`, `_wsTokens`)
- Constants / `#define`: `SCREAMING_SNAKE_CASE` (e.g. `HAL_MAX_DEVICES`, `BUZZER_PIN`, `LOG_NONE`)
- Enum values: `SCREAMING_SNAKE_CASE` (e.g. `HAL_STATE_AVAILABLE`, `HAL_DEV_DAC`, `HAL_DISC_BUILTIN`)

**JavaScript:**
- Functions: `camelCase` (e.g. `apiFetch()`, `buildInitialState()`, `renderHalDevices()`)
- Variables: `camelCase` (e.g. `currentWifiConnected`, `audioChannelMap`, `halDevices`)
- Constants: `SCREAMING_SNAKE_CASE` (e.g. `WS_MIN_RECONNECT_DELAY`, `HAL_CAP_HW_VOLUME`)
- All top-level JS declarations must be listed in `web_src/.eslintrc.json` globals (380 entries)

**Types:**
- C++ enums: `PascalCase` type name, `SCREAMING_SNAKE_CASE` values
- C++ structs: `PascalCase` (e.g. `HalDeviceDescriptor`, `AudioInputSource`, `WsToken`)
- C++ classes: `PascalCase` (e.g. `HalDevice`, `HalPcm5102a`, `DebugSerial`)

## Code Style

**Formatting:**
- C++: No enforced formatter (cppcheck for static analysis only). Indentation: 4 spaces. Braces on same line for functions, new line accepted too.
- JavaScript: ESLint enforced via `web_src/.eslintrc.json`. ES2020 syntax, `sourceType: "script"` (not module — all files concatenated into one `<script>` block).
- No trailing whitespace rule enforced; no line-length limit enforced.

**Linting (C++):**
- `cppcheck --enable=warning,performance --suppress=missingInclude --suppress=unusedFunction --suppress=badBitmaskCheck --std=c++11 --error-exitcode=1 -i src/gui/ src/`
- `src/gui/` is excluded from cppcheck (LVGL complexity)
- Run in CI (`cpp-lint` job) — not pre-commit

**Linting (JavaScript):**
- ESLint rules enforced: `no-undef` (error), `no-redeclare` (error), `eqeqeq` (smart)
- Run pre-commit and in CI (`js-lint` job)
- Duplicate global declarations checked by `node tools/find_dups.js`
- Undefined function references checked by `node tools/check_missing_fns.js`

## Conditional Compilation Guards

Every HAL driver `.cpp` file wraps its entire body in `#ifdef DAC_ENABLED`. Within those files, hardware-specific includes are further wrapped in `#ifndef NATIVE_TEST`. The standard pattern:

```cpp
#ifdef DAC_ENABLED
// HalFoo driver

#include "hal_foo.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "../debug_serial.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#ifndef OUTPUT
#define OUTPUT 1
#define LOW    0
#define HIGH   1
static void pinMode(int, int) {}
static void digitalWrite(int, int) {}
#endif
#endif

// ... driver implementation ...

#endif // DAC_ENABLED
```

Build flags controlling features: `DAC_ENABLED`, `DSP_ENABLED`, `GUI_ENABLED`, `USB_AUDIO_ENABLED`.
Native test flags: `UNIT_TEST`, `NATIVE_TEST` (set by `[env:native]` in `platformio.ini`).

## Import Organization

**C++ headers — typical order:**
1. Module's own header (e.g. `#include "smart_sensing.h"`)
2. Application state (`#include "app_state.h"`, `#include "globals.h"`)
3. Application utilities (`#include "debug_serial.h"`, `#include "config.h"`)
4. Sibling modules (`#include "i2s_audio.h"`, `#include "websocket_handler.h"`)
5. Third-party (`#include <ArduinoJson.h>`, `#include <LittleFS.h>`)
6. Standard library (`#include <cmath>`, `#include <cstring>`)

**Test file header pattern:**
```cpp
#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#include "../test_mocks/WiFi.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#endif

// Inline .cpp files directly (test_build_src = no)
#include "../../src/hal/hal_device_manager.cpp"
```

## AppState Access Pattern

All application state is accessed through the `appState` singleton using domain-qualified paths:
- `appState.wifi.ssid`, `appState.wifi.enabled`
- `appState.audio.adcEnabled[i]`, `appState.audio.amplifierState`
- `appState.dac.es8311Enabled`, `appState.dac.requestDacToggle(1)`
- `appState.general.darkMode`, `appState.general.deviceSerialNumber`
- `appState.dsp.enabled`, `appState.dsp.swapFailures`
- `appState.mqtt.broker`, `appState.mqtt.port`

Legacy macro aliases (e.g. `#define wifiSSID appState.wifiSSID`) exist for backward compat. New code uses `appState.domain.fieldName` directly.

## Error Handling

**C++:**
- Functions that can fail return `bool` (true=success) or `HalInitResult` (HAL drivers)
- `HalInitResult` wraps success/failure with an error code (`diag_error_codes.h`) and reason string
- Use `hal_init_ok()` / `hal_init_fail(DIAG_CODE, "reason")` factory functions
- HTTP handlers: return JSON `{ "success": false, "error": "message" }` with appropriate HTTP status
- HTTP 503 returned when DSP swap fails (15 endpoints guarded)
- HTTP 429 with `Retry-After` header for login rate limit violations
- Never silently swallow errors; use `LOG_E` before returning false

**JavaScript:**
- `apiFetch()` wrapper handles 401 by redirecting to `/login`
- Async errors in `try/catch` blocks; `console.error` for unexpected exceptions
- WS reconnect with exponential backoff: `WS_MIN_RECONNECT_DELAY=2000ms` → `WS_MAX_RECONNECT_DELAY=30000ms`

## Logging

**Framework:** `debug_serial.h` macros (`LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`)

**Log level semantics:**
- `LOG_I(fmt, ...)`: State transitions, significant events (connect/disconnect, start/stop, health changes)
- `LOG_D(fmt, ...)`: High-frequency operational details (pattern steps, param snapshots)
- `LOG_W(fmt, ...)`: Recoverable abnormal conditions (config fallback, unsupported value)
- `LOG_E(fmt, ...)`: Errors requiring attention (init failure, write failure, assertion fail)

**Mandatory prefix format — every log call includes `[ModuleName]`:**

| Module | Log Prefix | File |
|--------|-----------|------|
| MQTT handler | `[MQTT]` | `src/mqtt_handler.cpp`, `src/mqtt_publish.cpp` |
| Smart sensing | `[Sensing]` | `src/smart_sensing.cpp` |
| HAL device manager | `[HAL]` | `src/hal/hal_device_manager.cpp` |
| HAL discovery | `[HAL Discovery]` | `src/hal/hal_discovery.cpp` |
| HAL device DB | `[HAL DB]` | `src/hal/hal_device_db.cpp` |
| HAL API | `[HAL API]` | `src/hal/hal_api.cpp` |
| HAL PCM5102A | `[HAL:PCM5102A]` | `src/hal/hal_pcm5102a.cpp` |
| Audio / I2S | `[Audio]` | `src/i2s_audio.cpp` |
| WiFi | `[WiFi]` | `src/wifi_manager.cpp` |
| OTA | `[OTA]` | `src/ota_updater.cpp` |
| Settings | `[Settings]` | `src/settings_manager.cpp` |
| USB Audio | `[USB Audio]` | `src/usb_audio.cpp` |
| Signal generator | `[SigGen]` | `src/signal_generator.cpp` |
| Buzzer | `[Buzzer]` | `src/buzzer_handler.cpp` |
| GUI | `[GUI]` | `src/gui/gui_manager.cpp` etc. |

**ISR and audio task logging is FORBIDDEN.** The `audio_pipeline_task` (Core 1) must never call `LOG_*` or `Serial.print`. Use the dirty-flag pattern: task sets a flag, main loop calls `audio_periodic_dump()` for actual output.

**Log transitions, not steady state.** Use a `static prev` variable to detect changes and log only when state differs.

## Comments

**When to comment:**
- Non-obvious hardware interactions: clock topology, DMA constraints, ISR safety, I2C bus conflicts
- Cross-module contracts (e.g. `// NOTE: I2S TX channel is owned by i2s_audio.cpp`)
- Safety invariants (e.g. `// Never call i2s_configure_adc1() in the task loop`)
- Section dividers use `// ===== Section Name =====` style

**JSDoc / TSDoc:**
- Not used for internal functions. Header comment blocks at top of spec files describe test intent.
- E2E test files start with a `/** ... */` JSDoc block naming the spec and key constraints.

## Function Design

**Size:** Handlers and lifecycle methods are allowed to be long (MQTT handler ~1120 lines split across 3 files). New code should prefer extracting helpers.

**Parameters:** C-style `(const char* name, int value)` over references for POD types in C; references for objects. HAL `init()` reads config from `HalDeviceManager::instance().getConfig(_slot)` rather than taking parameters.

**Return values:**
- Void for fire-and-forget operations
- `bool` for fallible operations without detail needed
- `HalInitResult` for HAL driver `init()` (structured error with code + reason)
- `int` slot index (returns -1 on failure) for registration functions

## Module Design

**C++ modules** follow a header + implementation split:
- Header declares public API with `#pragma once` or `#ifndef` guard
- HAL headers wrap content in `#ifdef DAC_ENABLED`
- Implementation files start with `#ifdef DAC_ENABLED` for HAL drivers

**JavaScript modules** are concatenated scripts, not ES modules:
- Variables declared with `let` / `const` at the top of the file contribute to shared scope
- No `export`/`import` — all declarations must be in `web_src/.eslintrc.json` globals
- Numeric filename prefix `NN-` controls load order strictly

## Commit Convention

```
feat: Add new feature
fix: Fix bug
docs: Update documentation
refactor: Code refactoring
test: Add/update tests
chore: Maintenance tasks
```

**IMPORTANT:** Never add `Co-Authored-By` trailers to commits. No AI attribution lines.

---

*Convention analysis: 2026-03-09*
