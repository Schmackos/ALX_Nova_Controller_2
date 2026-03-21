# Coding Conventions

**Analysis Date:** 2026-03-21

## Naming Patterns

**Files:**
- C++ source: `src/module_name.cpp` + `src/module_name.h` (snake_case)
- Headers: `src/subdir/hal_device_name.h` (consistent with directory grouping)
- Test files: `test/test_module_name/test_*.cpp` (one test per directory to avoid duplicate main symbols)
- HAL drivers: `src/hal/hal_compatible_name.cpp/.h` (e.g., `hal_pcm5102a.h`)
- Web files: `web_src/js/NN-description.js` (numeric order prefix for load order), `web_src/css/NN-*.css` (numeric split)
- E2E tests: `e2e/tests/feature-name.spec.js` (kebab-case)

**Functions (C++):**
- Public functions: `camelCase()` with module prefix for HAL devices: `HalPcm5102a::init()`, `HalPcm5102a::setVolume()`
- Private file-scoped functions: `snake_case()` with `static` prefix in CPP only (not in headers)
- Handler callbacks: `handleModuleAction()` (e.g., `handleSmartSensingGet()`, `handleSmartSensingUpdate()`)
- Accessor functions: `get_/set_` or `isReady()`, `isEnabled()`
- FFT/DSP functions: `dsp_compute_*()`, `audio_pipeline_*()` (underscore separation for clarity in lower-level APIs)

**Functions (JavaScript):**
- Scope-shared globals (concatenated files): `camelCase()` with long names for clarity, exported to `.eslintrc.json` globals if used across files
- Event handlers: `onEventName()` (e.g., `onMatrixCellClick()`, `onInputGainChange()`)
- UI builders: `renderFeatureName()` or `buildHtml()` (e.g., `renderAudioSubView()`, `peqBandRowHtml()`)
- Async/Network: `fetchData()`, `submitForm()` (verb + noun pattern)
- Utility functions: `formatOutput()`, `escapeHtml()`, `parseDeviceYaml()`
- State setters in globals: `camelCase` with `writable` vs `readonly` in ESLint config

**Variables (C++):**
- Global AppState: `appState` singleton accessed via `AppState::getInstance()` or macro `appState`
- Private/file-scoped statics: `_camelCase` (underscore prefix, e.g., `_smoothedAudioLevel`, `_cachedAdcCfg[2]`)
- Constants: `UPPERCASE_WITH_UNDERSCORES` (e.g., `MAX_SESSIONS`, `WS_TOKEN_TTL_MS`)
- Struct members: `camelCase` (e.g., `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`)
- Boolean fields: `is*` or `*Enabled` (e.g., `_passwordNeedsMigration`, `_used`)

**Variables (JavaScript):**
- Global state: `camelCase` with `writable`/`readonly` in ESLint (e.g., `currentWifiConnected`, `numInputLanes`)
- Local/block scope: `camelCase` (e.g., `sessionId`, `maxHistoryPoints`)
- Constants: `UPPERCASE_WITH_UNDERSCORES` (e.g., `WS_MIN_RECONNECT_DELAY`, `DEBUG_MAX_LINES`)
- DOM cache prefixes: `*Current`, `*Target` for lerp animations (e.g., `waveformCurrent`, `waveformTarget`)
- Numeric prefixes: `numX`, `maxX`, `minX` (e.g., `numInputLanes`, `maxHistoryPoints`)

**Types/Enums:**
- C++ enums: `UPPERCASE_VALUE` members (e.g., `STATE_IDLE`, `AUDIO_OK`, `DSP_BIQUAD_LPF`)
- Struct names: `CamelCaseLikeClasses` (e.g., `DspBiquadParams`, `WsToken`, `MQTTSettings`)
- HAL device classes: `HalDeviceName` inheriting `HalAudioDevice` or `HalDevice` (e.g., `HalPcm5102a`, `HalEs8311`)

## Code Style

**Formatting:**
- No dedicated formatter enforced (manual style)
- Brace style: Allman for functions, same-line for control flow (if/for/while)
- Indentation: 2 spaces (observed in all source)
- Line length: Soft limit ~100 chars (observed in practice)
- Include guards: `#ifndef MODULE_H` + `#define MODULE_H` + `#endif // MODULE_H`

**Linting (JavaScript):**
- Tool: ESLint via `web_src/.eslintrc.json` with 411 global declarations
- Key rules: `no-undef: error`, `no-redeclare: error`, `eqeqeq: smart` (smart equality to allow == null)
- Execution: `npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json` in e2e directory
- Pre-commit: `.githooks/pre-commit` runs ESLint + duplicate/missing function checks

**Linting (C++):**
- Static analysis: cppcheck run in CI only on `src/` (excluding `src/gui/`)
- Pre-commit checks: `tools/find_dups.js` (JS duplicate declarations), `tools/check_missing_fns.js` (undefined references)

## Import Organization

**Order (C++):**
1. Local headers (project): `#include "module.h"`
2. HAL headers: `#include "hal/hal_device_manager.h"` (grouped together)
3. Library headers: `#include <Arduino.h>`, `#include <ArduinoJson.h>` (alphabetically)
4. System headers: `#include <cmath>`, `#include <stdint.h>` (last)

Example from `auth_handler.cpp`:
```cpp
#include "auth_handler.h"
#include "app_state.h"
#include "globals.h"
#include "config.h"
#include "debug_serial.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
```

**Order (JavaScript):**
- Single concatenated file (no imports), so order managed by filename prefixes
- Load order: `01-core.js`, `02-ws-router.js`, `04-shared-audio.js`, `05-audio-tab.js`, etc.
- Global functions must be exported to `.eslintrc.json` before use in other files

**Path Aliases:**
- None in C++ (direct includes, no aliases)
- None in JavaScript (single-file concatenation)

## Error Handling

**Patterns (C++):**
- Boolean return: Functions return `bool` for success/fail (e.g., `loadMqttSettings()` returns true if enabled)
- Struct return with success field: HAL init returns `HalInitResult` with `.success` flag (check via `if (dev->init().success)`)
- Logging on error: All error paths log via `LOG_E()` macro with module prefix `[ModuleName]`
- Graceful degradation: Missing devices set `_ready=false`, pipeline skips unready sinks/sources
- Rate limiting: `loginFailCount` + `_lastFailTime` with `LOGIN_COOLDOWN_US` gate in `auth_handler.cpp`
- Null guards: Check pointer before dereference (e.g., `if (!dev) return false;`)

**Error Types:**
- I/O errors: Log with context (file, device, GPIO), return false
- Configuration errors: Log warning, apply safe default, continue
- Resource exhaustion: Log warning, return false, retry on next cycle (e.g., DSP stage pool)
- Timing-critical paths: Avoid logging in ISR/audio task — use dirty flags instead

Example from `dsp_pipeline.cpp` (config import):
```cpp
if (!dsp_add_stage(...)) {
  LOG_W("[DSP] Stage %d pool exhausted, rolling back", i);
  dsp_reset_config();
  return false; // Config import failed, keep old config
}
```

**Logging (C++):**
- `LOG_D()`: High-frequency details (pattern steps, loop iterations) — often conditional on state change
- `LOG_I()`: State transitions, significant events (device added/removed, connection established, timer fired)
- `LOG_W()`: Warnings, recovery actions (out-of-memory, invalid config, fallback taken)
- `LOG_E()`: Errors, failed operations (I2C timeout, authentication failure, file not found)
- **Never log in real-time tasks** (audio_pipeline_task, ISR): Use dirty flags, main loop calls `audio_periodic_dump()`

Module prefix format: `[ModuleName]` at start of message (extracted by frontend for filtering). Examples:
- `[WiFi] Connected to SSID`
- `[HAL] Device PCM5102A initialized at slot 0`
- `[Auth] Login rate limit: 5m cooldown`

**Logging (JavaScript):**
- `console.log()`: General info
- `console.warn()`: Non-fatal issues (e.g., `console.warn('Unauthorized (401)...')`)
- `console.error()`: Errors (e.g., `console.error('API Fetch Error [${url}]:', error)`)
- No custom logger; browser console is primary output

## Comments

**When to Comment (C++):**
- Complex algorithms: Explain the approach (e.g., EMA smoothing with specific alpha/tau values)
- Non-obvious state transitions: Why the state changes, not just what it does
- Security decisions: Why timing-safe comparison is used, constant-time secrets
- Performance optimizations: Why inline is chosen, PSRAM allocation rationale
- Workarounds: Document HAL-specific quirks (e.g., "Both I2S are masters due to ESP32 slave mode bugs")

**When NOT to comment:**
- Self-explanatory code (e.g., `appState.audio.sampleRate = 48000;`)
- Loop iterations (e.g., `for (int i = 0; i < 8; i++)`)
- Function signature (let the name speak)

**JSDoc/TSDoc:**
- Not used in this codebase (minimal comments)
- Headers document public APIs with brief descriptions only

Example from `i2s_audio.h`:
```cpp
// Get per-lane VU metering (dBFS). Returns -90.0f if lane has no registered source.
float audio_pipeline_get_lane_vu_l(int lane);
float audio_pipeline_get_lane_vu_r(int lane);
```

## Function Design

**Size Guidelines (C++):**
- Aim for <100 lines per function (observed norm)
- Handler functions: 30-60 lines (parse input → validate → update state → send response)
- Private helpers: <20 lines (single responsibility)
- Task loops: 20-30 lines (main operation, delegate details)

**Parameters:**
- Pass by const reference for large objects: `const String &password`, `const JsonDocument &doc`
- Pass by value for primitives: `int lane`, `float gain`
- Output parameters: Use return value first, `*out_param` last if needed (e.g., HAL discovery sets device data)
- Avoid >5 parameters; use struct if more needed (e.g., `struct HalInitResult`)

**Return Values:**
- Simple success/fail: `bool` (true = success, false = failure)
- Value + status: Struct with `.success` field (e.g., `HalInitResult`)
- Resource handle: Pointer (checked against nullptr)
- Data arrays: Return count/size, caller allocates or iterates via slots
- No-op operations: `void` (e.g., HAL `setEnabled()` succeeds or logs error internally)

Example from `audio_pipeline.h`:
```cpp
// Slot-indexed sink API — atomically places a sink at fixed slot index (0..AUDIO_OUT_MAX_SINKS-1)
void audio_pipeline_set_sink(int slot, const AudioOutputSink *sink);

// Returns count of active sinks (highest occupied slot + 1)
int  audio_pipeline_get_sink_count();

// Read-only accessor, returns NULL if lane is out of range or empty
const AudioOutputSink* audio_pipeline_get_sink(int idx);
```

## Module Design

**Exports:**
- Header files define public interface; implementation in .cpp
- Avoid exposing internal state; use getter functions
- File-scoped statics for module-private state (e.g., `static float _smoothedAudioLevel`)
- Extern declarations for cross-module singletons (e.g., `extern AppState appState`)

**Barrel Files:**
- Not used in C++ (direct includes)
- Not used in JavaScript (single concatenated file)

**Module Boundaries:**
- HAL modules: One device type per `hal_device_name.h/.cpp` (e.g., `hal_pcm5102a.h`)
- Feature modules: One feature per file (e.g., `smart_sensing.h`, `mqtt_handler.h`)
- State domain: Each domain in `src/state/domain_state.h` (e.g., `wifi_state.h`, `dsp_state.h`)
- API modules: One REST endpoint group per file (e.g., `dsp_api.cpp`, `hal_api.cpp`)

**Cross-Module Communication:**
- Shared state: Via `AppState` domain composition (e.g., `appState.wifi.ssid`)
- Events: Dirty flags + `app_events.h` event group (`EVT_XXX` bits, 16 assigned, 8 spare)
- Callbacks: HAL uses `HalStateChangeCb` for device lifecycle notifications
- Queues: `HalCoordState` toggle queue (capacity 8, same-slot dedup) for deferred HAL actions

## Special Patterns

**AppState Access:**
- New code: Use nested domain: `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`, `appState.dac.txUnderruns`
- Legacy macro aliases: `#define wifiSSID appState.wifiSSID` (present for backward compat, avoid in new code)
- Volatile fields: `volatile bool audioPaused` used for cross-core audio task sync
- Dirty flags: `isBuzzerDirty()`, `isDisplayDirty()`, `isOTADirty()` — indicate change detected, trigger WS broadcast

**HAL Device Model:**
- Lifecycle: UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ⇄ UNAVAILABLE → ERROR/REMOVED/MANUAL
- Volatile `_ready` flag: Lock-free read from audio pipeline (Core 1)
- Config persistence: Per-device `HalDeviceConfig` (GPIO overrides, I2C bus selection) via `/hal_config.json`
- Multi-instance: `instanceId` + `countByCompatible(compatible)` for slot/lane assignment
- Capability bits: `HAL_CAP_DAC_PATH`, `HAL_CAP_ADC_PATH` map to pipeline slots/lanes

**Web UI Global Scope:**
- All JS files concatenated into single scope
- Must declare ALL top-level variables/functions in `.eslintrc.json` globals (380+ entries)
- No module scoping (intentional for simplicity)
- Dirty flag pattern for state changes: Set flag, main loop broadcasts via WS

**Testing Conventions (C++):**
- Test files included in separate Unity runner (not src/ directly)
- Mock headers override real ones: `#ifdef NATIVE_TEST` selects mocks
- State reset in `setUp()`, cleanup in `tearDown()`
- Arrange-Act-Assert pattern with clear section comments

**Testing Conventions (JavaScript):**
- Playwright fixtures for setup (session auth, WS interception)
- `connectedPage` fixture handles session + WS auth automatically
- Custom `expect()` assertions built on standard Playwright
- No real hardware needed (mock server on port 3000)

---

*Convention analysis: 2026-03-21*
