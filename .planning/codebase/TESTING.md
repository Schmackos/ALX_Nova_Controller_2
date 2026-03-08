# Testing Patterns

**Analysis Date:** 2026-03-08

## Test Framework Overview

This project has two separate testing layers:

| Layer | Framework | Count | Hardware |
|-------|-----------|-------|----------|
| C++ unit tests | Unity (PlatformIO native) | 1503+ tests, 63+ modules | None required |
| E2E browser tests | Playwright + Express mock | 26 tests, 19 specs | None required |

---

## C++ Unit Tests (Unity / PlatformIO native)

### Framework

**Runner:** Unity (via PlatformIO `test_framework = unity`)
**Platform:** `native` environment (host GCC/MinGW — not ESP32)
**Config file:** `platformio.ini` `[env:native]` section

**Build flags for native tests:**
```
-std=c++11
-D UNIT_TEST
-D NATIVE_TEST
-D DSP_ENABLED
-D DAC_ENABLED
-D USB_AUDIO_ENABLED
```
`test_build_src = no` — test files include source headers and `.cpp` files directly; the `src/` directory is NOT auto-compiled.

**Assertion library:** Unity macros (`TEST_ASSERT_TRUE`, `TEST_ASSERT_EQUAL`, `TEST_ASSERT_FLOAT_WITHIN`, etc.)

**Run Commands:**
```bash
pio test -e native           # Run all C++ unit tests
pio test -e native -v        # Verbose output
pio test -e native -f test_wifi    # Run single module
pio test -e native -f test_mqtt
pio test -e native -f test_auth
```

### Test File Organization

**Location:** `test/` directory; one directory per test module (required by PlatformIO — avoids duplicate `main`/`setUp`/`tearDown` symbols).

**Naming:**
- Directory: `test_<module_name>/` e.g. `test_hal_bridge/`, `test_dsp/`
- File: `test_<module_name>.cpp` e.g. `test_hal_bridge.cpp`

**Directory layout:**
```
test/
├── test_mocks/            # Shared mock headers (included by all test modules)
│   ├── Arduino.h          # String, millis(), GPIO, LEDC mocks
│   ├── WiFi.h             # WiFiClass mock with scan results, status
│   ├── PubSubClient.h     # MQTT client mock with published-message capture
│   ├── Preferences.h      # NVS mock backed by std::map
│   ├── LittleFS.h         # Filesystem mock
│   ├── Wire.h             # I2C bus mock
│   ├── esp_random.h       # Random number mock
│   ├── esp_timer.h        # Timer mock with mockTimerUs control
│   └── mbedtls/           # PBKDF2 / SHA256 mock implementations
│
├── test_auth/             # Session, PBKDF2 hashing, timing-safe compare
├── test_hal_bridge/       # HAL→pipeline slot/lane mapping logic
├── test_hal_manager/      # Device registration, lifecycle, priority init
├── test_dsp/              # Biquad IIR coefficients, FIR, pipeline stages
├── test_audio_pipeline/   # float32 conversion, routing matrix, RMS
├── test_mqtt/             # MQTT connect, publish, HA discovery
├── test_smart_sensing/    # Signal detection, auto-off timer FSM
├── test_wifi/             # WiFi multi-network connect, AP mode
├── test_settings/         # JSON settings load/save, migration
├── test_button/           # Debounce, short/long/double-click FSM
├── ... (63+ modules total)
```

### Test Structure

**Suite setup pattern (Arrange-Act-Assert):**
```cpp
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
// ... more mocks as needed
#else
#include <Arduino.h>
#endif

// Optionally inline source files when test_build_src = no
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_pipeline_bridge.cpp"

// Global state for the module under test
static HalDeviceManager* mgr;

void setUp() {
    // Reset ALL state before each test
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
    hal_pipeline_reset();
    ArduinoMock::reset();
    Preferences::reset();
}

void tearDown() {}  // Usually empty

// ===== Group description =====

// Test naming: test_<what_it_does>
void test_dac_available_sets_sink_mapping() {
    // Arrange
    TestAudioDevice dac("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dac, HAL_DISC_BUILTIN);
    dac._state = HAL_STATE_AVAILABLE;
    dac._ready = true;

    // Act
    hal_pipeline_on_device_available(slot);

    // Assert
    TEST_ASSERT_EQUAL(1, hal_pipeline_output_count());
    TEST_ASSERT_EQUAL(0, hal_pipeline_input_count());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_dac_available_sets_sink_mapping);
    // ... all test functions
    return UNITY_END();
}
```

**Patterns observed:**
- `setUp()` resets all relevant static/global state before every test
- Test functions are named `test_<what_it_does>` in snake_case
- Tests grouped by scenario with `// ===== Group description =====` banners
- Each test is self-contained: no shared state leaks between tests
- `TEST_FAIL_MESSAGE("reason")` used for unreachable code branches

### Source Inclusion Strategy (two patterns)

**Pattern 1: Self-contained reimplementation**
Used when the production source has too many platform dependencies. The test file re-implements just the logic under test:
- `test_auth/test_auth_handler.cpp` — re-implements PBKDF2, SHA256, session management inline
- `test_button/test_button_handler.cpp` — re-implements the button FSM inline
- `test_audio_pipeline/test_audio_pipeline.cpp` — re-implements conversion helpers inline

**Pattern 2: Direct source inclusion (inline-include)**
Used when the module is cleanly separable. Source `.cpp` files are `#include`d directly:
```cpp
// From test_hal_bridge.cpp:
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
#include "../../src/hal/hal_pipeline_bridge.cpp"
#include "../../src/hal/hal_settings.cpp"
```
This avoids needing `test_build_src = yes` while still exercising real source.

**Pattern 3: DSP source inclusion**
DSP tests include both the lite ANSI-C fallback implementations and the production headers:
```cpp
// From test_dsp/test_dsp.cpp:
#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"
#include "../../src/dsp_biquad_gen.c"
#include "../../src/dsp_pipeline.cpp"
#include "../../src/dsp_crossover.cpp"
```

### Mocking Strategy

**Mock location:** `test/test_mocks/` — all mocks are header-only (`.h` only, no `.cpp`).

**Shared mock headers:**

| Mock | What it simulates |
|------|------------------|
| `Arduino.h` | `String`, `millis()`, `delay()`, `analogRead()`, `digitalWrite()`, `ESP.getFreeHeap()`, LEDC, `ps_calloc()` |
| `WiFi.h` | `WiFiClass` with `status()`, `begin()`, `scanNetworks()`, static `addMockNetwork()`, `reset()` |
| `PubSubClient.h` | MQTT client with `publishedMessages` map, `subscribedTopics` vector, `wasMessagePublished()`, `getPublishedMessage()` |
| `Preferences.h` | NVS backed by `std::map<namespace, std::map<key, value>>`, `reset()` clears all |
| `LittleFS.h` | File system mock (read/write to memory) |
| `Wire.h` | I2C bus mock |
| `esp_timer.h` | Timer mock with `mockTimerUs` counter (manually advanced in tests) |
| `esp_random.h` | Deterministic random for test repeatability |
| `mbedtls/` | Real mbedtls SHA256 / PKCS5 on host (not mocked — real crypto tested) |

**Mock reset pattern (mandatory in setUp):**
```cpp
void setUp() {
    ArduinoMock::reset();         // Resets millis, analog, digital pins
    ArduinoMock::resetLedc();     // Resets LEDC call counters
    Preferences::reset();         // Clears all NVS storage
    PubSubClient::reset();        // Clears published messages + subscriptions
    WiFiClass::reset();           // Resets connection state and scan results
}
```

**Mock state injection:**
```cpp
// Arduino time control
ArduinoMock::mockMillis = 5000;  // Set millis() return value
ArduinoMock::mockTimerUs += SESSION_TIMEOUT_US + 1000000;  // Advance timer past expiry

// WiFi scan results
WiFiClass::addMockNetwork("HomeWiFi", -55, 6, true);
WiFiClass::addMockNetwork("Work", -70, 11, true);

// MQTT publish verification
TEST_ASSERT_TRUE(PubSubClient::wasMessagePublished("alx/sensor/state"));
std::string payload = PubSubClient::getPublishedMessage("alx/sensor/state");
```

**What to mock:**
- All Arduino platform APIs (GPIO, timing, LEDC, WiFi, MQTT, NVS, Preferences)
- LittleFS filesystem
- FreeRTOS types (stubbed to compile — not executed)
- ESP-IDF APIs (`esp_timer`, `esp_random`)

**What NOT to mock:**
- mbedtls crypto (real library compiled on host — crypto logic is tested for real)
- Pure C/C++ logic (DSP biquad math, coefficient computation, audio conversion math)
- `std::` containers used in production logic

### Float Comparison

Use `TEST_ASSERT_FLOAT_WITHIN(tolerance, expected, actual)`:
```cpp
#define FLOAT_TOL  0.001f   // General float equality
#define COEFF_TOL  0.01f    // DSP biquad coefficient tolerance

TEST_ASSERT_FLOAT_WITHIN(COEFF_TOL, 1.0f, dcGain);
TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.5f, f);
TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, f);  // Tighter tolerance for edge detection
```

### Test Device Pattern (HAL tests)

HAL tests create local subclasses of `HalDevice` with configurable probe/init/health results:
```cpp
class TestAudioDevice : public HalDevice {
public:
    bool probeResult = true;
    bool initResult  = true;
    bool healthResult = true;
    int  initCount   = 0;

    TestAudioDevice(const char* compat, HalDeviceType type,
                    uint16_t priority = HAL_PRIORITY_HARDWARE) {
        strncpy(_descriptor.compatible, compat, 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = type;
        _initPriority = priority;
    }
    void setCapabilities(uint8_t caps) { _descriptor.capabilities = caps; }

    bool probe() override { return probeResult; }
    HalInitResult init() override {
        initCount++;
        return initResult ? hal_init_ok() : hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return healthResult; }
};
```

---

## E2E Browser Tests (Playwright)

### Framework

**Runner:** Playwright (Chromium only)
**Mock backend:** Express server (`e2e/mock-server/server.js`, port 3000) serving real `web_src/` HTML
**WebSocket mocking:** `page.routeWebSocket(/.*:81/, handler)` — no real device or WS server

**Run Commands:**
```bash
cd e2e
npm install                                    # First time only
npx playwright install --with-deps chromium   # First time only
npx playwright test                            # Run all 26 tests
npx playwright test tests/auth.spec.js        # Run single spec
npx playwright test --headed                  # Visible browser
npx playwright test --debug                   # Debug inspector
```

### Test File Organization

**Location:** `e2e/tests/` — one spec file per feature area.

**Naming:** `<feature-area>.spec.js` — kebab-case matching the feature tab name.

```
e2e/
├── tests/                         # 19 spec files
│   ├── auth.spec.js               # Login page, session cookie
│   ├── auth-password.spec.js      # Password change flow
│   ├── hal-devices.spec.js        # HAL device cards, rescan, disable
│   ├── mqtt.spec.js               # MQTT toggle, fields, WS fixture values
│   ├── wifi.spec.js               # WiFi scan, network selection
│   ├── audio-inputs.spec.js       # Audio tab sub-nav, input strips
│   ├── audio-matrix.spec.js       # Routing matrix UI
│   ├── audio-outputs.spec.js      # Output channel strips
│   ├── audio-siggen.spec.js       # Signal generator controls
│   ├── peq-overlay.spec.js        # PEQ/crossover/compressor overlays
│   ├── settings.spec.js           # Settings tab toggles and persistence
│   ├── ota.spec.js                # OTA check/update flow
│   ├── navigation.spec.js         # Sidebar tab switching
│   ├── dark-mode.spec.js          # Theme toggle
│   ├── debug-console.spec.js      # Debug log filtering
│   ├── hardware-stats.spec.js     # Hardware stats display
│   ├── control-tab.spec.js        # Sensing mode and amp control
│   ├── responsive.spec.js         # Responsive layout
│   └── support.spec.js            # Support/manual tab
│
├── helpers/
│   ├── fixtures.js                # connectedPage fixture (session + WS auth)
│   ├── ws-helpers.js              # buildInitialState(), handleCommand(), binary frame builders
│   └── selectors.js               # Reusable DOM selectors
│
├── fixtures/
│   ├── ws-messages/               # 15 JSON WS broadcast fixtures
│   │   ├── wifi-status.json
│   │   ├── smart-sensing.json
│   │   ├── display-state.json
│   │   ├── buzzer-state.json
│   │   ├── mqtt-settings.json
│   │   ├── hal-device-state.json
│   │   ├── audio-channel-map.json
│   │   ├── audio-graph-state.json
│   │   ├── signal-generator.json
│   │   ├── debug-state.json
│   │   └── ... (5 more)
│   └── api-responses/             # 14 JSON REST response fixtures
│
└── mock-server/
    ├── server.js                  # Express app (port 3000)
    ├── assembler.js               # Replicates HTML assembly from web_src/
    ├── ws-state.js                # Deterministic mock state singleton
    └── routes/                    # 12 Express route files matching firmware REST API
        ├── auth.js, hal.js, wifi.js, mqtt.js
        ├── settings.js, ota.js, pipeline.js
        ├── dsp.js, sensing.js, siggen.js
        ├── diagnostics.js, system.js
```

### connectedPage Fixture

Most tests use `connectedPage` from `e2e/helpers/fixtures.js` instead of bare `page`. It provides:
1. Real session cookie acquired from `POST /api/auth/login` on the mock server
2. WebSocket connection intercepted with `page.routeWebSocket(/.*:81/, ...)`
3. WS auth handshake completed automatically (`authRequired` → `auth` → `authSuccess`)
4. All 10 initial-state fixture messages broadcast after `authSuccess`
5. Waits until `#wsConnectionStatus` text equals "Connected"

```javascript
const { test, expect } = require('../helpers/fixtures');

test('my test', async ({ connectedPage: page }) => {
    // Page is already connected with WS auth complete
    await page.locator('.sidebar-item[data-tab="mqtt"]').click();
    await expect(page.locator('#appState\\.mqttEnabled')).toBeChecked();

    // Inject additional WS messages:
    page.wsRoute.send({ type: 'smartSensing', ampOn: true, ... });
});
```

Tests that don't need WS (login page tests) use bare `{ test, expect }` from `@playwright/test`.

### Key Playwright Patterns

**Tab navigation — use `evaluate()` not sidebar clicks:**
```javascript
// Correct — avoids scroll issues with sidebar click events
await page.evaluate(() => switchTab('audio'));

// Also correct for sub-views inside the audio tab
await page.locator('.audio-subnav-btn[data-view="inputs"]').click();
```

**CSS-hidden toggle inputs — use `toBeChecked()` not `toBeVisible()`:**
```javascript
// Toggles are styled with label.switch (opacity:0; width:0; height:0 on <input>)
// State assertion:
await expect(page.locator('#appState\\.mqttEnabled')).toBeChecked();

// Interaction — click the parent label:
const label = page.locator('label.switch:has(#appState\\.mqttEnabled)');
await label.click();
```

**WS message injection:**
```javascript
// Send a JSON WS message from server to browser
page.wsRoute.send({ type: 'smartSensing', ampOn: true, signalDetected: false });

// Send a binary frame
page.wsRoute.sendBinary(buildWaveformFrame());
```

**API interception:**
```javascript
let called = false;
await page.route('/api/hal/scan', async (route) => {
    called = true;
    await route.fulfill({ status: 200, body: JSON.stringify({ status: 'ok', devicesFound: 6 }) });
});
await page.locator('#hal-rescan-btn').click();
await page.waitForTimeout(500);
expect(called).toBe(true);
```

**Strict mode — use `.first()` when multiple matches are possible:**
```javascript
const disableBtn = deviceList.locator('.hal-device-card.expanded button')
    .filter({ hasText: /Disable/i }).first();
```

**routeWebSocket event handlers use capital M/C:**
```javascript
// Correct: onMessage, onClose (capital M, C)
ws.onMessage((msg) => { ... });
ws.onClose(() => { ... });
// Not: ws.onmessage / ws.onclose
```

### WS Fixture System

Initial state is broadcast from 10 JSON fixture files in `e2e/fixtures/ws-messages/`. Each fixture is a complete WS broadcast message object loaded at test startup:

```javascript
// ws-helpers.js buildInitialState()
function buildInitialState() {
    return [
        'wifi-status',       // WiFi connection state
        'smart-sensing',     // Amp state, mode, levels
        'display-state',     // TFT backlight, brightness
        'buzzer-state',      // Buzzer enabled, volume
        'mqtt-settings',     // Broker, topic, HA discovery
        'hal-device-state',  // All 6 HAL device cards
        'audio-channel-map', // Input/output device mapping
        'audio-graph-state', // Waveform, spectrum data
        'signal-generator',  // SigGen parameters
        'debug-state',       // Debug toggles
    ].map(loadFixture);
}
```

When adding new WS broadcast types, add a corresponding fixture file and update `buildInitialState()`.

### Mock Server Routes

Each route file in `e2e/mock-server/routes/` mirrors a section of the firmware's REST API:
- `auth.js` — `POST /api/auth/login`, `POST /api/auth/logout`, `GET /api/auth/status`
- `hal.js` — `GET /api/hal/devices`, `POST /api/hal/scan`, `PUT /api/hal/devices`, `DELETE /api/hal/devices`, `GET /api/hal/db/presets`
- `wifi.js` — `GET /api/wifi/status`, `POST /api/wifi/connect`, `GET /api/wifi/scan`
- `dsp.js` — DSP config CRUD endpoints
- etc.

State is held in `ws-state.js` singleton, reset between tests via `POST /api/__test__/reset`.

---

## CI Quality Gates

Four parallel jobs run on every push/PR to `main` and `develop`. The firmware `build` job only runs after all four pass:

```yaml
# .github/workflows/tests.yml
jobs:
  cpp-tests:   # pio test -e native -v
  cpp-lint:    # cppcheck on src/ (excludes src/gui/)
  js-lint:     # find_dups.js + check_missing_fns.js + ESLint
  e2e-tests:   # cd e2e && npx playwright test (Chromium only)
  build:
    needs: [cpp-tests, cpp-lint, js-lint, e2e-tests]
```

**cpp-lint command:**
```bash
cppcheck --enable=warning,style,performance \
         --suppress=missingInclude \
         --suppress=unusedFunction \
         --std=c++11 \
         --error-exitcode=1 \
         -i src/gui/ src/
```

**js-lint commands:**
```bash
node tools/find_dups.js                           # Duplicate JS declarations
node tools/check_missing_fns.js                  # Undefined function references
cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json
```

Playwright HTML report is uploaded as an artifact (14-day retention) on E2E failure.

## Pre-commit Hooks

Located at `.githooks/pre-commit`. Activate with: `git config core.hooksPath .githooks`

Runs three fast checks before every commit:
1. `node tools/find_dups.js` — catches duplicate `let`/`const` across concatenated JS scope
2. `node tools/check_missing_fns.js` — catches calls to undefined functions
3. `cd e2e && npx eslint ../web_src/js/` — ESLint with the project's rule set

## Coverage Requirements

**C++ tests:** No minimum percentage enforced. Coverage is requirement-driven: every new module needs a test file. Changed function signatures require updating affected test files. The 1503-test count is maintained across all changes.

**E2E tests:** Feature-driven: every new toggle/button/dropdown needs a test verifying the WS command it sends. Every new WS broadcast type needs a fixture JSON + DOM verification test. Every new tab/section needs navigation + element presence tests.

**No coverage tooling (lcov/istanbul)** is configured. Coverage is enforced via the mandatory test rules in `CLAUDE.md`.

## Mandatory Coverage Rules Summary

For any code change, before marking complete:

| Change type | Required action |
|-------------|----------------|
| New `src/` module | Add `test/test_<module>/test_<module>.cpp` |
| Changed C++ function signature | Update all affected test includes |
| New `web_src/` toggle or button | Add Playwright test verifying WS command |
| New WS broadcast field | Add/update fixture JSON + DOM test |
| New REST API endpoint | Add route in `e2e/mock-server/routes/` + fixture + test |
| Changed element IDs in HTML | Update `e2e/helpers/selectors.js` + affected specs |
| New JS top-level declaration | Add to `web_src/.eslintrc.json` globals |

---

*Testing analysis: 2026-03-08*
