# Testing Patterns

**Analysis Date:** 2026-03-25

## Test Infrastructure

**Three test tiers:**

| Tier | Framework | Count | Location | Hardware Required |
|------|-----------|-------|----------|-------------------|
| C++ Unit | Unity (PlatformIO) | 3701 tests / 125 modules | `test/test_*/` | No |
| E2E Browser | Playwright | 358 tests / 57 specs | `e2e/tests/` | No |
| On-Device | pytest | 206 tests / 21 modules | `device_tests/tests/` | Yes (ESP32-P4) |

**CI/CD quality gates (all must pass):**

| Gate | Job Name | What It Checks |
|------|----------|----------------|
| C++ tests | `cpp-tests` | `pio test -e native -v` — all 3701 Unity tests |
| C++ lint | `cpp-lint` | cppcheck on `src/` (excludes `src/gui/`) |
| JS lint | `js-lint` | `find_dups.js` + `check_missing_fns.js` + ESLint + diagram validation + web_pages guard |
| E2E tests | `e2e-tests` | Playwright (excludes `@visual` tag) |
| Doc coverage | `doc-coverage` | `check_mapping_coverage.js` — all source files mapped |
| Security | `security-check` | Blocks `TEST_MODE` in `platformio.ini` build flags |
| Build | `build` | `pio run -e esp32-p4` — only runs after all other gates pass |

CI config: `.github/workflows/tests.yml`

## Test Types & Locations

### C++ Unit Tests

**Location:** `test/test_<module>/test_<module>.cpp` — 128 test files across 125 module directories (excluding `test/test_mocks/`)

**Build configuration (in `platformio.ini` `[env:native]`):**
- Platform: native (gcc/MinGW, no hardware)
- Framework: Unity
- Build flags: `-D UNIT_TEST -D NATIVE_TEST -D DSP_ENABLED -D DAC_ENABLED -D USB_AUDIO_ENABLED`
- `test_build_src = no` — tests do not compile `src/` as a whole; they inline specific `.cpp` files
- `test_ignore = test_mocks` — mock directory excluded from test discovery

**Mock files:** `test/test_mocks/`
- `Arduino.h` — `millis()`, `micros()`, `delay()`, `Serial`, GPIO, ADC, `ArduinoMock::reset()`
- `WiFi.h` — WiFi connection state, scan results
- `Preferences.h` — NVS key-value storage mock
- `PubSubClient.h` — MQTT client mock
- `Wire.h` — I2C bus mock
- `LittleFS.h` — Filesystem mock
- `ETH.h` — Ethernet mock
- `IPAddress.h` — IP address type mock
- `esp_random.h` — Deterministic random values
- `esp_timer.h` — Microsecond timer mock
- `i2s_std_mock.h` — I2S driver mock
- `mbedtls/md.h` — SHA256 hashing mock
- `mbedtls/pkcs5.h` — PBKDF2 mock

**Test module categories:**

| Category | Example Modules | Count |
|----------|----------------|-------|
| HAL core | `test_hal_core`, `test_hal_discovery`, `test_hal_bridge`, `test_hal_coord` | ~15 |
| HAL drivers | `test_hal_es9038q2m`, `test_hal_pcm5102a`, `test_hal_cirrus_dac_2ch`, `test_hal_ess_dac_2ch` | ~30 |
| Audio pipeline | `test_audio_pipeline`, `test_pipeline_output`, `test_pipeline_bounds`, `test_pipeline_dma_guard` | ~8 |
| DSP | `test_dsp`, `test_dsp_presets`, `test_dsp_rew`, `test_dsp_swap`, `test_dsp_cpu_guard`, `test_peq` | ~7 |
| Network | `test_wifi`, `test_mqtt`, `test_eth_manager`, `test_eth_settings` | ~4 |
| WebSocket | `test_websocket`, `test_websocket_auth`, `test_websocket_messages`, `test_ws_adaptive_rate` | ~4 |
| Auth/Security | `test_auth`, `test_http_security`, `test_http_rate_limit` | ~3 |
| Settings | `test_settings`, `test_settings_export_v2`, `test_settings_transactional` | ~3 |
| Diagnostics | `test_audio_diagnostics`, `test_health_check`, `test_diag_journal`, `test_clock_diagnostics`, `test_device_deps` | ~5 |
| GUI | `test_gui_home`, `test_gui_input`, `test_gui_navigation` | ~3 |
| Other | `test_buzzer`, `test_button`, `test_ota`, `test_signal_generator`, `test_usb_audio`, etc. | ~25+ |

### E2E Browser Tests

**Location:** `e2e/tests/*.spec.js` — 56 spec files (57 including snapshot dirs)

**Infrastructure:**
- Playwright v1.50+ (`e2e/package.json`)
- Chromium only (single project in `playwright.config.js`)
- Mock Express server at `http://localhost:3000` (`e2e/mock-server/server.js`)
- WebSocket mocking via Playwright's `page.routeWebSocket()` — no real device needed
- Auto-starts mock server via Playwright `webServer` config

**E2E directory structure:**
```
e2e/
├── tests/                    # 56 spec files
│   ├── auth.spec.js          # Login, session, password
│   ├── hal-devices.spec.js   # HAL device list, cards
│   ├── clock-diagnostics.spec.js  # DPLL clock quality
│   ├── device-deps.spec.js   # Dependency graph UI
│   ├── visual-*.spec.js      # Screenshot comparisons
│   ├── a11y-*.spec.js        # Accessibility scans
│   └── ...
├── helpers/
│   ├── fixtures.js           # connectedPage fixture (auth + WS mock)
│   ├── ws-helpers.js         # buildInitialState(), handleCommand()
│   ├── ws-assertions.js      # expectWsCommand(), captureApiCall()
│   ├── selectors.js          # All CSS selectors as constants
│   ├── fixture-factories.js  # Programmatic fixture builders (buildHalDevice(), etc.)
│   └── a11y-helpers.js       # axe-core accessibility scan helpers
├── pages/                    # 19 Page Object Models
│   ├── BasePage.js           # switchTab(), wsSend(), expectWsCommand(), expectToast()
│   ├── DevicesPage.js
│   ├── AudioPage.js
│   ├── NetworkPage.js
│   └── ...
├── fixtures/
│   ├── ws-messages/          # 19 JSON fixture files for WS broadcasts
│   │   ├── wifi-status.json
│   │   ├── hal-device-state.json
│   │   ├── audio-channel-map.json
│   │   └── ...
│   └── api-responses/        # 19 JSON fixture files for REST responses
│       ├── hal-devices.json
│       ├── settings.json
│       └── ...
└── mock-server/
    ├── server.js             # Express app with API route mounting
    ├── assembler.js          # Assembles HTML from web_src/ for serving
    ├── ws-state.js           # Deterministic mock state singleton
    └── routes/               # 14 route modules (auth, hal, wifi, mqtt, etc.)
```

**E2E test tags:** `@smoke`, `@ws`, `@api`, `@hal`, `@audio`, `@settings`, `@error`, `@visual`, `@a11y`

**API versioning in E2E:** Mock server mounts all routes at both `/api/<path>` and `/api/v1/<path>`. Frontend `apiFetch()` auto-rewrites `/api/` to `/api/v1/`. E2E route patterns use `/api/v1/...`.

### On-Device Tests

**Location:** `device_tests/tests/test_*.py` — 22 test files across 21 modules

**Infrastructure:**
- pytest with pyserial + Python requests
- Connects to real ESP32-P4 hardware via serial + HTTP
- Config: `device_tests/pytest.ini`
- Utilities: `device_tests/utils/` (serial_reader, health_parser, issue_creator, ws_client)

**Markers:** `boot`, `reboot`, `slow`, `settings`, `mqtt`, `hal`, `health`, `audio`, `network`, `ws`, `wifi`, `ethernet`, `ota`, `dsp`, `sync`, `perf`, `stress`

**CI trigger:** `.github/workflows/device-tests.yml` — runs on self-hosted runner with `has-esp32p4` label. Triggered by `workflow_dispatch`, `[device-test]` in commit message, or `device-test` PR label.

## Running Tests

```bash
# ===== C++ Unit Tests =====
pio test -e native                    # Run all 3701 tests
pio test -e native -v                 # Verbose output
pio test -e native -f test_wifi       # Single module
pio test -e native -f test_hal_core   # HAL core tests
pio test -e native -f test_clock_diagnostics  # Clock diagnostics

# ===== E2E Browser Tests =====
cd e2e && npm install                              # First time
cd e2e && npx playwright install --with-deps chromium  # First time
cd e2e && npx playwright test                      # All tests (excludes @visual in CI)
cd e2e && npx playwright test tests/auth.spec.js   # Single spec
cd e2e && npx playwright test --grep @smoke        # By tag
cd e2e && npx playwright test --grep @hal          # HAL tests only
cd e2e && npx playwright test --headed             # Visible browser
cd e2e && npx playwright test --debug              # Debug inspector
cd e2e && npx playwright test --ui                 # Interactive UI mode

# ===== On-Device Tests =====
cd device_tests && pip install -r requirements.txt  # First time
cd device_tests && pytest tests/ --device-port COM8 --device-ip <IP> -v
cd device_tests && pytest tests/ -m "not reboot" -v          # Skip reboot tests
cd device_tests && pytest tests/test_boot_health.py -v       # Single module
cd device_tests && pytest tests/ --create-issues             # Auto-create GitHub issues on failure

# ===== Static Analysis =====
cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json
node tools/find_dups.js
node tools/check_missing_fns.js
node tools/check_mapping_coverage.js
node tools/diagram-validation.js
```

## Test Patterns & Conventions

### C++ Unit Test Pattern

Every C++ test file follows this structure:

```cpp
/**
 * test_<module>.cpp
 *
 * Unit tests for <feature description>:
 *   1. <Section 1 description>
 *   2. <Section 2 description>
 *   ...
 *
 * Runs on the native platform (no hardware needed).
 */

#include <unity.h>
#include <cstring>
#include <cstdint>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Include production headers
#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"

// For tests that need HAL manager: inline the .cpp files
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"

// ============================================================
// Section 1: Description
// ============================================================

void test_descriptive_name() {
    // Arrange
    SomeStruct s = {};

    // Act
    bool result = someFunction(s);

    // Assert
    TEST_ASSERT_TRUE(result);
}

// ============================================================
// Test Runner
// ============================================================

void setUp() {
    ArduinoMock::reset();  // Reset mock state before each test
    // Reset module-specific state (HAL manager, registries, etc.)
}

void tearDown() {}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_descriptive_name);
    // ... all tests registered here
    return UNITY_END();
}
```

**Key patterns:**
- `setUp()` resets `ArduinoMock::reset()` + module singletons (e.g., `mgr->reset()`, `hal_registry_reset()`)
- Section comments: `// ============================================================`
- Test functions: `void test_<specific_behavior>()` — descriptive names that read as documentation
- Inline `.cpp` inclusion: tests include production `.cpp` files directly (since `test_build_src = no`)
- Test devices: Create minimal subclasses of `HalDevice` with controllable `probe()`/`init()` results

**Test device pattern (HAL tests):**
```cpp
class TestDevice : public HalDevice {
public:
    bool probeResult = true;
    bool initResult = true;
    int  initCallCount = 0;

    TestDevice(const char* compatible, HalDeviceType type) {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = type;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    bool probe() override { return probeResult; }
    HalInitResult init() override {
        initCallCount++;
        if (initResult) return hal_init_ok();
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return true; }
};
```

### E2E Test Pattern

Tests use the custom `connectedPage` fixture which provides an authenticated browser page with mocked WebSocket:

```javascript
/**
 * <feature>.spec.js — <Feature description> E2E tests.
 *
 * Verifies:
 *   1. <What is verified>
 *   2. <What is verified>
 *
 * @tag1 @tag2
 */

const { test, expect } = require('../helpers/fixtures');

test.describe('@hal @smoke Feature Name', () => {

  test('descriptive test name', async ({ connectedPage: page }) => {
    // Navigate to tab
    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);

    // Push WS state
    page.wsRoute.send(FIXTURE_DATA);

    // Assert DOM state
    await expect(page.locator('#element')).toBeVisible();
    await expect(page.locator('.count')).toHaveCount(11);
  });

});
```

**Key patterns:**
- Import `test` and `expect` from `../helpers/fixtures` (NOT directly from `@playwright/test`)
- Destructure `connectedPage` as `page` in test args
- Tab navigation: `page.evaluate(() => switchTab('tabName'))` — never click sidebar
- WS messages: `page.wsRoute.send(jsonObject)` to push state to frontend
- WS capture: `page.wsCapture` array captures frontend-to-server messages
- Use `page.waitForTimeout(200-300)` after tab switch for DOM settle
- CSS-hidden checkboxes: assert with `toBeChecked()` not `toBeVisible()`
- Load fixtures from `e2e/fixtures/ws-messages/` via `fs.readFileSync()`

**Page Object Model pattern:**
```javascript
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class DevicesPage extends BasePage {
  async open() {
    await this.switchTab('devices');
    this.wsSend(halDeviceFixture);
    await this.page.locator(SELECTORS.halDeviceList).waitFor({ state: 'visible' });
  }

  cardByName(name) {
    return this.page.locator(SELECTORS.halDeviceCards)
      .filter({ has: this.page.locator('.hal-device-name', { hasText: name }) });
  }
}
```

### On-Device Test Pattern

```python
"""Boot health checks: verify the device booted cleanly."""

import pytest

class TestBootHealth:
    """Verify the device completed a healthy boot sequence."""

    def test_no_serial_errors(self, health_parser):
        """No ERROR-level log lines should appear during normal boot."""
        errors = health_parser.get_errors()
        critical_errors = [e for e in errors if "MQTT" not in e.module]
        assert len(critical_errors) == 0, (
            f"Found {len(critical_errors)} error(s) during boot:\n"
            + "\n".join(e.raw for e in critical_errors[:10])
        )

    def test_hal_discovery_complete(self, api):
        """HAL device discovery must complete."""
        resp = api.get("/api/hal/devices")
        assert resp.status_code == 200
        devices = resp.json()
        assert isinstance(devices, list)
        assert len(devices) > 0
```

**Key patterns:**
- Class-based test grouping: `class Test<Feature>:`
- Fixtures via `conftest.py`: `api` (authenticated requests session), `health_parser` (serial log parser), `serial_reader`
- Descriptive docstrings on every test method
- Assert with descriptive failure messages
- Markers in `pytest.ini` for selective execution

## Mocking Strategy

### C++ Mocks (`test/test_mocks/`)

**Philosophy:** Manual header-only mocks that replace Arduino/ESP-IDF types. No mocking framework.

**ArduinoMock singleton pattern:**
```cpp
class ArduinoMock {
public:
    static void reset() {
        mockMillis = 0;
        // Reset all mock state
    }
    static unsigned long mockMillis;
};

unsigned long millis() { return ArduinoMock::mockMillis; }
```

**Usage in `setUp()`:**
```cpp
void setUp() {
    ArduinoMock::reset();
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
}
```

**What to mock:** Arduino functions, WiFi/MQTT/ETH clients, NVS Preferences, I2C Wire, LittleFS, cryptographic functions, ESP timer, I2S driver

**What NOT to mock:** ArduinoJson (use real parser), standard C++ library, production logic under test

### E2E Mocks

**Mock Express server** (`e2e/mock-server/server.js`):
- Serves real frontend HTML assembled from `web_src/`
- 14 route modules mirror firmware REST API
- All routes mounted at both `/api/` and `/api/v1/` for versioning
- `ws-state.js` provides deterministic mock state with `resetState()` via `POST /api/__test__/reset`

**WebSocket mocking** (Playwright `routeWebSocket()`):
- Intercepts browser WS to port 81 at browser level
- Auth handshake: client sends `{type:'auth', token:'...'}`, mock responds `{type:'authSuccess'}`
- Initial state: `buildInitialState()` sends 10 ordered fixture messages after auth
- Command routing: `handleCommand(type, data)` returns response arrays

**Fixture factories** (`e2e/helpers/fixture-factories.js`):
```javascript
function buildHalDevice(overrides = {}) {
    return deepMerge({
        slot: 0,
        compatible: 'ti,pcm5102a',
        name: 'PCM5102A',
        type: 1,
        state: 3,
        // ... all default fields
    }, overrides);
}
```

## CI/CD Pipeline

**Workflow:** `.github/workflows/tests.yml` — "Quality Gates"

**Triggers:** Push to `main`/`develop`, PRs to `main`/`develop`

**Job dependency chain:**
```
cpp-tests ─┐
cpp-lint  ─┤
js-lint   ─┼──→ build (firmware binary artifact)
e2e-tests ─┤
doc-coverage─┤
security-check─┘
```

**Job details:**

1. **cpp-tests** (ubuntu-latest): Python 3.11, PlatformIO, `pio test -e native -v`
2. **cpp-lint** (ubuntu-latest): cppcheck with `--enable=warning,performance --error-exitcode=1`
3. **js-lint** (ubuntu-latest): Node 20, runs `find_dups.js`, `check_missing_fns.js`, ESLint, `diagram-validation.js`, web_pages guard
4. **e2e-tests** (ubuntu-latest): Node 20, Playwright Chromium, `npx playwright test --grep-invert @visual`. Uploads report on failure (14-day retention)
5. **doc-coverage** (ubuntu-latest): `node tools/check_mapping_coverage.js`
6. **security-check** (ubuntu-latest): Blocks `TEST_MODE` in `platformio.ini`
7. **build** (ubuntu-latest): `pio run -e esp32-p4`, uploads `firmware.bin` artifact (30-day retention). Only runs after all gates pass.

**Device tests workflow:** `.github/workflows/device-tests.yml`
- Self-hosted runner with `has-esp32p4` label
- Triggered by: `workflow_dispatch`, `[device-test]` in commit message, `device-test` PR label
- Skips `reboot` marker tests in CI
- Can auto-create GitHub issues on failure (`--create-issues`)

**Other workflows:**
- `.github/workflows/docs.yml` — Documentation site build/deploy
- `.github/workflows/release.yml` — Release automation
- `.github/workflows/device-tests.yml` — On-device integration tests
- `.github/workflows/claude-review.yml` — AI code review
- `.github/workflows/review-automation.yml` — Review automation
- `.github/workflows/issue-fix.yml` — Automated issue fixing
- `.github/workflows/issue-triage.yml` — Issue triage

## Pre-Commit Hooks

**Location:** `.githooks/pre-commit`
**Activate:** `git config core.hooksPath .githooks`

**7 checks run before every commit:**

1. `node tools/find_dups.js` — Duplicate JS declarations across `web_src/js/` modules
2. `node tools/check_missing_fns.js` — Functions called in ws-router + HTML but not defined in JS
3. ESLint on `web_src/js/` — Undefined globals, redeclare, eqeqeq
4. `node tools/check_mapping_coverage.js` — Doc-mapping coverage of source files
5. `node tools/diagram-validation.js` — Architecture diagram symbol validation
6. **SECURITY:** Block `TEST_MODE` in `platformio.ini` build flags (disables auth rate limiting)
7. Guard `web_pages.cpp` — Blocks direct edits without corresponding `web_src/` changes

## Coverage Requirements

**No hard numeric coverage target enforced.** Quality is ensured through mandatory test rules:

**Mandatory test coverage rules:**

| Change Type | Required Action |
|-------------|----------------|
| New C++ module in `src/` | Create `test/test_<module>/test_<module>.cpp` with tests |
| Changed C++ function signature | Update related tests |
| New web UI control | E2E test verifying WS command sent |
| New WS broadcast type | Fixture JSON + E2E test for DOM update |
| Changed element ID | Update `e2e/helpers/selectors.js` + affected specs |
| New WS command handler | Update `e2e/helpers/ws-helpers.js` `handleCommand()` |
| New REST endpoint | Update `e2e/mock-server/routes/*.js` + fixture |
| New JS global | Add to `web_src/.eslintrc.json` |
| Health check changes | Update smoke test expected lists |

## Known Testing Gaps

**Hardware-specific paths:**
- I2C bus SDIO conflict timing (Bus 0 + WiFi) — cannot be unit tested
- Real-time audio DMA under load — no synthetic load generator
- ESP32-P4 silicon-specific behavior (ECO2 vs ECO3)
- MCLK continuity requirements for PCM1808 PLL stability

**E2E limitations:**
- Mock server simplifies some REST responses vs real firmware
- No actual WebSocket port 81 connection — all intercepted at browser level
- Visual regression tests (`@visual`) excluded from CI (run locally only)

**Coverage blind spots:**
- Fuzz testing for WS/REST protocol parsing
- Concurrent multi-client WebSocket stress testing
- OTA update end-to-end (partial coverage via on-device tests)
- GUI (LVGL) screens — only 3 test modules, limited by native mock complexity

**On-device test limitations:**
- Requires physical ESP32-P4 hardware on self-hosted runner
- WiFi/MQTT tests depend on network environment
- Reboot tests skipped in CI (`-m "not reboot"`)
- Test results can vary with hardware revision

---

*Testing analysis: 2026-03-25*
