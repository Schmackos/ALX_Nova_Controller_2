# Testing Patterns

**Analysis Date:** 2026-03-23

## Test Framework

**C++ Unit Tests (Firmware):**

**Runner:**
- PlatformIO with Unity framework (vendored in `framework` dependency)
- Native platform for host compilation (gcc/MinGW): `pio test -e native`
- Build without hardware needed — all hardware mocked

**Assertion Library:**
- Unity macros: `TEST_ASSERT_TRUE()`, `TEST_ASSERT_FALSE()`, `TEST_ASSERT_EQUAL_INT()`, `TEST_ASSERT_EQUAL_STRING()`, `TEST_ASSERT_NULL()`, `TEST_ASSERT_NOT_NULL()`

**Run Commands:**
```bash
pio test -e native              # Run all tests (~3050 tests across 110 modules)
pio test -e native -f test_wifi # Run single test module
pio test -e native -v           # Verbose test output
```

**Browser Tests (E2E):**

**Runner:**
- Playwright v1.x (installed via npm)
- Chromium browser (installed via `npx playwright install`)
- Mock Express server at http://localhost:3000 (no real device needed)

**Run Commands:**
```bash
cd e2e
npm install                              # First time only
npx playwright install --with-deps chromium  # First time only
npx playwright test                      # Run all 113 tests across 22 specs
npx playwright test tests/auth.spec.js   # Run single spec
npx playwright test --headed             # Run with visible browser
npx playwright test --debug              # Debug mode with inspector
```

## Test File Organization

**C++ Unit Tests:**

**Location:**
- Test directories mirror modules: `test/test_<module>/test_<module>.cpp`
- Each module gets its own directory to avoid duplicate `main()` symbol conflicts
- Example tree:
  ```
  test/
  ├── test_auth/
  │   └── test_auth_handler.cpp
  ├── test_wifi/
  │   └── test_wifi_manager.cpp
  ├── test_dsp/
  │   └── test_dsp.cpp
  └── test_mocks/
      ├── Arduino.h
      ├── Preferences.h
      ├── WiFi.h
      └── ...
  ```

**Naming:**
- Test file: `test_<feature>.cpp` (e.g., `test_auth_handler.cpp`, `test_smart_sensing_logic.cpp`)
- Test function: `void test_<specific_case>(void)` (e.g., `void test_pbkdf2_hash_p2_format(void)`)
- Test suite organization: Grouped by feature using descriptive test names

**Structure:**
```cpp
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#endif

// Module state
static int mockState = 0;

void setUp(void) {
  // Reset state before each test
  mockState = 0;
  ArduinoMock::reset();
}

void tearDown(void) {
  // Cleanup after each test (optional)
}

void test_function_returns_true_on_valid_input(void) {
  // Arrange
  const char *input = "test";

  // Act
  bool result = someFunction(input);

  // Assert
  TEST_ASSERT_TRUE(result);
}

void test_function_returns_false_on_null_input(void) {
  bool result = someFunction(NULL);
  TEST_ASSERT_FALSE(result);
}
```

**E2E Browser Tests:**

**Location:**
- `e2e/tests/*.spec.js` (currently 113 tests across 49+ spec files)
- No subdirectories — all specs at top level
- Example specs: `auth.spec.js`, `wifi.spec.js`, `audio-tab.spec.js`

**Naming:**
- File: `<feature>.spec.js` (e.g., `hal-devices.spec.js`, `settings-debug.spec.js`)
- Test: `test('<description>', async ({ page }) => { ... })` (e.g., `'login page renders with password field'`)

**Structure:**
```javascript
const { test, expect } = require('@playwright/test');

test('login page renders with password field and submit button', async ({ page }) => {
  await page.goto('/login');

  const pwdInput = page.locator('input[type="password"]');
  const submitBtn = page.locator('button[type="submit"]');

  await expect(pwdInput).toBeVisible();
  await expect(submitBtn).toBeVisible();
});

test('correct password submits and redirects to main page', async ({ page }) => {
  await page.goto('/login');

  await page.locator('input[type="password"]').fill('anypassword');
  await page.locator('button[type="submit"]').click();

  const resp = await page.request.post('/api/auth/login', {
    data: { password: 'anypassword' },
  });
  const body = await resp.json();
  expect(body.success).toBe(true);
});
```

## Test Structure

**C++ Arrange-Act-Assert Pattern:**

All unit tests follow AAA structure for clarity:

```cpp
void test_wifi_manager_connects_with_valid_ssid(void) {
  // ===== ARRANGE =====
  WiFiNetworkConfig config = {
    .ssid = "TestNetwork",
    .password = "testpass123",
    .useStaticIP = false
  };

  // ===== ACT =====
  bool result = connectToWiFi(config);

  // ===== ASSERT =====
  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_EQUAL_STRING("TestNetwork", currentWiFiSSID);
}
```

**Test Patterns:**
- `setUp()` → runs before every test, resets mocks and module state
- `tearDown()` → runs after every test (optional, rarely used)
- One assertion per test (or closely related group of assertions)
- Descriptive test names that read as documentation

**Common assertions:**
- `TEST_ASSERT_TRUE(condition)`
- `TEST_ASSERT_FALSE(condition)`
- `TEST_ASSERT_EQUAL_INT(expected, actual)`
- `TEST_ASSERT_EQUAL_STRING(expected, actual)`
- `TEST_ASSERT_NULL(ptr)`
- `TEST_ASSERT_NOT_NULL(ptr)`
- `TEST_ASSERT_GREATER_THAN(a, b)`
- `TEST_ASSERT_LESS_THAN(a, b)`

**E2E Test Patterns:**

Playwright tests use async/await with Locator API:

```javascript
test('button click sends websocket command', async ({ connectedPage }) => {
  // Navigate to tab
  await connectedPage.evaluate(() => switchTab('settings'));

  // Find and click element
  const toggleBtn = connectedPage.locator('#featureToggle');
  await toggleBtn.click();

  // Expect state change via WS
  await expect(connectedPage.locator('#featureStatus')).toContainText('Enabled');
});
```

## Mocking

**Framework (C++):** Manual mocks in `test/test_mocks/` — no external mocking library.

**Mock files location:** `test/test_mocks/`

**Mocks implemented:**
- `Arduino.h` — `millis()`, `micros()`, `delay()`, `Serial.print()`, GPIO read/write, ADC simulation
- `WiFi.h` — WiFi connection state, scan results, event simulation
- `Preferences.h` — NVS key-value storage (file-backed)
- `Preferences.h` — MQTT PubSubClient callbacks
- `WebSocketsServer.h` — WebSocket message simulation
- `esp_random.h` — `esp_random()` deterministic values
- `esp_timer.h` — microsecond timer (`esp_timer_get_time()`)
- `mbedtls/md.h` — MD5/SHA256 hashing
- `mbedtls/pkcs5.h` — PBKDF2 password hashing

**Mock pattern (example from `test/test_mocks/Arduino.h`):**
```cpp
class ArduinoMock {
public:
  static void reset() {
    time_us = 0;
    // ... reset all mock state
  }
  static uint32_t time_us;
};

uint32_t millis() {
  return ArduinoMock::time_us / 1000;
}
```

**Usage in tests:**
```cpp
void setUp(void) {
  ArduinoMock::reset();  // Reset mock state before each test
}

void test_timer_logic(void) {
  // Arrange
  ArduinoMock::time_us = 5000000;  // 5 seconds

  // Act
  uint32_t ms = millis();

  // Assert
  TEST_ASSERT_EQUAL_INT(5000, ms);
}
```

**Framework (E2E Playwright):**

**Custom fixture:** `e2e/helpers/fixtures.js`

```javascript
const test = base.extend({
  connectedPage: async ({ page, request, baseURL }, use) => {
    // 1. Acquire session cookie from mock server
    const sessionId = await acquireSessionCookie(request, baseURL || 'http://localhost:3000');

    // 2. Set cookie in browser
    await page.context().addCookies([{ name: 'sessionId', value: sessionId, domain: 'localhost', path: '/' }]);

    // 3. Route WebSocket at port 81
    await page.routeWebSocket(/.*:81\//, async (route) => {
      const ws = route.websocket();
      ws.onMessage(async (event) => {
        const msg = JSON.parse(event.data);
        const responses = handleCommand(msg.type, msg);
        responses.forEach(r => ws.send(JSON.stringify(r)));
      });
    });

    // 4. Use fixture in test
    await use(page);
  }
});
```

**Patterns in tests:**
- `page.locator(selector)` — find element by CSS selector
- `page.goto(url)` — navigate to URL
- `await expect(locator).toBeVisible()` — assert element is visible
- `await page.routeWebSocket()` — intercept WebSocket connections
- `page.routeWebSocket()` captures `onMessage()` (capital M), `onClose()` (capital C)

**What to mock:**
- Arduino functions (`millis()`, `delay()`, GPIO)
- WiFi state and events
- MQTT broker and callbacks
- Hardware sensors (temperature, ADC readings)
- NVS Preferences storage
- Cryptographic functions (deterministic test vectors)

**What NOT to mock:**
- ArduinoJson library (use real JSON parsing in tests)
- Core C++ standard library functions
- Module behavior — test actual logic, not mocks

## Fixtures and Factories

**Test data (C++):**

Test data lives in test file scope, not separate fixture files:

```cpp
// In test_auth_handler.cpp
static const char *TEST_PASSWORDS[] = {
  "password123",
  "anotherpass",
  "verylongpasswordtotestbufferfilling"
};

static const int TEST_PASSWORD_COUNT = 3;

void test_password_hashing(void) {
  for (int i = 0; i < TEST_PASSWORD_COUNT; i++) {
    String hash = hashPassword(TEST_PASSWORDS[i]);
    TEST_ASSERT_NOT_NULL(hash.c_str());
  }
}
```

**WebSocket fixtures (E2E):**

Fixture JSON files in `e2e/fixtures/ws-messages/`:
- `wifi-status.json` — WiFi connection state broadcast
- `smart-sensing.json` — amplifier/signal detection state
- `audio-channel-map.json` — input/output lane count
- `hal-device-state.json` — all HAL devices and their states
- `debug-state.json` — debug mode, log level, hardware stats

**Fixture loading (E2E):**

```javascript
// e2e/helpers/ws-helpers.js
function loadFixture(name) {
  return JSON.parse(fs.readFileSync(path.join(FIXTURE_DIR, `${name}.json`), 'utf8'));
}

function buildInitialState() {
  return [
    'wifi-status',
    'smart-sensing',
    'display-state',
    'buzzer-state',
    'mqtt-settings',
    'hal-device-state',
    'audio-channel-map',
    'audio-graph-state',
    'signal-generator',
    'debug-state',
  ].map(loadFixture);
}
```

**REST API fixtures (E2E):**

Mock server routes in `e2e/mock-server/routes/`:
- `hal.js` — `/api/hal/devices` endpoints
- `audio.js` — `/api/audio/*` endpoints
- `settings.js` — `/api/settings` endpoints
- Response fixtures return deterministic data matching firmware schema

**Location:**
- C++ test data: inline in test file
- E2E WS fixtures: `e2e/fixtures/ws-messages/*.json`
- E2E REST fixtures: `e2e/fixtures/api-responses/*.json`

## Coverage

**Requirements:** No hard target enforced in CI/CD; coverage tracking informational only.

**View coverage:**
```bash
# C++ coverage (not automated — requires extra tooling)
# E2E coverage: 113 tests across 49 specs = ~85% feature coverage (subjective estimate)

# All 4 CI/CD quality gates must pass:
# 1. cpp-tests: pio test -e native -v
# 2. cpp-lint: cppcheck on src/
# 3. js-lint: find_dups.js + check_missing_fns.js + ESLint
# 4. e2e-tests: npx playwright test
```

**Coverage gaps:**
- UI visual regression testing (Playwright screenshots not currently used)
- Hardware-specific paths (I2C bus conflicts, SDIO timing) — limited by emulation
- Real-time audio performance under load (no synthetic load generators)
- Fuzz testing for protocol parsing

## Test Types

**Unit Tests (C++):**

**Scope:** Single function or tightly-coupled module pair.

**Approach:**
- Compile with `-D UNIT_TEST -D NATIVE_TEST` flags (automatic via `pio test`)
- Mock all external dependencies (hardware, WiFi, MQTT)
- Test edge cases: null inputs, boundary values, error conditions
- Fast execution: ~2-3 seconds total for all 3050 tests

**Examples:**
- `test_auth_handler.cpp` — PBKDF2 hashing, password validation, session management
- `test_smart_sensing_logic.cpp` — signal detection thresholds, timer logic, state machine
- `test_dsp.cpp` — biquad coefficient computation, filter stability

**Integration Tests (C++):**

**Scope:** Multiple modules interacting together.

**Approach:**
- Still run on native platform with mocks
- Test module boundaries and data flow
- Example: `test_hal_integration.cpp` — device discovery → HAL manager → pipeline bridge → audio output

**Examples:**
- `test_hal_integration.cpp` — device lifecycle across discovery, config, init, removal
- `test_audio_pipeline.cpp` — input sources → DSP → matrix routing → output sinks
- `test_hal_probe_retry.cpp` — I2C discovery with timeout/retry logic

**E2E Tests (Playwright):**

**Scope:** Full user workflow from browser perspective.

**Approach:**
- Mock Express server replicates firmware REST API + WebSocket protocol
- Browser tests real UI interactions: clicks, form input, navigation
- WS interception: mock server returns deterministic state changes
- Example workflow: Login → Configure WiFi → Save Settings → Verify broadcast

**Examples:**
- `auth.spec.js` — login page, password submission, session management
- `wifi.spec.js` — WiFi network selection, static IP config, save/remove networks
- `audio-tab.spec.js` — input/output routing, DSP controls, waveform visualization
- `hal-devices.spec.js` — device list, toggle enable/disable, config edit, reinit

**Test categories in Playwright:**
- **Functional**: Does feature work as intended? (e.g., `auth.spec.js`)
- **Responsive**: Does layout adapt to screen size? (e.g., `responsive.spec.js`)
- **Visual**: Do DOM updates reflect state? (e.g., `visual-*.spec.js`)
- **Accessibility**: Are modals/tabs navigable? (e.g., `a11y-*.spec.js`)
- **Error handling**: Does app recover from errors? (e.g., `error-*.spec.js`)

## Common Patterns

**Async Testing (C++):**

Firmware has no async/await syntax. Simulate timing with mock time:

```cpp
void test_reconnect_backoff_exponential(void) {
  // Arrange: start with min reconnect delay
  ArduinoMock::time_us = 0;
  int delay1 = getReconnectDelay();

  // Act: advance time, trigger fail
  ArduinoMock::time_us += delay1 * 1000;
  // ... simulate network failure ...

  int delay2 = getReconnectDelay();

  // Assert: delay increased
  TEST_ASSERT_GREATER_THAN(delay1, delay2);
}
```

**Async Testing (E2E Playwright):**

```javascript
test('WebSocket reconnects after timeout', async ({ connectedPage }) => {
  // Arrange: initial connection established
  await expect(connectedPage.locator('#wsConnectionStatus')).toContainText('Connected');

  // Act: simulate server disconnect and reconnect
  await connectedPage.wsRoute.close();
  await connectedPage.waitForTimeout(5000);  // Wait for auto-reconnect

  // Assert: status shows connected again
  await expect(connectedPage.locator('#wsConnectionStatus')).toContainText('Connected');
});
```

**Error Testing (C++):**

```cpp
void test_function_handles_invalid_json(void) {
  // Arrange
  const char *invalidJson = "{invalid";

  // Act
  JsonDocument doc;
  bool result = parseJson(invalidJson, doc);

  // Assert: error flag set
  TEST_ASSERT_FALSE(result);
}

void test_function_logs_error_on_null_pointer(void) {
  // Arrange
  // (mock Serial to capture log output)

  // Act
  processData(NULL);

  // Assert: error logged
  TEST_ASSERT_EQUAL_STRING("[ERROR]", capturedLog);
}
```

**Error Testing (E2E Playwright):**

```javascript
test('error toast appears on failed login', async ({ page }) => {
  await page.goto('/login');

  // Route to make login fail
  await page.route('/api/auth/login', route => route.abort());

  await page.locator('input[type="password"]').fill('wrongpass');
  await page.locator('button[type="submit"]').click();

  // Toast appears with error
  await expect(page.locator('.toast.error')).toBeVisible();
});
```

## Mandatory Test Coverage Rules

**Every code change MUST keep tests green.** Before completing any task:

**C++ firmware changes** (`src/`):
1. Run `pio test -e native -v` to verify all 3050 tests pass
2. New modules: create `test/test_<module>/test_<module>.cpp` with at least 3 tests
3. Changed functions: update related test cases with new parameter handling
4. Core modules (wifi, auth, dsp): add negative test cases (null inputs, bounds)

**Web UI changes** (`web_src/`):
1. Run `cd e2e && npx playwright test` to verify all 113 E2E tests pass
2. New toggle/button: add test in `e2e/tests/` verifying it sends correct WS command
3. New WS broadcast type: add fixture JSON file + test verifying DOM updates
4. New tab or section: add navigation test + element presence checks
5. Changed element IDs: update `e2e/helpers/selectors.js` + affected specs
6. Removed features: remove corresponding tests + fixtures

**WebSocket protocol changes** (`src/websocket_*.cpp`):
1. Update `e2e/fixtures/ws-messages/` with new/changed message fixtures
2. Update `e2e/helpers/ws-helpers.js` `buildInitialState()` and `handleCommand()`
3. Update `e2e/mock-server/ws-state.js` if new state fields added
4. Add Playwright test verifying frontend handles new message type

**REST API changes** (`src/*_api.cpp`):
1. Update matching route in `e2e/mock-server/routes/*.js`
2. Update `e2e/fixtures/api-responses/` with new/changed response fixtures
3. Add Playwright test if UI depends on new endpoint
4. Document endpoint in `docs-site/docs/developer/api/`

**Pre-commit hooks:**
`.githooks/pre-commit` runs fast checks before every commit:
1. `node tools/find_dups.js` — duplicate JS declarations
2. `node tools/check_missing_fns.js` — undefined function references
3. ESLint on `web_src/js/`

Activate: `git config core.hooksPath .githooks`

---

*Testing analysis: 2026-03-23*
