# Codebase Concerns

**Analysis Date:** 2026-03-22

## Resolved Issues (Archived)

The following concerns have been **fully addressed** and are retained for historical reference:

- HAL Registry capacity exhaustion (FIXED: `HAL_MAX_DEVICES` 24→32)
- Weak password hashing (FIXED: PBKDF2 10k→50k iterations, `p2:` format with auto-migration)
- WebSocket rate saturation (FIXED: Adaptive skip tiers 2/4/6/8 based on client count, warning toast)
- OTA certificate management (FIXED: Extracted to `src/ota_certs.h`, `tools/update_certs.js`)
- Web UI JSON injection (FIXED: `safeJson()` on 32 call sites, `validateWsMessage()` for 3 WS types)
- HTTP response headers (FIXED: `http_add_security_headers()` adds X-Frame-Options/X-Content-Type-Options)
- I2C Bus 0 SDIO conflict (FIXED: `hal_wifi_sdio_active()` skips Bus 0, returns `partialScan`)
- HAL device toggle queue overflow (FIXED: `HalCoordState` capacity 8 with overflow telemetry)
- DMA buffer heap pressure (FIXED: Eager pre-allocation 16×2KB at boot, `DIAG_AUDIO_DMA_ALLOC_FAIL`)
- Heap memory pressure (FIXED: Graduated 3-state system at 50KB/40KB thresholds)

---

## Active Concerns (Current)

### Memory & Allocation

**PSRAM Allocation Fragmentation:**
- **Problem**: PSRAM allocations across ~10 sites (audio pipeline, DSP, HAL) lack centralized governance. Lifetime allocation tracking present but no compaction strategy.
- **Files**: `src/psram_alloc.cpp`, `src/audio_pipeline.cpp`, `src/dsp_pipeline.cpp`
- **Impact**: Long-running sessions may fragment PSRAM, reducing available contiguous blocks. No runtime defragmentation during feature disables.
- **Mitigation**: `psram_get_stats()` exposes fallback/failure counts. Graduated pressure states trigger feature shedding (DSP delay/convolution refused at 512KB critical). Actual worst-case unchanged (~330KB max).
- **Recommendation**: Add per-subsystem allocation tracking timestamps. Consider PSRAM reset path on extended idle.

**Stack Depth in Audio Task:**
- **Problem**: Audio pipeline task running on Core 1 with `TASK_STACK_SIZE_AUDIO = 12288` bytes. Deep call chains (pipeline → DSP → biquad/FIR → esp_dsp) leave narrow margin.
- **Files**: `src/config.h` (line 223), `src/audio_pipeline.cpp`, `src/dsp_pipeline.cpp`
- **Impact**: Stack overflow in audio task crashes entire system silently (watchdog reboot). Symptoms may appear as audio dropouts or random resets during high DSP load.
- **Mitigation**: No dynamic checks currently in place. Static analysis suggests current worst-case ~3KB used.
- **Recommendation**: Implement `uxTaskGetStackHighWaterMark(audio_pipeline_task)` polling every 30s. Log warning at 25% margin, error at 10%. Consider increasing stack if high water mark exceeds 9KB.

**DMA Buffer Internal SRAM Reserve:**
- **Problem**: 16×2KB=32KB DMA buffers MUST stay in internal SRAM (DMA cannot access PSRAM). Eagerly allocated at boot before WiFi connects, but WiFi RX buffers also claim internal heap. If heap drops below 40KB, WiFi RX becomes lossy.
- **Files**: `src/audio_pipeline.cpp` (lines 44-55), `src/config.h` (line 239-240)
- **Impact**: Incoming WiFi packets (HTTP, WebSocket, MQTT) silently dropped when heap saturated. MQTT publish still works (TX buffered). User perceives unresponsive web UI or delayed state sync.
- **Mitigation**: Graduated heap pressure gate (50KB warning, 40KB critical). OTA checks skipped at critical. DSP stages refused. WS binary data suppressed.
- **Recommendation**: Monitor with `GET /api/psram/status` + Web UI hardware stats. Reserve 40KB explicitly for WiFi by refusing new DSP allocations earlier (threshold 60KB).

---

### Cross-Core Concurrency

**Audio Task I2S Driver Reinstall Race:**
- **Problem**: DAC module may call `i2s_driver_uninstall()` on Core 0, while audio task reads I2S on Core 1. Synchronization via `appState.audio.paused` (volatile bool) + binary semaphore `appState.audio.taskPausedAck` (FreeRTOS).
- **Files**: `src/dac_hal.cpp`, `src/audio_pipeline.cpp`, `src/state/audio_state.h`
- **Impact**: Race condition between driver uninstall and pending DMA read can trigger crash or audio pop. Semaphore timeout (50ms) can expire if audio task starved.
- **Mitigation**: Semaphore handshake implemented. Task yields after observing `paused=true` and giving semaphore.
- **Recommendation**: Add WDT guard: if semaphore timeout occurs, emit `DIAG_AUDIO_DRIVER_STALL` and force recovery. Consider increasing timeout to 100ms.

**MQTT Task State Change Detection:**
- **Problem**: MQTT publish task (`mqtt_task` on Core 0) reads `appState` fields directly (shadow comparison in `mqtt_publish.cpp`). Main loop (Core 1) sets dirty flags and modifies state. No mutex protecting reads.
- **Files**: `src/mqtt_publish.cpp`, `src/mqtt_task.cpp`, `src/app_state.h`
- **Impact**: Torn reads on fields larger than 4 bytes (strings, arrays) possible on ARM. Example: SSID being updated while MQTT task reads it.
- **Mitigation**: String fields read into local copies (strncpy). Numeric comparisons use direct volatile reads. No observed incidents.
- **Recommendation**: Add compile-time documentation: "All reads in mqtt_task are local-copy or volatile-safe. Do not add non-atomic state."

**Dirty Flag Race in DSP Config Swap:**
- **Problem**: DSP pipeline double-buffers state. `dsp_config_swap()` atomically swaps `_activeIndex`. Main loop reads active state via `dsp_get_active_state()`. No lock between read and use in pipeline task.
- **Files**: `src/dsp_pipeline.cpp` (lines 45-49), `src/audio_pipeline.cpp`
- **Impact**: Pipeline may process audio with stale state briefly after swap. Glitches audible as clicks/pops on EQ changes.
- **Mitigation**: Swap occurs between DMA buffer cycles (synchronized via pipeline task yield). Click-free in practice due to frame boundaries.
- **Recommendation**: Add atomic swap counter to detect missed updates. Log if pipeline detects swap in-flight.

---

### Performance & Timing

**WebSocket Frame Rate Adaptation Lag:**
- **Problem**: Binary rate skip factor (1/2/4/6/8) computed every 20ms based on client count. No hysteresis — can oscillate rapidly if client count hovers near threshold (e.g., 3.5 clients if averaged).
- **Files**: `src/websocket_handler.cpp` (lines 2430-2460), `src/config.h` (lines 150-153)
- **Impact**: Skip factor flutters, causing inconsistent frame delivery to UI. Spectrum visualization may stutter briefly.
- **Mitigation**: Adaptive rate recount occurs every `WS_AUTH_RECOUNT_INTERVAL_MS` (10s), not per-frame. Low impact in practice.
- **Recommendation**: Add low-pass filter to authenticated client count (e.g., moving average over 1s) before skip factor selection.

**GPIO Debounce Timing Variability:**
- **Problem**: Button/encoder debounce uses fixed 50ms delay (`BTN_DEBOUNCE_TIME` in `config.h`). Tolerant of edge jitter but ISR-driven (GPIO interrupt) timing depends on main loop responsiveness.
- **Files**: `src/button_handler.cpp`, `src/gui/gui_input.cpp`, `src/config.h` (line 161)
- **Impact**: Multipress detection can fail if main loop stalls >50ms during OTA or WiFi scanning. False double-clicks possible.
- **Mitigation**: Encoder ISR accumulates state via atomic XOR (Gray code). Button presses debounced in main loop sampling (not interrupt-driven).
- **Recommendation**: Add watchdog for main loop responsiveness. If task stall detected, increase debounce to 100ms dynamically.

**MQTT Publish Interval Drift:**
- **Problem**: MQTT task checks for state changes at 20 Hz (50ms). No timer-based synchronization with heartbeat interval (60s). Drift possible if mqtt_task blocked by WiFi/TLS.
- **Files**: `src/mqtt_task.cpp`, `src/mqtt_handler.cpp`, `src/config.h` (lines 200-203)
- **Impact**: Heartbeat may arrive late or skip if broker connection stalls. Home Assistant may mark device offline prematurely.
- **Mitigation**: Task uses `vTaskDelay(50)` polling, not blocking event wait. Reconnect logic retries with backoff.
- **Recommendation**: Add wall-clock check for heartbeat due time (compare elapsed time vs 60s). Force publish if deadline exceeded.

**I2S Sample Rate Change Glitch:**
- **Problem**: `i2s_audio_set_sample_rate()` calls `i2s_configure_adc1()` which may reinstall driver. No mute ramp before rate change. Audio suffers brief pop/click.
- **Files**: `src/i2s_audio.cpp`, `src/audio_pipeline.cpp`
- **Impact**: Audible click when sample rate changed (e.g., via web UI). User perceives quality loss.
- **Mitigation**: Rate changes rare in practice (manual menu only). Pipeline buffers zeroed after rate change.
- **Recommendation**: Implement mute ramp (e.g., 5ms linear fade to 0, rate change, fade back). Gated by `appState.audio.paused`.

---

### Hardware & I2C

**I2C Bus Timeout & Retry Backoff:**
- **Problem**: HAL I2C probe retry logic (`HAL_PROBE_RETRY_COUNT=2`, backoff `50ms`) may be insufficient for slow/high-capacitance buses. ESP-IDF default timeout on `i2c_master_read_slave()` is ~1s per transaction.
- **Files**: `src/hal/hal_discovery.cpp` (lines 118-120), `src/config.h` (lines 119-120)
- **Impact**: Expansion device discovery can hang for 5-10s on first boot if mezzanine connector loosely seated. User perceives delayed startup.
- **Mitigation**: Bus 0 (SDIO conflict) skipped when WiFi active. Buses 1/2 (onboard/expansion) always safe. Post-boot rescan via API.
- **Recommendation**: Increase max retry count to 3, backoff to 100ms. Add I2C bus reset (SCL/SDA toggle) before retry sequence.

**ADC PCM1808 PLL Stability:**
- **Problem**: Both PCM1808 ADCs share MCLK clock line. MCLK must remain continuous for PLL lock. If I2S driver reinstalled during audio task active, MCLK glitches.
- **Files**: `src/i2s_audio.cpp`, `src/audio_pipeline.cpp`, `CLAUDE.md` (Current Gotchas section)
- **Impact**: PCM1808 PLL loses lock, producing noise bursts or zero samples until relock (typically <100ms). Audible in final mix.
- **Mitigation**: Audio task pauses before driver reinstall (binary semaphore). MCLK fed from same 160MHz D2CLK divider for both I2S0/I2S1.
- **Recommendation**: Add MCLK frequency monitor (count GPIO22 toggles in 100ms window). Emit diagnostic if lock lost.

**DAC I2C Communication Frequency:**
- **Problem**: Expansion DAC drivers (ES9038PRO, ES9039PRO, etc.) update volume/mute via I2C writes on every request. No rate limiting or batching. High-frequency controls (slider drag) generate burst of transactions.
- **Files**: `src/hal/hal_es9038pro.cpp`, `src/hal/hal_es9039pro.cpp`, etc. (buildSink methods)
- **Impact**: I2C bus saturation under rapid volume changes. Pipeline latency increases. Possible underruns if DSP frame processing delayed.
- **Mitigation**: DAC API applies debounced save (2s). Per-device `setVolume()` methods write immediately (expected behavior).
- **Recommendation**: Add rate limiter to I2C writes: queue updates, batch into 1 transaction every 10ms. Preserve volume/mute path separate from DSP path.

---

### Networking & Web Services

**WiFi Reconnection Backoff Saturation:**
- **Problem**: WiFi retry state uses static retry intervals (30s full-list retry). If all configured networks unreachable, device attempts connection every 30s indefinitely. No exponential backoff beyond network list cycle.
- **Files**: `src/wifi_manager.cpp` (lines 48, 50), `src/config.h`
- **Impact**: Continuous WiFi churn on Core 0 starves other tasks. Battery drain on mobile deployments. MQTT disconnects frequent.
- **Mitigation**: Backoff exists for OTA checks (5/15/30/60min intervals). WiFi reconnect logic separate.
- **Recommendation**: Cap WiFi reconnect attempts to 12/hour after first 6 failures. Emit `EVT_WIFI_CRITICAL` at 10 consecutive failures. Notify user via Web UI.

**MQTT Broker Connection Timeout Blocking:**
- **Problem**: MQTT client socket timeout set to `MQTT_SOCKET_TIMEOUT_MS = 5000` (5s). If broker IP reachable but not responsive on port 1883, TCP handshake waits full 5s before timeout. Synchronous in `mqtt_task`, blocking other Core 0 work.
- **Files**: `src/mqtt_handler.cpp`, `src/config.h` (line 198)
- **Impact**: UI freezes for 5s if broker unreachable (poor UX). If multiple addresses resolved, 5s × number of IPs = 15-30s stall.
- **Mitigation**: Socket timeout capped at 5s (reasonable for LAN). Task uses 20Hz polling between retries (non-blocking).
- **Recommendation**: Reduce timeout to 2s for cloud brokers. Add DNS TTL caching to avoid repeated slow DNS lookups. Implement async TCP connect with external timer.

**Web Server Gzip Decompression Memory:**
- **Problem**: Web pages served as gzip-compressed binaries (`src/web_pages_gz.cpp`, 6.7KB compressed). Decompression buffer allocated dynamically. No size bounds on Accept-Encoding negotiation.
- **Files**: `src/web_pages.cpp` (auto-generated), `src/websocket_handler.cpp`
- **Impact**: Malformed gzip header could trigger unbounded allocation. Unlikely but possible vector for heap exhaustion DoS.
- **Mitigation**: Compression ratio ~2-3x (typical for HTML/JS), max uncompressed ~20KB. Heap pressure gates prevent huge allocations.
- **Recommendation**: Add max size check (50KB) before decompression. Emit `DIAG_HTTP_GZIP_FAIL` on oversized payload.

---

### Security

**Authentication Cookie HttpOnly Flag:**
- **Problem**: Web auth uses cookie with `HttpOnly` flag set. Cookie extraction via XSS impossible, but logout functionality relies on same cookie. No CSRF token on logout endpoint.
- **Files**: `src/auth_handler.cpp`, `src/login_page.h`
- **Impact**: Logout can be triggered cross-site if user visits attacker domain while logged in (CSRF). Limited impact since logout just clears local state, no side effects.
- **Mitigation**: PBKDF2 auth (50k iterations) mitigates brute-force. Password field cleared on submit.
- **Recommendation**: Add CSRF token (nonce in HTML form) to logout request. Validate token matches session.

**WebSocket Auth Token Reuse:**
- **Problem**: WS auth tokens generated via `GET /api/ws-token` (60s TTL, 16-slot pool). No rotation after token use — same token valid for full 60s or until pool eviction. Multiple WS clients can share 1 token.
- **Files**: `src/websocket_handler.cpp` (lines 2440-2480), `src/auth_handler.cpp`
- **Impact**: If token leaked (e.g., via browser DevTools), attacker can create unlimited WS connections for 60s. Rate limiting per-token not enforced.
- **Mitigation**: Tokens are short-lived (60s). Pool size limited (16 tokens). Auth count recalibrated every 10s.
- **Recommendation**: Implement one-time token consumption (invalidate after first WS handshake). Rotate token on each new WS connection.

**Telemetry Endpoint No Rate Limiting:**
- **Problem**: `/api/diag/snapshot` and `/api/psram/status` endpoints return detailed hardware state (heap, PSRAM, task counts, diagnostics). No per-IP rate limiting.
- **Files**: `src/psram_api.cpp`, `src/main.cpp` (HAL API registration)
- **Impact**: Unauthenticated attacker can poll hardware state at 100 Hz, inferring load patterns and device capabilities.
- **Mitigation**: Endpoints auth-guarded (require web password). API returns generic JSON, no command injection.
- **Recommendation**: Add global rate limiter: max 10 reqs/sec per client IP. Return HTTP 429 (Too Many Requests) on violation.

**Settings Export File Plaintext Storage:**
- **Problem**: `/config.json` persisted to LittleFS contains WiFi SSID + MQTT broker hostname (not credentials). No encryption at rest.
- **Files**: `src/settings_manager.cpp`, `src/hal/hal_settings.cpp`
- **Impact**: Physical device theft allows reading network topology. Credentials stored separately in NVS (more secure). Low risk in practice.
- **Mitigation**: WiFi credentials stored in NVS partition (hardware-backed on ESP32). MQTT credentials not persisted (requires re-entry on reboot).
- **Recommendation**: Document that `/config.json` requires physical access (not exposed via web). Add optional encryption layer (AES-CTR with derived key from device MAC + master password).

---

### Testing & Coverage

**USB Audio Path Not Tested on Hardware:**
- **Problem**: USB Audio feature (`USB_AUDIO_ENABLED`) implemented but E2E testing only on mock server. No real hardware validation of TinyUSB audio streaming.
- **Files**: `src/usb_audio.cpp` (1070 lines), `src/hal/hal_usb_audio.h`, guarded by `-D USB_AUDIO_ENABLED`
- **Impact**: Potential for audio dropout, stuttering, or host enumeration failures in real USB scenarios. Ring buffer (1024 frames, PSRAM) allocation strategy untested at sustained load.
- **Mitigation**: 1070 lines of code with mocks + unit tests. Compilation guard prevents regressing non-USB builds.
- **Recommendation**: Add hardware E2E test: enumerate device on Linux host, stream 48kHz/16-bit for 10s, verify no buffer underrun diagnostics emitted.

**DSP Filter Coefficient Stability Not Exhaustively Tested:**
- **Problem**: RBJ audio EQ cookbook coefficient generation (`src/dsp_biquad_gen.h/.c`) used to generate biquad filters. Boundary cases (very high Q, extreme gains, Nyquist) not tested across all audio rates (48k-768k).
- **Files**: `src/dsp_biquad_gen.h/.c`, `src/dsp_coefficients.h`
- **Impact**: Extreme filter settings (Q>100, gain±24dB, freq approaching Nyquist) may produce NaN/Inf coefficients, causing audio glitch. Rare in practice (UI constrains Q to 0.1-10, gain to ±12dB).
- **Mitigation**: Coefficient import validates against NaN (skips failed stages with LOG_W). Config presets curated by audio experts.
- **Recommendation**: Add numeric stability unit tests: generate coefficients for 10k random param combos, check all finite. Add saturation test: apply extreme filter, verify no NaN in output.

**HAL Multi-Instance Edge Cases:**
- **Problem**: Dual mezzanine (ADC + DAC simultaneously) + onboard devices + optional signal gen + optional USB audio = complex state space (24 device slots, 56 GPIO pins tracked). Not exhaustively tested.
- **Files**: `src/hal/hal_device_manager.cpp`, `src/hal/hal_pipeline_bridge.cpp`, `src/hal/hal_discovery.cpp`
- **Impact**: Pin conflict or slot overflow under rare device combinations possible. Example: user loads ADC mezzanine, enables USB audio, enables signal gen, then DAC mezzanine — may exceed pin capacity.
- **Mitigation**: `static_assert` on dimension bounds. `registerDevice()` returns -1 on slot overflow (handled by callers). Pin tracking prevents GPIO conflicts.
- **Recommendation**: Add integration test: iterate all (9 ADCs) × (12 DACs) × (USB audio on/off) × (signal gen on/off) — verify no pin conflicts, all slots filled correctly.

---

### Edge Cases & Fragility

**Floating-Point Audio Peak Detection:**
- **Problem**: VU meter peak detection uses floating-point max accumulation (`float peak = 0.0f`). If peak value persists >5s without decaying, no exponential decay applied — peak "sticks" until reset manually.
- **Files**: `src/i2s_audio.cpp` (FFT/analysis section), `src/output_dsp.cpp` (limiter peak)
- **Impact**: VU meter shows stale peak forever. User may think audio is still clipping when signal stopped.
- **Mitigation**: Web UI resets peak on user request (button click).
- **Recommendation**: Implement auto-decay: if peak unchanged for 3s, begin exponential decay at 20dB/s. Reset on new peak detected.

**DSP CPU Load Calculation Unsigned Overflow:**
- **Problem**: DSP processing time measured via `esp_timer_get_time()` (microseconds, uint64_t). CPU load computed as `(procTime / frameTime) * 100`. If procTime > frameTime, load > 100%. No saturation applied.
- **Files**: `src/dsp_pipeline.cpp` (CPU metrics), `src/config.h` (lines 94-95: DSP_CPU_WARN/CRIT_PERCENT)
- **Impact**: Load displayed as >100% in UI if DSP overloaded. User confused, but not a crash.
- **Mitigation**: Thresholds at 80% (warn) / 95% (critical). At critical, DSP stages refused, load capped <95%.
- **Recommendation**: Clamp CPU load display to 100% max in UI. Add warning if load > 95% sustained for >5s.

**PSRAM Allocation Failure Silent in Some Paths:**
- **Problem**: `psram_alloc()` returns nullptr on failure. Some callers check, some log warnings but continue. Example: delay line allocation in DSP — returns nullptr, stage setup skips delay (logs warning), proceeds anyway.
- **Files**: `src/psram_alloc.cpp`, `src/dsp_pipeline.cpp` (line 225-226)
- **Impact**: User expects delay stage but gets no delay (silent feature loss). No error reported in API response.
- **Mitigation**: Callers check return value, log warnings, return false from `add_stage()`. API returns 400 Bad Request if stage fails.
- **Recommendation**: Add failure reason codes to API response. Example: `{"success": false, "reason": "PSRAM_ALLOC_FAIL", "details": "Delay line 19.2KB"}`.

**Task WDT Reconfiguration Dependency:**
- **Problem**: Task Watchdog Timer (TWDT) must be reconfigured early in `setup()` via `esp_task_wdt_reconfigure(30)` (30s timeout). If boot sequence exceeds 30s (unlikely but possible during OTA recovery), TWDT fires before app ready.
- **Files**: `src/main.cpp` (setup section), `CLAUDE.md` (Arduino ESP32 3.x compatibility)
- **Impact**: Unintended MCU reboot during slow boot (e.g., NVS recovery, filesystem mount).
- **Mitigation**: TWDT reconfigure called early before any heavy init. FreeRTOS tasks explicitly subscribed via `esp_task_wdt_add()`.
- **Recommendation**: Add telemetry: log TWDT triggers with context (which task, cumulative stack usage). Consider increasing to 45s if slow storage boot observed.

---

### Known Limitations (By Design)

**HAL Pin Capacity at Saturation:**
- **Constraint**: ESP32-P4 has 55 usable GPIO pins (0-54, total 56 tracked). Current onboard + expansion usage: 40+ pins (I2S, I2C, SPI, control lines, LEDs, relays, encoders). Adding 2 more expansion connectors would exceed capacity.
- **Files**: `src/hal/hal_device_manager.cpp` (pin claim), `src/config.h`, `CLAUDE.md`
- **Scope**: 14 onboard slots at boot + 2 expansion slots (ADC+DAC) = 16/32 device slots free. Pin tracking: 56/56 pins available, max 40 claimed.
- **Recommendation**: Plan for single DAC mezzanine OR single ADC mezzanine per device, not both simultaneously on standard kit. Document pin map clearly.

**Audio Pipeline Matrix 32×32 Fixed Size:**
- **Constraint**: Routing matrix is compile-time `float _matrixGain[32][32]` (4KB static). Supports up to 16 sink slots (stereo = 32 output channels) and 8 input lanes (stereo = 16 input channels).
- **Files**: `src/audio_pipeline.cpp` (line 80), `src/config.h` (line 245)
- **Scope**: Expansion DACs register up to 4 sink slots (e.g., ES9038PRO = 4 stereo pairs = 8 channels). With 2 such DACs, matrix at capacity. Cannot add 3rd 8ch DAC without recompile.
- **Recommendation**: If 3+ 8ch DACs needed, recompile with `AUDIO_OUT_MAX_SINKS=24` (matrix grows to 48×48, uses 9KB instead of 4KB).

**OTA Update Staging Capacity:**
- **Constraint**: OTA download stored in unused app partition (typically ~2MB free after firmware ~1.2MB). No resume on interrupted download.
- **Files**: `src/ota_updater.cpp`, `src/config.h` (lines 234-236: timeouts)
- **Scope**: Firmware updates only. LittleFS config and user presets are separate.
- **Recommendation**: Document timeout expectations (20s connect, 5s per-read). Pre-check free space before download starts.

---

## Recommendations for Future Phases

1. **Implement stack high water mark monitoring** for audio task (see Memory & Allocation section).
2. **Add I2C bus reset logic** to HAL discovery before probe retry (see Hardware & I2C section).
3. **Rate-limit telemetry endpoints** to prevent info disclosure DoS (see Security section).
4. **Add CSRF token to logout** (see Security section).
5. **Integrate hardware E2E tests** for USB audio and DSP filter stability (see Testing & Coverage section).
6. **Document pin map saturation limit** and expansion mezzanine restrictions (see Known Limitations section).

---

*Concerns audit: 2026-03-22*
