# Coding Conventions

**Analysis Date:** 2026-03-21

## Naming Patterns

**Files:**
- C++ source: lowercase_with_underscores.cpp (e.g., `audio_pipeline.cpp`, `wifi_manager.cpp`)
- C++ headers: lowercase_with_underscores.h (e.g., `audio_pipeline.h`, `config.h`)
- State headers: domain_state.h pattern in `src/state/` (e.g., `audio_state.h`, `wifi_state.h`)
- HAL drivers: hal_device_name.h/.cpp (e.g., `hal_pcm5102a.h`, `hal_discovery.h`)
- Test files: test_<module>/test_<module>.cpp pattern (e.g., `test/test_auth/test_auth_handler.cpp`)
- Web JS: NN-name.js numbered by load order (e.g., `01-core.js`, `02-ws-router.js`, `27a-health-dashboard.js`)

**Functions (C++):**
- Module-scoped init/main functions: lowercase_with_underscores (e.g., `audio_pipeline_init()`, `wifi_manager_begin()`)
- Public API functions: lowercase_with_underscores, module-prefixed (e.g., `audio_pipeline_set_source()`, `dsp_add_stage()`)
- Static/internal helpers: lowercase_with_underscores with leading underscore optional (e.g., `_routing_matrix_apply()`)
- Getter functions: `get_` prefix (e.g., `audio_pipeline_get_source()`, `dsp_get_cpu_usage()`)
- Setter functions: `set_` prefix (e.g., `audio_pipeline_set_matrix_gain_db()`, `dac_settings_set_volume()`)
- Boolean predicates: `is_` or `has_` prefix (e.g., `audio_pipeline_is_matrix_bypass()`, `hal_device_has_error()`)
- Bypass control: `bypass_` prefix (e.g., `audio_pipeline_bypass_input()`, `audio_pipeline_bypass_dsp()`)

**Variables (C++):**
- Global state: camelCase for extern variables (e.g., `appState`, `pendingConnection`, `wifiStatusUpdateRequested`)
- Static module locals: snake_case (e.g., `_matrixGain`, `_sources`, `_activeIndex`)
- Instance members (structs): camelCase (e.g., `appState.audio.adcEnabled[i]`, `dev.compatString`)
- Loop counters: single letters (i, j, k, f for frame, s for sample)
- Macro constants: UPPERCASE_WITH_UNDERSCORES (e.g., `MAX_24BIT_F`, `AUDIO_PIPELINE_MAX_INPUTS`)
- Configuration defines: UPPERCASE_WITH_UNDERSCORES (e.g., `I2S_BCK_PIN`, `DSP_MAX_STAGES`)
- Volatile cross-core flags: snake_case with `volatile` keyword (e.g., `volatile bool audioPaused`, `volatile bool _swapRequested`)

**Types (C++):**
- Structs: PascalCase (e.g., `AudioInputSource`, `HalDeviceConfig`, `DspState`)
- Enums: PascalCase for enum name, UPPERCASE_WITH_UNDERSCORES for values (e.g., `enum AppFSMState { STATE_IDLE, STATE_SIGNAL_DETECTED, ... }`)
- Enum classes: PascalCase (e.g., `enum class LogLevel { DEBUG, INFO, WARN, ERROR }`)
- Type aliases (using): PascalCase (e.g., `using HalDeviceId = uint8_t`)

**Functions (JavaScript):**
- Event handlers: camelCase with on/handle prefix (e.g., `onMatrixCellClick()`, `handleBinaryMessage()`, `handleHalDeviceState()`)
- UI updaters: camelCase starting with update/render/toggle/set (e.g., `updateConnectionStatus()`, `renderAudioSubView()`, `toggleDim()`)
- DOM queries: camelCase returning element or boolean (e.g., `document.getElementById()`, `isValidIP()`)
- Animation/loop functions: camelCase ending in Loop/Anim (e.g., `audioAnimLoop()`, `vuAnimLoop()`)
- State setters: camelCase starting with set (e.g., `setMatrixGainDb()`, `setScreenTimeout()`)
- Toggle/boolean: camelCase starting with toggle/is (e.g., `toggleVuMode()`, `toggleDim()`)

**Variables (JavaScript):**
- Global state: camelCase (e.g., `currentWifiSSID`, `audioSubscribed`, `numInputLanes`, `darkMode`)
- Constants: UPPERCASE_WITH_UNDERSCORES (e.g., `WS_MIN_RECONNECT_DELAY`, `MAX_BUFFER`, `DEBUG_MAX_LINES`)
- Internal state objects: snake_case or camelCase depending on scope (e.g., `_binDiag`, `inputFocusState`)
- Arrays: plural camelCase (e.g., `waveformCurrent[]`, `spectrumTarget[]`, `audioChannelMap`)
- Loop counters: i, j, f (frame)

**Abbreviations in naming:**
- ADC/DAC: always uppercase (e.g., `adcEnabled`, `dacState`, `halDev`)
- DSP: always uppercase (e.g., `dspBypass`, `dspPipeline`)
- HAL: always uppercase (e.g., `halSlot`, `halDevice`, `halCoord`)
- I2C/I2S: uppercase (e.g., `i2cBus`, `i2sAudio`)
- RMS/VU/FFT: uppercase (e.g., `float_rms()`, `vuDetected`, `fftWindow`)
- WiFi/MQTT: lowercase/camelCase in code (e.g., `wifiSSID`, `mqttConnected`)

## Code Style

**Formatting:**
- No automated formatter (Prettier not used on C++)
- Indentation: 2 spaces for JavaScript, 4 spaces for C++
- Line length: No hard limit, but keep function signatures readable
- Brace style: Allman style for C++ (opening brace on new line), K&R for JavaScript

**Linting:**
- **JavaScript**: ESLint with config in `web_src/.eslintrc.json`
  - Rules: `no-undef` (error), `no-redeclare` (error), `eqeqeq: ["error", "smart"]`
  - 380+ global definitions via the `globals` object for concatenated scope compatibility
  - 14-day comment prefixes for source-generated sections (e.g., `// 14-day: auto-generated from design_tokens.h`)
- **C++**: cppcheck static analysis (run in CI on `src/` excluding `src/gui/`)
- **Duplicate JS declarations**: `node tools/find_dups.js` checks for `let`/`const` redeclaration across files
- **Missing function references**: `node tools/check_missing_fns.js` detects undefined function calls

## Import Organization

**C++ includes:**
1. Standard library (cstdint, cmath, cstring, cstdio)
2. Arduino/ESP-IDF (Arduino.h, FreeRTOS, driver/...)
3. Third-party (ArduinoJson.h, WebSockets.h, PubSubClient.h)
4. Local project headers (app_state.h, config.h, debug_serial.h)
5. Sub-module headers (hal/, state/, gui/ — only include needed)

**JavaScript module order** (files concatenated in numerical order):
- 01-core.js — WebSocket setup, global API fetch, connection state
- 02-ws-router.js — Binary message handler, WS dispatch, authSuccess handler
- 03-app-state.js — AppState singleton snapshot storage
- 04-shared-audio.js — Shared audio properties (numInputLanes, resizeAudioArrays)
- 05-audio-tab.js — Audio tab main view, sub-nav, input/output strips, matrix UI
- 06-canvas-helpers.js — Shared canvas utilities (resizeCanvasIfNeeded, spectrum color)
- 06-peq-overlay.js — PEQ/DSP overlay modals, biquad math visualization
- 07-ui-core.js — Toast notifications, tooltips
- 08-ui-status.js — Status bar, WiFi/MQTT/AMP indicators
- 09-audio-viz.js — Animation loops, waveform/spectrum drawing, VU meters
- 13-signal-gen.js — Signal generator UI (waveform select, freq/amp sliders)
- 15-hal-devices.js — HAL device list, expansion, edit forms, YAML export/import
- 15a-yaml-parser.js — YAML string encode/decode helper
- 20-wifi-network.js — WiFi scan, network select, AP config, static IP
- 21-mqtt-settings.js — MQTT broker config, Home Assistant discovery
- 22-settings.js — General settings (theme, timezone, backlight, buzzer, debug)
- 23-firmware-update.js — OTA check, manual upload, release browser
- 24-hardware-stats.js — System info tables (task monitor, CPU/memory graphs)
- 25-debug-console.js — Debug log viewer with module filters, search, export
- 26-support.js — Support/manual content, QR code, release notes
- 27-auth.js — Password change modal, logout, default password warning
- 27a-health-dashboard.js — Device health grid, error counters, diagnostic journal
- 28-init.js — Page initialization, tab restore, event listeners

**Path aliases** (C++ include directives):
- Include `<hal/hal_device.h>` for HAL framework types
- Include `<state/audio_state.h>` for domain-specific state headers
- Include `"audio_pipeline.h"` (quote) for same-directory headers

## Error Handling

**Patterns (C++):**
- **Boundary validation**: Early return with false/nullptr/0 and optional `LOG_W()`
  - Example: `if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return nullptr;`
  - No exceptions (embedded C++, Arduino framework)
  - All out-of-range checks precede actual logic
- **Boolean returns**: false for error, true for success (predicates use is/has/can prefixes)
  - Example: `bool audio_pipeline_set_sink(int slot, const AudioOutputSink *sink)`
- **Nullable pointers**: Return nullptr on failure (checked by caller)
  - Example: `const AudioInputSource* audio_pipeline_get_source(int lane)`
  - Caller: `if (src == nullptr) { LOG_W(...); return; }`
- **Struct returns**: Return struct with `.success` field (no out-parameters)
  - Example: `struct ProbeResult { bool success; HalDevice *dev; } hal_probe_device(...)`
  - Usage: `ProbeResult r = hal_probe_device(...); if (!r.success) { LOG_W(...); }`
- **Status/error codes**: Single 16-bit diagnostic code via `diag_emit(0xXXXX)` (not traditional errno)
  - Example: `if (overflow) diag_emit(DIAG_HAL_TOGGLE_OVERFLOW);`
  - Codes defined in `src/diag_error_codes.h`
- **Array capacity checks**: Call function that returns bool before using
  - Example: `if (!requestDeviceToggle(slot, ACTION_ENABLE)) { LOG_W("toggle queue full"); return false; }`
- **Allocation failures**: Check pointer after `malloc/calloc`, emit diagnostic
  - Example: `float *buf = (float*)heap_caps_calloc(...); if (!buf) { diag_emit(DIAG_SYS_PSRAM_ALLOC_FAIL); ... }`
- **I2C/Wire errors**: Check endTransmission() != 0, log reason
  - Example: `if (EEPROM_I2C.endTransmission(false) != 0) { LOG_W("[EEPROM] I2C error"); return false; }`

**Patterns (JavaScript):**
- **Fetch errors**: wrapped in try-catch, logged to console, show toast for user-facing
  - Example: `try { const r = await fetch(...); } catch (e) { console.error(...); showToast(...); }`
- **DOM queries**: null-check before using
  - Example: `const el = document.getElementById('x'); if (el) { el.textContent = '...'; }`
- **Array bounds**: Check length before access or use optional chaining
  - Example: `if (i < arr.length) { ... }` or `arr?.[i]?.property`
- **WS message handling**: Type-check and data validation before use
  - Example: `if (data.type === 'authSuccess' && data.token) { ... }`
- **Async/await**: All .then()/.catch() chains used, no unhandled rejections
- **No uncaught rejections**: Every async function wrapped or has .catch()

## Logging

**Framework:**
- C++: `debug_serial.h` macros (`LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`)
- JavaScript: `console.log()`, `console.warn()`, `console.error()`

**Module prefixes** (C++ `[ModuleName]`):
- `[Audio]` — i2s_audio.cpp (sample rate, ADC health)
- `[Sensing]` — smart_sensing.cpp (threshold, timer, amplifier state)
- `[SigGen]` — signal_generator.cpp (start/stop, param changes)
- `[Buzzer]` — buzzer_handler.cpp (pattern start/complete)
- `[WiFi]` — wifi_manager.cpp (connect attempts, AP mode, scan)
- `[MQTT]` — mqtt_handler.cpp, mqtt_publish.cpp (broker state, HA discovery)
- `[OTA]` — ota_updater.cpp (version checks, download progress)
- `[Settings]` — settings_manager.cpp (load/save)
- `[USB Audio]` — usb_audio.cpp (init, streaming)
- `[GUI]` — gui_*.cpp files (navigation, transitions)
- `[HAL]`, `[HAL Discovery]`, `[HAL DB]`, `[HAL API]` — hal_*.cpp files
- `[OutputDSP]` — output_dsp.cpp (stage add/remove)

**When to log:**
- **LOG_I**: State transitions (connect/disconnect, enable/disable, mode changes), significant events
- **LOG_D**: High-frequency operational details (pattern steps, parameter snapshots, but NOT per-sample)
- **LOG_W**: Warnings (bounds exceeded, soft recovery, timeout)
- **LOG_E**: Errors (failures, crashes, unrecoverable conditions)

**What NOT to log:**
- Never log inside ISR or audio_pipeline_task — use dirty-flag pattern (task sets flag, main loop logs)
- Never log per-sample or per-frame in real-time tasks (causes audio dropouts)
- Never log plaintext passwords or session tokens
- Never log inside mutex/semaphore critical sections if mutex wraps Serial.print

## Comments

**When to comment:**
- Complex algorithms (matrix operations, DSP coefficient generation, FFT windowing)
- Non-obvious design decisions (why trade-off X was chosen over Y)
- State machine transitions and FSM state meanings
- Tricky edge cases or bounds-related logic
- TODO/FIXME items (with rationale): `// TODO: implement X when Y is available`
- Struct field meanings that aren't obvious from type alone

**When NOT to comment:**
- Self-documenting code (good function/variable names eliminate need)
- Obvious loops or conditionals
- Trivial assignments
- Every single line (over-commenting reduces signal-to-noise)

**JSDoc/TSDoc (C++):**
- Minimal usage — function declarations above public APIs with brief description
- Format: `/// \brief Brief description` or `/**< ... */` inline
- Example:
  ```cpp
  /// Register an audio input source at a given lane index.
  void audio_pipeline_set_source(int lane, const AudioInputSource *src);
  ```
- Do NOT use full Doxygen multi-line comments on every function

**Code block comments (C++):**
- Use `// ===== Section Title =====` to separate logical blocks
- Example: `// ===== Compile-time dimension invariants =====`

## Function Design

**Size:**
- Keep functions under 40 lines when possible
- Complex functions document their logical sections with comment blocks
- One responsibility per function (SRP)

**Parameters:**
- Const pointers for read-only inputs: `const AudioInputSource *src`
- Mutable pointers for output: `float *out_buf`
- Value parameters for primitives (int, float, bool)
- No default parameters in C++ (embedded context)
- Maximum 4-5 parameters (use struct for more)

**Return values:**
- bool for success/failure predicates
- Pointer for optional resource (nullptr = not found/error)
- Struct for multiple return values (e.g., `struct ProbeResult { bool success; HalDevice *dev; }`)
- void for setup/init functions that can't fail
- Enums for status codes (e.g., `enum HalDeviceState { UNKNOWN, DETECTED, AVAILABLE, ... }`)

**Examples (C++):**
```cpp
// Good: minimal signature, clear intent
bool audio_pipeline_set_sink(int slot, const AudioOutputSink *sink);

// Good: struct for multiple returns
struct ProbeResult hal_probe_device(uint8_t addr);

// Good: const pointer input
void audio_pipeline_set_source(int lane, const AudioInputSource *src);

// Avoid: too many parameters
// bool dsp_add_stage_complex(int ch, float *in, float *out, int frames, ...);
// Instead, use a config struct
```

## Module Design

**Exports:**
- Header exposes only public API (no impl details)
- Static/internal helpers stay in .cpp file
- Forward declarations for opaque types in headers
- Example (audio_pipeline.h):
  ```cpp
  void audio_pipeline_init();
  void audio_pipeline_set_source(int lane, const AudioInputSource *src);
  float audio_pipeline_get_matrix_gain(int out_ch, int in_ch);
  // Implementation details hidden in .cpp
  ```

**Barrel files (index headers):**
- Used for HAL framework: `src/hal/hal.h` includes all hal_*.h headers
- Used for state: `src/state/app_state.h` includes all domain_state.h files
- Used sparingly to avoid circular dependencies

**File coupling:**
- Module A (audio_pipeline.cpp) includes headers from:
  - Standard library
  - Configuration (config.h)
  - Its own API (audio_pipeline.h)
  - Dependencies (app_state.h, dsp_pipeline.h, dac_hal.h)
  - NOT other peer modules (wifi_manager.h) — use appState instead
- Minimize inter-module coupling via appState singleton

**Initialization order (main.cpp):**
1. Arduino setup() — Serial, GPIO, pinouts
2. Firmware version, config load
3. AppState singleton
4. Subsystem init (WiFi, MQTT, Audio, DAC, DSP, OTA, GUI)
5. Task creation (audio_pipeline_task, mqtt_task, gui_task)

---

*Conventions analysis: 2026-03-21*
