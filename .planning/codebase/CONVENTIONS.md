# Coding Conventions

**Analysis Date:** 2026-03-25

## Naming Patterns

**Files:**
- C++ headers: `snake_case.h` (e.g., `src/debug_serial.h`, `src/app_state.h`, `src/wifi_manager.h`)
- C++ implementations: `snake_case.cpp` (e.g., `src/wifi_manager.cpp`, `src/settings_manager.cpp`)
- JavaScript: `NN-descriptive-name.js` where NN is two-digit load order (e.g., `web_src/js/01-core.js`, `web_src/js/02-ws-router.js`, `web_src/js/15-hal-devices.js`)
- Suffixed JS: `NNa-name.js` for sub-modules that load after NN (e.g., `web_src/js/06-peq-overlay.js`, `web_src/js/15a-yaml-parser.js`, `web_src/js/27a-health-dashboard.js`)
- HAL driver headers: `hal_<device>.h` (e.g., `src/hal/hal_ess_dac_2ch.h`, `src/hal/hal_pcm5102a.h`)
- HAL base classes: `hal_<family>_base.h` (e.g., `src/hal/hal_ess_sabre_dac_base.h`, `src/hal/hal_ess_sabre_adc_base.h`, `src/hal/hal_cirrus_dac_base.h`)
- Register headers: `<device>_regs.h` in `src/drivers/` (e.g., `src/drivers/es8311_regs.h`, `src/drivers/ess_sabre_common.h`)
- State headers: `<domain>_state.h` in `src/state/` (e.g., `src/state/audio_state.h`, `src/state/wifi_state.h`)
- Test directories: `test/test_<module>/` with file `test_<module>.cpp` (e.g., `test/test_hal_core/test_hal_core.cpp`)
- E2E specs: `<feature>.spec.js` in `e2e/tests/` (e.g., `e2e/tests/auth.spec.js`, `e2e/tests/clock-diagnostics.spec.js`)
- E2E page objects: `PascalCase.js` in `e2e/pages/` (e.g., `e2e/pages/DevicesPage.js`, `e2e/pages/BasePage.js`)
- Device tests: `test_<feature>.py` in `device_tests/tests/` (e.g., `device_tests/tests/test_boot_health.py`)

**Functions (C++):**
- Global functions: `camelCase` starting with lowercase verb (e.g., `initWebSocket()`, `connectToWiFi()`, `loadSettings()`)
- Class methods: `camelCase` (e.g., `getInstance()`, `setFSMState()`, `markAdcEnabledDirty()`)
- Handler callbacks: `handle<Action>` (e.g., `handleWiFiConfig()`, `handleSettingsGet()`, `handleAPToggle()`)
- Initialization: `init<System>` (e.g., `initWebSocket()`, `initWiFiEventHandler()`)
- Getter/setter pairs: `get<Property>()` / `set<Property>()` (e.g., `getResetReasonString()`, `setFSMState()`)
- Predicates: `is<Condition>` / `has<Condition>` (e.g., `isDisplayDirty()`, `isValidIP()`, `hasDependency()`)
- HAL lifecycle: `probe()`, `init()`, `deinit()`, `dumpConfig()`, `healthCheck()` — virtual overrides on `HalDevice`
- Cross-core atomics: `setReady(bool)` / `isReady()` using `__ATOMIC_RELEASE` / `__ATOMIC_ACQUIRE`
- Pipeline bridge: `hal_wire_builtin_dependencies()`, `audio_pipeline_request_pause()`, `audio_pipeline_resume()`
- Inline security wrappers: `server_send()`, `json_response()`, `http_add_security_headers()`

**Variables (C++):**
- Private class members: `_camelCase` with underscore prefix (e.g., `_lastFailTime`, `_loginFailCount`, `_minLevel`, `_dependsOn`)
- Global state: `camelCase` (e.g., `activeSessions[]`, `pendingConnection`)
- Static local state: `static <type> variable` at function scope (e.g., `static int _loginFailCount = 0;`)
- Constants/defines: `UPPER_SNAKE_CASE` (e.g., `MAX_SESSIONS`, `HAL_MAX_DEVICES`, `EVT_FORMAT_CHANGE`)
- Struct members: `camelCase` (e.g., `sessionId`, `createdAt`, `lastSeen`, `sampleRateMask`)
- Enum values: `UPPER_SNAKE_CASE` (e.g., `HAL_STATE_AVAILABLE`, `LOG_DEBUG`, `HAL_CAP_DPLL`)

**Types (C++):**
- Structs: `PascalCase` (e.g., `WiFiNetworkConfig`, `ClockStatus`, `AudioOutputSink`, `HalDeviceDescriptor`)
- Classes: `PascalCase` (e.g., `AppState`, `HalDeviceManager`, `HalEssDac2ch`, `HalEssSabreDacBase`)
- Enums: `PascalCase` name, `UPPER_SNAKE_CASE` values (e.g., `HalDeviceState`, `HalDeviceType`, `LogLevel`)
- Typed enums: Use explicit underlying type (e.g., `enum HalDeviceType : uint8_t`, `enum EssDac2chVolType : uint8_t`)
- Type aliases: `using <PascalCase> = <type>` (e.g., `using HalEepromData = DacEepromData`)

**Variables (JavaScript):**
- Global state: `camelCase` (e.g., `ws`, `wsReconnectDelay`, `currentWifiConnected`, `halDevices`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `WS_MIN_RECONNECT_DELAY`, `HAL_CAP_HW_VOLUME`, `LERP_SPEED`)
- Private/internal: `_leadingUnderscore` (e.g., `_wsLimitWarned`, `_binDiag`, `_pendingImportData`)
- DOM references cached: `<domain>DomRefs` pattern (e.g., `vuDomRefs`)

**Functions (JavaScript):**
- Event handlers: `handle<Action>` (e.g., `handleBinaryMessage()`, `handleHalDeviceState()`)
- Initialization: `init<System>` (e.g., `initWebSocket()`, `initSidebar()`, `initHealthDashboard()`)
- Toggle/set: `toggle<Feature>()` / `set<Feature>()` (e.g., `toggleTheme()`, `setBrightness()`)
- Builders: `build<Thing>()` / `render<Thing>()` (e.g., `buildHalDeviceCard()`, `renderHealthDashboard()`)
- UI navigation: `switchTab()`, `switchAudioSubView()` — all via `page.evaluate()` in E2E tests

## Code Style

**Formatting:**
- No explicit formatter (no `.prettierrc`). Follow existing style by example.
- C++ indentation: 4 spaces
- JavaScript indentation: 8 spaces (web_src/js/ — concatenated files are indented one level in)
- E2E/Node.js indentation: 2 spaces
- Line length: No hard limit; aim for <120 characters
- Semicolons: Required in JavaScript
- Braces: Same-line opening for C++ and JavaScript (`if (x) {`)

**Linting:**

- **JavaScript (ESLint via `web_src/.eslintrc.json`):**
  - Environment: browser + ES2020, sourceType: script
  - Rules enforced:
    - `no-undef`: Error — all globals must be declared in `.eslintrc.json` globals section
    - `no-redeclare`: Error (with `builtinGlobals: false`)
    - `eqeqeq: ["error", "smart"]` — use `===`/`!==` (except null checks where `==` is allowed)
  - 480+ globals declared for concatenated single-namespace scope
  - **When adding new JS globals:** Add to both `.eslintrc.json` and the appropriate JS file
  - Run: `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`

- **C++ (cppcheck in CI):**
  - Runs on `src/` (excludes `src/gui/`)
  - Flags: `--enable=warning,performance --suppress=missingInclude --suppress=unusedFunction --suppress=badBitmaskCheck --std=c++11 --error-exitcode=1`
  - No local config file; CI enforces via `.github/workflows/tests.yml`

- **Custom static analysis (pre-commit + CI):**
  - `node tools/find_dups.js` — finds duplicate top-level declarations across `web_src/js/` modules
  - `node tools/check_missing_fns.js` — finds functions called in ws-router + HTML but not defined in any JS module
  - `node tools/check_mapping_coverage.js` — validates `tools/doc-mapping.json` covers all source files
  - `node tools/diagram-validation.js` — validates Mermaid architecture diagram symbols

## Import Organization

**C++ includes (order):**
1. Standard library headers (`<cstring>`, `<cstdint>`, `<string>`)
2. Arduino framework (`<Arduino.h>`)
3. External dependencies (`<ArduinoJson.h>`, `<WiFi.h>`, `<LittleFS.h>`)
4. Project headers in quotes (`"app_state.h"`, `"debug_serial.h"`, `"config.h"`)
5. HAL headers with directory prefix (`"hal/hal_device_manager.h"`, `"hal/hal_api.h"`)
6. Conditional blocks for optional features:
   ```cpp
   #ifdef DSP_ENABLED
   #include "dsp_pipeline.h"
   #endif
   #ifdef DAC_ENABLED
   #include "hal/hal_device_manager.h"
   #endif
   #ifdef USB_AUDIO_ENABLED
   #include "usb_audio.h"
   #endif
   ```
7. Conditional test includes:
   ```cpp
   #ifdef NATIVE_TEST
   #include "../test_mocks/Arduino.h"
   #else
   #include <Arduino.h>
   #endif
   ```

**JavaScript (web_src/js/):**
- No `import`/`export` — all files concatenated via `<script>` tags in numeric order
- All functions and state variables exist in global scope
- Every global must be registered in `web_src/.eslintrc.json`
- Load order enforced by numeric filename prefix: `01-core.js` -> `02-ws-router.js` -> ... -> `28-init.js`

**JavaScript (E2E/Node.js):**
- CommonJS `require()` imports
- Standard ordering: test framework first, then helpers, then fixtures
  ```javascript
  const { test, expect } = require('../helpers/fixtures');
  const { test: base } = require('@playwright/test');
  const path = require('path');
  const fs = require('fs');
  ```

## Commit Message Format

Use conventional commit prefixes:
```
feat: / fix: / docs: / refactor: / test: / chore:
```

Scoped variants are used for subsystems:
```
feat(docs): enterprise section, landing page UX
fix: replace dynamic_cast with static_cast + capability check
```

**Rules:**
- Never add `Co-Authored-By` trailers (per project CLAUDE.md)
- PR merges use `(#NN)` suffix with PR number
- Keep subject line under ~72 characters
- Body is optional; used for multi-item changes

**Examples from recent history:**
```
feat: clock quality diagnostics and device dependency graph (Phase 3) (#87)
docs: update CLAUDE.md with docs site structure (enterprise sidebar, blog, showcase)
fix: replace dynamic_cast with static_cast + capability check in main.cpp
refactor: replace vTaskSuspendAll with atomic pointer swap for sink/source ops
test: add clock diagnostics and device dependency test modules
```

## PR/Branch Conventions

**Branch naming:**
- Feature branches: `feature/<description>` (e.g., `feature/phase2-asrc-dsd`)
- Fix branches: `fix/<description>`

**PR workflow:**
- Create from feature branch targeting `main`
- All 6 CI gates must pass before merge (cpp-tests, cpp-lint, js-lint, e2e-tests, doc-coverage, security-check)
- Merge to main, then delete feature branch (both local and remote)
- After merge, verify CI on main

## Documentation Standards

**Code comments:**
- Public API: Doxygen-style `/** ... */` block above declaration
- Section separators: `// ===== Section Name =====` for major sections within files
- Inline separator comments: `// ---------------------------------------------------------------------------`
- HAL driver file headers include compatible strings handled and pattern letter
- Workarounds: explain hardware quirk and reference
- Deprecated code: `// Deprecated in DEBT-X` with replacement guidance

**Auto-generated files:**
- `src/web_pages.cpp` and `src/web_pages_gz.cpp` — auto-generated from `web_src/`
- Edit `web_src/` files, then run `node tools/build_web_assets.js`
- Pre-commit hook blocks direct edits to web_pages*.cpp without web_src/ changes

**Documentation site:**
- Docusaurus v3 in `docs-site/`
- MDX files: escape `\{variable\}` outside code blocks
- Internal planning docs: `docs-internal/`

## Error Handling Patterns

**C++ — Return codes:**
- Boolean: `true` = success, `false` = failure (e.g., `bool loadSettings()`)
- Structured: `HalInitResult` with error code + reason string on failure
  ```cpp
  HalInitResult init() override {
      if (initResult) return hal_init_ok();
      return hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
  }
  ```
- Diagnostic error codes: `DIAG_*` constants in `src/diag_error_codes.h`

**C++ — Validation:**
- Guard clauses before expensive operations
- Bounds checking on array access (e.g., `if (slot >= HAL_MAX_DEVICES) return;`)
- Null checks explicit: `if (ptr == nullptr) { return; }`
- `strncpy` with explicit null termination: always `buf[sizeof(buf) - 1] = '\0'`
- Config validation via `hal_validate_config()` before applying device settings

**C++ — Recovery:**
- Dirty-flag pattern: task sets flag, main loop does Serial/WS output
- Deferred toggle via `appState.halCoord.requestDeviceToggle(slot, action)`
- Graceful degradation on heap pressure (disable binary WS streams, keep JSON)
- Rate limiting on retries (e.g., `LOGIN_COOLDOWN_US = 300000000ULL`)

**JavaScript — Error handling:**
- `apiFetch()` wrapper: auto-redirects on 401, 10s timeout, `safeJson()` method
- Try-catch on JSON parse: always handle non-JSON error bodies
- Toast notifications: `showToast(message, severity)` for user-facing errors
- Never-resolving promise after auth failure: `return new Promise(() => {})`

**REST API responses:**
- New endpoints use `json_response()` envelope: `{"success":true/false,"data":...,"error":"..."}`
- Do NOT retrofit existing endpoints (would break clients)
- Error strings are JSON-escaped in `json_response()` to prevent malformed output

## Logging Conventions

**Framework:** `src/debug_serial.h` — `LOG_D`/`LOG_I`/`LOG_W`/`LOG_E` macros
- Levels: `LOG_DEBUG` (0), `LOG_INFO` (1), `LOG_WARN` (2), `LOG_ERROR` (3), `LOG_NONE` (4)
- Runtime control via `applyDebugSerialLevel(enabled, level)`

**Module prefixes:**

| Module | Prefix | When to Log |
|--------|--------|-----------|
| `smart_sensing` | `[Sensing]` | Mode changes, threshold, timer, amplifier state |
| `i2s_audio` | `[Audio]` | Init, sample rate changes, ADC detection |
| `signal_generator` | `[SigGen]` | Init, start/stop, param changes |
| `wifi_manager` | `[WiFi]` | Connection attempts, AP mode, scan results |
| `mqtt_handler` | `[MQTT]` | Connect/disconnect, HA discovery, publish errors |
| `ota_updater` | `[OTA]` | Version checks, download progress, verification |
| `settings_manager` | `[Settings]` | Load/save operations |
| `usb_audio` | `[USB Audio]` | Init, connect/disconnect, streaming |
| `hal_*` | `[HAL]`/`[HAL Discovery]` | Device lifecycle, discovery, config |
| `output_dsp` | `[OutputDSP]` | Per-output DSP stage add/remove |
| `health_check` | `[Health]` | Category status changes, clock diagnostics |

**Critical rules:**
- Use `LOG_I` for state transitions (start/stop, connect/disconnect)
- Use `LOG_D` for high-frequency operational details
- **NEVER log inside ISR or audio task** — blocks UART TX, starves DMA, causes dropout
- Use dirty-flag pattern: audio task sets flag, main loop outputs
- Log transitions, not repetitive state (use static `prev` variables to detect changes)
- Save log files to `logs/` directory

## Icons

All web UI icons use inline SVG from [Material Design Icons (MDI)](https://pictogrammers.com/library/mdi/). No external CDN.

```html
<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true">
  <path d="<paste MDI path here>"/>
</svg>
```

Use `fill="currentColor"`, explicit `width`/`height`, `aria-hidden="true"` on decorative icons, `aria-label` on interactive icon-only elements.

## Module Design

**C++ modules:**
- Header declares public interface; implementation has static helpers and file-local state
- No global mutable state without justification — use `AppState` singleton or file-local statics
- Single responsibility per module (e.g., `wifi_manager.h` handles WiFi only)
- HAL drivers follow inheritance hierarchy: `HalDevice` -> `HalAudioDevice` -> `HalEssSabreDacBase` -> `HalEssDac2ch`
- Generic driver patterns (Pattern A-D) driven by descriptor tables — no per-chip subclasses

**JavaScript modules:**
- All functions in global scope (concatenated file loading)
- Globals declared in `.eslintrc.json` with `"readonly"` or `"writable"` access
- No module system — direct concatenation via script tags
- Private functions prefixed with `_` by convention
- No barrel files or re-exports

**State management:**
- 15 domain-specific state headers in `src/state/`, composed into `AppState` singleton
- Access: `appState.domain.field` (e.g., `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`)
- Dirty flags minimize WS/MQTT broadcasts: `isDirty()` / `clearDirty()`
- Every dirty setter calls `app_events_signal(EVT_XXX)` for immediate main-loop wake

---

*Convention analysis: 2026-03-25*
