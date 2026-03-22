# Coding Conventions

**Analysis Date:** 2026-03-23

## Naming Patterns

**Files:**
- C++ headers: `snake_case.h` (e.g., `debug_serial.h`, `app_state.h`, `wifi_manager.h`)
- C++ implementations: `snake_case.cpp` (e.g., `wifi_manager.cpp`, `settings_manager.cpp`)
- JavaScript: `NN-descriptive-name.js` where NN is two-digit load order (e.g., `01-core.js`, `02-ws-router.js`, `15-hal-devices.js`)
- Driver headers: `hal_<device>.h` (e.g., `hal_es9038q2m.h`, `hal_pcm5102a.h`)
- Register definition headers: `<device>_regs.h` (e.g., `es9038pro_regs.h`, `es8311_regs.h`)
- Test directories: `test_<module>/` with file `test_<module>.cpp` (e.g., `test/test_auth/test_auth_handler.cpp`)
- Test E2E specs: `<feature>.spec.js` (e.g., `auth.spec.js`, `wifi.spec.js`)

**Functions (C++):**
- Global functions: `camelCase` starting with lowercase verb (e.g., `initWebSocket()`, `connectToWiFi()`, `loadSettings()`)
- Class methods: `camelCase` (e.g., `getInstance()`, `setFSMState()`, `markAdcEnabledDirty()`)
- Handler callbacks: `handle<Action>` (e.g., `handleWiFiConfig()`, `handleSettingsGet()`, `handleAPToggle()`)
- Initialization: `init<System>` (e.g., `initWebSocket()`, `initWiFiEventHandler()`, `initializeNetworkServices()`)
- Getter/setter pairs: `get<Property>()` / `set<Property>()` (e.g., `getResetReasonString()`, `setFSMState()`)
- Predicates: `is<Condition>` (e.g., `isDisplayDirty()`, `isValidIP()`)
- Conversion: `<Source>To<Destination>` (e.g., `rssiToQuality()`, `compareVersions()`)
- Deprecated functions: `// Deprecated in DEBT-X — use newFunction() instead` comment included

**Variables (C++):**
- Global state: `snake_case` (e.g., `activeSessions[]`, `mockWebPassword`, `pendingConnection`)
- Private class members: `_camelCase` prefixed with underscore (e.g., `_lastFailTime`, `_loginFailCount`, `_minLevel`)
- Static local state: `static <type> variable` declared at function scope (e.g., `static int _loginFailCount = 0;`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_SESSIONS`, `SESSION_TIMEOUT_US`, `LOGIN_COOLDOWN_US`)
- Struct members: `camelCase` (e.g., `sessionId`, `createdAt`, `lastSeen`)
- Enums: `UPPER_SNAKE_CASE` values (e.g., `ESP_RST_POWERON`, `ESP_RST_EXT`, `LOG_DEBUG`, `LOG_INFO`)

**Types (C++):**
- Structs: `PascalCase` (e.g., `WiFiNetworkConfig`, `WiFiConnectionRequest`, `Session`, `WsToken`)
- Classes: `PascalCase` (e.g., `AppState`, `ButtonHandler`, `HalDeviceManager`, `HalTdmInterleaver`)
- Enums: `PascalCase` for enum name, `UPPER_SNAKE_CASE` for values (e.g., `AppFSMState`, `LogLevel`)
- Type aliases: `using <name> = <type>` style with PascalCase name (e.g., `using HalEepromData = DacEepromData`)

**Variables (JavaScript):**
- Global state: `camelCase` (e.g., `ws`, `wsReconnectDelay`, `currentWifiConnected`, `currentActiveTab`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `WS_MIN_RECONNECT_DELAY`, `WS_MAX_RECONNECT_DELAY`, `MAX_BUFFER`)
- Private/internal functions: `_leadingUnderscore()` (e.g., `_wsLimitWarned`, `_binDiag`)
- Event handlers: `handle<Event>` or `on<Event>` (e.g., `handleBinaryMessage()`, `routeWsMessage()`)
- Getter/builder patterns: `get<Data>()` or `build<Component>()` (e.g., `buildInitialState()`, `handleCommand()`)

**Functions (JavaScript):**
- Event handlers: `handle<Action>` (e.g., `handleBinaryMessage()`, `handleCommand()`)
- Initialization: `init<System>` (e.g., `initWebSocket()`)
- Async operations: `verb + description` (e.g., `apiFetch()`, `loadFixture()`, `acquireSessionCookie()`)
- Converters: `<Source>To<Destination>` (e.g., `escapeHtml()`)
- Predicates: `is<Condition>` or `can<Action>` (e.g., `validateWsMessage()`)
- Toggle/set operations: `toggle<Feature>()` or `set<Feature>()` (e.g., `toggleTheme()`, `setBrightness()`)

## Code Style

**Formatting:**
- No explicit formatter configured (`.prettierrc` not present). Follow existing style by example.
- Indentation: 2 spaces in JavaScript (web_src/js/), 4 spaces in C++ (inferred from code)
- Line length: No hard limit enforced; aim for <120 characters for readability
- Semicolons: JavaScript requires semicolons (enforced by `eqeqeq` linting rule)
- Braces: Same-line opening for C++ (`if (x) {`), followed by code

**Linting:**
- **JavaScript (ESLint via `web_src/.eslintrc.json`):**
  - Environment: browser + ES2020
  - Rules enforced:
    - `no-undef`: Error — all global variables must be declared in `.eslintrc.json` globals
    - `no-redeclare`: Error — no duplicate `let`/`const`/`var` declarations
    - `eqeqeq: ["error", "smart"]` — use `===` / `!==` (except for null checks where `==` allowed)
  - Total 468+ globals declared for concatenated scope (all JS files load in one namespace)
  - Run: `npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`

- **C++ (cppcheck in CI/CD):**
  - Runs on `src/` (excludes `src/gui/`)
  - No configuration file in repo; uses default cppcheck rules
  - Enforces memory safety, logic errors, style issues

## Import Organization

**C++ includes:**
1. Standard library headers (e.g., `<string>`, `<cstring>`, `<Arduino.h>`)
2. External dependencies (e.g., `<ArduinoJson.h>`, `<Preferences.h>`)
3. Project-local includes in quotes (e.g., `"app_state.h"`, `"debug_serial.h"`)
4. Conditional test includes (e.g., `#ifdef NATIVE_TEST` guards for mock headers)
5. Hardware abstraction headers (e.g., `<driver/i2s_std.h>`, `<mbedtls/md.h>`)

**C++ patterns:**
- Headers first, then implementation
- Blank line after includes before code
- Conditional compilation: `#ifdef NATIVE_TEST` for test-specific includes

**JavaScript imports:**
- Concatenated file loading via numeric prefix (01-core.js loads first, then 02-ws-router.js, etc.)
- No explicit `import` statements — all functions in global scope
- Globals must be declared in `.eslintrc.json` to pass linting
- Fixture loading: `const fs = require('fs'); const path = require('path');` for file I/O in E2E helpers

**Path aliases:**
- No aliases configured in C++ (direct includes)
- No aliases in JavaScript (files load in strict order)

## Error Handling

**Patterns (C++):**

1. **Return codes:**
   - Boolean: `true` for success, `false` for failure (e.g., `bool loadSettings()`, `bool connectToWiFi()`)
   - Explicit codes: Enum-based (e.g., `AppFSMState`, `HalInitResult`)
   - Integer codes: For structured data (e.g., `-1` for error, `0` for success, `>0` for specific codes)

2. **Validation:**
   - Guard clauses before expensive operations (e.g., `if (!isValidIP(ip)) { LOG_W(...); return false; }`)
   - Bounds checking on array access (e.g., `if (index >= MAX_SIZE) { return false; }`)
   - Null checks explicit (e.g., `if (ptr == nullptr) { return; }`)

3. **Logging errors:**
   - Use `LOG_E()` macro for failures (e.g., `LOG_E("Auth failed: %s", reason)`)
   - Use `LOG_W()` for warnings/recovery (e.g., `LOG_W("WiFi reconnecting...")`)
   - Use `LOG_I()` for state transitions (e.g., `LOG_I("WiFi connected")`)
   - Use `LOG_D()` only for high-frequency operational details (never in ISR/audio loop)
   - Always include context: module prefix in brackets + descriptive message

4. **Recovery:**
   - Deferred state changes via dirty flags (e.g., `_settingsDirty = true` → main loop saves)
   - Graceful degradation (e.g., heap critical → disable binary WS streams, keep JSON only)
   - Rate limiting on retries (e.g., `LOGIN_COOLDOWN_US = 300000000ULL` for 5-minute backoff)

**Patterns (JavaScript):**

1. **Try-catch:**
   - Wrap JSON parsing: `try { return await this.json(); } catch(e) { throw new Error('Invalid response format'); }`
   - Wrap fetch: `catch (error) { console.error('API Fetch Error:', error); throw error; }`
   - Wrap file I/O in tests: `try { return JSON.parse(fs.readFileSync(...)); } catch(e) { ... }`

2. **Validation:**
   - Existence checks: `if (!data || typeof data !== 'object') return false;`
   - Field presence: `validateWsMessage(data, ['field1', 'field2'])` function checks all required fields
   - Safe JSON: `response.safeJson()` custom method that validates response.ok before parsing
   - HTML escaping: `escapeHtml(str)` on all untrusted user input (e.g., WS message fields)

3. **Error messaging:**
   - API errors: Extract error message from JSON body: `const errBody = await this.clone().json(); if (errBody.error) errorMsg = errBody.error;`
   - User-facing: Toast notifications via `showToast(message, severity)` (e.g., `showToast('Failed to connect', 'error')`)
   - Console logging: `console.log()` for diagnostics, `console.warn()` for warnings, `console.error()` for failures

4. **Authentication errors:**
   - 401 responses redirect to `/login` (e.g., `if (response.status === 401) { window.location.href = '/login'; }`)
   - Never allow further `.then()` calls after auth failure: `return new Promise(() => {})` (never-resolving promise)

## Logging

**Framework:** `debug_serial.h` with `LOG_D`/`LOG_I`/`LOG_W`/`LOG_E` macros; `Serial.print` forbidden in ISR/audio tasks (blocks UART, starves DMA).

**Logging conventions by module:**

| Module | Prefix | When to Log |
|--------|--------|-----------|
| `smart_sensing` | `[Sensing]` | Mode changes, threshold, timer, amplifier state |
| `i2s_audio` | `[Audio]` | Init, sample rate changes, ADC detection changes |
| `signal_generator` | `[SigGen]` | Init, start/stop, PWM duty, param changes |
| `buzzer_handler` | `[Buzzer]` | Init, pattern start/complete (exclude tick/click) |
| `wifi_manager` | `[WiFi]` | Connection attempts, AP mode, scan results |
| `mqtt_handler` | `[MQTT]` | Connect/disconnect, HA discovery, publish errors |
| `ota_updater` | `[OTA]` | Version checks, download progress, verification |
| `settings_manager` | `[Settings]` | Load/save operations |
| `usb_audio` | `[USB Audio]` | Init, connect/disconnect, streaming start/stop |
| `button_handler` | — | Logged from main.cpp (11 LOG calls) |
| `gui_*` | `[GUI]` | Navigation, screen transitions, theme changes |
| `hal_*` | `[HAL]`/`[HAL Discovery]`/etc | Device lifecycle, discovery, config save/load |
| `output_dsp` | `[OutputDSP]` | Per-output DSP stage add/remove |

**Rules:**
- Use `LOG_I` for state transitions (start/stop, connect/disconnect, health changes)
- Use `LOG_D` for high-frequency operational details (pattern steps, param snapshots)
- **NEVER log inside ISR or audio task** — `Serial.print` blocks, starves DMA, causes audio dropout
- Use dirty-flag pattern: task sets flag, main loop calls `audio_periodic_dump()` for output
- Log transitions, not repetitive state (use static `prev` variables to detect changes)

**Log level control:**
- Runtime via `applyDebugSerialLevel(enabled, level)` function
- Levels: `LOG_NONE` (0), `LOG_ERROR` (1), `LOG_INFO` (2), `LOG_DEBUG` (3)
- Master off = errors only; master on = respects selected level

## Comments

**When to comment:**
- Public API function intentions (header comment above declaration)
- Non-obvious algorithm logic (e.g., version comparison with beta suffix handling)
- Workarounds for hardware quirks (e.g., `PCM1808 FMT pin driven to match configured format`)
- Deprecated code with replacement guidance (e.g., `// Deprecated in DEBT-5 — use requestDeviceToggle() instead`)
- State transition explanations (e.g., `// WiFi reconnect handshake: set paused flag, wait for audio task ACK`)

**When NOT to comment:**
- Self-evident code (e.g., `if (!ptr) return;`)
- Redundant rewording (e.g., `x++; // increment x`)
- Obviously correct logic (e.g., `return a && b;`)

**JSDoc/TSDoc:**
- C++ headers document public functions with Doxygen-style comments:
  ```cpp
  /**
   * Compare semantic version strings like "1.0.7" and "1.1.2"
   * Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
   */
  int compareVersions(const String &v1, const String &v2);
  ```
- JavaScript: minimal documentation; self-explanatory code preferred

## Function Design

**Size guidelines:**
- Target <50 lines per function for readability
- Break up large functions into helpers (e.g., `handleWiFiConfig()` calls `connectToWiFi()` + `saveWiFiNetwork()`)
- Handlers in `main.cpp` delegating to modular functions

**Parameters:**
- Pass struct/object over many scalar params (e.g., `connectToWiFi(const WiFiNetworkConfig &config)` vs 8 separate string params)
- Use `const` references for large objects (e.g., `const JsonDocument &doc`)
- Prefer value types for small scalars (e.g., `bool useStaticIP`, `int level`)
- JavaScript: object destructuring for optional params (e.g., `function apiFetch(url, options = {})`)

**Return values:**
- Boolean for success/failure checks (e.g., `bool loadSettings()`)
- Struct for multi-value returns (e.g., `I2sPortInfo getInfo()`)
- Reference for efficiency (e.g., `const String& getName()`)
- Never return null pointers without documentation (use sentinel values or bool flags instead)
- JavaScript: Promise-based for async operations (e.g., `async function apiFetch(url, options)` returns `Promise<Response>`)

## Module Design

**Exports (C++):**
- Header file declares all public functions and types
- Implementation file has static helpers and file-scoped state (e.g., `static int _loginFailCount = 0;`)
- No global mutable state without justification (use AppState singleton or file-local statics)
- Single responsibility per module (e.g., `wifi_manager.h` handles WiFi only, not WiFi + settings)

**Exports (JavaScript):**
- All functions in global scope (concatenated file loading)
- Globals declared in `.eslintrc.json` (both functions and variables)
- No module system (no `export`/`import` — direct concatenation via script tags)
- Private functions prefixed with `_` by convention (e.g., `_wsLimitWarned`, `_binDiag`)

**Barrel files (re-exports):**
- Not used in this codebase
- Each module is self-contained; import directly from source

---

*Convention analysis: 2026-03-23*
