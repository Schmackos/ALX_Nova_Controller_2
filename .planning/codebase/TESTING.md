# Testing Patterns

**Analysis Date:** 2026-03-09

## Test Framework Overview

Two independent test layers exist — neither requires hardware.

| Layer | Framework | Count | Location |
|-------|-----------|-------|----------|
| C++ Unit Tests | Unity (PlatformIO native) | 1614 tests, 70 modules | `test/` |
| Browser E2E Tests | Playwright + Chromium | 26 tests, 19 specs | `e2e/` |

---

## C++ Unit Tests (Unity / Native Platform)

### Runner

- **Framework:** Unity (bundled with PlatformIO)
- **Platform:** `native` (compiles and runs on host with gcc/MinGW — no hardware)
- **Config:** `platformio.ini` `[env:native]`
- **Build flags:** `-D UNIT_TEST -D NATIVE_TEST`
- **`test_build_src = no`** — tests never compile `src/` automatically; each test file directly `#include`s the `.cpp` files it needs

**Run Commands:**
```bash
pio test -e native              # Run all 70 modules
pio test -e native -v           # Verbose output (show pass/fail per test)
pio test -e native -f test_wifi # Run a single module
pio test -e native -f test_hal_core
```

### Test File Organization

- Location: `test/test_<module>/test_<module>.cpp` (one `.cpp` per directory)
- Naming: `test_<module_name>` directory, `test_<module_name>.cpp` file inside
- Each directory maps to exactly one test binary — no duplicate `main()`/`setUp()`/`tearDown()` symbols

```
test/
├── test_wifi/
│   └── test_wifi_manager.cpp     # All WiFi tests
├── test_hal_core/
│   └── test_hal_core.cpp         # HAL device manager tests
├── test_hal_bridge/
│   └── test_hal_bridge.cpp       # HAL pipeline bridge tests
├── test_mocks/
│   ├── Arduino.h                 # millis(), digitalWrite(), LEDC, etc.
│   ├── WiFi.h                    # WiFiClass with mock scan/connect
│   ├── Preferences.h             # NVS mock (std::map namespace storage)
│   ├── LittleFS.h                # Filesystem mock
│   ├── Wire.h                    # I2C mock
│   ├── PubSubClient.h            # MQTT client mock
│   ├── IPAddress.h               # IP address parsing mock
│   └── i2s_std_mock.h            # I2S driver mock
```

### Test Structure

**Suite file structure:**
```cpp
#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
// ... other mocks
#else
#include <Arduino.h>
#endif

// Inline .cpp source files being tested (test_build_src = no)
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// Optional: concrete test device
class TestDevice : public HalDevice { /* ... */ };

static HalDeviceManager* mgr;

void setUp(void) {
    // Arrange: reset all state before each test
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
    ArduinoMock::reset();
}

void tearDown(void) {}

// Individual test — Arrange-Act-Assert pattern
void test_register_and_get_device() {
    TestDevice dev("ti,pcm5102a", HAL_DEV_DAC);        // Arrange
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);  // Act

    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);             // Assert
    TEST_ASSERT_EQUAL_PTR(&dev, mgr->getDevice(slot));
    TEST_ASSERT_EQUAL(1, mgr->getCount());
}

// Test runner — called from main()
int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_register_and_get_device);
    RUN_TEST(test_find_by_compatible);
    // ...
    return UNITY_END();
}
```

**For Arduino-compatible tests (e.g. `test_wifi`):**
```cpp
int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_save_single_network);
    // ...
    return UNITY_END();
}

#ifdef NATIVE_TEST
int main(void) { return runUnityTests(); }
#endif

#ifndef NATIVE_TEST
void setup() { delay(2000); runUnityTests(); }
void loop()  {}
#endif
```

### Mocking

**Mock headers** live in `test/test_mocks/` and are header-only (no `.cpp` — included directly).

**ArduinoMock namespace** provides controllable state:
```cpp
// Advance simulated time
ArduinoMock::mockMillis += 15000;  // simulate 15 second elapsed

// Simulate GPIO readings
ArduinoMock::mockAnalogValue = 512;
ArduinoMock::mockDigitalPins[5] = HIGH;

// Check LEDC calls
TEST_ASSERT_EQUAL(1, ArduinoMock::ledcWriteToneCount);
TEST_ASSERT_EQUAL_FLOAT(440.0, ArduinoMock::ledcLastFreq);

// Reset all mock state in setUp()
ArduinoMock::reset();
ArduinoMock::resetLedc();
```

**WiFi mock** — controllable connection state:
```cpp
WiFiClass::addMockNetwork("NetworkA", -50);   // Add scan result
WiFiClass::reset();                            // Reset in setUp()
WiFi.begin("MySSID", "pass");
TEST_ASSERT_EQUAL(WiFiClass::WL_CONNECTED, WiFi.status());
```

**Preferences mock** — in-memory namespace storage (`std::map<string, map<string,string>>`):
```cpp
Preferences prefs;
prefs.begin("wifi-list", false);
prefs.putString("s0", "TestSSID");
prefs.end();
// ... reset between tests
Preferences::reset();
```

**Native-test guards for hardware calls inside source files:**
```cpp
#ifndef NATIVE_TEST
#include <Arduino.h>
#include "../debug_serial.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#ifndef OUTPUT
#define OUTPUT 1
static void pinMode(int, int) {}
static void digitalWrite(int, int) {}
#endif
#endif
```

**What to mock:**
- All Arduino hardware functions (`millis()`, `analogRead()`, `digitalWrite()`, LEDC, I2C, I2S)
- WiFi, MQTT (PubSubClient), NVS (Preferences), LittleFS
- ESP-specific: `esp_random.h`, `esp_timer.h`

**What NOT to mock:**
- `ArduinoJson` (used directly — full library included)
- Math functions (`cmath`, `cstring`, `stdlib.h`)
- The module under test itself

### Test Device Pattern (HAL tests)

HAL tests create minimal `TestDevice` subclasses that track call counts and return configurable results:
```cpp
class TestDevice : public HalDevice {
public:
    bool probeResult = true;
    bool initResult  = true;
    int  initCallCount = 0;

    TestDevice(const char* compatible, HalDeviceType type,
               uint16_t priority = HAL_PRIORITY_HARDWARE) {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.type = type;
        _initPriority = priority;
    }

    bool probe() override { return probeResult; }
    HalInitResult init() override {
        initCallCount++;
        return initResult ? hal_init_ok() : hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return true; }
};
```

### DSP Test Pattern (inline C source files)

DSP tests include the esp_dsp_lite ANSI C fallback sources directly:
```cpp
#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_f32_ansi.c"
#include "../../src/dsp_biquad_gen.c"
#include "../../src/dsp_pipeline.h"
#include "../../src/dsp_pipeline.cpp"
```

### Coverage

**Requirements:** No numeric coverage threshold enforced by tooling, but:
- Every new module must have a test file in `test/test_<module>/`
- Changed function signatures must update affected tests
- Every HAL driver has a corresponding `test_hal_<device>` module

**View test output:**
```bash
pio test -e native -v   # Shows PASS/FAIL per test with file:line on failure
```

---

## Browser / E2E Tests (Playwright)

### Runner

- **Framework:** Playwright v1.x (Chromium only)
- **Config:** `e2e/playwright.config.js`
- **Base URL:** `http://localhost:3000` (mock Express server)
- **Timeout per test:** 30 seconds
- **CI workers:** 1 (sequential); local: parallel (default)
- **CI retries:** 1; local: 0

**Run Commands:**
```bash
cd e2e
npm install                                        # First time
npx playwright install --with-deps chromium        # First time

npx playwright test                                # Run all 26 tests
npx playwright test tests/auth.spec.js             # Single spec
npx playwright test --headed                       # Visible browser
npx playwright test --debug                        # Inspector mode
```

### Test Infrastructure

The mock server at `e2e/mock-server/server.js` (Express, port 3000) replaces the real firmware:
- Serves the real frontend HTML assembled from `web_src/` via `assembler.js`
- Provides 12 route files matching the firmware REST API exactly (`e2e/mock-server/routes/*.js`)
- `GET /api/__test__/reset` endpoint resets mock server state between tests
- WebSocket connections to `*:81` are intercepted at browser level by Playwright's `routeWebSocket()` — no real WS server

### connectedPage Fixture

Most tests use the `connectedPage` custom fixture from `e2e/helpers/fixtures.js`:

```javascript
const { test, expect } = require('../helpers/fixtures');

test('my test', async ({ connectedPage: page }) => {
    // page already has: session cookie, WS intercepted + auth completed,
    // all initial state fixtures broadcast, status shows "Connected"

    // Inject additional WS messages
    page.wsRoute.send({ type: 'hardwareStats', cpuPercent: 45 });

    // Test against DOM
    await expect(page.locator('#cpuTotal')).toContainText('45');
});
```

The fixture performs:
1. POST `/api/auth/login` to acquire a real session cookie from mock server
2. Sets the session cookie in the browser context
3. Installs `routeWebSocket(/.*:81/, handler)` — intercepts all WS connections
4. Navigates to `/`
5. Handles the WS auth handshake (`authRequired` → `auth` → `authSuccess`)
6. Broadcasts all initial state from `buildInitialState()` (10 fixture files)
7. Waits for `#wsConnectionStatus` to show "Connected"

### WS Message Interception Pattern

```javascript
await page.routeWebSocket(/.*:81/, (ws) => {
    ws.onMessage((msg) => {         // Capital M — messages from browser → mock
        const data = JSON.parse(msg);
        if (data.type === 'auth') {
            ws.send(JSON.stringify({ type: 'authSuccess' }));
            // send initial state...
        }
    });
    ws.onClose(() => {});           // Capital C
    ws.send(JSON.stringify({ type: 'authRequired' }));
});
```

### Test Structure (E2E)

```javascript
/**
 * Module description and key constraints as JSDoc block.
 */
const { test, expect } = require('../helpers/fixtures');

test('descriptive test name', async ({ connectedPage: page }) => {
    // Navigate to tab
    await page.locator('.sidebar-item[data-tab="settings"]').click();
    // OR use JS call to avoid scroll issues:
    await page.evaluate((tabId) => switchTab(tabId), 'debug');

    // Assert DOM state from WS fixture
    await expect(page.locator('#buzzerToggle')).toBeChecked({ timeout: 3000 });

    // Intercept REST call
    let apiCalled = false;
    await page.route('/api/hal/scan', async (route) => {
        apiCalled = true;
        await route.fulfill({ status: 200, body: JSON.stringify({ status: 'ok' }) });
    });
    await page.locator('#hal-rescan-btn').click();
    expect(apiCalled).toBe(true);
});
```

### Fixtures

**WS fixtures** (`e2e/fixtures/ws-messages/*.json`) — 15 files representing server broadcast messages:
- `wifi-status.json`, `smart-sensing.json`, `display-state.json`, `buzzer-state.json`
- `mqtt-settings.json`, `hal-device-state.json`, `audio-channel-map.json`
- `audio-graph-state.json`, `signal-generator.json`, `debug-state.json`
- `auth-required.json`, `auth-success.json`, `audio-levels.json`, `debug-log.json`, `hardware-stats.json`

**API fixtures** (`e2e/fixtures/api-responses/*.json`) — 14 files for REST responses:
- `hal-devices.json`, `hal-presets.json`, `wifi-list.json`, `wifi-scan.json`, `wifi-status.json`
- `settings.json`, `mqtt-config.json`, `ota/check-update.json`, `releases.json`
- `signal-generator.json`, `smart-sensing.json`, `pipeline-matrix.json`, `auth-status.json`, `diagnostics.json`

**Loading fixtures:**
```javascript
// ws-helpers.js pattern
function loadFixture(name) {
    return JSON.parse(fs.readFileSync(path.join(FIXTURE_DIR, `${name}.json`), 'utf8'));
}
```

### Selectors

All reusable selectors are in `e2e/helpers/selectors.js` — keyed by semantic name:
```javascript
const SELECTORS = {
    sidebarTab:     (name) => `.sidebar-item[data-tab="${name}"]`,
    panel:          (name) => `#${name}`,
    wsConnectionStatus: '#wsConnectionStatus',
    halDeviceList:  '#hal-device-list',
    buzzerToggle:   '#buzzerToggle',
    // ...
};
```

When element IDs change in `web_src/index.html`, update `e2e/helpers/selectors.js` and all affected specs.

### Key Playwright Patterns

**Tab navigation** — use `page.evaluate()` to avoid scroll issues with sidebar clicks:
```javascript
await page.evaluate((tab) => switchTab(tab), 'debug');
```

**CSS-hidden toggles** — use `toBeChecked()`, not `toBeVisible()`:
```javascript
// Toggle inputs are opacity:0 inside label.switch — never use toBeVisible()
await expect(page.locator('#buzzerToggle')).toBeChecked();
await expect(page.locator('#darkModeToggle')).not.toBeChecked();
```

**Strict mode disambiguation** — use `.first()` when selector matches multiple elements:
```javascript
const disableBtn = deviceList.locator('button').filter({ hasText: /Disable/i }).first();
```

**Binary WS frames** — use `buildWaveformFrame()` / `buildSpectrumFrame()` helpers:
```javascript
const { buildWaveformFrame, buildSpectrumFrame } = require('../helpers/ws-helpers');
page.wsRoute.sendBinary(buildWaveformFrame(0, Array(256).fill(128)));
page.wsRoute.sendBinary(buildSpectrumFrame(0, 1000, Array(16).fill(0.1)));
```

**Graceful optional elements** — skip instead of fail when a feature may not be rendered:
```javascript
const count = await dspBtn.count();
if (count === 0) { return; }  // feature not available in this fixture state
```

---

## Static Analysis Tools

**Pre-commit hooks** (`.githooks/pre-commit` — activate with `git config core.hooksPath .githooks`):
1. `node tools/find_dups.js` — detect duplicate JS `let`/`const` across concatenated files
2. `node tools/check_missing_fns.js` — detect undefined function references
3. ESLint on `web_src/js/` — `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`

**cppcheck** (CI only — `cpp-lint` job):
```bash
cppcheck --enable=warning,performance \
         --suppress=missingInclude \
         --suppress=unusedFunction \
         --suppress=badBitmaskCheck \
         --std=c++11 --error-exitcode=1 \
         -i src/gui/ src/
```

---

## CI Quality Gates

All 4 gates run in parallel; the `build` job has `needs: [cpp-tests, cpp-lint, js-lint, e2e-tests]`. Firmware binary is only produced after all 4 pass.

| Gate | Job name | What it runs |
|------|----------|-------------|
| C++ unit tests | `cpp-tests` | `pio test -e native -v` |
| C++ static analysis | `cpp-lint` | cppcheck on `src/` |
| JS static analysis | `js-lint` | find_dups + check_missing_fns + ESLint |
| Browser E2E | `e2e-tests` | `npx playwright test` (Chromium) |

**CI config:** `.github/workflows/tests.yml` — triggers on push/PR to `main` and `develop` branches.

**Playwright report on failure:** uploaded as artifact `playwright-report-<sha>`, retained 14 days.

---

## Mandatory Test Coverage Rules

1. **C++ firmware changes** (`src/`):
   - New module → create `test/test_<module>/test_<module>.cpp`
   - Changed function signature → update all affected test files
   - New HAL driver → create `test/test_hal_<device>/test_hal_<device>.cpp`

2. **Web UI changes** (`web_src/`):
   - New toggle/button → add E2E test verifying it sends the correct WS command
   - New WS broadcast type → add fixture JSON + test verifying DOM updates
   - New top-level JS declaration → add to `web_src/.eslintrc.json` globals
   - Changed element IDs → update `e2e/helpers/selectors.js` + affected specs

3. **WebSocket protocol changes** (`src/websocket_handler.cpp`):
   - Update `e2e/fixtures/ws-messages/` with new/changed fixture JSON
   - Update `e2e/helpers/ws-helpers.js` `buildInitialState()` and `handleCommand()`
   - Update `e2e/mock-server/ws-state.js` for new state fields

4. **REST API changes** (`src/main.cpp`, `src/hal/hal_api.cpp`):
   - Update matching route in `e2e/mock-server/routes/*.js`
   - Update `e2e/fixtures/api-responses/` with new/changed response fixture

---

*Testing analysis: 2026-03-09*
