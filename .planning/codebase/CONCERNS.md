# Codebase Concerns

**Analysis Date:** 2026-03-08

---

## CI-Blocking Bugs (Must Fix Before Next Green Build)

These issues were identified by cppcheck in the 2026-03-08 CI run and currently block the `cpp-lint` quality gate.

---

### OOB-1: Input Name Arrays Sized for 4, Loop Iterates 16

**Severity: Critical — buffer over-read, undefined behaviour at runtime**

- Issue: Three separate static arrays are declared with 4 elements but iterated with `for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++)` where `AUDIO_PIPELINE_MAX_INPUTS = 8`, so the loop runs `i = 0..15`, reading indices 4–15 past the end of the array.
- Files:
  - `src/mqtt_ha_discovery.cpp:1414-1426` — `inputLabels[4]` and `inputDisplayNames[4]`
  - `src/mqtt_publish.cpp:889-893` — `labels[4]`
  - `src/settings_manager.cpp:539-549` — `INPUT_NAME_DEFAULTS[4]`
- Trigger: Calling `publishMqttHADiscovery()`, `publishMqttInputNames()`, or `loadInputNames()` when defaults are applied (first boot or no `/inputnames.txt`).
- Impact: Reading garbage memory past the array end. On embedded targets with strict MPU the read wraps; on native tests it silently returns junk strings, masking the bug.
- Fix approach: Expand each array to `AUDIO_PIPELINE_MAX_INPUTS * 2` (16) entries with meaningful defaults, e.g. `"Input 1L", "Input 1R", ..., "Input 8L", "Input 8R"`. Also update the MQTT HA discovery block, which still has hard-coded 4-label arrays, to dynamically build labels from the loop index rather than a fixed array.

---

### OOB-2: Preprocessor Directive Inside LOG_I Macro Argument

**Severity: High — preprocessor error; build fails for some toolchain versions**

- Issue: `src/hal/hal_dsp_bridge.cpp:46-52` passes `#ifndef NATIVE_TEST` / `#else` / `#endif` directives inside the argument list of `LOG_I()`. The C preprocessor processes directives before macro expansion, but placing a `#` directive mid-argument is undefined by the C standard and rejected by GCC with `-Werror`.
- Files: `src/hal/hal_dsp_bridge.cpp:45-53`
- Trigger: Any build with the `DAC_ENABLED` flag (firmware target).
- Impact: Compiler error or silent wrong-argument selection depending on toolchain.
- Fix approach: Hoist the conditional into a local variable before the `LOG_I` call:
  ```cpp
  void HalDspBridge::dumpConfig() {
  #ifndef NATIVE_TEST
      int dspEn = AppState::getInstance().dspEnabled;
  #else
      int dspEn = 0;
  #endif
      LOG_I("[HAL:DspBridge] DSP Pipeline bridge — dspEnabled=%d", dspEn);
  }
  ```

---

### BITMASK-1: `| true` Always Evaluates to `true`

**Severity: High — logic bug; ArduinoJson default values silently ignored**

- Issue: ArduinoJson v7 uses operator `|` for default values on `JsonVariant`. When the default is `true` (a non-zero integer), `anyValue | true` is always `true` regardless of what the JSON contains. This means the `linked` and `en` fields can never be `false` even when the client sends `false`.
- Files:
  - `src/websocket_handler.cpp:620` — `bool linked = doc["linked"] | true;`
  - `src/websocket_handler.cpp:693` — `bool en = doc["enabled"] | true;`
  - `src/websocket_handler.cpp:708` — `bool en = doc["enabled"] | true;`
  - `src/dsp_api.cpp:1413` — `bool linked = doc["linked"] | true;`
- Impact: `setDspStereoLink` WebSocket message and `POST /api/dsp/stereolink` REST endpoint cannot unlink stereo pairs (sending `"linked": false` is ignored; the pair stays linked). `setPeqBandEnabled` and `setPeqAllEnabled` cannot disable bands via those code paths.
- Fix approach: Use `doc["linked"].as<bool>()` with explicit presence check, or provide `false` as the default and rely on explicit payload validation: `bool linked = doc["linked"] | false;` is safe because `false` (zero) does not corrupt the result.

---

### UNINIT-1: `AppState::cachedReleaseList` — `isPrerelease` Bool Uninitialized

**Severity: Medium — cppcheck style/warning; bool reads undefined value until first OTA check**

- Issue: `AppState` has a private empty constructor `AppState() {}`. The inner struct `ReleaseInfo` at `src/app_state.h:142` contains `bool isPrerelease` with no in-class initializer. The `cachedReleaseList[OTA_MAX_RELEASES]` array member is therefore zero-initialized by the Meyer's singleton's static storage, but cppcheck flags it because the constructor provides no explicit initializer for the member.
- Files: `src/app_state.h:142-151`
- Impact: Low at runtime because static storage zero-initializes POD members. Risk is future refactoring moves `cachedReleaseList` off the singleton.
- Fix approach: Add `bool isPrerelease = false;` in-class initializer to the `ReleaseInfo` struct definition.

---

### MEMSET-1: `memset` on Structs Containing Floats and Function Pointers

**Severity: Low — style warning; technically non-portable but safe on ESP32/x86**

- Issue: Several structs containing `float` members and function pointers are zeroed with `memset`. IEEE 754 zero is all-bits-zero on all ESP32/x86 targets so the behavior is correct, but the C++ standard does not guarantee this.
- Files:
  - `src/audio_pipeline.cpp:900` — `memset(&_sinks[slot], 0, sizeof(AudioOutputSink))`
  - `src/dsp_pipeline.cpp:104,106` — `memset(&_mbSlots[i], 0, sizeof(DspMultibandSlot))`
  - `src/hal/hal_siggen.cpp:64` — `memset(&_source, 0, sizeof(AudioInputSource))`
  - `src/hal/hal_usb_audio.cpp:62` — `memset(&_source, 0, sizeof(AudioInputSource))`
  - `src/thd_measurement.cpp:29,41` — `memset(&_result, 0, sizeof(ThdResult))`
- Impact: No real-world impact on ESP32-P4. Suppressing cppcheck warnings here is acceptable with `// cppcheck-suppress memsetClassFloat`.
- Fix approach: Suppress with inline cppcheck directives, or replace with value-initialized struct assignment (`_result = ThdResult{}`). The former is lower risk on a time-critical audio path.

---

## Tech Debt

### ~~DEBT-1: DSP Swap Failure "Retry" Is a Lie~~ RESOLVED

- **Fixed:** All 42 external callsites across 4 files (`dsp_api.cpp`, `websocket_handler.cpp`, `mqtt_handler.cpp`, `scr_dsp.cpp`) replaced with `dsp_log_swap_failure()` helper. Double-counting bug eliminated — only `dsp_swap_config()` itself increments the failure counter. 15 HTTP endpoints now return 503 on swap failure instead of lying with 200 success. Log message changed from "staged for retry" to "change not applied". 2 new tests verify single-increment behavior.

### ~~DEBT-2: `dspGetInputLevel`/`dspGetOutputLevel` Hard-Coded to Lanes 0-1~~ RESOLVED

- **Fixed**: `dspGetInputLevel()` guard changed from `lane < 2` to `lane < AUDIO_PIPELINE_MAX_INPUTS`. `dspGetOutputLevel()` rewired to read actual per-sink VU metering via `audio_pipeline_get_sink()` with dBFS→linear conversion, replacing the stale input-data fallback. 16 new tests in `test/test_hal_dsp_bridge/`.

### DEBT-3: Legacy `dac_output_init()` Still Called in Audio Pipeline and Main Loop

- Issue: `src/audio_pipeline.cpp:599` and `src/main.cpp:1208` call `dac_output_init()` directly, bypassing the HAL framework lifecycle. The function is 200+ lines in `src/dac_hal.cpp` and maintains its own static `_settingsLoaded` / `_eepromScanned` flags.
- Files: `src/dac_hal.cpp:681`, `src/audio_pipeline.cpp:599`, `src/main.cpp:1208`
- Impact: Two parallel code paths manage DAC initialization (HAL lifecycle and direct call). State divergence is possible if both paths run, though static guards currently prevent double-init. New contributors are likely to miss this and break the invariant.
- Fix approach: Deferred from v3. Route all DAC init through the HAL manager; remove direct `dac_output_init()` calls.

### ~~DEBT-4: `AppState` Is a 553-Line God Object~~ RESOLVED (2026-03-09)

- **Fixed**: All 10 phases complete. AppState decomposed into 15 domain-specific state headers in `src/state/` (enums, general, ota, audio, dac, dsp, display, buzzer, siggen, usb_audio, wifi, mqtt, ethernet, debug, hal_coord). AppState reduced to ~80 lines (thin composition shell). All 1,947 references across 30 source files mechanically updated via multi-pass sed. Cross-core volatile semantics preserved on sub-struct field. All 1579 C++ tests + 26 E2E tests passing. Commit: `7d0f072`. Usage pattern: `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`, `appState.dac.es8311Enabled`, etc. Dirty flags + event signaling unchanged in AppState. Cross-task coordination flags (`_mqttReconfigPending`, `_pendingApToggle`) remain in AppState (inherently cross-cutting).

### DEBT-5: `websocket_handler.cpp` Is 2411 Lines

- Issue: `src/websocket_handler.cpp` handles WS authentication, binary audio streaming, all state broadcasts (25+ `send*()` functions), and all incoming WS command dispatch (~50 message types). It directly imports 15+ headers.
- Files: `src/websocket_handler.cpp`
- Impact: High coupling to every major subsystem. A single file modification touching DSP, audio, HAL, auth, or settings forces a full recompile of this translation unit.
- Fix approach: Extract inbound command dispatch into a separate `ws_commands.cpp` and outbound broadcast functions into `ws_broadcast.cpp`.

---

## Security Considerations

### SEC-1: Default AP Password Is a Known Weak Value

- Risk: `src/config.h:177` defines `DEFAULT_AP_PASSWORD "12345678"`. All devices ship with this password if the user has not changed it, and it is stored in plaintext in `appState.apPassword`.
- Files: `src/config.h:177`, `src/wifi_manager.cpp:382,387`
- Current mitigation: The web UI shows a warning that AP password should be changed.
- Recommendations: Generate a per-device random AP password at first boot (same approach used for `webPassword` — 10 chars, ~57-bit entropy), display it on the TFT, and persist it to NVS. This is noted as deferred in MEMORY.md.

### SEC-2: AP Password Logged at DEBUG Level

- Risk: `src/wifi_manager.cpp:387` logs `LOG_D("[WiFi] Password: %s", appState.apPassword)`. When debug level is set to `LOG_DEBUG`, this message is forwarded to all authenticated WebSocket clients via `DebugOut.broadcastLine()`.
- Files: `src/wifi_manager.cpp:387`
- Current mitigation: Only authenticated WS clients receive the log stream; `LOG_DEBUG` is not the default level.
- Recommendations: Redact the password: `LOG_D("[WiFi] Password: [%d chars]", appState.apPassword.length())`.

### SEC-3: Settings Export Includes Plain-Text Credentials

- Risk: `GET /api/settings/export` (auth-required) returns a JSON file containing `wifi.password` and `accessPoint.password` in plaintext at `src/settings_manager.cpp:1048-1054`. This file is downloaded by the browser and may be stored on disk, synced to cloud storage, or shared without realizing it contains credentials.
- Files: `src/settings_manager.cpp:1048-1054`
- Current mitigation: Endpoint requires session authentication.
- Recommendations: Omit `wifi.password` from the export response body (passwords can be re-entered on import). If backward compatibility requires including them, mask with a fixed marker (e.g., `"<unchanged>"`) that the importer recognizes.

### SEC-4: HTTP-Only Web UI (No TLS)

- Risk: The HTTP server runs on port 80 and WebSocket server on port 81, both unencrypted. Session cookies and WS auth tokens are transmitted in cleartext on the local network.
- Files: `src/main.cpp:90` (`WebServer server(80)`), `src/websocket_handler.h`
- Current mitigation: Noted as deferred in MEMORY.md. Session tokens use `HttpOnly` cookie flag and short 60s WS token TTL.
- Recommendations: TLS server support remains deferred. Acceptable for a local-network-only device in a home audio context.

---

## Performance Bottlenecks

### PERF-1: Static `JsonDocument` in `dsp_api.cpp` May Grow Unbounded

- Problem: `static JsonDocument _dspApiDoc` at `src/dsp_api.cpp:21` persists across HTTP requests. ArduinoJson v7 dynamic documents grow their pool on demand and never shrink unless `clear()` is called. All 20+ handler functions call `_dspApiDoc.clear()` before use, but the pool retains its high-watermark allocation.
- Files: `src/dsp_api.cpp:21`
- Cause: The DSP config GET handler serializes the full 4-channel DSP state (potentially 8KB+). The static doc retains that capacity for the lifetime of the device.
- Improvement path: Low priority. On PSRAM-equipped P4 boards the allocation lives in PSRAM. If heap pressure is detected, this could be replaced with a stack-allocated `JsonDocument` with a fixed capacity.

### PERF-2: `sendHardwareStats()` Runs Every 2 Seconds Unconditionally

- Problem: `sendHardwareStats()` in `src/websocket_handler.cpp` is called from the main loop every `HARDWARE_STATS_INTERVAL = 2000 ms` and broadcasts heap, CPU, ADC diagnostics, and task monitor data to all authenticated clients even when the web UI is not open.
- Files: `src/config.h:173`, `src/main.cpp` (broadcast dispatch)
- Cause: No check for whether any authenticated client is subscribed to hardware stats; `wsAnyClientAuthenticated()` gates all broadcasts but does not distinguish interest.
- Improvement path: Add a per-client "subscribeHardwareStats" flag similar to the existing `_audioSubscribed[]` pattern, or skip the broadcast when no clients have been authenticated for >30s.

---

## Fragile Areas

### FRAGILE-1: I2C Bus 0 (GPIO 48/54) SDIO Conflict

- Files: `src/hal/hal_discovery.cpp`, `src/dac_hal.cpp:706`
- Why fragile: GPIO 48/54 are shared between the expansion I2C bus and the ESP32-C6 WiFi SDIO interface. Any I2C transaction on Bus 0 while WiFi is active causes `sdmmc_send_cmd` errors and can trigger an MCU reset. The HAL discovery correctly skips Bus 0 when WiFi is active, but `dac_output_init()` has its own static `_eepromScanned` flag that prevents re-scanning — if the scan was skipped on first boot (WiFi already connected), the DAC EEPROM on Bus 0 is never probed.
- Safe modification: Never call `Wire1.begin(48, 54)` or any I2C operation on Bus 0 after `WiFi.begin()`. Any future code adding Bus 0 I2C must check `appState.wifiConnecting || WiFi.isConnected()`.
- Test coverage: No native test exercises the SDIO conflict guard path.

### FRAGILE-2: `vTaskSuspendAll()` Used for Pipeline Atomicity

- Files: `src/audio_pipeline.cpp:740,752,827,873,898`
- Why fragile: `vTaskSuspendAll()` suspends ALL tasks on the current core, including the FreeRTOS idle task. Any call inside the critical window that blocks (malloc, I2C, UART) will deadlock. Currently the window only calls `memset` and pointer assignments, which is safe, but any future addition to `audio_pipeline_set_sink()` or `audio_pipeline_remove_sink()` that calls an external API would silently deadlock.
- Safe modification: Keep `vTaskSuspendAll()` windows to pure in-SRAM pointer assignments and `memset` of already-allocated buffers. Do not add logging, memory allocation, or any OS call inside these windows.
- Test coverage: `test_audio_pipeline` covers slot lifecycle but runs with the `#ifndef NATIVE_TEST` guards removing `vTaskSuspendAll()` calls entirely.

### FRAGILE-3: MQTT Callback Must Not Call WiFi/LittleFS/WebSocket

- Files: `src/mqtt_handler.cpp` (`mqttCallback()`)
- Why fragile: `mqttCallback()` runs inside `mqttClient.loop()` on the `mqtt_task` (Core 0). Direct WebSocket, LittleFS, or WiFi calls from this context race with the main loop's HTTP server. Currently the callback only sets dirty flags, which is correct. Any future refactoring that adds direct I/O (e.g., saving MQTT-received settings directly) would introduce a race.
- Safe modification: All side-effects in `mqttCallback()` must go through dirty flags or `_pendingApToggle`-style coordination variables. Never call `webSocket.broadcastTXT()`, `LittleFS.open()`, or `WiFi.*` from the MQTT callback.

### FRAGILE-4: DSP Config Swap Race Window

- Files: `src/dsp_pipeline.cpp:411` (`dsp_swap_config()`), `src/audio_pipeline.cpp:105`
- Why fragile: The double-buffer swap uses a `_swapRequested` volatile flag. The audio task must observe the flag within one DMA buffer period (≤10ms at 48kHz/512 frames). If the audio task is preempted by a higher-priority interrupt for >10ms, a second swap request from the web UI could overwrite the inactive buffer before the first swap completes, losing the first config change silently. The comment at `src/audio_pipeline.cpp:105` acknowledges this.
- Test coverage: `test_dsp_swap` exercises the swap lifecycle but cannot reproduce the multi-swap race in native testing.

---

## Scaling Limits

### SCALE-1: HAL Device Cap at 24

- Current capacity: `HAL_MAX_DEVICES = 24` (raised from 16 in v3 Phase 3). Builtin devices consume 8-9 slots at boot (PCM5102A, ES8311, PCM1808 ×2, NS4150B, TempSensor, SigGen, USB Audio, DSP Bridge = 9).
- Limit: 15 user-added expansion devices maximum before `halManager.registerDevice()` returns `-1`. Adding a 4-bus I2C expander with many attached devices could approach this limit.
- Scaling path: `HAL_MAX_DEVICES` is a compile-time constant in `src/hal/hal_types.h:9`. Increasing it increases the size of four fixed arrays in `HalDeviceManager`. On P4 with PSRAM this is not a memory concern; increase to 32 if needed.

### SCALE-2: Event Group Bit Exhaustion

- Current capacity: `EVT_ANY = 0x00FFFFFF` — 24 usable bits, 16 assigned (bits 0–15), 8 spare.
- Limit: FreeRTOS reserves bits 24-31. Once all 24 bits are assigned, new subsystems cannot use event-driven wake without restructuring.
- Scaling path: Coalesce related events (e.g., all HAL events into one `EVT_HAL_CHANGE` bit with a queue for specifics), freeing bits for new subsystems.

---

## Dependencies at Risk

### DEP-1: No Platform Archive Mirror for OTA Firmware Downloads

- Risk: OTA downloads pull firmware binaries from `objects.githubusercontent.com`. If GitHub CDN is unreachable (outage, DNS failure, network policy), OTA updates silently fail. There is no fallback mirror.
- Impact: Devices in restricted network environments (corporate WiFi, some ISPs) cannot update.
- Current mitigation: OTA failure is non-fatal; device continues running current firmware. Error is logged and broadcast.
- Migration plan: Deferred from MEMORY.md. Add a configurable `otaMirrorUrl` to settings; OTA check task tries mirror on GitHub failure.

### DEP-2: WebSockets Library Vendored in `lib/WebSockets/`

- Risk: The WebSockets library was vendored locally (commit `46887d8`) to fix native build issues. It is now pinned at a specific version with no automatic update path. Security patches or ESP32 compatibility fixes upstream will not reach the firmware without a manual vendor update.
- Files: `lib/WebSockets/`
- Migration plan: Track the upstream repo; manually update `lib/WebSockets/` when security-relevant releases are made.

---

## Test Coverage Gaps

### TCOV-1: Input Name OOB Not Tested

- What's not tested: The loop in `src/settings_manager.cpp:548` that uses `INPUT_NAME_DEFAULTS[i]` with `i` up to 15 is never exercised in native tests with `AUDIO_PIPELINE_MAX_INPUTS = 8`. Test files that define this macro use `#define AUDIO_PIPELINE_MAX_INPUTS 2`, which also masks the OOB.
- Files: `src/settings_manager.cpp:539-563`, `test/test_audio_diagnostics/test_audio_diagnostics.cpp:46`
- Risk: The actual firmware build with `AUDIO_PIPELINE_MAX_INPUTS = 8` reads past the array end on first boot, silently using garbage defaults for input names 5-16.
- Priority: High — fix the array size first (OOB-1), then add a test.

### TCOV-2: DSP Swap Failure Handling Not Tested

- What's not tested: No test verifies that `dsp_swap_config()` failure is handled gracefully. The "staged for retry" path (which does not actually retry) is never exercised.
- Files: `src/dsp_api.cpp` (17 callsites), `test/test_dsp_swap/`
- Risk: Silent data loss on swap contention goes undetected.
- Priority: Medium.

### TCOV-3: MQTT HA Discovery Input Labels Loop Not Tested

- What's not tested: The `for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++)` loop in `src/mqtt_ha_discovery.cpp:1416` is not covered by any test in `test/test_mqtt/`.
- Files: `src/mqtt_ha_discovery.cpp:1412-1428`
- Risk: OOB access in HA discovery (see OOB-1) will not be caught by native tests.
- Priority: High — tied to OOB-1 fix.

### TCOV-4: Security Cookie/Token Path Not E2E Tested

- What's not tested: The WS token short-TTL expiry (60s), the 16-slot pool exhaustion path, and the `Retry-After` HTTP 429 response on login rate limiting are not verified by existing Playwright tests in `e2e/tests/`.
- Files: `src/auth_handler.cpp`, `e2e/tests/auth.spec.js`
- Risk: Regression in auth edge cases goes undetected.
- Priority: Low — security path, not a user-facing regression risk.

---

*Concerns audit: 2026-03-08 | DEBT-4 resolved: 2026-03-09*
