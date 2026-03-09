# Testing Infrastructure

**Analysis Date:** 2026-03-09

---

## C++ Unit Tests (Unity + Native Platform)

### Overview

- **Framework**: Unity (via PlatformIO `test_framework = unity`)
- **Platform**: `native` — runs on host machine with MinGW/gcc, no hardware required
- **Count**: 1,630 test functions across 70 test modules (counted by `grep "^void test_"` across all `test/test_*/`)
- **CLAUDE.md reported count**: 1,614 tests (slight discrepancy may reflect additions since last update)
- **Config**: `[env:native]` in `platformio.ini`

### Run Commands

```bash
pio test -e native            # Run all test modules
pio test -e native -v         # Verbose output (shows each test name + PASS/FAIL)
pio test -e native -f test_wifi       # Run a single module
pio test -e native -f test_hal_core   # Run HAL core tests
pio test -e native -f test_mqtt       # Run MQTT tests
```

### Environment Configuration (`platformio.ini` `[env:native]`)

```ini
platform = native
test_framework = unity
build_flags =
    -std=c++11
    -D UNIT_TEST
    -D NATIVE_TEST
    -D DSP_ENABLED
    -D DSP_MAX_STAGES=24
    -D DSP_PEQ_BANDS=10
    -D DSP_MAX_FIR_TAPS=256
    -D DAC_ENABLED
    -D USB_AUDIO_ENABLED
lib_deps =
    bblanchon/ArduinoJson@^7.4.2
    kosme/arduinoFFT@^2.0
lib_compat_mode = off
lib_ignore = WebSockets
test_ignore = test_mocks        # test_mocks/ is not a test module
test_build_src = no             # CRITICAL: src/ is NOT compiled; tests inline .cpp files directly
```

The `test_build_src = no` setting means tests cannot link against `src/` object files. Each test file directly `#include`s the `.cpp` implementation files it needs.

### Test File Layout

```
test/
├── test_mocks/              # Mock headers — excluded from test runs (test_ignore = test_mocks)
├── test_<module>/
│   └── test_<module>.cpp   # Exactly ONE .cpp per directory
└── (hardware-only dirs)
    ├── idf4_pcm1808_test/
    ├── idf5_dac_test/
    ├── idf5_pcm1808_test/
    └── p4_hosted_update/
```

**One test file per directory** is mandatory. Multiple `.cpp` files in one directory cause duplicate `main()`/`setUp()`/`tearDown()` link errors.

### Test Module List (70 modules)

```
test_api                test_audio_diagnostics    test_audio_health_bridge
test_audio_pipeline     test_auth                 test_button
test_buzzer             test_crash_log            test_dac_eeprom
test_dac_hal            test_dac_settings         test_debug_mode
test_deferred_toggle    test_diag_journal         test_dim_timeout
test_dsp                test_dsp_presets          test_dsp_rew
test_dsp_swap           test_es8311               test_esp_dsp
test_eth_manager        test_evt_any              test_fft
test_gui_home           test_gui_input            test_gui_navigation
test_hal_adapter        test_hal_bridge           test_hal_button
test_hal_buzzer         test_hal_core             test_hal_custom_device
test_hal_discovery      test_hal_dsp_bridge       test_hal_eeprom_v3
test_hal_encoder        test_hal_es8311           test_hal_integration
test_hal_mcp4725        test_hal_multi_instance   test_hal_ns4150b
test_hal_pcm1808        test_hal_pcm5102a         test_hal_retry
test_hal_siggen         test_hal_state_callback   test_hal_usb_audio
test_hal_wire_mock      test_i2s_audio            test_mqtt
test_ota                test_ota_task             test_output_dsp
test_peq                test_pinout               test_pipeline_bounds
test_pipeline_output    test_settings             test_signal_generator
test_sink_slot_api      test_smart_sensing        test_task_monitor
test_usb_audio          test_utils                test_vrms
test_websocket          test_websocket_messages   test_wifi
```

### Test Structure Pattern

Every test file follows Arrange-Act-Assert with Unity assertions:

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

// Inline implementation .cpp files directly (test_build_src = no)
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// ===== Test Fixtures =====
static HalDeviceManager* mgr;

void setUp() {
    // Reset all mock state before every test
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
}

void tearDown() {}   // Must be present; typically empty

// ===== One concern per test function =====
void test_register_and_get_device() {
    // Arrange
    TestDevice dev("ti,pcm5102a", HAL_DEV_DAC);

    // Act
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Assert
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);
    TEST_ASSERT_EQUAL_PTR(&dev, mgr->getDevice(slot));
    TEST_ASSERT_EQUAL(1, mgr->getCount());
    TEST_ASSERT_EQUAL(HAL_DISC_BUILTIN, dev.getDiscovery());
}

// ===== Test Runner =====
int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_register_and_get_device);
    RUN_TEST(test_find_by_compatible);
    // ... all test functions listed here
    return UNITY_END();
}
```

**Structural rules:**
- `setUp()` resets all module state before every test — mocks expose `::reset()` static methods
- `tearDown()` is typically empty but must be declared
- Test functions named `test_<what_is_being_verified>`
- Every test function registered with `RUN_TEST()` in `main()`
- Section dividers use `// ===== Section Name =====` style

### Unity Assertion Reference

```cpp
TEST_ASSERT_TRUE(condition)
TEST_ASSERT_FALSE(condition)
TEST_ASSERT_EQUAL(expected, actual)
TEST_ASSERT_NOT_EQUAL(a, b)
TEST_ASSERT_EQUAL_PTR(expected, actual)
TEST_ASSERT_NULL(ptr)
TEST_ASSERT_NOT_NULL(ptr)
TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual)
TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(threshold, actual, "failure message")
TEST_ASSERT_EQUAL_STRING(expected, actual)
TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)
```

### Mock System (`test/test_mocks/`)

Mock headers implement the same API as real Arduino/ESP32 libraries using standard C++ (`std::map`, `std::string`, `std::vector`). They are `#include`d directly under `#ifdef NATIVE_TEST`.

**Available mock headers:**

| File | Simulates |
|---|---|
| `Arduino.h` | `millis()`, `analogRead()`, `digitalWrite()`, `pinMode()`, `delay()`, `ESP` class, LEDC functions, ISR attach/detach, `ps_calloc()`, `random()` |
| `WiFi.h` | `WiFiClass` — scan, connect, AP mode, static IP, RSSI, MAC. `addMockNetwork()` injects scan results. |
| `PubSubClient.h` | MQTT connect/publish/subscribe; `publishedMessages` map for assertion |
| `Preferences.h` | NVS backed by `std::map<string,map<string,string>>`; namespaced get/put for all supported types |
| `LittleFS.h` | In-memory filesystem backed by `std::map<string,string>`; read/write/append/rename/remove; `injectFile()` and `getFile()` test helpers; `_mountShouldFail` flag |
| `i2s_std_mock.h` | IDF5 `<driver/i2s_std.h>` type stubs (handles, error codes, enums) |
| `Wire.h` | I2C bus mock |
| `esp_random.h` | `esp_random()`, `esp_fill_random()` |
| `esp_timer.h` | `esp_timer_get_time()` returning `mockMicros * 1000` |
| `mbedtls/md.h` | mbedTLS HMAC/hash stubs |
| `mbedtls/pkcs5.h` | PBKDF2 stubs (for auth handler tests) |

**Mock state reset pattern** — `setUp()` resets all mocks used by the module under test:

```cpp
void setUp() {
    ArduinoMock::reset();          // millis=0, digitalPins cleared, LEDC counters zeroed
    WiFiClass::reset();            // scan results cleared, disconnected state
    PubSubClient::reset();         // published messages and subscriptions cleared
    MockFS::reset();               // LittleFS files cleared, unmounted
    Preferences::storage.clear(); // NVS namespaces cleared
}
```

**Arduino mock controllable state:**

```cpp
// Control time
ArduinoMock::mockMillis = 5000;

// Control analog read result
ArduinoMock::mockAnalogValue = 2048;

// Control digital pin state
ArduinoMock::mockDigitalPins[32] = HIGH;

// Verify LEDC was called
TEST_ASSERT_EQUAL(1, ArduinoMock::ledcWriteToneCount);
TEST_ASSERT_EQUAL(1000.0, ArduinoMock::ledcLastFreq);
```

**WiFi mock helpers:**

```cpp
WiFiClass::addMockNetwork("MySSID", -65, 6, true);  // ssid, rssi, channel, encrypted
WiFiClass::lastStatusCode = WiFiClass::WL_CONNECTED;
WiFiClass::connectedSSID = "MySSID";
```

**MQTT mock assertions:**

```cpp
TEST_ASSERT_TRUE(PubSubClient::wasMessagePublished("alx/status"));
TEST_ASSERT_EQUAL_STRING("online", PubSubClient::getPublishedMessage("alx/status").c_str());
TEST_ASSERT_TRUE(PubSubClient::wasTopicSubscribed("alx/cmd/#"));
```

**LittleFS mock helpers:**

```cpp
MockFS::injectFile("/config.json", R"({"version":1,"settings":{}})");
std::string content = MockFS::getFile("/config.json");
MockFS::_mountShouldFail = true;    // Test mount failure path
```

### Inline .cpp Testing Pattern for HAL Drivers

HAL driver tests define a `TestDevice` stub that overrides lifecycle methods with controllable state:

```cpp
class TestDevice : public HalDevice {
public:
    bool probeResult = true;
    bool initResult = true;
    bool healthResult = true;
    int  initCallCount = 0;
    int  probeCallCount = 0;
    int  healthCallCount = 0;
    int  deinitCallCount = 0;

    bool probe() override { probeCallCount++; return probeResult; }
    HalInitResult init() override {
        initCallCount++;
        return initResult ? hal_init_ok() : hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override { deinitCallCount++; }
    void dumpConfig() override {}
    bool healthCheck() override { healthCallCount++; return healthResult; }
};
```

### Coverage

No minimum coverage percentage is enforced by tooling. All 70 production modules have corresponding test modules. The CI `cpp-tests` gate validates by running all tests — any failure blocks the firmware `build` job.

---

## Browser E2E Tests (Playwright)

### Overview

- **Framework**: Playwright `@playwright/test@^1.50.0`
- **Browser**: Chromium only (single project in `e2e/playwright.config.js`)
- **Count**: 26 tests across 19 spec files in `e2e/tests/`
- **No hardware required** — Express mock server + Playwright `routeWebSocket()` interception at browser level

### Run Commands

```bash
cd e2e
npm install                                          # First time only
npx playwright install --with-deps chromium          # First time only

npx playwright test                                  # Run all 26 tests
npx playwright test tests/auth.spec.js               # Run single spec
npx playwright test --headed                         # Run with visible browser window
npx playwright test --debug                          # Debug mode with Playwright inspector
npx playwright test --ui                             # Interactive UI mode
```

### Playwright Config (`e2e/playwright.config.js`)

```js
{
  testDir: './tests',
  fullyParallel: true,
  forbidOnly: !!process.env.CI,      // Fail on test.only() in CI
  retries: process.env.CI ? 1 : 0,  // 1 retry in CI only
  workers: process.env.CI ? 1 : undefined,  // Single worker in CI
  timeout: 30000,
  use: {
    baseURL: 'http://localhost:3000',
    trace: 'on-first-retry',
    screenshot: 'only-on-failure',
  },
  webServer: {
    command: 'node mock-server/server.js',
    url: 'http://localhost:3000',
    reuseExistingServer: !process.env.CI,  // Fresh server each CI run
    timeout: 10000,
  }
}
```

### Spec Files (19 files)

```
e2e/tests/
├── audio-inputs.spec.js      # Audio tab sub-nav, input channel strips from audioChannelMap WS
├── audio-matrix.spec.js      # 16x16 routing matrix UI
├── audio-outputs.spec.js     # Output channel strips
├── audio-siggen.spec.js      # Signal generator enable/waveform/frequency controls
├── auth-password.spec.js     # Password change modal flow
├── auth.spec.js              # Login page render, correct password redirect, invalid session
├── control-tab.spec.js       # Smart sensing state, amp on/off, manual override
├── dark-mode.spec.js         # Theme toggle, persistence
├── debug-console.spec.js     # Log entries, module filter chips, search highlight
├── hal-devices.spec.js       # HAL device cards render, rescan, enable/disable
├── hardware-stats.spec.js    # CPU/memory/PSRAM stats display from WS broadcast
├── mqtt.spec.js              # MQTT settings form, enable/disable toggle
├── navigation.spec.js        # Sidebar tab switching, panel activation
├── ota.spec.js               # Firmware version display, check-for-update flow
├── peq-overlay.spec.js       # PEQ/crossover/compressor/limiter overlays
├── responsive.spec.js        # Mobile/desktop responsive breakpoints
├── settings.spec.js          # Dark mode, backlight, buzzer, screen timeout controls
├── support.spec.js           # Support tab, manual link, QR code element
└── wifi.spec.js              # WiFi tab, network select, AP toggle
```

### Mock Server Architecture (`e2e/mock-server/`)

Express server (port 3000) serves the real frontend HTML assembled from `web_src/` by `assembler.js` and provides mock REST API endpoints matching the firmware's REST API.

```
e2e/mock-server/
├── server.js        # Express app, route mounting, POST /api/__test__/reset
├── assembler.js     # Replicates tools/build_web_assets.js HTML assembly from web_src/
├── ws-state.js      # Deterministic mock state singleton; resetState() for inter-test isolation
└── routes/
    ├── auth.js      # POST /api/auth/login, GET /api/auth/status
    ├── hal.js       # /api/hal/devices, /api/hal/scan, /api/hal/db/presets
    ├── wifi.js      # WiFi list, scan, config, AP mode
    ├── mqtt.js      # MQTT config save/test
    ├── settings.js  # GET/POST /api/settings, export, import, factory reset
    ├── ota.js       # /api/ota/check-update, releases list
    ├── pipeline.js  # /api/pipeline/matrix
    ├── dsp.js       # DSP config endpoints
    ├── sensing.js   # Smart sensing config
    ├── siggen.js    # Signal generator
    ├── diagnostics.js  # Diagnostic journal
    └── system.js    # /api/system/reboot
```

**WS token endpoint** — mock server provides `/api/ws-token` (returns `{ success: true, token: "ws-token-<timestamp>" }` for any authenticated session):

```js
app.get('/api/ws-token', (req, res) => {
    const cookieId = req.cookies && req.cookies['sessionId'];
    if (!cookieId) return res.status(401).json({ success: false, error: 'Unauthorized' });
    res.json({ success: true, token: `ws-token-${Date.now()}` });
});
```

**Test reset endpoint** — resets server-side mock state between test suites:

```js
app.post('/api/__test__/reset', (req, res) => {
    resetState();
    res.json({ success: true });
});
```

### connectedPage Fixture (`e2e/helpers/fixtures.js`)

The primary test fixture — provides a fully connected browser page. Import it instead of `@playwright/test`:

```js
const { test, expect } = require('../helpers/fixtures');

test('my test', async ({ connectedPage: page }) => {
    // page: valid session cookie + intercepted WS + initial state broadcast + "Connected" status
    // page.wsRoute.send(obj)          — inject additional WS message (JSON serialized)
    // page.wsRoute.sendBinary(buf)    — inject binary frame (Buffer)
});
```

**What the fixture does in order:**
1. `POST /api/auth/login` with `testpass` → obtains `sessionId` cookie
2. Sets cookie on the browser context before navigation
3. `page.routeWebSocket(/.*:81/)` — intercepts ALL connections to port 81
4. On connection: server-side sends `{ type: 'authRequired' }` immediately
5. Frontend fetches `/api/ws-token`, sends `{ type: 'auth', token: '...' }`
6. Fixture responds `{ type: 'authSuccess' }` then broadcasts all 10 initial-state messages
7. Navigates to `/`, waits until `#wsConnectionStatus` shows "Connected"

Tests that don't need WS (e.g. login page tests) use `require('@playwright/test')` directly without this fixture.

### WS Helper Functions (`e2e/helpers/ws-helpers.js`)

```js
buildInitialState()
// Returns ordered array of 10 WS fixture messages broadcast after authSuccess:
// wifi-status, smart-sensing, display-state, buzzer-state, mqtt-settings,
// hal-device-state, audio-channel-map, audio-graph-state, signal-generator, debug-state

handleCommand(type, data)
// Routes inbound frontend WS commands, returns array of response objects.
// Handles: subscribeAudio, setDebugHwStats, setDebugMode, manualOverride, eepromScan, etc.

buildWaveformFrame(adc, samples)
// Binary 0x01 frame: [type:u8][adc:u8][256 samples:u8] = 258 bytes total

buildSpectrumFrame(adc, freq, bands)
// Binary 0x02 frame: [type:u8][adc:u8][dominantFreq:f32LE][16 bands:f32LE] = 70 bytes total
```

### WS Fixtures (`e2e/fixtures/ws-messages/`)

17 hand-crafted JSON files — server-to-browser WS broadcast messages:

```
audio-channel-map.json   audio-graph-state.json   audio-levels.json
auth-required.json       auth-success.json         buzzer-state.json
debug-log.json           debug-state.json          display-state.json
hal-device-state.json    hardware-stats.json       mqtt-settings.json
signal-generator.json    smart-sensing.json        wifi-status.json
```

### API Response Fixtures (`e2e/fixtures/api-responses/`)

14 JSON files for deterministic REST API responses:

```
auth-status.json   check-update.json    diagnostics.json     hal-devices.json
hal-presets.json   mqtt-config.json     pipeline-matrix.json releases.json
settings.json      signal-generator.json  smart-sensing.json wifi-list.json
wifi-scan.json     wifi-status.json
```

### Selectors (`e2e/helpers/selectors.js`)

Centralized DOM selectors keyed to `web_src/index.html` element IDs. Use `SELECTORS.*` rather than inline strings to avoid drift when HTML changes.

Key selectors:

```js
SELECTORS.wsConnectionStatus         // '#wsConnectionStatus'
SELECTORS.halDeviceList              // '#hal-device-list'
SELECTORS.audioSubNavBtn(view)       // `.audio-subnav-btn[data-view="${view}"]`
SELECTORS.sidebarTab(tabName)        // `.sidebar-item[data-tab="${tabName}"]`
SELECTORS.mqttEnabledToggle          // '#appState\\.mqttEnabled'
SELECTORS.panel(tabName)             // `#${tabName}`
SELECTORS.siggenEnable               // '#siggenEnable'
```

### Key Playwright Patterns

**Tab navigation** — use `page.evaluate` to avoid scroll/visibility issues with sidebar clicks in narrow viewports:

```js
await page.evaluate(() => switchTab('tabName'));
// or direct click when viewport is wide enough:
await page.locator('.sidebar-item[data-tab="audio"]').click();
```

**CSS-hidden checkbox toggles** — use `toBeChecked()` not `toBeVisible()` for inputs styled with `label.switch`:

```js
await expect(page.locator('#darkModeToggle')).toBeChecked();
await expect(page.locator('#darkModeToggle')).not.toBeChecked();
```

**Injecting WS messages mid-test:**

```js
page.wsRoute.send({ type: 'smartSensing', ampOn: true, signalDetected: true, audioLevel: -20.0 });
page.wsRoute.send({ type: 'hardwareStats', cpuPercent: 45, heapFree: 180000 });
page.wsRoute.sendBinary(buildWaveformFrame(0, new Array(256).fill(128)));
```

**Strict mode — disambiguate multi-match selectors:**

```js
await deviceList.locator('.hal-device-header').first().click();
const btn = deviceList.locator('.hal-device-card.expanded button').filter({ hasText: /Disable/i }).first();
```

**REST API interception:**

```js
let scanCalled = false;
await page.route('/api/hal/scan', async (route) => {
    scanCalled = true;
    await route.fulfill({ status: 200, body: JSON.stringify({ status: 'ok', devicesFound: 6 }) });
});
await page.locator('#hal-rescan-btn').click();
await page.waitForTimeout(500);
expect(scanCalled).toBe(true);
```

**WS event handler spelling** — Playwright uses capital M/C for WS events:

```js
ws.onMessage((msg) => { /* ... */ })  // NOT onmessage
ws.onClose(() => {})                  // NOT onclose
```

---

## CI Quality Gates (4 Parallel Jobs)

Defined in `.github/workflows/tests.yml`. All 4 jobs must pass before the `build` job runs (`needs: [cpp-tests, cpp-lint, js-lint, e2e-tests]`).

**Trigger**: push or pull_request to `main` or `develop` branches.

### Gate 1: cpp-tests

```bash
pio test -e native -v
```

Runs all 1,630 C++ Unity tests on ubuntu-latest. Uses PlatformIO cache keyed on `platformio.ini` hash. Python 3.11 + latest PlatformIO.

### Gate 2: cpp-lint

```bash
cppcheck \
  --enable=warning,performance \
  --suppress=missingInclude \
  --suppress=unusedFunction \
  --suppress=badBitmaskCheck \
  --std=c++11 \
  --error-exitcode=1 \
  -i src/gui/ \
  src/
```

`src/gui/` is excluded (LVGL generates many false positives). Runs on ubuntu-latest with system cppcheck.

### Gate 3: js-lint

Three sequential Node.js checks on ubuntu-latest (Node 20, `npm ci` from `e2e/package-lock.json`):

```bash
node tools/find_dups.js
node tools/check_missing_fns.js
cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json
```

### Gate 4: e2e-tests

```bash
cd e2e && npx playwright install --with-deps chromium
cd e2e && npx playwright test
```

CI uses `workers: 1` and 1 retry on failure. On failure, HTML report is uploaded as artifact `playwright-report-<sha>` (14-day retention).

### Build Job (Post-Gates)

```bash
pio run -e esp32-p4
```

Produces `firmware.bin`, uploaded as artifact `firmware-<sha>` (30-day retention).

---

## Pre-Commit Hook

Location: `.githooks/pre-commit`. Activate: `git config core.hooksPath .githooks`.

Runs 3 fast JS checks before every commit (`set -e`):

```bash
node tools/find_dups.js
node tools/check_missing_fns.js
cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json
```

C++ Unity tests and Playwright E2E tests are NOT run pre-commit (too slow). They run only in CI.

---

## Mandatory Coverage Rules

Every code change must keep tests green. Required actions by change type:

| Change | Required Action |
|---|---|
| New C++ module in `src/` | Create `test/test_<module>/test_<module>.cpp` with setUp, tearDown, test functions, main() |
| Changed C++ function signature | Update all affected test modules |
| New WS message type | Add fixture JSON to `e2e/fixtures/ws-messages/`, update `buildInitialState()` and `handleCommand()` in `e2e/helpers/ws-helpers.js`, add Playwright test |
| New REST endpoint | Add route in `e2e/mock-server/routes/*.js`, add response fixture in `e2e/fixtures/api-responses/`, add Playwright test if UI depends on it |
| New HTML toggle/button/dropdown | Add Playwright test verifying it sends the correct WS command |
| New tab or section | Add navigation test + element presence test |
| Changed element ID in HTML | Update `e2e/helpers/selectors.js` + all affected specs |
| New top-level JS `let`/`const`/function | Add to `web_src/.eslintrc.json` globals |
| Removed feature | Remove corresponding tests + fixture JSON files |

---

## Hardware-Only Test Directories (Not in CI)

These directories require physical hardware and are excluded from the native environment:

```
test/idf4_pcm1808_test/    # PCM1808 I2S ADC validation on IDF4
test/idf5_dac_test/        # DAC hardware test on IDF5
test/idf5_pcm1808_test/    # PCM1808 hardware test on IDF5
test/p4_hosted_update/     # OTA firmware update smoke test (requires COM8 device)
```

---

*Testing analysis: 2026-03-09*
