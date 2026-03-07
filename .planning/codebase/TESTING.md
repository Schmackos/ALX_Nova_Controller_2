# Testing Patterns

**Analysis Date:** 2026-03-07

## Test Frameworks

**C++ Unit Tests:**
- Runner: Unity (via PlatformIO `test_framework = unity`)
- Platform: `native` (host machine, gcc/MinGW — no hardware required)
- Config: `platformio.ini` `[env:native]` section
- 1,271+ tests across 57+ test modules

**Browser / E2E Tests:**
- Runner: Playwright
- Config: `e2e/playwright.config.js`
- 26 tests across 19 specs
- Mock server: Express (`e2e/mock-server/server.js`, port 3000)

**Static Analysis (CI-only):**
- `cppcheck` on `src/` (excluding `src/gui/`)
- ESLint on `web_src/js/` via `web_src/.eslintrc.json`
- `node tools/find_dups.js` — duplicate JS declarations
- `node tools/check_missing_fns.js` — undefined JS function references

**Run Commands:**
```bash
# C++ unit tests (all modules)
pio test -e native

# Verbose C++ tests
pio test -e native -v

# Single C++ test module
pio test -e native -f test_hal_core
pio test -e native -f test_hal_bridge

# E2E browser tests
cd e2e && npx playwright test

# Single E2E spec
cd e2e && npx playwright test tests/hal-devices.spec.js

# E2E with visible browser
cd e2e && npx playwright test --headed

# E2E debug mode
cd e2e && npx playwright test --debug

# ESLint
cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json

# Duplicate/missing JS checks
node tools/find_dups.js && node tools/check_missing_fns.js
```

## Test File Organization

**C++ tests:**
- Location: `test/` — each module in its own directory (required to prevent duplicate `main`/`setUp`/`tearDown` symbols)
- Naming: `test_<module>/test_<module>.cpp` or `test_<module>/test_<module>_<topic>.cpp`
- Examples:
  ```
  test/test_utils/test_utils.cpp
  test/test_hal_core/test_hal_core.cpp
  test/test_hal_retry/test_hal_retry.cpp
  test/test_hal_bridge/test_hal_bridge.cpp
  test/test_smart_sensing/test_smart_sensing_logic.cpp
  ```
- Mocks: `test/test_mocks/` (shared across all test modules)

**E2E tests:**
- Location: `e2e/tests/*.spec.js`
- Fixtures: `e2e/fixtures/ws-messages/*.json` (15 WS broadcast fixtures) and `e2e/fixtures/api-responses/*.json` (14 REST response fixtures)
- Helpers: `e2e/helpers/` (`fixtures.js`, `ws-helpers.js`, `selectors.js`)
- Mock routes: `e2e/mock-server/routes/*.js` (12 route files matching firmware REST API)

**platformio.ini native environment:**
```ini
test_build_src = no      # Tests do NOT compile src/ directly
test_ignore = test_mocks # test_mocks is not a test module
build_flags = -D UNIT_TEST -D NATIVE_TEST -D DSP_ENABLED ...
```

## C++ Test Structure

**Module pattern (Arrange-Act-Assert):**
```cpp
#include <unity.h>
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Inline .cpp files for native testing (since test_build_src = no)
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// Test device / mock implementation
class TestDevice : public HalDevice {
public:
    bool initResult;
    int  initCallCount;

    TestDevice(const char* compatible, HalDeviceType type) { ... }

    bool probe() override { return true; }
    HalInitResult init() override {
        initCallCount++;
        return initResult ? hal_init_ok() : hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return true; }
};

// Fixture state
static HalDeviceManager* mgr;

void setUp() {
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
    ArduinoMock::mockMillis = 0;
}

void tearDown() {}

// Test naming: test_<what>_<expected_outcome>
void test_hal_init_ok_returns_success() {
    HalInitResult r = hal_init_ok();

    TEST_ASSERT_TRUE(r.success);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)DIAG_OK, r.errorCode);
    TEST_ASSERT_EQUAL_CHAR('\0', r.reason[0]);
}

// Test runner
int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hal_init_ok_returns_success);
    RUN_TEST(test_hal_init_fail_returns_failure);
    // ...
    return UNITY_END();
}

#ifdef NATIVE_TEST
int main(void) { return runUnityTests(); }
#endif
#ifndef NATIVE_TEST
void setup() { delay(2000); runUnityTests(); }
void loop() {}
#endif
```

**Grouping pattern:**
Tests are grouped with section-comment headers and numbered:
```cpp
// =====================================================================
// Group 1: HalInitResult Basics (5 tests)
// =====================================================================
// 1. hal_init_ok() returns success with correct defaults
void test_hal_init_ok_returns_success() { ... }
// 2. hal_init_fail() returns failure with error code and reason
void test_hal_init_fail_returns_failure() { ... }
```

## Mocking (C++)

**Mock location:** `test/test_mocks/`

**Available mocks:**
- `Arduino.h` — `String` class, GPIO functions, `millis()`/`micros()`/`delay()`, `analogRead()`, `digitalWrite()`, LEDC, PSRAM (`ps_calloc`), `ArduinoMock::reset()`
- `Preferences.h` — Full NVS mock using `std::map<string,map<string,string>>` keyed by namespace. Has static `Preferences::reset()` to clear all storage between tests
- `PubSubClient.h` — MQTT client mock with `PubSubClient::reset()` and call tracking
- `LittleFS.h` — Filesystem stub
- `WiFi.h`, `Wire.h`, `ETH.h` — Peripheral stubs
- `i2s_std_mock.h` — I2S driver stub
- `esp_timer.h`, `esp_random.h` — IDF stubs

**Key mock patterns:**

Controlling time:
```cpp
ArduinoMock::mockMillis = 5000; // Advance time for retry tests
```

Resetting all state in setUp:
```cpp
void setUp() {
    ArduinoMock::reset();
    Preferences::reset();
    PubSubClient::reset();
}
```

Module state reset with namespace:
```cpp
namespace TestMQTTState {
    void reset() {
        mqttSettings.broker = "";
        mqttSettings.port = 1883;
        PubSubClient::reset();
        Preferences::reset();
        ArduinoMock::reset();
    }
}

void setUp() { TestMQTTState::reset(); }
```

**What to mock:**
- All Arduino hardware calls (GPIO, I2S, LEDC, millis)
- NVS/Preferences storage
- MQTT client operations
- LittleFS filesystem

**What NOT to mock:**
- The module under test's own logic — use `#include "../../src/module.cpp"` to inline the real implementation
- ArduinoJson parsing — use the real library (included in native env deps)

**Inline .cpp technique** — since `test_build_src = no`, tests directly include implementation files:
```cpp
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
```

## Fixtures and Factories (C++)

**Test device pattern** — each test module defines its own `TestDevice` subclass of `HalDevice`:
```cpp
class TestDevice : public HalDevice {
public:
    bool probeResult   = true;
    bool initResult    = true;
    bool healthResult  = true;
    int  initCallCount = 0;

    TestDevice(const char* compatible, HalDeviceType type,
               uint16_t priority = HAL_PRIORITY_HARDWARE) {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = type;
        _initPriority = priority;
    }
    // ... override all 5 pure virtuals
};
```

**State injection** — mock state is set directly on the struct/object under test:
```cpp
dac._state = HAL_STATE_AVAILABLE;
dac._ready = true;
TestState::currentMode = TestState::SMART_AUTO;
```

## E2E Test Structure (Playwright)

**connectedPage fixture** — all stateful tests use this custom fixture from `e2e/helpers/fixtures.js`:
- Acquires a real session cookie from the mock server's `/api/auth/login`
- Intercepts WebSocket connections to `*:81` via `page.routeWebSocket()`
- Completes WS auth handshake (`authRequired` → `auth` → `authSuccess`)
- Broadcasts all 10 initial-state fixture messages after auth
- Waits for `#wsConnectionStatus` to show "Connected" before yielding

```javascript
const { test, expect } = require('../helpers/fixtures');

test('device cards render', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="devices"]').click();
  const cards = page.locator('#hal-device-list .hal-device-card');
  await expect(cards).toHaveCount(6, { timeout: 5000 });
});
```

**Sending additional WS messages in tests:**
```javascript
page.wsRoute.send({ type: 'smartSensing', amplifierState: true });
page.wsRoute.sendBinary(buildWaveformFrame());
```

**Tab navigation in tests** — use `page.evaluate()` to call `switchTab()` rather than clicking sidebar (avoids scroll issues at 720px viewport height):
```javascript
await page.evaluate((tabId) => switchTab(tabId), 'audio');
await expect(page.locator('#audio')).toHaveClass(/active/);
```

**CSS-hidden checkboxes** — toggle inputs styled with `label.switch` require `toBeChecked()` not `toBeVisible()`:
```javascript
await expect(page.locator('#toggleBuzzer')).toBeChecked();
// NOT: await expect(page.locator('#toggleBuzzer')).toBeVisible();
```

**Intercepting REST calls:**
```javascript
let scanCalled = false;
await page.route('/api/hal/scan', async (route) => {
    scanCalled = true;
    await route.fulfill({ status: 200, body: JSON.stringify({ status: 'ok' }) });
});
await page.locator('#hal-rescan-btn').click();
expect(scanCalled).toBe(true);
```

**Strict mode** — use `.first()` when a selector might match multiple elements:
```javascript
const firstCard = cards.first();
```

## E2E Fixtures

**WS message fixtures** (`e2e/fixtures/ws-messages/*.json`):
- `wifi-status.json`, `smart-sensing.json`, `display-state.json`, `buzzer-state.json`
- `mqtt-settings.json`, `hal-device-state.json`, `audio-channel-map.json`, `audio-graph-state.json`
- `signal-generator.json`, `debug-state.json`, `audio-levels.json`, `auth-required.json`, `auth-success.json`, `debug-log.json`, `hardware-stats.json`

**API response fixtures** (`e2e/fixtures/api-responses/*.json`):
- `hal-devices.json`, `hal-presets.json`, `wifi-list.json`, `wifi-scan.json`, `wifi-status.json`
- `mqtt-config.json`, `settings.json`, `signal-generator.json`, `smart-sensing.json`
- `releases.json`, `check-update.json`, `auth-status.json`, `diagnostics.json`, `pipeline-matrix.json`

**DOM selectors** — centralized in `e2e/helpers/selectors.js` as a `SELECTORS` object. All selectors reference real HTML IDs/classes from `web_src/index.html`.

## Coverage

**C++ coverage:** No coverage tooling configured. `pio test -e native` runs the full suite but does not measure coverage.

**E2E coverage:** No formal coverage measurement. Tests are structured to cover:
- Tab navigation (all 8 tabs)
- Authentication flow (login, session, logout)
- Core UI features: HAL devices, WiFi, MQTT, OTA, settings, audio, debug console

**Coverage enforcement:** Mandatory by policy (see CLAUDE.md). Every code change must keep both test suites green before the task is considered complete.

## CI/CD Quality Gates

All 4 gates run in parallel and must pass before firmware build:

1. `cpp-tests` — `pio test -e native -v` (1,271+ Unity tests, 57+ modules)
2. `cpp-lint` — cppcheck on `src/` (excluding `src/gui/`)
3. `js-lint` — `find_dups.js` + `check_missing_fns.js` + ESLint on `web_src/js/`
4. `e2e-tests` — Playwright (26 tests, 19 specs, Chromium only)

**Pre-commit hooks** (`.githooks/pre-commit`):
1. `node tools/find_dups.js`
2. `node tools/check_missing_fns.js`
3. ESLint on `web_src/js/`

Activate: `git config core.hooksPath .githooks`

## Test Coverage Rules (Mandatory)

**C++ firmware changes (`src/`):**
- Run `pio test -e native -v`
- New modules: add `test/test_<module>/test_<module>.cpp`
- Changed function signatures: update affected test modules

**Web UI changes (`web_src/`):**
- Run `cd e2e && npx playwright test`
- New toggle/button/dropdown: add test verifying correct WS command is sent
- New WS broadcast type: add fixture JSON + test verifying DOM update
- New tab or section: add navigation + element presence tests
- Changed element IDs: update `e2e/helpers/selectors.js` + affected specs
- New top-level JS declarations: add to `web_src/.eslintrc.json` globals

**WebSocket protocol changes (`src/websocket_handler.cpp`):**
- Update `e2e/fixtures/ws-messages/` with new/changed fixture
- Update `e2e/helpers/ws-helpers.js` (`buildInitialState()` and `handleCommand()`)
- Update `e2e/mock-server/ws-state.js` if new state fields are added

**REST API changes (`src/main.cpp`, `src/hal/hal_api.cpp`):**
- Update matching route in `e2e/mock-server/routes/*.js`
- Update `e2e/fixtures/api-responses/` if response shape changes

## Test Type Summary

**Unit Tests (C++/Unity):**
- Scope: Pure logic — version comparison, sensing logic, DSP coefficients, HAL lifecycle, retry, settings persistence
- No hardware — all peripherals mocked
- File-level state isolated with `setUp()` and `namespace TestXxxState::reset()`

**Integration Tests (C++/Unity):**
- `test/test_hal_integration/` — HAL manager + bridge + pipeline together
- `test/test_hal_bridge/` — bridge mapping tables
- Uses same inline .cpp technique; tests cross-module interactions

**E2E Tests (Playwright/Chromium):**
- Scope: Frontend UI behaviour against mock REST + WS server
- No real firmware — Express mock server at port 3000
- Tests: login, tab navigation, sensor state display, HAL device cards, WiFi/MQTT settings forms, OTA controls, audio sub-views, PEQ overlay, debug console, dark mode, responsive layout

---

*Testing analysis: 2026-03-07*
