# Codebase Concerns

**Analysis Date:** 2026-03-07

## Tech Debt

**DSP Float-Int Bridge (Per-Pipeline Buffer):**
- Issue: The per-input DSP path (`pipeline_run_dsp()`) converts float→int32→dsp_process_buffer→int32→float on every buffer call (every ~5.33ms). This is an acknowledged temporary bridge at `src/audio_pipeline.cpp:284-308`.
- Files: `src/audio_pipeline.cpp` lines 282-309, `src/dsp_pipeline.cpp`
- Impact: ~2× float↔int conversion cost per DSP buffer; marginal CPU overhead at 48kHz/256-frame buffers. PSRAM bridge buffer `_dspBridgeBuf` wastes 2KB of internal SRAM (DMA constraint).
- Fix approach: Port `dsp_pipeline.cpp` to operate natively on float32 buffers, removing the `_dspBridgeBuf` scratch and the `MAX_24BIT_F` normalization round-trips.

**Legacy Line-Based Text Persistence:**
- Issue: `settings.txt` and `mqtt_config.txt` use positional line-by-line text files instead of JSON. Adding, removing, or reordering settings requires careful line-index accounting, placeholder lines for missing features, and a manual version header (`VER=2`). Already has placeholder lines 28-29 for removed features.
- Files: `src/settings_manager.cpp` (lines 27-340), `src/mqtt_handler.cpp` (lines 70-145)
- Impact: Fragile migration path — adding any new setting requires updating line offsets for all existing callers. The 30-slot `lines[]` array has no overflow protection if the file grows unexpectedly.
- Fix approach: Migrate both files to ArduinoJson (already a dependency). HAL config (`/hal_config.json`) demonstrates the correct pattern.

**Legacy Flat AppState Accessors for Dual-ADC:**
- Issue: 22 reference fields (`audioRmsLeft`, `audioRmsRight`, `audioVuLeft`, etc.) in `src/app_state.h` lines 199-221 are C++ references aliasing `audioAdc[0]`. These exist to avoid breaking existing WebSocket/MQTT code that predates the dual-ADC `AdcState` array.
- Files: `src/app_state.h` lines 199-221
- Impact: Adds confusion — any new ADC2 metering code must use `audioAdc[1]`, but ADC1 code may accidentally mix the alias form with the direct array form.
- Fix approach: Audit all reference sites (WS/MQTT handlers) and replace with `audioAdc[0].fieldName` directly; remove the alias block.

**Audio Pipeline Sink Cap Mismatch:**
- Issue: `AUDIO_OUT_MAX_SINKS` is defined as 4 (`src/config.h` line 209: `AUDIO_PIPELINE_MAX_OUTPUTS`), but the matrix has 8 output channels (`AUDIO_PIPELINE_MATRIX_SIZE = 8`). Only the first 4 matrix output channels can be dispatched to physical sinks; channels 4-7 are silently dropped.
- Files: `src/config.h` lines 207-209, `src/audio_output_sink.h` line 18, `src/audio_pipeline.cpp` lines 83-91
- Impact: 8-output routing scenarios (e.g., 4-way active crossover to 4 stereo DACs) are architecturally blocked by the sink array size. Currently only 2 physical outputs exist (PCM5102A + ES8311), so this is latent.
- Fix approach: Raise `AUDIO_PIPELINE_MAX_OUTPUTS` to 8 to match `AUDIO_PIPELINE_MATRIX_SIZE`; verify internal SRAM budget for the additional 4 `_sinkBuf` arrays (4 × 512 × 4 = 8KB).

**HAL I2C Bus Scan Stub (Expansion Bus):**
- Issue: `hal_i2c_scan_bus()` in `src/hal/hal_discovery.cpp` lines 127-142 is a non-functional stub that always returns 0. The function was created for "Phase 2 hardware validation" but never implemented. Bus EXT and Bus EXP scans in `hal_discover_devices()` call it, get empty masks, and proceed as if no devices were found.
- Files: `src/hal/hal_discovery.cpp` lines 127-142
- Impact: External I2C devices on the expansion bus (GPIO 28/29) are never discovered via the I2C scan path. EEPROM-probed devices still work because `dac_i2c_scan()` uses a different code path. MCP4725 and future expansion devices silently fail auto-discovery.
- Fix approach: Implement `hal_i2c_scan_bus()` using the appropriate Wire instance per bus index, scanning address range 0x03-0x77.

**Memory Leak in `hal_pipeline_sync()` forEach Context:**
- Issue: `src/hal/hal_pipeline_bridge.cpp` line 225 allocates `new int[2]{0, 0}` as the forEach callback context but never frees it. The comment on line 227 acknowledges the issue: "forEach ctx lifetime is local above — recount here."
- Files: `src/hal/hal_pipeline_bridge.cpp` lines 215-235
- Impact: 8 bytes leaked on every call to `hal_pipeline_sync()`. Currently only called once at boot, so it is benign in practice. Would become significant if rescan ever calls sync.
- Fix approach: Use a stack-allocated `int counts[2] = {0, 0}` and pass `&counts`.

**HAL Discovery SDIO Conflict Check Ignores Ethernet:**
- Issue: `hal_discover_devices()` in `src/hal/hal_discovery.cpp` line 32 checks only `appState.wifiConnectSuccess` to decide whether to skip Bus EXT (GPIO 48/54). When Ethernet is the active interface (WiFi off but `appState.activeInterface == NET_ETHERNET`), the SDIO bus is unused and Bus EXT would be safe to scan — but the code still skips it if `wifiConnectSuccess` is ever true.
- Files: `src/hal/hal_discovery.cpp` lines 31-47
- Impact: First device power-on with WiFi then switch to Ethernet would permanently skip Bus EXT scans for the session, even though no SDIO conflict exists.
- Fix approach: Condition on `appState.activeInterface == NET_WIFI` or check `WiFi.isConnected()` at scan time rather than the cached `wifiConnectSuccess` flag.

**`hal_online_fetch` References Non-Existent Repository:**
- Issue: `src/hal/hal_online_fetch.cpp` line 21 hardcodes `https://raw.githubusercontent.com/alx-audio/hal-devices/main/devices/`. The `alx-audio` GitHub organization and `hal-devices` repository do not exist. The function `hal_online_fetch()` is declared but never called from production code.
- Files: `src/hal/hal_online_fetch.cpp`, `src/hal/hal_online_fetch.h`
- Impact: Dead code consuming ~8KB of flash; any future code that calls it will receive a TLS 404 and silently fail. The YAML parser stub is incomplete.
- Fix approach: Either remove the file pair entirely, or replace the base URL with the correct repository when the device database is published.

---

## Known Bugs

**Blocking `delay()` on Login Failure Starves HTTP/WebSocket:**
- Symptoms: On failed web login with accumulated failures, the HTTP server stalls for up to 30 seconds (progressive rate-limit delay). During this time, WebSocket keepalives time out, MQTT polling continues independently (it is on its own task), but all other web requests queue behind the login handler.
- Files: `src/auth_handler.cpp` lines 371-375
- Trigger: Any login attempt after 5+ failures.
- Workaround: The rate limiting state resets automatically after 5 minutes of no failures (`LOGIN_COOLDOWN_US`). The 30-second stall can only occur if `_loginFailCount >= 5`.
- Fix approach: Replace `delay(delayMs)` with a timestamp-based gate that rejects early requests without blocking, or move the login handler to return 429 immediately with a `Retry-After` header.

**Session Cookie Missing `HttpOnly` Flag:**
- Symptoms: Web session cookie is intentionally set without `HttpOnly` so client-side JavaScript can read it for WebSocket authentication. This exposes the session token to XSS.
- Files: `src/auth_handler.cpp` line 418 (comment on line 416 acknowledges the trade-off)
- Trigger: Any stored XSS vulnerability in the web UI would allow session token exfiltration.
- Workaround: The web UI is self-contained (no external scripts) and served from flash, limiting XSS attack surface.
- Fix approach: Implement WebSocket authentication via a short-lived one-time token issued by a separate `/api/ws-token` endpoint, allowing the session cookie to have `HttpOnly`.

---

## Security Considerations

**Default Password Hardcoded and Shared Between AP and Web Interface:**
- Risk: `DEFAULT_AP_PASSWORD "12345678"` in `src/config.h` line 180 is used for both the WiFi AP password and the default web UI password. Any device that has never had its password changed is accessible over both WiFi AP and the web UI with a known credential.
- Files: `src/config.h` line 180, `src/auth_handler.cpp` lines 112-117
- Current mitigation: The web UI warns when using the default password (`isDefaultPassword()` check on login response). The AP SSID contains the device serial number, so the device is uniquely identifiable.
- Recommendations: Generate a unique random password per device on first boot (stored in NVS), using the serial number as a seed. Alert the user to change both passwords on first access.

**Web Interface Served Over Plain HTTP:**
- Risk: The web server runs on port 80 with no TLS. Credentials (session cookies, password change requests) are transmitted in plaintext on the local network.
- Files: `src/main.cpp` (WebServer initialization), `src/config.h` line 124
- Current mitigation: WiFiClientSecure is used for outbound connections (OTA, GitHub API). Inbound web traffic is unencrypted. SameSite=Strict on cookies prevents cross-site request forgery over HTTPS-to-HTTP downgrade vectors.
- Recommendations: Consider a self-signed certificate for the web server, or at minimum document the HTTP-only limitation in the user manual.

**SHA256 Password Hashing Without Salt:**
- Risk: `hashPassword()` in `src/auth_handler.cpp` lines 44-66 applies SHA256 directly to the password with no salt. Identical passwords produce identical hashes, enabling rainbow table attacks if NVS storage is read.
- Files: `src/auth_handler.cpp` lines 44-66
- Current mitigation: NVS storage requires physical flash access. The default password is flagged as insecure via the UI.
- Recommendations: Add a device-unique salt (e.g., eFuse MAC bytes) to the hash input. This makes the hash unique per device without requiring PBKDF2 or bcrypt overhead.

---

## Performance Bottlenecks

**`mqtt_handler.cpp` Monolithic File (3819 lines):**
- Problem: The entire MQTT system — settings load/save, Home Assistant discovery payload generation, state publish logic, WebSocket forwarding stubs, and all MQTT command handlers — is in one 3819-line file.
- Files: `src/mqtt_handler.cpp`
- Cause: Organic growth over multiple feature additions. No refactoring has occurred since the mqtt_task split.
- Improvement path: Split into `mqtt_settings.cpp`, `mqtt_ha_discovery.cpp`, `mqtt_publish.cpp`, `mqtt_commands.cpp`. This also improves compile-time incrementality.

**`websocket_handler.cpp` Large Broadcast Functions (2363 lines):**
- Problem: Several broadcast functions (hardware stats, audio analysis, HAL device list) rebuild complete JSON documents on every call rather than diff-ing against previous state. The main loop calls these at 2-second intervals unconditionally.
- Files: `src/websocket_handler.cpp`
- Cause: Dirty-flag pattern is correctly used to gate broadcasts, but individual JSON builders are not incremental.
- Improvement path: Cache serialized JSON strings for stable sub-objects (e.g., device list only changes on HAL events) and only re-serialize changed fields.

**`pipeline_run_dsp()` Double Float↔Int Conversion Per Buffer:**
- Problem: Two format conversion loops (512 multiply-divide operations each) run on every 5.33ms audio buffer for each active DSP lane. At 48kHz, this is ~188 conversions per second per lane.
- Files: `src/audio_pipeline.cpp` lines 282-308
- Cause: DSP pipeline was designed around int32 RJ buffers; float pipeline was added later with a bridge layer. See Tech Debt section above.
- Improvement path: Port `dsp_process_buffer()` to accept float32, removing both conversion loops.

---

## Fragile Areas

**DAC Toggle Deferred-Toggle Mechanism:**
- Files: `src/app_state.h` lines 471-472 (`_pendingDacToggle`, `_pendingEs8311Toggle`), `src/main.cpp` (loop handling)
- Why fragile: DAC init/deinit must be deferred to the main loop via volatile int8_t flags because direct `dev->deinit()` from HTTP handlers does not stop the audio pipeline. The deferred mechanism is undocumented in the code and requires correct sequencing: set flag → main loop observes → calls `dac_output_init()`/`es8311_init()`. Missing any step (e.g., calling deinit directly from a new code path) bypasses the handshake and causes audio task use-after-free crashes.
- Safe modification: Any new code path that needs to toggle DAC output devices must go through `appState._pendingDacToggle = 1` or `-1`, never call `dev->deinit()` directly.
- Test coverage: `test/test_dac_hal/` and `test/test_hal_adapter/` exist but do not cover the deferred-toggle path.

**AppState as God Object (620-line Header):**
- Files: `src/app_state.h`
- Why fragile: All application state (60+ fields spanning WiFi, MQTT, OTA, audio, display, DSP, USB, HAL, diagnostics) lives in a single singleton header. Every module includes it, creating a full recompile cascade whenever any state field is added.
- Safe modification: New state fields should be added in the appropriate section with dirty-flag methods. Do not access AppState from the `audio_pipeline_task` (Core 1 real-time) except through the already-approved `audioPaused`, `audioTaskPausedAck`, and `adcEnabled` fields — all other reads from the audio task are racy.
- Test coverage: AppState is exercised indirectly by most test modules but has no dedicated unit test.

**FreeRTOS `vTaskSuspendAll()` in Audio Pipeline Sink Management:**
- Files: `src/audio_pipeline.cpp` lines 780, 841, 874, 899
- Why fragile: `vTaskSuspendAll()` is used to atomically install or remove sinks while the audio task is running. If any suspended code path calls a FreeRTOS API that would block (e.g., `xSemaphoreTake`), it will trigger an assert. Current callers (HTTP handlers) are on the main loop, which does not hold any semaphores during sink operations. This invariant must be maintained for any future sink management callers.
- Safe modification: Never call `audio_pipeline_set_sink()` or `audio_pipeline_remove_sink()` from within an ISR or from a task that holds a FreeRTOS semaphore.
- Test coverage: `test/test_sink_slot_api/` covers the API but runs on native (no FreeRTOS scheduling).

**I2S Dual-Master Clock Coordination:**
- Files: `src/i2s_audio.cpp` (`i2s_configure_adc1()`, `i2s_configure_adc2()`)
- Why fragile: Both I2S_NUM_0 (ADC1) and I2S_NUM_1 (ADC2) are configured as master with identical divider chains. Any change to sample rate, DMA buffer count, or clock divider for one ADC must be replicated identically to the other, or the BCK frequencies will desynchronize and ADC2 will produce corrupted data. The CLAUDE.md notes that "I2S slave mode has intractable DMA issues" on this platform, making the dual-master approach the only viable path.
- Safe modification: Sample rate changes must update both ADCs atomically using the `audioPaused` semaphore handshake. Never change only one ADC's clock configuration.
- Test coverage: `test/test_i2s_audio/` tests the module on native where I2S is mocked; hardware behavior is not covered.

---

## Scaling Limits

**HAL Device Registry (16 Slots):**
- Current capacity: `HAL_MAX_DEVICES = 16` hard limit in `src/hal/hal_types.h`
- Limit: Adding more than 16 HAL devices (currently using ~12 builtins + PCM5102A + ES8311 + PCM1808 ×2 + NS4150B + TempSensor + display + encoder + buzzer + LED + relay + button + siggen) approaches the cap.
- Scaling path: Increase `HAL_MAX_DEVICES` (requires verifying pin tracking and mapping table sizes).

**DSP Preset Slots (32):**
- Current capacity: `DSP_PRESET_MAX_SLOTS = 32` in `src/config.h` line 95
- Limit: Preset name array `dspPresetNames[32][21]` in AppState occupies 672 bytes of SRAM permanently.
- Scaling path: Load preset names lazily from LittleFS rather than caching all in AppState.

**USB Audio Ring Buffer (20ms):**
- Current capacity: `USB_AUDIO_RING_BUFFER_MS = 20` at 48kHz = 960 stereo frames (~7.5KB PSRAM)
- Limit: USB audio buffer underruns occur if the audio pipeline task is preempted for more than 20ms. Currently the DMA runway is ~64ms (12 buffers × 256 frames), so this is safe. Any reduction in DMA buffer count would expose the ring buffer as the bottleneck.
- Scaling path: Increase `USB_AUDIO_RING_BUFFER_MS` if underrun count (`usbAudioBufferUnderruns`) grows.

---

## Dependencies at Risk

**PlatformIO ESP32-P4 Platform (Pinned Archive URL):**
- Risk: `platformio.ini` line 16 pins the platform to a specific GitHub release archive URL (`platform-espressif32.zip`). If this URL becomes unavailable or the release is deleted, CI and fresh developer builds will fail silently or produce unclear errors.
- Impact: All firmware builds blocked.
- Migration plan: Mirror the platform archive in a controlled location (e.g., the repository's GitHub releases or an S3 bucket).

**WebSockets Library Requires Patch Script:**
- Risk: `pre:tools/patch_websockets.py` modifies the `links2004/WebSockets` library source after installation. If the library updates or the patch fails silently, the build may succeed with broken WebSocket behavior on ESP32-P4.
- Files: `platformio.ini` line 24, `tools/patch_websockets.py`
- Impact: WebSocket disconnections or frame corruption on ESP32-P4 RISC-V target.
- Migration plan: Fork the library and reference the fork directly in `lib_deps`, eliminating the patch script dependency.

---

## Missing Critical Features

**I2C Expansion Bus Auto-Discovery Not Implemented:**
- Problem: The `hal_i2c_scan_bus()` function is a stub (always returns 0). External I2C devices on Bus EXP (GPIO 28/29) cannot be auto-discovered. The MCP4725 entry exists in the HAL database (`src/hal/hal_device_db.cpp` lines 219-234) but is never matched by discovery.
- Blocks: Any expansion hardware that relies on I2C auto-discovery (MCP4725, future DACs, level sensors).

**HAL Online Device Database Fetch Non-Functional:**
- Problem: `hal_online_fetch()` in `src/hal/hal_online_fetch.cpp` targets a GitHub URL (`alx-audio/hal-devices`) that does not exist. The function is not called from any production code path.
- Blocks: The planned "fetch unknown device descriptor from cloud registry" workflow.

---

## Test Coverage Gaps

**`hal_audio_health_bridge.cpp` — New File, Limited Coverage:**
- What's not tested: Flap guard edge cases (exactly 2 transitions in window vs. 3), timer wraparound in `_flap_record_transition()`, ERROR-state persistence across health checks, flap guard interaction with the pipeline bridge ADC lane disable.
- Files: `src/hal/hal_audio_health_bridge.cpp`, `test/test_audio_health_bridge/test_audio_health_bridge.cpp`
- Risk: Incorrect flap detection could leave an ADC permanently in ERROR state after transient noise, or allow a genuinely broken ADC to stay AVAILABLE.
- Priority: High — newly merged code (modified 2026-03-07).

**`hal_pipeline_bridge.cpp` — `hal_pipeline_sync()` Boot-Time Path:**
- What's not tested: The `forEach` lambda path that calls `hal_pipeline_on_device_available()` for already-AVAILABLE devices at boot. The memory leak (unreleased `new int[2]`) is in this exact code path.
- Files: `src/hal/hal_pipeline_bridge.cpp` lines 204-235, `test/test_hal_bridge/test_hal_bridge.cpp`
- Risk: Boot-time sync may register duplicate sink slots if called more than once, or fail silently if the callback lambda does not fire.
- Priority: Medium.

**`dac_hal.cpp` Deferred-Toggle Paths:**
- What's not tested: The `_pendingDacToggle` / `_pendingEs8311Toggle` deferred init/deinit flow through the main loop. Tests exercise `dac_output_init()` directly, not the flag-based deferred path.
- Files: `src/dac_hal.cpp`, `src/main.cpp` (loop section handling pending toggles)
- Risk: Code changes to the deferred mechanism could break DAC toggling without any failing tests.
- Priority: Medium.

**`hal_discovery.cpp` I2C Scan Paths:**
- What's not tested: The `hal_i2c_scan_bus()` function body — it is a stub that always returns 0, so tests for it would currently trivially pass. The EEPROM probe path is tested in `test/test_hal_discovery/` but relies on mocked I2C.
- Files: `src/hal/hal_discovery.cpp`, `test/test_hal_discovery/`
- Risk: When the I2C scan is implemented, there are no baseline tests to validate correct behavior.
- Priority: Low (cannot regress a stub).

**`mqtt_handler.cpp` Login Blocking Behavior:**
- What's not tested: The `delay(delayMs)` progressive rate-limiting path in `handleLogin()` has no E2E test that verifies either the blocking duration or its effect on concurrent WebSocket connections.
- Files: `src/auth_handler.cpp` lines 368-385, `e2e/tests/auth.spec.js`
- Risk: If the blocking delay is removed or reduced in a refactor, no test would catch a regression in brute-force protection.
- Priority: Low (security hardening, not functionality).

---

*Concerns audit: 2026-03-07*
