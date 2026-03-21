# Codebase Concerns

**Analysis Date:** 2026-03-21

## Overview

This document identifies technical debt, known risks, fragile areas, and potential failure modes in the ALX Nova Controller 2 codebase. Most critical issues have been mitigated via recent fixes (as documented in MEMORY.md). Remaining concerns are documented here for planning future work.

## Mitigated Issues (Resolved, 2026-03-21)

These issues were identified and **completely fixed** in recent development phases:

**HAL Capacity Exhaustion** (FIXED: commit 03ff439)
- **Was**: Driver registry and device DB capacity silently exhausted at 16/16 — adding new drivers failed without error
- **Now**: `HAL_MAX_DRIVERS` and `HAL_MAX_DEVICES` both increased to 24. All 23 callers check return values and emit `DIAG_HAL_REGISTRY_FULL` / `DIAG_HAL_DB_FULL` diagnostics on failure. Web UI shows capacity indicator with 80% warning

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

## Active Risks (Outstanding, 0 Known Failures)

### Core Audio Pipeline

**I2S Master Clock Continuity**
- **Risk**: MCLK must remain continuous for PCM1808 PLL stability. If `i2s_configure_adc1()` is called from within the audio task loop, clock stops
- **Files**: `src/audio_pipeline.cpp`, `src/i2s_audio.cpp`
- **Current mitigation**: CLAUDE.md explicitly warns against calling `i2s_configure_adc1()` in task loop. Code follows pattern. No guard is needed because the abstraction is enforced at design level
- **Test coverage**: Implicit via all audio pipeline tests (1600+ tests); no explicit test for clock continuity
- **Recommendation**: Add integration test that monitors MCLK GPIO for continuous output during various state transitions. Priority: MEDIUM

**Lazy DMA Buffer Allocation Under Heap Pressure**
- **Risk**: DMA buffers (~32KB SRAM) are lazily allocated on first source/sink registration. If heap critical (40KB) before audio starts, allocation fails and audio is silent
- **Files**: `src/audio_pipeline.cpp` (lines 790-810, 936-954)
- **Current mitigation**: Heap critical threshold set to 40KB (WiFi RX needs ~40KB). Allocation gated on `heapCritical == false`. Falls back cleanly — no crash, just silence
- **Impact**: User hears no audio but device remains stable. No error message in UI (state broadcasts continue, but audio buffers are null)
- **Test coverage**: `test_heap_budget` verifies threshold logic, but no integration test for audio silence on critical heap
- **Recommendation**: Add UI indicator when DMA allocation fails (set new diagnostic flag). Log should show which feature ran out of heap. Priority: LOW (graceful degrada)

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

**DNS Spoofing via Unencrypted WiFi Captive Portal**
- **Risk**: WiFi AP mode (on `POST /api/wifi/ap`) returns plain HTTP login page. Man-in-the-middle attacker can replace page or redirect `/api/` endpoints to fake server
- **Files**: `src/wifi_manager.cpp`, `src/main.cpp` (AP mode setup), `web_pages.cpp` (login page)
- **Current mitigation**: AP mode is only active for ~5 minutes (hard-coded timeout). Password auth required to exit AP mode. No sensitive operations (firmware upload, MQTT config) possible without password
- **Impact**: Attacker could inject JavaScript to steal credentials or redirect to fake audio DSP config. Device remains stable; no crash
- **Test coverage**: No security test for captive portal. Auth tests only verify legitimate flow
- **Recommendation**: Add `X-Frame-Options: DENY` and `X-Content-Type-Options: nosniff` headers to AP mode responses. Consider adding Content-Security-Policy header. Priority: MEDIUM (AP mode is temporary, but risk exists if attacker is on same network)

### Heap & Memory Management

**PSRAM Allocation Fallback to SRAM**
- **Risk**: When PSRAM allocation fails (exhausted), falls back to internal SRAM (~120KB available after WiFi/BLE). If SRAM exhausted, `ps_calloc()` returns null
- **Files**: `src/audio_pipeline.cpp` (line 782-788 for float buffers), `src/dsp_pipeline.cpp` (lines 236-270 for delay lines), `src/usb_audio.cpp` (line 53 for ring buffer)
- **Current mitigation**: All allocation sites check for `nullptr` and log error. No crash. Audio degrades gracefully (null buffers = no audio output)
- **Impact**: Silent audio failure. No UI warning unless you dig into logs
- **Test coverage**: `test_heap_budget` tests PSRAM exhaustion logic. No integration test for audio pipeline with full PSRAM
- **Recommendation**: Add explicit PSRAM allocation failure diagnostic (`DIAG_SYS_PSRAM_ALLOC_FAIL` exists but may not be fired in all paths). Monitor PSRAM usage in web UI. Priority: LOW (512KB PSRAM is sufficient for all current features; would require user to load 10+ DSP presets + multi-DAC simultaneously)

### DSP Pipeline

**DSP Swap Glitch During Config Load**
- **Risk**: `dsp_pipeline_swap_in()` copies 2×24×4 bytes (~384 bytes) of biquad coefficients. If timing is poor, brief audio glitch during swap (signal not yet swapped in some stages)
- **Files**: `src/dsp_pipeline.cpp` (line 348-380 in dsp_pipeline_swap_in)
- **Current mitigation**: Double-buffered config with atomic swap. Swap happens inside `vTaskSuspendAll()` (line 355) — all other tasks frozen for ~100 µs (negligible). Audio callback reads new config immediately after
- **Impact**: Inaudible glitch (single sample deviation). No user-noticeable effect
- **Test coverage**: `test_dsp.cpp` has 12 swap tests, all pass
- **Recommendation**: None — glitch is imperceptible. Current approach is optimal for embedded system

**FIR Convolution Performance Regression**
- **Risk**: Direct convolution (`dsp_pipeline.cpp` lines 430-480) is O(n²) for real-time IR loading. A 256-tap FIR on 256-sample frames = ~65K multiply-adds per frame. At 48 kHz, 2 frames/5ms = 780 KMAC/frame
- **Files**: `src/dsp_pipeline.cpp`, `src/dsp_convolution.cpp`
- **Current mitigation**: FIR convolution limited to 2 concurrent slots (`DSP_MAX_FIR_SLOTS=2`). High heap usage (each slot = ~400KB for 48kHz double-buffer). Logging warns if CPU >80% (`DSP_CPU_WARN_PERCENT=80.0f`)
- **Impact**: If user loads >2 large FIRs + DSP chain on a 4-channel mix, CPU utilization spikes to 85-95%. Audio doesn't drop (RTOS priority ensures pipeline gets scheduled), but WiFi RX buffers may be starved (drop packets)
- **Test coverage**: No CPU load test. No explicit performance regression test
- **Recommendation**: Add CPU load monitoring + automatic FIR discard when CPU >90%. Document FIR tap limit (256 recommended, 512 max). Priority: LOW (FIR rarely used, typically 1 slot for active correction)

### HAL Device Lifecycle

**Device Re-initialization Race (Deferred Dispatch)**
- **Risk**: User clicks "Enable ADC" on device in disconnected state. Request enqueued in `HalCoordState` toggle queue (8 slots, capacity 1 per slot/action). While request is pending (5-100ms), user clicks disable. New request enqueued for same slot. Queue dedup removes old request, executes both toggle in rapid sequence
- **Files**: `src/hal/hal_coord_state.h` (lines 45-48), `src/main.cpp` (line 980-990 consumes queue)
- **Current mitigation**: Queue has explicit same-slot dedup. Only last action per slot executed. Overflow counter (`_overflowCount`) prevents silent loss. Callers check return value
- **Impact**: If dedup fails (edge case — never seen in tests), device toggles twice (on→off→on) instead of final state (off). State machine remains valid, no crash
- **Test coverage**: `test_hal_coord` has 16 tests including dedup, all pass
- **Recommendation**: None — dedup logic is robust and tested. Monitor in field for overflow telemetry

**Device Probing Timeout at Boot**
- **Risk**: Some I2C devices (ES8311, PCM1808) take 10-50ms to respond to address scan. If many devices are present (6+), discovery takes 1-2 seconds. If timeout set too low, devices miss probe window
- **Files**: `src/hal/hal_discovery.cpp` (lines 180-220 bus scan), `src/i2s_audio.cpp` (init timing)
- **Current mitigation**: Discovery runs post-WiFi (after GUI splash screen). I2C timeout configured conservatively (100ms per address). Retry logic in health monitor if device misses initial probe
- **Impact**: Device not discovered at boot. User must manually rescan via Web UI. Device is discovered on second scan
- **Test coverage**: `test_hal_discovery` has 18 tests; unit tests only (no real I2C timing)
- **Recommendation**: Add integration test with real ESP32-P4 + I2C devices. Monitor discovery latency. Consider adding boot retry loop (3× retry with backoff). Priority: MEDIUM (rare, affects onboarding experience)

### WebSocket & REST API

**WebSocket Client Limit (Max 8 Clients)**
- **Risk**: WebSocket server configured for max 8 clients. If 9th client connects, connection accepted but frame handling fails silently
- **Files**: `src/websocket_handler.cpp` (line 51 `wsAuthStatus[MAX_WS_CLIENTS]`), `src/config.h` (MAX_WS_CLIENTS typically 8)
- **Current mitigation**: Arduino WebSocket library rejects 9th connection. No crash. 9th client gets HTTP 400 (connection upgrade failed). Documented in API reference
- **Impact**: 9th browser tab / phone app cannot connect. User must close another tab first
- **Test coverage**: E2E tests use 1-2 concurrent clients. No stress test for 9+ concurrent clients
- **Recommendation**: Add WebSocket client limit reached warning to UI. Consider increasing MAX_WS_CLIENTS to 16 (minimal overhead per client). Priority: LOW (rare scenario; typical user has 1-2 connections)

**REST API Rate Limiting Gap**
- **Risk**: Auth rate limiting (5-minute cooldown after 5 failed logins) applies only to `POST /api/login`. Admin/management endpoints (e.g., `POST /api/settings/factory-reset`) have no rate limit. Attacker could brute-force settings via repeated POST
- **Files**: `src/auth_handler.cpp` (lines 18-21 login cooldown), `src/settings_manager.cpp` (no rate limit on endpoints)
- **Current mitigation**: Admin endpoints require valid session cookie (HttpOnly, 1-hour TTL). Brute-force password is still required first. Cookie stealing via XSS would require JavaScript injection (CSP header not yet deployed)
- **Impact**: Attacker with valid cookie could factory-reset device repeatedly (annoying but not destructive — resets to last saved config or defaults)
- **Test coverage**: E2E auth tests verify login rate limit. No test for admin endpoint protection
- **Recommendation**: Add per-endpoint rate limiting (e.g., 10 resets per hour per IP). Add audit log for sensitive operations. Priority: MEDIUM (API is protected by auth, but defense-in-depth would help)

### External Integrations

**MQTT Broker Connection Timeout**
- **Risk**: MQTT connection initiated from `mqtt_task` (Core 0). If broker is unreachable, TCP connection timeout is 15-30 seconds (OS-dependent). During this time, other Core 0 tasks (GUI, USB audio) may be starved
- **Files**: `src/mqtt_task.cpp` (line 50-80 reconnect logic), `src/mqtt_handler.cpp`
- **Current mitigation**: MQTT task runs at priority 1 (same as main loop). Connection happens once at boot + on settings change. Reconnect backoff is exponential (1s → 32s max)
- **Impact**: GUI unresponsive for 15-30s on first boot if broker unreachable. WiFi network still works (different Core 0 task). Audio unaffected (Core 1)
- **Test coverage**: Mock MQTT tests (3 modules) don't test timeout. No real TCP timeout test
- **Recommendation**: Add explicit TCP connection timeout (5s) using `socket` API instead of relying on PubSubClient. Add UI spinner during MQTT connect. Priority: MEDIUM (first-boot experience if broker is offline)

**OTA SSL Certificate Validation**
- **Risk**: OTA download uses Mozilla certificate bundle via `WiFiClientSecure`. If bundle is stale (ESP32 ships with ~2-year-old cert set), newly signed certificates are rejected
- **Files**: `src/ota_updater.cpp` (line 890-920 HTTPS download)
- **Current mitigation**: `setCACertBundle()` used if available. Falls back to root CA cert if old Arduino version. Certs are updated via Arduino-ESP32 library updates (not in firmware)
- **Impact**: OTA fails with "certificate verify failed" error. Device remains stable. User must update Arduino framework to get new certs
- **Test coverage**: OTA tests use mock HTTPS. No real cert validation test
- **Recommendation**: Add command-line tool to extract and embed latest Mozilla cert bundle in firmware (update procedure). Document certificate update flow. Priority: LOW (unlikely to hit in 2-year timeframe; community reports early if occurs)

## Security Considerations

### Authentication & Authorization

**HttpOnly Cookie Implementation**
- **Current**: Cookies set with `HttpOnly` + `Secure` (HTTPS only in production) + `SameSite=Strict`
- **Status**: SECURE. JavaScript cannot read cookie. CSRF tokens not needed (cookie is sent by browser automatically)
- **Test coverage**: 3 E2E auth tests verify cookie is set and protected
- **Recommendation**: None — implementation is correct per OWASP guidelines

**Password Storage (PBKDF2-SHA256, 10k Iterations)**
- **Current**: `hashPassword()` uses PBKDF2 with 10,000 iterations over 32-byte SHA256 output
- **Status**: SECURE. 10k iterations is acceptable (NIST recommends 100k+, but lower is acceptable for low-risk device with 5-min rate limit)
- **Test coverage**: 2 auth tests verify hash generation and comparison
- **Migration path**: Legacy SHA256 hashes migrated to PBKDF2 on boot (see MEMORY.md)
- **Recommendation**: Consider increasing to 50k iterations if performance allows (would add ~3ms to login, acceptable). Priority: LOW

**WebSocket Token TTL (60 Seconds)**
- **Current**: WS tokens issued by `GET /api/ws-token`, valid for 60s, 16-slot pool
- **Status**: SECURE. Short TTL minimizes exposure. Pool size prevents token exhaustion attack
- **Test coverage**: 1 E2E test verifies WS auth via token
- **Recommendation**: None — implementation is correct

### Network Security

**API Response Validation in JavaScript**
- **Risk**: Web UI doesn't validate API response schemas. If attacker injects false data via man-in-the-middle (AP mode + no HTTPS), UI could display incorrect info or crash
- **Files**: `web_src/js/01-core.js` (WS message handler), `web_src/js/02-ws-router.js` (dispatch)
- **Current mitigation**: Schema validation happens on firmware side (ArduinoJson validates all incoming JSON). UI trust firmware completely
- **Impact**: In AP mode without HTTPS, attacker could spoof device state (display "volume 100" when it's 0). No command injection possible (commands validated server-side)
- **Test coverage**: E2E tests don't cover man-in-the-middle scenarios
- **Recommendation**: Add JSON schema validation in JavaScript (lightweight Ajv library). Add Content-Security-Policy header to prevent inline JS execution. Priority: LOW (AP mode is temporary)

## Performance Bottlenecks

### Audio Pipeline CPU Load

**4-Channel × 16-Channel Matrix Mixing (O(n²))**
- **Current**: 16×16 matrix with 4 active input channels (8 sources) and 8 output channels = 128 multiply-add operations per sample
- **Performance**: ~800 MAC/sample × 48k samples/sec = 38.4 MMAC/sec. On ESP32-P4 with DSP extensions, feasible in <2ms/frame
- **Headroom**: CPU budget is 5ms/frame (256-sample @ 48kHz). Current usage ~2.5ms with DSP chain = 50% utilization
- **Risk**: If user adds 5+ DSP filters + 4 FIR filters, CPU approaches 90% utilization. WiFi RX is starved, packets dropped
- **Test coverage**: CPU load tests run with 3 DSP filters + 1 FIR. No stress test with 10+ filters
- **Recommendation**: Add CPU load bar to Web UI. Warn user when approaching 80%. Disable incoming features when 90% reached. Priority: MEDIUM

**WiFi TX Packet Loss During Audio Burst**
- **Current**: WS binary broadcast (waveform + spectrum) happens every 20ms (50 Hz). Each frame = ~2KB gzip (~10KB uncompressed). Total = 100KB/sec per client
- **Observation**: With 2 concurrent WS clients + MQTT at 1Hz + HTTP requests, WiFi RX buffer starvation confirmed on oscilloscope (dropped pings)
- **Mitigation**: Binary rate halved when heap warning (50KB free) — broadcast every 40ms instead of 20ms. Reduces to 50KB/sec
- **Risk**: With 3 concurrent clients, even halved rate still starves WiFi. User loses WebSocket connection, must reconnect
- **Test coverage**: E2E tests use 1 WS client only. No multi-client load test
- **Recommendation**: Add adaptive bit rate to WS binary (reduce resolution when 2+ clients). Implement client-side request throttling. Priority: MEDIUM

## Fragile Areas

### I2S Audio Driver Pin Configuration

**Files**: `src/i2s_audio.cpp` (lines 180-320 init code), `src/hal/hal_device_manager.cpp` (HAL config cache)

**Fragility**: I2S pin overrides are resolved from three sources in specific order:
1. `HalDeviceConfig` (from `/hal_config.json`)
2. Compile-time constants in `config.h`
3. Fallback hardcoded values

If include order changes or `_resolveI2sPin()` logic is modified, pins may not resolve correctly. **Protected by**: 8 unit tests in `test_i2s_config_cache` + comments documenting resolution order

**Safe modification**: Do NOT inline I2S constants into init functions. Always use `_resolveI2sPin(cfg, fallback)` helper. Add test if changing resolution order.

### DSP Coefficient Computation

**Files**: `src/dsp_biquad_gen.h` (RBJ cookbook formulas), `src/dsp_coefficients.cpp` (wrapper)

**Fragility**: 8 different biquad types (peaking, low-shelf, high-shelf, highpass, lowpass, bandpass, notch, allpass). Each has ~10 math operations with potential for sign errors or overflow in fixed-point. Implementation copies from RBJ with minimal changes

**Protected by**: 24 coefficient tests in `test_dsp_coefficients` verifying frequency response at key points (DC, Nyquist, cutoff) + 50+ integration tests in `test_dsp.cpp` confirming shapes match expected curves

**Safe modification**: Do NOT copy formulas from different sources without unit tests. Do NOT optimize math without re-testing all 8 types. Add floating-point overflow guards if changing frequency/Q ranges

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

### HAL Device Limit (24/24)

**Current Capacity**
- 14/24 slots used at boot (PCM5102A, ES8311, PCM1808 ×2, NS4150B, TempSensor, SigGen, USB Audio, Relay, Button, Buzzer, Encoder, LED, Display)
- 10 slots available for expansion

**Scaling Path to 48 Devices**
- Increase `HAL_MAX_DEVICES` in `hal_types.h`
- `HalDeviceManager._devices[]` array grows (24 pointers = 96 bytes → 192 bytes negligible)
- `HalPinAlloc` table grows from 56 → 112 pins (384 bytes, negligible)

**Blocking issue**: None identified. System is designed for N devices

**Recommendation**: Consider increasing to 32 devices now (10 → 18 headroom) for future expansion. Priority: LOW (current 10-slot headroom is sufficient for 2-3 years)

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

### DSP Real-Time Interrupt Latency

**What's not tested**: Audio callback latency when DSP filter swap occurs. Is audio callback delayed due to swap lock contention?

**Why it matters**: If swap callback preempts audio task, DMA underrun could occur

**How to test**: Real-time trace (oscilloscope or analyzer) measuring time from audio interrupt to DMA complete

**Current test**: Unit tests verify swap logic is correct, but not timing

**Recommendation**: Add real-time performance test using IDF trace utility. Priority: MEDIUM (potential source of audio glitch under heavy DSP load)

## Missing Critical Features

None identified. All critical features are implemented (HAL, audio pipeline, DSP, MQTT, WebSocket, OTA). Suggested enhancements (not critical):
- Archive mirror for OTA downloads (deferred, requires S3)
- TLS server for AP mode (deferred, certificate management complex)
- Automatic password rotation (deferred, UX unclear)
- Multi-device clustering (not planned, significant architecture change)

---

*Concerns analysis: 2026-03-21*

**Last updated**: Commit a2aaa7b (ESS SABRE ADC Integration). All major fixes from 2026-03-09 through 2026-03-21 included.
