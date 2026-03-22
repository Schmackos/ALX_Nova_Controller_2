# Testing Patterns

**Analysis Date:** 2026-03-22

## Test Framework

**Runner (C++ Unit Tests):**
- **Framework:** Unity (embedded C testing framework)
- **Platform:** `native` (host machine with gcc/MinGW, no hardware)
- **Build flags:** `-D UNIT_TEST -D NATIVE_TEST`
- **Config file:** `platformio.ini` sections `[env:native]`
- **Test discovery:** Each test module in `test/test_<module_name>/test_<module_name>.cpp`

**Build Configuration (native environment):**
```ini
[env:native]
platform = native
test_framework = unity
lib_deps = bblanchon/ArduinoJson@^7.4.2, kosme/arduinoFFT@^2.0
lib_compat_mode = off
lib_ignore = WebSockets
test_ignore = test_mocks
test_build_src = no
```

**Assertion Library:**
- Unity built-in assertions: `TEST_ASSERT_*` macros
- Common patterns:
  - `TEST_ASSERT_TRUE(condition)` / `TEST_ASSERT_FALSE(condition)`
  - `TEST_ASSERT_EQUAL(expected, actual)`
  - `TEST_ASSERT_EQUAL_STRING(expected, actual)`
  - `TEST_ASSERT_FLOAT_WITHIN(tolerance, expected, actual)`
  - `TEST_ASSERT_NULL(ptr)` / `TEST_ASSERT_NOT_NULL(ptr)`

**Run Commands:**
```bash
# Run all C++ unit tests (2933 tests across 105 modules)
pio test -e native

# Run specific test module
pio test -e native -f test_wifi
pio test -e native -f test_auth
pio test -e native -f test_dsp

# Verbose output
pio test -e native -v

# Watch mode (repeat on file changes)
pio test -e native --monitor
```

---

## Test File Organization

**Location Pattern:**
- Each test module gets its own directory: `test/test_<module_name>/`
- Single C++ source file per module: `test/test_<module_name>/test_<module_name>.cpp`
- Prevents duplicate symbol conflicts (`setUp`/`tearDown`/`main`)

**Naming Convention:**
- Module test file matches module name: `src/audio_pipeline.cpp` → `test/test_audio_pipeline/test_audio_pipeline.cpp`
- Test function names: `test_<feature_being_tested>()` with underscores
- Examples: `test_lpf_coefficients()`, `test_save_single_network()`, `test_resolve_i2s_pin_override()`

**Directory Structure:**
```
test/
├── test_mocks/           # Mock implementations of Arduino, WiFi, MQTT, NVS
├── test_auth/
│   └── test_auth_handler.cpp
├── test_wifi/
│   └── test_wifi_manager.cpp
├── test_audio_pipeline/
│   └── test_audio_pipeline.cpp
├── test_dsp/
│   └── test_dsp.cpp
├── test_hal_*/           # 60+ HAL driver tests
├── test_utils/
│   └── test_utils.cpp
└── ... (105 test modules total)
```

**Excluded Modules:**
- `test_mocks/` — Skipped via `test_ignore = test_mocks` in platformio.ini

---

## Test Structure

**Standard Pattern (Unity setup/teardown):**
```cpp
void setUp(void) {
    // Reset all state before each test
    TestWiFiState::reset();
    dsp_init();
    // Clear mock counters
}

void tearDown(void) {
    // Optional: cleanup after each test
}

void test_feature_name(void) {
    // Arrange: Set up test data
    int result = someFunction();

    // Act: Execute function under test
    bool success = doSomething(result);

    // Assert: Verify result
    TEST_ASSERT_TRUE(success);
}
```

**Test Grouping (descriptive comments):**
```cpp
// ===== Credential Persistence Tests =====

void test_save_single_network(void) { /* ... */ }
void test_save_multiple_networks(void) { /* ... */ }

// ===== Static IP Configuration Tests =====

void test_save_network_with_static_ip(void) { /* ... */ }
```

**Typical Patterns from Codebase:**

From `test/test_wifi/test_wifi_manager.cpp`:
```cpp
void test_save_single_network(void) {
    bool saved = saveWiFiNetwork("TestSSID", "password123");

    TEST_ASSERT_TRUE(saved);
    TEST_ASSERT_EQUAL(1, wifiNetworkCount);
    TEST_ASSERT_EQUAL_STRING("TestSSID", wifiNetworks[0].ssid.c_str());
}

void test_update_existing_network(void) {
    saveWiFiNetwork("MyNetwork", "oldpassword");
    int countAfterFirst = wifiNetworkCount;

    // Act: Update same network
    saveWiFiNetwork("MyNetwork", "newpassword");

    // Assert: Count unchanged, password updated
    TEST_ASSERT_EQUAL(countAfterFirst, wifiNetworkCount);
    TEST_ASSERT_EQUAL_STRING("newpassword", wifiNetworks[0].password.c_str());
}
```

From `test/test_dsp/test_dsp.cpp`:
```cpp
void test_lpf_coefficients(void) {
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.Q = 0.707f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_LPF, 48000);

    // DC gain = (b0+b1+b2)/(1+a1+a2) should be ~1.0 for LPF
    float dcGain = (p.coeffs[0] + p.coeffs[1] + p.coeffs[2]) /
                   (1.0f + p.coeffs[3] + p.coeffs[4]);
    TEST_ASSERT_FLOAT_WITHIN(COEFF_TOL, 1.0f, dcGain);
    TEST_ASSERT_TRUE(p.coeffs[0] > 0.0f);
}
```

---

## Mocking

**Mock Framework:**
- Custom mock implementations in `test/test_mocks/` (not a commercial library)
- Replicate Arduino/IDF API surface for testing in native environment

**Available Mocks:**
- `Arduino.h` — `millis()`, `micros()`, `delay()`, GPIO functions
- `Preferences.h` — NVS key-value storage mock
- `esp_random.h` — ESP random number generation
- `esp_timer.h` — FreeRTOS timer callbacks
- `mbedtls/md.h`, `mbedtls/pkcs5.h` — Crypto (real implementation, not mocked)
- WiFi/MQTT mocks via `PubSubClient` (injected into test via `#ifdef NATIVE_TEST`)

**Mock Usage Pattern:**
```cpp
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#endif
```

**What to Mock:**
- Hardware I/O (GPIO reads/writes, I2C, I2S)
- Timing functions (`millis()`, `delay()`)
- Persistent storage (NVS via `Preferences`)
- Network APIs (WiFi, MQTT `PubSubClient`)

**What NOT to Mock:**
- Cryptographic functions (use real `mbedtls` implementation for security testing)
- Core business logic (compute functions, DSP filters)
- Standard library functions (`string`, `vector`, math functions)

**State Management in Tests:**
- Mock objects store state in static/module-local variables
- `setUp()` calls `reset()` methods to clear mock state between tests
- Example: `TestWiFiState::reset()` clears all mock WiFi networks

**Example Mock Implementation** (from `test_auth_handler.cpp`):
```cpp
// Mock state
String mockWebPassword = "default_password";
Session activeSessions[MAX_SESSIONS];

// Mock function
bool timingSafeCompare(const String &a, const String &b) {
  size_t lenA = a.length();
  size_t lenB = b.length();
  size_t maxLen = (lenA > lenB) ? lenA : lenB;

  if (maxLen == 0) {
    return (lenA == 0 && lenB == 0);
  }

  volatile uint8_t result = (lenA != lenB) ? 1 : 0;
  // Timing-safe comparison loop...
  return result == 0;
}

// Cleanup in setUp
void setUp(void) {
    mockWebPassword = "default_password";
    memset(activeSessions, 0, sizeof(activeSessions));
}
```

---

## Fixtures and Factories

**Test Data:**
- Hardcoded constants at module scope (no factory libraries)
- Test structures initialized inline in test functions
- Example from `test/test_dsp/test_dsp.cpp`:
  ```cpp
  #define FLOAT_TOL 0.001f
  #define COEFF_TOL 0.01f

  void test_lpf_coefficients(void) {
      DspBiquadParams p;
      dsp_init_biquad_params(p);  // Initialize to defaults
      p.frequency = 1000.0f;      // Configure for test
      // ... test code
  }
  ```

**Location:**
- Fixture data inline in test files (no separate fixture directories)
- Common constants defined as `#define` at module top
- Example: `MAX_BUFFER = 256`, `FLOAT_TOL = 0.001f`

**Factory Pattern:**
- No factory functions (simple inline initialization)
- Reset functions for mock state: `TestWiFiState::reset()`

---

## Coverage

**Requirements:**
- No hard minimum enforced by CI (optional monitoring)
- Best effort: every new code path should have a test
- Critical paths (auth, DSP, pipeline) have extensive coverage

**View Coverage:**
- Native tests don't generate coverage reports (feature not configured in platformio.ini)
- Manual code review for untested paths
- Test count metrics tracked: ~2933 C++ tests across 105 modules

---

## Test Types

**Unit Tests (C++ native platform):**
- **Scope:** Single function or small module
- **Approach:** Mock all external dependencies (hardware, network, storage)
- **Count:** 2933 tests across 105 modules
- **Examples:**
  - `test_auth_handler.cpp` — PBKDF2, password hashing, session management
  - `test_dsp.cpp` — Biquad coefficient generation, filtering
  - `test_wifi_manager.cpp` — Network list management, RSSI conversion
  - `test_audio_pipeline.cpp` — Source/sink registration, matrix routing
  - `test_hal_*.cpp` — Individual HAL driver initialization and I2C control (60+ tests)

**Integration Tests (C++ native platform):**
- **Scope:** Multiple modules interacting
- **Approach:** Combine tested units (e.g., DSP + pipeline)
- **Examples:**
  - `test_audio_pipeline.cpp` tests pipeline + multiple source/sink registrations
  - `test_hal_integration/` tests HAL manager + discovery + lifecycle transitions
  - HAL bridge tests verify device state callbacks → sink removal

**E2E Tests (Playwright browser tests):**
- **Framework:** Playwright (22 test specs)
- **Count:** 113 tests across 22 specs
- **Scope:** Full web UI against mock Express server + WebSocket
- **No real hardware needed** — Express mock + WS state injection
- **File locations:**
  - `e2e/tests/*.spec.js` — Test specifications (22 specs)
  - `e2e/mock-server/` — Express app (port 3000) replicating firmware REST API
  - `e2e/mock-server/assembler.js` — HTML assembly (mirrors `tools/build_web_assets.js`)
  - `e2e/mock-server/routes/` — 12 Express route handlers matching firmware endpoints
  - `e2e/helpers/` — Playwright helpers and selectors
  - `e2e/fixtures/` — WS message + API response JSON fixtures

**E2E Test Commands:**
```bash
cd e2e
npm install                              # First time
npx playwright install --with-deps chromium  # First time
npx playwright test                      # Run all 113 tests
npx playwright test tests/auth.spec.js   # Run single spec
npx playwright test --headed             # Visible browser
npx playwright test --debug              # Inspector mode
```

**E2E Test Patterns (Playwright):**

From `e2e/tests/auth.spec.js`:
```javascript
test('login page renders with password field and submit button', async ({ page }) => {
  await page.goto('/login');

  const pwdInput = page.locator('input[type="password"]');
  const submitBtn = page.locator('button[type="submit"], input[type="submit"]');

  await expect(pwdInput).toBeVisible();
  await expect(submitBtn).toBeVisible();
});

test('correct password submits and redirects to main page', async ({ page }) => {
  await page.goto('/login');

  await page.route('/api/auth/login', async (route) => {
    await route.continue();
  });

  await page.locator('input[type="password"]').fill('anypassword');
  await page.locator('button[type="submit"]').click();

  const resp = await page.request.post('/api/auth/login', {
    data: { password: 'anypassword' },
  });
  const body = await resp.json();
  expect(body.success).toBe(true);
});
```

**Key Playwright Patterns:**
- **WS interception:** `page.routeWebSocket(/.*:81/, handler)` — intercepts WebSocket messages
  - Handler: `onMessage(msg)` (capital M) / `onClose()` (capital C)
- **Tab switching:** `page.evaluate(() => switchTab('tabName'))` — safer than sidebar clicks
- **CSS-hidden checkboxes:** Use `.toBeChecked()` not `.toBeVisible()` for styled inputs
- **Strict mode:** Use `.first()` when selector matches multiple elements
- **Mock response injection:** Fixtures in `e2e/fixtures/ws-messages/*.json` provide deterministic WS broadcasts

---

## Common Patterns

**Async Testing (C++ Unity):**
- No async patterns in native C++ tests (synchronous execution)
- I2S/WiFi async operations mocked to return immediately

**Error Testing:**
```cpp
void test_invalid_input_returns_false(void) {
    bool result = saveWiFiNetwork("", "password");  // Empty SSID
    TEST_ASSERT_FALSE(result);
}

void test_null_pointer_returns_nullptr(void) {
    const AudioInputSource* src = audio_pipeline_get_source(-1);  // Invalid lane
    TEST_ASSERT_NULL(src);
}
```

**Boundary Testing:**
```cpp
void test_rssi_to_quality_boundaries(void) {
    TEST_ASSERT_EQUAL(0, rssiToQuality(-100));   // Min
    TEST_ASSERT_EQUAL(100, rssiToQuality(-50));  // Max
    TEST_ASSERT_EQUAL(50, rssiToQuality(-75));   // Midpoint
}

void test_save_rejects_sixth_network(void) {
    // Save 5 networks (max)
    for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
        saveWiFiNetwork("NetworkX", "pwd");
    }
    // 6th should fail
    bool result = saveWiFiNetwork("Network6", "pwd");
    TEST_ASSERT_FALSE(result);
}
```

**Float Comparisons:**
```cpp
#define FLOAT_TOL 0.001f

void test_float_equals(void) {
    float result = someFloatFunc();
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, result);
}
```

---

## Mandatory Test Coverage Rules

**Every code change MUST keep tests green.** Before completing any task:

### C++ Firmware Changes (`src/`)
1. Run `pio test -e native -v` or use the `firmware-test-runner` agent
2. New modules require a test file in `test/test_<module>/`
3. Changed function signatures → update affected tests
4. New test modules must not have duplicate `setUp`/`tearDown` (separate directory prevents conflicts)

### Web UI Changes (`web_src/`)
1. Run `cd e2e && npx playwright test` or use the `test-engineer` agent
2. New toggle/button/dropdown → add test verifying it sends correct WS command
3. New WS broadcast type → add fixture JSON + test verifying DOM updates
4. New tab or section → add navigation + element presence tests
5. Changed element IDs → update `e2e/helpers/selectors.js` + affected specs
6. Removed features → remove corresponding tests + fixtures
7. New top-level JS declarations → add to `web_src/.eslintrc.json` globals

### WebSocket Protocol Changes (`src/websocket_handler.cpp`)
1. Update `e2e/fixtures/ws-messages/` with new/changed message fixtures
2. Update `e2e/helpers/ws-helpers.js` `buildInitialState()` and `handleCommand()`
3. Update `e2e/mock-server/ws-state.js` if new state fields are added
4. Add Playwright test verifying frontend handles the new message type

### REST API Changes (`src/main.cpp`, `src/hal/hal_api.cpp`)
1. Update matching route in `e2e/mock-server/routes/*.js`
2. Update `e2e/fixtures/api-responses/` with new/changed response fixtures
3. Add Playwright test if UI depends on the new endpoint

### Agent Workflow for Test Maintenance

**Always verify tests after code changes.** Use specialized agents:

| Change Type | Agent(s) | What They Do |
|---|---|---|
| C++ firmware only | `firmware-test-runner` | Runs `pio test -e native -v`, diagnoses failures |
| Web UI only | `test-engineer` or `test-writer` | Runs Playwright, fixes selectors, adds coverage |
| Both firmware + UI | **Both agents in parallel** | Full coverage verification |
| New HAL driver | `hal-driver-scaffold` → `firmware-test-runner` | Scaffold creates test automatically |
| New web feature | `web-feature-scaffold` → `test-engineer` | Scaffold creates DOM + E2E tests |
| Bug investigation | `debugger` or `debug` agent | Root cause with test reproduction |

**Parallel execution pattern:**
```
Agent 1: "Run pio test -e native -v and report results"
Agent 2: "Run cd e2e && npx playwright test and fix any failures"
```

---

## Static Analysis (CI-enforced)

**ESLint** (`web_src/.eslintrc.json`):
- Lints all JS files with 380 globals for concatenated scope
- Rules: `no-undef`, `no-redeclare`, `eqeqeq: smart`
- Run: `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`

**cppcheck:**
- C++ static analysis on `src/` (excludes `src/gui/`)
- Run in CI only (not developer workflow)

**find_dups.js + check_missing_fns.js:**
- Duplicate/missing JS declaration checks
- Run: `node tools/find_dups.js && node tools/check_missing_fns.js`

---

## Quality Gates (CI/CD)

All 4 gates must pass before firmware build proceeds (parallel):

| Gate | Command | Scope |
|------|---------|-------|
| cpp-tests | `pio test -e native -v` | 2933 Unity tests, 105 modules |
| cpp-lint | `cppcheck` on `src/` | Static analysis (excludes `src/gui/`) |
| js-lint | find_dups + check_missing_fns + ESLint | JavaScript declarations + linting |
| e2e-tests | `npx playwright test` (22 specs) | 113 browser tests |

Pipeline diagram: `docs-internal/architecture/ci-quality-gates.mmd`

---

## Pre-commit Hooks

`.githooks/pre-commit` runs fast checks before every commit:

1. `node tools/find_dups.js` — duplicate JS declarations
2. `node tools/check_missing_fns.js` — undefined function references
3. ESLint on `web_src/js/`

Activate: `git config core.hooksPath .githooks`

---

*Testing analysis: 2026-03-22*
