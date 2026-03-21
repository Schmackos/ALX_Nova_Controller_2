# Testing Patterns

**Analysis Date:** 2026-03-21

## Test Framework

**C++ Unit Tests:**
- Runner: Unity (embedded test framework)
- Platform: `native` environment (gcc/MinGW on host machine, not ESP32 hardware)
- Compile flags: `-D UNIT_TEST -D NATIVE_TEST -D DSP_ENABLED` (and other domain flags)
- Config file: `platformio.ini` `[env:native]` section with Unity test_framework

**JavaScript E2E Tests:**
- Runner: Playwright (browser automation)
- Browser: Chromium (installed via `npx playwright install --with-deps chromium`)
- Mock server: Express.js on port 3000 (assembles HTML from `web_src/`, routes REST/WS)
- Total coverage: 26 tests across 19 spec files (e.g., auth.spec.js, audio-inputs.spec.js)

**Run Commands:**

```bash
# C++ Unit Tests
pio test -e native -v              # Run all tests with verbose output
pio test -e native -f test_wifi    # Run specific test module
pio test -e native -f test_mqtt    # Run MQTT tests
pio test -e native -f test_auth    # Run auth tests

# JavaScript E2E Tests
cd e2e
npm install                        # First time only
npx playwright install --with-deps chromium  # First time only
npx playwright test                # Run all 26 tests
npx playwright test tests/auth.spec.js      # Run single spec
npx playwright test --headed       # Show browser window
npx playwright test --debug        # Debug mode with inspector
npx playwright test --ui           # UI mode (interactive)
```

## Test File Organization

**C++ Location:**
- Pattern: `test/test_module_name/test_*.cpp` (one test per directory)
- Directory-per-test rule prevents duplicate `main()`, `setUp()`, `tearDown()` symbols
- Example: `test/test_auth/test_auth_handler.cpp`, `test/test_mqtt/test_mqtt_handler.cpp`

**C++ File Count:**
- 70 test modules covering 1620+ Unity test cases
- Examples: test_utils, test_auth, test_wifi, test_mqtt, test_settings, test_ota, test_button, test_websocket, test_api, test_smart_sensing, test_buzzer, test_gui_home, test_gui_input, test_gui_navigation, test_pinout, test_i2s_audio, test_fft, test_signal_generator, test_audio_diagnostics, test_audio_health_bridge, test_audio_pipeline, test_vrms, test_dim_timeout, test_debug_mode, test_dsp, test_dsp_rew, test_dsp_presets, test_dsp_swap, test_crash_log, test_task_monitor, test_esp_dsp, test_usb_audio, test_hal_core, test_hal_bridge, test_hal_coord, test_hal_dsp_bridge, test_hal_discovery, test_hal_integration, test_hal_eeprom_v3, test_hal_pcm5102a, test_hal_pcm1808, test_hal_es8311, test_hal_mcp4725, test_hal_siggen, test_hal_usb_audio, test_hal_custom_device, test_hal_multi_instance, test_hal_state_callback, test_hal_retry, test_hal_wire_mock, test_hal_buzzer, test_hal_button, test_hal_encoder, test_hal_ns4150b, test_output_dsp, test_dac_hal, test_dac_eeprom, test_dac_settings, test_diag_journal, test_peq, test_evt_any, test_sink_slot_api, test_sink_write_utils, test_deferred_toggle, test_pipeline_bounds, test_pipeline_output, test_eth_manager, test_es8311, test_i2s_config_cache

**JavaScript Location:**
- Pattern: `e2e/tests/feature-name.spec.js` (kebab-case)
- Mock server: `e2e/mock-server/` (server.js, routes/*.js, ws-state.js, assembler.js)
- Helpers: `e2e/helpers/` (fixtures.js, ws-helpers.js, selectors.js)
- Fixtures: `e2e/fixtures/` (ws-messages/*.json, api-responses/*.json)

**JavaScript File Count:**
- 19 test specs (auth, auth-password, audio-inputs, audio-matrix, audio-outputs, audio-siggen, control-tab, dark-mode, debug-console, hal-devices, hardware-stats, mqtt, navigation, ota, peq-overlay, responsive, settings, support, wifi)

## Test Structure

**C++ Test Pattern:**

```cpp
#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
// ... other mock includes
#else
#include <Arduino.h>
#include <Preferences.h>
// ... real includes
#endif

// Global test state (mutable across tests, reset in setUp)
static TestState testState;

void setUp(void) {
    // Reset all mutable state, clear mocks
    testState.reset();
    ArduinoMock::reset();
    Preferences::reset();
}

void tearDown(void) {
    // Cleanup if needed (often empty)
}

// Test function using Arrange-Act-Assert pattern
void test_module_feature_behavior(void) {
    // Arrange: Set up initial state
    testState.enabled = true;

    // Act: Call the function under test
    bool result = moduleFunction();

    // Assert: Verify expected outcome
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT(42, testState.value);
}

void test_module_error_handling(void) {
    // Arrange
    testState.enabled = false;

    // Act
    bool result = moduleFunction();

    // Assert
    TEST_ASSERT_FALSE(result);
}
```

Key macros:
- `TEST_ASSERT_TRUE(condition)` — assert condition is true
- `TEST_ASSERT_FALSE(condition)` — assert condition is false
- `TEST_ASSERT_EQUAL_INT(expected, actual)` — integer equality
- `TEST_ASSERT_EQUAL_STRING(expected, actual)` — string equality
- `TEST_ASSERT_FLOAT_WITHIN(tolerance, expected, actual)` — float with epsilon tolerance
- `TEST_IGNORE()` — skip test temporarily (leave marker in output)

**JavaScript Test Pattern (Playwright):**

```javascript
const { test, expect } = require('../helpers/fixtures');  // or @playwright/test

test('feature renders and accepts input', async ({ connectedPage }) => {
  // Arrange: Navigate and find elements
  const inputField = connectedPage.locator('#inputId');
  const submitBtn = connectedPage.locator('button[data-action="submit"]');

  // Act: Interact with page
  await inputField.fill('test value');
  await submitBtn.click();

  // Assert: Verify expected outcome
  await expect(connectedPage.locator('#result')).toContainText('Success');
});

test('WebSocket command sends correct payload', async ({ connectedPage }) => {
  // Arrange: Get WS route handle from fixture
  const wsRoute = connectedPage.wsRoute;

  // Act: Trigger action that sends WS command
  await connectedPage.locator('#toggleButton').click();

  // Assert: Verify WS message was routed
  // (Intercepted by mock server, recorded in ws-state.js)
  const response = await connectedPage.request.get('/api/some-state');
  const data = await response.json();
  expect(data.stateField).toBe(true);
});
```

Key Playwright methods:
- `page.locator(selector)` — find element by CSS/role/text
- `page.goto(url)` — navigate to page
- `page.request.get/post/put/delete(url)` — make REST request
- `page.routeWebSocket(pattern, handler)` — intercept WS connections
- `expect(element).toBeVisible()` — assertion on element visibility
- `expect(element).toContainText(text)` — assertion on DOM text
- `await element.click()`, `await element.fill(text)` — user interactions

## Mocking

**C++ Mocks Location:**
- Path: `test/test_mocks/` (11 mock headers)
- Files: Arduino.h, Preferences.h, WiFi.h, PubSubClient.h, Wire.h, LittleFS.h, esp_random.h, esp_timer.h, ETH.h, IPAddress.h, i2s_std_mock.h

**Arduino.h Mock Pattern:**
```cpp
// test/test_mocks/Arduino.h
#pragma once

namespace ArduinoMock {
  extern unsigned long _currentMillis;
  extern uint32_t _randomValue;

  void reset();
  void advance_millis(unsigned long ms);
}

unsigned long millis() {
  return ArduinoMock::_currentMillis;
}

uint32_t random(uint32_t max) {
  return ArduinoMock::_randomValue % max;
}
```

**Preferences.h Mock Pattern:**
```cpp
// test/test_mocks/Preferences.h — NVS key-value store simulation
class Preferences {
public:
  bool begin(const char *ns, bool readonly = false);
  void end();

  String getString(const char *key, const String &defaultValue = "");
  int32_t getInt(const char *key, int32_t defaultValue = 0);

  void putString(const char *key, const String &value);
  void putInt(const char *key, int32_t value);

  static void reset();  // Clear all stored values (called in setUp)
};
```

**What to Mock:**
- Hardware (GPIO, I2C, SPI, UART) → Mock return values, verify call order
- Timers (millis, micros) → Mock to advance time in tests, verify timeout logic
- Network (WiFi, Preferences) → Stub responses, verify state transitions
- Random (esp_random) → Control entropy, test both paths
- Peripherals (MQTT, HTTP) → Stub responses, verify request format

**What NOT to Mock:**
- Pure computation (DSP biquad math, FFT) → Test real implementation
- State management (AppState fields) → Use real AppState, test composition
- Data structures (JsonDocument) → Use real ArduinoJson, verify JSON output

## Fixtures and Factories

**C++ Test Data Pattern:**

```cpp
// Global state reset in setUp()
static MQTTSettings mqttSettings = {
    .broker = "",
    .port = 1883,
    .username = "",
    .password = "",
    .baseTopic = "alx_nova",
    .enabled = false
};

void setUp(void) {
    // Reset to defaults before each test
    mqttSettings.broker = "";
    mqttSettings.port = 1883;
    mqttSettings.username = "";
    mqttSettings.password = "";
    mqttSettings.baseTopic = "alx_nova";
    mqttSettings.enabled = false;

    Preferences::reset();  // Clear NVS mock storage
}

// Factory function for test data
static void create_mqtt_config(const char *broker, uint16_t port) {
    mqttSettings.broker = broker;
    mqttSettings.port = port;
    mqttSettings.enabled = true;
}
```

**JavaScript Fixtures Location:**
- Path: `e2e/fixtures/` (ws-messages/, api-responses/)
- WS fixtures: JSON files matching `buildInitialState()` order in ws-helpers.js
  - `audio-channel-map.json` — list of available input/output devices from HAL
  - `wifi-status.json` — WiFi connection state, SSID, RSSI
  - `smart-sensing.json` — Amplifier relay state, audio threshold, timer
- API fixtures: JSON response templates for `e2e/mock-server/routes/*.js`
  - `hal-devices.json` — HAL device list with states
  - `auth-status.json` — Session authentication status

**JavaScript Custom Fixture (connectedPage):**
- Location: `e2e/helpers/fixtures.js`
- Provides: `connectedPage` with session cookie + WS auth + initial state broadcasts
- Session flow:
  1. `POST /api/auth/login` with password
  2. Receive session cookie (sessionId=...)
  3. Navigate to main page
  4. Intercept `new WebSocket(:81)` with mock handler
  5. Send `{type:'auth', token:'<from /api/ws-token>'}` automatically
  6. Receive `{type:'authSuccess'}` from mock
  7. Receive all initial-state broadcasts (10 messages)
  8. Test code runs with live mock connection
- Usage: `async ({ connectedPage }) => { ... }` in test body

## Coverage

**C++ Requirements:**
- Target: No enforced minimum (test-driven development)
- Current: 1620+ tests across 70 modules (comprehensive)
- Strategy: Critical modules (auth, HAL, pipeline, DSP) have dedicated test files
- Pre-commit gate: All tests must pass before commit

**JavaScript Requirements:**
- Target: No enforced minimum
- Current: 26 tests across 19 specs covering UI workflows
- Strategy: Integration-focused (test full feature, not individual functions)
- Pre-commit gate: All tests must pass before commit

**View Coverage:**

```bash
# C++ test summary (shown after pio test)
pio test -e native -v 2>&1 | grep -E "^(test_|PASSED|FAILED|Test Summary)"

# JavaScript test summary
cd e2e && npx playwright test --reporter=html
# Open: playwright-report/index.html in browser
```

## Test Types

**C++ Unit Tests:**
- Scope: Individual function or small component
- Setup: Mock hardware, create test state
- Execution: Call function under test with known inputs
- Verification: Assert return value, state changes, or side effects
- Example: `test_lpf_coefficients()` in test_dsp.cpp verifies RBJ EQ Cookbook computation
- Speed: ~1-5ms per test, 1620 tests run in <10 seconds

**C++ Integration Tests:**
- Scope: Multiple modules interacting (e.g., HAL discovery + device registration)
- Setup: Initialize dependent modules, configure mocks
- Execution: Simulate real-world workflow
- Verification: Assert final state across module boundaries
- Example: `test_hal_discovery.cpp` verifies I2C scan → EEPROM detection → device registration
- Note: Kept in single test files, not separate directory

**JavaScript Integration Tests:**
- Scope: Full user workflow end-to-end
- Setup: Mock server running, browser page connected with WS
- Execution: User clicks buttons, types input, navigates tabs
- Verification: Assert DOM updates, WS commands sent, API responses received
- Example: `test('PEQ overlay opens and applies boost')` in peq-overlay.spec.js
- Speed: ~500-1000ms per test (browser overhead)

**No E2E Hardware Tests:**
- Firmware tests run on native (host), not on ESP32-P4 hardware
- Browser tests use mock server, not real device
- Device integration verified manually or via CI on real hardware (future)

## Common Patterns

**Async Testing (JavaScript):**

```javascript
test('API call succeeds with timeout', async ({ connectedPage }) => {
  const response = await connectedPage.request.get('/api/hal/devices');
  expect(response.status()).toBe(200);

  const devices = await response.json();
  expect(devices).toBeInstanceOf(Array);
});

test('WS message triggers DOM update', async ({ connectedPage }) => {
  // Wait for specific element to appear
  const meter = connectedPage.locator('#inputVuMeter');

  // Trigger WS message from mock
  connectedPage.wsRoute.send(JSON.stringify({
    type: 'audioChannelMap',
    lanes: [/* ... */]
  }));

  // Wait for DOM to update
  await expect(meter).toBeVisible({ timeout: 5000 });
});
```

**Error Testing (C++):**

```cpp
void test_authentication_rate_limit_blocks_login(void) {
    // Arrange: Simulate multiple failed logins
    for (int i = 0; i < 5; i++) {
        authHandler_handleLoginAttempt("wrongpassword");
    }

    // Act: Try login during cooldown
    bool result = authHandler_handleLoginAttempt("correctpassword");

    // Assert: Blocked by rate limit
    TEST_ASSERT_FALSE(result);  // Even correct password rejected
}

void test_recovery_after_rate_limit_timeout(void) {
    // Arrange: Set up rate limit state
    test_authentication_rate_limit_blocks_login();

    // Advance time past cooldown
    ArduinoMock::advance_millis(LOGIN_COOLDOWN_US / 1000 + 100);

    // Act: Try login again
    bool result = authHandler_handleLoginAttempt("correctpassword");

    // Assert: Rate limit cleared
    TEST_ASSERT_TRUE(result);
}
```

**Error Testing (JavaScript):**

```javascript
test('invalid session redirects to login', async ({ page }) => {
  // Set invalid session cookie
  await page.context().addCookies([{
    name: 'sessionId',
    value: 'invalid-session-id',
    domain: 'localhost',
    path: '/',
  }]);

  // Navigate to protected page
  await page.goto('/');

  // Verify redirected to login
  await expect(page).toHaveURL('/login');
});

test('network error shows connection warning', async ({ connectedPage }) => {
  // Simulate network failure (mock server stops responding)
  await connectedPage.context().setOffline(true);

  // Verify connection status shows error
  const status = connectedPage.locator('#wsConnectionStatus');
  await expect(status).toContainText('Disconnected');

  // Restore connectivity
  await connectedPage.context().setOffline(false);

  // Verify reconnection
  await expect(status).toContainText('Connected');
});
```

## Quality Gates (CI/CD)

**4 Parallel Gates** (must all pass before firmware build):

1. **cpp-tests** — `pio test -e native -v`
   - Runs 1620 Unity tests
   - Timeout: ~30 seconds
   - Failure: Any test assertion fails

2. **cpp-lint** — cppcheck on `src/`
   - Static analysis (excluding `src/gui/`)
   - Checks: memory leaks, uninitialized variables, logic errors
   - Failure: Linting errors found

3. **js-lint** — `tools/find_dups.js` + `tools/check_missing_fns.js` + ESLint
   - Duplicate JS declarations
   - Undefined function references
   - ESLint rules (no-undef, no-redeclare, eqeqeq)
   - Failure: Any linting error

4. **e2e-tests** — `npx playwright test`
   - Runs 26 browser tests
   - Timeout: ~3-5 minutes
   - Failure: Any test assertion fails
   - HTML report uploaded to artifact (14-day retention)

**File:** `.github/workflows/tests.yml` (trigger: push/PR to main/develop)
**Pipeline:** Diagram in `docs-internal/architecture/ci-quality-gates.mmd`

## Pre-commit Hooks

**Location:** `.githooks/pre-commit` (activate: `git config core.hooksPath .githooks`)

**Checks (fast, before each commit):**

```bash
# 1. Check for duplicate JS declarations
node tools/find_dups.js

# 2. Check for undefined function references
node tools/check_missing_fns.js

# 3. Lint web_src/js/
cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json
```

**On failure:** Commit is prevented; fix linting errors and stage files again.

## Test Maintenance Rules

**Mandatory:** Every code change MUST keep tests green.

**C++ Changes:**
- New modules need test file in `test/test_module_name/`
- Changed function signatures → update affected tests
- New features → add test cases to verify behavior
- Removed features → remove corresponding tests

**Web UI Changes:**
- New toggle/button/dropdown → add test verifying correct WS command
- New WS broadcast type → add fixture JSON + test verifying DOM updates
- New tab or section → add navigation + element presence tests
- Changed element IDs → update `e2e/helpers/selectors.js` + affected specs
- Removed features → remove corresponding tests + fixtures
- New top-level JS declarations → add to `web_src/.eslintrc.json` globals

**WebSocket Protocol Changes:**
- Update `e2e/fixtures/ws-messages/` with new/changed message fixtures
- Update `e2e/helpers/ws-helpers.js` `buildInitialState()` and `handleCommand()`
- Update `e2e/mock-server/ws-state.js` if new state fields are added
- Add Playwright test verifying frontend handles new message type

**REST API Changes:**
- Update matching route in `e2e/mock-server/routes/*.js`
- Update `e2e/fixtures/api-responses/` with new/changed response fixtures
- Add Playwright test if UI depends on new endpoint

---

*Testing analysis: 2026-03-21*
