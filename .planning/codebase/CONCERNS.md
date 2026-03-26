# Concerns & Technical Debt

**Analysis Date:** 2026-03-25

## Critical Issues (blocking or high-risk)

### HAL Capability Flags Full (uint16_t saturated)

**Status: RESOLVED (2026-03-25)** — Widened to uint32_t, 16 spare bits available.

- **Severity:** Critical
- **Issue:** All 16 bits of the `uint16_t capabilities` field are now assigned (bits 0-15: HW_VOLUME, FILTERS, MUTE, ADC_PATH, DAC_PATH, PGA_CONTROL, HPF_CONTROL, CODEC, MQA, LINE_DRIVER, APLL, DSD, HP_AMP, POWER_MGMT, ASRC, DPLL). The next capability flag requires widening to `uint32_t`.
- **Files:**
  - `src/hal/hal_types.h` (line 136: `uint16_t capabilities`)
  - `src/hal/hal_cirrus_dac_2ch.h` (line 50)
  - `src/hal/hal_ess_adc_2ch.h` (line 71)
  - `src/hal/hal_ess_adc_4ch.h` (line 74)
  - `src/hal/hal_ess_dac_2ch.h` (line 59)
  - `src/hal/hal_ess_dac_8ch.h` (line 44)
  - `src/hal/hal_pipeline_bridge.cpp` (line 82)
  - `src/health_check.cpp` (line 629)
- **Impact:** Any new feature requiring a capability flag (e.g., hardware EQ, clock output control, DRC support) is blocked until the type is widened. Widening touches `HalDeviceDescriptor`, all 5 driver descriptor structs, NVS serialization, WS/REST JSON payloads, and web UI JavaScript bitmask constants.
- **Fix approach:** Change `uint16_t capabilities` to `uint32_t` in `HalDeviceDescriptor` and all 5 per-driver descriptor structs. Update `hal_init_descriptor()` parameter type. Verify WS/MQTT JSON serialization (ArduinoJson handles uint32_t natively). Update web UI `HAL_CAP_*` JS constants in `web_src/js/15-hal-devices.js`. Add `static_assert(sizeof(HalDeviceDescriptor::capabilities) >= 4)` guard.

### ASRC Fixed-Frame Pipeline Mismatch

**Status: RESOLVED (2026-03-25)** — Lane buffers resized to ASRC_OUTPUT_FRAMES_MAX, ASRC return value captured, downsampled tails zero-filled.

- **Severity:** High
- **Issue:** The ASRC engine produces a variable number of output frames (e.g., 256 input at 44.1kHz produces 279 output frames at 48kHz), but the pipeline operates on a fixed `FRAMES` (256) count downstream. For upsampling, extra samples are processed; for downsampling, the buffer tail retains stale audio data that leaks through DSP and matrix stages. Additionally, lane buffers were allocated at FRAMES (256) not ASRC_OUTPUT_FRAMES_MAX (280), causing heap overflow on upsampling.
- **Files:**
  - `src/audio_pipeline.cpp` (line 415: explicit `TODO (v2)` comment)
  - `src/asrc.h` (line 99: `ASRC_OUTPUT_FRAMES_MAX 280`)
- **Impact:** Downsampled lanes (e.g., 96kHz to 48kHz = 128 output frames from 256 input) will have 128 stale samples in the buffer tail processed by DSP biquads and routed through the matrix. Audible artifacts possible in edge cases. Upsampling case is safe but wastes ~9% of DSP computation on extra frames that get truncated at the I2S write stage.
- **Fix approach:** Propagate actual output frame count from `asrc_process_lane()` return value through DSP and matrix stages. Requires changing `pipeline_run_dsp()` and `pipeline_run_matrix()` to accept per-lane frame counts. Moderate complexity.

### removeDevice() Memory Leak

**Status: RESOLVED (2026-03-25)** — Ownership tracking added, removeDevice() now deinits + deletes owned devices.

- **Severity:** High
- **Issue:** `HalDeviceManager::removeDevice()` sets `_devices[slot] = nullptr` without calling `delete` on the device pointer. The device object (heap-allocated via `new` in factory functions) is leaked. Some callers (`hal_api.cpp`, `hal_custom_device.cpp`) call `dev->deinit()` before `removeDevice()`, but none call `delete dev`. The `hal_discovery.cpp` rescan path also calls `removeDevice()` without `delete`.
- **Files:**
  - `src/hal/hal_device_manager.cpp` (lines 81-97: `removeDevice()`)
  - `src/hal/hal_api.cpp` (line 411: `mgr.removeDevice(slot)` without delete)
  - `src/hal/hal_custom_device.cpp` (line 440: `mgr.removeDevice(i)` without delete)
  - `src/hal/hal_discovery.cpp` (line 348: `mgr.removeDevice(i)` without delete)
  - `src/hal/hal_builtin_devices.cpp` (lines 44-80: all factory functions use `new`)
- **Impact:** Each removed device leaks ~200-500 bytes (HalDevice subclass + vtable). On hot-plug expansion boards with rescan cycles, memory pressure accumulates. Builtin devices are never removed in practice, but expansion devices are.
- **Fix approach:** Either (a) have `removeDevice()` call `delete _devices[slot]` before nulling, or (b) return the pointer and require callers to delete. Option (a) is simpler and matches the factory ownership pattern.

## Technical Debt

### Redundant Constant Definitions Across Headers

- **Severity:** Medium
- **Issue:** `AUDIO_PIPELINE_MAX_INPUTS` is `#define`d in 4 separate files (`src/config.h`, `src/audio_pipeline.h`, `src/audio_input_source.h`, `src/i2s_audio.h`) all guarded by `#ifndef`. `AUDIO_PIPELINE_MATRIX_SIZE` is defined as 32 in `src/config.h` but has a fallback of 16 in `src/audio_pipeline.h`. `AUDIO_PIPELINE_MAX_OUTPUTS` is 16 in `src/config.h` but 8 in `src/audio_pipeline.h`. Include order determines which value wins.
- **Files:**
  - `src/config.h` (lines 254-257: authoritative definitions)
  - `src/audio_pipeline.h` (lines 8-16: fallback definitions with different values)
  - `src/audio_input_source.h` (line 18)
  - `src/i2s_audio.h` (line 8)
- **Impact:** If any file includes `audio_pipeline.h` before `config.h`, it gets MATRIX_SIZE=16 instead of 32 and MAX_OUTPUTS=8 instead of 16. Currently works because `config.h` is included transitively via `app_state.h` early in most translation units, but a new `.cpp` file that includes only `audio_pipeline.h` would silently get wrong values. Could cause matrix routing bugs or array bounds issues.
- **Fix approach:** Remove all fallback definitions from non-config headers. Define constants only in `src/config.h`. Add `#error` guards in dependent headers if constants are not defined.

### Raw `server.send()` Bypassing Security Headers (16 calls)

- **Severity:** Medium
- **Issue:** 16 `server.send()` calls in `src/main.cpp` bypass the `server_send()` wrapper from `src/http_security.h`, meaning those responses lack `X-Frame-Options: DENY` and `X-Content-Type-Options: nosniff` headers. These are mostly 404 responses for browser auto-requests (favicon, robots.txt, manifest.json) and captive portal redirects.
- **Files:**
  - `src/main.cpp` (lines 429-441: 404 handlers; lines 446-452: redirect handlers; lines 651-739: I2S port API)
- **Impact:** Low security risk for 404/redirect responses, but inconsistent header policy creates maintenance confusion. The I2S port API responses at lines 651-739 are more concerning as they return JSON with device state data.
- **Fix approach:** Replace all `server.send()` calls in `main.cpp` with `server_send()`. Grep codebase for remaining `server.send(` outside `http_security.h` and fix. Consider adding a pre-commit check.

### Content-Security-Policy Header Not Implemented

- **Severity:** Medium
- **Issue:** The previous CONCERNS.md marked CSP as "RESOLVED" but no `Content-Security-Policy` header exists anywhere in the codebase. `http_add_security_headers()` in `src/http_security.h` only sets `X-Frame-Options` and `X-Content-Type-Options`.
- **Files:**
  - `src/http_security.h` (lines 9-12: only two headers set)
- **Impact:** Without CSP, the web UI is vulnerable to XSS via injected inline scripts if an attacker can control any part of the page content (e.g., device names, SSID names rendered via `innerHTML`). The `escapeHtml()` function in `web_src/js/01-core.js` mitigates this for most paths, but 76 `innerHTML` assignments exist across JS files, not all of which use `escapeHtml()`.
- **Fix approach:** Add `Content-Security-Policy: default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self' ws: wss:; frame-src 'none'; object-src 'none'` to `http_add_security_headers()`. The `unsafe-inline` for script/style is necessary because the web UI uses inline JS/CSS in the monolithic HTML page.

### Power Management API Without Driver Implementations

- **Severity:** Low
- **Issue:** The `HAL_CAP_POWER_MGMT` capability flag (bit 13), REST endpoint (`PUT /api/hal/devices/power`), WS broadcast, and web UI power management controls all exist. However, no driver overrides `enterStandby()`, `wake()`, `powerOff()`, or `powerOn()` -- all use the base class stubs that return `false`. The web UI shows power management controls for devices that claim the capability but transitions always fail silently.
- **Files:**
  - `src/hal/hal_device.h` (lines 96-99: default stubs returning false)
  - `src/hal/hal_api.cpp` (lines 490-540: REST endpoint)
  - No driver `.cpp` files override these methods
- **Impact:** No functional impact (all transitions fail gracefully with error response). But the API surface and UI controls create a false impression of functionality. Currently no device's descriptor sets `HAL_CAP_POWER_MGMT` in its capabilities bitmask, so the controls are hidden.
- **Fix approach:** Either implement power management for ESS/Cirrus DACs (they support standby registers), or remove `HAL_CAP_POWER_MGMT` from the capability flags to free bit 13 for a real feature. The latter would also free a capability bit, easing the uint16_t saturation issue.

### Arduino String Usage in Cross-Core State (Heap Fragmentation Risk)

- **Severity:** Medium
- **Issue:** `AudioState::inputNames` uses `String[16]` (Arduino heap-allocated strings) in a struct accessed from both Core 0 (main loop) and Core 1 (audio task). Ethernet, OTA, and error states also use `String`. Arduino `String` objects cause heap fragmentation over time due to repeated alloc/free cycles and are not thread-safe for concurrent access.
- **Files:**
  - `src/state/audio_state.h` (line 74: `String inputNames[AUDIO_PIPELINE_MAX_INPUTS * 2]`)
  - `src/state/ethernet_state.h` (lines 11-27: 9 String fields)
  - `src/app_state.h` (line 209: `String errorMessage`)
- **Impact:** On a long-running device, String operations fragment the internal SRAM heap. With only ~40KB reserved for WiFi RX buffers, fragmentation can cause WiFi packet drops. The cross-core access pattern on `inputNames` (written by main loop, potentially read by audio diagnostics) lacks memory barriers.
- **Fix approach:** Replace `String` fields in state structs with fixed-size `char[]` arrays. `inputNames` can be `char[16][32]`. Ethernet fields can be `char[16]` (IP) or `char[18]` (MAC). This eliminates heap fragmentation from state management.

### Stale Worktree Directories (22MB disk waste)

- **Severity:** Low
- **Issue:** Two git worktree directories remain under `.claude/worktrees/`: `fix-matrix-routing-bounds` and `phase2-asrc-dsd`, totaling 22MB. These are leftover from completed work and should be cleaned up.
- **Files:**
  - `.claude/worktrees/fix-matrix-routing-bounds/`
  - `.claude/worktrees/phase2-asrc-dsd/`
- **Impact:** Disk waste only. No functional impact.
- **Fix approach:** `git worktree remove .claude/worktrees/fix-matrix-routing-bounds && git worktree remove .claude/worktrees/phase2-asrc-dsd`

## Security Concerns

### innerHTML Usage Without Consistent Sanitization

- **Severity:** Medium
- **Issue:** 76 `innerHTML` assignments exist across web UI JS files. While `escapeHtml()` is available and used in some paths (e.g., device names in `05-audio-tab.js`), many `innerHTML` assignments in `15-hal-devices.js`, `06-peq-overlay.js`, and `08-ui-status.js` construct HTML strings from device data without escaping all interpolated values.
- **Files:**
  - `web_src/js/15-hal-devices.js` (line 911: `preview.innerHTML` with `escapeHtml()` but other paths without)
  - `web_src/js/06-peq-overlay.js` (lines 125, 177, 444, 498, 552)
  - `web_src/js/05-audio-tab.js` (lines 129, 219, 289, 308)
  - `web_src/js/08-ui-status.js` (line 124)
- **Impact:** If an attacker can set a device name, SSID, or other user-configurable string to contain `<script>` or event handler attributes, the web UI could execute arbitrary JavaScript. The attack surface is limited (requires local network access + MQTT/API access to set device names), but should be defense-in-depth hardened.
- **Fix approach:** Audit all `innerHTML` assignments. Use `textContent` for plain text, `escapeHtml()` for all interpolated values in HTML strings. Add ESLint rule to warn on `innerHTML` assignments.

### No HTTPS/WSS Support on Device

- **Severity:** Low
- **Issue:** The ESP32-P4 web server runs HTTP only (port 80) and WebSocket on port 81 (ws://). All local network traffic including auth tokens, session cookies, and audio configuration is transmitted in plaintext. The `HttpOnly` cookie flag and token-based WS auth mitigate some risks, but network observers can sniff credentials.
- **Files:**
  - `src/main.cpp` (WebServer on port 80)
  - `src/websocket_command.cpp` (WS on port 81)
  - `web_src/js/01-core.js` (line 126-128: constructs ws:// URL)
- **Impact:** Low for typical home network use. The device is not internet-facing. MQTT TLS is already supported for external broker connections. Local HTTPS would require certificate management (self-signed or Let's Encrypt) which is complex for embedded devices.
- **Fix approach:** Deferred. Document as a known limitation. For enterprise deployments, recommend placing behind a reverse proxy with TLS termination.

### sprintf Without Bounds Checking (OTA module)

- **Severity:** Low
- **Issue:** Two `sprintf()` calls in `src/ota_updater.cpp` (lines 741, 926) write hex digest characters without bounds checking. The buffers are `char str[3]` which is sufficient for `"%02x"` output, so this is not currently exploitable, but violates the project convention of using `snprintf()` everywhere else.
- **Files:**
  - `src/ota_updater.cpp` (lines 741, 926)
- **Impact:** No current vulnerability. Theoretical risk if buffer size or format string changes.
- **Fix approach:** Replace `sprintf(str, "%02x", ...)` with `snprintf(str, sizeof(str), "%02x", ...)`.

## Performance Risks

### ASRC Coefficient Table (20KB PSRAM at boot)

**Status: RESOLVED (2026-03-26)** — Lazy allocation: coefficients + history buffers deferred to first non-1:1 `asrc_set_ratio()` call. Boot no longer allocates 20KB PSRAM.

- **Severity:** Low
- **Issue:** The ASRC polyphase filter coefficient table (160 phases x 32 taps x 4 bytes = 20KB) is computed at boot time using `psram_alloc()` and held permanently. Per-lane history buffers add 256 bytes each (2KB for all 8 lanes). Total: ~22KB PSRAM permanently allocated even when no sample rate conversion is active.
- **Files:**
  - `src/asrc.cpp` (lines 70, 184: coefficient allocation; lines 195-196: per-lane history)
- **Impact:** Minor PSRAM pressure. The ESP32-P4 has 32MB PSRAM, so 22KB is negligible. However, the coefficient computation at boot adds ~50ms to startup.
- **Fix approach:** Consider lazy allocation (allocate on first `asrc_set_ratio()` with non-equal rates). Low priority given ample PSRAM.

### DSD Mode Switching Uses String Comparison

**Status: RESOLVED (2026-03-26)** — Virtual `setDsdMode()`/`isDsdMode()` added to `HalAudioDevice` base class. `strncmp` + `static_cast<HalCirrusDac2ch*>` removed from diagnostics_loop.cpp.

- **Severity:** Low
- **Issue:** The DSD DAC mode switching logic in `main.cpp` identifies Cirrus DACs using `strncmp(desc.compatible, "cirrus,", 7)` combined with `HAL_CAP_DSD` check, then performs `static_cast<HalCirrusDac2ch*>(dev)`. This pattern is fragile -- any future DSD-capable non-Cirrus DAC (e.g., ESS SABRE with DoP support) would not be handled, and a miscategorized device could cause undefined behavior via the `static_cast`.
- **Files:**
  - `src/main.cpp` (lines 1371-1377: DSD mode switching)
- **Impact:** Currently safe because only Cirrus DACs have `HAL_CAP_DSD`. Future ESS DACs with DSD support would need the same pattern extended with "ess," prefix check, creating a maintenance burden.
- **Fix approach:** Add a virtual `setDsdMode(bool)` method to `HalAudioDevice` base class with default no-op. Override in Cirrus drivers. Call via `HalAudioDevice*` pointer without `static_cast`. Eliminates string matching entirely.

### 16 Dirty Flags Without Atomic Access

**Status: RESOLVED (2026-03-26)** — All 16 dirty flags marked `volatile bool` for consistency with existing cross-core volatile pattern.

- **Severity:** Low
- **Issue:** AppState has 16 `bool` dirty flags (non-volatile, non-atomic) written by various tasks (MQTT on Core 0, audio on Core 1, HTTP handlers) and read by the main loop. While bool writes are naturally atomic on RISC-V, the compiler may optimize reads away without `volatile`. The `_mqttReconfigPending` and `_pendingApToggle` fields are correctly marked `volatile`, but the dirty flags are not.
- **Files:**
  - `src/app_state.h` (lines 229-250: 16 dirty flag declarations)
- **Impact:** Theoretically, the compiler could cache a dirty flag read and miss a set from another task. In practice, the event group `app_events_wait()` provides a memory fence that makes this unlikely. Low risk but inconsistent with the project's otherwise careful cross-core access patterns.
- **Fix approach:** Mark all dirty flags `volatile bool` or use `std::atomic<bool>`. Alternatively, document that `app_events_signal()` serves as the memory barrier.

## Scalability Limitations

### HAL Driver Registry: 44/48 Slots Used

**Status: RESOLVED (2026-03-25)** — Increased to 64 slots (44/64 used).

- **Severity:** High
- **Issue:** 44 of 48 `HAL_MAX_DRIVERS` slots are occupied. Only 4 slots remain for new device drivers. Each new expansion DAC/ADC/codec chip requires a driver registration.
- **Files:**
  - `src/hal/hal_builtin_devices.cpp` (line 204: `HAL_BUILTIN_DRIVER_COUNT 44`)
  - `src/hal/hal_types.h` (line 12: `HAL_MAX_DRIVERS 48`)
  - `src/hal/hal_device_db.h` (line 9: `HAL_DB_MAX_ENTRIES 48`)
- **Impact:** Adding 5+ new device drivers requires increasing `HAL_MAX_DRIVERS` and `HAL_DB_MAX_ENTRIES`. Each slot uses ~70 bytes (HalDriverEntry) + ~140 bytes (HalDeviceDescriptor), so increasing to 64 adds ~3.3KB BSS.
- **Fix approach:** Increase `HAL_MAX_DRIVERS` and `HAL_DB_MAX_ENTRIES` to 64 preemptively. Update `HAL_BUILTIN_DRIVER_COUNT` static_assert.

### HAL Device Slots: 14/32 Used at Boot

- **Severity:** Low
- **Issue:** 14 onboard devices registered at boot. Expansion adds 2 per mezzanine (ADC+DAC). Maximum simultaneous expansion: 9 mezzanines = 18 expansion + 14 onboard = 32 (exactly at limit). In practice, more than 4-5 mezzanines are unrealistic.
- **Files:**
  - `src/hal/hal_types.h` (line 9: `HAL_MAX_DEVICES 32`)
  - `src/hal/hal_device_manager.cpp` (line 51: registration check)
- **Impact:** Comfortable headroom for foreseeable use cases. No immediate action needed.
- **Fix approach:** Monitor. If custom devices + expansion exceed 24, bump to 48.

### Event Group Bits: 17/24 Used

- **Severity:** Low
- **Issue:** 17 of 24 usable FreeRTOS event group bits are assigned (bits 0-18 with bits 5 and 13 freed). 7 spare bits remain (5, 13, 19-23). Bits 24-31 are reserved by FreeRTOS.
- **Files:**
  - `src/app_events.h` (all EVT_* definitions)
- **Impact:** Sufficient for ~7 more features. If exhausted, would need a second event group or alternative signaling mechanism.
- **Fix approach:** Monitor. Consider consolidating low-frequency events if approaching limit.

## Maintenance Burden

### Generated Web Pages Monolith (22,751 LOC)

- **Severity:** Medium
- **Issue:** `src/web_pages.cpp` (14,961 lines) and `src/web_pages_gz.cpp` (7,790 lines) are auto-generated from `web_src/`. Any change to the web UI requires running `node tools/build_web_assets.js`, which regenerates the entire monolith. IDE navigation, syntax highlighting, and search are impacted by these large files.
- **Files:**
  - `src/web_pages.cpp` (14,961 LOC)
  - `src/web_pages_gz.cpp` (7,790 LOC)
  - `web_src/js/` (25 JS files, 8,756 LOC total)
  - `web_src/css/` (CSS files)
  - `web_src/index.html` (HTML template)
  - `tools/build_web_assets.js` (build script)
- **Impact:** Pre-commit hook and CI guard prevent direct edits. The build tool is well-documented. Main concern is compile time -- 22K lines of PROGMEM string literals slow down incremental builds.
- **Fix approach:** Already mitigated by CI guard and pre-commit hook. Consider splitting into chunked PROGMEM arrays if compile time becomes problematic.

### main.cpp Complexity (1,547 LOC, mixed concerns)

**Status: RESOLVED (2026-03-26)** — Route registration extracted to `src/routes.cpp` (414 LOC). Timer-based diagnostics extracted to `src/diagnostics_loop.cpp` (254 LOC). main.cpp reduced to ~952 LOC.

- **Severity:** Medium
- **Issue:** `src/main.cpp` contains `setup()`, `loop()`, route registration, format negotiation, DSD mode switching, diagnostics rules, and LED blink logic. The `loop()` function alone handles 15+ dirty flag checks, format negotiation, ASRC configuration, DSD switching, clipping detection, and timer management.
- **Files:**
  - `src/main.cpp` (1,547 lines)
- **Impact:** Adding new main-loop features requires editing a large file with many interleaved concerns. Risk of unintended interaction between timer-based checks. Reviewer fatigue on PRs touching this file.
- **Fix approach:** Extract timer-based diagnostic rules (clipping, format negotiation, DSD switching) into a `src/diagnostics_loop.cpp` module. Extract route registration into `src/routes.cpp`. Keep `setup()` and core `loop()` dispatch in `main.cpp`.

### static_cast Without RTTI Guards (10 occurrences)

- **Severity:** Medium
- **Issue:** 10 `static_cast<HalXxx*>` calls across the codebase cast `HalDevice*` to specific subclasses without runtime type verification (RTTI is disabled via `-fno-rtti`). Each relies on either capability flag checks or type/compatible string comparisons as a proxy for type safety.
- **Files:**
  - `src/main.cpp` (line 1376: `static_cast<HalCirrusDac2ch*>`)
  - `src/main.cpp` (line 961: `static_cast<HalLed*>`)
  - `src/smart_sensing.cpp` (line 332: `static_cast<HalRelay*>`)
  - `src/hal/hal_pipeline_bridge.cpp` (line 213: `static_cast<HalNs4150b*>`)
  - `src/gui/gui_input.cpp` (line 193: `static_cast<HalEncoder*>`)
  - `src/websocket_broadcast.cpp` (line 50: `static_cast<HalAudioDevice*>`)
- **Impact:** If the type guard (capability check, device type check) ever becomes inconsistent with the actual object type, the `static_cast` produces undefined behavior. Most are well-guarded, but the DSD mode string check (`strncmp("cirrus,", 7)`) is the most fragile.
- **Fix approach:** Add virtual methods to base classes where possible (e.g., `setDsdMode()` on `HalAudioDevice`). For remaining cases, document the type guard invariant next to each `static_cast`.

## Missing Features / Gaps

### No Watchdog Recovery for Audio Pipeline Task

- **Severity:** Medium
- **Issue:** The audio pipeline task (`audio_pipeline_task`, Core 1, priority 3) has no explicit watchdog enrollment. If the task hangs (e.g., I2S read blocks indefinitely due to hardware fault), no recovery mechanism exists. The ESP-IDF task watchdog monitors the idle task but not custom high-priority tasks.
- **Files:**
  - `src/audio_pipeline.cpp` (line 939: `xTaskCreatePinnedToCore`)
  - `src/main.cpp` (line 182: comment about `esp_task_wdt_delete` issues)
- **Impact:** A hung audio task would silently stop audio processing. The health check would eventually flag the issue, but no automatic restart occurs.
- **Fix approach:** Register the audio task with `esp_task_wdt_add()` and add periodic `esp_task_wdt_reset()` in the audio loop. On watchdog trigger, use `audio_pipeline_request_pause()`/`resume()` to restart. Note the comment in main.cpp about `esp_task_wdt_delete` causing list corruption in IDF5.5 -- investigate if this is resolved.

### No Integration Test for ASRC + DSP + Matrix Chain

- **Severity:** Medium
- **Issue:** The ASRC engine, DSP pipeline, and routing matrix are each unit tested independently, but no integration test verifies the full chain: ASRC resampling -> DSP biquad processing -> matrix routing -> output sink. The ASRC TODO about variable frame count propagation means the interaction between these stages has untested edge cases.
- **Files:**
  - `test/test_asrc/` (ASRC unit tests)
  - `test/test_dsp_pipeline/` (DSP unit tests)
  - `test/test_pipeline_bounds/` (matrix bounds tests)
- **Impact:** The fixed-frame mismatch (ASRC produces variable frames, pipeline processes fixed 256) could cause audible artifacts that only surface in the integrated path, not in isolated unit tests.
- **Fix approach:** Add `test/test_pipeline_integration/` that chains ASRC -> DSP -> matrix with known test signals at 44.1kHz -> 48kHz. Verify output frame count and signal integrity.

### No Graceful Degradation for PSRAM Failure

- **Severity:** Low
- **Issue:** If PSRAM fails at boot or becomes unreliable, the firmware attempts SRAM fallback (capped at 64KB via `psram_alloc()`), but many subsystems (ASRC, DSP FIR, LVGL GUI, audio float buffers) require more PSRAM than the SRAM fallback cap allows. A complete PSRAM failure would leave the device in a degraded but unpredictable state.
- **Files:**
  - `src/psram_alloc.h` / `src/psram_alloc.cpp` (fallback logic)
  - `src/asrc.cpp` (20KB coefficient table)
  - `src/audio_pipeline.cpp` (multiple PSRAM allocations)
- **Impact:** Rare hardware failure scenario. Diagnostic emission (`DIAG_SYS_PSRAM_ALLOC_FAIL`) surfaces the issue, but no automatic mode downgrade (e.g., disable ASRC and FIR if PSRAM unavailable).
- **Fix approach:** Add a "PSRAM degraded" mode that disables ASRC, FIR convolution, and GUI if PSRAM allocation fails during init. Check `psram_get_stats().failedCount` after init and enter reduced-capability mode.

## Recommendations (prioritized)

1. **[Critical] ~~Widen capability flags to uint32_t~~** -- **RESOLVED (2026-03-25).** Widened to uint32_t, 16 spare bits available.

2. **[High] ~~Fix removeDevice() memory leak~~** -- **RESOLVED (2026-03-25).** Ownership tracking via `takeOwnership` parameter; removeDevice() deinits + deletes owned devices.

3. **[High] ~~Increase HAL_MAX_DRIVERS to 64~~** -- **RESOLVED (2026-03-25).** Increased HAL_MAX_DRIVERS and HAL_DB_MAX_ENTRIES to 64 (44/64 used).

4. **[High] ~~Fix ASRC frame count propagation~~** -- **RESOLVED (2026-03-25).** Lane buffers resized from FRAMES to ASRC_OUTPUT_FRAMES_MAX (fixes heap overflow on upsampling). ASRC return value captured per-lane. Downsampled buffer tails zero-filled (fixes stale data leaking through DSP/matrix). 6 new ASRC frame count tests.

5. **[Medium] ~~Implement Content-Security-Policy header~~** -- **RESOLVED (2026-03-25).** CSP with 9 directives added to http_add_security_headers(). 9 new CSP tests.

6. **[Medium] Consolidate constant definitions** -- Remove redundant `#define` guards from `audio_pipeline.h`, `audio_input_source.h`, `i2s_audio.h`. Define only in `config.h`. Estimated effort: 1 hour.

7. **[Medium] ~~Migrate raw server.send() to server_send()~~** -- **RESOLVED (2026-03-25).** All raw calls migrated to wrappers.

8. **[Medium] ~~Add virtual setDsdMode() to HalAudioDevice~~** -- **RESOLVED (2026-03-26).** Virtual dispatch added; strncmp + static_cast removed.

9. **[Low] Replace Arduino String in state structs** -- Convert `String` fields to `char[]` in `AudioState`, `EthernetState`, `AppState`. Reduces heap fragmentation. Estimated effort: 2 hours.

10. **[Low] Clean up stale worktrees** -- Remove `.claude/worktrees/fix-matrix-routing-bounds` and `.claude/worktrees/phase2-asrc-dsd`. Estimated effort: 5 minutes.

---

*Concerns audit: 2026-03-26 -- 4 critical/high resolved, 4 medium resolved (CSP, server.send, setDsdMode, main.cpp extraction), 3 performance resolved (ASRC lazy, DSD virtual, volatile flags). Remaining: 2 medium (constants, innerHTML), 3 low (Strings, worktrees, sprintf), 3 monitor-only (device slots, event bits, web pages monolith), 3 gaps (watchdog, integration test, PSRAM degradation).*
