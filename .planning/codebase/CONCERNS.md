# Codebase Concerns

**Analysis Date:** 2026-03-21

## Overview

This document identifies technical debt, known risks, fragile areas, and potential failure modes in the ALX Nova Controller 2 codebase. Most critical issues have been mitigated via recent fixes (as documented in MEMORY.md). Remaining concerns are documented here for planning future work.

## Mitigated Issues (Resolved, 2026-03-21)

These issues were identified and **completely fixed** in recent development phases:

**HAL Capacity Exhaustion** (FIXED: commit 03ff439)
- **Was**: Driver registry and device DB capacity silently exhausted at 16/16 — adding new drivers failed without error
- **Now**: `HAL_MAX_DRIVERS` and `HAL_DB_MAX_ENTRIES` both increased to 32. `HAL_MAX_DEVICES` increased to 32. All 23 callers check return values and emit `DIAG_HAL_REGISTRY_FULL` / `DIAG_HAL_DB_FULL` diagnostics on failure. Web UI shows capacity indicator with 80% warning

**I2C Bus 0 WiFi SDIO Conflict** (FIXED: commit e610299)
- **Was**: Bus 0 (GPIO 48/54) scanned during WiFi connection, causing MCU reset
- **Now**: `hal_wifi_sdio_active()` guard checks `connectSuccess || connecting || activeInterface == NET_WIFI`. Bus 0 skipped when WiFi active, `DIAG_HAL_I2C_BUS_CONFLICT` emitted, REST API returns `partialScan` flag

**Matrix Routing Bounds Validation** (FIXED: commit 7f8c419)
- **Was**: Lanes 4-7 audio silently discarded by hardcoded `inCh[0-3]` loop. Drivers computed `firstChannel = sinkSlot * 2` without overflow check. No compile-time dimension safety
- **Now**: `static_assert` for dimension invariants, loop-based `inCh[]` population (all 8 lanes), `set_sink()`/`set_source()` channel validation with guard in HAL drivers

**I2S Pin HAL Config Ignored** (FIXED: commit 5c548e3)
- **Was**: `HalDeviceConfig.pinMclk/pinBck/pinLrc` persisted to JSON but ignored by all init callsites — pins always hardcoded, config overrides lost on sample rate change
- **Now**: `_cachedAdcCfg[2]` + `_resolveI2sPin()` helper ensures overrides respected. Fixed 4 callsites including `i2s_audio_set_sample_rate()` (most impactful)

**HalCoordState Toggle Queue Overflow** (FIXED: commit fafbf90)
- **Was**: All 6 callers of `requestDeviceToggle()` silently ignored return value — overflow dropped requests with zero feedback
- **Now**: Overflow telemetry (`_overflowCount` lifetime + `_overflowFlag` one-shot) + `DIAG_HAL_TOGGLE_OVERFLOW` diagnostic + caller error handling (HTTP 503, LOG_W)

**Heap Memory Pressure** (FIXED: commit 5876c20)
- **Was**: Binary heapCritical (40KB), no early warning, silent PSRAM→SRAM fallback, no allocation tracking
- **Now**: Graduated 3-state pressure (normal/warning/critical). `HeapBudgetTracker` records per-subsystem allocation. WS binary rate halved at warning (50KB). DMA refused at critical (40KB). PSRAM allocation failures now emit `DIAG_SYS_PSRAM_ALLOC_FAIL`

**DMA Buffer Allocation Under Heap Pressure** (FIXED: commit 9963667)
- **Was**: DMA buffers (~32KB SRAM) lazily allocated on first source/sink registration. Plain `calloc()` could allocate from PSRAM. Expansion devices discovered after WiFi in heap danger zone. `set_source()`/`set_sink()` returned `void` — bridge could not detect failure. No UI indicator
- **Now**: All 16 DMA buffers eagerly pre-allocated in `audio_pipeline_init()` before WiFi using `heap_caps_calloc(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)`. `set_source()`/`set_sink()` return `bool` — bridge checks and emits `DIAG_AUDIO_DMA_ALLOC_FAIL` (0x200E). Web UI indicator + MQTT HA binary sensor. 9 new tests (2253 total, 86 modules)

## Active Risks (Outstanding, 0 Known Failures)

### Core Audio Pipeline

**I2S Master Clock Continuity** (REVIEWED: 2026-03-22)
- **Risk**: MCLK must remain continuous for PCM1808 PLL stability. If `i2s_configure_adc1()` is called from within the audio task loop, clock stops
- **Files**: `src/audio_pipeline.cpp`, `src/i2s_audio.cpp`
- **Current mitigation**: `i2s_configure_adc1()` is `static` in `i2s_audio.cpp` — not externally callable. Only two callsites exist, both from main loop context (never from audio task). CLAUDE.md explicitly documents the constraint
- **Review outcome**: 5-agent team reviewed (architect, embedded, backend, testing, code-reviewer). Code reviewer rejected runtime guard as over-engineering — function's `static` linkage prevents incorrect callers. Documentation-only mitigation is appropriate
- **Test coverage**: Implicit via all audio pipeline tests (2335 tests); no explicit test for clock continuity
- **Recommendation**: None — risk is well-mitigated by static linkage + documentation. Priority: CLOSED (downgraded from MEDIUM)

### FreeRTOS Task Watchdog

**Task Watchdog Timer (TWDT) Starvation Risk** (FIXED: 2026-03-21)
- **Was**: OTA tasks (`otaDownloadTask`, `otaCheckTaskFunc`) not subscribed to TWDT and never fed watchdog. Download runs 30-90s — network stall caused MCU reboot via watchdog panic. HTTP timeout (30s) matched TWDT timeout, providing no safety margin
- **Now**: Both OTA tasks subscribe (`esp_task_wdt_add`) at entry and unsubscribe (`esp_task_wdt_delete`) before task deletion. Download loop feeds watchdog every iteration. Network stall detection: `OTA_STALL_TIMEOUT_MS` (20s) aborts download if no data received. HTTP timeouts reduced: `OTA_CONNECT_TIMEOUT_MS` (20s), `OTA_READ_TIMEOUT_MS` (5s). `DIAG_OTA_NETWORK_STALL` (0x6006) diagnostic emitted on stall. 13 new tests (8 in test_ota, 5 in test_ota_task). All timeouts verified < 30s TWDT via compile-time test assertions

### WiFi & Networking

**WiFi SDIO Bus 0 Race During Boot**
- **Risk**: WiFi co-processor (ESP32-C6) initializes during `setup()`. If WiFi init completes before HAL discovery runs, Bus 0 SDIO may not be marked active yet, and discovery thread might scan Bus 0 and reset MCU
- **Files**: `src/hal/hal_discovery.cpp`, `src/wifi_manager.cpp`
- **Current mitigation**: `activeInterface` is set to `NET_WIFI` inside WiFi connection callback (not during `WiFi.begin()`). Discovery checks both `connectSuccess` and `connecting` flags
- **Actual risk**: Boot sequence is (1) WiFi `begin()`, (2) Wait for connection callback, (3) HAL discovery. Discovery waits for WiFi to connect before running. No race
- **Test coverage**: `test_hal_discovery` has 5 SDIO guard tests, all passing
- **Recommendation**: None — risk is well-mitigated by design. Monitor for any future async WiFi changes

**DNS Spoofing via Unencrypted WiFi Captive Portal** (MITIGATED: 2026-03-22)
- **Was**: All HTTP responses lacked security headers. Man-in-the-middle attacker could inject content via clickjacking (iframe embedding) or MIME-type confusion
- **Now**: `X-Frame-Options: DENY` and `X-Content-Type-Options: nosniff` headers added to ALL HTTP responses via `http_add_security_headers()` helper in `src/http_security.h`. Applied to: gzipped pages (`sendGzipped()`), auth responses, API responses in `main.cpp` and `settings_manager.cpp`. CSP intentionally omitted — code reviewer determined `unsafe-inline` provides zero XSS protection for inline scripts (security theater). 5 new tests in `test_http_security`
- **Remaining risk**: AP mode still uses plain HTTP (no TLS). Content injection via MITM remains possible but clickjacking and MIME sniffing are now blocked. TLS for AP mode deferred (certificate management complex)

### Heap & Memory Management

**PSRAM Allocation Fallback to SRAM** (FIXED: 2026-03-21)
- **Was**: ~10 PSRAM allocation sites silently fell back to SRAM with no diagnostic emission (only `audio_pipeline.cpp` emitted `DIAG_SYS_PSRAM_ALLOC_FAIL`). No per-subsystem budget tracking. No PSRAM-specific pressure thresholds. Web UI showed aggregate PSRAM % but no allocation breakdown or failure warnings
- **Now**: Unified `psram_alloc()` wrapper (`src/psram_alloc.h/.cpp`) centralizes PSRAM-preferred allocation with SRAM fallback, automatic `heap_budget` recording, and `DIAG_SYS_PSRAM_ALLOC_FAIL` emission. All 9 allocation sites migrated. Graduated PSRAM pressure thresholds (`PSRAM_WARNING_THRESHOLD` 1MB, `PSRAM_CRITICAL_THRESHOLD` 512KB) with `DIAG_SYS_PSRAM_WARNING` / `DIAG_SYS_PSRAM_WARNING_CLEARED` diagnostics. Feature shedding: DSP delay/convolution allocations refused at PSRAM critical. `GET /api/psram/status` REST endpoint for monitoring integration. Web UI: PSRAM budget breakdown table, fallback count badge, health dashboard warning banner, toast on new fallbacks. WebSocket `hardwareStats` includes `psramFallbackCount`, `psramWarning`, `psramCritical`. Tests: 10 new `test_psram_alloc` + 4 new `test_heap_budget` remove tests + E2E coverage

### DSP Pipeline

**DSP Swap Glitch During Config Load** (MITIGATED: 2026-03-21)
- **Status: MITIGATED** — Double-buffered config with atomic swap inside `vTaskSuspendAll()` confirmed safe. Edge-case tests added for swap timeout path and coefficient morphing state. `swapLatencyUs` metric tracks actual swap duration
- **Was**: Risk of brief audio glitch during biquad coefficient swap if timing is poor
- **Now**: 12 swap tests in `test_dsp.cpp` all pass, including new edge-case coverage for swap timeout and coefficient morphing. Swap latency is measurable via `swapLatencyUs`. Glitch is imperceptible (single sample deviation within `vTaskSuspendAll()` window of ~100 µs)

**FIR Convolution Performance Regression** (MITIGATED: 2026-03-21)
- **Status: MITIGATED** — CPU load thresholds with auto-bypass prevent runaway FIR processing. FIR/convolution stages automatically skipped when `cpuCritical` >= 95%
- **Was**: O(n²) direct convolution with no automatic shedding — CPU could spike to 85-95% with multiple FIR slots, starving WiFi RX buffers
- **Now**: Graduated CPU thresholds (`DSP_CPU_WARN_PERCENT=80.0f`, critical at 95%). FIR/convolution stages auto-bypassed when CPU critical. `DIAG_DSP_CPU_CRIT` emitted on auto-bypass. Pipeline timing instrumentation tracks per-frame cost

### HAL Device Lifecycle

**Device Re-initialization Race (Deferred Dispatch)**
- **Risk**: User clicks "Enable ADC" on device in disconnected state. Request enqueued in `HalCoordState` toggle queue (8 slots, capacity 1 per slot/action). While request is pending (5-100ms), user clicks disable. New request enqueued for same slot. Queue dedup removes old request, executes both toggle in rapid sequence
- **Files**: `src/hal/hal_coord_state.h` (lines 45-48), `src/main.cpp` (line 980-990 consumes queue)
- **Current mitigation**: Queue has explicit same-slot dedup. Only last action per slot executed. Overflow counter (`_overflowCount`) prevents silent loss. Callers check return value
- **Impact**: If dedup fails (edge case — never seen in tests), device toggles twice (on→off→on) instead of final state (off). State machine remains valid, no crash
- **Test coverage**: `test_hal_coord` has 16 tests including dedup, all pass
- **Recommendation**: None — dedup logic is robust and tested. Monitor in field for overflow telemetry

**Device Probing Timeout at Boot** (MITIGATED: 2026-03-22)
- **Was**: Single I2C scan pass at boot. Expansion devices with slow power-on (EEPROM POR 5-100ms) missed on first probe. User required manual rescan
- **Now**: Targeted retry in `hal_i2c_scan_bus()`: addresses returning I2C timeout (error codes 4/5) are collected (max 16) and retried up to `HAL_PROBE_RETRY_COUNT` (2) times with increasing backoff (`HAL_PROBE_RETRY_BACKOFF_MS` × attempt = 50ms, 100ms). NACK addresses (error 2 = "nobody home") are NOT retried. Worst-case boot delay: ~300ms. `DIAG_HAL_PROBE_RETRY_OK` (0x1105) emitted on retry success. 8 new tests in `test_hal_probe_retry`
- **Remaining risk**: Hardware integration test with real ESP32-P4 + I2C devices not yet added. Priority: LOW (retry logic covers the common case)

### WebSocket & REST API

**WebSocket Client Limit** (FIXED: 2026-03-22)
- **Was**: MAX_WS_CLIENTS=10, silent TCP rejection for 11th client, no UI feedback
- **Now**: MAX_WS_CLIENTS=16 with `WEBSOCKETS_SERVER_CLIENT_MAX=16` build flag. `wsClientCount`/`wsClientMax` fields added to `hardwareStats` WS broadcast. UI warning toast when approaching limit. Adaptive rate tiers extended: 5-7 clients (skip-6), 8+ clients (skip-8). 4 new tests in `test_ws_adaptive_rate`. Priority: CLOSED

**REST API Rate Limiting Gap** (REVIEWED: 2026-03-22)
- **Risk**: Admin endpoints (factory-reset, reboot) have no server-side rate limiting beyond session auth
- **Review outcome**: 5-agent team reviewed. Code reviewer rejected server-side rate limiting as security theater: (1) attacker must already have valid session cookie, (2) reboot resets RAM-based counter, (3) MQTT path exposes same operations without rate limiting, creating inconsistent posture. UX risk: legitimate user blocked by 429 on retry after network glitch
- **Mitigation applied**: Client-side confirmation dialog with 3-second countdown timer added for factory-reset and reboot buttons in `web_src/js/22-settings.js`. Prevents accidental rapid-fire clicks (the actual UX problem). Modal uses MDI warning icon, disabled confirm button with countdown, cancel/backdrop dismiss
- **Recommendation**: None — server-side rate limiting deferred. If needed in future, also guard MQTT `system/factory_reset` and `system/reboot` topics for consistency. Priority: CLOSED (downgraded from MEDIUM)

### External Integrations

**MQTT Broker Connection Timeout** (MITIGATED: 2026-03-22)
- **Was**: `WiFiClient` TCP socket timeout was OS-dependent (15-30s). `mqttWifiClient.connect()` already had a 1s pre-connect timeout (line 889), but `PubSubClient.connect()` could still block for up to 15-30s on the TCP stack level
- **Now**: `mqttWifiClient.setTimeout(MQTT_SOCKET_TIMEOUT_MS)` (5000ms) caps the overall socket timeout at 5 seconds. Combined with the existing 1s pre-connect timeout, total worst-case blocking is ~6 seconds — well under the 30s TWDT timeout. 1 new test verifying constant value and TWDT safety margin
- **Review outcome**: Full PubSubClient → async MQTT library swap was rejected by code reviewer: 317 callsites, mock rewrite required, ESP32-P4 compatibility unverified. 1-line `setTimeout()` fix provides the needed protection. Full library swap deferred to dedicated future release
- **Remaining risk**: PubSubClient still blocks during `connect()`, but for max 5s instead of 30s. GUI may be briefly unresponsive. Priority: LOW (acceptable for first-boot edge case)

**OTA SSL Certificate Validation** (FIXED: 2026-03-22)
- **Was**: 3 root CA PEM strings hardcoded inline in `ota_updater.cpp`. No update procedure or tooling. Cert rotation required manual code editing
- **Now**: Certs extracted to `src/ota_certs.h` with expiry date comments and update timestamp. `tools/update_certs.js` Node.js script connects to GitHub API/CDN, extracts cert chains, regenerates header. Documented in Docusaurus developer docs (`build-setup.md`). Current certs: Sectigo R46 (2046), Sectigo E46 (2046), DigiCert G2 (2038). Priority: CLOSED

## Security Considerations

### Authentication & Authorization

**HttpOnly Cookie Implementation**
- **Current**: Cookies set with `HttpOnly` + `Secure` (HTTPS only in production) + `SameSite=Strict`
- **Status**: SECURE. JavaScript cannot read cookie. CSRF tokens not needed (cookie is sent by browser automatically)
- **Test coverage**: 3 E2E auth tests verify cookie is set and protected
- **Recommendation**: None — implementation is correct per OWASP guidelines

**Password Storage (PBKDF2-SHA256, 50k Iterations)** (UPGRADED: 2026-03-22)
- **Was**: PBKDF2 with 10,000 iterations (`p1:` format), hardcoded in `auth_handler.cpp`
- **Now**: PBKDF2 with 50,000 iterations (`p2:` format). `PBKDF2_ITERATIONS` constant in `config.h`. Backward-compatible: `p1:` (10k) and legacy SHA256 hashes auto-migrated to `p2:` on successful login. 5 new tests verify format, roundtrip, backward compat, and migration. Priority: CLOSED

**WebSocket Token TTL (60 Seconds)**
- **Current**: WS tokens issued by `GET /api/ws-token`, valid for 60s, 16-slot pool
- **Status**: SECURE. Short TTL minimizes exposure. Pool size prevents token exhaustion attack
- **Test coverage**: 1 E2E test verifies WS auth via token
- **Recommendation**: None — implementation is correct

### Network Security

**API Response Validation in JavaScript** (FIXED: 2026-03-22)
- **Was**: 50+ `apiFetch()` calls used `.json()` without HTTP status checks. WS messages dispatched without field validation. Non-2xx responses or missing fields could crash UI
- **Now**: `response.safeJson()` method added to `apiFetch()` (non-breaking, opt-in). Checks `response.ok`, wraps `.json()` in try/catch. 32 high-impact call sites migrated (`22-settings.js`, `23-firmware-update.js`, `20-wifi-network.js`). `validateWsMessage(data, requiredFields)` helper validates critical WS types: `audioLevels` (requires `adc`), `hardware_stats` (requires `cpu`), `wifiStatus` (requires `connected`). Priority: CLOSED

## Performance Bottlenecks

### Audio Pipeline CPU Load (FIXED: 2026-03-21)

**Status: FIXED** — Graduated CPU thresholds at 80%/95% with automatic feature shedding. Pipeline timing instrumentation tracks per-frame cost. Web UI DSP CPU card displays real-time load. FIR/convolution auto-bypassed at critical load

**4-Channel × 16-Channel Matrix Mixing (O(n²))**
- **Current**: 16×16 matrix with 4 active input channels (8 sources) and 8 output channels = 128 multiply-add operations per sample
- **Performance**: ~800 MAC/sample × 48k samples/sec = 38.4 MMAC/sec. On ESP32-P4 with DSP extensions, feasible in <2ms/frame
- **Headroom**: CPU budget is 5ms/frame (256-sample @ 48kHz). Current usage ~2.5ms with DSP chain = 50% utilization
- **Was**: No CPU load visibility, no automatic shedding — user could overload DSP chain and starve WiFi RX
- **Now**: `DSP_CPU_WARN_PERCENT` (80%) triggers `DIAG_DSP_CPU_WARN` diagnostic and web UI warning. `cpuCritical` (95%) triggers `DIAG_DSP_CPU_CRIT`, auto-bypasses FIR/convolution stages. `PipelineTimingMetrics` provides per-frame instrumentation. Web UI DSP CPU card shows real-time utilization bar

**WiFi TX Packet Loss During Audio Burst** (MITIGATED: 2026-03-22)
- **Was**: Binary WS broadcast at fixed 20ms interval regardless of client count. With 3+ concurrent clients, WiFi RX buffer starvation confirmed (dropped pings). Heap pressure only halved rate — insufficient for 3+ clients
- **Now**: Client-count adaptive rate scaling in `websocket_handler.cpp`. Five-tier skip factor: 1 client = every frame (20ms), 2 clients = skip-2 (40ms), 3-4 clients = skip-4 (80ms), 5-7 clients = skip-6 (120ms), 8+ clients = skip-8 (160ms). Combined with existing heap pressure via `&&` (both gates must allow). Periodic auth count recalibration every `WS_AUTH_RECOUNT_INTERVAL_MS` (10s) fixes stale counts from unclean disconnects. `wsAuthenticatedClientCount()` getter exposed in header. `wsClientCount`/`wsClientMax` fields in `hardwareStats` broadcast. UI warning toast when approaching limit. 15 tests in `test_ws_adaptive_rate`
- **Remaining risk**: Higher skip factors for many clients show visible stepping in waveform visualization. Acceptable tradeoff. No multi-client stress test yet. Priority: LOW

## Fragile Areas

### I2S Audio Driver Pin Configuration

**Files**: `src/i2s_audio.cpp` (lines 180-320 init code), `src/hal/hal_device_manager.cpp` (HAL config cache)

**Fragility**: I2S pin overrides are resolved from three sources in specific order:
1. `HalDeviceConfig` (from `/hal_config.json`)
2. Compile-time constants in `config.h`
3. Fallback hardcoded values

If include order changes or `_resolveI2sPin()` logic is modified, pins may not resolve correctly. **Protected by**: 8 unit tests in `test_i2s_config_cache` + comments documenting resolution order

**Safe modification**: Do NOT inline I2S constants into init functions. Always use `_resolveI2sPin(cfg, fallback)` helper. Add test if changing resolution order.

### DSP Coefficient Computation (FIXED: 2026-03-21)

**Status: FIXED** — `normalize()` guards added to all biquad coefficient generators. Return value checks on all computation paths. NaN/Inf/stability guards prevent invalid coefficients from reaching the audio pipeline. `DIAG_DSP_COEFF_INVALID` emitted on detection. 8 new edge-case tests for boundary conditions (zero Q, Nyquist frequency, extreme gain)

**Files**: `src/dsp_biquad_gen.h` (RBJ cookbook formulas), `src/dsp_coefficients.cpp` (wrapper)

**Was fragile**: 8 different biquad types with ~10 math operations each and potential for sign errors or overflow. No runtime guards against NaN/Inf coefficients reaching the pipeline

**Now protected by**: `normalize()` guards, NaN/Inf/stability validation, `DIAG_DSP_COEFF_INVALID` diagnostic emission, 24 coefficient tests + 8 new edge-case tests in `test_dsp_coefficients` + 50+ integration tests in `test_dsp.cpp`

**Safe modification**: Do NOT copy formulas from different sources without unit tests. Do NOT optimize math without re-testing all 8 types. Coefficient guards will catch invalid output at runtime

### HAL Device State Machine

**Files**: `src/hal/hal_device_manager.cpp` (lines 140-180 state transitions)

**Fragility**: Device lifecycle has 11 states (UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ↔ UNAVAILABLE → ERROR / REMOVED / MANUAL). State transitions are guarded by guards (e.g., can't go AVAILABLE→CONFIGURING). If a new state is added without updating all guard conditions, transitions may be possible that violate the state machine contract

**Protected by**: 30 HAL core tests + 24 HAL discovery tests verifying state transitions don't violate contract. Callback (`HalStateChangeCb`) ensures bridge + pipeline stay in sync

**Safe modification**: Do NOT add new states without adding tests for all incoming/outgoing transitions. Do NOT remove transitions without checking what depends on them (grep for `_state ==`). Always verify bridge callback is invoked

### WebSocket Message Dispatch

**Files**: `src/websocket_handler.cpp` (lines 2100-2400 parseCommand), `web_src/js/02-ws-router.js` (JS dispatch)

**Fragility**: Firmware and JS must agree on message type strings (e.g., `"cmd": "setDspStage"`). If firmware renames command without updating JS router, command silently fails (no error, no log)

**Protected by**: 18 E2E browser tests verify end-to-end command flow (JS → WS → firmware → state change → broadcast → JS). No isolated unit test for command parsing

**Safe modification**: Do NOT rename WS command strings without (1) updating both JS router AND firmware handler, (2) adding E2E test. Consider adding command version number to detect mismatches

## Scaling Limits

### Matrix Routing (8×16 Fixed)

**Current Capacity**
- 8 input lanes (up to 4 stereo ADCs)
- 16 output channels (8 sinks, stereo each)
- 256 float buffers in use (~66KB PSRAM)

**Scaling Path to 16×16**
- Change `AUDIO_PIPELINE_MAX_INPUTS` from 8 → 16 (requires `resizeAudioArrays()` in frontend)
- Change `AUDIO_PIPELINE_MATRIX_SIZE` from 16 → 32
- Matrix storage: 32×32×4 bytes = 4KB (negligible)
- Float buffer storage: 16×2×256×4 = 32KB (total 98KB with current)
- Per-channel DSP: Currently 4 channels with 24 stages. 8-channel would need 192KB (DSP_MAX_CHANNELS must increase, more memory pressure)

**Blocking issue**: Output DSP (`output_dsp.h`) assumes stereo pairs. Would need architectural refactor to support independent mono channels

**Recommendation**: Document 8×16 as current architectural limit. If user needs >4 stereo ADCs, recommend cascading two units via Ethernet (not yet supported). Priority: DEFERRED

### HAL Device Limit (UPGRADED: 2026-03-22)

**Was**: `HAL_MAX_DEVICES=24`, 14 slots at boot, 10 free
**Now**: `HAL_MAX_DEVICES=32`, 14 slots at boot, 18 free. `HAL_MAX_DRIVERS` (32) and `HAL_DB_MAX_ENTRIES` (32) already matched. Memory impact negligible. Test assertions updated (33rd device rejected). Priority: CLOSED

### Diagnostic Journal (800 Entries)

**Current Capacity**: 800 persistent entries on LittleFS (~64KB file)

**Scaling Path**: Change `DIAG_JOURNAL_MAX_ENTRIES` in `config.h` and allocate larger LittleFS partition

**Current usage**: ~100-200 entries/day (average 0.1% full). At current rate, 2000 entries (128KB) would last 10-20 days

**Recommendation**: Monitor actual usage in field. If growing faster than expected, consider rotating old entries or compressing JSON. Priority: LOW

## Dependencies at Risk

### Arduino-ESP32 Framework (v3.x, IDF5)

**Status**: Stable for 1.12.2 firmware. No known breaking changes in 3.0-3.2 range

**Risk**: IDF5 (pre-built `.a` libraries) has harder ABI guarantees than IDF4. Updating Arduino-ESP32 past v3.5 may introduce breaking changes to:
- I2S driver API (currently stable)
- LEDC PWM API (currently stable)
- Task watchdog API (currently stable)

**Mitigation**: Lock Arduino-ESP32 to `~3.0` in `platformio.ini`. Test any framework upgrades thoroughly before merging

**Recommendation**: Document framework version constraints in README. Set up CI to test against latest v3.x monthly. Priority: MEDIUM

### LovyanGFX Display Driver (v1.2.x)

**Status**: Stable. No DMA support on P4 (synchronous SPI only), which is fine for 160×128 display

**Risk**: Upstream may remove P4 support or change SPI API. Current code assumes synchronous SPI only

**Mitigation**: LovyanGFX is well-maintained. SPI API unlikely to change. No known blocking issues

**Recommendation**: Monitor release notes. Test display on any LovyanGFX upgrade (>= v1.3). Priority: LOW

### ESP-DSP Pre-built Library

**Status**: Stable. v1.x used in current firmware. No breaking changes expected

**Risk**: If Espressif drops P4 support, pre-built `.a` would not be updated. Assembly-optimized biquad/FFT would be unavailable

**Mitigation**: Source code for ESP-DSP is available on GitHub. Could rebuild from source if needed

**Recommendation**: Monitor ESP-DSP releases. If breaking change occurs, build from source (1-2 hour project). Priority: LOW

## Test Coverage Gaps

### I2S Clock Continuity Integration Test

**What's not tested**: MCLK GPIO remains high (not pulsing) when audio is paused or during sample rate change

**Why it matters**: PCM1808 PLL locks to MCLK. If MCLK stops, PLL unlocks and audio quality degradation occurs

**How to test**: Oscilloscope probe on MCLK GPIO during various state transitions (pause → resume, 44.1k → 48k, enable ADC → disable ADC)

**Current test**: Unit tests verify configuration logic, but no hardware test

**Recommendation**: Add hardware integration test to CI (requires P4 board + oscilloscope capture). Priority: MEDIUM

### WiFi & Ethernet Failover

**What's not tested**: Switching from WiFi to Ethernet and back. Network socket behavior during failover

**Why it matters**: Device should maintain MQTT connection during WiFi → Ethernet transition (unlikely but possible with external Ethernet hat)

**How to test**: Real network setup (WiFi AP + Ethernet network), simultaneous disconnection of WiFi while Ethernet is connected

**Current test**: No test. Only individual WiFi or Ethernet tests

**Recommendation**: Add WiFi/Ethernet failover test to integration suite. Priority: LOW (Ethernet support is proof-of-concept, rarely used)

### MQTT QoS 2 Guarantee

**What's not tested**: MQTT QoS 2 (exactly-once) message delivery. Only QoS 1 (at-least-once) is used in production

**Why it matters**: If MQTT broker loses connection, messages might not be delivered or might be delivered multiple times

**How to test**: MQTT broker emulation with message loss injection. Verify retransmit count and delivery guarantee

**Current test**: Mock MQTT tests only. Real broker integration test would be needed

**Recommendation**: Add MQTT broker integration test to E2E suite (Mosquitto in Docker). Priority: LOW (current QoS 1 is acceptable for non-critical telemetry)

### DSP Real-Time Interrupt Latency (MITIGATED: 2026-03-21)

**Status: MITIGATED** — Swap latency now instrumented via `swapLatencyUs` metric. Pipeline-wide timing metrics available via `PipelineTimingMetrics` struct. CPU load visible in web UI DSP CPU card. Swap occurs within `vTaskSuspendAll()` window (~100 µs), confirmed safe by timing instrumentation

**Was not tested**: Audio callback latency when DSP filter swap occurs — potential DMA underrun from lock contention

**Now instrumented**: `swapLatencyUs` tracks actual swap duration per operation. `PipelineTimingMetrics` provides per-frame timing for the full pipeline. `DIAG_DSP_CPU_WARN` / `DIAG_DSP_CPU_CRIT` emitted when thresholds exceeded. Web UI displays real-time DSP CPU load

**Remaining gap**: No oscilloscope-based hardware integration test. Unit tests verify swap logic and timing metrics, but real-time trace would require P4 board + analyzer. Priority: LOW (instrumentation provides sufficient visibility)

## Missing Critical Features

None identified. All critical features are implemented (HAL, audio pipeline, DSP, MQTT, WebSocket, OTA). Suggested enhancements (not critical):
- Archive mirror for OTA downloads (deferred, requires S3)
- TLS server for AP mode (deferred, certificate management complex)
- Automatic password rotation (deferred, UX unclear)
- Multi-device clustering (not planned, significant architecture change)

---

*Concerns analysis: 2026-03-21*

**Last updated**: 2026-03-22 (Low Priority Mitigations). 5 LOW risks resolved: HAL_MAX_DEVICES 24→32, PBKDF2 iterations 10k→50k with p1→p2 migration, MAX_WS_CLIENTS 10→16 with adaptive rate tiers and UI warning, OTA certs extracted to header with update tooling, frontend API/WS response validation added. Docusaurus developer docs updated across 5 pages.
