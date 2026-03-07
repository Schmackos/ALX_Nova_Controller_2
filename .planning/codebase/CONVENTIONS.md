# Coding Conventions

**Analysis Date:** 2026-03-07

## Naming Patterns

**Files:**
- C++ source pairs: `snake_case.h` / `snake_case.cpp` — e.g., `smart_sensing.h`, `hal_device_manager.cpp`
- HAL files grouped in `src/hal/` subdirectory with `hal_` prefix — e.g., `hal_types.h`, `hal_pipeline_bridge.cpp`
- GUI files in `src/gui/` with `gui_` prefix — e.g., `gui_manager.cpp`, `gui_navigation.cpp`
- JavaScript modules: `NN-kebab-case.js` numbered for load order — e.g., `01-core.js`, `15-hal-devices.js`
- Test files: one directory per module, file named `test_<module>.cpp` or `test_<module>_<topic>.cpp`

**Functions (C++):**
- Public API functions: `snake_case` matching module prefix — e.g., `audio_pipeline_set_sink()`, `hal_pipeline_on_device_available()`
- Class methods: `camelCase` — e.g., `mgr->registerDevice()`, `dev->healthCheck()`
- Static file-scope helpers: `camelCase` with leading underscore or descriptive name — e.g., `audioHealthName()`, `fftWindowName()`
- HTTP handlers: `handle<Subject><Verb>` — e.g., `handleSmartSensingGet()`, `handleSmartSensingUpdate()`
- HTTP API loaders/savers: `load<Subject>Settings()` / `save<Subject>Settings()` — e.g., `loadMqttSettings()`, `saveSmartSensingSettings()`
- Deferred save pattern: `save<Subject>SettingsDeferred()` + `checkDeferred<Subject>Save()` — e.g., `saveSmartSensingSettingsDeferred()`

**Variables (C++):**
- Module-level static state: `_camelCase` with underscore prefix — e.g., `_smoothedAudioLevel`, `_halSlotToSinkSlot[]`
- Module-level static previous-state trackers: `prev<Name>` — e.g., `prevMqttUptime`, `prevMqttHeapFree`
- AppState members: `camelCase` public — e.g., `appState.amplifierState`, `appState.audioThreshold_dBFS`
- Temporary/loop locals: descriptive camelCase — e.g., `modeStr`, `adcArr`

**Variables (JavaScript):**
- Global shared state: `camelCase` — e.g., `vuSegmentedMode`, `debugLogBuffer`, `audioChannelMap`
- Constants: `UPPER_SNAKE_CASE` — e.g., `WS_MIN_RECONNECT_DELAY`, `DEBUG_MAX_LINES`
- DOM element IDs (HTML): `camelCase` matching JS — e.g., `#wsConnectionStatus`, `#signalDetected`

**Types/Enums (C++):**
- Enum names: `PascalCase` — e.g., `AppFSMState`, `HalDeviceState`, `DspStageType`
- Enum values: `UPPER_SNAKE_CASE` with type prefix — e.g., `STATE_IDLE`, `HAL_STATE_AVAILABLE`, `DSP_BIQUAD_LPF`
- Struct names: `PascalCase` — e.g., `HalDeviceDescriptor`, `HalRetryState`, `HalBusRef`
- Classes: `PascalCase` — e.g., `HalDevice`, `HalDeviceManager`, `DebugSerial`
- Type aliases: `PascalCase` matching convention — e.g., `HalDeviceCallback`, `HalStateChangeCb`

**Constants/Defines (C++):**
- Build-flag constants: `UPPER_SNAKE_CASE` — e.g., `HAL_MAX_DEVICES`, `HAL_PRIORITY_BUS`
- Capability bit flags: `HAL_CAP_*` prefix — e.g., `HAL_CAP_HW_VOLUME`, `HAL_CAP_DAC_PATH`
- Rate mask constants: `HAL_RATE_*` prefix — e.g., `HAL_RATE_48K`

## Code Style

**Formatting:**
- No autoformatter config detected (no `.clang-format`). Style is consistent by convention.
- Indentation: 4 spaces in HAL/test files; 2 spaces in some older modules (inconsistency exists)
- Brace style: Allman/K&R mixed — HAL files use same-line `{`, older modules use new-line
- Max line length: ~100 characters observed

**Linting (JavaScript):**
- ESLint at `web_src/.eslintrc.json`
- Rules enforced: `no-undef`, `no-redeclare`, `eqeqeq: "smart"`
- All JS files concatenated into a single script scope — 380+ globals declared in the ESLint config
- JS source: ES2020, `sourceType: "script"` (not modules)

**Static Analysis (C++):**
- cppcheck runs on `src/` in CI (excludes `src/gui/`)
- `tools/find_dups.js` — detects duplicate JS declarations
- `tools/check_missing_fns.js` — detects undefined JS function references

## Header Guards

- Old headers: `#ifndef HEADER_NAME_H / #define HEADER_NAME_H / ... / #endif // HEADER_NAME_H`
- HAL headers: `#pragma once` (no traditional guards)
- Pattern example from `src/utils.h`:
  ```cpp
  #ifndef UTILS_H
  #define UTILS_H
  // ...
  #endif // UTILS_H
  ```

## Conditional Compilation

- Always guard hardware-specific code: `#ifdef DSP_ENABLED`, `#ifdef DAC_ENABLED`, `#ifdef USB_AUDIO_ENABLED`, `#ifdef GUI_ENABLED`
- Native test compatibility: `#ifdef NATIVE_TEST` with mock includes, `#else` with real Arduino includes
- Pattern across all modules:
  ```cpp
  #ifdef NATIVE_TEST
  #include "../test_mocks/Arduino.h"
  #else
  #include <Arduino.h>
  #endif
  ```
- Platform-specific inline stubs for native tests:
  ```cpp
  #ifdef NATIVE_TEST
  inline void audio_pipeline_notify_dsp_swap() {}
  #else
  void audio_pipeline_notify_dsp_swap();
  #endif
  ```

## Section Comments

- All header and source files use `// ===== Section Title =====` banners to group related declarations
- Test groups use the same pattern: `// ===== Group 1: Description (N tests) =====`
- Individual test functions begin with a comment describing the scenario (numbered for groups)
- Module-level explanation comments for non-obvious design decisions (e.g., "Why not slave mode")

## Import Organization

**C++ headers order:**
1. Module's own header (`.h` for `.cpp` files)
2. Other project headers (alphabetical or logical grouping)
3. Arduino framework and library headers (`<Arduino.h>`, `<ArduinoJson.h>`)
4. C++ standard library (`<cmath>`, `<cstring>`)
5. IDF-specific headers (`<LittleFS.h>`, `<driver/i2s_std.h>`)

**JavaScript modules:**
- Load order encoded in filename prefix (`01-core.js` → `28-init.js`)
- All files concatenated in order — shared scope, no `import`/`export`

## Error Handling

**C++ patterns:**
- Boolean return from init/config functions: `bool loadMqttSettings()`, `bool removeDevice(uint8_t slot)`
- Structured result type for HAL init: `HalInitResult` with `.success`, `.errorCode`, `.reason[48]`
  - Created with `hal_init_ok()` or `hal_init_fail(errorCode, "reason")`
- Early return on failure: check result immediately, log with `LOG_E`, return false/null
- No exceptions — this is embedded C++ with Arduino framework
- Null pointer defensively handled before use: `if (!key) return false;`, `if (!device) return;`

**JavaScript patterns:**
- Async/await with try/catch for fetch: `apiFetch()` wrapper handles 401 → redirect
- WS send wrapped in ready check: `ws && ws.readyState === WebSocket.OPEN` guard pattern

## Logging

**Framework:** `DebugSerial` class via `debug_serial.h` macros

**Macros:**
```cpp
LOG_D(fmt, ...)  // Debug — high-frequency operational details
LOG_I(fmt, ...)  // Info  — state transitions, significant events
LOG_W(fmt, ...)  // Warn  — unexpected but non-fatal
LOG_E(fmt, ...)  // Error — failures requiring attention
```

**Module prefix convention:** Every log line starts with `[ModuleName]`:
- `[Sensing]`, `[Audio]`, `[MQTT]`, `[WiFi]`, `[OTA]`, `[HAL]`, `[HAL Discovery]`, `[HAL DB]`, `[HAL API]`
- `[USB Audio]`, `[Buzzer]`, `[SigGen]`, `[Settings]`, `[GUI]`, `[OutputDSP]`

**Rules:**
- Use `LOG_I` for state transitions (connect/disconnect, start/stop, health changes)
- Use `LOG_D` for high-frequency per-cycle details — never in audio task hot path
- Use static `prev` variables to detect changes — log transitions, not repetitive state
- Never log inside ISR paths or `audio_pipeline_task` (UART TX blocks, causes audio dropouts)
- Dirty-flag pattern for audio task logging: task sets flag, main loop calls `audio_periodic_dump()`

## Module Design

**Singleton pattern:**
- `AppState::getInstance()` — Meyers singleton with `appState` macro alias
- `HalDeviceManager::instance()` — Meyers singleton with private constructor

**Exports:**
- C++ modules expose free functions declared in `.h`, implemented in `.cpp`
- No barrel files in C++ — include specific headers
- HAL devices implement `HalDevice` abstract class with five virtual methods: `probe()`, `init()`, `deinit()`, `dumpConfig()`, `healthCheck()`

**AppState dirty flags:**
- Every state change should set a dirty flag AND call `app_events_signal(EVT_XXX)`
- Main loop reads dirty flags to decide what to broadcast/save
- Never call WebSocket, LittleFS, or WiFi directly from MQTT callback — use dirty flags

**Deferred save pattern:**
```cpp
void saveSmartSensingSettingsDeferred();  // Mark dirty — actual save after 2s idle
void checkDeferredSmartSensingSave();     // Call from main loop
```

## Comments

**When to comment:**
- All public API functions in `.h` files get a brief doc comment
- Non-obvious design decisions get explanation comments (especially hardware quirks)
- Enum values get inline comments explaining purpose or constraint
- Magic numbers always commented — e.g., `// 40KB reserve`, `// 308ms time constant`

**No JSDoc/TSDoc used** — plain `//` and `/* */` comments throughout.

---

*Convention analysis: 2026-03-07*
