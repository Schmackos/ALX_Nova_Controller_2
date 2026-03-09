# Coding Conventions

**Analysis Date:** 2026-03-09

---

## C++ Firmware

### File Naming

- Source files use `snake_case`: `wifi_manager.cpp`, `hal_device_manager.cpp`
- Header/source pairs share the same base name: `debug_serial.h` / `debug_serial.cpp`
- HAL drivers live under `src/hal/` and follow `hal_<name>.h/.cpp` — e.g. `src/hal/hal_pcm5102a.h`, `src/hal/hal_es8311.cpp`
- Test files are named `test_<module>.cpp` inside `test/test_<module>/` — one `.cpp` per directory to avoid duplicate `main`/`setUp`/`tearDown` link symbols
- Web JS modules: `<NN>-<kebab-name>.js` — numeric prefix controls concat order: `01-core.js`, `05-audio-tab.js`
- Web CSS modules: `<NN>-<kebab-name>.css` — e.g. `00-tokens.css`, `03-components.css`
- E2E specs: `<kebab-name>.spec.js` — e.g. `hal-devices.spec.js`, `audio-inputs.spec.js`
- Fixtures: `<kebab-name>.json` in `e2e/fixtures/ws-messages/` and `e2e/fixtures/api-responses/`

### Symbol Naming

- **Classes**: `PascalCase` — `HalDevice`, `HalDeviceManager`, `DebugSerial`, `HalPcm5102a`
- **Enums**: `PascalCase` type name with `SCREAMING_SNAKE_CASE` values:
  ```cpp
  enum HalDeviceType : uint8_t { HAL_DEV_NONE = 0, HAL_DEV_DAC = 1, HAL_DEV_ADC = 2 };
  enum HalDeviceState : uint8_t { HAL_STATE_UNKNOWN = 0, HAL_STATE_AVAILABLE = 3 };
  ```
- **Free functions**: `snake_case` — `audio_pipeline_set_sink()`, `dsp_swap_config()`, `i2s_audio_init()`
- **Class methods**: `camelCase` — `registerDevice()`, `findByCompatible()`, `getInputSource()`, `healthCheck()`
- **Private/protected class members**: `_prefixedCamelCase` — `_descriptor`, `_slot`, `_initPriority`, `_ready`, `_state`
- **File-local statics**: `_prefixedCamelCase` — `_smoothedAudioLevel`, `_wsTokens`, `_halSlotToSinkSlot[]`
- **Constants/macros**: `SCREAMING_SNAKE_CASE` — `HAL_MAX_DEVICES`, `FIRMWARE_VERSION`, `BUZZER_PWM_CHANNEL`
- **Compile-time pins in `config.h`**: declared as `const int` with `#ifndef` guard (not raw `#define`):
  ```cpp
  #ifndef LED_PIN
  const int LED_PIN = 1;
  #endif
  ```
- **Test functions**: `test_<what_it_tests>` — `test_register_and_get_device()`, `test_max_devices_limit()`

### Compile-Time Guard Macros

These macros conditionally compile entire subsystems. Guard presence/absence must leave code coherent.

| Macro | Purpose | Where checked |
|---|---|---|
| `UNIT_TEST` | Native test build (set by `[env:native]`) | `config.h`, many src headers |
| `NATIVE_TEST` | Native test build — selects mock headers | All test files and mocked headers |
| `GUI_ENABLED` | Enables LVGL GUI subsystem (`src/gui/`) | All GUI code, `main.cpp` |
| `DAC_ENABLED` | Enables HAL DAC framework + I2S TX | `dac_hal.h`, `hal_device_manager.h`, all HAL `.cpp` wrappers |
| `DSP_ENABLED` | Enables biquad/FIR/DSP pipeline | `dsp_pipeline.h`, `dsp_coefficients.h` |
| `USB_AUDIO_ENABLED` | Enables TinyUSB UAC2 speaker device | `usb_audio.h`, `state/usb_audio_state.h` |
| `CONFIG_IDF_TARGET_ESP32P4` | Enables P4-only peripherals (temp sensor) | `hal_temp_sensor.cpp` |

**Standard mock-vs-hardware include pattern** (all test files follow this exactly):

```cpp
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#include "../test_mocks/WiFi.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#endif
```

**HAL driver `.cpp` wrapper pattern** — entire driver body wrapped in `#ifdef DAC_ENABLED`:

```cpp
#ifdef DAC_ENABLED
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

### HAL Driver Pattern (ESPHome-Inspired)

All hardware drivers inherit from `HalDevice` (`src/hal/hal_device.h`) and implement five pure virtual methods:

```cpp
class HalMyDriver : public HalDevice {
public:
    bool probe() override;         // Non-destructive — I2C ACK, GPIO check, chip ID verify
    HalInitResult init() override; // Full hardware init; returns error code on failure
    void deinit() override;        // Idempotent shutdown + resource release
    void dumpConfig() override;    // LOG_I output of full descriptor at boot
    bool healthCheck() override;   // Periodic register read (30s timer)
};
```

`initAll()` is never called on builtin devices at runtime — devices are explicitly `probe()`d then `init()`d after `registerDevice()`.

**Init result factory functions** (`src/hal/hal_init_result.h`):

```cpp
return hal_init_ok();
return hal_init_fail(DIAG_HAL_INIT_FAILED, "descriptive reason");
```

**Input device extension** — override `getInputSource()` for ADC/audio-source devices:

```cpp
virtual const AudioInputSource* getInputSource() const override { return &_inputSrc; }
```

**HAL device lifecycle states** (`src/hal/hal_types.h`):

`HAL_STATE_UNKNOWN` → `HAL_STATE_DETECTED` → `HAL_STATE_CONFIGURING` → `HAL_STATE_AVAILABLE` ⇄ `HAL_STATE_UNAVAILABLE` → `HAL_STATE_ERROR` / `HAL_STATE_REMOVED` / `HAL_STATE_MANUAL`

Volatile `_ready` and `_state` fields on `HalDevice` enable lock-free reads from Core 1 audio task.

**Init priority constants** (higher = initialised first in priority-sorted `initAll()`):

```cpp
#define HAL_PRIORITY_BUS       1000   // I2C, I2S, SPI bus controllers
#define HAL_PRIORITY_IO         900   // GPIO expanders
#define HAL_PRIORITY_HARDWARE   800   // Audio codec/DAC/ADC hardware
#define HAL_PRIORITY_DATA       600   // Data consumers (pipeline, metering)
#define HAL_PRIORITY_LATE       100   // Non-critical (diagnostics)
```

**Capability bit flags** (`src/hal/hal_types.h`): `HAL_CAP_HW_VOLUME`, `HAL_CAP_FILTERS`, `HAL_CAP_MUTE`, `HAL_CAP_ADC_PATH`, `HAL_CAP_DAC_PATH`, `HAL_CAP_PGA_CONTROL`, `HAL_CAP_HPF_CONTROL`, `HAL_CAP_CODEC`

### Header Guards

- New HAL headers: `#pragma once`
- Legacy non-HAL headers: `#ifndef MODULE_H / #define MODULE_H / #endif`

Both styles coexist; new code uses `#pragma once`.

### Import Organization

**C++ headers — typical order in `.cpp` files:**
1. Module's own header
2. Application state (`app_state.h`, `globals.h`)
3. Application utilities (`debug_serial.h`, `config.h`)
4. Sibling modules (`i2s_audio.h`, `websocket_handler.h`)
5. Third-party (`<ArduinoJson.h>`, `<LittleFS.h>`, `<Preferences.h>`)
6. Platform (`<Arduino.h>`, `<esp_wifi.h>`)
7. Standard library (`<cmath>`, `<cstring>`)

**Test file include pattern** (test files include `.cpp` directly since `test_build_src = no`):

```cpp
#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#endif

// Inline the .cpp files needed for the test
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
```

### AppState Access Pattern

Domain state accessed via nested composition — never via deprecated `#define` aliases in new code:

```cpp
appState.wifi.ssid                    // WifiState
appState.audio.adcEnabled[i]          // AudioState
appState.dac.es8311Enabled            // DacState
appState.general.darkMode             // GeneralState
appState.dsp.enabled                  // DspSettingsState
appState.mqtt.broker                  // MqttState
appState.debug.hwStats                // DebugState
```

Cross-cutting flags remain at top level: `appState._mqttReconfigPending`, `appState._pendingApToggle`.

Dirty flag setters call `app_events_signal(EVT_XXX)` to wake the main loop from `app_events_wait(5)`.

**DAC toggle deferrals** — never call `dev->deinit()` directly for `HAL_CAP_DAC_PATH` devices from REST handlers. Use validated setters that only accept -1, 0, 1:

```cpp
appState.dac.requestDacToggle(1);
appState.dac.requestEs8311Toggle(-1);
```

### Error Handling

**HTTP REST handlers** — uniform pattern:

```cpp
if (!requireAuth()) return;
if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }
if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
server.send(200, "application/json", "{\"success\":true}");
```

- HTTP 503 when DSP swap fails (15 endpoints guarded)
- HTTP 429 with `Retry-After` header for login rate limit violations
- HTTP 409 for concurrent HAL rescan attempt

**Memory allocation guard**:

```cpp
char* buf = (char*)malloc(size);
if (!buf) { server.send(503, "application/json", "{\"error\":\"Out of memory\"}"); return; }
```

**LittleFS file operations** — always check open and size:

```cpp
File f = LittleFS.open(path, FILE_READ);
if (!f || f.size() == 0) { if (f) f.close(); return false; }
```

**Functions that fail**: return `bool` (true=success), `HalInitResult` (HAL drivers), or `int` slot index (-1 on failure). Never silently swallow errors — always `LOG_E` before returning false.

### Logging

All modules use macros from `src/debug_serial.h`:

```cpp
#define LOG_D(fmt, ...) DebugOut.debug(fmt, ##__VA_ARGS__)  // High-frequency operational detail
#define LOG_I(fmt, ...) DebugOut.info(fmt, ##__VA_ARGS__)   // State transitions, significant events
#define LOG_W(fmt, ...) DebugOut.warn(fmt, ##__VA_ARGS__)   // Recoverable abnormal conditions
#define LOG_E(fmt, ...) DebugOut.error(fmt, ##__VA_ARGS__)  // Errors requiring attention
```

**Every log call includes a `[ModuleName]` prefix:**

| Module | Prefix | File |
|---|---|---|
| Smart sensing | `[Sensing]` | `src/smart_sensing.cpp` |
| I2S audio | `[Audio]` | `src/i2s_audio.cpp` |
| Signal generator | `[SigGen]` | `src/signal_generator.cpp` |
| Buzzer | `[Buzzer]` | `src/buzzer_handler.cpp` |
| WiFi | `[WiFi]` | `src/wifi_manager.cpp` |
| MQTT | `[MQTT]` | `src/mqtt_handler.cpp`, `src/mqtt_publish.cpp` |
| OTA | `[OTA]` | `src/ota_updater.cpp` |
| Settings | `[Settings]` | `src/settings_manager.cpp` |
| USB Audio | `[USB Audio]` | `src/usb_audio.cpp` |
| Output DSP | `[OutputDSP]` | `src/output_dsp.cpp` |
| HAL core | `[HAL]` | `src/hal/hal_device_manager.cpp` |
| HAL discovery | `[HAL Discovery]` | `src/hal/hal_discovery.cpp` |
| HAL device DB | `[HAL DB]` | `src/hal/hal_device_db.cpp` |
| HAL API | `[HAL API]` | `src/hal/hal_api.cpp` |
| GUI | `[GUI]` | `src/gui/gui_manager.cpp` |

**Logging rules:**
- `LOG_I` for state transitions and significant events (connect/disconnect, health changes, start/stop)
- `LOG_D` for high-frequency operational details (pattern steps, parameter snapshots)
- NEVER call `LOG_*` or `Serial.print` inside ISR paths or `audio_pipeline_task` (Core 1 — UART TX blocks, starves DMA, causes audio dropouts). Use dirty-flag pattern: task sets flag, main loop calls `audio_periodic_dump()` for actual output.
- Log transitions, not steady state. Use a `static prev` variable to detect changes before logging.
- Save log files (build output, test reports, serial captures) to `logs/` — keep project root clean.

### FreeRTOS / Cross-Core Safety

- `volatile` on fields read hot from Core 1 audio task: `appState.audioPaused`, `HalDevice::_ready`, `HalDevice::_state`
- `vTaskSuspendAll()` / `xTaskResumeAll()` for atomic source/sink slot operations in audio pipeline
- Binary semaphore `appState.audioTaskPausedAck` for deterministic DAC deinit/reinit handshake (replaces volatile + `vTaskDelay(40ms)` guesswork)
- Main loop: `app_events_wait(5)` replaces `delay(5)` — wakes in <1µs on any dirty flag, falls back to 5ms tick
- Core 1 is reserved for `loopTask` + `audio_pipeline_task` only. No new tasks may be pinned to Core 1.
- MQTT runs entirely on Core 0 (`mqtt_task`). Main loop never calls `mqttLoop()` or `publishMqtt*()`.

---

## JavaScript (Web Frontend)

### File Organization

JS files in `web_src/js/` are concatenated in filename alphabetical order into a single `<script>` block by `tools/build_web_assets.js`. All files share one global scope.

Current modules (load order by filename prefix):
- `01-core.js` — WebSocket connection, `apiFetch`, reconnect with exponential backoff
- `02-ws-router.js` — message dispatch (`routeWsMessage`), binary frame handling
- `03-app-state.js` — global state variable declarations
- `04-shared-audio.js` — dynamic `numInputLanes`, `resizeAudioArrays()` (no hardcoded `NUM_ADCS`)
- `05-audio-tab.js` — Audio tab: HAL-driven input/output strips, 16×16 matrix UI, SigGen sub-view
- `06-canvas-helpers.js` / `06-peq-overlay.js` — canvas drawing, PEQ/crossover/compressor/limiter overlays
- `07-ui-core.js` / `08-ui-status.js` — sidebar, tab switching, status bar
- `09-audio-viz.js` — waveform/spectrum canvas animation loops
- `13-signal-gen.js` — SigGen controls
- `15-hal-devices.js` / `15a-yaml-parser.js` — HAL device management UI
- `20-wifi-network.js` / `21-mqtt-settings.js` — WiFi multi-network config, MQTT settings
- `22-settings.js` through `28-init.js` — Settings, OTA, debug console, auth, health dashboard, init

### Naming Patterns

- **Global variables**: `camelCase` — `numInputLanes`, `audioChannelMap`, `halDevices`, `currentWifiConnected`
- **Functions**: `camelCase` — `resizeAudioArrays()`, `renderHalDevices()`, `openPeqOverlay()`, `buildInitialState()`
- **Constants**: `SCREAMING_SNAKE_CASE` — `WS_MIN_RECONNECT_DELAY`, `HAL_CAP_DAC_PATH`, `DEBUG_MAX_LINES`
- **DOM IDs** (from `web_src/index.html`): `camelCase` — `#halDeviceList`, `#wsConnectionStatus`, `#audioInputsContainer`
- **Data attributes**: `kebab-case` — `data-tab="devices"`, `data-view="matrix"`

### Concatenated Scope Rules

Because all JS shares one global scope, these rules are mandatory:

- No duplicate `let`/`const`/`var` declarations across files — `node tools/find_dups.js` enforces this at pre-commit and CI
- No ES module `import`/`export` syntax — `"sourceType": "script"` is set in `.eslintrc.json`
- Every top-level function and variable must be declared in exactly one file
- When adding a new global declaration, add it to `web_src/.eslintrc.json` under `"globals"` with `"writable"` or `"readonly"` appropriately

### ESLint Rules

Config: `web_src/.eslintrc.json`. Three enforced rules:

```json
"rules": {
  "no-undef": "error",
  "no-redeclare": ["error", { "builtinGlobals": false }],
  "eqeqeq": ["error", "smart"]
}
```

- `no-undef` — all referenced globals must be declared in the `"globals"` section (currently 380+ entries)
- `no-redeclare` — no duplicate variable or function declarations
- `eqeqeq: "smart"` — requires `===` except for null comparisons where `==` is acceptable

Run: `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`

### Static Analysis Tools

Two Node.js tools run before every commit and in CI (`js-lint` job):

**`node tools/find_dups.js`** — scans all `web_src/js/` files at scope depth=0 and reports any duplicate `let`/`const`/`var` or function declarations. Must pass before commit.

**`node tools/check_missing_fns.js`** — verifies that functions referenced in HTML event handlers and WS router dispatch tables exist in the JS source. Must pass before commit.

### WebSocket Protocol

Binary WebSocket frames (sent with `sendBIN`) use a type byte prefix:
- `0x01` — waveform: `[type:u8][adc:u8][256 samples:u8]` = 258 bytes total
- `0x02` — spectrum: `[type:u8][adc:u8][dominantFreq:f32LE][16 bands:f32LE]` = 70 bytes total

JSON frames dispatched by `routeWsMessage(data)` via `data.type` string.

Flat `audioAdc[0]`/`audioAdc[1]` broadcast fields are deprecated as of v1.14 — use per-lane arrays with dynamic `numInputLanes` iteration instead.

### Icon Convention

All icons use inline SVG from Material Design Icons (MDI, `pictogrammers.com`). No external CDN.

```html
<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true">
  <path d="<MDI path data>"/>
</svg>
```

- `fill="currentColor"` — inherits CSS `color` property
- `width`/`height`: 18px for inline text buttons, 24px for standalone buttons
- `aria-hidden="true"` on decorative icons; `aria-label` on icon-only interactive elements
- In JS-generated strings: single-quoted outer JS string, double-quoted SVG attributes

### Web Asset Build

**NEVER edit `src/web_pages.cpp` or `src/web_pages_gz.cpp` directly.** Both files are auto-generated.

After any change to `web_src/`:

```bash
node tools/build_web_assets.js
```

This regenerates both `.cpp` files from `web_src/index.html` + all CSS/JS files.

---

## Commit Convention

Format: `<type>: <short description>`

| Type | Use |
|---|---|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation update |
| `refactor` | Code restructuring without behavior change |
| `test` | Adding or updating tests |
| `chore` | Maintenance tasks |

**IMPORTANT**: Never add `Co-Authored-By` trailers to commit messages. No AI attribution lines.

---

## Pre-Commit Hooks

Activate with `git config core.hooksPath .githooks`.

The hook at `.githooks/pre-commit` runs three checks in sequence (`set -e` — any failure aborts):

```bash
node tools/find_dups.js           # 1/3 — duplicate JS declarations
node tools/check_missing_fns.js  # 2/3 — missing function references
cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json  # 3/3 — ESLint
```

---

## Documentation and Diagrams

- Architecture diagrams: `docs-internal/architecture/` — 10 Mermaid `.mmd` files (system-architecture, hal-lifecycle, hal-pipeline-bridge, boot-sequence, event-architecture, sink-dispatch, test-infrastructure, ci-quality-gates, e2e-test-flow, test-coverage-map)
- Internal planning documents: `docs-internal/planning/`
- Log files (build output, test reports, serial captures): `logs/` directory — keep project root clean

---

*Convention analysis: 2026-03-09*
