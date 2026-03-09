# Test Strategy & Automation Plan

## Architecture Diagrams

See `docs-internal/architecture/` for visual references:
- [test-infrastructure.mmd](../architecture/test-infrastructure.mmd) — E2E test stack (Playwright + Express mock + real frontend)
- [ci-quality-gates.mmd](../architecture/ci-quality-gates.mmd) — CI/CD pipeline with 4 parallel quality gates
- [e2e-test-flow.mmd](../architecture/e2e-test-flow.mmd) — Single test lifecycle sequence (auth + WS mock + DOM assertions)
- [test-coverage-map.mmd](../architecture/test-coverage-map.mmd) — What each test layer covers (unit, E2E, static, contract, hardware-only gaps)

## Context

The ALX Nova Controller 2 has 1,271 C++ unit tests but zero frontend/E2E testing. The 9,700-line web UI (24 JS files in global scope) has no automated test coverage — every regression in layout, WebSocket state sync, REST API contracts, and HAL device interactions is caught only by manual testing. This plan adds Playwright E2E tests, a Node.js mock server, static analysis (ESLint + cppcheck), pre-commit hooks, and WS protocol documentation to create a comprehensive quality gate pipeline.

---

## Phase 1: Foundation (~1 day)

Create `e2e/` directory in repo root with Playwright + Express mock server infrastructure.

### Directory Structure
```
e2e/
  package.json
  playwright.config.js
  .gitignore
  mock-server/
    server.js                         # Express HTTP server (port 3000)
    assembler.js                      # Assembles HTML from web_src/ (replicates build_web_assets.js)
    routes/
      auth.js, hal.js, wifi.js, mqtt.js, settings.js, ota.js,
      pipeline.js, dsp.js, sensing.js, siggen.js, diagnostics.js, system.js
    ws-state.js                       # Deterministic mock state singleton
  fixtures/
    ws-messages/                      # One JSON file per WS broadcast type
    api-responses/                    # One JSON file per REST endpoint response
  tests/                              # 26 Playwright spec files
  helpers/
    fixtures.js                       # connectedPage fixture with auth + WS mock
    ws-helpers.js                     # WS state machine, binary frame builders
    selectors.js                      # Reusable DOM selectors

docs/api/
  ws-protocol.md                      # Human-readable WS protocol overview
  schemas/                            # JSON Schema per message type

web_src/
  .eslintrc.json                      # ESLint config for concatenated global scope

.githooks/
  pre-commit                          # find_dups + check_missing_fns + ESLint
```

### Key Config Files

**`e2e/package.json`**: `@playwright/test`, `express`, `cookie-parser`. Scripts: `test`, `test:headed`, `test:debug`, `mock-server`, `lint`.

**`e2e/playwright.config.js`**: Chromium only, headless, timeout 30s, retries 1 in CI. `webServer` starts `node mock-server/server.js` on port 3000. Reporter: `html` in CI, `list` locally. Trace: `on-first-retry`.

### WS Port 81 Solution

Frontend hardcodes `new WebSocket(${wsProtocol}//${wsHost}:81)` at `web_src/js/01-core.js:99`. Port 81 is privileged on Linux CI runners. Solution: **Playwright `page.routeWebSocket()`** intercepts WS at browser level — no port binding needed. WS mock logic lives in `helpers/ws-helpers.js`, wired in the `connectedPage` fixture. Express handles REST only.

### Mock Server (`mock-server/server.js`)

**HTTP (port 3000)**: `assembler.js` reads `web_src/index.html`, injects CSS/JS from `web_src/css/*.css` and `web_src/js/*.js` (sorted alpha) — same logic as `tools/build_web_assets.js`. Serves at `GET /`. Login page extracted from `src/login_page.h` raw literal for `GET /login`. All `/api/*` routes return deterministic fixture JSON.

### Smoke Test
`e2e/tests/navigation.spec.js` — verifies mock server serves real frontend, WS auth completes, tabs render.

---

## Phase 2: Mock Server & Fixtures (~2 days)

### REST Routes (Express)

| Route File | Endpoints | Key Behavior |
|-----------|-----------|-------------|
| `auth.js` | POST login/logout, GET status, POST change | Session cookie, default password flag |
| `hal.js` | GET/POST/PUT/DELETE /api/hal/* | 5 builtin devices, 409 rescan guard |
| `wifi.js` | GET scan/list/status, POST config/toggleap | Scan results fixture |
| `mqtt.js` | GET/POST /api/mqtt | Broker config |
| `settings.js` | GET/POST settings, export/import | Bulk state |
| `ota.js` | GET checkupdate/releases | Version comparison |
| `pipeline.js` | GET/PUT matrix, sinks, output DSP | 8x8 float matrix |
| `dsp.js` | GET/PUT dsp, stages, bypass | Per-lane DSP config |
| `sensing.js` | GET/POST smartsensing | Mode, timer, threshold |
| `siggen.js` | GET/POST signalgenerator | Waveform, freq, amplitude |
| `diagnostics.js` | GET diagnostics | Heap, stack, PSRAM |
| `system.js` | POST factoryreset, reboot | Success responses |

### WS Mock (`helpers/ws-helpers.js`)

- `buildInitialState()`: Ordered array of all initial broadcasts (wifiStatus, smartSensing, displayState, buzzerState, mqttSettings, halDeviceState, audioChannelMap, dacState, dspState, etc.)
- `handleCommand(type, data, state)`: Routes inbound WS commands, returns responses
- `buildWaveformFrame(adc, samples)`: Binary 0x01 (258 bytes)
- `buildSpectrumFrame(adc, freq, bands)`: Binary 0x02 (70 bytes)

### Fixture Design
- Hand-crafted, deterministic — no random data, fixed timestamps (10000ms base)
- Realistic values: real HAL device names, valid IPs, proper SSIDs
- One JSON per message type

### Custom Fixture (`helpers/fixtures.js`)
`connectedPage`: sets sessionId cookie, intercepts WS via `page.routeWebSocket(/.*:81/)`, sends authSuccess + initial state, waits for connected indicator.

---

## Phase 3: Playwright E2E Tests — 26 tests across 19 specs (~3 days)

| Spec File | Tests | Validates |
|-----------|-------|-----------|
| `auth.spec.js` | 3 | Login page, correct password redirects, invalid session fails |
| `navigation.spec.js` | 2 | 8 sidebar tabs show correct panel, default is Control |
| `control-tab.spec.js` | 2 | Sensing mode radios, smartSensing WS updates amp status |
| `audio-inputs.spec.js` | 2 | Sub-nav renders, audioChannelMap populates input strips |
| `audio-matrix.spec.js` | 1 | 8x8 grid with labeled axes |
| `audio-outputs.spec.js` | 1 | Output strips with device names + DSP buttons |
| `audio-siggen.spec.js` | 1 | Enable toggle + waveform/freq/amplitude controls |
| `peq-overlay.spec.js` | 1 | Full-screen PEQ overlay with freq canvas + band controls |
| `hal-devices.spec.js` | 2 | Device cards render, rescan + disable send correct API calls |
| `wifi.spec.js` | 1 | SSID/password form, scan, saved networks, static IP toggle |
| `mqtt.spec.js` | 1 | Enable toggle reveals config, WS broadcast populates fields |
| `settings.spec.js` | 1 | Theme, buzzer, display controls; displayState populates selects |
| `ota.spec.js` | 1 | Version display, check button fetches /api/checkupdate |
| `debug-console.spec.js` | 2 | Log entries render, module chip filtering works |
| `dark-mode.spec.js` | 1 | Toggle night-mode class, persists to localStorage |
| `auth-password.spec.js` | 1 | Password change modal validation |
| `responsive.spec.js` | 1 | Mobile viewport: bottom bar visible, sidebar hidden |
| `hardware-stats.spec.js` | 1 | CPU/heap/PSRAM/temp fields populated from WS |
| `support.spec.js` | 1 | Support tab content renders |

---

## Phase 4: Static Analysis (~1 day)

### ESLint for web_src/js/

**`web_src/.eslintrc.json`**: ES2020, `browser: true`, `no-undef: error`, `no-redeclare: error`, `eqeqeq: ["error", "smart"]`. All cross-file globals listed explicitly (generated from top-level declarations across 24 JS files). Installed via `e2e/package.json`, run as `npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`.

### cppcheck for src/

`--enable=warning,style,performance`, `--suppress=missingInclude,unusedFunction`, `--std=c++11`, `--error-exitcode=1`. Scope: `src/` excluding `src/gui/`. Installed via `apt` in CI.

### Existing tools promoted to CI

`node tools/find_dups.js` + `node tools/check_missing_fns.js` — already exit non-zero on failure.

---

## Phase 5: CI/CD Integration (~1 day)

### Updated `.github/workflows/tests.yml`

4 parallel quality gates + 1 gated build:

```
jobs:
  cpp-tests:   pio test -e native -v                          (existing)
  cpp-lint:    cppcheck src/                                  (NEW)
  js-lint:     find_dups + check_missing_fns + ESLint         (NEW)
  e2e-tests:   npm ci + playwright install chromium + test    (NEW)
  build:       pio run -e esp32-p4                            (existing)
    needs: [cpp-tests, cpp-lint, js-lint, e2e-tests]
```

### Updated `.github/workflows/release.yml`

Same 4 gates before `release` job.

### CI Environment
Ubuntu latest, Node 20 LTS, Python 3.11. Caching: npm via `setup-node`, PlatformIO via `actions/cache`. Playwright report artifact (14-day retention).

### Pre-commit Hooks (`.githooks/pre-commit`)

1. `node tools/find_dups.js` (<1s)
2. `node tools/check_missing_fns.js` (<1s)
3. `cd e2e && npx eslint ../web_src/js/` (~2s)

Activate: `git config core.hooksPath .githooks`

---

## Phase 6: WS Protocol Documentation (~1 day)

### JSON Schema Files (`docs/api/schemas/`)

One schema per WS message type using JSON Schema 2020-12. Each has `required` fields, `type` constraints, `const` for the `type` discriminator. Binary formats documented as markdown (ws-binary-waveform.md, ws-binary-spectrum.md).

### Schema Usage
- Mock server validates outbound messages + fixtures at startup (via `ajv`)
- Playwright tests can validate received messages for contract testing
- Single source of truth for future C++ integration

### Human-readable overview
`docs/api/ws-protocol.md` — message catalog with descriptions, directions, and examples.

---

## Files to Create

| File | Purpose |
|------|---------|
| `e2e/package.json` | Playwright + Express dependencies |
| `e2e/playwright.config.js` | Test runner config |
| `e2e/.gitignore` | node_modules, reports |
| `e2e/mock-server/server.js` | Express HTTP mock server |
| `e2e/mock-server/assembler.js` | HTML assembly from web_src/ |
| `e2e/mock-server/ws-state.js` | Deterministic mock state |
| `e2e/mock-server/routes/*.js` | 12 route files |
| `e2e/fixtures/ws-messages/*.json` | ~15 WS fixture files |
| `e2e/fixtures/api-responses/*.json` | ~12 REST fixture files |
| `e2e/helpers/fixtures.js` | connectedPage Playwright fixture |
| `e2e/helpers/ws-helpers.js` | WS mock state machine |
| `e2e/helpers/selectors.js` | Reusable selectors |
| `e2e/tests/*.spec.js` | 19 test spec files |
| `web_src/.eslintrc.json` | ESLint config for frontend JS |
| `docs/api/ws-protocol.md` | Protocol overview |
| `docs/api/schemas/*.schema.json` | ~17 JSON schema files |
| `.githooks/pre-commit` | Pre-commit quality checks |

## Files to Modify

| File | Change |
|------|--------|
| `.github/workflows/tests.yml` | Add cpp-lint, js-lint, e2e-tests jobs; gate build on all 4 |
| `.github/workflows/release.yml` | Add same 4 gates before release job |
| `CLAUDE.md` | Add browser testing conventions, mandatory coverage rule |
| `.gitignore` | Add e2e/node_modules/, e2e/playwright-report/, e2e/test-results/ |

## Critical Reference Files (read-only)

| File | Why |
|------|-----|
| `tools/build_web_assets.js` | Assembly logic to replicate in assembler.js |
| `web_src/js/01-core.js` | WS connection (L99), apiFetch, session mgmt |
| `web_src/js/02-ws-router.js` | Complete WS message routing — all types mock must simulate |
| `src/websocket_handler.cpp` | All outbound WS broadcasts — source of truth for fixtures |
| `src/login_page.h` | Login HTML to extract |
| `src/hal/hal_api.cpp` | HAL REST endpoints to mock |
| `src/main.cpp` | All REST endpoint registrations |

## Verification

1. `cd e2e && npm install && npx playwright install --with-deps chromium`
2. `npx playwright test` — all 26 tests pass locally
3. `pio test -e native` — existing 1,271 C++ tests unaffected
4. `node tools/find_dups.js && node tools/check_missing_fns.js` — JS checks pass
5. Push to `develop` — all 4 CI quality gates pass, build proceeds
6. Playwright HTML report artifact available on failure

## Implementation Order

Each phase is independently valuable:
- Phase 1: Working E2E pipeline with smoke test
- Phase 2: Reusable mock server for development
- Phase 3: Full WebGUI regression coverage (26 tests)
- Phase 4: Static analysis catches bugs pre-browser
- Phase 5: All quality gates enforced in CI/CD
- Phase 6: WS protocol documented with machine-readable schemas
