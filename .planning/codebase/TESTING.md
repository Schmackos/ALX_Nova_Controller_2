# Testing Patterns

**Analysis Date:** 2026-03-22

## Test Framework

**C++ Unit Tests (Unity + native platform):**
- Framework: Unity (built into PlatformIO)
- Platform: `native` environment compiles with GCC/MinGW on host machine
- Total: ~2335 tests across 91 modules (90 test files in `test/test_*/`)
- Config: `platformio.ini` `[env:native]`

**Run Commands:**
```bash
pio test -e native              # Run all tests
pio test -e native -v           # Verbose output (shows each test result)
pio test -e native -f test_auth # Run single test module
```

**Configuration (`platformio.ini` native environment):**
```ini
[env:native]
platform = native
test_framework = unity
build_flags =
  -std=c++11
  -D UNIT_TEST
  -D NATIVE_TEST
  -D DSP_ENABLED
  -D DSP_MAX_STAGES=24
  -D DSP_PEQ_BANDS=10
  -D DAC_ENABLED
  -D USB_AUDIO_ENABLED
test_build_src = no          # Don't compile src/ directly — tests include headers + use mocks
lib_ignore = WebSockets
test_ignore = test_mocks
```

**Browser/E2E Tests (Playwright):**
- Framework: Playwright Test (browser automation, Chromium only)
- Total: ~98 tests across 22 spec files
- Config: `e2e/playwright.config.js`

**Run Commands:**
```bash
cd e2e
npm install                                    # First time only
npx playwright install --with-deps chromium   # First time only
npx playwright test                           # Run all tests
npx playwright test tests/auth.spec.js        # Run single spec
npx playwright test --headed                  # Run with visible browser
npx playwright test --debug                   # Debug mode with inspector
npx playwright test --reporter=html           # Generate HTML report
```

## Test File Organization

**Location (C++):**
- Each test module in its own directory: `test/test_<module>/test_<module>.cpp`
- Separate directories prevent duplicate `main`/`setUp`/`tearDown` symbols
- Mock implementations shared in `test/test_mocks/` (13 mock files + mbedtls subdirectory)

**Directory structure:**
```
test/
├── test_auth/
│   └── test_auth_handler.cpp       # Auth: PBKDF2, sessions, rate limiting
├── test_mqtt/
│   └── test_mqtt_handler.cpp       # MQTT: settings, connection, publish
├── test_hal_discovery/
│   └── test_hal_discovery.cpp      # HAL: I2C bus scan, EEPROM probe
├── test_hal_es9822pro/
│   └── test_hal_es9822pro.cpp      # ESS SABRE ADC driver tests
├── test_audio_pipeline/
│   └── test_audio_pipeline.cpp     # Pipeline: matrix routing, sinks
├── test_http_security/
│   └── test_http_security.cpp      # HTTP security header validation
├── test_ws_adaptive_rate/
│   └── test_ws_adaptive_rate.cpp   # WS binary frame rate scaling
├── test_hal_probe_retry/
│   └── test_hal_probe_retry.cpp    # I2C probe retry logic
├── test_mocks/
│   ├── Arduino.h                   # String class, millis(), GPIO, Serial stubs
│   ├── Preferences.h               # NVS key-value store (in-memory)
│   ├── PubSubClient.h              # MQTT client interface
│   ├── WiFi.h                      # WiFi connection state, scan results
│   ├── Wire.h                      # I2C bus mock
│   ├── LittleFS.h                  # Filesystem (in-memory)
│   ├── ETH.h                       # Ethernet interface mock
│   ├── IPAddress.h                 # IP address class
│   ├── esp_timer.h                 # ESP-IDF timer functions
│   ├── esp_random.h                # Deterministic random for tests
│   ├── i2s_std_mock.h              # I2S driver stub
│   └── mbedtls/
│       ├── md.h                    # Crypto hash functions
│       └── pkcs5.h                 # PBKDF2 functions
└── ... (91 test modules total)
```

**Location (JavaScript/E2E):**
```
e2e/
├── tests/                          # 22 spec files
│   ├── auth.spec.js                # Login flow, session, redirect (3 tests)
│   ├── auth-password.spec.js       # Password change modal (1 test)
│   ├── navigation.spec.js          # Tab switching, sidebar (2 tests)
│   ├── hal-devices.spec.js         # Device cards, rescan, disable (4 tests)
│   ├── hal-adc-controls.spec.js    # ADC volume/gain/filter controls (28 tests)
│   ├── ess-2ch-adc.spec.js         # ESS 2-channel ADC UI (23 tests)
│   ├── ess-4ch-tdm.spec.js         # ESS 4-channel TDM ADC UI (16 tests)
│   ├── hardware-stats.spec.js      # CPU, memory, task monitor (4 tests)
│   └── ...
├── helpers/
│   ├── fixtures.js                 # connectedPage fixture (auth + WS + state)
│   ├── ws-helpers.js               # buildInitialState(), handleCommand(), binary builders
│   └── selectors.js                # Reusable DOM selectors (160+ selectors)
├── mock-server/
│   ├── server.js                   # Express server (port 3000)
│   ├── assembler.js                # HTML assembly from web_src/
│   ├── ws-state.js                 # Deterministic mock state singleton
│   └── routes/                     # 12 Express route files matching firmware API
│       ├── auth.js
│       ├── hal.js
│       ├── wifi.js
│       ├── mqtt.js
│       ├── settings.js
│       ├── ota.js
│       ├── pipeline.js
│       ├── dsp.js
│       ├── sensing.js
│       ├── siggen.js
│       ├── diagnostics.js
│       └── system.js
├── fixtures/
│   ├── ws-messages/                # 15 WS broadcast message fixtures
│   │   ├── wifi-status.json
│   │   ├── hal-device-state.json
│   │   ├── audio-channel-map.json
│   │   ├── hardware-stats.json
│   │   └── ...
│   └── api-responses/              # 14 REST response fixtures
│       ├── hal-devices.json
│       ├── hal-presets.json
│       ├── settings.json
│       └── ...
└── playwright.config.js
```

**Naming (C++):**
- Test files: `test_<feature>.cpp`
- Test functions: `void test_<scenario>(void)` — e.g., `test_login_success()`, `test_skip_factor_2_clients()`
- Prefix pattern: `test_<subsystem>_<behavior>` — e.g., `test_session_creation_empty_slot()`

**Naming (JavaScript/E2E):**
- Spec files: `<feature>.spec.js` — e.g., `auth.spec.js`, `hal-devices.spec.js`
- Test names: descriptive sentence — e.g., `test('device cards render for all 8 HAL devices with name, type, and state', ...)`

## Test Structure

**Unity test pattern (C++ — standard structure):**
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

#include "../../src/config.h"

// Global test state
static bool mockValue = false;

void setUp(void) {
  // Reset state before EACH test
  mockValue = false;
  Preferences::reset();
}

void tearDown(void) {
  // Cleanup after each test (usually empty)
}

void test_feature_basic_case(void) {
  // Arrange
  mockValue = true;

  // Act
  bool result = testFunction();

  // Assert
  TEST_ASSERT_TRUE(result);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_feature_basic_case);
  return UNITY_END();
}
```

**Key Unity assertions (from test files):**
- `TEST_ASSERT_EQUAL_INT(expected, actual)`
- `TEST_ASSERT_EQUAL_STRING(expected, actual)`
- `TEST_ASSERT_EQUAL_HEX16(expected, actual)` — used for diagnostic codes
- `TEST_ASSERT_EQUAL_UINT8(expected, actual)`
- `TEST_ASSERT_TRUE(condition)` / `TEST_ASSERT_FALSE(condition)`
- `TEST_ASSERT_NULL(ptr)` / `TEST_ASSERT_NOT_NULL(ptr)`
- `TEST_ASSERT_LESS_THAN(threshold, actual)` / `TEST_ASSERT_GREATER_THAN(threshold, actual)`
- `TEST_ASSERT_LESS_OR_EQUAL(threshold, actual)`
- `TEST_ASSERT_MESSAGE(condition, "message")`

**Playwright test pattern (JavaScript — using connectedPage fixture):**
```javascript
const { test, expect } = require('../helpers/fixtures');
const path = require('path');
const fs = require('fs');

const HAL_FIXTURE = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'hal-device-state.json'), 'utf8')
);

test('device cards render for all 8 HAL devices', async ({ connectedPage: page }) => {
  // Navigate to tab
  await page.locator('.sidebar-item[data-tab="devices"]').click();

  // Wait for content
  const deviceList = page.locator('#hal-device-list');
  await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

  // Assert
  const cards = deviceList.locator('.hal-device-card');
  await expect(cards).toHaveCount(8, { timeout: 5000 });
  await expect(deviceList).toContainText('PCM5102A');
});
```

**Key Playwright assertions:**
- `await expect(element).toBeVisible()`
- `await expect(element).toBeChecked()` — for CSS-hidden toggle checkboxes styled with `label.switch`
- `await expect(element).toHaveValue(value)`
- `await expect(element).toHaveCount(n)`
- `await expect(element).toContainText(text)`
- `await expect(page).toHaveURL(url)`
- `expect(response.status()).toBe(200)`

## Mocking

**Mock Framework (C++):**
- Manual mocks in `test/test_mocks/` — no dynamic mocking framework (embedded constraint)
- Mock classes implement subsets of Arduino/ESP-IDF APIs with deterministic values
- Mocks have static `reset()` methods called in `setUp()` to clear state

**Mock patterns:**
```cpp
// From test/test_mocks/Arduino.h — String class extends std::string
class String : public std::string {
public:
  String() : std::string() {}
  String(const char *s) : std::string(s ? s : "") {}
  int toInt() const {
    try { return std::stoi(*this); }
    catch (...) { return 0; }
  }
};

// From test/test_mocks/Preferences.h — in-memory key-value store
class Preferences {
private:
  static std::map<String, String> _data;
public:
  static void reset() { _data.clear(); }
  void putString(const char *key, const String &value) { _data[String(key)] = value; }
  String getString(const char *key, const String &defaultValue) {
    auto it = _data.find(String(key));
    return it != _data.end() ? it->second : defaultValue;
  }
};
```

**Conditional include pattern (used in test files):**
```cpp
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#endif
```

**Inline implementation pattern (for complex module tests like `test_hal_discovery`):**
```cpp
// Include real source files directly for integration-level testing
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
#include "../../src/diag_journal.cpp"

// Provide simplified stubs for dependencies not needed
void hal_db_init() { /* stub */ }
bool hal_db_save() { return true; }
```

**What to Mock:**
- Hardware interfaces (GPIO, I2S, I2C Wire)
- System timing (millis, esp_timer)
- Persistent storage (Preferences, LittleFS)
- Random number generation (deterministic for crypto tests)
- Network interfaces (WiFi, MQTT PubSubClient, Ethernet)

**What NOT to Mock:**
- Core algorithms (DSP, crypto math, state machines) — test the real logic
- Data structures (arrays, structs) — use real instances
- Pure computation functions (RMS, VU, peak, biquad coefficients)
- Config constants from `src/config.h` — test actual values

## Fixtures and Factories

**Test Data (C++ — namespace-scoped reset):**
```cpp
// From test_mqtt_handler.cpp
namespace TestMQTTState {
    void reset() {
        mqttSettings.broker = "";
        mqttSettings.port = 1883;
        mqttSettings.username = "";
        mqttSettings.password = "";
        mqttSettings.baseTopic = "alx_nova";
        mqttSettings.enabled = false;
        PubSubClient::reset();
        Preferences::reset();
#ifdef NATIVE_TEST
        ArduinoMock::reset();
#endif
    }
}

void setUp(void) {
  TestMQTTState::reset();  // Called before every test
}
```

**Fixtures (JavaScript/E2E — JSON files):**
- WS broadcasts: `e2e/fixtures/ws-messages/*.json` (15 files)
  - `wifi-status.json`, `smart-sensing.json`, `display-state.json`, `buzzer-state.json`
  - `mqtt-settings.json`, `hal-device-state.json`, `audio-channel-map.json`
  - `audio-graph-state.json`, `signal-generator.json`, `debug-state.json`
  - `hardware-stats.json`, `audio-levels.json`, `auth-required.json`, `auth-success.json`, `debug-log.json`
- API responses: `e2e/fixtures/api-responses/*.json` (14 files)
  - `hal-devices.json`, `hal-presets.json`, `settings.json`, `wifi-status.json`, etc.

**Fixture loading pattern (from `e2e/helpers/ws-helpers.js`):**
```javascript
function loadFixture(name) {
  return JSON.parse(fs.readFileSync(path.join(FIXTURE_DIR, `${name}.json`), 'utf8'));
}

function buildInitialState() {
  return [
    'wifi-status', 'smart-sensing', 'display-state', 'buzzer-state',
    'mqtt-settings', 'hal-device-state', 'audio-channel-map',
    'audio-graph-state', 'signal-generator', 'debug-state',
  ].map(loadFixture);
}
```

**connectedPage fixture (from `e2e/helpers/fixtures.js`):**
- Acquires session cookie via `POST /api/auth/login`
- Sets cookie in browser context
- Intercepts WebSocket on port 81 via `page.routeWebSocket(/.*:81/, handler)`
- Completes WS auth handshake (fetch ws-token, send auth, receive authSuccess)
- Broadcasts all initial-state fixture messages
- Waits until `#wsConnectionStatus` reads "Connected"
- Exposes `connectedPage.wsRoute` for pushing additional WS messages in tests

## Coverage

**Requirements:**
- No numeric coverage target enforced
- CI requirement: all tests must pass (0 failures) before build/release
- ~2335 C++ tests across 91 modules
- ~98 E2E tests across 22 specs

**View Coverage (C++):**
```bash
pio test -e native -v    # Verbose output shows each test result
# No coverage HTML report generated (PlatformIO limitation on native)
```

**View Coverage (JavaScript/E2E):**
```bash
cd e2e && npx playwright test --reporter=html
# Opens playwright-report/index.html in browser
# CI uploads HTML report as artifact on failure (14-day retention)
```

## Test Types

**Unit Tests (C++ — majority of tests):**
- Scope: Single module or function, isolated behavior
- Examples: `test/test_auth/` (PBKDF2, sessions, rate limiting), `test/test_ws_adaptive_rate/` (skip factor calculation)
- Setup: Mock all dependencies, test real logic
- Duration: <1ms per test

**Config/Constraint Tests (C++ — validation of constants):**
- Scope: Verify config constants meet design constraints
- Examples: `test/test_hal_probe_retry/` (retry count bounded, worst-case delay <500ms, stack size)
- Pattern: Test actual `#define` values from `src/config.h` against safety bounds
```cpp
void test_retry_count_is_bounded(void) {
    TEST_ASSERT_LESS_OR_EQUAL(5, HAL_PROBE_RETRY_COUNT);
    TEST_ASSERT_GREATER_THAN(0, HAL_PROBE_RETRY_COUNT);
}

void test_worst_case_boot_delay(void) {
    uint32_t totalMs = /* compute from constants */;
    TEST_ASSERT_LESS_THAN(500, totalMs);  // Must be under 500ms
}
```

**Documentation Tests (C++ — verify design intent):**
- Scope: Record design decisions as executable assertions
- Example: `test/test_http_security/` (verify exactly 2 security headers, no CSP)
```cpp
void test_security_headers_count(void) {
    int headerCount = 2; // X-Frame-Options + X-Content-Type-Options
    TEST_ASSERT_EQUAL_INT(2, headerCount);
}
```

**Integration Tests (C++ — multi-module):**
- Scope: Multi-module interactions with real objects, mocked hardware
- Examples: `test/test_hal_discovery/` (includes real `hal_device_manager.cpp`, `hal_driver_registry.cpp`)
- Pattern: `#include "../../src/<module>.cpp"` to inline real implementations
- Duration: <10ms per test

**E2E Tests (JavaScript/Playwright):**
- Scope: Full web UI against mock Express server + WS interception
- No real hardware needed — mock server simulates firmware responses
- Examples: `e2e/tests/auth.spec.js` (login flow), `e2e/tests/hal-devices.spec.js` (device management)
- Duration: 100ms-2s per test

## Common Patterns

**Tab navigation in E2E tests:**
```javascript
// Use page.evaluate to call switchTab() — avoids scroll issues with sidebar clicks
await page.evaluate(() => switchTab('settings'));

// Or use sidebar click (works when sidebar is visible)
await page.locator('.sidebar-item[data-tab="devices"]').click();
```

**CSS-hidden checkbox toggles:**
```javascript
// Toggles styled with label.switch are CSS-hidden — use toBeChecked(), not toBeVisible()
await expect(page.locator('#debugModeToggle')).toBeChecked();
```

**Strict mode — multiple element matches:**
```javascript
// Use .first() when a selector might match multiple elements
const disableBtn = deviceList.locator('button').filter({ hasText: /Disable/i }).first();
```

**API interception in E2E tests:**
```javascript
test('rescan triggers POST /api/hal/scan', async ({ connectedPage: page }) => {
  let scanCalled = false;
  await page.route('/api/hal/scan', async (route) => {
    scanCalled = true;
    await route.fulfill({ status: 200, body: JSON.stringify({ status: 'ok', devicesFound: 6 }) });
  });

  await page.locator('#hal-rescan-btn').click();
  await page.waitForTimeout(500);
  expect(scanCalled).toBe(true);
});
```

**WS command handling in E2E tests (from `e2e/helpers/ws-helpers.js`):**
```javascript
function handleCommand(type, data) {
  switch (type) {
    case 'subscribeAudio':
      return [];  // Acknowledge, no state data
    case 'manualOverride':
      return [{ type: 'smartSensing', ampOn: !!data.on, ... }];
    default:
      return [];
  }
}
```

**Error Testing (C++):**
```cpp
void test_password_verify_rejects_wrong_input(void) {
  String hashedPwd = hashPassword("correct");
  TEST_ASSERT_FALSE(verifyPassword("wrong", hashedPwd));
  TEST_ASSERT_TRUE(verifyPassword("correct", hashedPwd));
}
```

**Replicated logic testing (for functions not easily extracted):**
```cpp
// From test_ws_adaptive_rate.cpp — replicate skip factor logic from websocket_handler.cpp
static uint8_t calcSkipFactor(uint8_t authCount) {
    if (authCount >= 8) return WS_BINARY_SKIP_8PLUS;
    if (authCount >= 5) return WS_BINARY_SKIP_5PLUS;
    if (authCount >= 3) return WS_BINARY_SKIP_3PLUS;
    if (authCount == 2) return WS_BINARY_SKIP_2_CLIENTS;
    return 1;
}
```

## Mandatory Test Coverage Rules

**Before completing any task:**

1. **C++ firmware changes** (`src/`):
   - Run: `pio test -e native -v` or use `firmware-test-runner` agent
   - New modules must have a test file in `test/test_<module>/`
   - Changed function signatures must update affected tests
   - All tests must pass before commit

2. **Web UI changes** (`web_src/`):
   - Run: `cd e2e && npx playwright test`
   - New toggle/button/dropdown: add test verifying correct WS command sent
   - New WS broadcast type: add fixture JSON + test verifying DOM update
   - New tab/section: add navigation + element presence tests
   - Changed element IDs: update `e2e/helpers/selectors.js` + affected specs
   - Removed features: delete corresponding tests + fixtures
   - New top-level JS declarations: add to `web_src/.eslintrc.json` globals

3. **WebSocket protocol changes** (`src/websocket_handler.cpp`):
   - Update `e2e/fixtures/ws-messages/` with new/changed message fixtures
   - Update `e2e/helpers/ws-helpers.js` `buildInitialState()` and `handleCommand()`
   - Update `e2e/mock-server/ws-state.js` if new state fields added
   - Add Playwright test verifying frontend handles new message type

4. **REST API changes** (`src/main.cpp`, `src/hal/hal_api.cpp`):
   - Update matching route in `e2e/mock-server/routes/*.js`
   - Update `e2e/fixtures/api-responses/` with new/changed response fixtures
   - Add Playwright test if UI depends on new endpoint

## Pre-commit Hooks

**Activated via:** `git config core.hooksPath .githooks`

**Pre-commit checks (`.githooks/pre-commit` — 5 checks):**
1. `node tools/find_dups.js` — Detect duplicate JS declarations across files
2. `node tools/check_missing_fns.js` — Find undefined function references
3. ESLint on `web_src/js/` with `web_src/.eslintrc.json` config
4. `node tools/check_mapping_coverage.js` — Doc mapping coverage
5. `node tools/diagram-validation.js` — Architecture diagram symbol validation

## CI/CD Quality Gates

All 4 gates must pass before firmware build/release proceeds (`.github/workflows/tests.yml`):

1. **cpp-tests** (`pio test -e native -v`) — ~2335 C++ tests across 91 modules
2. **cpp-lint** (cppcheck) — Static analysis on `src/` (excluding `src/gui/`)
3. **js-lint** — find_dups + check_missing_fns + ESLint + diagram validation
4. **e2e-tests** (Playwright) — ~98 browser tests across 22 specs

Execution: **parallel** (all 4 gates run simultaneously on separate ubuntu-latest runners).

**Playwright config:**
- Browser: Chromium only
- Timeout: 30s per test
- CI: 1 retry, 1 worker, HTML reporter
- Local: 0 retries, parallel workers, list reporter
- Web server: mock Express server auto-started via `node mock-server/server.js`
- On failure: Playwright HTML report uploaded as artifact (14-day retention)
- Trace: captured on first retry

## Adding New Test Modules

**New C++ test module:**
1. Create directory: `test/test_<module>/`
2. Create file: `test/test_<module>/test_<module>.cpp`
3. Include Unity: `#include <unity.h>`
4. Add mock includes with `#ifdef NATIVE_TEST` guards
5. Implement `setUp()`, `tearDown()`, test functions
6. Implement `main()` with `UNITY_BEGIN()` / `RUN_TEST()` / `UNITY_END()`

**New E2E test spec:**
1. Create file: `e2e/tests/<feature>.spec.js`
2. Import fixtures: `const { test, expect } = require('../helpers/fixtures');`
3. Use `connectedPage` fixture for tests needing authenticated WS connection
4. Add new selectors to `e2e/helpers/selectors.js`
5. Add WS fixtures to `e2e/fixtures/ws-messages/` if needed
6. Add API response fixtures to `e2e/fixtures/api-responses/` if needed
7. Update `e2e/helpers/ws-helpers.js` `handleCommand()` for new WS command types

---

*Testing analysis: 2026-03-22*
