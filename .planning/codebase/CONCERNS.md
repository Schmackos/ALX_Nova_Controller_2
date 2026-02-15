# Codebase Concerns

**Analysis Date:** 2026-02-15

## Tech Debt

### Legacy Macro-Based Global Variable Aliases

**Issue:** The codebase uses `#define` macros to alias AppState singleton members (e.g., `#define wifiSSID appState.wifiSSID`) for backward compatibility. This pattern obfuscates the actual global state access and makes code harder to trace.

**Files:**
- `src/main.cpp` lines 81-131 (50+ macro aliases)
- `src/app_state.h` line 522 (TODO comment)

**Impact:** New code must use these aliases or the direct `appState.memberName` approach, creating two incompatible patterns in the same codebase. Makes refactoring risky.

**Fix approach:** Incrementally update all handlers to use `appState.memberName` directly, then remove macro aliases. No API impact, but requires careful verification of each module.

### Settings File Line-Based Persistence

**Issue:** Settings are persisted as 28 lines in `/settings.txt` with no version header or schema documentation. Adding new settings requires appending to the end and updating `loadSettings()` to parse the correct line number.

**Files:**
- `src/settings_manager.cpp` (lines 27-200+ with hardcoded line parsing)
- Each feature adds more `String lineN = file.readStringUntil('\n')` calls

**Impact:** Schema is fragile. Missing a line number offset breaks all loaded settings. No way to version the file format. Expansion becomes linear complexity O(N).

**Fix approach:** Migrate to JSON-based settings file (`/settings.json`) with schema versioning. Gradual migration: load both formats, write JSON only. Keep backward compatibility window for firmware updates.

### Dual-ADC I2S Configuration Workaround

**Issue:** PCM1808 ADCs use ESP32-S3 I2S0 and I2S1 both configured as **master RX** (not slave) due to intractable ESP32-S3 I2S slave mode DMA bug. This is a known hardware limitation documented in CLAUDE.md but represents a significant deviation from typical I2S usage.

**Files:**
- `src/i2s_audio.cpp` (I2S init and driver configuration)
- `src/config.h` NUM_AUDIO_ADCS = 2

**Impact:** Both ADCs depend on I2S0 clock outputs. If I2S0 is uninitialized or reconfigured, I2S1 data becomes invalid. DAC TX changes can destabilize ADC RX. I2S clock line sharing increases noise coupling risk.

**Fix approach:** Document the dependency graph clearly. Add boot validation to confirm both ADCs sync to same clock. Consider single-master multi-slave redesign (requires hardware change).

## Known Bugs

### Session Timeout Calculation Uses `millis()` in Safety-Critical Path

**Issue:** Session timeout uses `unsigned long` millis-based clock which wraps every 49 days. After wraparound, all sessions become invalid (timeout comparison breaks). Worse, new sessions created at wraparound have `createdAt = 0` which could be interpreted as ancient.

**Files:**
- `src/auth_handler.cpp` lines 192-206 (validateSession)
- `src/auth_handler.cpp` line 145 (createSession)

**Impact:** Every 49 days, all users are logged out and cannot re-login until device is restarted. High-impact availability issue for always-on installations.

**Fix approach:** Use `esp_timer_get_time()` (64-bit microseconds, good for ~292 million years) instead of `millis()`. Update Session struct to use uint64_t timestamps. Add wraparound unit test.

### I2S Driver Reinstall Race Condition Incomplete Mitigation

**Issue:** DAC enable/disable uninstalls and reinstalls I2S0 driver while audio task may be calling `i2s_read()`. The `appState.audioPaused` flag mitigation has a potential edge case: the audio task checks the flag at loop entry, but if the flag is set mid-read, the task is not interrupted.

**Files:**
- `src/dac_hal.cpp` (dac_enable_i2s_tx / dac_disable_i2s_tx)
- `src/i2s_audio.cpp` audio_capture_task

**Impact:** Rare crashes when user toggles DAC on/off while audio is streaming. 50ms delay after setting flag reduces but doesn't eliminate risk on highly loaded systems.

**Fix approach:** Use FreeRTOS task suspend/resume (`vTaskSuspend(audioTaskHandle)`) instead of yield loop. More robust and immediate. Or: redesign to avoid driver reinstall (keep both RX+TX drivers installed, disable via I2S channel control).

### OTA Backoff Arithmetic Vulnerability

**Issue:** OTA check failure count increments unboundedly; backoff calculation has no upper bound check. If `_otaConsecutiveFailures` overflows (>INT_MAX), backoff returns negative interval causing logic inversion.

**Files:**
- `src/ota_updater.cpp` lines 108-119 (getOTAEffectiveInterval)

**Impact:** Low severity (requires months of continuous failures), but could cause rapid OTA polling at 1kHz if integer overflows.

**Fix approach:** Cap `_otaConsecutiveFailures` to 20 (already mapped in getOTAEffectiveInterval). Add compile-time assertion or explicit cast to unsigned.

## Security Considerations

### WebSocket Authentication Token Validation Uses Simple Equality

**Issue:** Session IDs are validated via direct string equality comparison in a loop (not constant-time). While not a password comparison, timing-based side-channel attacks could theoretically enumerate valid session IDs.

**Files:**
- `src/auth_handler.cpp` line 195 (activeSessions[i].sessionId == sessionId)

**Impact:** Low (attacker already needs to guess UUID format), but WebSocket is untrusted input. Multiple comparison attempts could be logged/analyzed.

**Fix approach:** Use `timingSafeCompare()` (already exists in auth_handler for passwords) for session ID validation as well. Prevents timing leaks.

### Crash Log Ring Buffer No Size Validation

**Issue:** Crash log counts are stored as plain integers with no bounds validation on load. Corrupted file could specify `count > CRASH_LOG_MAX_ENTRIES`, then writeIndex wraparound logic fails.

**Files:**
- `src/crash_log.cpp` lines 11-27 (crashlog_load)

**Impact:** Corrupted crash log prevents boot if read() silently fails or returns garbage. Could cause device to be unrecoverable without serial access.

**Fix approach:** Validate `count` and `writeIndex` on load, reset to 0 if out of bounds. Add CRC32 footer to detect corruption. Log warning when recovered.

### EEPROM I2C Access Not Protected from Concurrent Tasks

**Issue:** DAC EEPROM operations (`eeprom_read_block`, `eeprom_write_block`) run on main loop (Core 0) without mutex protection. If another task (unlikely but possible via WebSocket handler) attempts I2C access, race condition causes data corruption or lockup.

**Files:**
- `src/dac_eeprom.cpp` (all I2C functions)
- No mutex in `src/dac_hal.cpp` eeprom operations

**Impact:** Currently low (only main loop writes), but architecture doesn't prevent future concurrent access. I2C is not thread-safe by design.

**Fix approach:** Add `xSemaphoreMutex` for all I2C bus access (DAC EEPROM + any I2C DAC control). Pattern already used in buzzer_handler for GPIO ISR protection.

### Default AP Password Hardcoded in Firmware

**Issue:** `DEFAULT_AP_PASSWORD` (defined in config.h, likely "12345678" or similar) is compiled into every firmware binary. Reverse engineering firmware reveals access credentials for AP mode, which is enabled by default in public deployments.

**Files:**
- `src/config.h` (DEFAULT_AP_PASSWORD)
- `src/app_state.h` line 75 (initialized to DEFAULT_AP_PASSWORD)

**Impact:** Medium. AP mode is meant as fallback for WiFi setup, but compromises device if user doesn't change default password. No forced password change on first boot.

**Fix approach:** Generate random AP password from eFuse on first boot, store in NVS. Display in web UI during onboarding. Require password change before AP can be disabled.

## Performance Bottlenecks

### Large Web Pages String in Flash (11KB web_pages.cpp)

**Issue:** HTML/CSS/JS for web interface is embedded as 11KB uncompressed string in `src/web_pages.cpp`, then gzipped to `web_pages_gz.cpp`. Decompression happens on every HTTP request from the ESP32. Large allocations for decompression may spike heap usage.

**Files:**
- `src/web_pages.cpp` (11283 lines)
- Decompression during HTTP handling

**Impact:** Web UI load time on slow networks or high-frequency requests could cause heap fragmentation. Heap pressure during high WiFi RX traffic.

**Fix approach:** Already using gzip (good). Consider splitting into multiple smaller chunks (CSS, JS modules) lazy-loaded by HTML. Or: serve static files directly from LittleFS instead of embedding.

### Audio Waveform/Spectrum Broadcasting Every 20ms

**Issue:** `sendAudioData()` broadcasts 1KB+ binary frames (waveform + spectrum) every 20ms when audio is present. On multiple WebSocket clients, this multiplies bandwidth usage. No client subscription filtering initially implemented.

**Files:**
- `src/websocket_handler.cpp` (sendAudioData)
- Binary WS frames now implemented per MEMORY.md, but frequency not optimized

**Impact:** High WiFi load with 3+ clients, potential packet loss. Heap pressure from binary frame allocations.

**Fix approach:** Per-client subscription tracking already added (MEMORY.md). Implement adaptive update rate based on client count. Skip spectrum/waveform if all clients muted. Consider push-back on queue full.

### Settings Export/Import Allocates Large JSON in Stack

**Issue:** DSP config export creates large ArduinoJson documents (32+ KB) on the stack for serialization. On ESP32 with limited stack per task (typically 4-8KB per task), this risks stack overflow in non-main-loop contexts.

**Files:**
- `src/dsp_api.cpp` line 263 (export comment mentions heap allocation as workaround)

**Impact:** Nested MQTT callbacks or WebSocket handlers calling export could cause stack overflow. Already mitigated in some places but not consistently.

**Fix approach:** Always allocate JsonDocument on heap, not stack. Add size assertions at compile time for critical paths. Consider streaming JSON instead of buffering entire document.

### Heap Critical Flag Checked Every 30s, But Not Used Proactively

**Issue:** Main loop monitors `heapCritical` flag (free heap < 40KB) every 30s but only logs it. No proactive mitigation: high-memory operations aren't blocked, features aren't disabled, or buffers aren't shrunk.

**Files:**
- `src/main.cpp` (heap monitoring loop)
- `src/app_state.h` heapCritical flag

**Impact:** Device can silently degrade when heap is low (WiFi RX buffers fail silently, WebSocket broadcasts drop). User sees no warning. By the time heapCritical is true, operations are already failing.

**Fix approach:** Implement heap quota system: pre-reserve 40KB always, deny new allocations if available < threshold. Disable optional features (spectrum, waveform, DSP) in heap-critical mode. Broadcast critical state to UI.

## Fragile Areas

### Audio Capture Task Real-Time Guarantees Depend on Core Affinity

**Issue:** Audio capture runs on Core 1 with priority 3 to avoid starving Core 0 watchdog. But Core 1 also runs GUI task. If GUI task runs long without yield (LVGL draw operations), audio task is starved, causing DMA timeouts and I2S recovery cycles.

**Files:**
- `src/config.h` TASK_CORE_AUDIO = 1
- `src/gui/gui_manager.cpp` gui_task
- `src/i2s_audio.cpp` audio_capture_task

**Impact:** Audio drops under heavy GUI load (e.g., long canvas redraws). Triggers watchdog/recovery, slowing down both audio and GUI. High frame rates (30Hz+) increase dropout risk.

**Test coverage:** `test/test_i2s_audio/` tests don't model Core 1 contention.

**Safe modification:** Measure LVGL frame times on actual hardware. Add `vTaskDelay(1)` yield points in long loops. Consider moving non-critical GUI to separate task on Core 0.

### Settings File Format Fragility During Firmware Updates

**Issue:** When firmware adds new settings, old settings files have fewer lines. `loadSettings()` calls `readStringUntil('\n')` for all 28 lines, and missing lines read as empty strings. Silent migration can lose user settings if line numbers shift.

**Files:**
- `src/settings_manager.cpp` (all line parsing)

**Impact:** Firmware updates (even minor) risk silent settings loss if new features are added mid-list. No rollback mechanism.

**Test coverage:** `test/test_settings/` tests don't cover partial-file scenarios.

**Safe modification:** Add version header as line 1 (format: "VER=2"). Check version on load, apply migration functions per version. Preserve any lines not yet recognized (forward-compat). Never insert new settings mid-file.

### DSP Pipeline State Swap Non-Atomic

**Issue:** Double-buffered DSP config uses `volatile` exchange, but is not atomic with respect to audio processing. If config swap happens mid-frame, one half of audio processes with old config, second half with new config. Causes audible click/pop.

**Files:**
- `src/dsp_pipeline.cpp` (state swap logic)

**Impact:** DSP parameter changes (EQ, gain) audible as brief clicks/pops. Rare but noticeable on live audio.

**Test coverage:** `test/test_dsp/` tests don't measure frame-boundary timing.

**Safe modification:** Swap config only at frame boundary (after I2S callback completes processing). Use atomic flag to signal when swap is safe. Or: process entire frame with one config, then swap for next frame.

### WebSocket Broadcast Bypasses Auth Under Race Condition

**Issue:** WebSocket event handler sends initial `getAllState()` before authentication is fully validated. If client disconnects immediately after connect, auth timer (`wsAuthTimeout`) may fire with valid old session from previous client.

**Files:**
- `src/websocket_handler.cpp` (webSocketEvent, WStype_CONNECTED)
- `src/auth_handler.cpp` (session validation timeout logic)

**Impact:** Rare leak of device state (audio levels, WiFi status) to unauthenticated client on connection reuse. Usually millisecond window.

**Test coverage:** `test/test_websocket/` and `test/test_auth/` don't cover rapid connect/disconnect sequences.

**Safe modification:** Require explicit auth command before ANY state broadcast. Send minimal "login required" message on connect, full state only after auth success.

## Scaling Limits

### Max Concurrent WebSocket Clients = 10

**Issue:** Fixed array `wsAuthStatus[MAX_WS_CLIENTS]` with `MAX_WS_CLIENTS = 10` (see config). If more than 10 clients connect, server reuses oldest client's slot, overwriting its auth state and subscription flags.

**Files:**
- `src/websocket_handler.cpp` (wsAuthStatus array)
- `src/config.h` likely defines MAX_WS_CLIENTS

**Impact:** Home Assistant + 5 phones + 3 tablets = 9 clients is feasible. 11th client causes 1st client's session to be overwritten. Causes disconnection and re-auth of oldest client.

**Fix approach:** Increase MAX_WS_CLIENTS to 32 (still modest heap). Or: migrate to dynamic std::vector (more complex but scalable). Monitor actual concurrent client count.

### Max MQTT Topics Per Home Assistant Discovery = 1 Device

**Issue:** MQTT HA discovery publishes one "ALX Nova" device with ~30 entities. Home Assistant UI becomes cluttered. Cannot group multiple controllers under one MQTT broker without name collision.

**Files:**
- `src/mqtt_handler.cpp` (HA discovery logic)

**Impact:** Scaling to 5+ devices requires manual intervention. Device names must be globally unique or topics collide.

**Fix approach:** Include `deviceSerialNumber` in MQTT base topic (already uses `appState.mqttBaseTopic`). HA discovery will auto-create separate device per serial. Verify in HA UI that discovery works multi-device.

### DSP Max Stages = 24 Per Channel

**Issue:** Each of 4 channels can have max 24 DSP stages. Complex audio chains (multiband + all PEQ + custom filters) quickly exceed limit. No graceful degradation, just silent truncation.

**Files:**
- `src/config.h` DSP_MAX_STAGES = 24
- `src/dsp_api.cpp` silently skips stages beyond limit

**Impact:** Complex presets from Equalizer APO (~60+ stages) must be manually downsampled. Users lose detail in complex setups.

**Fix approach:** Expand to 40-50 stages (PSRAM permitting). Add warning when nearing limit. Implement stage compression (combine multiple EQ bands into single multiband compressor stage).

## Dependencies at Risk

### ArduinoJSON v7.4.2 Security Updates Lag

**Issue:** ArduinoJSON is a community-maintained library with occasional version bumps. Current v7.4.2 (specified in platformio.ini) may have known JSON parsing vulnerabilities if not updated regularly.

**Files:**
- `platformio.ini` (ArduinoJSON@^7.4.2)

**Impact:** Malformed JSON in API requests could trigger buffer overflows or DoS. Firmware updates infrequent, so vulnerability window is long.

**Fix approach:** Set up monthly dependency scanning (e.g., GitHub Dependabot). Pin to specific patch version and update quarterly. Monitor ArduinoJSON GitHub releases.

### Pre-Built ESP-DSP Library Cannot Be Updated

**Issue:** DSP pipeline uses pre-built `libespressif__esp-dsp.a` (Espressif closed-source SIMD library). If vulnerabilities or bugs are found, cannot patch without waiting for Espressif Arduino release.

**Files:**
- `platformio.ini` ESP-DSP include paths
- `lib/esp_dsp_lite/` ANSI C fallback (not used on ESP32)

**Impact:** Medium. Pre-built libs are opaque; bugs are hard to diagnose. No fix control.

**Fix approach:** Fallback `lib/esp_dsp_lite/` is already available for native tests. Consider porting critical SIMD ops to custom inline assembly (e.g., vector multiply for crossover routing). Limits future performance but enables control.

## Missing Critical Features

### No Firmware Rollback After Failed OTA

**Issue:** OTA update downloads, verifies, and installs. If new firmware crashes immediately, device is stuck in bad state. Manual UART/serial recovery needed.

**Files:**
- `src/ota_updater.cpp` (OTA install logic)

**Impact:** High for remote installations. A bad firmware release renders all devices unreachable.

**Fix approach:** Implement A/B partitioning (requires bootloader change). Or: add boot counter—if new firmware crashes within 2 boots, rollback to previous. Needs crash detection.

### No Rate Limiting on API Endpoints

**Issue:** REST API endpoints (/api/smartsensing, /api/diagnostics, etc.) have no rate limiting. Malicious client can flood ESP32 with requests, starving legitimate traffic.

**Files:**
- `src/main.cpp` (all server.on() registrations)
- No rate limiting middleware

**Impact:** Medium. Device can be DoS'd from within local WiFi (not external internet). Affects availability.

**Fix approach:** Track request count per client IP, return HTTP 429 if exceeded. Or: implement request queue with max pending (drop oldest if full). Add per-endpoint rate limits in config.

### No Graceful Shutdown / Firmware Update Timeout

**Issue:** When firmware update is initiated, device immediately begins download without waiting for existing operations to complete. Active DSP reconfiguration, MQTT publish, or WebSocket broadcast could be interrupted mid-operation.

**Files:**
- `src/ota_updater.cpp` (startOTADownloadTask)

**Impact:** Low probability but high impact: incomplete settings writes, truncated MQTT messages, orphaned WebSocket connections.

**Fix approach:** Implement shutdown sequence: 1) Notify all tasks to complete current operation. 2) Wait (timeout 10s) for each to ACK. 3) Begin OTA. Or: defer OTA to next reboot if critical operation in progress.

## Test Coverage Gaps

### WebSocket Concurrency Not Tested

**Issue:** WebSocket handler processes client messages sequentially in event loop. If two clients send conflicting commands rapidly (e.g., setAmplifier(true) + setAmplifier(false) in 1ms), race condition in dirty flag logic could cause inconsistent state.

**Files:**
- `src/websocket_handler.cpp`
- `test/test_websocket/` (no concurrency tests)

**Risk:** Medium. Timing-dependent, hard to reproduce.

**Priority:** High - WebSocket is user-facing and stress-tested.

### OTA Checksum Verification Not Tested Against Corrupted Files

**Issue:** OTA verifies SHA256 checksum, but tests only check happy path. No test for what happens if checksum verification fails mid-stream or firmware file is truncated.

**Files:**
- `src/ota_updater.cpp` (SHA256 verification)
- `test/test_ota/` (no corruption tests)

**Risk:** Low (GitHub releases are trusted), but edge case could leave device unrecoverable.

**Priority:** Medium - OTA is critical infrastructure.

### Audio Diagnostics Health Status Transitions Not Fully Tested

**Issue:** Audio health status (OK → CLIPPING → NO_DATA transitions) depend on timing thresholds and signal levels. Edge cases like brief signal dropout, intermittent clipping, or glitched ADC detection are not covered.

**Files:**
- `src/i2s_audio.cpp` (audio_derive_health_status)
- `test/test_audio_diagnostics/` (14 tests, but edge cases may be missing)

**Risk:** Medium. Incorrect health status affects auto-off logic and user diagnostics.

**Priority:** High - auto-off timer depends on health.

### Button Handler Long-Press Timing Edge Cases Not Tested

**Issue:** Button handler distinguishes long-press (2s) and very-long-press (10s) with no tolerance for timing jitter. If main loop runs late, millis() delta could be off by 10-50ms, causing wrong press type detected.

**Files:**
- `src/button_handler.cpp` (press timing logic)
- `test/test_button/` (no jitter/timing tests)

**Risk:** Low. Press timing is non-critical (just triggers factory reset or short commands).

**Priority:** Low.

### GUI State Sync with Web/MQTT Backend Not Tested

**Issue:** GUI, Web UI, and MQTT maintain same state for brightness, volume, mode. Test coverage for `backlightOn`, `screenTimeout` sync is missing.

**Files:**
- `src/gui/gui_manager.cpp`
- `test/test_gui_*` (no multi-backend sync tests)

**Risk:** Medium. GUI desyncs from web UI if simultaneous changes occur.

**Priority:** Medium - affects user experience.

---

*Concerns audit: 2026-02-15*
