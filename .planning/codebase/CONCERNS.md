# Codebase Concerns

**Analysis Date:** 2026-03-22

## Overview

This document identifies technical debt, known risks, fragile areas, and potential failure modes in the ALX Nova Controller 2 codebase. Most critical and high-priority issues have been mitigated via recent fixes. Remaining concerns and newly identified issues are documented here for planning future work.

**Test baseline:** 2354 C++ tests (106 modules), 113 E2E Playwright tests (22 specs).

---

## Active Concerns (NEW / OUTSTANDING)

No active concerns. All identified issues have been mitigated.

---

## Mitigated Issues (Resolved, Historical Record)

### CRITICAL/HIGH (All Fixed)

**HAL Capacity Exhaustion** (FIXED: commit 03ff439)
- `HAL_MAX_DRIVERS` and `HAL_DB_MAX_ENTRIES` increased to 32. `HAL_MAX_DEVICES` increased to 32. All 23 callers check return values. `DIAG_HAL_REGISTRY_FULL` / `DIAG_HAL_DB_FULL` diagnostics on failure. Web UI capacity indicator.

**I2C Bus 0 WiFi SDIO Conflict** (FIXED: commit e610299)
- `hal_wifi_sdio_active()` guard checks `connectSuccess || connecting || activeInterface == NET_WIFI`. Bus 0 skipped when WiFi active. `DIAG_HAL_I2C_BUS_CONFLICT` emitted. REST API returns `partialScan`.

**Matrix Routing Bounds Validation** (FIXED: commit 7f8c419)
- `static_assert` for dimension invariants. Loop-based `inCh[]` population (all 8 lanes). `set_sink()`/`set_source()` channel validation. HAL driver `buildSink()` overflow guards.

**I2S Pin HAL Config Ignored** (FIXED: commit 5c548e3)
- `_cachedAdcCfg[2]` + `_resolveI2sPin()` for 4 callsites including `i2s_audio_set_sample_rate()`.

**HalCoordState Toggle Queue Overflow** (FIXED: commit fafbf90)
- Overflow telemetry + `DIAG_HAL_TOGGLE_OVERFLOW` + caller error handling (HTTP 503, LOG_W).

**Heap Memory Pressure** (FIXED: commit 5876c20)
- Graduated 3-state pressure. `HeapBudgetTracker`. WS binary rate halved at warning (50KB). DMA refused at critical (40KB).

**DMA Buffer Allocation Under Heap Pressure** (FIXED: commit 9963667)
- Eager pre-allocation 16x2KB=32KB. `set_source()`/`set_sink()` return `bool`. `DIAG_AUDIO_DMA_ALLOC_FAIL` (0x200E).

**PSRAM Allocation Fallback** (FIXED: commit c677582)
- Unified `psram_alloc()` wrapper. Graduated PSRAM pressure. `GET /api/psram/status`. All 9 sites migrated.

**DSP Coefficient Safety** (FIXED: 2026-03-21)
- `normalize()` guards on all biquad generators. NaN/Inf/stability validation. `DIAG_DSP_COEFF_INVALID`.

### MEDIUM (All Mitigated/Closed)

**I2S Master Clock Continuity** (CLOSED: 2026-03-22)
- `i2s_configure_adc1()` is `static` in `i2s_audio.cpp`. Only 2 callsites, both from main loop context. 5-agent review confirmed documentation-only mitigation is appropriate.

**Task Watchdog Timer (TWDT) Starvation** (FIXED: 2026-03-21)
- OTA tasks subscribe to TWDT. Download loop feeds watchdog. `OTA_STALL_TIMEOUT_MS` (20s) aborts on stall. 13 new tests.

**DNS Spoofing / HTTP Security Headers** (MITIGATED: 2026-03-22)
- `X-Frame-Options: DENY` and `X-Content-Type-Options: nosniff` added to HTML pages and auth responses. CSP omitted (security theater for inline scripts). AP mode still plain HTTP. 5 tests in `test_http_security`.

**Device Probing Timeout at Boot** (MITIGATED: 2026-03-22)
- Targeted retry for I2C timeout addresses. `HAL_PROBE_RETRY_COUNT` (2) with increasing backoff. `DIAG_HAL_PROBE_RETRY_OK`. 8 tests.

**MQTT Broker Connection Timeout** (MITIGATED: 2026-03-22)
- `mqttWifiClient.setTimeout(MQTT_SOCKET_TIMEOUT_MS)` (5000ms). Total worst-case blocking ~6s. Full PubSubClient swap deferred.

**WebSocket Client Limit** (CLOSED: 2026-03-22)
- MAX_WS_CLIENTS=16. 5-tier adaptive rate scaling. `wsClientCount`/`wsClientMax` in `hardwareStats`. UI warning toast.

**REST API Rate Limiting** (CLOSED: 2026-03-22)
- Client-side confirmation dialog with 3-second countdown for factory-reset/reboot. Server-side rate limiting deferred.

**WiFi TX Packet Loss During Audio Burst** (MITIGATED: 2026-03-22)
- Client-count adaptive rate scaling. 5-tier skip factor. Periodic auth recount. 15 tests in `test_ws_adaptive_rate`.

**DSP Swap Glitch** (MITIGATED: 2026-03-21)
- Double-buffered config with atomic swap. `swapLatencyUs` metric. 12 swap tests.

**FIR Convolution Performance** (MITIGATED: 2026-03-21)
- CPU load thresholds (80%/95%). FIR/convolution auto-bypass at critical. `DIAG_DSP_CPU_CRIT`.

### LOW (Concerns Cleanup — 2026-03-22)

**HTTP Security Headers Incomplete Coverage** (FIXED: 2026-03-22)
- `server_send()` wrapper in `http_security.h` centralizes security headers. All 12+ handler files now covered.

**Frontend `.json()` Calls Without `safeJson()` Protection** (FIXED: 2026-03-22)
- Remaining `apiFetch()` chains migrated to `safeJson()`. All frontend `.json()` calls now protected.

**`websocket_handler.cpp` Size (2623 Lines)** (FIXED: 2026-03-22)
- Decomposed into 4 focused files: `websocket_command.cpp`, `websocket_broadcast.cpp`, `websocket_auth.cpp`, `websocket_cpu_monitor.cpp`.

**`main.cpp` Size (1579 Lines)** (FIXED: 2026-03-22)
- Extracted `diag_api.cpp` and `siggen_api.cpp`. Main reduced by ~170 lines.

**ArduinoJson `JsonDocument` Stack Allocation** (MITIGATED: 2026-03-22)
- Static reuse + heapCritical guard in `mqtt_ha_discovery.cpp`. ArduinoJson v7 handles overflow gracefully.

**`innerHTML` Usage Without Sanitization** (FIXED: 2026-03-22)
- `escapeHtml()` moved to `01-core.js`. Audit completed across 3 key files (`05-audio-tab.js`, `15-hal-devices.js`, `20-wifi-network.js`).

**`strncpy()` Without Explicit Null Termination** (FIXED: 2026-03-22)
- `hal_safe_strcpy()` helper added. All 107+ `strncpy()` calls migrated to safe wrapper.

### LOW (All Resolved — Prior)

**Password Storage** (UPGRADED: 2026-03-22)
- PBKDF2 50,000 iterations (`p2:` format). Backward-compatible migration from `p1:` and legacy SHA256.

**OTA SSL Certificate Validation** (FIXED: 2026-03-22)
- Certs extracted to `src/ota_certs.h`. `tools/update_certs.js` regeneration script.

**Frontend API/WS Response Validation** (FIXED: 2026-03-22)
- `safeJson()` on `apiFetch()`. `validateWsMessage()` for critical WS types. 32 high-impact sites migrated.

---

## Active Risks (No Known Failures, Monitoring)

### WiFi SDIO Bus 0 Race During Boot

- **Risk**: WiFi co-processor initializes during `setup()`. Discovery checks `connectSuccess`, `connecting`, and `activeInterface` flags.
- **Files**: `src/hal/hal_discovery.cpp`, `src/wifi_manager.cpp`
- **Current mitigation**: Boot sequence is sequential: WiFi `begin()` → connection callback → HAL discovery. No race observed.
- **Test coverage**: 5 SDIO guard tests in `test_hal_discovery`.
- **Recommendation**: Monitor for async WiFi changes.

### Device Re-initialization Race (Deferred Dispatch)

- **Risk**: Rapid enable/disable clicks on same device could create toggle sequence. Queue dedup removes old request, executes last action only.
- **Files**: `src/state/hal_coord_state.h`, `src/main.cpp`
- **Current mitigation**: Queue dedup + overflow counter. 16 tests in `test_hal_coord`.
- **Recommendation**: Monitor overflow telemetry in field.

---

## Security Considerations

### Authentication & Authorization

**HttpOnly Cookie**: Cookies set with `HttpOnly` + `Secure` + `SameSite=Strict`. SECURE.

**PBKDF2-SHA256 (50k iterations)**: `p2:` format with auto-migration from `p1:` (10k) and legacy SHA256. SECURE.

**WebSocket Token TTL (60s)**: Short-lived tokens from `GET /api/ws-token`. 16-slot pool. SECURE.

**Default AP Password**: `DEFAULT_AP_PASSWORD "12345678"` in `src/config.h`. Used for AP mode WiFi and initial web password. First-boot random password displayed on TFT/serial for web auth. The AP WiFi password remains `12345678` unless changed by user.
- **Files**: `src/config.h` (line 211), `src/state/wifi_state.h` (line 16)
- **Impact**: Anyone in WiFi range can connect to AP. Web auth provides second layer.
- **Recommendation**: Consider generating random AP password on first boot (same as web password). Priority: LOW (AP mode is initial setup only)

### Network Security

**No TLS on HTTP Server**: All HTTP/WS traffic is plaintext on the local network. AP mode captive portal uses plain HTTP. TLS would require certificate management on embedded device.
- **Impact**: Credentials and session cookies transmitted in cleartext on LAN.
- **Recommendation**: Deferred. TLS certificate management is complex on ESP32. Priority: LOW (local network only)

---

## Performance Bottlenecks

### Audio Pipeline CPU Load (MITIGATED)

Graduated CPU thresholds at 80%/95%. FIR/convolution auto-bypass at critical. Pipeline timing instrumentation. Web UI DSP CPU card. 50% typical utilization with DSP chain.

### Large Auto-Generated Web Assets

- **Issue**: `src/web_pages.cpp` (600KB) and `src/web_pages_gz.cpp` (684KB) are auto-generated from `web_src/`. Combined 1.28MB of source code that must be compiled on every build.
- **Files**: `src/web_pages.cpp`, `src/web_pages_gz.cpp`
- **Impact**: Increases compile time. Large diffs on any web change. Not a runtime concern (gzipped transfer is ~85% smaller).
- **Recommendation**: Already mitigated by gzip compression for transfer. Build time impact is acceptable.

---

## Fragile Areas

### I2S Audio Driver Pin Configuration

- **Files**: `src/i2s_audio.cpp` (lines 180-320), `src/hal/hal_device_manager.cpp`
- **Fragility**: I2S pin overrides resolved from 3 sources in specific order (HalDeviceConfig, config.h constants, hardcoded fallbacks).
- **Protected by**: 8 unit tests in `test_i2s_config_cache`. `_resolveI2sPin()` helper.
- **Safe modification**: Always use `_resolveI2sPin(cfg, fallback)`. Add test if changing resolution order.

### HAL Device State Machine

- **Files**: `src/hal/hal_device_manager.cpp` (lines 140-180)
- **Fragility**: 8 states with guarded transitions. New states require updating all guard conditions.
- **Protected by**: 30 HAL core tests + 24 discovery tests. `HalStateChangeCb` keeps bridge in sync.
- **Safe modification**: Do NOT add states without tests for all transitions. Grep for `_state ==`.

### WebSocket Message Dispatch (Firmware <-> JS Contract)

- **Files**: `src/websocket_command.cpp` (parseCommand), `web_src/js/02-ws-router.js`
- **Fragility**: Firmware and JS must agree on message type strings. Rename without updating both sides causes silent failure.
- **Protected by**: 18 E2E tests verify end-to-end command flow. `validateWsMessage()` for 3 critical types.
- **Safe modification**: Update both sides. Add E2E test for new commands.

### DSP Biquad Coefficient Computation

- **Files**: `src/dsp_biquad_gen.h`, `src/dsp_coefficients.cpp`
- **Protected by**: `normalize()` guards, NaN/Inf/stability validation, `DIAG_DSP_COEFF_INVALID`, 32+ coefficient tests.
- **Safe modification**: Do NOT copy formulas from different sources without unit tests.

---

## Scaling Limits

### Matrix Routing (8x16 Fixed)

- 8 input lanes, 16 output channels, ~66KB PSRAM buffers
- Scaling to 16x16 requires `AUDIO_PIPELINE_MAX_INPUTS` increase, output DSP stereo-pair refactor
- **Blocking**: Output DSP assumes stereo pairs. Priority: DEFERRED.

### HAL Device Limit (32 slots, 14 at boot, 18 free)

- Memory impact negligible. 33rd device rejected with diagnostic. CLOSED.

### Diagnostic Journal (800 entries, ~64KB)

- ~100-200 entries/day. Consider rotation at 2000+. Priority: LOW.

---

## Dependencies at Risk

### Arduino-ESP32 Framework (v3.x, IDF5)

- Stable for 1.12.3. Lock to `~3.0` in `platformio.ini`. Test any framework upgrades. Priority: MEDIUM.

### LovyanGFX (v1.2.x)

- Stable. No DMA on P4 (synchronous SPI). Monitor releases. Priority: LOW.

### ESP-DSP Pre-built Library

- Stable. Source available if Espressif drops P4 support. Priority: LOW.

### Vendored WebSockets (v2.7.3)

- No known CVEs. Periodically diff against upstream. Priority: LOW.
- **Decision (2026-03-22)**: Retain vendored library. ESP-IDF native httpd_ws migration evaluated and deferred (227 endpoint + 39 WS API rewrite, 113 E2E test rework). No known CVEs. Revisit on framework migration to pure ESP-IDF.

---

## Test Coverage Gaps

### I2S Clock Continuity (Hardware Integration)

- MCLK GPIO continuity during state transitions not testable without oscilloscope
- Unit tests verify config logic only
- Priority: MEDIUM

### WiFi/Ethernet Failover

- No test for WiFi-to-Ethernet transition
- Priority: LOW (Ethernet is proof-of-concept)

### MQTT Broker Integration

- Only mock MQTT tests. No real broker test.
- Priority: LOW (QoS 1 acceptable for telemetry)

---

## Missing Critical Features

None identified. Suggested enhancements (not critical):
- Archive mirror for OTA downloads (deferred, requires S3)
- TLS server for AP mode (deferred, certificate management complex)
- Multi-device clustering (not planned, significant architecture change)
- Centralized HTTP response middleware for security headers (DONE: `server_send()` wrapper)

---

*Concerns analysis: 2026-03-22*

**Summary of changes from previous version:**
- All 7 active concerns mitigated and moved to resolved section (HTTP security headers, frontend `.json()`, websocket_handler.cpp size, main.cpp size, JsonDocument stack allocation, innerHTML sanitization, strncpy null termination)
- No active concerns remain
- Test counts updated (2354 C++/106 modules, 113 E2E/22 specs)
- Vendored WebSockets decision record added (retain, defer ESP-IDF migration)
- Default AP password security consideration preserved
- All previously resolved items preserved as historical record
