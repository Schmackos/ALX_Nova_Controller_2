# Codebase Concerns

**Analysis Date:** 2026-03-21

## Tech Debt

**DEBT-1: DSP Swap Failure Recovery (RESOLVED)**
- Issue: DSP config swap could fail silently, leaving pipeline in broken state
- Files: `src/dsp_pipeline.cpp`, `src/websocket_handler.cpp`
- Status: FIXED in v1.11.0 (42 callsites audited, HTTP 503 on swap failure)
- Impact: Data loss + audio degradation
- Resolution: All swap callers now check return value; REST endpoints return HTTP 503; WS broadcasts failure code

**DEBT-2: dspGetInputLevel/OutputLevel (RESOLVED)**
- Issue: Legacy stubs returning 0 instead of actual pipeline RMS values
- Files: `src/dsp_api.cpp`
- Status: FIXED in v1.11.0 (rewired to audio_pipeline)
- Impact: UI VU meters showed zero amplitude
- Resolution: Now reads from `audio_pipeline_get_analysis()` with proper lane/slot mapping

**DEBT-3: WebSocket Broadcast Rate Under Heap Pressure (MITIGATED)**
- Issue: Binary waveform/spectrum frames sent at full 20 Hz even when SRAM heap critical
- Files: `src/websocket_handler.cpp`, `src/audio_pipeline.cpp`
- Status: GRADUATED HEAP MONITORING (2026-03-21, commit `5876c20`)
- Impact: WiFi RX buffer starvation at heapCritical (<40KB)
- Resolution: WS binary rate halved at heapWarning (50KB), suppressed at heapCritical. `EVT_HEAP_PRESSURE` signals state transitions. `DIAG_SYS_HEAP_WARNING` emitted on transition

**DEBT-4: AppState Monolith (RESOLVED)**
- Issue: 553-line AppState made navigation and testing difficult
- Files: `src/app_state.h`, `src/state/*.h`
- Status: DECOMPOSED (commit `7d0f072`, 2026-03-09)
- Impact: Code clarity, testability, cross-domain concern visibility
- Resolution: 15 domain-specific state headers in `src/state/`. Usage: `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`. AppState shell retained for dirty flags + event signaling

**DEBT-5: Legacy Static DAC Patterns (RESOLVED)**
- Issue: 7 deprecated static device-type-specific WS handlers, hardcoded device IDs
- Files: `src/websocket_handler.cpp`, `src/dac_api.cpp`, `src/dac_hal.cpp`
- Status: ALL 7 PHASES COMPLETE (commits across 2026-03-08 to 2026-03-09)
- Impact: Code duplication, fragility on multi-DAC systems
- Resolution: Single generic `requestDeviceToggle(halSlot, action)` path via `HalCoordState`. All device state now routed through HAL

**DEBT-6: Registry Unification & Bridge Sink Ownership (RESOLVED)**
- Issue: Circular dependencies, multiple sink lifecycle owners, rigid bus-specific init
- Files: `src/hal/hal_pipeline_bridge.cpp`, `src/dac_hal.cpp`
- Status: ALL 3 PHASES COMPLETE (commits 2026-03-08 to 2026-03-10)
- Impact: Tight coupling, fragile cross-module testing, multi-device limitations
- Resolution: Bridge is sole sink lifecycle owner. Generic `buildSink()` virtual on `HalAudioDevice`. `dac_hal.cpp` now bus-utility only (I2S TX, volume curves)

## Known Bugs

**I2C Bus 0 SDIO Conflict (FIXED)**
- Symptoms: MCU reset when I2C Bus 0 (GPIO 48/54) scanned with WiFi active
- Files: `src/hal/hal_discovery.cpp`, `src/wifi_manager.cpp`
- Trigger: Rescan button while WiFi connected, or bus scan before WiFi disconnects
- Fix: `hal_wifi_sdio_active()` checks `connectSuccess || connecting || activeInterface == NET_WIFI`. `wifi_manager.cpp` sets `activeInterface = NET_WIFI` on connect, clears on disconnect. Scan API returns `partialScan` flag. `DIAG_HAL_I2C_BUS_CONFLICT` (0x1101) emitted (commit `a86ee56`, 2026-03-21)

**Matrix Routing Silent Audio Loss (FIXED)**
- Symptoms: Lanes 4-7 audio silently discarded; hardcoded loop `for (i=0; i<4; i++) inCh[i]` ignored lanes 4-7
- Files: `src/audio_pipeline.cpp`, `src/audio_pipeline.h`
- Trigger: Any audio on lanes 4-7 (extended ADC count)
- Fix: Loop-based `inCh[]` population, `static_assert` dimension invariants, runtime `set_sink()` validation (commit `04ff496`, 2026-03-21, 24 new tests)

**HAL Slot Capacity Exhaustion (DETECTED)**
- Symptoms: New HAL devices silently fail to register if table full (14/24 at boot)
- Files: `src/hal/hal_device_manager.cpp`
- Status: TABLE SIZE OK (24 slots = 14 builtin + 10 spare for EEPROM/custom devices)
- Safeguard: `claimPin()` logs on table-full; `registerDevice()` returns false; tests added (commit `eb4d8b3`)

**HAL Toggle Queue Overflow (FIXED)**
- Symptoms: Concurrent device enable/disable requests silently dropped (queue capacity 8)
- Files: `src/state/hal_coord_state.h`, `src/state/hal_coord_state.cpp`
- Trigger: >8 simultaneous unique slot toggles in one drain cycle (5ms), same-slot requests coalescence
- Fix: Overflow telemetry (`_overflowCount` lifetime + `_overflowFlag` one-shot), `DIAG_HAL_TOGGLE_OVERFLOW` (0x100E), HTTP 503 on REST failure, LOG_W on internal failure (commit `3f77f6e`, 2026-03-21)

**I2S Pin HAL Config Ignored (FIXED)**
- Symptoms: `HalDeviceConfig.pinMclk/pinBck/pinLrc` persisted but ignored at runtime
- Files: `src/i2s_audio.cpp`, `src/hal/hal_i2s_bridge.cpp`
- Trigger: Changing I2S GPIO via `/hal_config.json`, then reboot or sample-rate change
- Fix: `_cachedAdcCfg[2]` statics + `_resolveI2sPin()` helper, applied at 4 callsites (commit `5c548e3`, 2026-03-10, 8 tests)

## Security Considerations

**WebSocket Authentication Token (IMPLEMENTED)**
- Risk: WebSocket clients could intercept auth state without valid session
- Files: `src/websocket_handler.cpp`, `src/auth_handler.cpp`, `web_src/01-core.js`
- Current mitigation: Short-lived token (60s TTL) from `GET /api/ws-token`, 16-slot pool, revoked on session expire
- Recommendations: Already PBKDF2-SHA256 (10k iterations) hardened; consider rotating token pool periodically

**WiFi Credentials in NVS (ACCEPTED RISK)**
- Risk: WiFi SSID/password stored in ESP32 NVS (encrypted by design, not user-accessible)
- Files: `src/wifi_manager.cpp`, `src/settings_manager.cpp`
- Mitigation: NVS encryption enabled in firmware config; credentials survive LittleFS format via native NVS
- Recommendation: Document in user guide that physical device access = credential compromise

**Web Password PBKDF2 (IMPLEMENTED)**
- Risk: Weak hashing or low iteration count
- Files: `src/auth_handler.cpp`
- Current: 10k iterations (2026-03-10 fix), HttpOnly cookie, rate limiting (HTTP 429)
- Recommendations: Already hardened; consider 2FA for critical operations (deferred to future sprint)

**API Rate Limiting (PARTIAL)**
- Risk: Brute force / DDoS on `/api/login` and HAL scan endpoints
- Files: `src/auth_handler.cpp`, `src/hal/hal_api.cpp`
- Current: `/api/login` returns HTTP 429 after 5 failures; scan has 409 guard (`_halScanInProgress`)
- Gap: No global rate limiting on other endpoints (e.g., `/api/dsp/config`, `/api/settings`)
- Recommendation: Implement per-IP request throttling middleware (deferred)

**EEPROM Write Endurance (MONITORED)**
- Risk: EEPROM cell lifetime (100k typical writes), repeated settings saves
- Files: `src/settings_manager.cpp`, `src/hal/hal_settings.cpp`
- Current: Debounced saves (2s for matrix, 10s for settings), LittleFS + NVS multi-tier
- Monitoring: `eepromDiag` tracks write cycles; health dashboard shows wear level
- Recommendation: Consider wear-leveling firmware (IDF5 native support available)

## Performance Bottlenecks

**Heap Pressure at 40KB Internal SRAM (MONITORED)**
- Problem: WiFi RX buffers dynamically allocated; below 40KB free, incoming packets silently dropped
- Files: `src/config.h`, `src/heap_monitor.cpp`, `src/audio_pipeline.cpp`
- Cause: DSP delay lines (77KB estimated PSRAM) + audio pipeline (66KB) + DMA (32KB) = ~175KB used
- Monitoring: `heapWarning` at 50KB, `heapCritical` at 40KB, WS binary rate halved/suppressed
- Improvement: Already using PSRAM for large buffers; heap budget tracker enables per-subsystem analysis

**WebSocket Broadcast Overhead (MITIGATED)**
- Problem: JSON state broadcast on every dirty flag transition could starve Core 0
- Files: `src/websocket_handler.cpp`
- Cause: No aggregation, per-field publish spikes
- Current: `wsAnyClientAuthenticated()` guard skips broadcasts if no WS clients connected; binary waveform rate limited at heap pressure
- Recommendation: Consider 50ms aggregation window for non-critical fields (deferred, profiling needed)

**Matrix Routing O(N²) Complexity (ACCEPTABLE)**
- Problem: 16×16 matrix = 256 gain lookups per sample frame
- Files: `src/audio_pipeline.cpp`, `pipeline_mix_matrix()`
- Impact: ~1% CPU on Core 1 (audio task runs at lower priority, preemptible)
- Safeguard: Matrix bypass flag `_matrixBypass` disables mixing when not needed
- Recommendation: Already sparse (most bins zero-gain); further optimization deferred

**FFT Computation (ACCEPTABLE)**
- Problem: Radix-4 FFT on 1024 samples at 48kHz = ~46ms per frame
- Files: `src/audio_pipeline.cpp`, `audio_pipeline_analyze_fft()`
- Mitigation: Runs async in audio task (doesn't block I2S ISR); computes every 2 frames (21ms effective)
- Impact: <3% CPU with ESP-DSP assembly optimization
- Recommendation: Acceptable for real-time spectrum visualization

## Fragile Areas

**Audio Pipeline Buffer Allocation (CRITICAL)**
- Files: `src/audio_pipeline.cpp`, `audio_pipeline_init()`
- Why fragile: 6 conditional buffer allocations (rawBuf, laneL/R, outCh, gatePrev, swapHold) with silent failure on heap exhaustion. No rollback if later allocation fails.
- Safe modification: Check `heapCritical` before entering `audio_pipeline_init()`; allocate in order; if any fails, free prior and return error
- Test coverage: `test_pipeline_bounds` validates dimension invariants; heap allocation tests (`test_heap_monitor`) confirm thresholds
- Improvement: Pool-based pre-allocation (deferrable, performance negligible)

**I2S Dual-Master Configuration (CRITICAL)**
- Files: `src/i2s_audio.cpp`, `i2s_configure_adc1()`, `i2s_configure_adc2()`
- Why fragile: Both PCM1808 ADCs share BCK/LRC/MCLK clocks; both I2S peripherals master mode required. Init order ADC2→ADC1 is load-bearing. Any sample-rate change must update BOTH channels with identical divider or frequency drift occurs
- Safe modification: Never reorder init; always call both configure functions together; cache dividers in statics (`_cachedAdcCfg`)
- Test coverage: `test_i2s_audio` validates both ADC presence; `test_i2s_config_cache` confirms pin resolution
- Gotcha: MCLK continuity required for PCM1808 PLL; never call `i2s_configure_adc1()` in audio task loop

**HAL Device Lifecycle State Machine (MODERATE)**
- Files: `src/hal/hal_device_manager.cpp`, HAL device classes
- Why fragile: 7-state lifecycle (UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ⇄ UNAVAILABLE → ERROR/REMOVED/MANUAL) with `volatile _ready` flag for lock-free audio task reads. Transient policy differs per state (UNAVAILABLE = auto-recovery; ERROR = manual intervention)
- Safe modification: State transitions fire `HalStateChangeCb`; bridge owns sink removal. Test all transitions in `test_hal_integration`
- Test coverage: `test_hal_device_db`, `test_hal_integration` (24 tests total, 10 integration tests)

**DSP Configuration Swap (MODERATE)**
- Files: `src/dsp_pipeline.cpp`, `dsp_pipeline_swap_staged_config()`
- Why fragile: Atomic swap of double-buffered DSP config; if swap fails, audio path broken. All 42 callers must check return value
- Safe modification: Always call `dsp_pipeline_swap_staged_config()` and handle failure (HTTP 503, LOG_W)
- Test coverage: `test_dsp_swap` validates swap success/failure paths; `test_dsp` confirms stage add/remove
- Monitoring: `DIAG_DSP_SWAP_FAIL` (0x0301) emitted on swap failure

**MQTT Task Independence (MODERATE)**
- Files: `src/mqtt_task.cpp`, `mqtt_task()` FreeRTOS task on Core 0
- Why fragile: Main loop no longer calls `mqttLoop()` or `publishMqtt*()` — all MQTT work offloaded to dedicated task polling at 20 Hz. Broker reconfiguration via `_mqttReconfigPending` volatile. Dirty flags flow from main loop to MQTT task via AppState
- Safe modification: Always set dirty flags in main loop handlers; MQTT task drains them. Never call MQTT functions from HTTP handlers (they already set flags)
- Test coverage: `test_mqtt` validates queue behavior; `test_mqtt_ha_discovery` confirms HA message format
- Monitoring: MQTT connection state in `appState.mqtt`; publish errors logged

**Web Frontend Concatenation (MODERATE)**
- Files: `web_src/js/*.js`, `.eslintrc.json`
- Why fragile: All JS files concatenated in load order into single `<script>` block. No module isolation. `node tools/find_dups.js` enforces no duplicate variable declarations
- Safe modification: Use `node tools/find_dups.js` before git commit; add new globals to `.eslintrc.json` if adding top-level declarations
- Test coverage: Pre-commit hook runs find_dups; E2E tests verify DOM updates match WS messages
- Gotcha: No bundler/minifier — all variable names exposed in DevTools

**DNS Resolution Race (LOW RISK)**
- Files: `src/mqtt_handler.cpp`, `esp_gethostbyname()` on WiFi task
- Why fragile: DNS resolution happens on WiFi task (Core 0); if DNS request interrupts MQTT task, possible stale hostname cache if WiFi reconnects
- Safe modification: MQTT task caches resolved IP; invalidate on broker string change (done via `_mqttReconfigPending`)
- Monitoring: Connection errors logged; diagnostic dashboard shows MQTT broker address

## Scaling Limits

**HAL Device Slots: 24 (ADEQUATE)**
- Current capacity: 14/24 slots at boot (PCM5102A, ES8311×2, PCM1808×2, NS4150B, TempSensor, SigGen, USB Audio, MCP4725, onboard devices)
- Limit: 24 devices max (`HAL_MAX_DEVICES`)
- Scaling path: Increase `HAL_MAX_DEVICES` in `src/hal/hal_types.h`; RAM impact ~50 bytes per slot
- Recommendation: Current limit sufficient for 2-3 expansion boards; future multiboard requires protocol changes

**I2S DMA Buffer Count: 4 (ADEQUATE)**
- Current capacity: `I2S_DMA_BUF_COUNT=4` buffers × `I2S_DMA_BUF_LEN=1024` samples = 4KB @ 48kHz = 21.3ms latency
- Limit: ESP32-P4 I2S DMA descriptor pool; 4 is typical max without underrun risk
- Scaling path: Not recommended — increasing DMA buffers increases audio latency. Keep at 4
- Recommendation: Adequate for 5.1 surround; monitor underrun counter via `appState.dac.txUnderruns`

**Audio Pipeline Lanes: 8 (ADEQUATE)**
- Current capacity: `AUDIO_PIPELINE_MAX_INPUTS=8` lanes (16×16 matrix)
- Limit: 8 ADC sources (USB Audio = 2 lanes, PCM1808×2 = 4 lanes, SigGen = 1 lane, 1 spare)
- Scaling path: Increase `AUDIO_PIPELINE_MAX_INPUTS` in `audio_pipeline.h`; float buffer cost ~8×2KB PSRAM per lane
- Recommendation: 8 is practical limit without additional I2S bus; multiboard requires µPD expansion connector

**Audio Pipeline Output Sinks: 8 (ADEQUATE)**
- Current capacity: `AUDIO_OUT_MAX_SINKS=8` sink slots
- Limit: 8 DAC outputs (PCM5102A stereo = 2 channels = 1 slot, ES8311 stereo = 2 channels = 1 slot, 6 spare slots)
- Scaling path: Increase `AUDIO_OUT_MAX_SINKS` in `audio_pipeline.h`; allocates sinkBuf + output DSP per slot
- Recommendation: Current usage 2/8; capacity sufficient for 4 stereo DACs

**Matrix Routing Channels: 16×16 (ADEQUATE)**
- Current capacity: 16 channels (8 input lanes × 2ch + 8 output channels)
- Limit: 16×16 = 256 gain values; O(N²) complexity with 1% CPU impact
- Scaling path: Not recommended — matrix is already sparse; current limit sufficient
- Recommendation: For >8 inputs, upgrade to µPD expansion bus with dedicated mixer

**EEPROM Wear Cycles: ~100k (MONITORED)**
- Current usage: LittleFS wear-leveling on top of NVS; matrix saved every 2s if dirty (max 60 saves/min), settings saved every 10s
- Limit: EEPROM physical cell lifetime ~100k cycles; assuming 50 saves/hour = ~2.3 years before wear
- Monitoring: `eepromDiag` tracks actual write count; Health Dashboard displays wear percentage
- Mitigation: Debounced saves, multi-tier persistence (NVS + LittleFS), wear-leveling FFS
- Recommendation: Acceptable for consumer device lifetime; future: implement dynamic erase-block rotation

## Dependencies at Risk

**PubSubClient@2.8 (MONITORED)**
- Risk: MQTT 3.1.1 client, last update ~2023; may have undiscovered TLS/protocol vulnerabilities
- Impact: Loss of MQTT connectivity to Home Assistant, commands silently dropped
- Migration path: Upgrade to `esp32-mqtt` or `mosquitto.h` library (requires API rewrite)
- Status: Works reliably on current codebase; no known CVEs for v2.8
- Recommendation: Monitor for security patches; test before upgrading (breaking API changes possible)

**ArduinoJson@7.4.2 (STABLE)**
- Risk: JSON parsing complexity on embedded devices; deep nesting could trigger OOM
- Impact: Settings load failure, config corrupted
- Current safeguard: Config files (~5KB) are flat JSON, no deep nesting
- Recommendation: Stable; no known vulnerabilities

**LVGL@9.4 (STABLE)**
- Risk: GUI framework for TFT; memory pressure if too many active objects
- Impact: TFT rendering glitches, display freeze
- Current safeguard: Screens use ~10-20 objects; active object count monitored
- Recommendation: Stable; LVGL 10 available but requires display refactor

**LovyanGFX@1.2.0 (STABLE)**
- Risk: Display driver abstraction; P4-specific ST7735S config
- Impact: Display won't initialize or shows garbage
- Current safeguard: `lgfx_config.h` has working settings (memory_width=128, offset_rotation=2)
- Recommendation: Stable; monitor for ESP32 P4 specific patches

**ESP-DSP (Prebuilt .a)**
- Risk: S3 assembly-optimized binary; if ESP-IDF breaks ABI, library incompatible
- Impact: Silent math corruption (FFT, biquad)
- Current safeguard: Native tests use ANSI C fallback (`lib/esp_dsp_lite/`); ESP32 tests validate output correctness
- Monitoring: `test_esp_dsp` compares output against known-good reference
- Recommendation: Monitor Espressif releases; test on major IDF updates

## Missing Critical Features

**AP Mode WiFi Credentials (DEFERRED)**
- Problem: AP mode password hardcoded; no way to set custom AP password via web UI
- Blocks: Deployment in production (security risk)
- Plan: `POST /api/settings/ap-password` endpoint + storage in NVS
- Priority: MEDIUM — workaround: use WiFi scanner + AP SSID (predictable MAC-based)

**TLS Server Certificate Management (DEFERRED)**
- Problem: Web server over HTTP only; no HTTPS support
- Blocks: Secure configuration over untrusted networks
- Plan: TinyHTTPD TLS support (requires cert provisioning), ESP32-P4 has hardware acceleration
- Priority: LOW — workaround: use HTTP within trusted network + VPN

**Archive/Mirror for Firmware Releases (DEFERRED)**
- Problem: OTA updates pull from GitHub directly; no failover if GitHub unreachable
- Blocks: Device updates during GitHub outages
- Plan: Add HTTP mirror endpoint configuration + fallback in OTA updater
- Priority: LOW — workaround: pre-load firmware on internal storage

**Real-Time Audio Latency Metrics (DEFERRED)**
- Problem: No visibility into I2S DMA underruns, buffer latency variance
- Blocks: Audio quality tuning for low-latency applications
- Plan: Add latency histogram + jitter analysis to diagnostics
- Priority: MEDIUM — monitoring: current underrun counter sufficient for basic diagnostics

## Test Coverage Gaps

**HAL Hot Swap (UNTESTED)**
- What's not tested: Live device insertion/removal during audio streaming
- Files: `src/hal/hal_device_manager.cpp`, `src/hal/hal_pipeline_bridge.cpp`
- Risk: Assertion failure or double-free if device removed while sink active
- Safe: Bridge observes state changes via callback; removes sink before device destruction
- Recommendation: Add integration test with device state machine (requires hardware)

**DSP Coefficient Computation Stability (PARTIAL COVERAGE)**
- What's not tested: Edge cases in RBJ biquad generator (Q→0, f→Nyquist, extreme gain)
- Files: `src/dsp_biquad_gen.h`, `dsp_gen_hp/lp/bp/peq()`
- Risk: Unstable coefficients → audio feedback/clipping
- Safeguard: Coefficient range checks in `dsp_api.cpp`; tests validate stability margin
- Recommendation: Add fuzz testing for coefficient generator

**Concurrent DSP Config Load (UNTESTED)**
- What's not tested: Multiple `PUT /api/dsp/config` requests in flight simultaneously
- Files: `src/dsp_api.cpp`, `audio_pipeline.cpp`
- Risk: Race condition in staged config swap; out-of-order apply
- Safeguard: Deferred save (2s debounce) serializes writes; `vTaskSuspendAll()` protects swap
- Recommendation: Add stress test with rapid config changes

**WiFi Reconnection Under Audio Load (MANUAL ONLY)**
- What's not tested: WiFi disconnect/reconnect while audio streaming
- Files: `src/wifi_manager.cpp`, `src/i2s_audio.cpp`
- Risk: Audio underrun due to SDIO bus contention
- Safeguard: HAL discovery skips Bus 0 (SDIO conflict) when WiFi active; I2S DMA continues independently
- Recommendation: Add hardware integration test (can't simulate SDIO in unit tests)

**MQTT Reconnection Backoff (PARTIAL COVERAGE)**
- What's not tested: Backoff algorithm under repeated broker failures
- Files: `src/mqtt_task.cpp`, `mqtt_handler.cpp`
- Risk: Exponential backoff overshoot; socket exhaustion from rapid reconnects
- Safeguard: Configurable max backoff (30s default); FreeRTOS task watchdog prevents deadlock
- Recommendation: Add simulation tests for broker failure scenarios

---

*Concerns audit: 2026-03-21*
