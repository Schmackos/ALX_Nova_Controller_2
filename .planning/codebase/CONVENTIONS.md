# Coding Conventions

**Analysis Date:** 2026-03-22

## Naming Patterns

**Files (C++):**
- `snake_case.cpp` and `snake_case.h` for all source and header files
- Test files: `test_<module_name>.cpp` in `test/test_<module>/` directories
- HAL drivers: `hal_<device>.cpp/.h` in `src/hal/`
- State headers: `<domain>_state.h` in `src/state/`
- One implementation per module; headers contain declarations + inline helpers

**Files (JavaScript — web_src/):**
- `NN-kebab-case.js` where `NN` is a two-digit load order (e.g., `01-core.js`, `06-peq-overlay.js`, `27-debug-console.js`)
- All JS files concatenated in filename order into a single `<script>` block — shared global scope
- No `const` or `let` redeclaration across files; verify with `node tools/find_dups.js`
- CSS files: `NN-kebab-case.css` with same ordering scheme (e.g., `01-variables.css`, `03-components.css`)

**Functions (C++):**
- `camelCase` for public functions: `verifyPassword()`, `timingSafeCompare()`, `loadMqttSettings()`
- `snake_case` for module-level public APIs: `audio_pipeline_init()`, `hal_db_lookup()`, `app_events_signal()`
- Private static functions use underscore prefix: `static void _helperFunction()`
- Init/deinit pairs: `<module>_init()` / `<module>_deinit()`

**Functions (JavaScript):**
- `camelCase` for all function names: `updateConnectionStatus()`, `switchTab()`, `apiFetch()`
- Event handlers: `on` prefix or descriptive verb (e.g., `onMatrixCellClick()`, `handleBinaryMessage()`)
- Nested helpers: `function localHelper() {}` inside parent scope

**Variables (C++):**
- `camelCase` for local and member variables: `maxAllocBlock`, `lastFailTime`
- Static file-scope (module-private): `_camelCase` prefix: `_analysis`, `_diagnostics`, `_wsTokens`
- Volatile cross-core variables: declared `volatile` with cross-core documentation: `volatile bool audioPaused`
- Constants: `UPPER_SNAKE_CASE`: `MAX_SESSIONS`, `DMA_BUF_COUNT`, `HEAP_WARNING_THRESHOLD`

**Variables (JavaScript):**
- `camelCase` for local and global variables: `audioSubscribed`, `halScanning`
- Constants: `UPPER_SNAKE_CASE`: `WS_MIN_RECONNECT_DELAY`, `DEBUG_MAX_LINES`, `LERP_SPEED`
- Booleans: `is*`/`has*`/`*Enabled`/`*Active` prefix: `isValidIP()`, `backlightOn`, `audioSubscribed`
- DOM element references: `*El` suffix: `statusEl`, `pwdInput`

**Types/Structs (C++):**
- `PascalCase` for struct and class names: `DspBiquadParams`, `HalDeviceConfig`, `AppState`
- Enum names: `PascalCase`: `DspStageType`, `AppFSMState`, `LogLevel`, `DiagErrorCode`
- Enum values: `UPPER_SNAKE_CASE`: `DSP_BIQUAD_PEQ`, `STATE_IDLE`, `DIAG_HAL_INIT_FAILED`
- Diagnostic codes use `0xSSCC` format (SS=subsystem, CC=error): `DIAG_SYS_HEAP_CRITICAL = 0x0101`

**Macros (C++):**
- `UPPER_SNAKE_CASE`: `DSP_ENABLED`, `DAC_ENABLED`, `UNIT_TEST`, `NATIVE_TEST`
- Build-time configuration: define in `platformio.ini` or `src/config.h` with `#ifndef` fallback defaults
- Feature guards: `#ifdef DAC_ENABLED`, `#ifdef GUI_ENABLED`, `#ifdef USB_AUDIO_ENABLED`
- Legacy alias macros: `#define wifiSSID appState.wifiSSID` — do NOT add new ones; use `appState.domain.field` directly

## Code Style

**Formatting (C++):**
- Indentation: 2 or 4 spaces (4 spaces dominant in core audio code, 2 in HAL/utility)
- Braces: K&R style — opening `{` on same line as function/if/loop
- Pointers/references: attach to variable, not type: `int *ptr`, `const String &ref`
- Max line length: ~100 characters (no hard limit enforced)
- Single space around operators, no space inside parentheses

**Example C++ style (from `src/auth_handler.cpp`):**
```cpp
bool timingSafeCompare(const String &a, const String &b) {
  size_t lenA = a.length();
  size_t lenB = b.length();
  size_t maxLen = (lenA > lenB) ? lenA : lenB;

  if (maxLen == 0) {
    return (lenA == 0 && lenB == 0);
  }

  volatile uint8_t result = (lenA != lenB) ? 1 : 0;
  const char *pA = a.c_str();
  const char *pB = b.c_str();

  for (size_t i = 0; i < maxLen; i++) {
    uint8_t byteA = (i < lenA) ? (uint8_t)pA[i] : 0;
    uint8_t byteB = (i < lenB) ? (uint8_t)pB[i] : 0;
    result |= byteA ^ byteB;
  }

  return result == 0;
}
```

**Formatting (JavaScript):**
- Indentation: 2 spaces (consistent)
- No prettier config — hand-formatted
- Template literals used for multi-line HTML (DOM card builders)
- Single quotes for strings, double quotes for HTML attributes
- Spacing: loose, favors readability over strict rules

**Example JavaScript style (from `web_src/js/01-core.js`):**
```javascript
async function apiFetch(url, options = {}) {
  try {
    const response = await fetch(url, mergedOptions);
    if (response.status === 401) {
      window.location.href = '/login';
      return new Promise(() => {}); // Never-resolving promise
    }
    return response;
  } catch (error) {
    console.error(`API Fetch Error [${url}]:`, error);
    throw error;
  }
}
```

**Linting:**
- ESLint (`web_src/.eslintrc.json`): 420+ globals registered to handle concatenated scope
  - Rules: `no-undef` (error), `no-redeclare` (error), `eqeqeq` (smart)
  - Run: `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`
- cppcheck: Static analysis on `src/` (CI-enforced, excluding `src/gui/`)
  - Flags: `--enable=warning,performance --suppress=missingInclude --suppress=unusedFunction`
- No `.prettierrc` — manual code review preferred
- Pre-commit hooks (`.githooks/pre-commit`): 5 checks (see Pre-commit Hooks section)

## Import Organization

**Order (C++ — standard pattern from `src/settings_manager.cpp`):**
1. Own header: `#include "settings_manager.h"`
2. Local project headers: `#include "app_state.h"`, `#include "config.h"`, `#include "debug_serial.h"`
3. Feature-guarded local headers: `#ifdef DAC_ENABLED #include "dac_hal.h"`
4. Third-party libraries: `#include <ArduinoJson.h>`, `#include <LittleFS.h>`
5. Platform headers: `#include <WiFi.h>`, `#include <Preferences.h>`

**Path conventions (C++):**
- HAL headers: `#include "hal/hal_types.h"`, `#include "hal/hal_device_manager.h"`
- State headers: `#include "state/audio_state.h"`, `#include "state/wifi_state.h"`
- No namespace aliases — full relative paths used

**Order (JavaScript — web_src/):**
- Not applicable — concatenated scripts in shared scope, no import statements
- File load order controlled by filename prefix (`01-core.js` loads before `22-settings.js`)

**Order (JavaScript — E2E tests):**
1. Playwright/test framework: `const { test, expect } = require('@playwright/test');`
2. Helpers: `const { buildInitialState } = require('./ws-helpers');`
3. Node builtins: `const path = require('path');`
4. Fixtures: `const HAL_FIXTURE = JSON.parse(fs.readFileSync(...));`

## Error Handling

**Pattern (C++ — embedded, no exceptions):**
- Early return with boolean: `if (condition) return false;`
- Logging + state flag: `LOG_E("[Module] error description"); appState.setError(CODE);`
- Null pointer checks: `if (!ptr) return;` or `if (ptr == nullptr) { LOG_E(...); return; }`
- Timing-safe comparisons for security: `timingSafeCompare(a, b)` — constant-time string comparison
- Return value checking: all callers of `requestDeviceToggle()` check return value and `LOG_W` on failure; REST endpoints return HTTP 503 on overflow
- Diagnostic emissions: `diag_emit(DIAG_HAL_INIT_FAILED, deviceName)` for structured error tracking

**Example from `src/auth_handler.cpp`:**
```cpp
bool verifyPassword(const String &inputPassword, const String &storedHash) {
  if (storedHash.find("p1:") == 0 && storedHash.length() == 100) {
    uint8_t salt[16];
    if (!hexToBytes(storedHash.c_str() + 3, salt, 16)) return false;

    String computed = hashPasswordPbkdf2WithSalt(inputPassword, salt);
    return timingSafeCompare(computed, storedHash);
  }

  return timingSafeCompare(hashPassword(inputPassword), storedHash);
}
```

**Pattern (JavaScript):**
- Try/catch for async operations: `try { await fetch(...); } catch(e) { console.error(...); }`
- Response status checks: `if (response.status === 401) { window.location.href = '/login'; }`
- Null element guards: `if (el) { el.textContent = ...; }`
- Use async/await, never promise chains

## Logging

**Framework (C++):** Custom `debug_serial.h` macros wrapping `DebugSerial` class
- Macros: `LOG_D()`, `LOG_I()`, `LOG_W()`, `LOG_E()`
- Format: `LOG_E("[ModuleName] message with %s formatting", var);`
- Module prefixes (bracket-enclosed): `[Audio]`, `[WiFi]`, `[Sensing]`, `[HAL]`, `[DSP]`, `[MQTT]`, `[OTA]`, `[Settings]`, `[USB Audio]`, `[GUI]`, `[HAL Discovery]`, `[HAL DB]`, `[HAL API]`, `[OutputDSP]`, `[Buzzer]`, `[SigGen]`
- Output: serial (115200 baud) AND WebSocket via `broadcastLine()` — sends `"module"` as separate JSON field extracted from `[ModuleName]` prefix

**When to log (C++):**
- `LOG_I()`: State transitions, significant events (connect/disconnect, start/stop, health changes)
- `LOG_D()`: High-frequency operational details (param snapshots, intermediate steps)
- `LOG_W()`: Recoverable errors, fallback paths, rate-limit hits
- `LOG_E()`: Critical failures, allocation errors, health transitions
- **NEVER log in ISR or audio task** — `Serial.print()` blocks on UART TX buffer full, starving DMA. Use dirty-flag pattern instead.

**Dirty-flag logging pattern (required for real-time tasks):**
```cpp
// In audio task — set flag, no logging
_dumpReady = true;

// In main loop — do logging
if (_dumpReady) {
  LOG_I("[Audio] RMS: %.2f dBFS", rms_db);
  _dumpReady = false;
}
```

**Browser logging (JavaScript):**
- `console.log()` for general info, `console.warn()` for recoverable issues, `console.error()` for failures
- No custom logging framework — native browser console

**Debug Console (Web UI — `web_src/js/27-debug-console.js`):**
- Live serial log streaming via WebSocket JSON `{ module, message, level, timestamp }`
- Module filtering via auto-populated chips from `[ModuleName]` prefixes
- Search/highlight, entry count badges (red=errors, amber=warnings), sticky localStorage filters

## Comments

**When to comment (C++):**
- Complex algorithm sections: FFT setup, biquad coefficient computation, matrix routing
- Non-obvious design decisions: cross-core synchronization, volatile semantics, heap allocation constraints
- Safety-critical code: timing-safe comparisons, rate limiting, resource cleanup
- Inline `// Note:` for removed or changed features explaining why

**Section separators:**
```cpp
// ===== PBKDF2 Hashing (mirrors auth_handler.cpp Phase 3) =====
```

**Header documentation:**
```cpp
/**
 * Hash password with PBKDF2-SHA256 + random salt.
 * @param password The plaintext password to hash
 * @return Hex-encoded string in format "p1:<salt>:<key>"
 */
String hashPasswordPbkdf2(const String &password);
```

**When to comment (JavaScript):**
- Complex state machine logic (WS reconnection, debouncing)
- Non-obvious math (LERP factors, dB conversions)
- Why a workaround exists (browser quirks, UI rendering edge cases)

**Comment style:**
- C++: `// Single-line` (dominant), `/* Multi-line */` (rarely)
- JavaScript: `// Single-line`, `/** JSDoc */` in E2E helpers and fixtures
- Section separators: `// ===== Section Name =====` (5 equals on each side)

## Function Design

**Size (C++):**
- Prefer functions <200 lines (common: 50-150 lines)
- Audio processing: pure functions accepting `const` inputs, returning computed results
- Initialization: ~100-200 lines typical (setup many subsystems)
- Avoid deeply nested code — use early returns, helpers

**Parameters (C++):**
- Pass by `const` reference for large objects: `const String &password`, `const HalDeviceConfig &cfg`
- Primitive types by value: `int count`, `bool enabled`
- Output parameters: avoid — prefer return struct or boolean
- Variadic functions for logging: `LOG_E(const char *fmt, ...)`

**Return values (C++):**
- Boolean for success/failure: `bool verifyPassword(...)`, `bool set_sink(...)`
- Enum for multi-state operations: `HalDeviceState discoverDevice(...)`
- Void for fire-and-forget: `void setAmplifierState(bool enabled)`

**Size (JavaScript):**
- Prefer <100 lines (common: 20-80 lines)
- Event handlers: 10-30 lines
- DOM builders: 30-60 lines using template literals

## Module Design

**Exports (C++):**
- One public header per module: `audio_pipeline.h`, `auth_handler.h`
- Header: struct definitions, enum definitions, public function declarations
- Private implementation in `.cpp` with `static` functions
- `#ifndef HEADER_NAME_H` / `#define HEADER_NAME_H` include guards (or `#pragma once` for newer headers)

**Barrel Files (JavaScript):**
- Not used — global scope means all functions automatically available after script load
- Each `NN-kebab-case.js` file loads independently in concatenation order

**Module structure pattern (C++):**
```cpp
// audio_pipeline.h
#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

enum AudioMode { AUDIO_IDLE, AUDIO_ACTIVE };
struct AudioPipelineConfig { int sampleRate; int maxChannels; };

void audio_pipeline_init(const AudioPipelineConfig &cfg);
bool audio_pipeline_set_sink(int slot, ...);
void audio_pipeline_deinit();

#endif // AUDIO_PIPELINE_H

// audio_pipeline.cpp
#include "audio_pipeline.h"
#include "debug_serial.h"

static volatile bool _initialized = false;
static AudioPipelineConfig _cfg = {};

static void _sanitizeConfig(AudioPipelineConfig &cfg) {
  if (cfg.sampleRate <= 0) cfg.sampleRate = 48000;
}

void audio_pipeline_init(const AudioPipelineConfig &cfg) {
  _cfg = cfg;
  _sanitizeConfig(_cfg);
  _initialized = true;
  LOG_I("[Audio] Init: SR=%d Hz", _cfg.sampleRate);
}
```

**Initialization pattern:**
- C++: explicit `<module>_init()` call from `setup()` or `main.cpp`, not in global constructors
- JavaScript: initialization in global scope on script load, or in tab-activation callbacks

## State Management Pattern

**AppState Singleton (C++):**
- Single instance accessed via `AppState::getInstance()` or `appState` macro
- Decomposed into 15 domain-specific headers in `src/state/`
- Usage: `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`, `appState.dsp.enabled`
- Dirty flags: `appState.setWifiDirty()`, `appState.isWifiDirty()` — trigger WebSocket broadcasts and MQTT publishes

**Event signaling (C++):**
- Every dirty-flag setter also calls `app_events_signal(EVT_XXX)` (bit flags in `src/app_events.h`)
- Main loop: `app_events_wait(5)` wakes on any event, falls back to 5ms tick when idle
- 17 event bits assigned (bits 0-16), 7 spare; bits 24-31 reserved by FreeRTOS

**Example state pattern:**
```cpp
appState.wifi.ssid = "MyNetwork";
appState.setWifiDirty();      // Also calls app_events_signal(EVT_WIFI)

// Main loop
uint32_t bits = app_events_wait(5);
if (appState.isWifiDirty()) {
  sendWifiState();             // WebSocket broadcast
  appState.clearWifiDirty();
}
```

## Cross-Core Safety

**Core allocation (FreeRTOS):**
- **Core 1 (audio-only)**: `loopTask` (Arduino main loop, priority 1) + `audio_pipeline_task` (priority 3)
- **Core 0 (system)**: `gui_task`, `mqtt_task`, `usb_audio_task`, OTA tasks
- Never pin new tasks to Core 1

**Synchronization mechanisms:**
- Dirty flags + event signaling for state change notifications
- Volatile fields for cross-core reads: `volatile bool audioPaused`
- Binary semaphore for I2S re-init handshake: `appState.audio.taskPausedAck`
- `vTaskSuspendAll()` for slot-indexed pipeline operations on Core 1

**I2S re-init handshake pattern:**
```cpp
appState.audio.paused = true;  // Signal audio task to pause
if (!xSemaphoreTake(appState.audio.taskPausedAck, pdMS_TO_TICKS(50))) {
  LOG_W("[DAC] Audio task did not pause in time");
}
i2s_driver_uninstall(...);     // Safe — audio task is paused
```

## Conditional Compilation

**Feature guards (used extensively):**
```cpp
#ifdef DAC_ENABLED      // HAL framework, DAC, pipeline bridge
#ifdef DSP_ENABLED      // DSP pipeline, biquad, FIR, compressor
#ifdef GUI_ENABLED      // LVGL GUI, TFT display
#ifdef USB_AUDIO_ENABLED // TinyUSB UAC2 USB audio
#ifdef UNIT_TEST        // Test stubs, mock replacements
#ifdef NATIVE_TEST      // Native platform-specific mock includes
```

**Test stub pattern (from `src/app_events.h`):**
```cpp
#ifdef UNIT_TEST
// Native test stubs — all no-ops
#define app_events_init()           ((void)0)
#define app_events_signal(bits)     ((void)0)
#define app_events_wait(timeout_ms) (0U)
#else
void app_events_init();
void app_events_signal(EventBits_t bits);
EventBits_t app_events_wait(uint32_t timeout_ms);
#endif
```

**Platform-specific includes (from test files):**
```cpp
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#endif
```

## Pre-commit Hooks

**Activated via:** `git config core.hooksPath .githooks`

**Pre-commit checks (`.githooks/pre-commit` — 5 checks):**
1. `node tools/find_dups.js` — Detect duplicate JS declarations across files
2. `node tools/check_missing_fns.js` — Find undefined function references
3. ESLint on `web_src/js/` with `web_src/.eslintrc.json`
4. `node tools/check_mapping_coverage.js` — Doc mapping coverage check
5. `node tools/diagram-validation.js` — Architecture diagram symbol validation

## Icons (Web UI)

Use inline SVG paths from [Material Design Icons (MDI)](https://pictogrammers.com/library/mdi/). No external icon CDN or font library — page is self-contained and works offline.

**Standard pattern:**
```html
<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true">
  <path d="<paste MDI path here>"/>
</svg>
```

- Use `fill="currentColor"` so icon inherits CSS `color` property
- Set explicit `width`/`height` (18px inline, 24px standalone)
- Always add `aria-hidden="true"` on decorative icons; add `aria-label` on icon-only interactive elements

## Web Asset Build Pipeline

**Source:** `web_src/` (HTML, CSS, JS files)
**Generated:** `src/web_pages.cpp` and `src/web_pages_gz.cpp` (auto-generated, do not edit)
**Build command:** `node tools/build_web_assets.js`

After ANY edit to `web_src/` files, run the build command before building firmware. New top-level JS declarations must be added to `web_src/.eslintrc.json` globals.

## Commit Convention

```
feat: Add new feature
fix: Fix bug
docs: Update documentation
refactor: Code refactoring
test: Add/update tests
chore: Maintenance tasks
```

Do NOT add `Co-Authored-By` trailers to commit messages.

---

*Convention analysis: 2026-03-22*
