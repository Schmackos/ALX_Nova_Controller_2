# Plan: Automated Browser Testing for ALX Nova Web UI

## Context

The ALX Nova web UI has 8 tabs, 35+ toggles/checkboxes, 25+ dropdowns, 12 canvases, 28 JS modules concatenated into a single scope, and ~60 WebSocket message types. All UI regression testing is currently manual. This plan adds Playwright-based E2E browser tests that:

1. Verify interactive elements (toggles, buttons, dropdowns, sliders) send correct WebSocket commands
2. Verify incoming WebSocket broadcasts update the DOM correctly
3. Detect regressions in tab switching, auth flow, and element visibility
4. Run automatically in CI (GitHub Actions) alongside existing PlatformIO tests
5. Integrate into the release workflow

## Current State

- **Frontend source**: `web_src/index.html` + `web_src/css/01-05-*.css` + `web_src/js/01-28-*.js`
- **Build tool**: `node tools/build_web_assets.js` → `src/web_pages.cpp` (raw literal) + `src/web_pages_gz.cpp`
- **HTML storage**: `web_pages.cpp` wraps HTML in `R"rawliteral(...)rawliteral;"`
- **Login page**: Separate HTML in `src/login_page.h`, same raw literal format
- **WebSocket**: Port 81, auth via `sessionId` cookie, binary frames for waveform (0x01) and spectrum (0x02)
- **CI**: `tests.yml` (PlatformIO test + build), `release.yml` (test + release) — no Node.js steps exist
- **No existing browser test infrastructure** (no package.json, Playwright, Cypress anywhere)

## Approach: Playwright + Mock Server + routeWebSocket

Since the ESP32 isn't available in CI, we mock both HTTP and WebSocket:
- A lightweight **Express server** serves the assembled HTML directly from `web_src/` files (same assembly logic as `build_web_assets.js`)
- Mock `/login` and `/api/session` endpoints for auth
- Playwright's **`routeWebSocket()`** intercepts `ws://localhost:81` connections, simulating the ESP32 WebSocket server with a state machine
- No real hardware needed — tests run fully headless in CI

## File Structure

```
test/web_ui/
├── package.json              # Playwright + Express deps
├── playwright.config.js      # Config (baseURL, webServer, timeout)
├── mock-server.js            # Express server: assembles HTML from web_src/, serves login, mock API
├── ws-helpers.js             # WebSocket mock: initial state builder, command handler, binary frame builders
├── fixtures.js               # Playwright custom fixture: routeWebSocket setup, auth flow
├── tests/
│   ├── auth.spec.js          # Auth flow: authRequired → auth → authSuccess, bad session → authFailed
│   ├── navigation.spec.js    # Tab switching (all 8 tabs), sidebar toggle, mobile bottom bar
│   ├── control-tab.spec.js   # Sensing mode radios, manual override buttons, amplifier status updates
│   ├── audio-tab.spec.js     # Audio toggles (waveform/spectrum/VU), dropdowns, canvas presence, DAC, siggen
│   ├── dsp-tab.spec.js       # DSP enable/bypass, add stage, PEQ bands, frequency canvas
│   ├── wifi-tab.spec.js      # SSID/password form, static IP toggle reveals fields, scan button
│   ├── mqtt-tab.spec.js      # MQTT enable toggle, form fields, save command
│   ├── settings-tab.spec.js  # Dark mode, buzzer, brightness/timeout dropdowns, firmware version display
│   └── debug-tab.spec.js     # HW stats toggle, task table population, CPU/memory graph canvases
└── .gitignore                # node_modules/, playwright-report/, test-results/
```

## Implementation Steps

### Step 1: Create `test/web_ui/package.json`

```json
{
  "name": "alx-nova-web-ui-tests",
  "private": true,
  "scripts": {
    "test": "npx playwright test",
    "test:headed": "npx playwright test --headed",
    "test:debug": "npx playwright test --debug",
    "update-snapshots": "npx playwright test --update-snapshots"
  },
  "devDependencies": {
    "@playwright/test": "^1.50.0",
    "express": "^4.21.0"
  }
}
```

### Step 2: Create `test/web_ui/mock-server.js`

The mock server:
- Reads `web_src/index.html`, injects CSS from `web_src/css/*.css` and JS from `web_src/js/*.js` (sorted alphanumerically), exactly like `tools/build_web_assets.js`
- Serves the assembled HTML at `GET /`
- Serves the login page at `GET /login` (reads from `src/login_page.h`, extracts raw literal)
- Mock `POST /api/login` — always returns `{ success: true, sessionId: "test-session" }` with `Set-Cookie`
- Mock `GET /api/session` — validates session cookie, returns `{ valid: true }`
- Listens on port 3000

This approach reads from `web_src/` directly rather than parsing `web_pages.cpp`, keeping it simpler and always in sync.

### Step 3: Create `test/web_ui/ws-helpers.js`

Provides reusable mock state and frame builders:

- `buildInitialState(overrides)` — returns the JSON broadcasts the ESP32 sends after auth success: `wifiStatus`, `smartSensing`, `displayState`, `buzzerState`, `mqttSettings`, `audioGraphState`, `debugState`, `signalGenerator`, `emergencyLimiterState`, `adcState`, `dacState`, `dspState`, `dspMetrics`
- `handleCommand(type, data, state)` — state machine handling inbound WS commands, returns appropriate response messages
- `buildWaveformFrame(adcIndex, samples)` — builds binary `[0x01][adc][256 bytes]`
- `buildSpectrumFrame(adcIndex, freq, bands)` — builds binary `[0x02][adc][f32LE freq][16× f32LE bands]`

### Step 4: Create `test/web_ui/fixtures.js`

Custom Playwright fixture extending `@playwright/test`:

```js
const { test as base, expect } = require('@playwright/test');

exports.test = base.extend({
  connectedPage: async ({ page, context }, use) => {
    // 1. Set session cookie on context
    await context.addCookies([{
      name: 'sessionId', value: 'test-session',
      domain: 'localhost', path: '/'
    }]);

    // 2. Intercept WebSocket to port 81 via routeWebSocket
    await page.routeWebSocket(/.*:81/, ws => {
      ws.onMessage(msg => {
        const data = JSON.parse(msg);
        if (data.type === 'auth') {
          // Send authSuccess + initial state broadcasts
          ws.send(JSON.stringify({ type: 'authSuccess' }));
          for (const broadcast of buildInitialState()) {
            ws.send(JSON.stringify(broadcast));
          }
        } else {
          // Route through command handler
          const responses = handleCommand(data.type, data, mockState);
          responses.forEach(r => ws.send(JSON.stringify(r)));
        }
      });
    });

    // 3. Navigate to app
    await page.goto('/');
    // 4. Wait for WS connection indicator
    await expect(page.locator('#statusWs')).toHaveClass(/connected/, { timeout: 5000 });

    await use(page);
  }
});
```

### Step 5: Test Specs (4 phases, priority order)

**Phase 1 — Core (catches most regressions):**

1. **`auth.spec.js`** (4-5 tests)
   - WS connects and sends auth with sessionId cookie
   - authSuccess triggers connected state (statusWs dot green)
   - authFailed redirects to /login
   - Missing cookie still attempts auth

2. **`navigation.spec.js`** (10-12 tests)
   - Click each of 8 sidebar items → correct panel visible, others hidden
   - Click mobile bottom bar tabs → same panel switching
   - Sidebar collapse/expand toggle works
   - Status bar remains visible across all tabs
   - Default tab is `control`

3. **`control-tab.spec.js`** (6-8 tests)
   - Radio buttons (always_on, always_off, smart_auto) send correct WS command
   - Manual override buttons send `manualOverride(true/false)`
   - `smartSensing` broadcast updates amplifier status dot, signal level, timer
   - Smart Auto settings card collapsible toggle
   - Timer/threshold inputs are number fields with correct ranges

**Phase 2 — Interactive elements:**

4. **`audio-tab.spec.js`** (15-20 tests)
   - Toggle waveform/spectrum/VU → sends `setWaveformEnabled`/`setSpectrumEnabled`/`setVuMeterEnabled`
   - Canvases (waveform, spectrum, PPM) present when enabled, hidden when disabled
   - `audioUpdateRateSelect` change → sends `setAudioUpdateRate`
   - `audioSampleRateSelect` change → sends update
   - `fftWindowSelect` change → sends `setFftWindowType`
   - ADC enable toggles → send `setAdcEnabled`
   - DAC card: enable/mute/volume → send correct commands
   - Signal generator: enable toggle, waveform select, frequency slider
   - Emergency limiter: enable toggle, threshold slider
   - Input name fields present for all 4 lanes

5. **`settings-tab.spec.js`** (10-12 tests)
   - Dark mode toggle sends command, body class updates
   - Buzzer toggle + volume select
   - Brightness/timeout/dim dropdowns send correct commands
   - `displayState` broadcast populates select values
   - Firmware version displays from `wifiStatus` broadcast
   - Import/export buttons present
   - Reboot/factory reset buttons present

6. **`wifi-tab.spec.js`** (6-8 tests)
   - SSID/password fields fillable
   - Static IP checkbox reveals/hides extra fields
   - Scan button present and clickable
   - `wifiStatus` broadcast updates status box
   - AP toggle present

7. **`mqtt-tab.spec.js`** (5-6 tests)
   - MQTT enable toggle shows/hides fields
   - Form fields (broker, port, user, pass, topic) fillable
   - `mqttSettings` broadcast populates all fields
   - Save button triggers command

**Phase 3 — Complex interactions:**

8. **`dsp-tab.spec.js`** (8-10 tests)
   - DSP enable/bypass toggles send commands
   - `dspState` broadcast populates channel tabs and stages
   - Frequency response canvas present
   - Graph layer toggles (Individual/RTA/Chain)
   - Add stage button opens menu with stage types
   - PEQ band strip updates from state

9. **`debug-tab.spec.js`** (6-8 tests)
   - HW stats toggle → sends `setDebugHwStats`
   - `hardware_stats` broadcast populates CPU/heap/PSRAM values and fills task table
   - Canvas graphs (cpuGraph, memoryGraph, psramGraph) present when enabled
   - Serial level dropdown sends `setDebugSerialLevel`
   - Task monitor toggle + table population

### Step 6: CI Integration — `tests.yml`

Add a new `web-ui-test` job that runs alongside the existing `test` job:

```yaml
web-ui-test:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - uses: actions/setup-node@v4
      with:
        node-version: '20'
        cache: 'npm'
        cache-dependency-path: test/web_ui/package-lock.json
    - name: Install dependencies
      working-directory: test/web_ui
      run: npm ci
    - name: Install Playwright browsers
      working-directory: test/web_ui
      run: npx playwright install --with-deps chromium
    - name: Run browser tests
      working-directory: test/web_ui
      run: npx playwright test
    - uses: actions/upload-artifact@v4
      if: always()
      with:
        name: playwright-report
        path: test/web_ui/playwright-report/
```

Update `build` job: `needs: [test, web-ui-test]` — firmware only builds when both test suites pass.

### Step 7: CI Integration — `release.yml`

Add same `web-ui-test` job. Update `release` job: `needs: [test, web-ui-test]`.

### Step 8: Playwright Config

```js
const { defineConfig } = require('@playwright/test');

module.exports = defineConfig({
  testDir: './tests',
  timeout: 30000,
  retries: process.env.CI ? 1 : 0,
  reporter: process.env.CI ? 'html' : 'list',
  use: {
    baseURL: 'http://localhost:3000',
    trace: 'on-first-retry',
  },
  webServer: {
    command: 'node mock-server.js',
    port: 3000,
    reuseExistingServer: !process.env.CI,
  },
  projects: [
    { name: 'chromium', use: { browserName: 'chromium' } },
  ],
});
```

### Step 9: Update `CLAUDE.md` — Enforce Browser Test Coverage for All Changes

Add a new section to `CLAUDE.md` establishing the policy that every web UI change must include corresponding Playwright test updates. This ensures the test-writer agent and any future Claude sessions know to create/update browser tests alongside code changes.

**Add to CLAUDE.md under "## Testing Conventions":**

#### Browser / E2E Tests (Playwright)

Playwright browser tests live in `test/web_ui/tests/`. They verify the web UI against a mock WebSocket server without real hardware.

**Mandatory coverage rule**: Any change to `web_src/` files (HTML, CSS, JS) or `src/websocket_handler.cpp` (new/changed WS message types) **must** include corresponding Playwright test updates:
- New toggle/button/dropdown → add test verifying it sends the correct WS command
- New WS broadcast type → add test verifying the DOM updates correctly
- New tab or section → add navigation test + element presence tests
- Changed element IDs or handler names → update affected test selectors
- Removed features → remove corresponding tests

**Running browser tests:**
```bash
cd test/web_ui
npm install                              # First time only
npx playwright install --with-deps chromium  # First time only
npx playwright test                      # Run all browser tests
npx playwright test tests/audio-tab.spec.js  # Run single spec
npx playwright test --headed             # Run with visible browser
```

**Test infrastructure:**
- `test/web_ui/mock-server.js` — Express server assembling HTML from `web_src/`
- `test/web_ui/ws-helpers.js` — WebSocket mock state machine + binary frame builders
- `test/web_ui/fixtures.js` — Playwright fixture providing `connectedPage` with auth + WS mock
- When adding new WS message types, update `ws-helpers.js` `buildInitialState()` and `handleCommand()`

**CI**: Browser tests run in both `tests.yml` and `release.yml` workflows. The firmware build and release are gated on browser tests passing.

**Also add to the `web_pages` module description in the Architecture section:**

> Browser tests in `test/web_ui/tests/` verify all interactive elements via Playwright with a mock WebSocket — update tests when changing `web_src/` files.

## Files Created/Modified

### New files (all in `test/web_ui/`):
1. `test/web_ui/package.json`
2. `test/web_ui/playwright.config.js`
3. `test/web_ui/mock-server.js`
4. `test/web_ui/ws-helpers.js`
5. `test/web_ui/fixtures.js`
6. `test/web_ui/.gitignore`
7. `test/web_ui/tests/auth.spec.js`
8. `test/web_ui/tests/navigation.spec.js`
9. `test/web_ui/tests/control-tab.spec.js`
10. `test/web_ui/tests/audio-tab.spec.js`
11. `test/web_ui/tests/dsp-tab.spec.js`
12. `test/web_ui/tests/wifi-tab.spec.js`
13. `test/web_ui/tests/mqtt-tab.spec.js`
14. `test/web_ui/tests/settings-tab.spec.js`
15. `test/web_ui/tests/debug-tab.spec.js`

### Modified files:
16. `.github/workflows/tests.yml` — add `web-ui-test` job, `build` needs both
17. `.github/workflows/release.yml` — add `web-ui-test` job, `release` needs both
18. `CLAUDE.md` — add browser testing conventions, mandatory coverage rule, and running instructions

## Key Design Decisions

- **Read from `web_src/` not `web_pages.cpp`**: The mock server assembles HTML from source files directly, avoiding fragile C++ raw literal parsing. Always in sync with the actual source.
- **`routeWebSocket()` over a real WS server**: Playwright's built-in interception is simpler, per-test isolated, and doesn't need port coordination. Each test controls its own mock state.
- **Chromium only**: ESP32 web UI users are on a local network — cross-browser differences aren't a concern. Keeps CI fast (~30s).
- **Phase 1 first**: Auth + navigation + control tab catch the most common regressions (broken JS killing all tabs, missing elements, auth flow breakage) with ~20 tests.
- **Element selection strategy**: Use `[id="appState.mqttEnabled"]` attribute selectors for the dot-containing IDs (Playwright handles these natively).
- **No visual regression in Phase 1-3**: Screenshot tests are fragile and require baseline maintenance. Deferred to Phase 4 once the functional test suite is stable.

## Verification

1. `cd test/web_ui && npm install && npx playwright install --with-deps chromium`
2. `npx playwright test` — all tests pass locally
3. `pio test -e native` — existing 918+ native tests still pass (no interference)
4. Push to `develop` — GitHub Actions runs `test`, `web-ui-test`, and `build` jobs
5. Playwright report artifact available on failure for debugging
