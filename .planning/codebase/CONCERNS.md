# Codebase Concerns

**Analysis Date:** 2026-03-09

---

## Tech Debt

**PLANNED — Dual device registries — `DacRegistry` and `HalDriverRegistry` both active:**
- Issue: Two parallel device registries coexist. `src/dac_registry.h/.cpp` maps `uint16_t deviceId → DacFactoryFn` (legacy) and is actively used in `src/dac_hal.cpp:565` and `src/dac_api.cpp:173,487`. `src/hal/hal_driver_registry.h/.cpp` maps compatible strings → HAL factory functions. `HalDacAdapter` (`src/hal/hal_dac_adapter.h/.cpp`) bridges between them using the `legacyId` field on `HalDeviceDescriptor`.
- Files: `src/dac_registry.cpp`, `src/dac_registry.h`, `src/hal/hal_driver_registry.h`, `src/hal/hal_dac_adapter.h/.cpp`, `src/dac_hal.cpp:565`, `src/dac_api.cpp:173,487`, `src/websocket_handler.cpp:31`
- Impact: New DAC drivers must be registered in both systems. `dac_api.cpp` enumerates drivers from `DacRegistryEntry` — not from `HalDeviceManager` — so the API driver list diverges from the HAL device list.
- Fix approach: Once all DAC drivers are HAL-native (compatible string registered), enumerate from `HalDeviceManager` in `dac_api.cpp`. Remove `DacRegistry`, `DacFactoryFn`, `legacyId`. Remove `HalDacAdapter` once `dac_hal.cpp` is fully bridge-driven.
- Plan: `docs-internal/planning/debt-registry-unification.md` (DEBT-6, Phases 1-3)
- Source: `docs-internal/architecture/legacy-inventory.md`, `docs-internal/architecture/disconnect-analysis.md` D10

**PLANNED — Bridge is metadata-only — sinks registered directly from `dac_hal.cpp`:**
- Issue: `hal_pipeline_bridge.cpp` `on_device_available()` tracks ordinal counts and fires dirty flags, but the actual `audio_pipeline_set_sink()` call is made from `src/dac_hal.cpp` (~line 640–680), bypassing the bridge entirely. The bridge's `_halSlotToSinkSlot[]` mapping table is accurate for forward-lookup but sink registration is split across two files.
- Files: `src/hal/hal_pipeline_bridge.cpp`, `src/dac_hal.cpp:640–680`
- Impact: Future HAL devices that expect the bridge to register their sink will silently produce no audio output.
- Fix approach: Move all `audio_pipeline_set_sink()` calls into `hal_pipeline_bridge.cpp` `on_device_available()`. `dac_hal.cpp` becomes bus-utility module only.
- Plan: `docs-internal/planning/debt-registry-unification.md` (DEBT-6, Phase 1)
- Source: `docs-internal/architecture/disconnect-analysis.md` D1

**FIXED — Deprecated WS message handlers removed (7 paths):**
- Resolution: All 7 deprecated handlers (`setDacEnabled`, `setDacVolume`, `setDacMute`, `setDacFilter`, `setEs8311Enabled`, `setEs8311Volume`, `setEs8311Mute`) and `_halSlotForCompatible()` helper removed from `src/websocket_handler.cpp`. Zero callers in frontend, E2E, tests, or MQTT. `PUT /api/hal/devices` is the sole replacement. Documentation updated in `docs-site/docs/developer/websocket.md`.

**`filterMode` field has no HAL equivalent:**
- Issue: `appState.dac.filterMode` (`src/state/dac_state.h:38`) is PCM5102A-specific with no `HalDeviceConfig` field. The deprecated WS handler `setDacFilter` was removed — only `POST /api/dac` (`src/dac_api.cpp:146`) remains as a write path, also deprecated.
- Files: `src/state/dac_state.h:38`, `src/dac_api.cpp:146–150`
- Impact: `filterMode` never persists across reboots via HAL config; would silently fail on any non-slot-0 device.
- Fix approach: Add `filterMode` field to `HalDeviceConfig`, remove from `DacState`, wire through `PUT /api/hal/devices`.

**Deprecated `audio_pipeline_register_source()` alias still present:**
- Issue: `src/audio_pipeline.cpp:760` is a one-line wrapper calling `audio_pipeline_set_source()`. Marked `// DEPRECATED alias`.
- Files: `src/audio_pipeline.cpp:760`, `src/audio_pipeline.h:55–56`
- Fix approach: Grep for callsites, migrate to `audio_pipeline_set_source()`, delete the alias.

**Deprecated v1.14 flat WS broadcast fields still emitted:**
- Issue: `src/smart_sensing.cpp:80–90` and `src/websocket_handler.cpp:2117–2120,2418–2428` still send `audioRms1/2`, `audioVu1/2`, `audioPeak1/2`, `audioVrms1/2`, `adcStatus`, `noiseFloorDbfs` as top-level flat fields alongside the correct `audio.adcs[]` array. Marked `// DEPRECATED v1.14`.
- Files: `src/smart_sensing.cpp:80–90`, `src/websocket_handler.cpp:2117–2120,2418–2428`
- Impact: Every WS broadcast carries ~10 redundant JSON keys.
- Fix approach: Confirm `web_src/` and `e2e/` do not rely on flat fields, then remove the deprecated emission blocks.

**Deferred toggle uses slot 0 fallback in `src/main.cpp`:**
- Issue: `src/main.cpp:1222` — `if (sinkSlot < 0) sinkSlot = 0;` — silently redirects a not-yet-mapped device toggle to sink slot 0 (PCM5102A) if a HAL device enable request arrives before `hal_pipeline_on_device_available()` has assigned the slot.
- Files: `src/main.cpp:1220–1228`
- Impact: Toggling ES8311 before bridge assignment would incorrectly activate PCM5102A's output path instead.
- Fix approach: Guard the activation with `sinkSlot >= 0`; log an error and skip if not yet mapped.

**Output DSP processes all 16 matrix channels regardless of active sink count:**
- Issue: `pipeline_run_output_dsp()` in `src/audio_pipeline.cpp:335–342` iterates `ch = 0..AUDIO_PIPELINE_MATRIX_SIZE-1` (all 16 channels). With only 2 active sinks, channels 2–15 are processed through `output_dsp_process()` with no consumer ever reading their result. `AUDIO_PIPELINE_MATRIX_SIZE = 16` while `AUDIO_OUT_MAX_SINKS = 8`.
- Files: `src/audio_pipeline.cpp:335–342`, `src/audio_pipeline.h:11–15`
- Impact: Wasted CPU cycles on Core 1 (the audio-only core) for every unused channel. Grows with added DSP stages.
- Fix approach: Iterate only `ch = 0..AUDIO_OUT_MAX_SINKS-1` (8 channels), since output DSP is post-matrix and keyed to sink slots.
- Source: `docs-internal/architecture/disconnect-analysis.md` D9

**FIXED — `HAL_MAX_PINS` increased to 56 with GPIO upper-bound validation:**
- Resolution: `HAL_MAX_PINS` increased from 24 to 56 in `src/hal/hal_types.h`. Added `HAL_GPIO_MAX = 54` constant. `claimPin()` now rejects GPIO > 54 with `LOG_W` and logs when pin table is full. `isPinClaimed()` has early-return guard for out-of-range values. 3 new unit tests cover high GPIO, bounds validation, and table exhaustion.
- Files: `src/hal/hal_types.h:10-11`, `src/hal/hal_device_manager.cpp:273-297,308-313`, `test/test_hal_core/test_hal_core.cpp`
- RAM impact: +128 bytes (56×4 vs 24×4). Negligible on ESP32-P4.

**Settings export includes WiFi and AP passwords in plaintext:**
- Issue: `src/settings_manager.cpp:1056,1061` unconditionally writes `appState.wifi.password` and `appState.wifi.apPassword` into the JSON export body. MQTT password is correctly excluded (line 1137/1144) but WiFi credentials are not.
- Files: `src/settings_manager.cpp:1041–1170`
- Impact: Users who share their settings export inadvertently leak WiFi and AP credentials.
- Fix approach: Remove password fields from the export serialisation; add `hasPassword` boolean indicator. Update the import handler to skip missing password fields gracefully.

---

## Known Bugs

**SDIO conflict detection relies on `activeInterface == NET_WIFI` only — misses Ethernet-with-WiFi-radio-active:**
- Symptoms: `hal_discover_devices()` checks `appState.ethernet.activeInterface == NET_WIFI` to decide whether to skip Bus EXT (GPIO 48/54). If Ethernet is the active default route but the WiFi radio is also up (client mode connected), `activeInterface` may be `NET_ETHERNET`, so the bus scan proceeds and triggers SDIO conflicts.
- Files: `src/hal/hal_discovery.cpp:32`, `src/eth_manager.cpp:42–53`
- Trigger: Ethernet cable connected while WiFi client also connected; then triggering `POST /api/hal/scan`.
- Workaround: Manual rescan is mutex-guarded; SDIO crash only occurs if both happen simultaneously.
- Fix approach: Change the check to `WiFi.status() == WL_CONNECTED || appState.wifi.connectSuccess` independent of which interface is "active".

**Single `PendingDeviceToggle` slot allows only one deferred HAL activation at a time:**
- Symptoms: Concurrent calls to `appState.dac.requestDeviceToggle()` from two WebSocket handlers in the same main-loop iteration overwrite the first request silently.
- Files: `src/state/dac_state.h:42–51`, `src/main.cpp:1216–1236`
- Trigger: Two HAL device enable/disable WS commands arrive within a single 5ms main-loop tick.
- Workaround: Space requests at least one main-loop cycle (~5ms) apart from automations or scripts.
- Fix approach: Replace single `pendingToggle` struct with a small ring buffer (capacity 4) in `DacState`.

---

## Security Considerations

**AP mode password hardcoded to `"12345678"` at compile time:**
- Risk: `DEFAULT_AP_PASSWORD "12345678"` is defined in `src/config.h:177` and assigned as default in `src/state/wifi_state.h:16`. Every device ships with the same AP password.
- Files: `src/config.h:177`, `src/state/wifi_state.h:16`, `src/wifi_manager.cpp:223,384,1065,1088,1446,1532`
- Current mitigation: Web auth (PBKDF2 + rate limiting + HttpOnly cookie) requires a separate unique password to access settings even after AP join.
- Recommendations: Generate a random per-device AP password from MAC/eFuse at first boot, similar to the web password generation in `src/auth_handler.cpp:288`. Display on TFT.

**No HTTP security headers (X-Frame-Options, CSP, CSRF protection):**
- Risk: The ESP32 web server serves all HTTP responses without `X-Frame-Options`, `X-Content-Type-Options`, `Content-Security-Policy`, or `X-XSS-Protection` headers. No CSRF token validation on POST endpoints.
- Files: `src/main.cpp` (HTTP route handlers), `src/auth_handler.cpp`
- Current mitigation: `SameSite=Strict; HttpOnly` cookie provides partial protection against cross-origin CSRF.
- Recommendations: Add a helper wrapping `server.sendHeader()` that prepends security headers. Add per-session CSRF token (generated on login, validated on state-changing POSTs).
- Source: `docs-internal/planning/improvement-plan.md` Phase 1B

**MQTT runs without TLS — credentials transmitted in plaintext:**
- Risk: `src/mqtt_handler.cpp` uses a plain `WiFiClient`. MQTT broker username and password are transmitted in plaintext on every connection. No `mqttTLS` flag exists.
- Files: `src/mqtt_handler.cpp`, `src/state/mqtt_state.h`
- Recommendations: Add `bool mqttTLS = false` to `MqttState`. Add `WiFiClientSecure` path selected by the flag. Reuse the root CA bundle from `src/ota_updater.cpp`.
- Source: `docs-internal/planning/improvement-plan.md` Phase 2A

**OTA cert validation remotely disableable via MQTT:**
- Risk: `appState.general.enableCertValidation` can be set to `false` via MQTT (`src/mqtt_handler.cpp:366–369`). A remote attacker with MQTT broker access can disable cert validation and then serve a malicious firmware image through a MITM attack.
- Files: `src/ota_updater.cpp`, `src/mqtt_handler.cpp:363–371`, `src/gui/screens/scr_settings.cpp:112`
- Recommendations: Remove the MQTT-controllable toggle or require additional local confirmation (TFT button press or web UI re-authentication) before disabling cert validation.

**WS session ID fallback still accepted (legacy path):**
- Risk: `src/websocket_handler.cpp:193–195` still accepts a raw `sessionId` in the WS auth message as a fallback after the one-time token path. Any JS code with access to the cookie value (old XSS vector) could authenticate a WS connection.
- Files: `src/websocket_handler.cpp:193–195`
- Current mitigation: Cookies are `HttpOnly` so `document.cookie` cannot read them in modern browsers.
- Recommendations: Remove the `sessionId` WS fallback once all clients are confirmed to use the token flow. The E2E fixture at `e2e/helpers/fixtures.js` already uses the token path.

**AP password logged in plaintext at DEBUG level:**
- Risk: `src/wifi_manager.cpp:389` — `LOG_D("[WiFi] Password: %s", appState.wifi.apPassword)` prints the AP password to serial at `LOG_DEBUG` level. If a serial capture is active, this leaks the credential.
- Files: `src/wifi_manager.cpp:389`
- Current mitigation: `LOG_D` is filtered out unless runtime log level is `LOG_DEBUG`.
- Recommendations: Mask the password in the log (first 2 chars + `***`).

---

## Performance Bottlenecks

**DSP `_gainBuf[256]` reused across limiter, speaker protection, and bass enhancement:**
- Problem: A single shared scratch buffer `_gainBuf` in `src/dsp_pipeline.cpp` is used by `dsp_limiter_process()`, `dsp_speaker_prot_process()`, and `dsp_bass_enhance_process()` in the same per-channel loop. If a channel has multiple of these stages, the buffer is overwritten sequentially.
- Files: `src/dsp_pipeline.cpp:149,151,1212,1238,1264`
- Cause: Intentional memory efficiency; stages are sequential so there is no concurrent access today. Adding a stage that processes `_gainBuf` non-locally could silently corrupt another stage's pass.
- Improvement path: Document the constraint explicitly in the buffer declaration comment. Stages writing `_gainBuf` must not be interleaved with stages reading it.

**`DSP_MULTIBAND_COMP` band buffers capped at 256 samples regardless of block size:**
- Problem: `dsp_multiband_comp_process()` in `src/dsp_pipeline.cpp:1287` — `int n = len > 256 ? 256 : len`. The band buffer is fixed at 256. Any caller with a longer block silently processes only the first 256 samples per DMA cycle.
- Files: `src/dsp_pipeline.cpp:1287`, `src/dsp_pipeline.cpp:89`
- Cause: Pool was designed for 256-frame DMA buffers (`I2S_DMA_BUF_LEN=256`). Current pipeline sends 256-frame blocks so the cap is never hit in practice.
- Improvement path: Add `static_assert(I2S_DMA_BUF_LEN <= 256)` or make the band buffer length a compile-time constant from `I2S_DMA_BUF_LEN`.

---

## Fragile Areas

**`dsp_stereo_width` processing happens outside the `dsp_process_channel()` loop:**
- Files: `src/dsp_pipeline.cpp:613–628` (int32 path), `src/dsp_pipeline.cpp:711–726` (float path)
- Why fragile: `DSP_STEREO_WIDTH` is a special case requiring both L and R buffers simultaneously. It is applied after `dsp_process_channel()` in two separate code paths. Adding a third entry point must repeat the same pattern or the stereo-width stage is silently skipped.
- Safe modification: Any new `dsp_process_buffer_*()` entry point must include a matching post-channel stereo-width loop.
- Test coverage: 4 unit tests cover the int32 path. The float path has no dedicated stereo-width test.

**`HAL_DISC_ONLINE` and `HAL_DISC_GPIO_ID` discovery modes are reserved stubs:**
- Files: `src/hal/hal_types.h:43–45`
- Why fragile: Both enum values have zero implementation. `hal_discovery.cpp` only handles `BUILTIN`, `EEPROM`, and `MANUAL`. If a device is assigned `HAL_DISC_ONLINE`, the GUI displays "Online" but no fetch occurs.
- Safe modification: Do not assign either value to real devices until implemented. Guard any code path checking `HAL_DISC_ONLINE` with a feature flag.
- Test coverage: No tests for these paths.

**Single `PendingDeviceToggle` slot — first request silently overwritten on concurrent calls:**
- Files: `src/state/dac_state.h:42–51`, `src/main.cpp:1216–1236`
- Why fragile: `appState.dac.pendingToggle` is a single struct. Two rapid HAL device toggles from web UI or HA automation overwrite the first before the main loop can process it.
- Safe modification: Space requests ≥ 1 main-loop cycle (~5ms) apart from automations.
- Test coverage: No test for concurrent toggle overwrite.

**FIXED — I2S MCLK/BCK/LRC pins hardcoded in `i2s_configure_adc1()` and `hal_i2s_bridge.cpp` despite HAL config:**
- Files: `src/i2s_audio.cpp`, `src/hal/hal_i2s_bridge.cpp`
- Resolution: Added `_cachedAdcCfg[2]` / `_cachedAdcCfgValid[2]` file-scope statics in `i2s_audio.cpp`. `i2s_audio_configure_adc()` populates the cache on init. `_resolveI2sPin()` helper reads `HalDeviceConfig` fields with compile-time fallback (`> 0` guard, consistent with existing `pinData` pattern). Fixed 4 callsites: `i2s_configure_adc1()` (MCLK/BCK/LRC), `i2s_audio_enable_tx()` (MCLK/BCK/LRC/DIN), `i2s_audio_set_sample_rate()` (passes cached config instead of `nullptr` — prevents pin loss on sample rate change), `i2s_log_params()` (boot log shows actual resolved GPIO numbers). `hal_i2s_bridge.cpp` BCK/LRC now resolved from `cfg->pinBck`/`cfg->pinLrc`. 8 new tests in `test_i2s_config_cache`. Branch: `fix/i2s-pin-hal-config`.

**`DSP_MULTIBAND_MAX_SLOTS = 1` limits multiband compressor to one global instance:**
- Files: `src/dsp_pipeline.cpp:69`
- Why fragile: Adding a second `DSP_MULTIBAND_COMP` stage on any channel returns `dsp_mb_alloc_slot() == -1` silently. The pool is not checked at the `dsp_add_chain_stage` API level — callers receive a stage with `mbSlot == -1` which the processor skips without error.
- Safe modification: Check `dsp_mb_alloc_slot()` return value in `dsp_add_stage()` and fail gracefully with a swap rollback, like FIR stages do.
- Test coverage: `test_multiband_slot_alloc_and_free` covers alloc/free; the overflow case (alloc beyond 1) is not tested.

**MITIGATED — WS authentication count `_wsAuthCount` can underflow on unexpected disconnect order:**
- Files: `src/websocket_handler.cpp` (`_wsAuthCount` decrement path)
- Mitigation: Dual guard `if (wsAuthStatus[num] && _wsAuthCount > 0) _wsAuthCount--` prevents underflow regardless of disconnect ordering. All WS operations run exclusively on Core 1 (single-core access — confirmed in `docs-site/docs/developer/websocket.md:771`), eliminating data-race risk without requiring a mutex. 5 dedicated tests cover all counter paths including disconnect-before-auth.
- No code change needed; flag coherence is guaranteed by the single-core access constraint.

**`_outCh` lazy allocation — NULL-pointer write risk if guard is missed:**
- Files: `src/audio_pipeline.cpp:266,274,298,337`
- Why fragile: `_outCh[ch]` is a lazy-allocated pointer array (NULL until `set_sink` is called). Most loops guard with `if (!_outCh[ch]) continue;` but any new code path that skips the NULL check and writes to an unallocated channel writes to address NULL.
- Safe modification: Always guard `_outCh[ch]` with a NULL check before writing in any new pipeline loop.

**`HalDacAdapter` `_ownsDriver = false` — dangling pointer risk if lifecycle order changes:**
- Files: `src/hal/hal_dac_adapter.h/.cpp`, `src/dac_hal.cpp:39,663`
- Why fragile: `HalDacAdapter` is constructed with `_ownsDriver = false`; the underlying `DacDriver*` is managed by `dac_hal.cpp`'s `_driverForSlot[]`. If `dac_output_deinit()` frees the driver before the adapter's destructor runs, the adapter holds a dangling pointer.
- Safe modification: Always call `dac_output_deinit()` before removing the `HalDacAdapter` from the device manager. The bridge's hybrid transient policy already enforces this for normal lifecycle transitions.

**I2S dual-master clock ordering — init sequence must not change:**
- Files: `src/i2s_audio.cpp`, `src/hal/hal_pcm1808.cpp`
- Why fragile: ADC2 must be initialised before ADC1. If the order reverses, BCK/WS/MCLK ownership conflicts and ADC2 gets no clock signal. This is documented but not enforced by any assert.
- Safe modification: When adding a third ADC, call data-only init first, then the clock master init last. The two-pass init in `i2s_audio_init_channels()` enforces this for HAL-discovered devices.

**`audioPaused` semaphore 50ms timeout — race if audio task exceeds window:**
- Files: `src/dac_hal.cpp` (deinit path), `src/audio_pipeline.cpp` (semaphore give)
- Why fragile: `dac_output_deinit()` takes `audioTaskPausedAck` with a 50ms timeout. If the audio task is blocked for >50ms (PSRAM latency spike, I2S DMA IRQ storm), deinit proceeds without confirming the task has halted, and subsequent `i2s_driver_uninstall()` may race with an ongoing `i2s_read()`.
- Safe modification: Do not reduce the 50ms timeout. If audio task priority is ever raised above 3, re-evaluate.

---

## Scaling Limits

**`HAL_MAX_DEVICES = 24` with ~14–16 builtin devices at boot:**
- Current capacity: 24 device slots. Builtin devices at boot include PCM5102A, ES8311, PCM1808×2, NS4150B, TempSensor, SigGen (generic + HAL), USB Audio, MCP4725, Encoder, Button, Buzzer, Relay, LED, Display. Approximate count: 14–16 depending on enabled features.
- Limit: Only 8–10 slots remain for expansion devices. Adding a third PCM1808, external DAC, GPIO expander, and a custom device would exhaust the table.
- Scaling path: Increase `HAL_MAX_DEVICES` in `src/hal/hal_types.h`. The arrays `_devices[]`, `_configs[]`, `_retryState[]`, `_faultCount[]` grow linearly — all static allocation, no heap impact.

**`DSP_MAX_DELAY_SLOTS = 2` and `DSP_MAX_FIR_SLOTS = 2` are tight for multi-output setups:**
- Current capacity: 2 concurrent delay stages and 2 concurrent FIR stages total across all channels and both DSP state buffers.
- Limit: An 8-output system using time-alignment delays on 4 independent outputs needs 4 delay slots. Overflow fails silently (stage added with `delaySlot == -1`, processor skips).
- Scaling path: Increase `DSP_MAX_DELAY_SLOTS` and `DSP_MAX_FIR_SLOTS` in `src/config.h`. PSRAM allocation in `dsp_pipeline_init()` must be updated accordingly.

**`AUDIO_OUT_MAX_SINKS = 8` limits output devices:**
- Current capacity: 8 sink slots (PCM5102A + ES8311 + 6 spare).
- Limit: Each HAL output device with `HAL_CAP_DAC_PATH` consumes one slot. Adding 6 more external DACs fills the table; the bridge returns `-1` for `_sinkSlotForDevice()` and the device is silently skipped.
- Scaling path: Raise `AUDIO_PIPELINE_MAX_OUTPUTS` in `src/config.h`. This also changes the routing matrix column count.

---

## Dependencies at Risk

**Embedded OTA TLS root CAs for GitHub will expire eventually:**
- Risk: `src/ota_updater.cpp:25–110` embeds three hardcoded PEM certificates (Sectigo R46, E46, DigiCert G2). A CA rotation before the documented expiry window (2038–2046) would break OTA TLS silently.
- Impact: OTA update check and download return SSL handshake error; `appState.ota.updateAvailable` never becomes true.
- Migration plan: Update embedded certs in a future firmware release. The `enableCertValidation` toggle provides an escape hatch if a cert expires before a fix is deployed.

**`WebSockets` library vendored in `lib/WebSockets/` without version pin:**
- Risk: Local copy has no lockfile or version tag reference. Upstream security vulnerabilities are invisible until a developer manually diffs.
- Impact: Build always uses the local copy (no spontaneous breakage), but security vulnerabilities in the WebSocket handshake code would not be auto-discovered.
- Migration plan: Tag the vendored copy with the upstream commit SHA in a README comment. Periodically diff against upstream.

**`peaceiris/actions-gh-pages@v3` in `docs.yml` is unpinned by SHA:**
- Risk: Third-party GitHub Actions referenced by mutable tag can be force-pushed by the upstream maintainer — a supply chain vector.
- Files: `.github/workflows/docs.yml:82`
- Impact: Only the docs deployment workflow is affected; the firmware `tests.yml` uses only official `actions/*` at `@v4`.
- Migration plan: Pin to `peaceiris/actions-gh-pages@<sha>`.

---

## Missing Critical Features

**Online HAL device database (`HAL_DISC_ONLINE`) not implemented:**
- Problem: `src/hal/hal_types.h:45` defines `HAL_DISC_ONLINE = 4` as "Fetched from GitHub YAML DB". No fetch logic, no YAML parser, and no network DB URL exists anywhere in the codebase.
- Blocks: Users cannot add expansion devices from an online catalog; they must manually enter compatible strings.

**Platform archive mirror for OTA not implemented:**
- Problem: OTA downloads firmware directly from `objects.githubusercontent.com` CDN. No fallback mirror or local update server exists. If GitHub's CDN is unreachable (firewalled environments, GitHub outage), OTA fails silently.
- Blocks: Enterprise or air-gapped deployments cannot perform OTA updates.

**No HTTPS/TLS on the web interface:**
- Problem: The embedded HTTP server on port 80 has no SSL wrapper. Session cookies and API responses are cleartext on LAN.
- Blocks: Strict security posture requirements.

---

## Documentation Site Gaps

**`ANTHROPIC_API_KEY` secret not configured in GitHub:**
- Issue: `.github/workflows/docs.yml:50–57` checks for `secrets.ANTHROPIC_API_KEY`. If absent, the workflow logs a warning and skips doc generation, building only existing committed docs. The `Documentation` CI job passes but docs are never updated on source changes.
- Files: `.github/workflows/docs.yml:50–59`, `tools/generate_docs.js:74–82`
- Fix: Add `ANTHROPIC_API_KEY` as a repository secret in GitHub → Settings → Secrets and Variables → Actions. Key must have access to `claude-sonnet-4-6`.

**GitHub Pages not yet enabled:**
- Issue: The `peaceiris/actions-gh-pages@v3` action pushes to the `gh-pages` branch, but GitHub Pages must be explicitly enabled on the repository to serve content publicly.
- Fix: GitHub → Settings → Pages → Source: Deploy from branch → `gh-pages` / `/ (root)`.

**`tools/generate_docs.js` never run end-to-end with a live API key:**
- Issue: The Claude API generation pipeline is written and wired into CI, but actual doc generation and Docusaurus build have never been validated together. The `--dry-run` mode works. Live generation against this repo's actual source files remains untested.
- Files: `tools/generate_docs.js`, `tools/doc-mapping.json`, `tools/prompts/`, `.github/workflows/docs.yml`
- Risk: Generated docs may trip the MDX `{variable}` JSX expression gotcha or produce malformed markdown that fails `npm run build`.
- Fix approach: Run `node tools/generate_docs.js --sections developer/overview` locally with a valid `ANTHROPIC_API_KEY`, then `cd docs-site && npm run build` to validate the full pipeline before relying on CI.

**`web_src/css/00-tokens.css` is auto-generated — must be regenerated after token changes:**
- Issue: `web_src/css/00-tokens.css` is produced by `node tools/extract_tokens.js` from `src/design_tokens.h`. `tools/build_web_assets.js` reads all `*.css` files sorted alphanumerically, so `00-tokens.css` is always included first (correct). However, there is no CI check that verifies `00-tokens.css` is current with `src/design_tokens.h`.
- Files: `web_src/css/00-tokens.css`, `src/design_tokens.h`, `tools/extract_tokens.js`
- Risk: A developer changes `src/design_tokens.h` and runs `node tools/build_web_assets.js` without first running `node tools/extract_tokens.js`. The firmware ships with stale web UI theme colours.
- Fix approach: Add `node tools/extract_tokens.js` as a step before `node tools/build_web_assets.js` in the developer workflow, or add a CI check that verifies the generated file matches the current `design_tokens.h`.

---

## Platform / Hardware Constraints

**Core 1 reserved for audio — no new tasks may be pinned there:**
- Rule: Core 1 runs only `loopTask` (Arduino main loop, priority 1) and `audio_pipeline_task` (priority 3). No additional tasks may be pinned to Core 1.
- Files: `src/config.h` (`TASK_CORE_AUDIO`), `src/audio_pipeline.cpp`
- Impact of violation: Any new task on Core 1 competes with `audio_pipeline_task` for scheduler time, causing audio dropouts or DMA buffer underruns.

**I2C Bus 0 (GPIO 48/54) — never access while WiFi radio is active:**
- Rule: GPIO 48 (SDA) and GPIO 54 (SCL) share the SDIO bus with the ESP32-C6 WiFi co-processor. Any `Wire.beginTransmission()` on these pins while WiFi is active causes `sdmmc_send_cmd` errors and MCU reset.
- Files: `src/hal/hal_types.h:59`, `src/hal/hal_discovery.cpp`
- Safe pattern: Check `WiFi.status() != WL_CONNECTED` before any I2C transaction on Bus 0.

**HAL_MAX_DEVICES = 24 — hard static array limit:**
- Current count at boot: ~14–16 onboard devices. ~8–10 expansion slots remaining. See Scaling Limits above.
- Files: `src/hal/hal_types.h:9`, `src/hal/hal_device_manager.h:74–78`

**PSRAM required for DSP delay lines:**
- Rule: DSP delay lines use `ps_calloc()`. If PSRAM is unavailable, the fallback `calloc()` requires a 40KB free-heap pre-flight check. On a device without PSRAM, complex FIR/delay configs will fail allocation silently with a log warning.
- Files: `src/dsp_pipeline.cpp`, `src/config.h` (`HEAP_RESERVE_BYTES`)

**LovyanGFX on P4 — no DMA for SPI display:**
- Rule: TFT display uses synchronous SPI on P4 (GDMA completion not wired in LovyanGFX for P4). All display writes block the GUI task. Non-blocking SPI transfers are not available.
- Files: `src/gui/lgfx_config.h`

**ESP-DSP pre-built library compiled for Xtensa LX7 (ESP32-S3), not for P4 RISC-V:**
- Rule: `libespressif__esp-dsp.a` is built for Xtensa LX7 SIMD. The ESP32-P4 runs RISC-V and executes the library correctly but without the assembly acceleration. Biquad IIR, FIR, and FFT throughput is lower than on S3.
- Files: `platformio.ini` (lib path), `lib/esp_dsp_lite/` (native-test ANSI-C fallback)
- Impact: Acceptable at current DSP stage counts. With >32 biquad stages or FIR >256 taps, monitor Core 1 task watermark via the task monitor (`debugTaskMonitor`).

---

## Test Coverage Gaps

**No unit test for the float `dsp_process_buffer_float()` stereo-width path:**
- What's not tested: Stereo-width mid-side processing in `src/dsp_pipeline.cpp:711–726` (float entry point).
- Risk: A regression in the float path's mid-side math would not be caught before deployment.
- Priority: Low (same algorithm as the tested int32 path; logic is shared)

**No test for `PendingDeviceToggle` overwrite race:**
- What's not tested: Concurrent `requestDeviceToggle()` calls overwriting the first request in the same main-loop tick.
- Files: `src/state/dac_state.h:46–51`, `src/main.cpp:1216–1236`
- Risk: Silent loss of a device activation/deactivation request.
- Priority: Medium

**No test for HAL device slot exhaustion (≥ 24 devices registered):**
- What's not tested: `HalDeviceManager::registerDevice()` returning -1 when `_count == HAL_MAX_DEVICES`. Downstream behaviour (bridge skipping the device, pipeline not receiving a source) is untested.
- Files: `src/hal/hal_device_manager.cpp`, `src/hal/hal_pipeline_bridge.cpp`
- Risk: Silent data loss when expansion devices exceed capacity.
- Priority: Medium

**No test for `HAL_DISC_GPIO_ID` or `HAL_DISC_ONLINE` discovery paths:**
- What's not tested: Both enum values have no implementation; no test scaffolding exists to define the expected contract.
- Files: `src/hal/hal_types.h:43–45`
- Priority: Low (implement first, then test)

**No integration test for Ethernet + WiFi simultaneous active (SDIO scan bug):**
- What's not tested: HAL discovery SDIO skip logic when `activeInterface == NET_ETHERNET` but the WiFi radio is active.
- Files: `src/hal/hal_discovery.cpp:32`
- Risk: Silent MCU reset or SDIO corruption on dual-interface hardware.
- Priority: Medium

**No test for `DSP_MULTIBAND_MAX_SLOTS` overflow (second instance allocation):**
- What's not tested: `dsp_mb_alloc_slot()` returning -1 on a second `DSP_MULTIBAND_COMP` stage; processor skipping it silently.
- Files: `src/dsp_pipeline.cpp:69,1287`
- Priority: Low

**Docs CI pipeline requires two manual GitHub setup steps with no in-repo guidance:**
- What's not configured: `ANTHROPIC_API_KEY` secret and GitHub Pages source must be configured manually. The workflow degrades gracefully but this is not self-documenting for new contributors.
- Files: `.github/workflows/docs.yml:52–57,81–86`
- Priority: Low — add a setup section to `docs-site/README.md` or CLAUDE.md.

---

*Concerns audit: 2026-03-09*
