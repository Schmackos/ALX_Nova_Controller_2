# Testing Architecture

This document describes the ALX Nova Controller 2 testing infrastructure, how the layers connect, and how to keep coverage current as the codebase evolves.

## Architecture Diagrams

All diagrams are in `docs-internal/architecture/` as Mermaid `.mmd` files:

| Diagram | File | Shows |
|---------|------|-------|
| E2E Test Stack | [test-infrastructure.mmd](architecture/test-infrastructure.mmd) | How Playwright, Express mock server, and real frontend connect |
| CI Quality Gates | [ci-quality-gates.mmd](architecture/ci-quality-gates.mmd) | 4 parallel gates → gated build → release |
| Test Execution Flow | [e2e-test-flow.mmd](architecture/e2e-test-flow.mmd) | Single test lifecycle: cookie → WS auth → assertions → teardown |
| Coverage Map | [test-coverage-map.mmd](architecture/test-coverage-map.mmd) | What each test layer covers and hardware-only gaps |

## Test Layers

### Layer 1: C++ Unit Tests (1,271+ tests, 57 modules)

**Location**: `test/` directories, each in its own folder.
**Runner**: `pio test -e native -v` (Unity framework on host machine).
**Scope**: All firmware logic — HAL lifecycle, audio pipeline, DSP, networking, auth, settings, sensors, button handling.
**Mocks**: `test/test_mocks/` provides Arduino, WiFi, MQTT, Preferences stubs.

### Layer 2: Playwright E2E Tests (26 tests, 19 specs)

**Location**: `e2e/tests/*.spec.js`
**Runner**: `cd e2e && npx playwright test`
**Scope**: Full web UI in real Chromium — tab navigation, form controls, WS state sync, REST API contracts, responsive layout.

#### How It Works

```
Playwright Test Runner
  │
  ├── connectedPage fixture (e2e/helpers/fixtures.js)
  │     ├── POST /api/auth/login → session cookie
  │     ├── page.routeWebSocket(/.*:81/) → intercepts WS at browser level
  │     ├── WS auth handshake: authRequired → auth → authSuccess
  │     ├── Broadcasts 10 initial state fixtures in order
  │     └── Waits for #wsConnectionStatus = "Connected"
  │
  ├── Express Mock Server (e2e/mock-server/server.js, port 3000)
  │     ├── assembler.js → reads web_src/index.html + injects CSS/JS
  │     ├── 12 route files → deterministic REST responses
  │     └── ws-state.js → singleton state, reset between tests
  │
  └── Test Specs (e2e/tests/*.spec.js)
        └── DOM assertions against real frontend rendered with mock data
```

#### Test Spec Inventory

| Spec | Tests | What It Validates |
|------|-------|-------------------|
| auth.spec.js | 3 | Login page, correct password redirect, invalid session |
| navigation.spec.js | 2 | 8 sidebar tabs switch panels, default is Control |
| control-tab.spec.js | 2 | Sensing mode radios, amplifier status from WS |
| audio-inputs.spec.js | 2 | Audio sub-nav, channel strip population from audioChannelMap |
| audio-matrix.spec.js | 1 | 8x8 routing matrix grid renders |
| audio-outputs.spec.js | 1 | Output strips with HAL device names |
| audio-siggen.spec.js | 1 | Signal generator enable toggle and parameters |
| peq-overlay.spec.js | 1 | PEQ overlay opens with frequency response canvas |
| hal-devices.spec.js | 2 | Device cards render, rescan/disable send correct API calls |
| wifi.spec.js | 1 | SSID/password form, scan, saved networks, static IP |
| mqtt.spec.js | 1 | Enable toggle, config fields populated from WS |
| settings.spec.js | 1 | Theme, buzzer, display controls populated from WS |
| ota.spec.js | 1 | Version display, check-for-updates API call |
| debug-console.spec.js | 2 | Log entries render, module chip filtering |
| dark-mode.spec.js | 1 | Night-mode class toggle, localStorage persistence |
| auth-password.spec.js | 1 | Password change modal validation and API submission |
| responsive.spec.js | 1 | Mobile viewport: bottom bar visible, sidebar hidden |
| hardware-stats.spec.js | 1 | CPU/heap/PSRAM/temp from WS hardware_stats |
| support.spec.js | 1 | Support tab content renders |

### Layer 3: Static Analysis

| Tool | Scope | Run |
|------|-------|-----|
| ESLint | `web_src/js/` (380 globals) | `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json` |
| cppcheck | `src/` (excl. `gui/`) | CI only: `cppcheck --enable=warning,style,performance src/` |
| find_dups.js | Cross-file duplicate declarations | `node tools/find_dups.js` |
| check_missing_fns.js | Undefined function references | `node tools/check_missing_fns.js` |

### Layer 4: CI Quality Gates

All 4 run in parallel on every push/PR to `main` and `develop`:

```
Push/PR → ┌─ cpp-tests ────┐
          ├─ cpp-lint ──────┤
          ├─ js-lint ───────┤ → build → (release)
          └─ e2e-tests ─────┘
```

Firmware build only proceeds if all 4 pass. Same gates in `release.yml`.

## Key Technical Decisions

### WS Port 81 Problem
Frontend hardcodes `new WebSocket(${wsProtocol}//${wsHost}:81)` at `web_src/js/01-core.js:99`. Port 81 is privileged on Linux CI runners.

**Solution**: Playwright's `page.routeWebSocket(/.*:81/)` intercepts WebSocket connections at browser level — no actual port binding needed. The mock server (Express) only handles HTTP on port 3000.

### connectedPage Fixture
Every test that needs a connected UI uses the `connectedPage` fixture from `e2e/helpers/fixtures.js`. It:
1. Acquires a session cookie via `POST /api/auth/login`
2. Sets the cookie in the browser context
3. Intercepts WS via `page.routeWebSocket()`
4. Completes the auth handshake (authRequired → auth → authSuccess)
5. Sends 10 initial state broadcasts in order
6. Waits for `#wsConnectionStatus` to show "Connected"
7. Exposes `page.wsRoute.send(obj)` for per-test WS injection

### Tab Navigation
Tests use `page.evaluate(() => switchTab('tabName'))` instead of clicking sidebar items. This avoids scroll/visibility issues when the sidebar height is constrained.

### Fixture Design
- All fixtures are hand-crafted, deterministic JSON files
- No random data — timestamps use base 10000ms
- Realistic values from actual HAL device DB (PCM5102A, ES8311, PCM1808, etc.)
- One JSON file per WS message type / REST response
- Fixture values must match what `src/websocket_handler.cpp` broadcasts

## Keeping Tests Up-to-Date

### When Adding a New Web Feature

1. **HTML** (`web_src/index.html`): Add elements with unique `id` attributes
2. **JS** (`web_src/js/*.js`): Add handler functions, WS message routing
3. **Fixture** (`e2e/fixtures/ws-messages/`): Create JSON fixture for new WS message type
4. **Route** (`e2e/mock-server/routes/`): Add/update Express route if new REST endpoint
5. **State** (`e2e/mock-server/ws-state.js`): Add new state fields if needed
6. **Helpers** (`e2e/helpers/ws-helpers.js`): Update `buildInitialState()` and `handleCommand()`
7. **Selectors** (`e2e/helpers/selectors.js`): Add new DOM selectors
8. **Test** (`e2e/tests/`): Write spec with `connectedPage` fixture
9. **ESLint** (`web_src/.eslintrc.json`): Add new top-level declarations to globals
10. **Verify**: `cd e2e && npx playwright test` — all 26+ tests pass

### When Adding a New HAL Driver

1. Use the `hal-driver-scaffold` agent — it creates header, implementation, factory registration, DB entry, and test module
2. Run `firmware-test-runner` agent to verify C++ tests pass
3. If the driver has web UI controls, follow the web feature checklist above
4. Add device to `e2e/mock-server/ws-state.js` `halDevices` array
5. Update `e2e/fixtures/ws-messages/hal-device-state.json`

### When Changing WS Protocol

1. Update `src/websocket_handler.cpp` broadcast function
2. Update matching fixture in `e2e/fixtures/ws-messages/`
3. Update `e2e/helpers/ws-helpers.js` if message is part of initial state
4. Update `web_src/js/02-ws-router.js` `routeWsMessage()` handler
5. Add/update Playwright test verifying frontend handles the message
6. Run both `firmware-test-runner` and `test-engineer` agents in parallel

### Agent Workflow

Use specialised agents to verify test coverage after changes:

```
┌────────────────────────────────────┐
│ Code Change Detected               │
├────────────────────────────────────┤
│ C++ only?  → firmware-test-runner  │
│ Web only?  → test-engineer         │
│ Both?      → Launch BOTH parallel  │
│ New driver?→ hal-driver-scaffold   │
│ New UI?    → web-feature-scaffold  │
│ Bug?       → debugger agent        │
└────────────────────────────────────┘
```

## Not Covered (Hardware-Only)

These cannot be tested without real hardware:
- I2S DMA streaming and audio quality
- GPIO ISR / interrupts
- FreeRTOS multi-core task scheduling under real load
- PSRAM allocation under heap pressure
- WiFi SDIO I2C bus conflict (GPIO 48/54)
- Actual TFT display rendering (LVGL on ST7735S)
