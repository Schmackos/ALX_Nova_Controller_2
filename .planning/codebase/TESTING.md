# Testing

_Last updated: 2026-03-21_

## Overview

Two separate test suites covering the full stack:

1. **C++ Unit Tests** — Unity framework on the `native` platform (~1620+ tests, 70+ modules)
2. **E2E Browser Tests** — Playwright against a mock Express server (26 tests, 19 specs)

Both suites run in CI as parallel quality gates and must pass before firmware build proceeds.

---

## C++ Unit Tests (Unity / PlatformIO native)

### Framework & Configuration
- **Framework**: [Unity](https://github.com/ThrowTheSwitch/Unity) via PlatformIO
- **Platform**: `native` (host machine, gcc/MinGW — no ESP32 hardware needed)
- **Config** (`platformio.ini`):
  ```ini
  [env:native]
  platform = native
  test_framework = unity
  test_build_src = no     # Tests don't compile src/ directly — inline .cpp manually
  build_flags = -D UNIT_TEST -D NATIVE_TEST
  ```
- **Run all**: `pio test -e native`
- **Run single module**: `pio test -e native -f test_hal_core`
- **Verbose output**: `pio test -e native -v`

### Structure
Each test module lives in its own directory to avoid symbol conflicts:

```
test/
├── test_mocks/                 # Mock implementations for all external deps
│   ├── Arduino.h               # millis(), delay(), digitalRead/Write, analogRead
│   ├── WiFi.h                  # WiFi connection mocking
│   ├── PubSubClient.h          # MQTT client mock
│   ├── Preferences.h           # NVS persistence mock
│   ├── LittleFS.h              # Filesystem mock
│   ├── Wire.h                  # I2C mock
│   ├── ETH.h                   # Ethernet mock
│   ├── i2s_std_mock.h          # I2S driver mock
│   └── mbedtls/                # TLS mock headers
├── test_<module>/
│   └── test_<module>.cpp       # One test file per module
└── [hardware-specific dirs]    # idf4_pcm1808_test, idf5_dac_test, etc.
```

### Test File Pattern

```cpp
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#else
#include <Arduino.h>
#endif

// Include .cpp files directly (since test_build_src = no)
#include "../../src/some_module.cpp"

void setUp() {
    // Reset state before each test
}

void tearDown() {}

void test_feature_does_expected_thing() {
    // Arrange
    // Act
    // Assert
    TEST_ASSERT_EQUAL(expected, actual);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_feature_does_expected_thing);
    return UNITY_END();
}
```

### Key Pattern: `.cpp` Inlining
Because `test_build_src = no`, tests don't compile `src/` automatically. Each test file `#include`s the specific `.cpp` implementation files it needs, along with all their mock dependencies. This avoids pulling in the entire firmware but requires careful dependency management.

Example from `test_hal_core`:
```cpp
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
```

### Mocking Approach
- **ArduinoMock** namespace: `ArduinoMock::mockMillis`, `ArduinoMock::mockAnalogValue`, `ArduinoMock::mockDigitalPins[50]`
- **I2C**: `Wire.h` mock with configurable `read()` return values
- **Filesystem**: LittleFS mock with in-memory backing
- **NVS**: Preferences mock with map-backed storage
- **MQTT**: PubSubClient mock with callback simulation
- **Compile guards**: Use `#ifdef NATIVE_TEST` / `#ifdef UNIT_TEST` for conditional compilation

### Test Modules (70+ total)

| Category | Modules |
|----------|---------|
| **Core utilities** | test_utils, test_settings, test_auth, test_pinout, test_crash_log, test_debug_mode |
| **Network** | test_wifi, test_mqtt, test_ota, test_ota_task, test_eth_manager |
| **Audio pipeline** | test_audio_pipeline, test_audio_diagnostics, test_audio_health_bridge, test_i2s_audio, test_i2s_config_cache, test_vrms, test_fft |
| **DSP** | test_dsp, test_dsp_rew, test_dsp_presets, test_dsp_swap, test_esp_dsp, test_peq, test_output_dsp |
| **HAL core** | test_hal_core, test_hal_bridge, test_hal_coord, test_hal_discovery, test_hal_integration, test_hal_eeprom_v3, test_hal_dsp_bridge |
| **HAL drivers** | test_hal_pcm5102a, test_hal_pcm1808, test_hal_es8311, test_hal_mcp4725, test_hal_siggen, test_hal_usb_audio, test_hal_ns4150b, test_hal_custom_device, test_hal_multi_instance, test_hal_state_callback, test_hal_retry, test_hal_wire_mock |
| **HAL I/O devices** | test_hal_buzzer, test_hal_button, test_hal_encoder |
| **DAC subsystem** | test_dac_hal, test_dac_eeprom, test_dac_settings |
| **Pipeline** | test_pipeline_bounds, test_pipeline_output, test_sink_slot_api, test_sink_write_utils, test_deferred_toggle |
| **Signal/Gen** | test_signal_generator, test_hal_siggen |
| **GUI** | test_gui_home, test_gui_input, test_gui_navigation |
| **Web/WS** | test_websocket, test_websocket_messages, test_api |
| **Other** | test_button, test_buzzer, test_smart_sensing, test_task_monitor, test_usb_audio, test_dim_timeout, test_diag_journal, test_evt_any, test_es8311 |

### Coverage Rules
- **New modules** → add `test/test_<module>/test_<module>.cpp`
- **Changed function signatures** → update affected test includes
- **New HAL drivers** → use `hal-driver-scaffold` agent (creates test module automatically)
- Test count as of 2026-03-10: ~1620 tests passing

---

## E2E Browser Tests (Playwright)

### Framework & Configuration
- **Framework**: [Playwright](https://playwright.dev/) with Chromium
- **Test server**: Express (port 3000) assembling HTML from `web_src/`
- **WS interception**: `page.routeWebSocket(/.*:81/, handler)` — no real firmware needed
- **Config**: `e2e/playwright.config.js`

### Commands
```bash
cd e2e
npm install                                    # First time
npx playwright install --with-deps chromium   # First time
npx playwright test                            # All 26 tests
npx playwright test tests/auth.spec.js         # Single spec
npx playwright test --headed                   # Visible browser
npx playwright test --debug                    # Inspector mode
```

### Structure

```
e2e/
├── tests/                          # 19 spec files
│   ├── auth.spec.js
│   ├── auth-password.spec.js
│   ├── navigation.spec.js
│   ├── audio-inputs.spec.js
│   ├── audio-matrix.spec.js
│   ├── audio-outputs.spec.js
│   ├── audio-siggen.spec.js
│   ├── hal-devices.spec.js
│   ├── peq-overlay.spec.js
│   ├── control-tab.spec.js
│   ├── wifi.spec.js
│   ├── mqtt.spec.js
│   ├── settings.spec.js
│   ├── ota.spec.js
│   ├── debug-console.spec.js
│   ├── hardware-stats.spec.js
│   ├── support.spec.js
│   ├── dark-mode.spec.js
│   └── responsive.spec.js
├── helpers/
│   ├── fixtures.js        # connectedPage: session cookie + WS auth + initial state
│   ├── ws-helpers.js      # buildInitialState(), handleCommand(), binary frame builders
│   └── selectors.js       # Reusable DOM selectors matching web_src/index.html IDs
├── mock-server/
│   ├── server.js          # Express server (port 3000)
│   ├── assembler.js       # HTML assembly from web_src/ (mirrors build_web_assets.js)
│   ├── ws-state.js        # Deterministic mock state singleton, reset between tests
│   └── routes/            # 12 Express route files matching firmware REST API
├── fixtures/
│   ├── ws-messages/       # 15 hand-crafted WS broadcast fixtures (JSON)
│   └── api-responses/     # 14 deterministic REST response fixtures (JSON)
└── test-results/          # Playwright HTML reports (gitignored)
```

### Key Playwright Patterns

```javascript
// Import connectedPage fixture (session + WS auth already done)
const { test, expect } = require('../helpers/fixtures');

// Tab navigation — use evaluate() to avoid scroll issues with sidebar clicks
await page.evaluate(() => switchTab('settings'));

// WS interception
page.routeWebSocket(/.*:81/, (ws) => {
  ws.onMessage((msg) => { /* handle outgoing WS from page */ });
  ws.send(JSON.stringify({ type: 'stateUpdate', ... })); // inject WS message
});

// CSS-hidden checkboxes — use toBeChecked(), not toBeVisible()
await expect(page.locator('#some-toggle')).toBeChecked();

// Strict mode — use .first() when selector may match multiple elements
await page.locator('.hal-device-card').first().click();
```

### Mock Infrastructure
- **`ws-state.js`**: Singleton holding deterministic test state. Reset between tests via `beforeEach`.
- **WS message flow**: `ws-helpers.js` `buildInitialState()` populates all WS fields; `handleCommand()` simulates firmware responses to WS commands.
- **REST routes**: 12 Express route files in `e2e/mock-server/routes/` mirror every firmware endpoint.
- **Fixtures**: JSON files in `e2e/fixtures/` provide consistent API/WS responses across tests.

### Coverage Rules
- New toggle/button → test verifies correct WS command sent
- New WS broadcast type → add fixture JSON + DOM update test
- New tab/section → navigation + element presence tests
- Changed element IDs → update `e2e/helpers/selectors.js` + affected specs
- New WS message type → update `ws-helpers.js` + `ws-state.js` + `e2e/fixtures/ws-messages/`
- New REST endpoint → update matching route in `e2e/mock-server/routes/` + fixture JSON
- Removed features → delete corresponding tests + fixtures

---

## Static Analysis

### JavaScript (ESLint)
```bash
cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json
```
- Config: `web_src/.eslintrc.json`
- 380 globals for concatenated scope
- Rules: `no-undef`, `no-redeclare`, `eqeqeq`
- New top-level JS declarations → add to globals section

### C++ (cppcheck)
- Runs in CI on `src/` (excluding `src/gui/`)
- Not run locally by default

### Duplicate/missing checks
```bash
node tools/find_dups.js           # Duplicate JS global declarations
node tools/check_missing_fns.js   # Undefined function references
```

---

## Pre-commit Hooks

`.githooks/pre-commit` runs automatically before each commit:
1. `node tools/find_dups.js`
2. `node tools/check_missing_fns.js`
3. ESLint on `web_src/js/`

Activate: `git config core.hooksPath .githooks`

---

## CI/CD Quality Gates

4 parallel gates must all pass before firmware build:

| Gate | Command | What it checks |
|------|---------|----------------|
| `cpp-tests` | `pio test -e native -v` | ~1620 Unity tests |
| `cpp-lint` | cppcheck | C++ static analysis |
| `js-lint` | find_dups + check_missing_fns + ESLint | JS quality |
| `e2e-tests` | `npx playwright test` | 26 browser tests |

Playwright HTML report uploaded as artifact on failure (14-day retention).

---

## Agent Workflow

| Change Type | Agent(s) | Purpose |
|-------------|----------|---------|
| C++ firmware only | `firmware-test-runner` | `pio test -e native -v`, diagnoses failures |
| Web UI only | `test-engineer` or `test-writer` | Playwright, fixes selectors, adds coverage |
| Both firmware + UI | Both agents in parallel | Full coverage |
| New HAL driver | `hal-driver-scaffold` → `firmware-test-runner` | Scaffold creates test module automatically |
| New web feature | `web-feature-scaffold` → `test-engineer` | DOM scaffold then E2E tests |
| Bug investigation | `debugger` or `debug` agent | Root cause with test reproduction |
