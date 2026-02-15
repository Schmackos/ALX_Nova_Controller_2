# Coding Conventions

**Analysis Date:** 2026-02-15

## Naming Patterns

**Files:**
- `snake_case.h` / `snake_case.cpp` for implementation files
- Headers use `#ifndef HEADER_H` / `#define HEADER_H` / `#endif` guards
- GUI screens: `scr_<name>.h/.cpp` (e.g., `scr_home.h`, `scr_settings.cpp`)
- Driver modules: `dac_<type>.h/.cpp` (e.g., `dac_pcm5102.h`)
- DSP modules: `dsp_<subsystem>.h/.cpp` (e.g., `dsp_pipeline.h`, `dsp_coefficients.cpp`)
- Test files: `test_<module>.cpp` in dedicated test directories

**Functions:**
- `camelCase` for function names (e.g., `detectSignal()`, `setAmplifierState()`, `createSession()`)
- Public API functions: descriptive names like `loadSmartSensingSettings()`, `sendSmartSensingState()`
- Boolean-returning functions: prefix with `is`, `has`, or similar (e.g., `isAPMode`, `isFSMStateDirty()`)
- Handler functions: `handle<Action>` or `handle<Module><Action>` (e.g., `handleSmartSensingGet()`, `handleSmartSensingUpdate()`)
- Static/local helper functions: descriptive names in implementation (e.g., `getWiFiDisconnectReason()`)

**Variables:**
- Class members: `camelCase` with underscore prefix for private members (e.g., `_webSocket`, `_minLevel`, `_lineBuffer`)
- Local variables: `camelCase` (e.g., `pendingConnection`, `wifiStatusUpdateRequested`)
- Constants/Enums: `SCREAMING_SNAKE_CASE` or `camelCase` (enum values use `camelCase`, e.g., `LOG_DEBUG`, `STATE_IDLE`)
- Boolean variables: descriptive names like `isAPMode`, `wifiConnected`, `amplifierState`
- Module-scoped statics: lowercase snake_case with descriptive name (e.g., `wifiDisconnected`, `wifiScanInProgress`, `lastReconnectAttempt`)

**Types:**
- Classes/Structs: `PascalCase` (e.g., `AppState`, `ButtonHandler`, `DebugSerial`)
- Enums: `PascalCase` for enum type (e.g., `AppFSMState`, `LogLevel`, `FftWindowType`)
- Enum values: `SCREAMING_SNAKE_CASE` (e.g., `STATE_IDLE`, `LOG_ERROR`, `FFT_WINDOW_HANN`)
- Type aliases: `camelCase` with descriptive suffix (e.g., `WiFiConnectionRequest`)
- Struct sub-types: `PascalCase` (e.g., `AdcState`, `I2sRuntimeMetrics`)

## Code Style

**Formatting:**
- No enforced formatter detected in codebase
- Manual formatting conventions observed:
  - 2-space indentation throughout
  - Opening braces on same line: `if (...) {`
  - Class members grouped by category with comment headers: `// ===== Category Name =====`
  - Section separators: `// ===== SECTION NAME =====` (all caps, equals signs)
  - Blank lines between logical sections within functions

**Linting:**
- No ESLint or ClangFormat config detected
- Code follows consistent Arduino/C++ style conventions
- Enum declarations use colons for explicit types: `enum LogLevel : uint8_t { ... }` or `enum FftWindowType : uint8_t { ... }`

**Includes:**
- Order typically:
  1. Standard C/C++ headers (`<cstring>`, `<math.h>`, `<string>`)
  2. Arduino headers (`<Arduino.h>`)
  3. Third-party library headers (`<ArduinoJson.h>`, `<WebSocketsServer.h>`, `<WiFi.h>`)
  4. Custom local headers (quoted: `"app_state.h"`)
- Conditional includes guarded by preprocessor flags (e.g., `#ifdef DSP_ENABLED`, `#ifdef GUI_ENABLED`)

## Import Organization

**Order:**
1. Standard library includes
2. Platform/framework includes (`<Arduino.h>`, `<WiFi.h>`)
3. External library includes (JSON, WebSockets, MQTT, etc.)
4. Local module headers (relative paths in quotes)
5. DSP/DAC/GUI headers (conditionally compiled)

**Path Aliases:**
- No path aliases (no `jsconfig.json` or similar) — direct relative includes
- Some forward declarations in header files for circular dependency avoidance
- Example: `src/wifi_manager.cpp` includes `"app_state.h"` directly, not via alias

## Error Handling

**Patterns:**
- Return-based error codes: Functions return `bool` for success/failure (e.g., `createSession()`, `detectSignal()`)
- Error messages stored in AppState: `wifiConnectError`, `otaStatusMessage` for user-facing errors
- Serial logging for diagnostics: `LOG_E()` macro for error-level messages
- Graceful degradation: Failed allocations check heap and fall back (e.g., DSP delay lines fall back from PSRAM to heap with pre-flight check)
- No exceptions: C++ exceptions not used; errors propagated via return values or state flags

**Exception Handling:**
- Not used; all error handling via return codes, state flags, and logging
- Watchdog protection: WDT configured with 15s timeout, monitored via `esp_task_wdt_init()`

**State Validation:**
- Dirty flags used to detect changes (e.g., `isFSMStateDirty()`, `isLedStateDirty()`)
- Validation occurs before persistence: `loadPasswordFromPrefs()` checks key existence before reading
- Preference migrations handled inline (e.g., legacy plaintext → SHA256 hash migration in auth handler)

## Logging

**Framework:** Custom `DebugSerial` class in `src/debug_serial.h/.cpp`

**Patterns:**
- Macros: `LOG_D()`, `LOG_I()`, `LOG_W()`, `LOG_E()` for debug, info, warn, error levels
- Module prefixes in brackets: `[ModuleName]` at start of message (e.g., `[WiFi]`, `[Audio]`, `[Sensing]`)
- Level filtering: `currentLogLevel` global gate; master `debugMode` in AppState forces LOG_ERROR when disabled
- Real-time filtering: `applyDebugSerialLevel()` applies master + sublevel settings at runtime
- WebSocket forwarding: Log messages sent to connected web clients via `broadcastLine()`
- High-frequency tasks: NO serial logging in real-time paths (e.g., `audio_capture_task`). Use dirty-flag pattern: task sets flag, main loop calls `audio_periodic_dump()` for batch output

## Comments

**When to Comment:**
- Section headers: `// ===== SECTION NAME =====` (all caps, equals signs on both sides)
- Complex logic: Explain WHY, not WHAT (e.g., "Timing-safe comparison prevents timing attacks")
- Algorithm choices: Document rationale (e.g., "Radix-4 FFT via ESP-DSP for speed optimization")
- Gotchas and workarounds: Flag non-obvious solutions (e.g., "ESP32-S3 I2S slave mode has DMA issues — must use master mode with GPIO matrix routing")
- TODO/FIXME: `// TODO:` or `// FIXME:` with context (e.g., `// TODO: Remove these legacy extern aliases once all handlers use appState directly`)
- Avoid redundant comments: Don't repeat what the code already expresses

**JSDoc/TSDoc:**
- Not used in this C++ codebase
- Function documentation via header comments above declarations:
  ```cpp
  // Generate cryptographically random session ID (UUID format)
  String generateSessionId();

  // Create a new session; returns true if successful, false if eviction needed
  bool createSession(String &sessionId);
  ```

## Function Design

**Size:**
- Functions typically 30-100 lines
- DSP pipeline handlers larger (150+ lines) for complex stage processing
- Handler functions tend to be 50-200 lines (HTTP/WebSocket/MQTT handlers)
- Helper functions kept under 30 lines

**Parameters:**
- Passed by value for primitives (`bool`, `int`, `float`)
- Passed by reference for objects to avoid copies (`const String &`, `String &`)
- Output parameters use references (e.g., `String &sessionId` for output from `createSession()`)
- Variadic functions use `va_list` for logging (e.g., `logWithLevel(LogLevel level, const char *format, va_list args)`)

**Return Values:**
- `void` for setters and state mutations
- `bool` for success/failure
- `String` for text results
- `float` for numerical results (e.g., audio levels)
- Enum values for state queries (e.g., `AppFSMState`, `AudioHealthStatus`)
- Const references for efficiency: `const String &`, `const uint8_t *`

## Module Design

**Exports:**
- Header files define public API as function declarations
- Private implementation details in `.cpp` files with static functions
- Module initialization: `moduleName_init()` or module-specific setup function (e.g., `i2s_audio_init()`, `dac_hal_init()`)
- State management via AppState singleton for cross-module communication

**Barrel Files:**
- Not used; each module has its own header file
- App state is centralized via `AppState` singleton in `app_state.h`
- Main loop in `main.cpp` orchestrates module function calls

**Conditional Compilation:**
- Entire modules guarded by feature flags: `-D GUI_ENABLED`, `-D DAC_ENABLED`, `-D DSP_ENABLED`, `-D USB_AUDIO_ENABLED`
- Header includes wrapped: `#ifdef GUI_ENABLED ... #endif`
- Build flags in `platformio.ini` define availability

## Preprocessor & Macros

**Common Patterns:**
- Configuration macros: Defined in `src/config.h` and overridable via build flags
- Pin definitions: Via `-D LED_PIN=2` build flags with fallback defaults in config.h
- Logging macros: `LOG_D()`, `LOG_I()`, `LOG_W()`, `LOG_E()` provide consistent interface
- Legacy compatibility: `#define wifiSSID appState.wifiSSID` for gradual migration to direct member access
- Guard macros: `#ifndef UNIT_TEST`, `#ifdef NATIVE_TEST`, `#ifdef DSP_ENABLED` for conditional compilation

**Avoid:**
- Function-like macros for complex logic (use inline functions)
- Macro constants without uppercase naming (SCREAMING_SNAKE_CASE enforced for `#define` constants)

## Concurrency & Thread Safety

**FreeRTOS Task Safety:**
- Shared state protected via volatile flags: `appState.audioPaused`, `heapCritical`
- Cross-core communication uses dirty flags (main loop reads, tasks write)
- Mutexes for hardware access: `buzzer_handler` uses mutex for dual-core safety
- Critical sections: `esp_task_wdt_delete()` in setup to prevent IDLE task watchdog
- No direct locking on AppState reads — dirty-flag pattern is sufficient for WiFi/MQTT state

**Audio Real-Time Constraints:**
- No Serial/LOG calls in `audio_capture_task` (Core 1, priority 3)
- `audioPaused` volatile flag gates I2S reads during driver reinit
- Main loop staggers broadcasts (5ms delay, 5s WiFi check throttle) to prevent blocking

---

*Convention analysis: 2026-02-15*
