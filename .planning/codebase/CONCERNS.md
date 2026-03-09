# Codebase Concerns

**Analysis Date:** 2026-03-09

## Tech Debt

**Duplicate `#include "globals.h"` in multiple translation units:**
- Issue: Many `.cpp` files include `globals.h` twice consecutively. Harmless due to include guards but indicates copy-paste slop in initial file setup.
- Files: `src/main.cpp` (lines 2-3), `src/wifi_manager.cpp` (lines 3-4), `src/auth_handler.cpp` (lines 3-4), `src/ota_updater.cpp` (lines 4-5), `src/mqtt_handler.cpp` (lines 3-4), `src/mqtt_publish.cpp` (lines 7-8), `src/mqtt_ha_discovery.cpp` (lines 13-14), `src/smart_sensing.cpp` (lines 3-4), `src/mqtt_task.cpp` (lines 4-5), `src/settings_manager.cpp` (lines 3-4), `src/dsp_api.cpp` (lines 10-11), `src/websocket_handler.cpp` (lines 5-6)
- Impact: None at runtime, but extra preprocessor work and confusing to read.
- Fix approach: Remove the second `#include "globals.h"` in each affected file.

**Deprecated WS message handlers still active in `src/websocket_handler.cpp`:**
- Issue: 7 WS message types (`setDacEnabled`, `setDacVolume`, `setDacMute`, `setDacFilter`, `setEs8311Enabled`, `setEs8311Volume`, and `setEs8311Mute`) still dispatch at lines 950-1065. Each logs `LOG_W(...DEPRECATED: '%s' — use PUT /api/hal/devices...)` but still executes. These are compatibility shims for pre-HAL clients.
- Files: `src/websocket_handler.cpp` lines 950-1065
- Impact: Web frontend was updated but the code path grows stale. Any client sending these still works — until a future HAL refactor removes the underlying path they rely on.
- Fix approach: Once no known client sends these types, delete the handler blocks. Search web_src/ and e2e/ first to confirm no remaining callers.

**`filterMode` field has no HAL equivalent:**
- Issue: `appState.dac.filterMode` (`src/state/dac_state.h` line 38) is set/applied via deprecated WS handler `setDacFilter` using slot-0 hard-coding (`dac_get_driver_for_slot(0)`). HAL layer has no `filterMode` in `HalDeviceConfig`. Only the PCM5102A supports this; it never applies to ES8311 or future DACs.
- Files: `src/state/dac_state.h:38`, `src/websocket_handler.cpp:1009-1024`
- Impact: The `filterMode` setting never persists across reboots to HAL config, and would silently fail on any non-slot-0 device.
- Fix approach: Add `filterMode` field to `HalDeviceConfig`, remove from `DacState`, wire through `PUT /api/hal/devices`.

**Deprecated `audio_pipeline_register_source()` alias still present:**
- Issue: `audio_pipeline_register_source()` in `src/audio_pipeline.cpp` line 761 is a one-line wrapper calling `audio_pipeline_set_source()`. Marked `// DEPRECATED alias`.
- Files: `src/audio_pipeline.cpp:761`, `src/audio_pipeline.h:55-56`
- Impact: Dead weight. Any code still calling it should be updated.
- Fix approach: Grep for callsites, migrate to `audio_pipeline_set_source()`, delete the alias.

**Deprecated v1.14 flat WS broadcast fields still emitted:**
- Issue: `src/smart_sensing.cpp` lines 80-90 and `src/websocket_handler.cpp` lines 2117-2120 and 2418-2428 still send `audioRms1/2`, `audioVu1/2`, `audioPeak1/2`, `audioVrms1/2`, `adcStatus`, `noiseFloorDbfs` as top-level flat fields alongside the correct `audio.adcs[]` array. Marked `// DEPRECATED v1.14`.
- Files: `src/smart_sensing.cpp:80-90`, `src/websocket_handler.cpp:2117-2120`, `src/websocket_handler.cpp:2418-2428`
- Impact: Every WS broadcast carries roughly 10 redundant JSON keys. After removing old client support, these can be stripped.
- Fix approach: Confirm E2E tests and web_src/ do not rely on flat fields, then remove the deprecated emission blocks.

**Deferred toggle uses slot 0 fallback in `src/main.cpp`:**
- Issue: `src/main.cpp` line 1222 — `if (sinkSlot < 0) sinkSlot = 0;` — silently redirects a not-yet-mapped device toggle to sink slot 0 (PCM5102A). This happens if a HAL device enable request arrives before `hal_pipeline_on_device_available()` has assigned the slot.
- Files: `src/main.cpp:1220-1228`
- Impact: Toggling a second DAC (ES8311) before bridge assignment would incorrectly activate PCM5102A's output path instead.
- Fix approach: Guard the activation with `sinkSlot >= 0` rather than falling through to slot 0; log an error and skip if not yet mapped.

## Known Bugs

**SDIO conflict detection relies on `NET_WIFI` only — misses Ethernet-active-with-WiFi-radio:**
- Symptoms: `hal_discover_devices()` checks `appState.ethernet.activeInterface == NET_WIFI` to decide whether to skip Bus EXT (GPIO 48/54). If Ethernet is the active default route but the WiFi radio is also up (client mode), `activeInterface` may be `NET_ETHERNET`, so the bus scan proceeds and triggers SDIO conflicts.
- Files: `src/hal/hal_discovery.cpp:32`, `src/eth_manager.cpp:42-53`
- Trigger: Ethernet cable connected while WiFi client also connected; then triggering a POST /api/hal/scan.
- Workaround: Manual rescan is mutex-guarded; SDIO crash only occurs if both happen simultaneously. In practice most users have WiFi OR Ethernet active, not both.
- Fix approach: Change the check to `WiFi.status() == WL_CONNECTED || appState.wifi.connectSuccess` independent of which interface is "active".

## Security Considerations

**AP mode password defaults to `"12345678"` (config.h hardcoded):**
- Risk: The Wi-Fi AP password (`DEFAULT_AP_PASSWORD "12345678"`) is defined at compile time in `src/config.h:177` and assigned as default in `src/state/wifi_state.h:16`. Every device ships with the same AP password. The web UI login password is randomly generated per-device (first-boot, PBKDF2), but the AP itself uses a well-known static password.
- Files: `src/config.h:177`, `src/state/wifi_state.h:16`, `src/wifi_manager.cpp:223,384,1065,1088,1446,1532`
- Current mitigation: Web auth (PBKDF2 + rate limiting + HttpOnly cookie) requires a separate unique password to access device settings even after AP join.
- Recommendations: Generate a random per-device AP password from MAC/eFuse at first boot (similar to the web password generation already implemented in `src/auth_handler.cpp:288`). Display on TFT like the web password.

**No TLS/HTTPS on the web server:**
- Risk: HTTP on port 80 transmits session cookies and API tokens in cleartext on the local network. `SameSite=Strict; HttpOnly` cookie flags protect against cross-site theft but not passive LAN sniffing.
- Files: `src/main.cpp:92`, `src/auth_handler.cpp:647`
- Current mitigation: SameSite=Strict prevents cross-origin cookie abuse. The device is intended for local LAN use only.
- Recommendations: Deferred. ESP32 HTTPS requires SSL/TLS overhead that may not be feasible at current heap usage levels. Would require dedicated SRAM budget analysis.

**WS session ID fallback still accepted:**
- Risk: `src/websocket_handler.cpp` lines 193-195 still accept a raw `sessionId` in the WS auth message as a legacy fallback after one-time token (`/api/ws-token`) auth fails. This means any JS with access to the cookie value (via old browser XSS) could authenticate a WS connection.
- Files: `src/websocket_handler.cpp:193-195`
- Current mitigation: Cookies are HttpOnly so `document.cookie` cannot read them. The fallback path therefore only triggers if a client manually sends a sessionId (old client code path).
- Recommendations: Remove the `sessionId` WS fallback path after confirming all clients use the token flow. The E2E fixture at `e2e/helpers/fixtures.js` already uses the token path.

**AP password logged in plaintext at DEBUG level:**
- Risk: `src/wifi_manager.cpp:389` — `LOG_D("[WiFi] Password: %s", appState.wifi.apPassword)` prints the AP password to serial when debug level is `LOG_DEBUG`. If serial capture is enabled, this leaks the credential.
- Files: `src/wifi_manager.cpp:389`
- Current mitigation: `LOG_D` is filtered out unless the runtime log level is set to `LOG_DEBUG` (the default on first boot).
- Recommendations: Mask the password in the log: log only the first 2 characters followed by `***`.

## Performance Bottlenecks

**DSP `_gainBuf[256]` reused across speaker protection, bass enhance, and limiter:**
- Problem: `static float *_gainBuf` in `src/dsp_pipeline.cpp` is a single shared scratch buffer used by `dsp_limiter_process()`, `dsp_speaker_prot_process()`, and `dsp_bass_enhance_process()` in the same per-channel processing loop. If a channel has both a limiter and a bass-enhance stage, the buffer is overwritten between passes.
- Files: `src/dsp_pipeline.cpp:149,151,1212,1238,1264`
- Cause: Single PSRAM-allocated working buffer intentional for memory efficiency; works correctly now because each stage uses the buffer sequentially, not concurrently. However, adding a stage that processes `_gainBuf` non-locally could silently corrupt another stage's pass.
- Improvement path: Document the constraint explicitly in the buffer declaration comment. Any stage that writes to `_gainBuf` must not be interleaved with another stage that reads it.

**`DSP_MULTIBAND_COMP` band buffers capped at 256 samples regardless of block size:**
- Problem: `dsp_multiband_comp_process()` in `src/dsp_pipeline.cpp:1287` — `int n = len > 256 ? 256 : len`. The band buffer `slot.bandBuf[DSP_MULTIBAND_MAX_BANDS][256]` is fixed at 256. Any caller with a longer block silently processes only the first 256 samples per DMA cycle.
- Files: `src/dsp_pipeline.cpp:1287`, `src/dsp_pipeline.cpp:89`
- Cause: Pool was designed for 256-frame DMA buffers (`I2S_DMA_BUF_LEN=256`). Current pipeline sends 256-frame blocks so the cap is never hit in practice.
- Improvement path: Add `static_assert(I2S_DMA_BUF_LEN <= 256)` or make the band buffer length a compile-time constant from `I2S_DMA_BUF_LEN`.

## Fragile Areas

**`dsp_stereo_width` processing happens outside `dsp_process_channel()` loop:**
- Files: `src/dsp_pipeline.cpp:613-628`, `src/dsp_pipeline.cpp:711-726`
- Why fragile: `DSP_STEREO_WIDTH` is a special case that cannot be handled per-channel because it needs both L and R buffers simultaneously. It is applied after `dsp_process_channel()` in two separate code paths (int32 path line 613 and float path line 711). Adding a third entry point (e.g., surround processing) must repeat the same pattern or the stereo width stage will be silently skipped.
- Safe modification: Any new `dsp_process_buffer_*()` entry point must include a matching post-channel stereo-width loop.
- Test coverage: 4 unit tests (`test_stereo_width_mono_collapse`, `test_stereo_width_normal_passthrough`, etc.) verify the int32 path. Float path has no dedicated tests.

**`HAL_DISC_ONLINE` and `HAL_DISC_GPIO_ID` discovery modes are reserved stubs:**
- Files: `src/hal/hal_types.h:43-45`
- Why fragile: `HAL_DISC_GPIO_ID = 2` is marked `// Resistor ID on GPIO (placeholder)`. `HAL_DISC_ONLINE = 4` is marked `// Fetched from GitHub YAML DB`. Neither has any implementation. The `hal_discovery.cpp` only handles BUILTIN, EEPROM, and MANUAL. If a device is somehow assigned `HAL_DISC_ONLINE`, the GUI displays "Online" (`src/gui/screens/scr_devices.cpp:90`) but no fetch will occur.
- Safe modification: Do not assign either value to real devices until implemented. Guard any new code path that checks `HAL_DISC_ONLINE` with a feature flag.
- Test coverage: No tests for these paths.

**Single `PendingDeviceToggle` slot allows only one deferred HAL activation at a time:**
- Files: `src/state/dac_state.h:42`, `src/main.cpp:1216-1236`
- Why fragile: `appState.dac.pendingToggle` is a single struct. If two HAL devices (e.g., PCM5102A and ES8311) are enabled in rapid succession from the web UI, only the second request survives — the first is silently overwritten before the main loop processes it.
- Safe modification: When enabling multiple devices via script or HA automation, space requests at least one main-loop cycle (~5ms) apart.
- Test coverage: No test for concurrent toggle requests.

**I2S_MCLK_PIN hardcoded in `i2s_configure_adc1()` despite HAL config:**
- Files: `src/i2s_audio.cpp:387,395,401`, `src/config.h:53`
- Why fragile: Both `tx_cfg.gpio_cfg.mclk` and `rx_cfg.gpio_cfg.mclk` are set to `(gpio_num_t)I2S_MCLK_PIN` (a compile-time constant) inside the deprecated `i2s_configure_adc1()`. The new `i2s_audio_configure_adc()` path reads MCLK from HAL config. Code that still calls the old path (recovery/retry) uses the hardcoded pin and ignores `HalDeviceConfig.pinMclk`.
- Safe modification: Route all ADC init through `i2s_audio_configure_adc()`. The deprecated `i2s_configure_adc1/2()` functions should not be called for new code.
- Test coverage: Only HAL path is covered by `test_hal_pcm1808`. The deprecated path has no test.

**`DSP_MULTIBAND_MAX_SLOTS = 1` limits multiband compressor to one instance globally:**
- Files: `src/dsp_pipeline.cpp:69`
- Why fragile: Only one multiband compressor can exist across all channels and both DSP state buffers. Adding a second `DSP_MULTIBAND_COMP` stage on any channel returns `dsp_mb_alloc_slot() == -1` silently. The pool is not checked at the API layer (`dsp_add_chain_stage`) — callers receive a stage with `mbSlot == -1` which the processor skips.
- Safe modification: Check `dsp_mb_alloc_slot()` return value in `dsp_add_stage()` and fail gracefully with a swap rollback like FIR stages do.
- Test coverage: `test_multiband_slot_alloc_and_free` covers alloc/free; overflow case (alloc beyond 1) is not tested.

**WS authentication count `_wsAuthCount` can underflow on unexpected disconnect order:**
- Files: `src/websocket_handler.cpp` (the `_wsAuthCount` decrement path)
- Why fragile: The counter decrements on WS disconnect for authenticated clients. The underflow guard exists but the count can become stale if a client disconnects before auth completes or if auth is validated twice for the same slot.
- Safe modification: Always check `wsAuthStatus[num]` before decrementing and ensure auth status is cleared atomically with the decrement.
- Test coverage: 5 tests cover the counter; disconnect-before-auth edge case has a guard but relies on flag coherence.

## Scaling Limits

**`HAL_MAX_DEVICES = 24` with 14 builtin devices at boot:**
- Current capacity: 24 device slots. Builtin devices at boot: PCM5102A, ES8311, PCM1808 x2, NS4150B, Temp Sensor, SigGen (generic + HAL), USB Audio, MCP4725, Encoder, Button, Buzzer, Relay, LED, Display. Approximate count: 14-16 depending on optional devices.
- Limit: Only 8-10 slots remain for expansion devices discovered via I2C scan or EEPROM. Adding a third PCM1808, external DAC, GPIO expander, and a custom device would exhaust the table.
- Scaling path: Increase `HAL_MAX_DEVICES` in `src/hal/hal_types.h`. Current value was already raised from 16 to 24. The arrays `_devices[]`, `_configs[]`, `_retryState[]`, `_faultCount[]` grow linearly with this value — all static allocation, no heap impact.

**`DSP_MAX_DELAY_SLOTS = 2` and `DSP_MAX_FIR_SLOTS = 2` are tight for multi-output setups:**
- Current capacity: 2 concurrent delay stages and 2 concurrent FIR stages total across all channels and both DSP state buffers.
- Limit: An 8-output system using time-alignment delays on 4 independent outputs would need 4 delay slots. Overflow fails silently (stage added with `delaySlot == -1`, processor skips).
- Scaling path: Increase `DSP_MAX_DELAY_SLOTS` and `DSP_MAX_FIR_SLOTS` in `src/config.h`. PSRAM allocation in `dsp_pipeline_init()` must be updated accordingly. Pre-flight heap check already exists for no-PSRAM fallback.

**`AUDIO_OUT_MAX_SINKS = 8` limits output devices:**
- Current capacity: 8 sink slots (PCM5102A + ES8311 + 6 spare).
- Limit: Each HAL output device with `HAL_CAP_DAC_PATH` consumes one slot. Adding 6 more external DACs would fill the table; the bridge would return `-1` for `_sinkSlotForDevice()` and the device would be silently skipped.
- Scaling path: Raise `AUDIO_PIPELINE_MAX_OUTPUTS` in `src/config.h`. This also changes the routing matrix column count.

## Dependencies at Risk

**Embedded OTA TLS root CAs for GitHub will expire eventually:**
- Risk: `src/ota_updater.cpp` lines 25-110 embed three hardcoded PEM certificates (Sectigo R46, E46, DigiCert G2). Per the comment, these expire 2038-2046. A CA rotation before that date would break OTA silently — firmware downloads would fail TLS verification.
- Impact: OTA update check and download would return SSL handshake error; `appState.ota.updateAvailable` would never become true.
- Migration plan: The embedded certs can be updated in a future firmware release. The `enableCertValidation` toggle in settings (`appState.general.enableCertValidation`) provides an escape hatch if a cert expires before a fix is deployed.

**`WebSockets` library is vendored in `lib/WebSockets/` without a version pin:**
- Risk: The library is a local copy with no lockfile or version tag reference. Upstream changes (API breaks, security fixes) are invisible until a developer manually diffs.
- Impact: Build always uses the local copy, so no spontaneous breakage. But security vulnerabilities in the WebSocket handshake code would not be auto-discovered.
- Migration plan: Tag the vendored copy with the upstream commit SHA in the directory or a `README`. Periodically diff against upstream.

**`peaceiris/actions-gh-pages@v3` in docs.yml is unpinned by SHA:**
- Risk: GitHub Actions third-party action is referenced by tag (`@v3`) not by commit SHA. A tag can be force-pushed.
- Files: `.github/workflows/docs.yml:82`
- Impact: Supply chain attack vector for the docs deployment workflow only; the firmware build (`tests.yml`) uses only official `actions/*` at `@v4`.
- Migration plan: Pin to `peaceiris/actions-gh-pages@<sha>` for supply chain safety.

## Missing Critical Features

**Online HAL device database (`HAL_DISC_ONLINE`) not implemented:**
- Problem: `src/hal/hal_types.h:45` defines `HAL_DISC_ONLINE = 4` as "Fetched from GitHub YAML DB". No fetch logic, no YAML parser, and no network DB URL exists anywhere in the codebase.
- Blocks: Users cannot add expansion devices by selecting from an online catalog; they must manually enter compatible strings.

**Platform archive mirror for OTA not implemented:**
- Problem: OTA downloads firmware directly from `objects.githubusercontent.com` CDN (GitHub Releases). No fallback mirror or local update server exists. If GitHub's CDN is unreachable (firewalled environments, GitHub outage), OTA silently fails.
- Blocks: Enterprise or air-gapped deployments cannot perform OTA updates.

**No HTTPS/TLS on the web interface:**
- Problem: The embedded HTTP server on port 80 has no SSL wrapper. Session cookies and API responses are cleartext on LAN.
- Blocks: Strict security requirements (e.g., WPA3-only networks with certificate validation expectations).

## Test Coverage Gaps

**No unit tests for the float `dsp_process_buffer_float()` stereo-width path:**
- What's not tested: The stereo-width mid-side processing in the float-native entry point (`src/dsp_pipeline.cpp:711-726`). Only the int32 `dsp_process_buffer()` path has stereo-width tests.
- Files: `src/dsp_pipeline.cpp:677-762`
- Risk: A regression in the float path's mid-side math would not be caught before deployment.
- Priority: Low (same algorithm, different wrapper; math tests cover the core logic)

**No test for `PendingDeviceToggle` overwrite race:**
- What's not tested: Concurrent calls to `appState.dac.requestDeviceToggle()` from two WebSocket handlers in the same main-loop iteration overwrites the first request.
- Files: `src/state/dac_state.h:46-51`, `src/main.cpp:1216-1236`
- Risk: Silent loss of a device activation/deactivation request; hard to diagnose in the field.
- Priority: Medium

**No test for `HAL_DISC_GPIO_ID` or `HAL_DISC_ONLINE` discovery paths:**
- What's not tested: Both enum values in `src/hal/hal_types.h` have no implementation; test scaffolding for them would catch future regressions when implemented.
- Files: `src/hal/hal_types.h:43-45`
- Risk: Low until implemented, but adding tests now would define the expected contract.
- Priority: Low (implement first, then test)

**No test for HAL device slot exhaustion (≥ 24 devices):**
- What's not tested: `HalDeviceManager::registerDevice()` returning -1 when `_count == HAL_MAX_DEVICES`. Downstream behavior (bridge skipping the device, pipeline not getting a source) is untested.
- Files: `src/hal/hal_device_manager.cpp`, `src/hal/hal_pipeline_bridge.cpp`
- Risk: Silent data loss when expansion devices exceed capacity.
- Priority: Medium

**No integration test for Ethernet + WiFi simultaneous active (SDIO scan bug):**
- What's not tested: The HAL discovery SDIO skip logic when `activeInterface == NET_ETHERNET` but WiFi radio is active.
- Files: `src/hal/hal_discovery.cpp:32`
- Risk: Silent MCU reset or SDIO corruption on dual-interface hardware.
- Priority: Medium

**Docs CI pipeline requires two manual GitHub setup steps:**
- What's not configured: `ANTHROPIC_API_KEY` GitHub Actions secret and GitHub Pages source (`gh-pages` branch) must be configured manually in the repo settings before `.github/workflows/docs.yml` produces any output. The workflow degrades gracefully (builds the existing site without generating docs) but this is not self-documenting.
- Files: `.github/workflows/docs.yml:52-57,81-86`
- Risk: New contributors enabling the workflow see silent no-ops without understanding why.
- Priority: Low — add a `README` note to `docs-site/` or a one-time setup section in CLAUDE.md.

---

*Concerns audit: 2026-03-09*
