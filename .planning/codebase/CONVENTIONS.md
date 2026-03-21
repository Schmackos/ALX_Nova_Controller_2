# Coding Conventions

**Analysis Date:** 2026-03-21

## Naming Patterns

**Files (C++):**
- `snake_case.cpp` and `snake_case.h` for implementation and headers
- Test files: `test_<module_name>.cpp` (e.g., `test_auth_handler.cpp`)
- Pattern: one implementation per module, headers with full declaration + inline helpers

**Files (JavaScript):**
- `NN-kebab-case.js` where `NN` is a two-digit load order (e.g., `01-core.js`, `06-peq-overlay.js`)
- All JS files concatenated in filename order into a single `<script>` block — shared global scope
- No `const` or `let` redeclaration across files; use `node tools/find_dups.js` to verify

**Functions (C++):**
- `camelCase` for function names (e.g., `verifyPassword()`, `timingSafeCompare()`)
- Private static functions use underscore prefix: `static void _helperFunction()`
- Pattern: utility functions at file scope, public functions in headers with declarations

**Functions (JavaScript):**
- `camelCase` for function names
- ISR/callback handlers: prefixed with event type or context (e.g., `onMatrixCellClick()`, `updateConnectionStatus()`)
- Nested helpers use `function localHelper() {}` syntax inside parent function scope
- IIFE pattern (immediately-invoked) used for module initialization, but sparse — most code in global scope

**Variables (C++):**
- `camelCase` for local and member variables (e.g., `maxAllocBlock`, `lastFailTime`)
- Static file-scope (module-private): `_camelCase` prefix (e.g., `_analysis`, `_diagnostics`)
- Volatile cross-core variables: declared `volatile` with explicit cross-core documentation (e.g., `volatile bool audioPaused` in AudioState)
- Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_SESSIONS`, `DMA_BUF_COUNT`)

**Variables (JavaScript):**
- `camelCase` for local and global variables
- Constants: `UPPER_SNAKE_CASE` (e.g., `WS_MIN_RECONNECT_DELAY`, `DEBUG_MAX_LINES`)
- Booleans: `is*` or `has*` or `*Enabled`/`*Active` prefix (e.g., `isValidIP()`, `backlightOn`, `audioSubscribed`)
- DOM element references: `*El` suffix (e.g., `statusEl`, `pwdInput`)

**Types/Structs (C++):**
- `PascalCase` for struct and class names (e.g., `DspBiquadParams`, `HalDeviceConfig`, `AppState`)
- Nested structs: capitalized member prefix before struct name (e.g., `struct Session`, `struct WsToken`)
- `enum` names: `PascalCase` (e.g., `DspStageType`, `AppFSMState`, `LogLevel`)
- Enum values: `UPPER_SNAKE_CASE` (e.g., `DSP_BIQUAD_PEQ`, `STATE_IDLE`)

**Macros (C++):**
- `UPPER_SNAKE_CASE` (e.g., `DSP_ENABLED`, `DAC_ENABLED`, `UNIT_TEST`)
- Build-time configuration: define in `platformio.ini` or `src/config.h` with fallback defaults
- Legacy alias macros: `#define wifiSSID appState.wifiSSID` for backward compat (discourage new code)

**Imports/Includes (C++):**
- `#include <system>` for system/library headers (angle brackets)
- `#include "local.h"` for project headers (quotes)
- Order: 1) System, 2) Library (Arduino, ESP-IDF), 3) Local headers
- HAL drivers include HAL core first: `#include "hal/hal_types.h"` before device-specific headers

**Imports (JavaScript):**
- `const { ... } = require('path')` for CommonJS (mock server, E2E tests)
- Global scope access — all functions declared at top-level or inside parent function (no ES6 modules)
- External libraries loaded via `<script>` tags in HTML: QRCode, marked (release notes), ws library (browser WebSocket)

## Code Style

**Formatting (C++):**
- Max line length: ~100 characters (no hard limit enforced)
- Indentation: 2 or 4 spaces (mix observed; 4 spaces dominant in core audio code)
- Braces: K&R style — opening `{` on same line as function/if/loop
- Pointers/references: attach to variable, not type (e.g., `int *ptr`, `String &ref`)
- Spacing: single space around operators, inside parentheses rarely

**Example C++ style:**
```cpp
struct HalDeviceConfig {
  String compatible;
  int pinA;
  int pinB;
};

void processDevice(const HalDeviceConfig &cfg) {
  if (cfg.pinA > 0) {
    int value = cfg.pinA + cfg.pinB;
    return value;
  }
  return -1;
}
```

**Formatting (JavaScript):**
- No prettier config active — code is hand-formatted
- Indentation: 2-4 spaces (typically 2)
- Template literals used for multi-line HTML (e.g., building DOM cards)
- Single quotes for strings (common), double quotes for attributes
- Spacing: loose, favors readability over strict rules

**Example JavaScript style:**
```javascript
function updateConnectionStatus(connected) {
  const statusEl = document.getElementById('wsConnectionStatus');
  if (statusEl) {
    if (connected) {
      statusEl.textContent = 'Connected';
      statusEl.className = 'info-value text-success';
    } else {
      statusEl.textContent = 'Disconnected';
      statusEl.className = 'info-value text-error';
    }
  }
}
```

**Linting:**
- ESLint (`web_src/.eslintrc.json`): 380 globals registered to handle concatenated scope
  - Rules: `no-undef` (error), `no-redeclare` (error), `eqeqeq` (smart)
  - Run: `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`
- cppcheck: Static analysis on `src/` (CI-enforced, excluding `src/gui/`)
- No `.prettierrc` — manual code review preferred
- Pre-commit hooks (`.githooks/pre-commit`): find_dups.js, check_missing_fns.js, ESLint

## Import Organization

**Order (C++ — from `auth_handler.cpp`):**
1. Local project headers: `#include "auth_handler.h"`, `#include "app_state.h"`
2. Arduino framework: `#include <Arduino.h>`
3. Third-party libraries: `#include <ArduinoJson.h>`, `#include <Preferences.h>`
4. System/vendor crypto: `#include <mbedtls/md.h>`, `#include <esp_timer.h>`
5. Conditional (platform-specific): `#ifndef NATIVE_TEST #include <driver/i2s_std.h>` etc.

**Path aliases (C++):**
- HAL headers: `#include "hal/hal_types.h"`, `#include "hal/hal_device_manager.h"`
- State headers: `#include "state/audio_state.h"`, `#include "state/wifi_state.h"`
- No namespace aliases — full paths used

**Order (JavaScript — from `01-core.js`):**
1. WebSocket connection setup
2. Global state initialization
3. Helper functions (`apiFetch()`, event handlers)
4. Event loop (connection polling, reconnection)
5. Exported functions (typically loaded last in file order)

No bundling or ES6 imports — entire app runs as concatenated scripts in shared scope.

## Error Handling

**Pattern (C++):**
- Early return with error codes: `if (condition) return false;`
- Logging + state flag: `LOG_E("[Module]"); appState.setError(ERROR_CODE);`
- No exceptions — embedded systems avoid C++ exception overhead
- Null checks: `if (!ptr) return;` or `if (ptr == nullptr) { LOG_E(...); return; }`
- Timing-safe comparisons: `if (timingSafeCompare(a, b))` for password/token checks (constant-time)

**Example from `auth_handler.cpp`:**
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

**Pattern (JavaScript — from `apiFetch()`):**
- Try/catch for async operations: `try { const resp = await fetch(...); } catch(e) { console.error(...); }`
- Response status checks: `if (response.status === 401) { window.location.href = '/login'; }`
- Null element guards: `if (el) { el.textContent = ...; }`
- Promise rejection handling: no promise chains, use async/await instead

**Example from `apiFetch()`:**
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

## Logging

**Framework (C++):**
- `debug_serial.h` macros: `LOG_D()`, `LOG_I()`, `LOG_W()`, `LOG_E()`
- Format: `LOG_E("[ModuleName] message with %s", var);`
- Module prefixes (bracket-enclosed): `[Audio]`, `[WiFi]`, `[Sensing]`, `[HAL]`, `[DSP]`
- Sent to serial (115200 baud) AND WebSocket via `broadcastLine()`
- **Never log in ISR or audio task** — `Serial.print()` blocks on UART, starves DMA. Use dirty-flag pattern instead.

**When to log (C++):**
- `LOG_I()`: State transitions, significant events (connect/disconnect, start/stop, errors)
- `LOG_D()`: High-frequency details (param snapshots, intermediate steps) — filtered at `LOG_DEBUG` level
- `LOG_W()`: Recoverable errors, fallback paths, rate-limit hits
- `LOG_E()`: Critical failures, allocation errors, health transitions

**Example logging pattern:**
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
- `console.log()` for general info
- `console.warn()` for recoverable issues
- `console.error()` for failures (e.g., API errors, WebSocket auth failures)
- WebSocket received messages printed as: `console.log('WS recv:', msg);`
- No custom logging framework — native browser console

**Debug Console (Web UI):**
- Live serial log streaming via WebSocket `broadcastLine()` → JSON `{ module, message, level, timestamp }`
- Module filtering via chips (auto-populated from `[ModuleName]` prefixes)
- Search/highlight, entry count badges (red=errors, amber=warnings), sticky localStorage filters
- Relative/absolute timestamp toggle, downloadable log (`.log` file)
- Implemented in `web_src/js/27-debug-console.js`

## Comments

**When to comment (C++):**
- Complex algorithm sections: FFT setup, biquad coefficient computation, matrix routing
- Non-obvious design decisions: cross-core synchronization, volatile semantics, heap allocation constraints
- Safety-critical code: timing-safe comparisons, rate limiting, resource cleanup
- Avoid obvious comments (`i++` doesn't need "increment i")

**JSDoc/TSDoc pattern (C++ headers):**
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
- Skip obvious comments

**Comment style:**
- C++: `// Single-line`, `/* Multi-line */` (rarely used)
- JavaScript: `// Single-line` (no JSDoc in test files unless extensive)
- Section separators: `// ===== Section Name =====` (5 equals on each side)

**Example section header:**
```cpp
// ===== PBKDF2 Hashing (mirrors auth_handler.cpp Phase 3) =====

static String hashPasswordPbkdf2(const String &password) {
  // ...
}
```

## Function Design

**Size (C++):**
- Prefer functions <200 lines (common: 50-150 lines)
- Audio processing: pure functions accepting `const` inputs, returning computed results
- Initialization: ~100-200 lines typical (setup many subsystems)
- Avoid deeply nested code — use early returns, helpers

**Parameters (C++):**
- Pass by `const` reference for large objects: `const String &password`, `const HalDeviceConfig &cfg`
- Primitive types by value: `int count`, `bool enabled`
- Output parameters: avoid — prefer return struct or tuple
- Variadic functions for logging: `LOG_E(const char *fmt, ...)`

**Example parameter pattern:**
```cpp
// Good: const ref for large objects
void processPipeline(const AudioPipeline &pipeline, int sampleCount) {
  // ...
}

// Bad: passing large struct by value (copy overhead)
void processPipeline(AudioPipeline pipeline, int sampleCount) { // Avoid
  // ...
}
```

**Return values (C++):**
- Boolean for success/failure: `bool verifyPassword(...)`
- Enum for multi-state operations: `HalDeviceState discoverDevice(...)`
- Struct for multiple return values (uncommon — prefer output reference params or simple bool)
- Void for fire-and-forget: `void setAmplifierState(bool enabled)`

**Size (JavaScript):**
- Prefer <100 lines (common: 20-80 lines)
- Event handlers: 10-30 lines
- Utility functions: 5-15 lines
- DOM builders: 30-60 lines

**Parameters (JavaScript):**
- Single `options` object for >3 parameters: `function updateUI(opts) { const { connected, level, mode } = opts; }`
- Optional parameters: default to `undefined` in function signature
- No required object destructuring in parameters — check existence first

**Return values (JavaScript):**
- `true`/`false` for existence/validity checks
- Arrays or objects for multiple values: `return { x: val1, y: val2 };`
- `undefined`/`null` for "not found" (common), but be explicit in callers
- Event handlers typically return nothing (undefined)

## Module Design

**Exports (C++):**
- One public header per module (e.g., `audio_pipeline.h`)
- Header contains struct definitions, enum definitions, public function declarations
- No static exports — all exporting is via public header
- Private implementation in `.cpp` file with `static` functions

**Barrel Files (JavaScript):**
- Not used — no `index.js` re-exports
- Each module (file) loads its own state
- Global scope means all functions automatically available after script load

**Example module structure (C++):**
```cpp
// audio_pipeline.h
#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

enum AudioMode { AUDIO_IDLE, AUDIO_ACTIVE };
struct AudioPipelineConfig { int sampleRate; int maxChannels; };

void audio_pipeline_init(const AudioPipelineConfig &cfg);
void audio_pipeline_process(float *samples, int count);
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

**Initialization patterns:**
- C++: explicit `init()` call from `setup()` or `main()`, not in global constructors
- JavaScript: initialization in `setup()` function or directly in module scope

## State Management Pattern

**AppState Singleton (C++):**
- Single instance accessed via `AppState::getInstance()` or `appState` macro
- Decomposed into 15 domain-specific headers in `src/state/`
- Usage: `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`, `appState.dsp.enabled`
- Dirty flags: `appState.setWifiDirty()`, `appState.isWifiDirty()` — trigger WebSocket broadcasts
- Cross-core synchronization: volatile fields in sub-structs for Core 0 ↔ Core 1 communication

**Event signaling (C++):**
- Every dirty-flag setter also calls `app_events_signal(EVT_XXX)` (bit flags in `app_events.h`)
- Main loop: `app_events_wait(5)` wakes on any event, falls back to 5ms tick when idle
- 17 event bits assigned (bits 0-16), 7 spare; bits 24-31 reserved by FreeRTOS

**Example state pattern:**
```cpp
appState.wifi.ssid = "MyNetwork";
appState.setWifiDirty();
app_events_signal(EVT_WIFI);  // Explicit, or implicit via dirty flag setter

// Main loop
uint32_t bits = app_events_wait(5);
if (bits & EVT_WIFI) {
  // Handle WiFi change
  if (appState.isWifiDirty()) {
    // Broadcast new state to WebSocket
    sendWsUpdate("wifiStatus", appState.wifi);
    appState.clearWifiDirty();
  }
}
```

## Cross-Core Safety

**Core allocation (FreeRTOS):**
- **Core 1 (audio)**: `loopTask` (Arduino main loop, priority 1) + `audio_pipeline_task` (priority 3)
- **Core 0 (system)**: `gui_task`, `mqtt_task`, `usb_audio_task`, OTA tasks
- No new tasks ever pinned to Core 1

**Synchronization mechanisms:**
- Dirty flags + event signaling for Core 0 → Core 1 (push new config, audio task picks up on next iteration)
- Volatile fields for Core 1 → Core 0 (audio task sets `audioPaused`, main loop reads)
- Binary semaphore for I2S re-init: `appState.audio.taskPausedAck` — audio task acknowledges pause before dac deinit

**Example I2S re-init handshake (from `dac_hal.cpp`):**
```cpp
appState.audio.paused = true;  // Signal audio task to pause
// Wait for audio task to acknowledge (up to 50ms)
if (!xSemaphoreTake(appState.audio.taskPausedAck, pdMS_TO_TICKS(50))) {
  LOG_W("[DAC] Audio task did not pause in time");
}
i2s_driver_uninstall(...);  // Safe — audio task is paused
```

---

*Convention analysis: 2026-03-21*
