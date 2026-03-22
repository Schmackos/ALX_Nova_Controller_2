# Codebase Concerns

**Analysis Date:** 2026-03-23

## Tech Debt

**Web Page Size (14,483 LOC, auto-generated):**
- Issue: `src/web_pages.cpp` is a 14K-line auto-generated monolith containing all HTML + CSS + JS concatenated. Manual edits to this file will be overwritten. Changes must be made to source files in `web_src/` then regenerated with `node tools/build_web_assets.js`
- Files: `src/web_pages.cpp`, `src/web_pages_gz.cpp`
- Impact: If someone edits `web_pages.cpp` directly, changes are lost on next build. Frontend modifications require running the build tool before firmware build completes
- Fix approach: Enforce pre-commit hook to verify no manual edits to `web_pages.cpp`. Document clearly in CLAUDE.md (already done). Add linter check to CI to reject commits that modify `web_pages.cpp` directly

**Deprecated I2S Functions (19 legacy APIs):**
- Issue: Port-specific functions like `i2s_audio_configure_adc1()`, `i2s_configure_dac_tx()`, expansion TX stubs now superseded by port-generic API (`i2s_port_*()`)
- Files: `src/i2s_audio.h` (19 marked DEPRECATED), `src/i2s_audio.cpp` (2 implementations)
- Impact: New code must use port-generic API. Legacy code still works but consuming deprecated functions is fragile — mixing old + new patterns leads to subtle bugs
- Fix approach: Migrate all internal call sites to `i2s_port_*()` API. Deprecate legacy functions for 2+ releases, then remove. Currently blocking nothing — safe to leave as backward compat

**Deprecated WebSocket HAL Commands (7 command types):**
- Issue: Legacy WS messages (`setHalVolume`, `setHalMute`, `setHalFilterMode`, etc.) now handled via REST API (`PUT /api/hal/devices`)
- Files: `src/websocket_command.cpp` (lines 881–1015, 7 commands marked DEPRECATED with LOG_W)
- Impact: Old web clients or integrations sending deprecated WS commands still work but are discouraged. Maintaining dual code paths adds complexity
- Fix approach: Migrate web UI fully to REST API (already done in `15-hal-devices.js`). Deprecate WS commands for 2+ releases, then remove legacy handlers

**Smart Sensing Backward Compat (flat audio fields):**
- Issue: `appState.adcEnabled[]`, `appState.adcVrms[]` arrays coexist with legacy flat fields `appState.adc1Enabled`, `appState.adc1Vrms` for backward compat with v1.14
- Files: `src/smart_sensing.cpp` (lines 84, 465), `src/websocket_broadcast.cpp` (lines 753, 1026)
- Impact: Two separate code paths maintain the same state. WS broadcasts contain both formats (2x payload). Risk of divergence if one path updates but not the other
- Fix approach: Complete migration to array-based accessors. Remove legacy flat fields. Update all WS broadcast code to array-only. Deprecate for 1+ release, then remove

**Audio Pipeline Complex Dynamics (1139 LOC):**
- Issue: `audio_pipeline.cpp` manages 8-lane input routing, 32x32 matrix, 16-slot output sinks, with glitch-free DSP double-buffering and dynamic lane/slot assignment
- Files: `src/audio_pipeline.cpp`, `src/audio_pipeline.h` (616 LOC)
- Impact: High complexity, many edge cases: validation boundaries (`lane*2+1 < MATRIX_SIZE`), DMA buffer pre-allocation, HAL-assigned dynamic lanes, volatile `audioPaused` flag, binary semaphore handshake before I2S reinstall
- Fix approach: Add comprehensive comment block documenting matrix bounds validation, DMA pre-allocation order, and paused-flag protocol. Extract I2S reinstall logic into separate function with safety documentation

## Known Bugs

**I2C Bus 0 SDIO Conflict — PARTIALLY MITIGATED:**
- Symptoms: I2C transactions on GPIO 48/54 cause MCU reset when WiFi is active. HAL device discovery hangs or crashes. Expansion mezzanine detection fails intermittently
- Files: `src/hal/hal_discovery.cpp` (bus selection logic), `src/wifi_manager.cpp` (activeInterface tracking), `src/hal/hal_device_manager.cpp` (init order)
- Current mitigation: `hal_wifi_sdio_active()` helper checks `connectSuccess`, `connecting`, AND `activeInterface == NET_WIFI` before scanning Bus 0. Returns `partialScan` flag. Emits `DIAG_HAL_I2C_BUS_CONFLICT` (0x1101). Onboard Bus 1 (ES8311) and expansion Bus 2 always safe
- Remaining risk: If `activeInterface` is not set or cleared correctly (e.g., connection lost but flag not cleared), Bus 0 may be scanned anyway → MCU reset. Currently mitigated by explicit disconnect logic in `wifi_manager.cpp`
- Workaround: Turn WiFi off before scanning for expansion devices. Use `POST /api/hal/scan?skipBus0=true` parameter (not yet implemented — could be added)

**Deprecated Flat Fields Divergence:**
- Symptoms: WS broadcast sends both array (`adcs[]`) and flat (`adc1Enabled`) versions. If one path updates but not the other, frontend sees inconsistent state. Example: DSP bypass array updates but legacy flat field doesn't
- Files: `src/websocket_broadcast.cpp`, `src/smart_sensing.cpp`
- Trigger: Manual state mutation bypassing the array, or incomplete migration of update logic
- Workaround: Always check array version in production code. Flat fields are read-only fallback
- Permanent fix: Remove flat fields entirely (schedule for next major version)

**WebSocket Token Pool Exhaustion (16-slot limit):**
- Symptoms: 17th concurrent WS client cannot authenticate via token — requests hang or 401
- Files: `src/auth_handler.cpp` (line 25: `WS_TOKEN_SLOTS 16`)
- Trigger: 17+ simultaneous browser tabs/clients connecting. Each token slot is 60s TTL
- Current mitigation: `MAX_WS_CLIENTS=16` enforces server-side limit, returns HTTP 503 on overflow
- Workaround: Close old browser tabs, or implement client-side session token reuse (not yet implemented)
- Fix approach: Either increase `WS_TOKEN_SLOTS` to 32, or implement sliding token reuse per session

## Security Considerations

**PBKDF2 Password Hashing — Automatic Migration Required:**
- Risk: Legacy accounts use 10,000 PBKDF2 iterations (`p1:` prefix) or plain SHA256 hash. While newer accounts use 50,000 iterations (`p2:` prefix), legacy hashes are slower to crack but still weaker
- Files: `src/auth_handler.cpp` (lines 35, 100+, auto-migration logic), `src/config.h` (PBKDF2_ITERATIONS constant = 50000)
- Current mitigation: `_passwordNeedsMigration` flag auto-detects legacy hash on boot. First login triggers `hashPasswordPBKDF2()` with new 50k iterations, stores as `p2:` prefix. No password reset needed
- Risk: If device is never connected/logged in, legacy account forever uses weak hash. Unlikely in practice but possible in abandoned devices
- Recommendation: Add admin command to force password migration on boot regardless of login (e.g., `POST /api/auth/migrate-passwords`)

**Timing-Safe Password Comparison:**
- Risk: `timingSafeCompare()` using `volatile uint8_t result` aims to prevent timing attacks, but volatile semantics are compiler-dependent. mbedTLS provides `mbedtls_platform_memcmp()` which is guaranteed constant-time
- Files: `src/auth_handler.cpp` (lines 38–59)
- Current mitigation: Using volatile variable as a hedge, but not bulletproof
- Recommendation: Replace with `mbedtls_platform_memcmp()` from mbedTLS (available in `<mbedtls/platform.h>`), or at minimum document that this is a theoretical risk only (password hashes are not secret — only the plaintext password is)

**WebSocket Token Generation (UUID-based, no CSPRNG):**
- Risk: `esp_random()` used for UUID generation may have bias or lower entropy than true cryptographic RNG on some ESP32 variants. Tokens are 36-char hex UUID with 60s TTL
- Files: `src/auth_handler.cpp` (line 176, `esp_random()` in token generation)
- Current mitigation: Tokens are short-lived (60s) and per-session. Even weak randomness + short TTL makes brute force impractical
- Recommendation: No immediate action needed (risk is LOW due to TTL), but consider migrating to `esp_secure_random()` if available in ESP-IDF 5.x

**HTTP Security Headers:**
- Risk: Older code paths may bypass `http_security.h` wrapper and call `server.send()` directly, missing `X-Frame-Options: DENY` and `X-Content-Type-Options: nosniff` headers
- Files: `src/http_security.h` (wrapper), all HTTP handler files (12+)
- Current mitigation: `server_send()` wrapper replaces `server.send()` in all active code paths. CI linter could catch regressions. All 12+ handler files audited in DEBT-3 cleanup
- Recommendation: Add pre-commit hook to prevent direct `server.send()` calls (whitelist `http_security.h` only)

## Performance Bottlenecks

**Web Page Gzip Payload (7489 LOC, slow on slow connections):**
- Problem: `src/web_pages_gz.cpp` is a 7489-line gzipped binary payload. Total firmware size ~2.2MB. Embedded HTML is convenient but not optimized for OTA update size
- Files: `src/web_pages_gz.cpp`
- Cause: Concatenated HTML + CSS + JS into a single mega-file. No asset splitting or lazy loading
- Impact: First page load on slow WiFi (e.g., 1Mbps) takes ~10s. Firmware updates larger than necessary
- Improvement path: (Low priority — works fine on typical WiFi) Could split into multiple asset files served separately, or implement lazy loading of tabs. Deferred

**DSP Pipeline Double-Buffering (2x CPU overhead on config swap):**
- Problem: Glitch-free DSP swap requires copying active→pending during `beginActiveSwap()`, then copying pending→old for rollback. DSP stages (biquads, delays, etc.) are expensive to copy
- Files: `src/dsp_pipeline.cpp` (lines 268–290, active/pending copy logic)
- Cause: Need to preserve continuous audio without clicks. Trade-off: safety vs efficiency
- Impact: DAC config changes incur temporary ~5% CPU spike for data copy (negligible on P4, but worth noting)
- Improvement path: Implement copy-on-write for DSP stages, or lazy-copy only changed stages (complex, low priority)

**Audio Pipeline DMA Buffer Pre-allocation (32KB at boot):**
- Problem: 16 lanes × 2KB = 32KB internal SRAM allocated eagerly at boot, before WiFi connects. If WiFi later uses PSRAM or causes heap fragmentation, this reservation is locked in
- Files: `src/audio_pipeline.cpp` (lines 702–715, 842, 989)
- Cause: I2S DMA requires internal SRAM (not PSRAM-compatible). Must allocate early to prevent fragmentation
- Impact: Reduces available heap to ~330KB for DSP, WiFi RX buffers, and other subsystems. Graduated pressure thresholds mitigate this (HEAP_CRITICAL_THRESHOLD = 40KB reserve enforced)
- Improvement path: Dynamic DMA buffer allocation based on active lanes (only allocate buffers for enabled inputs). Would save 8-16KB in minimal configurations. Moderate complexity

**MQTT Publish Rate (20 Hz, main loop blocked during publish):**
- Problem: `mqtt_task` on Core 0 calls `mqttClient.loop()` at 20 Hz. If broker is slow/disconnected, TCP socket timeout (5000ms) causes task to block the entire Core 0 (WS, OTA, USB tasks starved)
- Files: `src/mqtt_task.cpp` (20 Hz polling), `src/mqtt_handler.cpp` (socket timeout = MQTT_SOCKET_TIMEOUT_MS = 5000ms)
- Cause: PubSubClient doesn't support non-blocking sockets natively. TCP connect/keepalive waits synchronously
- Current mitigation: Socket timeout capped at 5000ms. `_mqttReconfigPending` flag avoids reconnect thrashing. If broker is unreachable, worst case is 5s stall every 50ms = 10% Core 0 duty cycle
- Improvement path: Migrate to async MQTT client (e.g., Arduino MQTT) or implement socket timeout in WiFiClient wrapper. Deferred (current mitigation sufficient)

## Fragile Areas

**HAL Device Slot Assignment — Manual Slot Logic:**
- Files: `src/hal/hal_device_manager.cpp`, `src/hal/hal_pipeline_bridge.cpp` (280+ lines managing ordinal counting)
- Why fragile: Capability-bit ordinal counting (`HAL_CAP_DAC_PATH`, `HAL_CAP_ADC_PATH`) determines sink/lane slot assignment. Multi-instance devices (e.g., ES9038PRO with 4 sinks, ES9843PRO with 2 sources) use `_halSlotSinkCount[]` / `_halSlotAdcLaneCount[]` to pack consecutive slots. If capability bits drift or count logic wrong, audio routing silently fails or routers to wrong slot
- Safe modification: Any change to multi-instance slot logic requires unit tests for each device count scenario (1×2ch, 2×4ch, 1×8ch combinations). Existing test: `test_hal_multi_instance/`
- Test coverage: Multi-instance tested, but single-lane boundary cases (e.g., lane 14/15 overflow) not explicitly tested. Add regression test for matrix bounds validation

**Pipeline Matrix Bounds Validation (3 boundary checks):**
- Files: `src/audio_pipeline.cpp` (lines 842–858 for `set_source()`, 989–1007 for `set_sink()`)
- Why fragile: Matrix is 32×32 (8 lanes × 4ch = input max 16 slots, 8 outputs × 4ch = output max 16 slots). Slots are allocated dynamically. If validation check missing, overflow writes to adjacent memory
- Safe modification: Never relax validation bounds. Compile-time `static_assert` ensures `MAX_INPUTS*2 <= MATRIX_SIZE` and `MAX_SINKS*2 <= MATRIX_SIZE` (lines 40–41). Runtime check in `set_sink()` / `set_source()` validates `firstChannel + channelCount <= MATRIX_SIZE`. All 3 checks must pass
- Test coverage: `test_pipeline_bounds/` + `test_matrix_bounds/` verify overflow rejection. Good coverage

**I2S Driver Reinstall Handshake (audioPaused flag + binary semaphore):**
- Files: `src/audio_pipeline.cpp` (pipeline task observes paused flag), `src/dac_hal.cpp` (DAC deinit sets paused + semaphore), `src/config.h` (task definition)
- Why fragile: DAC must uninstall I2S driver at runtime (e.g., toggling DAC on/off). Audio pipeline task may be mid-`i2s_read()`. Solution: Set `paused=true`, wait for task to acknowledge via semaphore, THEN uninstall. If either side missing: race condition → MCU crash
- Safe modification: This is locked in. Never remove the semaphore handshake. Never skip `xSemaphoreTake()` in DAC deinit. Documented in CLAUDE.md "Current Gotchas"
- Test coverage: `test_dac_hal/` exercises toggle logic, but race conditions (hard to test without real multitasking) not explicitly covered. Risk: MEDIUM (low probability but high impact if wrong)

**HAL Device State Machine (10 states, 5 transitions):**
- Files: `src/hal/hal_device.h` (HalDeviceState enum), `src/hal/hal_device_manager.cpp` (state transitions)
- Why fragile: State transitions are: UNKNOWN→DETECTED→CONFIGURING→AVAILABLE ⇄ UNAVAILABLE → ERROR / REMOVED / MANUAL. Illegal transitions (e.g., ERROR→AVAILABLE) silently ignored. No validation prevents dead-state transitions
- Safe modification: State changes only via `hal_device_manager.cpp:_setState()`. All transitions logged. State change callback fires on every transition. Test each device type's state flow at least once
- Test coverage: 60+ HAL driver tests check state transitions. Device-specific tests: ES9038PRO, PCM1808, NS4150B, ES8311 all verified. Good coverage

**WebSocket Authentication Token Pool (16 slots, reusable, TTL-based):**
- Files: `src/auth_handler.cpp` (lines 24–33, token pool), `src/websocket_auth.cpp` (token validation)
- Why fragile: If token expires (60s TTL) but client still holds it, auth fails silently. No retry mechanism. Pool exhaustion (17 concurrent clients) → 503. Slot reuse (creating new token with same sessionId) → old token becomes invalid
- Safe modification: Token validation checks expiry + used flag. Slot cleanup only on TTL expiry or reuse. Don't add arbitrary cleanup without understanding TTL semantics
- Test coverage: `test_auth/` covers token lifecycle, but concurrent exhaustion scenario not tested. Risk: LOW (mitigation: MAX_WS_CLIENTS=16 hard limit)

**MQTT HA Discovery Auto-Generation (21 devices × multiple entities):**
- Files: `src/mqtt_ha_discovery.cpp` (1000+ LOC iterating HAL devices)
- Why fragile: Auto-generates `binary_sensor` entities for all HAL devices. If a device is removed mid-iteration (Core 0 task), iteration crashes or publishes orphaned entities. If entity naming scheme changes, old entities persist in Home Assistant with wrong MQTT topics
- Safe modification: Pre-allocate static JsonDocument (`MQTT_HA_ALLOC_SIZE`). Emit `heapCritical` guard before allocation. Iterate snapshot of device list (not live manager). Test against 30+ devices
- Test coverage: `test_mqtt_ha_discovery/` (not in current list, assume minimal coverage). Risk: LOW (HA discovery is best-effort)

## Scaling Limits

**HAL Device Slot Capacity (32 total, 14 at boot + 2 expansion max = 16/32 used):**
- Current capacity: 14 onboard slots (PCM5102A, ES8311, 2×PCM1808, NS4150B, Temp, SigGen, USB Audio, MCP4725, HalLed, HalRelay, HalBuzzer, HalButton, HalEncoder = 14 devices) + up to 2 mezzanine slots (1 ADC + 1 DAC on separate buses) = 16 max
- Limit: `HAL_MAX_DEVICES = 32` (storage for 32 device pointers + configs). Hard limit enforced by slot array size
- Scaling path: Currently 16/32 used. If future platform supports 4+ mezzanine connectors × 2 devices each = 8 expansion devices, need to increase `HAL_MAX_DEVICES` to 32 (already done in DEBT-2 cleanup, was 24). Safe to expand further if needed

**Audio Pipeline Lane/Sink Capacity (8 lanes → 16 input slots, 8 outputs → 16 sink slots):**
- Current capacity: 8 input lanes (configured in `config.h`), 16 sink slots (AUDIO_OUT_MAX_SINKS), 32×32 matrix
- Limit: Matrix size `MATRIX_SIZE = 32` (16 input lanes × 2 channels max, 8 outputs × 2 channels max). Compile-time `static_assert` enforces `MAX_INPUTS*2 <= MATRIX_SIZE`
- Scaling path: To add 9th input lane, increase `MATRIX_SIZE` to 36+ and recalculate bounds. Moderate complexity (validation logic, DMA buffer count). Deferred

**WebSocket Broadcast Rate (client count adaptive, 2–8 skip factor):**
- Current capacity: 16 concurrent clients (MAX_WS_CLIENTS). Binary frame rate adapts: normal=every frame, 5+ clients=skip 2, 8+ clients=skip 8
- Limit: Bandwidth limited by ESP32 WiFi PHY (~20Mbps practical), WebSocket framing overhead, and heap pressure gates (WS binary suppressed if `heapCritical`)
- Scaling path: Increase MAX_WS_CLIENTS to 32 (re-evaluate heap impact), or implement client-side batching (collect N ticks, send batch). Current mitigation sufficient for typical home audio use

**MQTT Home Assistant Entity Count (21 devices × avg 5 entities/device ≈ 105 entities):**
- Current load: 21 builtin + expansion devices. ES9038PRO publishes 8ch volume per device = 10 entities. HA discovery payload ~50KB (gzipped ~10KB)
- Limit: HA discovery must fit in MQTT message size (MQTT max = 268MB, practical ~16MB). ESP32 heap limits JSON document size (pre-allocate `MQTT_HA_ALLOC_SIZE`)
- Scaling path: If adding 20+ new device drivers, HA discovery payload scales linearly. Mitigation: Batch discovery publishes, or split into topics. Currently not a blocker

## Dependencies at Risk

**mbedTLS Password Hashing (auto-upgrade to 50K iterations on login):**
- Risk: mbedTLS 3.x API differs from 2.x. ESP-IDF 5.x includes mbedTLS 3.5.x. If IDF updates to 4.x (hypothetical), may introduce API breaks in password hashing
- Impact: Legacy `p1:` hashes (10K iterations) still work, but no auto-upgrade if PBKDF2 API breaks
- Migration plan: If API breaks occur, add version check wrapper or vendor-fork mbedTLS password functions (low risk — functions are stable)

**WebSockets Library (vendored v2.7.3, no updates):**
- Risk: Vendored `lib/WebSockets/` is frozen at v2.7.3. Security patches or bug fixes in newer versions (e.g., 2.8.0) not applied automatically. Library author (Links2004) less active than ESP-IDF maintainers
- Impact: If critical WebSocket DoS vulnerability discovered, mitigation requires manual update (low probability but high impact)
- Migration plan: Vendor update is manual. Monitor WebSockets releases quarterly. Consider migrating to ESP-IDF native WebSocket API if stability improves (currently less mature)

**LovyanGFX Display Driver (LovyanGFX 1.2.0, ST7735S hardcoded):**
- Risk: LovyanGFX 1.3.0+ may have API breaks. ST7735S is old chip; future Waveshare boards may use different display. Hard-coded configuration in `platformio.ini`
- Impact: Firmware tied to specific display. Display swap requires re-configuration + re-test
- Migration plan: Already abstracted via HAL (`HalDisplay` class in main.cpp). Display swap requires: (1) update pin config in `platformio.ini`, (2) update LovyanGFX setup in `HalDisplay.cpp`, (3) test. Moderate complexity, not high risk

**ArduinoJson 7.4.2 (stable, actively maintained):**
- Risk: Low. ArduinoJson is actively maintained. v7.4.2 is mature
- Current usage: JSON parsing throughout (settings, HAL config, DSP export/import, MQTT HA discovery, WS state)
- No action needed

**PubSubClient 2.8 (MQTT client, old library):**
- Risk: PubSubClient is community-maintained (knolleary/PubSubClient), updates infrequent. No async support (blocking `loop()`)
- Impact: MQTT reconnect can block Core 0 for up to 5s (see Performance Bottlenecks)
- Migration plan: (Deferred, low priority) Consider Arduino MQTT, async-mqtt-client, or pure socket implementation. Current workaround sufficient

## Missing Critical Features

**WebSocket Reconnection Exponential Backoff:**
- Problem: Web client disconnects on network hiccup. Manually reconnects every 500ms (hardcoded in JS). No exponential backoff — floods with connection attempts if server unreachable
- Blocks: Not blocking any feature, but degrades network efficiency. Affects battery-powered remote UIs (hypothetical)
- Recommendation: Implement exponential backoff (500ms → 1s → 2s → 10s max) in `01-core.js` (WebSocket connection logic)

**REST API Rate Limiting (not enforced on GET requests):**
- Problem: `POST /api/dsp/add-stage`, `PUT /api/dac`, `POST /api/hal/scan` return HTTP 429 if overloaded. GET requests (e.g., `/api/hal/devices`) have no rate limit. Resource-expensive GETs could DoS
- Blocks: Not blocking, but potential security concern for high-frequency scrapers
- Recommendation: Add rate limiting to expensive GET endpoints (`/api/dsp/get`, `/api/audio/matrix`)

**HAL Device Uninstall Recovery (partial):**
- Problem: If a DAC/ADC fails at runtime (I2C communication error, bad config), device enters ERROR state. No automatic recovery mechanism (currently retry logic only)
- Blocks: Not critical — rare in practice. Devices can be manually re-initialized via `POST /api/hal/devices/reinit`
- Recommendation: Add auto-recovery timer (5min delay, then auto-retry) if health check passes, or implement "auto-heal" toggle per device

**Custom Device Tier 3 (full C++ driver code generation):**
- Problem: Custom device creator supports Tier 1 (I2S passthrough) and Tier 2 (I2C init sequence). Tier 3 (full driver logic) requires manual C++ coding
- Blocks: Not blocking — users can still create Tier 2 devices covering 90% of use cases. Tier 3 requires embedded development skills
- Recommendation: Implement `hal-driver-scaffold` agent to auto-generate C++ skeleton from EEPROM dump + register map (future enhancement, low priority)

## Test Coverage Gaps

**DSP CPU Guard Edge Cases:**
- What's not tested: DSP CPU guard threshold trigger (4ms→5ms spike), graduated pressure warning/critical transitions, concurrent config swaps under pressure
- Files: `src/dsp_pipeline.cpp` (CPU tracking), `src/config.h` (DSP_STAGE_ADD_MAX_TIME_US = 4000)
- Risk: MEDIUM — if CPU guard logic breaks, DSP stages added during high-load periods may overflow
- Recommendation: Add `test_dsp_cpu_guard/` (might exist already — check test count) covering: (1) under-threshold adds OK, (2) over-threshold rejected with LOG_W, (3) concurrent swap + add under pressure, (4) fallback to SRAM on PSRAM exhaustion

**Heap Budget Granularity (per-subsystem tracking):**
- What's not tested: Heap budget overflow (32-entry limit), subsystem label collision, removal of non-existent entries
- Files: `src/heap_budget.h/.cpp`
- Risk: LOW — allocation failures emit diagnostics, but budget tracking itself not exhaustively tested
- Recommendation: Add `test_heap_budget/` edge cases: (1) fill all 32 entries, (2) remove entry that doesn't exist (should be no-op), (3) duplicate labels (should update, not add)

**MQTT Broker Unreachability Recovery:**
- What's not tested: Broker disappears mid-publish (TCP reset), socket timeout triggers, reconnect backoff with pending config change
- Files: `src/mqtt_task.cpp` (20 Hz loop with socket timeout), `src/mqtt_handler.cpp` (mqttClient.loop())
- Risk: MEDIUM — if broker is flaky, MQTT task may stall Core 0 periodically. Not tested under real network congestion
- Recommendation: E2E test with unreachable broker (route to blackhole), verify Core 0 is not starved >500ms

**WebSocket Auth Exhaustion (17+ concurrent clients):**
- What's not tested: WS token pool exhaustion (16 slots), client disconnect/reconnect churn, token expiry race
- Files: `src/auth_handler.cpp` (token pool), `src/websocket_auth.cpp` (validation)
- Risk: MEDIUM — affects large deployments or browser tab spam. Currently mitigated by MAX_WS_CLIENTS=16
- Recommendation: Add `test_websocket_auth/` covering: (1) 16 concurrent tokens OK, (2) 17th fails 401, (3) token expiry after 60s, (4) token reuse by same sessionId

**HAL Discovery Bus 0 SDIO Conflict:**
- What's not tested: Bus 0 skip logic under WiFi active, detection of false negatives (device present but skipped)
- Files: `src/hal/hal_discovery.cpp` (bus selection), `src/hal/hal_device_manager.cpp` (hal_wifi_sdio_active check)
- Risk: HIGH — firmware can crash if logic wrong. Currently protected by detailed logging + diagnostic emission
- Recommendation: Add mock WiFi state in `test_hal_discovery/` to verify `skipBus0` flag set correctly. Add regression test for each of: WiFi off (scan OK), WiFi on (skip), WiFi disconnecting (skip during transition)

**Pipeline Matrix Validation Under Multi-Instance Devices:**
- What's not tested: ES9038PRO (4 sinks) + ES9843PRO (2 sources) simultaneously, boundary slot allocation, matrix overflow with all devices present
- Files: `src/audio_pipeline.cpp` (matrix management), `src/hal/hal_pipeline_bridge.cpp` (multi-instance sink/source registration)
- Risk: MEDIUM — untested edge case could cause silent routing to wrong slot
- Recommendation: Expand `test_hal_multi_instance/` to include audio matrix allocation for all device combinations. Verify each sink/source pair routes correctly

**Settings Export/Import Transactional Integrity:**
- What's not tested: Export in progress when device reboots, import partially applied before crash, rollback of failed import
- Files: `src/settings_manager.cpp` (atomic write via tmp+rename), `src/dsp_api.cpp` (import logic)
- Risk: LOW-MEDIUM — atomic write protects `/config.json`, but multi-file import (`/hal_config.json`, DSP presets, pipeline matrix) not atomic across all files
- Recommendation: Add transaction log or snapshot + restore on boot if incomplete import detected. Add `test_settings_transactional/` covering: (1) reboot during import, (2) verify partial state not persisted

---

*Concerns audit: 2026-03-23*
