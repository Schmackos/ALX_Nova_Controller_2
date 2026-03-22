# Coding Conventions

**Analysis Date:** 2026-03-22

## Naming Patterns

**Files:**
- C++ headers: `.h` extension (e.g., `src/audio_pipeline.h`, `src/config.h`)
- C++ source: `.cpp` extension (e.g., `src/audio_pipeline.cpp`, `src/auth_handler.cpp`)
- Test modules: `test_<module_name>.cpp` in `test/test_<module_name>/` directory (e.g., `test/test_auth/test_auth_handler.cpp`)
- JavaScript modules: numeric prefix + hyphenated name (e.g., `01-core.js`, `02-ws-router.js`, `06-peq-overlay.js`)
- Web pages: `web_src/` contains source (never edit generated `src/web_pages.cpp` directly)

**Functions:**
- C++ functions: `snake_case` (e.g., `i2s_audio_init()`, `audio_derive_health_status()`, `audio_parse_24bit_sample()`)
- C++ class methods: `camelCase` (e.g., `getInstance()`, `clearDisplayDirty()`, `isSignalGenDirty()`)
- JavaScript functions: `camelCase` (e.g., `apiFetch()`, `initWebSocket()`, `handleBinaryMessage()`)
- Lifecycle/initialization: prefix pattern `init_` (e.g., `i2s_audio_init()`, `dsp_init()`)
- Getter/setter: `is` / `get` / `set` prefix (e.g., `isDisplayDirty()`, `getLogLevel()`, `setBacklightBrightness()`)
- Test functions (Unity): `test_<description>()` with underscores (e.g., `test_lpf_coefficients()`, `test_save_single_network()`)

**Variables:**
- C++ member variables: `_camelCase` with underscore prefix (e.g., `_displayDirty`, `_minLevel`, `_webSocket`)
- C++ statics/globals: `camelCase` (e.g., `currentWifiConnected`, `appState`)
- JavaScript globals: `camelCase` (e.g., `ws`, `currentActiveTab`, `darkMode`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `LED_PIN`, `MAX_BUFFER`, `FLOAT_TOL`)
- Loop counters: single letters (`i`, `j`, `k`)
- Boolean variables/flags: prefix with `is`, `has`, `can`, or `enable` (e.g., `_adcEnabledDirty`, `hadPreviousConnection`)

**Types:**
- Structs: `PascalCase` (e.g., `GeneralState`, `OtaState`, `AudioInputSource`)
- Enums: `PascalCase` for type, `UPPER_SNAKE_CASE` for values (e.g., `enum LogLevel { LOG_DEBUG, LOG_INFO, ... }`)
- Aliases: `using HalEepromData = DacEepromData;` (in `src/hal/hal_eeprom.h`)
- Bitfields: `HAL_CAP_*` constants as bit positions (e.g., `HAL_CAP_HW_VOLUME` = 0, `HAL_CAP_FILTERS` = 1)

## Code Style

**Formatting:**
- **Indentation**: 4 spaces per level (verified in platformio.ini and source files)
- **Line length**: No hard limit enforced; typical files stay under 120 characters
- **Braces**: K&R style (opening brace on same line, closing on new line)
  ```cpp
  if (condition) {
      // code
  } else {
      // code
  }
  ```
- **Spacing**: Single space after keywords (`if`, `while`, `for`)
- **Semicolons**: Terminate all statements (no implicit returns in C++)

**Linting:**
- **ESLint** (`web_src/.eslintrc.json`): Browser ES2020 environment
  - Rules: `no-undef` (error), `no-redeclare` (error), `eqeqeq: smart` (error)
  - 380 globals declared for concatenated scope (shared JS context)
  - No formatter configured (code style enforced by convention)
- **cppcheck**: Static analysis on `src/` (CI/CD, excludes `src/gui/`)
- **No C++ formatter enforced**: Follow existing patterns in the codebase

## Import Organization

**C++ Include Order:**
1. System headers (`<Arduino.h>`, `<unity.h>`, `<cstring>`)
2. Third-party headers (`<ArduinoJson.h>`, `<WebSocketsServer.h>`)
3. Project headers (`"config.h"`, `"state/audio_state.h"`)

Example from `test/test_auth/test_auth_handler.cpp`:
```cpp
#include <cstring>
#include <string>
#include <unity.h>
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
```

**JavaScript Module Loading:**
- Concatenated in filename order (01-core.js loads first, then 02-ws-router.js, etc.)
- All functions and state declared in global scope (shared context across 28 files)
- No `import`/`export` statements (not using ES modules)
- Dependency on load order (e.g., `01-core.js` must load before `02-ws-router.js`)
- Common patterns: `01-core.js` (core API/fetch), `02-ws-router.js` (message routing), `03-app-state.js` (state tracking)

**Path Aliases:**
- None in C++ (use relative includes within project structure)
- Frontend scripts use direct global function calls (no module system)

## Error Handling

**C++ Patterns:**
- **Boolean returns for errors**: Functions return `bool` when success/failure is the primary concern
  - Return `true` on success, `false` on failure
  - Caller responsible for logging the failure via `LOG_W()` or `LOG_E()`
  - Example: `bool i2s_audio_set_sample_rate(uint32_t rate);`
- **Pointer returns for nullable data**: Functions return `nullptr` when data not available
  - Example: `const AudioInputSource* audio_pipeline_get_source(int lane);`
  - Caller checks `if (!ptr)` before using
- **No exceptions**: Arduino/embedded context; all error handling via return codes
- **Validation pattern**: Check inputs immediately, return early on invalid state
  ```cpp
  if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return false;
  if (!src) return false;
  if (lane * 2 + 1 >= AUDIO_PIPELINE_MATRIX_SIZE) return false;
  ```
- **HTTP status codes**: Error responses use appropriate HTTP codes
  - `400` Bad Request (invalid input)
  - `401` Unauthorized (auth failure)
  - `409` Conflict (race condition, e.g., scan already in progress)
  - `429` Too Many Requests (rate limiting)
  - `503` Service Unavailable (resource exhausted, e.g., toggle queue overflow)

**JavaScript Patterns:**
- **Fetch wrapper with `safeJson()`**: All `apiFetch()` calls use `response.safeJson()` before parsing
  - Handles non-JSON responses gracefully
  - Throws on parse failure with `Error('Invalid response format')`
  - Example: `const data = await resp.safeJson();`
- **401 Unauthorized**: `apiFetch()` redirects to `/login` on 401 response
- **Message validation**: `validateWsMessage(data, requiredFields)` checks required WS fields before processing
  - Returns `false` if any required field is undefined
  - Logs warning to console
- **Graceful degradation**: UI features degrade if WS/API fails (e.g., waveform still renders without data)

## Logging

**Framework:** Arduino `Serial` class with `DebugSerial` wrapper (`src/debug_serial.h`)

**Log Levels:**
- `LOG_DEBUG` (0): Detailed debugging info
- `LOG_INFO` (1): General information
- `LOG_WARN` (2): Warnings
- `LOG_ERROR` (3): Errors
- `LOG_NONE` (4): Suppress all serial output

**Macros:**
- `LOG_D(fmt, ...)` — Debug level (high-frequency operational details)
- `LOG_I(fmt, ...)` — Info level (state transitions, significant events)
- `LOG_W(fmt, ...)` — Warn level (recoverable errors, unusual conditions)
- `LOG_E(fmt, ...)` — Error level (critical failures, unrecoverable errors)

**Patterns:**
- **Module prefixes in brackets**: `[ModuleName]` at start of message (e.g., `[Auth]`, `[Audio]`, `[Sensing]`)
  - Frontend extracts module name to enable category filtering
  - Standard prefixes: `[Audio]`, `[Sensing]`, `[SigGen]`, `[Buzzer]`, `[WiFi]`, `[MQTT]`, `[OTA]`, `[Settings]`, `[USB Audio]`, `[GUI]`, `[HAL]`, `[OutputDSP]`
- **Never log in ISR/audio task**: Serial.print blocks on UART, starves DMA
  - Use dirty-flag pattern: task sets flag, main loop calls `audio_periodic_dump()` for actual output
- **Transition logging**: Log when state changes, not every periodic tick
  - Use static `prev` variables to detect changes and avoid log spam
- **Rate-limited logs**: HAL discovery logs "retrying" with increasing backoff; success logged once via `DIAG_*` events

## Comments

**When to Comment:**
- Non-obvious algorithm or complex business logic
- Why a workaround exists (link to issue/commit if possible)
- Preconditions/postconditions for critical functions
- Rationale for performance-critical choices (e.g., PSRAM fallback, heap guards)
- Do NOT comment obvious code (e.g., `i++` in a loop)

**JSDoc/TSDoc:**
- Used sparingly in test files and header functions
- Example (from `test_dsp.cpp`):
  ```cpp
  // Tolerance for float comparisons
  #define FLOAT_TOL 0.001f

  void setUp(void) {
      dsp_init();
  }
  ```
- No formal JSDoc block style; inline comments preferred for clarity

**Header Comments:**
- File purpose at top: `#ifndef`, `#define`, blank, comments
- Example from `src/config.h`:
  ```cpp
  #ifndef CONFIG_H
  #define CONFIG_H

  #include <Arduino.h>

  // ===== Device Information =====
  #define MANUFACTURER_NAME "ALX Audio"
  ```

## Function Design

**Size:**
- Aim for functions under 50 lines (when practical)
- Large implementations broken into smaller helpers
- Example: `audio_pipeline.cpp` splits codec logic, routing, and DSP into separate functions

**Parameters:**
- Pass by value for primitives (`int`, `float`, `bool`)
- Pass by const reference for large objects/structs (`const AudioInputSource &src`)
- Optional parameters use defaults: `bool i2s_audio_get_waveform(uint8_t *out, int adcIndex = 0);`
- Pointer parameters indicate nullable or out-parameters: `float* bands` (out), `size_t* bytes_read` (out)

**Return Values:**
- Prefer `bool` for success/failure operations
- Use `nullptr` for nullable pointers
- Return structures by value (compiler optimizations apply)
- No out-parameters for simple types (use return value instead)

**Signature Pattern:**
```cpp
// ADC read API: standard signature across implementations
bool i2s_audio_read_adc1(void *buf, size_t size, size_t *bytes_read, uint32_t timeout_ms);

// Getter: simple bool return
bool i2s_audio_adc2_ok();

// Analysis access: const reference to volatile data
AudioAnalysis i2s_audio_get_analysis();

// Optional parameter with default
bool i2s_audio_get_waveform(uint8_t *out, int adcIndex = 0);
```

## Module Design

**Exports:**
- Header `.h` files declare public API (functions, types, constants)
- Implementation `.cpp` files contain static helper functions (not in header)
- Singleton classes use `getInstance()` static method: `AppState::getInstance()`
- Module initialization: `init_` prefix function called once at boot

**Barrel Files:**
- Not used (no single header re-exports everything)
- Each module includes only what it needs

**State Management:**
- Decomposed into domain-specific headers in `src/state/` (15 total)
- Composed into `AppState` singleton for centralized access
- Dirty flags for change detection: `_adcEnabledDirty`, `_displayDirty`, `_otaDirty`
- Event signaling: dirty flag setter calls `app_events_signal(EVT_*)` for FreeRTOS wake-up

**Struct Field Access:**
- Public members in state structs (no getters for trivial access)
- Example: `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`, `appState.general.darkMode`
- Toggle operations: `appState.halCoord.requestDeviceToggle(halSlot, action)` (single path, validates return)

---

*Convention analysis: 2026-03-22*
