# Coding Conventions

**Analysis Date:** 2026-03-10

## Naming Patterns

**Files (C++ firmware):**
- Module files: `snake_case.cpp` / `snake_case.h` (e.g., `smart_sensing.cpp`, `audio_pipeline.cpp`, `dac_hal.cpp`)
- HAL device drivers: `hal_<device>.h/.cpp` (e.g., `hal_pcm5102a.h`, `hal_es8311.cpp`, `hal_ns4150b.cpp`)
- HAL framework files: `hal_<subsystem>.h/.cpp` (e.g., `hal_device_manager.h`, `hal_pipeline_bridge.h`, `hal_discovery.h`)
- State headers: `src/state/<domain>_state.h` (e.g., `audio_state.h`, `wifi_state.h`, `dac_state.h`)
- Driver headers: `src/drivers/<chip>_regs.h` (e.g., `es8311_regs.h`)
- Config header: `src/config.h` (all compile-time constants)
- Auto-generated files: `web_pages.cpp`, `web_pages_gz.cpp` -- NEVER edit manually

**Files (JavaScript web frontend):**
- Numbered prefix for load order: `NN-<module>.js` (e.g., `01-core.js`, `05-audio-tab.js`, `22-settings.js`)
- Sub-numbered variants: `06-peq-overlay.js`, `27a-health-dashboard.js`
- CSS: `NN-<concern>.css` (e.g., `01-variables.css`, `03-components.css`, `05-responsive.css`)

**Files (E2E tests):**
- Spec files: `<feature>.spec.js` in `e2e/tests/` (e.g., `auth.spec.js`, `control-tab.spec.js`)
- Helpers: `e2e/helpers/<purpose>.js` (e.g., `fixtures.js`, `ws-helpers.js`, `selectors.js`)
- Fixtures: `e2e/fixtures/ws-messages/<msg-type>.json`, `e2e/fixtures/api-responses/<endpoint>.json`

**Functions (C++):**
- Public module APIs: `snake_case` with module prefix (e.g., `audio_pipeline_set_sink()`, `dac_volume_to_linear()`, `dsp_compute_biquad_coeffs()`)
- HTTP handlers: `handleCamelCase()` (e.g., `handleSmartSensingGet()`, `handleWiFiConfig()`)
- WS broadcast: `sendCamelCase()` (e.g., `sendDacState()`, `sendHalDeviceState()`)
- Init/deinit: `<module>_init()` / `<module>_deinit()` (e.g., `buzzer_init()`, `siggen_deinit()`)
- HAL lifecycle: `probe()`, `init()`, `deinit()`, `healthCheck()`, `dumpConfig()` (virtual overrides)
- Inline helpers: `pipeline_int32_to_float()`, `dsp_is_biquad_type()` (snake_case with module prefix)

**Functions (JavaScript):**
- camelCase: `initWebSocket()`, `updateConnectionStatus()`, `apiFetch()`, `routeWsMessage()`
- Tab switching: `switchTab('tabName')`
- WS send: `wsSend(jsonObject)`

**Variables (C++):**
- Local/static: `camelCase` (e.g., `mockAudioLevel_dBFS`, `_smoothedAudioLevel`)
- Private members: underscore prefix `_camelCase` (e.g., `_ready`, `_state`, `_volume`, `_muteRampState`)
- AppState domain access: `appState.domain.field` (e.g., `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`)
- Constants/defines: `UPPER_SNAKE_CASE` (e.g., `HAL_MAX_DEVICES`, `AUDIO_PIPELINE_MAX_INPUTS`, `EVT_DAC`)
- Pin defines: `<FUNCTION>_PIN` (e.g., `LED_PIN`, `I2S_BCK_PIN`, `BUZZER_PIN`)

**Variables (JavaScript):**
- camelCase for mutable state: `currentWifiConnected`, `wsReconnectDelay`
- UPPER_SNAKE_CASE for constants: `WS_MIN_RECONNECT_DELAY`, `WS_MAX_RECONNECT_DELAY`

**Types/Structs (C++):**
- Classes: `PascalCase` (e.g., `HalDeviceManager`, `HalPcm5102a`, `AppState`, `DebugSerial`)
- Structs: `PascalCase` (e.g., `AudioOutputSink`, `HalDeviceConfig`, `AdcState`, `DspBiquadParams`)
- Enums: `PascalCase` type, `UPPER_SNAKE_CASE` values (e.g., `enum HalDeviceState : uint8_t { HAL_STATE_UNKNOWN, ... }`)
- Typed enums: Use `enum Name : uint8_t` for storage efficiency and type safety

## Code Style

**Formatting:**
- No enforced auto-formatter (no `.prettierrc` or `.clang-format` detected)
- Indentation: 4 spaces in C++, 8 spaces in JS (all web_src JS files use 8-space indent)
- Brace style: K&R (opening brace on same line) for functions and control flow in C++
- Max line width: Not enforced, but most lines stay under ~120 chars

**Linting:**
- **C++**: cppcheck (`--enable=warning,performance --suppress=missingInclude --suppress=unusedFunction --suppress=badBitmaskCheck --std=c++11 --error-exitcode=1 -i src/gui/`). GUI sources excluded from cppcheck
- **JavaScript**: ESLint with 380+ globals for concatenated scope. Rules: `no-undef` (error), `no-redeclare` (error), `eqeqeq` (smart). Config: `web_src/.eslintrc.json`
- **JS duplicate detection**: `node tools/find_dups.js` (no duplicate `let`/`const` across files since all JS shares one scope)
- **JS missing function check**: `node tools/check_missing_fns.js`

**Conditional Compilation:**
- Use `#ifdef NATIVE_TEST` / `#ifdef UNIT_TEST` for test-only code paths
- Feature guards: `#ifdef DAC_ENABLED`, `#ifdef DSP_ENABLED`, `#ifdef USB_AUDIO_ENABLED`, `#ifdef GUI_ENABLED`
- Pattern for test/real includes:
```cpp
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif
```

## Import Organization

**C++ header order:**
1. Own header (e.g., `#include "smart_sensing.h"`)
2. Project headers (`#include "app_state.h"`, `#include "config.h"`, `#include "debug_serial.h"`)
3. HAL headers (`#include "hal/hal_device_manager.h"`)
4. Third-party / system headers (`#include <ArduinoJson.h>`, `#include <LittleFS.h>`)
5. Standard C/C++ headers (`#include <cmath>`, `#include <stdint.h>`)

**C++ header guards:**
- Traditional `#ifndef` / `#define` / `#endif` guards for most files: `#ifndef CONFIG_H` / `#define CONFIG_H`
- `#pragma once` used in newer HAL framework headers (`hal_types.h`, `hal_device.h`, `hal_device_manager.h`)
- Use `#pragma once` for all new HAL and state headers; use `#ifndef` guards for legacy modules

**JavaScript imports:**
- No module imports (no `import`/`require`) -- all JS files are concatenated into a single scope
- File numbering (01-28) determines load order; dependencies must be in lower-numbered files
- When adding a new top-level `let`/`const`/`function`, add it to `web_src/.eslintrc.json` globals

**Path aliases:**
- None in C++ (relative paths with `../` for cross-directory includes)
- HAL includes from src: `#include "hal/hal_device_manager.h"`
- State includes: `#include "state/audio_state.h"`

## Error Handling

**HAL device init:**
- Use `HalInitResult` struct: `hal_init_ok()` on success, `hal_init_fail(DIAG_HAL_INIT_FAILED, "reason")` on failure
- Callers check `.success` field (not bool conversion)
- Error codes defined in `src/diag_error_codes.h`
```cpp
HalInitResult init() override {
    if (failure_condition) {
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "I2S TX enable failed");
    }
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    return hal_init_ok();
}
```

**API/HTTP error responses:**
- Return JSON with `{"success": false, "message": "..."}` or `{"error": "..."}` on failure
- Use HTTP status codes: 200 (ok), 400 (bad request), 401 (unauthorized), 409 (conflict), 429 (rate limit), 503 (service unavailable)

**DSP pipeline errors:**
- `dsp_add_stage()` rolls back on pool exhaustion
- Config swap returns HTTP 503 on failure
- Double-buffered config with atomic swap for glitch-free updates

**Null/bounds checking:**
- Always bounds-check array indices before access
- HAL slot API: silently ignores out-of-range slots (`slot >= AUDIO_OUT_MAX_SINKS`)
- Pipeline source/sink APIs: NULL check before dereference, return safe defaults (-90.0f for VU)

## Logging

**Framework:** Custom `DebugSerial` class wrapping `Serial` with WebSocket forwarding

**Macros:** Use `LOG_D`, `LOG_I`, `LOG_W`, `LOG_E` (defined in `src/debug_serial.h`)

**Module prefix convention:**
```cpp
LOG_I("[HAL:PCM5102A] Initializing (sr=%luHz bits=%u)", (unsigned long)_sampleRate, _bitDepth);
LOG_W("[WiFi] Connection timeout after %lums", timeout);
LOG_E("[Audio] I2S read failed with error %d", err);
```

**When to use each level:**
- `LOG_I`: State transitions, init/deinit, connect/disconnect, significant events
- `LOG_D`: High-frequency operational details, parameter snapshots, periodic dumps
- `LOG_W`: Recoverable issues, deprecated code paths, configuration warnings
- `LOG_E`: Unrecoverable errors, hardware failures, assertion violations

**Critical rules:**
- NEVER log inside ISR paths or the `audio_pipeline_task` (Core 1) -- `Serial.print` blocks on UART TX buffer full, causing DMA starvation and audio dropouts
- Use dirty-flag pattern: task sets flag, main loop calls dump function for actual output
- Log transitions, not repetitive state -- use static `prev` variables to detect changes

## Comments

**Section headers:** Use `// ===== Section Name =====` for major sections in both .h and .cpp files:
```cpp
// ===== Smart Sensing Core Functions =====
// ===== WebSocket Event Handler =====
// ===== Test Setup/Teardown =====
```

**Inline comments:** Use `//` for single-line explanations of non-obvious logic:
```cpp
// Smooth audio level for stable signal detection (EMA, alpha=0.15, tau~308ms)
// DEPRECATED v1.14: flat fields -- use adc[] array above
```

**Deprecation markers:** Use `// DEPRECATED` with version and replacement guidance

**JSDoc/Doxygen:**
- Minimal JSDoc in JavaScript (`/** ... */` for key helper functions in test helpers)
- C++ uses `/** */` sparingly in `src/utils.h` for public API documentation
- HAL headers use `//` comments for brief purpose descriptions inline with declarations

## Function Design

**Size:** Most functions are under 100 lines. Largest modules split across multiple files (e.g., MQTT split into `mqtt_handler.cpp` ~1120 lines + `mqtt_publish.cpp` ~948 lines + `mqtt_ha_discovery.cpp` ~1898 lines)

**Parameters:**
- Use `const` references for non-modified structs: `const WiFiNetworkConfig &config`
- Use pointers for output parameters: `AudioOutputSink* out`
- Use `uint8_t slot` for HAL/pipeline slot indices
- Use `int lane` for audio pipeline input lane indices

**Return values:**
- `bool` for simple success/failure
- `HalInitResult` for HAL init (carries error code + reason string)
- `-1` or `nullptr` for "not found" lookups
- Void for fire-and-forget operations (broadcast, save)

## Module Design

**Exports (C++):**
- Each module has a `.h` file declaring its public API
- Implementation in matching `.cpp` file
- File-scope `static` for internal helpers and state (no anonymous namespaces in production code)
- `extern` declarations for shared globals in `src/globals.h`

**Singleton pattern:**
- `AppState`: Meyers singleton via `static AppState& getInstance()`, accessed via `#define appState AppState::getInstance()`
- `HalDeviceManager`: Meyers singleton via `static HalDeviceManager& instance()`
- Both delete copy/move constructors

**Barrel files:** Not used. Each module includes what it needs directly.

**Dirty flag pattern for cross-task coordination:**
```cpp
// Setter (called from any task/ISR):
void markDacDirty() { _dacDirty = true; app_events_signal(EVT_DAC); }

// Consumer (main loop only):
if (appState.isDacDirty()) {
    appState.clearDacDirty();
    sendDacState();
}
```

**Cross-core volatile fields:**
- Use `volatile bool` for flags read across cores (e.g., `volatile bool _ready`, `volatile bool audioPaused`)
- Use `volatile HalDeviceState _state` for lock-free reads from audio task on Core 1
- FreeRTOS semaphores for deterministic handshakes (e.g., `audioTaskPausedAck`)

## Web Frontend Conventions

**Concatenated scope:**
- All JS files share one global scope after concatenation
- No `let`/`const` name collisions across files -- verify with `node tools/find_dups.js`
- Every top-level declaration must be registered in `web_src/.eslintrc.json` globals

**DOM ID convention:**
- `camelCase` IDs matching AppState field paths: `#amplifierStatus`, `#signalDetected`, `#wsConnectionStatus`
- Settings inputs: `#appState\\.fieldName` (escaped dot for CSS selectors)
- Panel IDs match sidebar `data-tab` values: `#control`, `#audio`, `#wifi`, `#mqtt`, `#settings`, `#firmware`, `#debug`, `#support`

**Icons:**
- All icons use inline SVG paths from Material Design Icons (MDI) -- no external CDN or font library
- Pattern: `<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true"><path d="..."/></svg>`
- Use `fill="currentColor"` for CSS color inheritance

**Build pipeline for web assets:**
- Edit source in `web_src/` only
- Run `node tools/build_web_assets.js` to regenerate `src/web_pages.cpp` and `src/web_pages_gz.cpp`
- NEVER edit generated files directly

---

*Convention analysis: 2026-03-10*
