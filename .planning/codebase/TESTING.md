# Testing Patterns

**Analysis Date:** 2026-03-08

## Test Framework

**C++ Unit Tests:**
- Runner: Unity (via PlatformIO `test_framework = unity`)
- Platform: `native` (host GCC/MinGW — no hardware needed)
- Config: `platformio.ini` `[env:native]` section
- Build flags: `-D UNIT_TEST -D NATIVE_TEST -std=c++11`

**E2E Browser Tests:**
- Runner: Playwright `@playwright/test`
- Browser: Chromium only
- Config: `e2e/playwright.config.js`
- Mock server: Express.js on port 3000 (`e2e/mock-server/server.js`)

**Run Commands:**
```bash
# C++ unit tests — all modules
pio test -e native

# C++ unit tests — verbose output
pio test -e native -v

# C++ unit tests — single module
pio test -e native -f test_wifi
pio test -e native -f test_mqtt
pio test -e native -f test_auth

# E2E tests — all specs
cd e2e && npx playwright test

# E2E tests — single spec
cd e2e && npx playwright test tests/auth.spec.js

# E2E tests — headed browser (visible)
cd e2e && npx playwright test --headed

# E2E tests — debug mode
cd e2e && npx playwright test --debug
```

## C++ Test File Organization

**Location:** Each test module in its own directory under `test/`:
```
test/
├── test_mocks/         # Shared mock headers (NOT a test module)
│   ├── Arduino.h
│   ├── Preferences.h
│   ├── WiFi.h
│   ├── PubSubClient.h
│   ├── LittleFS.h
│   ├── Wire.h
│   ├── ETH.h
│   ├── IPAddress.h
│   ├── esp_random.h
│   ├── esp_timer.h
│   ├── i2s_std_mock.h
│   └── mbedtls/        # mbedtls mock headers (md.h, pkcs5.h)
├── test_auth/
│   └── test_auth_handler.cpp
├── test_websocket/
│   └── test_websocket_handler.cpp
├── test_hal_bridge/
│   └── test_hal_bridge.cpp
└── ...
```

**Naming:**
- Directory: `test_<module_name>/`
- File: `test_<module_name>.cpp` or `test_<subject>.cpp`
- Test functions: `test_<what_is_tested>(void)`

**Critical rule:** Each test directory must contain exactly one `main()` entry point and one `setUp()`/`tearDown()` pair. Multiple test modules in one directory cause duplicate symbol linker errors.

**platformio.ini rule:** `test_build_src = no` — tests do NOT compile `src/` automatically. Each test file includes specific source files (or their dependencies) directly.

## C++ Test Structure

**Standard entry point pattern (both native and hardware):**
```cpp
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
// ... additional mocks as needed
#else
#include <Arduino.h>
#include <Preferences.h>
#endif

// ===== Test State Reset Namespace =====
namespace TestModuleState {
void reset() {
    // Reset all module-local state
    ArduinoMock::reset();
}
} // namespace TestModuleState

void setUp(void) { TestModuleState::reset(); }
void tearDown(void) { /* usually empty */ }

// ===== Tests =====
void test_some_behavior(void) {
    // Arrange
    // Act
    // Assert
    TEST_ASSERT_TRUE(condition);
}

// ===== Test Runner =====
int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_some_behavior);
    return UNITY_END();
}

// Native entry point
#ifdef NATIVE_TEST
int main(void) { return runUnityTests(); }
#endif

// Arduino entry point (for hardware flashing)
#ifndef NATIVE_TEST
void setup() { delay(2000); runUnityTests(); }
void loop() {}
#endif
```

**Arrange-Act-Assert pattern:**
```cpp
void test_rate_limit_resets_on_successful_login(void) {
    // Arrange: accumulate 3 failed attempts
    mockWebPassword = hashPasswordPbkdf2("correct_password");
    unsigned long retryAfter = 0;
    for (int i = 0; i < 3; i++) {
        simulateLogin("wrong_password", retryAfter);
        ArduinoMock::mockMillis += 50000;
    }

    // Act: successful login
    int status = simulateLogin("correct_password", retryAfter);

    // Assert
    TEST_ASSERT_EQUAL(200, status);
}
```

## Mocking Strategy

**Mock library location:** `test/test_mocks/` — header-only implementations.

**Mock philosophy:** Each mock header mirrors the real Arduino/ESP-IDF library API. Mocks use static/global state with a `reset()` function so `setUp()` can restore clean state between tests.

**Arduino mock (`test/test_mocks/Arduino.h`):**
```cpp
namespace ArduinoMock {
    static unsigned long mockMillis = 0;
    static unsigned long mockMicros = 0;
    static int mockAnalogValue = 0;
    static int mockDigitalPins[50] = {0};
    // LEDC state for buzzer tests
    static double ledcLastFreq = 0;
    // ESP timer mock (for auth rate limiting)
    static uint64_t mockTimerUs = 0;

    inline void reset() { /* zero all */ }
}
inline unsigned long millis() { return ArduinoMock::mockMillis; }
```

**Time control in tests:** Advance `ArduinoMock::mockMillis` and `ArduinoMock::mockTimerUs` directly to simulate elapsed time without calling `delay()`:
```cpp
ArduinoMock::mockMillis += 50000;     // advance millis by 50s
ArduinoMock::mockTimerUs += SESSION_TIMEOUT_US + 1; // trigger session expiry
```

**Preferences mock (`test/test_mocks/Preferences.h`):**
- Backed by `static std::map<string, map<string, string>> storage`
- Supports `getString`, `putString`, `getBool`, `putBool`, `getInt`, `putInt`, `getDouble`, `putDouble`, `isKey`, `remove`, `clear`
- `Preferences::reset()` clears all namespaces between tests

**WiFi mock (`test/test_mocks/WiFi.h`):**
- Static members for scan results, connection status, RSSI, IP addresses
- `WiFiClass::reset()` restores defaults; `addMockNetwork()` populates scan results

**PubSubClient mock (`test/test_mocks/PubSubClient.h`):**
- Tracks publish calls, connection state, callback registration
- Static reset for clean state per test

**What to mock:**
- All Arduino platform APIs (`millis()`, `analogRead()`, `digitalWrite()`, `delay()`)
- All NVS/Preferences operations
- All network operations (WiFi, MQTT)
- File system (LittleFS mock via `test/test_mocks/LittleFS.h`)
- ESP-IDF primitives (`esp_random.h`, `esp_timer.h`)
- Cryptography (`mbedtls/md.h`, `mbedtls/pkcs5.h`)

**What NOT to mock:**
- Pure computation logic (DSP coefficients, FFT math, version comparison)
- Data structures (enums, structs, constants from `src/`)

## Inline Source Inclusion Pattern

Since `test_build_src = no`, tests must include source files directly for the code under test. Two patterns are used:

**Pattern 1 — Include headers only (most common):**
Tests re-implement the logic locally (mirrors production `auth_handler.cpp` in `test_auth_handler.cpp`) so the test is self-contained and doesn't pull in all production dependencies.

**Pattern 2 — Inline `.cpp` files (for integration-style tests):**
Used in HAL tests that need the full implementation chain:
```cpp
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
#include "../../src/hal/hal_pipeline_bridge.cpp"
#include "../../src/hal/hal_settings.cpp"
```
Used in DSP tests:
```cpp
#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"
#include "../../src/dsp_biquad_gen.c"
#include "../../src/dsp_pipeline.cpp"
#include "../../src/dsp_coefficients.cpp"
```

## Fixtures and State Reset

**Test namespace pattern:** Each test file defines a `TestXxxState` namespace with a `reset()` function called from `setUp()`:
```cpp
namespace TestWSState {
void reset() {
    wsClients.clear();
    lastBroadcastMessage.clear();
    broadcastCount = 0;
    for (int i = 0; i < MAX_WS_CLIENTS; i++) wsAuthStatus[i] = false;
    _wsAuthCount = 0;
#ifdef NATIVE_TEST
    ArduinoMock::reset();
#endif
}
} // namespace TestWSState

void setUp(void) { TestWSState::reset(); }
```

**HAL test fixtures:** Use `setUp()` to call `mgr->reset()`, `hal_registry_reset()`, and `hal_pipeline_reset()` to clear singleton state between tests.

**Float tolerance:** `#define FLOAT_TOL 0.001f` and `#define COEFF_TOL 0.01f` — use `TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, expected, actual)` for all float comparisons.

## Common Unity Assertions

```cpp
TEST_ASSERT_TRUE(condition);
TEST_ASSERT_FALSE(condition);
TEST_ASSERT_EQUAL(expected, actual);           // integer equality
TEST_ASSERT_EQUAL_STRING(expected, actual);    // c-string equality
TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual);
TEST_ASSERT_GREATER_THAN(threshold, actual);
TEST_ASSERT_LESS_THAN(threshold, actual);
TEST_ASSERT_NULL(pointer);
TEST_ASSERT_NOT_NULL(pointer);
TEST_FAIL_MESSAGE("descriptive message");
```

## Test Groups and Comments

Tests are grouped by category using comment banners and `UNITY_BEGIN`/`UNITY_END` contains all tests in a single runner:
```cpp
// ===== Session Management Tests =====
void test_session_creation_empty_slot(void) { ... }

// ===== Non-blocking Rate Limiting Tests (Phase 2) =====
void test_rate_limit_429_after_5_failures(void) { ... }

int runUnityTests(void) {
    UNITY_BEGIN();

    // Session management tests
    RUN_TEST(test_session_creation_empty_slot);

    // Rate limiting tests
    RUN_TEST(test_rate_limit_429_after_5_failures);

    return UNITY_END();
}
```

## E2E Test File Organization

**Location:**
```
e2e/
├── tests/               # 19 spec files
│   ├── auth.spec.js
│   ├── hal-devices.spec.js
│   ├── audio-inputs.spec.js
│   └── ...
├── helpers/
│   ├── fixtures.js      # connectedPage fixture (WS auth + session)
│   ├── ws-helpers.js    # buildInitialState(), handleCommand(), binary frame builders
│   └── selectors.js     # Reusable DOM selectors
├── fixtures/
│   ├── ws-messages/     # 15 JSON WS broadcast fixtures (kebab-case filenames)
│   │   ├── wifi-status.json
│   │   ├── hal-device-state.json
│   │   ├── audio-channel-map.json
│   │   └── ...
│   └── api-responses/   # 14 REST response fixtures
├── mock-server/
│   ├── server.js        # Express app wiring all routes
│   ├── assembler.js     # HTML assembly from web_src/ (mirrors build_web_assets.js)
│   ├── ws-state.js      # Deterministic mock state singleton, reset between tests
│   └── routes/          # 12 Express route files (auth, hal, wifi, mqtt, etc.)
└── playwright.config.js
```

## E2E connectedPage Fixture

All tests needing a connected, authenticated browser session use the `connectedPage` fixture from `e2e/helpers/fixtures.js`:

```javascript
const { test, expect } = require('../helpers/fixtures');

test('my test', async ({ connectedPage: page }) => {
    // page has: valid sessionId cookie, WS authenticated, initial state broadcast
    // page.wsRoute.send(obj) — push additional WS messages
});
```

The fixture:
1. Acquires a real session cookie from `POST /api/auth/login` (mock server returns any password as valid)
2. Sets the cookie in the browser context before navigation
3. Intercepts all WebSocket connections to `*:81` via `page.routeWebSocket(/.*:81/, handler)`
4. Handles the WS auth handshake (`authRequired` → client sends `{type:'auth'}` → server sends `authSuccess`)
5. Broadcasts all 10 initial-state fixture messages from `buildInitialState()`
6. Waits for `#wsConnectionStatus` to read "Connected"

## E2E Test Patterns

**WS interception:**
```javascript
await page.routeWebSocket(/.*:81/, (ws) => {
    ws.onMessage((msg) => { /* handle outbound */ });
    ws.onClose(() => {});
    ws.send(JSON.stringify({ type: 'authRequired' }));
});
```
Use `onMessage` / `onClose` with capital M/C (Playwright API).

**Tab navigation:** Use `page.evaluate()` to avoid scroll issues with sidebar clicks:
```javascript
await page.evaluate(() => switchTab('tabName'));
```

**CSS-hidden checkbox toggles:** Use `toBeChecked()` not `toBeVisible()`:
```javascript
await expect(page.locator('#someToggle')).toBeChecked();
```

**Strict mode — multiple elements:** Use `.first()` when selector may match multiple:
```javascript
const card = deviceList.locator('.hal-device-card').first();
```

**REST API interception:**
```javascript
let scanCalled = false;
await page.route('/api/hal/scan', async (route) => {
    scanCalled = true;
    await route.fulfill({ status: 200, body: JSON.stringify({ status: 'ok' }) });
});
await page.locator('#hal-rescan-btn').click();
expect(scanCalled).toBe(true);
```

**WS message injection (pushing state to frontend):**
```javascript
page.wsRoute.send({ type: 'hardwareStats', freeHeap: 150000, ... });
await expect(page.locator('#free-heap')).toContainText('150');
```

**Binary WS frames (audio data):**
```javascript
const buf = buildWaveformFrame(); // from e2e/helpers/ws-helpers.js
page.wsRoute.sendBinary(buf);
```

## WS Message Fixtures

Fixtures in `e2e/fixtures/ws-messages/` are kebab-case JSON files loaded by `buildInitialState()` in the order:
1. `wifi-status.json`
2. `smart-sensing.json`
3. `display-state.json`
4. `buzzer-state.json`
5. `mqtt-settings.json`
6. `hal-device-state.json`
7. `audio-channel-map.json`
8. `audio-graph-state.json`
9. `signal-generator.json`
10. `debug-state.json`

When adding a new WS broadcast type: create a fixture JSON, add it to `buildInitialState()` in `ws-helpers.js`, and update `ws-state.js` if the mock server needs to track new state fields.

## Coverage

**Counts (current):**
- C++ unit tests: 1,556 tests across 65 modules
- E2E browser tests: 26 tests across 19 specs

**Requirements:** No formal coverage percentage enforced, but every new module must have a corresponding test file and every public function change must update affected tests.

**Coverage gap policy:** New code must come with tests. WebSocket protocol changes require fixture updates and Playwright tests. REST API changes require mock-server route updates and Playwright tests.

## CI Quality Gates

4 parallel jobs all must pass before the `build` job runs (`.github/workflows/tests.yml`):

| Job | Command | What it checks |
|-----|---------|----------------|
| `cpp-tests` | `pio test -e native -v` | All 1,556 Unity C++ tests |
| `cpp-lint` | `cppcheck ... src/` | C++ static analysis (excludes `src/gui/`) |
| `js-lint` | `find_dups.js` + `check_missing_fns.js` + ESLint | JS duplicates, undefined refs, code quality |
| `e2e-tests` | `npx playwright test` | All 26 Playwright browser tests |

The `build` job (firmware compile) only runs after all 4 gates pass:
```yaml
build:
    needs: [cpp-tests, cpp-lint, js-lint, e2e-tests]
```

CI triggers on push/PR to `main` and `develop` branches. Playwright HTML report uploaded as artifact on failure (14-day retention).

## Pre-commit Hooks

`.githooks/pre-commit` runs 3 fast checks before every commit:
```bash
git config core.hooksPath .githooks   # activate hooks
```

Checks:
1. `node tools/find_dups.js` — duplicate JS declarations
2. `node tools/check_missing_fns.js` — undefined function references
3. `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`

## Mandatory Test Coverage Rules

**C++ firmware changes (`src/`):**
- New module → new `test/test_<module>/` directory with test file
- Changed function signatures → update all affected test files
- Verify: `pio test -e native -v`

**Web UI changes (`web_src/`):**
- New toggle/button/dropdown → add Playwright test verifying it sends correct WS command
- New WS broadcast type → add fixture JSON + test verifying DOM updates
- New tab or section → add navigation + element presence tests
- Changed element IDs → update `e2e/helpers/selectors.js` + affected specs
- New top-level JS declarations → add to `web_src/.eslintrc.json` globals section
- Verify: `cd e2e && npx playwright test`

**WebSocket protocol changes (`src/websocket_handler.cpp`):**
- Update `e2e/fixtures/ws-messages/` with new/changed message fixtures
- Update `e2e/helpers/ws-helpers.js` `buildInitialState()` and `handleCommand()`
- Update `e2e/mock-server/ws-state.js` if new state fields are added

**REST API changes:**
- Update matching route in `e2e/mock-server/routes/*.js`
- Update `e2e/fixtures/api-responses/` fixtures

---

*Testing analysis: 2026-03-08*
