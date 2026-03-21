# Testing Patterns

**Analysis Date:** 2026-03-21

## Test Framework

**C++ Unit Tests (Firmware):**
- **Runner**: Unity (open-source C unit test framework)
- **Environment**: `native` platform (host machine gcc/MinGW, no hardware)
- **Config**: `platformio.ini [env:native]`
- **Build flags**: `-D UNIT_TEST -D NATIVE_TEST -D DSP_ENABLED`
- **Run commands**:
  ```bash
  pio test -e native              # Run all tests
  pio test -e native -v           # Verbose output
  pio test -e native -f test_wifi # Run specific module
  ```
- **Test count**: ~1732 assertions across 73+ modules (as of 2026-03-21)

**JavaScript Browser Tests (Web UI):**
- **Runner**: Playwright v1.40+ (Chromium only)
- **Location**: `e2e/tests/*.spec.js` (26 tests across 19 specs)
- **Mock server**: Express.js on localhost:3000, in `e2e/mock-server/`
- **Run commands**:
  ```bash
  cd e2e && npm install              # First time
  npx playwright install --with-deps # First time
  npx playwright test                # All 26 tests
  npx playwright test tests/auth.spec.js    # Single spec
  npx playwright test --headed       # Visible browser
  npx playwright test --debug        # Inspector mode
  ```
- **Test output**: HTML report on failure (14-day artifact retention)

## Test File Organization

**C++ Location:**
- Pattern: `test/test_<module>/test_<module>.cpp`
- One test file per directory (prevents duplicate main/setUp/tearDown symbols)
- Mocks live in `test/test_mocks/`
- Examples:
  - `test/test_audio_pipeline/test_audio_pipeline.cpp`
  - `test/test_auth/test_auth_handler.cpp`
  - `test/test_hal_discovery/test_hal_discovery.cpp`

**JavaScript Location:**
- Pattern: `e2e/tests/<feature>.spec.js`
- Examples:
  - `e2e/tests/auth.spec.js` — login, session, cookies
  - `e2e/tests/audio-inputs.spec.js` — input routing
  - `e2e/tests/audio-matrix.spec.js` — matrix cell manipulation
  - `e2e/tests/wifi-network.spec.js` — WiFi connect/scan

**Naming (C++):**
- Test names: `test_<function_underscore>_<scenario>` (snake_case)
- Examples:
  - `void test_audio_pipeline_matrix_identity()` — identity matrix routes input 0 → output 0
  - `void test_audio_pipeline_set_sink_validates_bounds()` — validates slot >= 0
  - `void test_auth_pbkdf2_matches_known_vector()` — validates PBKDF2 vs reference

**Naming (JavaScript):**
- Test names: descriptive sentence starting with lowercase verb
- Examples:
  - `test('login page renders with password field and submit button', ...)`
  - `test('correct password submits and redirects to main page', ...)`
  - `test('invalid session cookie results in error', ...)`

## Test Structure

**C++ Test Suite:**

```cpp
#include <unity.h>
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"

// ===== Setup and Teardown =====
void setUp(void) {
    // Reset mocks before each test
    // Example: clear static arrays, reset global state
}

void tearDown(void) {
    // Cleanup after each test (optional)
}

// ===== Test Cases =====
void test_module_function_scenario(void) {
    // Arrange: set up test data
    float input[4] = { 1.0f, 0.5f, -0.5f, -1.0f };
    float expected[4] = { /* ... */ };

    // Act: call the function under test
    float output[4] = {};
    some_function(input, output, 4);

    // Assert: verify the result
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, expected[i], output[i]);
    }
}

// ===== Test Runner =====
// Unity auto-invokes setUp(), test_*(), tearDown() in sequence
```

**JavaScript Test Suite:**

```javascript
const { test, expect } = require('@playwright/test');

test('user can login with valid password', async ({ page, request, baseURL }) => {
  // Arrange: Navigate to login page
  await page.goto('/login');

  // Act: Enter password and submit
  await page.locator('input[type="password"]').fill('testpassword');
  await page.locator('button[type="submit"]').click();

  // Assert: Verify redirect or state change
  await expect(page).toHaveURL('/');
});

test('invalid password shows error message', async ({ page }) => {
  await page.goto('/login');

  // Intercept POST and simulate failure
  await page.route('/api/auth/login', async (route) => {
    await route.abort('failed');
  });

  await page.locator('input[type="password"]').fill('wrongpassword');
  await page.locator('button[type="submit"]').click();

  // Verify error is displayed
  await expect(page.locator('.error-message')).toBeVisible();
});
```

**Playwright Fixtures:**

The `connectedPage` fixture in `e2e/helpers/fixtures.js` provides:
1. Valid session cookie (via mock `/api/auth/login`)
2. WebSocket connection on port 81 mocked at browser level (`page.routeWebSocket()`)
3. Auth handshake automated (fetch `/api/ws-token`, send auth token)
4. Initial state broadcasts pre-loaded from fixtures
5. Waits until `#wsConnectionStatus` reads "Connected"

Usage:
```javascript
const { test, expect } = require('../helpers/fixtures');

test('audio tab shows input sources', async ({ connectedPage }) => {
  const page = connectedPage;

  // Page is already authenticated and WS is connected
  await page.locator('[data-tab="audio"]').click();

  // Verify audio elements render
  await expect(page.locator('#audio-inputs-container')).toBeVisible();
});
```

## Mocking

**C++ Mocks (test_mocks/ directory):**

Mock headers replicate Arduino/ESP-IDF APIs for native compilation:
- `Arduino.h` — millis(), micros(), digitalWrite(), pinMode(), analogRead()
- `Preferences.h` — key-value storage interface
- `WiFi.h` — WiFi class stubs
- `PubSubClient.h` — MQTT client stubs
- `Wire.h` — I2C bus mock (beginTransmission, write, read, endTransmission)
- `i2s_std_mock.h` — I2S driver stubs
- `esp_random.h`, `esp_timer.h`, `mbedtls/md.h` — crypto stubs
- `LittleFS.h` — file system mock

Example mock usage:
```cpp
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif
```

**JavaScript Mocks (e2e/mock-server/ and e2e/fixtures/):**

Mock server provides:
- Express routes matching firmware REST API (port 3000)
- Route files in `e2e/mock-server/routes/`:
  - `auth.js` — login, ws-token, status endpoints
  - `audio.js` — pipeline matrix, sink/source config
  - `hal.js` — device list, discovery, config
  - `wifi.js` — scan, connect, AP mode
  - `mqtt.js` — broker config, discovery status
  - `dsp.js` — DSP config CRUD
  - (and others: ota.js, settings.js, system.js, etc.)
- Mock state in `e2e/mock-server/ws-state.js` — singleton holding appState snapshot
- WS fixtures in `e2e/fixtures/ws-messages/` — 15 JSON broadcast examples

**What to mock:**
- Hardware: GPIO, I2C, UART (use mock headers)
- External APIs: WiFi, MQTT brokers (use mock server routes)
- Hardware I/O: ADC reads, DAC writes (return sensible values)
- Time: use `millis()` mock for time-based logic

**What NOT to mock:**
- Core algorithm logic (DSP math, matrix operations)
- State machine transitions
- Allocation/deallocation patterns (test actual behavior)
- Concurrency primitives (FreeRTOS semaphores) — use real implementations if testable

## Fixtures and Factories

**C++ Test Data:**

Test fixtures are inline in the test file or static arrays at module scope:
```cpp
// Example: Known biquad coefficient vector
static const float BQ_COEFFS_LOW_SHELF[] = {
    0.5f, 0.0f, 0.0f,  // B coeffs (numerator)
    1.0f, -0.5f, 0.0f  // A coeffs (denominator)
};

void test_dsp_biquad_low_shelf(void) {
    DspBiquadState state = {};
    float input[16] = { /* ... */ };
    float output[16] = {};

    dsp_apply_biquad(state, BQ_COEFFS_LOW_SHELF, input, output, 16);

    // Assert output
}
```

**JavaScript Test Fixtures:**

Fixtures stored as JSON files in `e2e/fixtures/`:
- `e2e/fixtures/ws-messages/authSuccess.json` — WS auth response
- `e2e/fixtures/ws-messages/hardwareStats.json` — system info broadcast
- `e2e/fixtures/ws-messages/audioChannelMap.json` — input/output device list
- `e2e/fixtures/api-responses/authStatus.json` — REST `/api/auth/status`
- `e2e/fixtures/api-responses/halDevices.json` — HAL device list

Helpers in `e2e/helpers/ws-helpers.js`:
- `buildInitialState()` — assemble all initial broadcasts
- `handleCommand(wsMsg)` — simulate firmware response to client command
- Binary frame builders for waveform/spectrum data

Example:
```javascript
const { buildInitialState } = require('../helpers/ws-helpers');

test('hardware stats displayed on boot', async ({ connectedPage }) => {
  const page = connectedPage;

  // Initial state includes hardwareStats fixture
  const hardwareStats = page.evaluate(() => {
    // Access the mock state injected by fixture
    return window.hardwareStatsFromWs;
  });

  expect(hardwareStats.heapFreeInternal).toBeGreaterThan(0);
});
```

## Coverage

**Requirements:**
- No automated coverage enforcement, but new code should have tests
- Critical paths must be tested: auth, audio pipeline, DSP, HAL discovery, OTA, MQTT

**View coverage (native):**
```bash
pio test -e native -v            # Verbose output shows assertion counts
# Coverage report generation not integrated (gcov available if needed)
```

**Manual coverage audit:**
- Review test count per module in MEMORY.md (e.g., "1732 tests across 75 modules")
- Check CONCERNS.md for untested areas
- Playwright E2E coverage: 19 specs covering critical UI paths

**Coverage targets (informal):**
- Core algorithms: 90%+ (DSP, matrix, conversion)
- Public API functions: 100% (at least basic happy path)
- Error paths: 80%+ (boundary checks, allocation failures)
- UI interactions: 70%+ (E2E tests for critical workflows)

## Test Types

**Unit Tests (C++):**
- Scope: Single function or module
- Approach: No hardware, use mocks, test pure logic
- Example: `test_dsp_biquad_apply()` — feed sinusoid through filter, verify magnitude response
- Runs on native platform (~0.5s total, 1732 tests)

**Integration Tests (C++):**
- Scope: Multiple modules together (e.g., HAL discovery → pipeline sink registration)
- Approach: Mock hardware interfaces, test end-to-end behavior
- Example: `test_hal_discovery_i2c_bus_conflict()` — WiFi active → skip Bus 0, emit diagnostic
- Runs alongside unit tests in native environment

**E2E Tests (JavaScript/Playwright):**
- Scope: Full web UI workflow against mock Express server + mocked WebSocket
- Approach: Browser automation, no real hardware, deterministic fixtures
- Example: `test('user navigates to audio tab and routes input to output', ...)`
- 26 tests covering auth, WiFi, audio, HAL, MQTT, OTA, debug console, health dashboard
- Runs in Chromium only (no multi-browser needed)

**Not included (no test harness):**
- Hardware integration tests (real ESP32-P4 board)
- Performance/stress tests (embedded real-time constraint)
- Load tests (WiFi client counts, MQTT publish rate)

## Common Patterns

**Async Testing (C++):**
- No async in native tests (single-threaded)
- Timer-based logic tested with mock `millis()` advancement
- Example:
  ```cpp
  void test_auto_off_timer_fires_after_delay(void) {
      smart_sensing_set_auto_off_enabled(true, 5000);  // 5 second timeout

      // Advance mock time past timeout
      mock_millis = 6000;
      smart_sensing_update();

      TEST_ASSERT_TRUE(amplifier_should_disable());  // Auto-off fires
  }
  ```

**Async Testing (JavaScript/Playwright):**
- All async operations use `await` or `.then().catch()`
- Wait for elements/conditions before assertions
- Example:
  ```javascript
  test('wifi connects after submit', async ({ connectedPage }) => {
    const page = connectedPage;

    // Arrange: intercept the WiFi connect request
    await page.route('/api/wifi/config', async (route) => {
      await new Promise(r => setTimeout(r, 500));  // Simulate delay
      await route.continue();
    });

    // Act: fill and submit form
    await page.locator('input[name="ssid"]').fill('TestNetwork');
    await page.locator('button:has-text("Connect")').click();

    // Assert: wait for status update
    await expect(page.locator('#statusWifi')).toContainText('Connected', { timeout: 2000 });
  });
  ```

**Error Testing (C++):**
- Test boundary conditions and failure modes explicitly
- Example:
  ```cpp
  void test_audio_pipeline_set_sink_rejects_out_of_bounds(void) {
      const AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;

      bool result = audio_pipeline_set_sink(AUDIO_OUT_MAX_SINKS + 1, &sink);

      TEST_ASSERT_FALSE(result);  // Should reject invalid slot
  }
  ```

**Error Testing (JavaScript):**
- Intercept failed requests, verify error UI
- Example:
  ```javascript
  test('hal scan shows error on I2C failure', async ({ connectedPage }) => {
    const page = connectedPage;

    // Route scan to return error
    await page.route('/api/hal/scan', async (route) => {
      await route.abort('failed');
    });

    await page.locator('button:has-text("Rescan")').click();

    // Verify error toast appears
    await expect(page.locator('.toast.error')).toBeVisible();
  });
  ```

**WS Binary Frame Testing (JavaScript):**
- Manually construct binary frames, inject via connectedPage.wsRoute
- Example:
  ```javascript
  test('waveform binary frame updates canvas', async ({ connectedPage }) => {
    const page = connectedPage;

    // Construct binary waveform frame: [type:0x01][adc:0x00][samples:256×u8]
    const buf = new ArrayBuffer(258);
    const dv = new DataView(buf);
    dv.setUint8(0, 0x01);  // type = waveform
    dv.setUint8(1, 0);     // adc = 0
    for (let i = 0; i < 256; i++) {
      dv.setUint8(2 + i, Math.floor(Math.random() * 256));
    }

    // Inject via mock WS
    await connectedPage.wsRoute.send(buf);

    // Verify canvas was redrawn
    const canvas = page.locator('#audioWaveformCanvas0');
    await expect(canvas).toHaveJSProperty('width', 256);
  });
  ```

## Pre-commit Hooks

**Location**: `.githooks/pre-commit` (activate with `git config core.hooksPath .githooks`)

**Checks** (run before every commit):
1. `node tools/find_dups.js` — detect duplicate `let`/`const` declarations in JS files
2. `node tools/check_missing_fns.js` — detect undefined function references in JS
3. ESLint on `web_src/js/` — linting rules from `web_src/.eslintrc.json`

**Failure behavior**: If any check fails, commit is blocked. Fix the issues and retry.

## CI/CD Quality Gates

**GitHub Actions** (`.github/workflows/tests.yml`):

4 parallel quality gates (all must pass before firmware build):

1. **cpp-tests** (`pio test -e native -v`)
   - Runs ~1732 Unity tests across 73+ modules
   - Fails if any assertion fails
   - Takes ~30-60 seconds

2. **cpp-lint** (cppcheck on `src/`)
   - Static analysis excluding `src/gui/`
   - Checks for memory leaks, buffer overflows, suspicious logic
   - Fails on any issues (warnings configured as errors)

3. **js-lint** (find_dups + check_missing_fns + ESLint)
   - `node tools/find_dups.js` — duplicate declarations
   - `node tools/check_missing_fns.js` — undefined references
   - `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`
   - Fails on any issues

4. **e2e-tests** (`cd e2e && npx playwright test`)
   - Runs 26 Playwright tests in Chromium
   - Mock Express server on localhost:3000
   - Fails if any test fails
   - Takes ~60-90 seconds
   - HTML report uploaded to artifacts on failure (14-day retention)

**Release gate** (`.github/workflows/release.yml`):
- Runs same 4 quality gates before publishing release
- Manual trigger only

**Success criteria**: All 4 gates pass + no build errors

---

*Testing analysis: 2026-03-21*
