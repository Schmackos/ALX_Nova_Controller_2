# Testing Patterns

**Analysis Date:** 2026-03-21

## Test Framework

**C++ Unit Tests (Unity + native platform):**
- Framework: Unity (built into PlatformIO)
- Platform: `native` environment compiles with GCC/MinGW on host machine
- Total: 1866 tests across 78 modules (as of 2026-03-21)
- Command: `pio test -e native` or `pio test -e native -v` (verbose)

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
test_build_src = no          ; Don't compile src/ directly — tests include headers + use mocks
lib_ignore = WebSockets      ; WebSockets not available on native
```

**Browser/E2E Tests (Playwright):**
- Framework: Playwright Test (browser automation)
- Total: 44 tests across 20 spec files
- Commands:
  ```bash
  cd e2e
  npm install                                    # First time only
  npx playwright install --with-deps chromium   # First time only
  npx playwright test                           # Run all tests
  npx playwright test tests/auth.spec.js        # Run single spec
  npx playwright test --headed                  # Run with visible browser
  npx playwright test --debug                   # Debug mode with inspector
  ```

**Test Infrastructure:**
- Mock server: `e2e/mock-server/server.js` (Express on port 3000)
- WebSocket interception: `page.routeWebSocket(/.*:81/, handler)`
- Mock state: `e2e/mock-server/ws-state.js` (singleton, reset between tests)
- Fixtures: `e2e/fixtures/ws-messages/*.json` (15 deterministic WS broadcast messages)
- API responses: `e2e/fixtures/api-responses/*.json` (14 REST response fixtures)

## Test File Organization

**Location (C++):**
- Co-located with source: `test/test_<module>/test_<module>.cpp`
- Each test module in its own directory to avoid duplicate `main`/`setUp`/`tearDown` symbols
- Mock implementations in `test/test_mocks/` (shared across all C++ tests)

**Naming (C++):**
- Test files: `test_<feature>.cpp` (e.g., `test_auth_handler.cpp`, `test_audio_pipeline.cpp`)
- Test functions: `void test_<scenario>(void)` (e.g., `void test_login_success(void)`)
- Prefix naming pattern: `test_<subsystem>_<behavior>` (e.g., `test_session_creation_empty_slot()`)

**Structure (C++):**
```
test/
├── test_auth/
│   └── test_auth_handler.cpp      # ~500 lines, 30+ test cases
├── test_audio_pipeline/
│   └── test_audio_pipeline.cpp
├── test_mocks/
│   ├── Arduino.h
│   ├── Preferences.h
│   ├── WiFi.h
│   ├── mbedtls/
│   │   ├── md.h
│   │   └── pkcs5.h
│   └── ... (15 total mock files)
└── ... (78 test modules total)
```

**Location (JavaScript/E2E):**
- `e2e/tests/*.spec.js` (one or more spec files per feature)
- Helpers: `e2e/helpers/` (fixtures.js, selectors.js, ws-helpers.js)
- Mock server: `e2e/mock-server/` (Express routes, WebSocket state)
- Fixtures: `e2e/fixtures/` (ws-messages, api-responses JSON files)

**Naming (JavaScript/E2E):**
- Spec files: `<feature>.spec.js` (e.g., `auth.spec.js`, `audio-matrix.spec.js`)
- Test names: `test('<scenario>', async ({ page }) => { ... })`
- Example: `test('login page renders with password field and submit button', ...)`

## Test Structure

**Unity test pattern (C++):**
```cpp
#include <unity.h>
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"

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
  TEST_ASSERT_EQUAL_INT(true, result);
}

void test_feature_edge_case(void) {
  mockValue = false;
  bool result = testFunction();
  TEST_ASSERT_EQUAL_INT(false, result);
}
```

**Key Unity assertions (from test files):**
- `TEST_ASSERT_EQUAL_INT(expected, actual)`
- `TEST_ASSERT_EQUAL_STRING(expected, actual)`
- `TEST_ASSERT_TRUE(condition)`
- `TEST_ASSERT_FALSE(condition)`
- `TEST_ASSERT_NULL(ptr)`
- `TEST_ASSERT_NOT_NULL(ptr)`
- `TEST_ASSERT_MESSAGE(condition, "message")`

**Playwright test pattern (JavaScript):**
```javascript
const { test, expect } = require('@playwright/test');

test('login page renders with password field and submit button', async ({ page }) => {
  // Arrange
  await page.goto('/login');

  // Act
  const pwdInput = page.locator('input[type="password"]');
  const submitBtn = page.locator('button[type="submit"]');

  // Assert
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

**Key Playwright assertions:**
- `await expect(element).toBeVisible()`
- `await expect(element).toBeChecked()`
- `await expect(element).toHaveValue(value)`
- `await expect(page).toHaveURL(url)`
- `expect(response.status()).toBe(200)`

## Mocking

**Mock Framework (C++):**
- Manual mocks in `test/test_mocks/` — no dynamic mocking framework (embedded constraint)
- Mock classes implement Arduino APIs (String, millis, Serial, GPIO, etc.)
- Mock implementations: subsets of real APIs, return deterministic values

**Mock Files:**
- `Arduino.h` — String class (extends std::string), millis(), delay(), random()
- `Preferences.h` — key-value storage (in-memory, reset-able)
- `WiFi.h` — WiFi connection state, scan results
- `PubSubClient.h` — MQTT client interface
- `esp_timer.h`, `esp_random.h` — ESP-IDF functions
- `mbedtls/md.h`, `mbedtls/pkcs5.h` — Crypto functions
- `LittleFS.h` — File system (in-memory, deterministic)
- `i2s_std_mock.h` — I2S driver (stub, no audio I/O)

**Mock patterns:**
```cpp
// From Arduino.h mock
class String : public std::string {
public:
  String() : std::string() {}
  String(const char *s) : std::string(s ? s : "") {}

  int toInt() const {
    try { return std::stoi(*this); }
    catch (...) { return 0; }
  }
};

// From Preferences.h mock
class Preferences {
private:
  static std::map<String, String> _data;
public:
  static void reset() { _data.clear(); }
  void putString(const char *key, const String &value) {
    _data[String(key)] = value;
  }
  String getString(const char *key, const String &defaultValue) {
    auto it = _data.find(String(key));
    return it != _data.end() ? it->second : defaultValue;
  }
};
```

**Mocking pattern in tests:**
```cpp
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#endif
```

This allows the same test to compile on both `native` (with mocks) and hardware (with real libraries).

**What to Mock:**
- Hardware interfaces (GPIO, I2S, I2C)
- System timing (millis, esp_timer)
- Persistent storage (Preferences, EEPROM, NVS)
- Random number generation (for deterministic crypto tests)
- Network interfaces (WiFi, MQTT, HTTP)

**What NOT to Mock:**
- Core algorithms (DSP, crypto math, state machines) — test the real logic
- Data structures (arrays, structs) — use real instances
- Pure computation functions — no need to mock input/output

## Fixtures and Factories

**Test Data (C++):**
```cpp
// From test_auth_handler.cpp
void resetLoginRateLimit() {
  _loginFailCount = 0;
  _lastFailTime = 0;
  _nextLoginAllowedMs = 0;
  _passwordNeedsMigration = false;
}

// Global test state (reset in setUp())
namespace TestAuthState {
  void reset() {
    for (int i = 0; i < MAX_SESSIONS; i++) {
      activeSessions[i].sessionId = "";
      activeSessions[i].createdAt = 0;
      activeSessions[i].lastSeen = 0;
    }
    mockWebPassword = "default_password";
    mockAPPassword = "ap_password";
    Preferences::reset();
    EspRandomMock::reset();
    resetLoginRateLimit();
  }
}
```

**Location (C++):**
- Test data declared at file scope in test file
- Reset in `setUp()` function
- No separate factory files — factories inline in test code

**Fixtures (JavaScript/E2E):**
- Location: `e2e/fixtures/ws-messages/*.json` (15 JSON files)
- Pattern: each fixture is a deterministic WS broadcast message
- Examples: `wifi-status.json`, `audio-channel-map.json`, `hal-device-state.json`

**Fixture usage in tests:**
```javascript
// From e2e/helpers/ws-helpers.js
function loadFixture(name) {
  return JSON.parse(fs.readFileSync(
    path.join(FIXTURE_DIR, `${name}.json`), 'utf8'
  ));
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

**Playwright fixtures (`e2e/helpers/fixtures.js`):**
```javascript
// connectedPage fixture — sets up authenticated session + WS + initial state
test.use({
  connectedPage: async ({ page }, use) => {
    // Set session cookie
    await page.context().addCookies([{
      name: 'sessionId',
      value: 'test-session-id',
      domain: 'localhost',
      path: '/',
    }]);

    // Navigate and authenticate WS
    await page.goto('/');
    // Mock server sends initial state broadcasts
    // connectedPage is now ready to use

    await use(page);
  }
});
```

## Coverage

**Requirements:**
- **Baseline**: ~1866 C++ tests across 78 modules (as of 2026-03-21)
- **No target enforcement** — coverage tracked informally
- **CI requirement**: All tests must pass (0 failures) before build/release

**View Coverage (C++):**
```bash
# Test modules report results to stdout
# No coverage HTML report generated (PlatformIO limitation on native)
pio test -e native -v    # Verbose output shows each test result
```

**View Coverage (JavaScript/E2E):**
```bash
# Playwright HTML report (generated on failure or with --reporter=html)
npx playwright test --reporter=html
# Opens test-results/index.html in browser
```

**Coverage tracking:**
- Browser tests: 44 tests covering auth, audio, DSP, WiFi, MQTT, HAL, OTA, debug
- C++ tests: 1866 tests covering utilities, state, HAL, audio pipeline, DSP, OTA, MQTT, auth
- Manual spot-checks via coverage report artifacts (CI uploads on failure)

## Test Types

**Unit Tests (C++):**
- Scope: Single module or function, isolated behavior
- Example: `test_auth_handler.cpp` — password hashing, session management, rate limiting
- Setup: Mock all dependencies (Preferences, random, millis, etc.)
- Duration: <1ms per test, ~30s total for all 1866 tests

**Integration Tests (C++):**
- Scope: Multi-module interactions (e.g., HAL device discovery → audio pipeline registration)
- Example: `test_hal_pipeline_bridge.cpp` — device state changes propagate to sink slots
- Setup: Real objects, mocked hardware (GPIO, I2S)
- Duration: <10ms per test

**Integration Tests (JavaScript/E2E):**
- Scope: Frontend + mock server (Express, WS) interactions
- Example: `audio-matrix.spec.js` — drag matrix cell → WS command sent → state broadcast received → UI updates
- Setup: Real Playwright page, mocked server, WS interception
- Duration: 100-500ms per test

**E2E Tests (full-stack simulation):**
- Scope: Login page, main UI, all tabs, state persistence
- No real hardware — mock server simulates firmware responses
- Example: `auth.spec.js` — login flow, session cookie, redirect
- Duration: 500ms-2s per test

## Common Patterns

**Async Testing (C++):**
Not applicable — native tests are synchronous.

**Async Testing (JavaScript/E2E):**
```javascript
test('async operation completes', async ({ page }) => {
  // All page operations are async
  await page.goto('/');

  // Wait for element and perform action
  const btn = page.locator('button[id="scanBtn"]');
  await btn.click();

  // Wait for response
  const response = await page.waitForResponse(
    resp => resp.url().includes('/api/hal/scan') && resp.status() === 200
  );
  const data = await response.json();
  expect(data.success).toBe(true);
});
```

**Error Testing (C++):**
```cpp
void test_password_verify_rejects_wrong_input(void) {
  String hashedPwd = hashPassword("correct");

  // Should fail for wrong password
  bool result = verifyPassword("wrong", hashedPwd);
  TEST_ASSERT_FALSE(result);

  // Should succeed for correct password
  result = verifyPassword("correct", hashedPwd);
  TEST_ASSERT_TRUE(result);
}
```

**Error Testing (JavaScript/E2E):**
```javascript
test('invalid password shows error message', async ({ page }) => {
  await page.goto('/login');

  // Try to login with wrong password
  const resp = await page.request.post('/api/auth/login', {
    data: { password: 'wrongpassword' },
  });

  // Expect 401
  expect(resp.status()).toBe(401);
  const body = await resp.json();
  expect(body.success).toBe(false);
});
```

**WebSocket Testing (JavaScript/E2E):**
```javascript
test('matrix gain change sends WS command', async ({ connectedPage }) => {
  const page = connectedPage;

  // Intercept WS outbound message
  let sentMessage = null;
  await page.routeWebSocket(/.*:81/, async (route) => {
    route.onMessage(msg => {
      try {
        sentMessage = JSON.parse(msg);
        console.log('WS sent:', sentMessage);
      } catch (e) {}
      route.continue();
    });
  });

  // Trigger matrix cell interaction
  const cell = page.locator('[data-matrix-cell="0,0"]');
  await cell.click();
  const input = page.locator('input[type="range"]');
  await input.fill('6');  // 6 dB gain

  // Wait for WS message
  await page.waitForTimeout(100);

  // Verify message was sent
  expect(sentMessage).not.toBeNull();
  expect(sentMessage.type).toBe('setMatrixGain');
  expect(sentMessage.slot).toBe(0);
  expect(sentMessage.gainDb).toBe(6);
});
```

**Binary Frame Testing (JavaScript/E2E):**
```javascript
test('waveform binary frame contains 256 samples', async ({ connectedPage }) => {
  const page = connectedPage;
  let waveformFrame = null;

  // Intercept binary WS message
  await page.routeWebSocket(/.*:81/, async (route) => {
    route.onMessage(msg => {
      if (msg instanceof ArrayBuffer) {
        const view = new Uint8Array(msg);
        if (view[0] === 0x01) {  // WS_BIN_WAVEFORM
          waveformFrame = view;
        }
      }
      route.continue();
    });
  });

  // Subscribe to audio
  await page.evaluate(() => wsSend({ type: 'subscribeAudio' }));

  // Wait for binary frame
  await page.waitForTimeout(200);

  // Verify frame structure
  expect(waveformFrame).not.toBeNull();
  expect(waveformFrame.length).toBe(258);  // 1 type + 1 reserved + 256 samples
  expect(waveformFrame[0]).toBe(0x01);
});
```

## Mandatory Test Coverage Rules

**Before completing any task:**

1. **C++ firmware changes** (`src/`):
   - Run: `pio test -e native -v` or use firmware-test-runner agent
   - New modules must have a test file in `test/test_<module>/`
   - Changed function signatures → update affected tests
   - **All tests must pass** before commit

2. **Web UI changes** (`web_src/`):
   - Run: `cd e2e && npx playwright test`
   - New toggle/button/dropdown → add test verifying correct WS command sent
   - New WS broadcast type → add fixture JSON + test verifying DOM update
   - New tab/section → add navigation + element presence tests
   - Changed element IDs → update `e2e/helpers/selectors.js` + specs
   - Removed features → delete corresponding tests + fixtures
   - New top-level JS declarations → add to `web_src/.eslintrc.json` globals

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

**Pre-commit checks (`.githooks/pre-commit`):**
1. `node tools/find_dups.js` — Detect duplicate JS declarations across files
2. `node tools/check_missing_fns.js` — Find undefined function references
3. `npx eslint web_src/js/ --config web_src/.eslintrc.json` — ESLint all JS

**Runs before every commit** — prevents committing broken code.

## CI/CD Quality Gates

All 4 gates must pass before firmware build/release proceeds (`.github/workflows/tests.yml`):

1. **cpp-tests** (`pio test -e native -v`) — 1866 C++ tests
2. **cpp-lint** (cppcheck) — Static analysis on `src/`
3. **js-lint** (find_dups + check_missing_fns + ESLint) — JavaScript validation
4. **e2e-tests** (Playwright) — 44 browser tests

Execution: **parallel** (all 4 gates run simultaneously).
On failure: Playwright HTML report uploaded as artifact (14-day retention).

---

*Testing analysis: 2026-03-21*
