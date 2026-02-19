# Codebase Concerns

**Analysis Date:** 2025-02-19

## Test Coverage Gaps

**DSP Swap & Emergency Limiter:**
- Issue: 12 pre-existing test failures in `test/test_dsp_swap/` and `test/test_emergency_limiter/`
- Files: `test/test_dsp_swap/test_dsp_swap.cpp`, `test/test_emergency_limiter/test_emergency_limiter.cpp`
- Impact: DSP config hot-swap (glitch-free buffer exchanges) and emergency limiting under extreme clipping may not behave correctly. Risk of audio artifacts when applying new DSP presets dynamically
- Fix approach: Investigate FreeRTOS synchronization issues in `_swapMutex` (line 48 in `src/dsp_pipeline.cpp`), possibly related to timeouts in native test environment. May need FreeRTOS stub improvements for native tests or review of swap timeout logic
- Status: Known, documented in project memory as "12 pre-existing failures in native tests"

## Memory Fragmentation Risk

**Heap Fragmentation Without PSRAM Redundancy:**
- Issue: WiFi RX buffers are dynamically allocated from internal SRAM heap. If largest free block < 40KB, incoming packets are silently dropped
- Files: `src/main.cpp` (lines 963-978), `src/config.h` (40KB reserve threshold)
- Impact: Device becomes unreachable over WiFi (HTTP/WebSocket/MQTT RX fail) while still able to transmit. Loss of remote control and monitoring. Silent degradation with no user indication until reboot
- Current mitigation: `heapCritical` flag (checked every 30s), broadcast to WebSocket/MQTT, but only when critical threshold hit. No active heap compaction
- Fix approach: (1) Pre-allocate fixed-size WiFi RX buffers in PSRAM during init, (2) Implement periodic heap analysis + early warning at 60KB threshold, (3) Add WiFi watchdog that detects consecutive failed RX cycles and triggers graceful pause/resume with backoff
- Severity: High — silent packet drop is undetectable to user without accessing device serial logs

## Serial Logging Performance Bottleneck

**Serial Print Blocks I2S DMA:**
- Issue: `Serial.print()` blocks for milliseconds when UART TX buffer fills (baud rate 115200). Main loop logging in audio hot path starves I2S_DMA
- Files: `src/debug_serial.h/.cpp`, `src/i2s_audio.cpp` (lines 57-58 note), `src/main.cpp` (lines 1009)
- Impact: Audio discontinuities, buffer underruns, waveform glitches during debug output. Visible as clicks/pops in audio when logging enabled
- Current mitigation: Audio task avoids `LOG_*` calls; instead sets `_dumpReady` flag, main loop calls `audio_periodic_dump()` with 2ms max output per iteration
- Remaining issue: Other modules (WiFi, MQTT, settings, OTA) still log from tasks. WebSocket log forwarding must parse `[Module]` prefix and handle bandwidth
- Fix approach: (1) Implement ring buffer for off-ISR serial writes with background flush task, (2) Make all debug logging async via dirty flags + main loop processing, (3) Add per-module rate-limiting
- Severity: Medium — visible audio quality regression when `debugSerialLevel > LOG_ERROR`

## WebSocket Authentication Edge Case

**Session Validation Not Atomic:**
- Issue: `validateSession()` is called per-message without per-client session lock. Client could be authenticated, then timeout/disconnect, then old cached sessionId accepted on reconnect
- Files: `src/websocket_handler.cpp` (lines 127-135), `src/auth_handler.h/.cpp` (session storage)
- Impact: Brief window for session hijacking if attacker knows sessionId from earlier connection. Device password is still required, but replay risk exists
- Current mitigation: Timeout set to 5 seconds (line 105). Session cleared on disconnect (line 94). Token rotated on each login attempt
- Fix approach: (1) Add per-sessionId timestamp + invalidate after 1 minute, (2) Bind sessionId to client IP (wsAuthStatus per-client), (3) Use nonce in auth handshake
- Severity: Low — 5s window, requires network packet capture or rapid reconnection; device password still required

## Dual I2S Clock Synchronization Fragility

**Both I2S ADCs as Master RX (No Slave Mode):**
- Issue: ESP32-S3 I2S slave mode DMA broken (bclk_div hardcoded to 4, below hardware minimum of 8). Both ADCs configured as master with frequency-locked clocks
- Files: `src/i2s_audio.cpp` (I2S init order), `CLAUDE.md` (lines 107-117 explain design)
- Impact: If MCLK or BCK jitter occurs, ADC2 (on I2S1 without clock output) could desynchronize with ADC1. Audio timing differences between channels → stereo phase misalignment
- Current mitigation: Both use same 160MHz D2CLK divider chain. DOUT2 on GPIO 9 with `INPUT_PULLDOWN` so missing data reads as zeros (NO_DATA) not floating noise
- Risk: No hardware synchronization; relies on matched software clock dividers. Clock slippage undetectable until audio quality analysis
- Fix approach: (1) Add SNR/SFDR periodic analysis to detect phase shifts, (2) Implement adaptive sync check (measure zero-cross timing between channels), (3) Consider I2C sync pulse from master ADC if available
- Severity: Medium — rare, only if hardware clock drift exceeds divider precision; audio diagnostics partially detect via health status

## DSP Delay Line Heap Allocation Policy

**PSRAM Fallback Without Pre-flight Check on Reconfig:**
- Issue: DSP delay lines allocated via `heap_caps_calloc(MALLOC_CAP_SPIRAM)` on first add_stage. If PSRAM full, falls back to internal heap. No check at reconfig time
- Files: `src/dsp_pipeline.cpp` (delay line allocation in `dsp_add_stage()`), `src/config.h` (DSP_MAX_DELAY_SAMPLES=4800, ~19.2KB per line)
- Impact: If PSRAM gets fragmented or filled by other modules, new DSP stages will silently allocate from internal heap, competing with WiFi RX buffers. Creates heap pressure at runtime
- Current mitigation: `heapCritical` flag detects < 40KB free, but this is checked AFTER allocation
- Fix approach: (1) Pre-allocate all DSP delay lines at startup if `DSP_ENABLED`, (2) Implement DSP pre-flight check before importing config (verify PSRAM space), (3) Document max concurrent DSP configs and delay slots
- Severity: Medium — race condition between audio DSP growth and WiFi buffer allocation; can trigger heap critical state unexpectedly

## Web Pages Build Step Fragility

**GZip Rebuild Not Integrated into Build:**
- Issue: After editing `src/web_pages.cpp`, must manually run `node tools/build_web_assets.js` to regenerate `web_pages_gz.cpp` before firmware build. Missing rebuild silently uses stale gzipped HTML
- Files: `src/web_pages.cpp` (11,580 lines), `src/web_pages_gz.cpp` (5,785 lines, auto-generated), `tools/build_web_assets.js`
- Impact: Frontend changes (UI, layout, JavaScript logic) don't take effect on upload. Operator sees old web interface. No build error/warning
- Current mitigation: Documented in `CLAUDE.md` (line 68), but relies on human memory
- Fix approach: (1) Integrate gzip rebuild into PlatformIO `pre:extra_script` hook, (2) Add target hash comparison to avoid unnecessary rebuilds, (3) Fail build if `web_pages_gz.cpp` is stale relative to `web_pages.cpp`
- Severity: High for developer experience — easy to forget, results in deployed firmware with broken web UI. No runtime symptom

## Large Monolithic Web Pages File

**Web Pages Component Complexity:**
- Issue: Single `src/web_pages.cpp` file is 11,580 lines, containing all HTML/CSS/JS/inline JSON. No modular separation
- Files: `src/web_pages.cpp`
- Impact: Difficult to edit, high risk of JavaScript syntax errors that break entire `<script>` block (observed: bulk find-replace that added `appState.` prefix to JS variables broke entire web app). No linting or module testing
- Current mitigation: None documented. Documented gotcha in project memory: "NEVER do bulk find-replace on JS variable names"
- Fix approach: (1) Split into separate HTML/CSS/JS files, use bundler (webpack/vite), (2) Add ESLint + pre-commit hook, (3) Implement web component architecture with scoped styles
- Severity: Medium — high risk during future maintenance; current state requires careful manual editing

## Spinlock Usage in I2S Real-Time Path

**Critical Section Contention in Audio Task:**
- Issue: `portENTER_CRITICAL_ISR()` / `portEXIT_CRITICAL_ISR()` spinlocks in `audio_capture_task` (lines 973-978 in `i2s_audio.cpp`), holding interrupts disabled while writing `_analysis` struct
- Files: `src/i2s_audio.cpp` (lines 973-978 for ISR-level lock), `i2s_audio_get_analysis()` (non-ISR read with `portENTER_CRITICAL()` spinlock)
- Impact: If main loop calls `i2s_audio_get_analysis()` at high frequency, spinlock contention can delay I2S DMA task and cause audio underruns. No timeout on spinlock
- Current mitigation: `i2s_audio_get_analysis()` is called only from WebSocket/MQTT broadcast loops (not per audio frame), so contention rare
- Risk: If new code adds frequent calls to `i2s_audio_get_analysis()` (e.g., per-frame analysis), spinlock overhead could spike
- Fix approach: (1) Use double-buffered volatile struct instead of spinlock (atomic read of struct is safe on 32-bit systems), (2) Profile spinlock hold times, (3) Add comment with frequency constraints
- Severity: Low — current usage pattern safe, but refactoring risk if code reads analysis continuously

## OTA Update Task Blocking on Core 1

**OTA Download on GUI Core:**
- Issue: OTA download task runs on Core 1 (via `startOTADownloadTask()`) which shares the core with GUI task and `audio_capture_task` (priority 3). Large file download (100+ MB) can starve GUI and audio
- Files: `src/ota_updater.cpp` (OTA task priority not documented), `src/main.cpp` (Core 1 audio task setup), `src/gui/gui_manager.cpp`
- Impact: GUI becomes unresponsive during OTA download. Audio may dropout if I2S DMA doesn't get scheduled
- Current mitigation: OTA check task and download task are one-shot (not continuous). Main loop detects completion via dirty flag, no blocking in main loop itself
- Risk: If OTA download takes > 30s (WiFi slow/large file), user sees "frozen" device
- Fix approach: (1) Run OTA download on separate task with priority 1 (below audio), (2) Implement background progress updates to GUI via dirty flags, (3) Add task yield points in HTTP download loop
- Severity: Medium — impacts UX but not safety; device recovers after OTA completes or user force-restarts

## MQTT Message Ordering Risk

**Async MQTT Publishes Without Ordering Guarantee:**
- Issue: MQTT publishes called from multiple FreeRTOS tasks (audio_capture_task, gui_task, OTA tasks) without sequencing. Packets may arrive out-of-order at broker
- Files: `src/mqtt_handler.cpp` (3,847 lines, multiple entry points), all modules that call `mqtt_publish()`
- Impact: Home Assistant sees state updates arrive out-of-order. Example: sensor reports "enabled" then "disabled" due to task scheduling, UI shows disabled even though device enabled
- Current mitigation: Home Assistant subscribes to heartbeat topics and reconciles state. MQTT message filtering in broker
- Risk: Non-critical for Home Assistant (stateless sensors), but impacts monitoring dashboards
- Fix approach: (1) Single MQTT outgoing queue with sequence numbers, (2) Dedicate one task to MQTT transmission, (3) Add HA discovery with `retain=true` on critical state topics
- Severity: Low — Home Assistant recovers via heartbeat; mainly a logging/monitoring visibility issue

## Buzzer Mutex Complexity

**Non-blocking Mutex with Silent Skip:**
- Issue: `buzzer_update()` in main loop uses `xSemaphoreTake(buzzer_mutex, 0)` (non-blocking, line 212 in `src/buzzer_handler.cpp`). If buzzer_handler already running on Core 1, silently skips processing
- Files: `src/buzzer_handler.cpp` (lines 152-267), `src/main.cpp` (calls `buzzer_update()`)
- Impact: If buzzer pattern is triggered while main loop is servicing buzzer, the pending update is lost. Buzzer may not complete pattern
- Current mitigation: Buzzer patterns are short (< 1 second). Non-blocking take prevents main loop stall. But no retry logic
- Risk: Under heavy WiFi/WebSocket load, main loop may skip buzzer updates and pattern gets cut short
- Fix approach: (1) Queue buzzer requests in ring buffer instead of mutex, (2) Handle retry with timeout, (3) Add skipped-update counter to diagnostics
- Severity: Low — cosmetic impact (buzzer sound is incomplete), not critical for device operation

## Heap Critical Threshold Static

**40KB Reserve Hardcoded:**
- Issue: WiFi heap reserve is `40000` bytes hardcoded in `src/main.cpp` (line 969). No configuration for different deployment scenarios
- Files: `src/main.cpp` (line 969), `src/config.h` (may have related defines)
- Impact: On device with large PSRAM but small internal SRAM, or in memory-constrained environments, 40KB may be too conservative or too permissive
- Current mitigation: Documented in `CLAUDE.md` (line 91) and project memory
- Fix approach: (1) Move to `src/config.h` as `HEAP_CRITICAL_THRESHOLD_BYTES`, (2) Add MQTT sensor to report threshold and allow runtime override, (3) Implement dynamic scaling based on WiFi buffer pool size
- Severity: Low — threshold is conservative and generally works; mainly a configurability issue

## Test Framework Limitations for Real-time

**Native Tests Cannot Simulate I2S/Audio DMA Timing:**
- Issue: Tests run on native platform (x86/ARM Linux) with Unix scheduler, not FreeRTOS. I2S DMA timing, interrupt jitter, and real-time constraints are not simulated
- Files: `test/test_dsp_swap/test_dsp_swap.cpp`, `test/test_i2s_audio/test_i2s_audio.cpp`, all real-time tests
- Impact: DSP swap deadlock, audio buffer underrun, and task starvation bugs may not appear in native tests but appear on real ESP32-S3 hardware. Example: Test 2 in `test_dsp_swap.cpp` (line 49) is marked as "difficult to implement" and skipped
- Current mitigation: Hardware integration tests (manual firmware testing). CI/CD runs native tests for regression, not coverage
- Risk: New code touching FreeRTOS, mutexes, or I2S driver may silently pass native tests but fail in production
- Fix approach: (1) Use QEMU or ESP32 simulator for timing-sensitive tests, (2) Implement FreeRTOS scheduler stub with configurable task switching, (3) Add hardware CI step (flash real device, automated serial validation)
- Severity: Medium — known limitation, mitigated by manual testing, but increases test blind spots

## Security: Default Web Password

**AP Mode Password Not Unique Per Device:**
- Issue: Access Point password defaults to `ALX-XXXXXXXXXXXX` (derived from MAC but simplified). Web interface also uses this default if not configured
- Files: `src/config.h` (DEFAULT_AP_PASSWORD), `src/app_state.h` (line 106, apPassword), `src/auth_handler.h/.cpp`
- Impact: All devices with default config have predictable AP password. If user doesn't change password, device is accessible to anyone on network
- Current mitigation: Documentation warns to change password. Web UI requires password for sensitive operations (factory reset, etc.)
- Risk: Non-technical users may skip password configuration. Multi-device deployments (e.g., commercial install) with default passwords
- Fix approach: (1) Generate random password at first boot, display in QR code or serial output, (2) Require password change before web UI accessible, (3) Add firmware hash validation in OTA to prevent downgrade to default-password version
- Severity: Medium — default password is weak security posture, but device is not Internet-facing (local network only)

## Task Stack Size Assumptions

**Audio Task Stack Not Validated at Startup:**
- Issue: `TASK_STACK_SIZE_AUDIO = 12288` bytes hardcoded (bumped for dual ADC + FFT + DSP). If FFT or DSP algorithms grow, overflow risk increases silently
- Files: `src/config.h` (TASK_STACK_SIZE_AUDIO), `src/i2s_audio.cpp` (audio task creation), `src/dsp_pipeline.cpp` (large local structs)
- Impact: Buffer overflow in audio task leads to memory corruption, unpredictable crashes, or security vulnerability if exploited
- Current mitigation: FreeRTOS stack watermark monitoring (Task Monitor reports `audioTaskStackFree`). Manual inspection of local variable sizes
- Risk: If DSP stack frame grows (e.g., adding new biquad stages with local buffers), overflow goes undetected until runtime crash
- Fix approach: (1) Add stack overflow detection at task init (fill pattern, validate on every iteration), (2) Implement stack usage analysis in automated tests, (3) Configure FreeRTOS stack overflow hook, (4) Document stack budget per algorithm (FFT = X bytes, DSP = Y bytes)
- Severity: Medium — potential memory safety issue, but Task Monitor provides visibility

## Legacy AppState Macro Pattern

**Inconsistent State Access (Macros vs Direct):**
- Issue: Code uses both `#define wifiSSID appState.wifiSSID` (legacy macros) and `appState.memberName` (new code). Macros hide dependency on AppState singleton
- Files: `src/config.h` (legacy macros), `src/app_state.h` (singleton), throughout codebase
- Impact: Confusion during debugging (macro names don't match code). Harder to search for state dependencies. Risk of circular include if new macro added carelessly
- Current mitigation: Documented in `CLAUDE.md` (line 42): "Legacy code uses macros, new code should use direct access"
- Risk: New developers may add more legacy macros instead of using direct access, perpetuating inconsistency
- Fix approach: (1) Grep codebase for all legacy macro definitions, create deprecation list, (2) Gradually replace with direct `appState.` access, (3) Lint rule to flag new macro definitions in config.h
- Severity: Low — maintainability issue, not a functional bug

## WiFi AP Mode Generation

**AP SSID Uses MAC Address:**
- Issue: AP SSID hardcoded as `ALX-XXXXXXXXXXXX` where X = hex digits from MAC. On first boot, SSID is predictable based on board revision
- Files: `src/config.h`, `src/settings_manager.cpp` (AP SSID initialization)
- Impact: Weak device identification for users with multiple devices. All devices with same board revision have colliding SSID prefixes
- Current mitigation: Full MAC address used, so still unique per device
- Risk: User confusion if deploying multiple devices in same location (all show similar SSID, different suffixes)
- Fix approach: (1) Add device name field (user-configurable, persisted), (2) Show device name in AP SSID, (3) Display name in boot animation
- Severity: Low — cosmetic/UX issue, not a functional or security problem

## Dynamic Audio Update Rate Risk

**Audio Update Rate Affects Smoothing Constants:**
- Issue: `detectSignal()` rate matches `appState.audioUpdateRate` (user-configurable, see `CLAUDE.md` line 49). Smoothing alpha scaled dynamically. If user sets rate very high (e.g., 10ms), alpha becomes very small, signal detection loses responsiveness
- Files: `src/smart_sensing.cpp` (detectSignal), `src/app_state.h` (audioUpdateRate)
- Impact: User-configurable audio update rate can break signal detection time constant (~308ms target). Fast rates → sluggish sensing, slow rates → laggy audio display
- Current mitigation: Rate bounded by default (likely 100-200ms range). Smoothing alpha clamped in code
- Risk: No validation in UI that prevents setting rate too high or too low
- Fix approach: (1) Add min/max bounds in REST API and WebSocket validation (e.g., 50ms-500ms), (2) Implement adaptive alpha with clamps, (3) Add warning if time constant drifts > 10% from 308ms
- Severity: Low — operator error issue, fixable via UI validation

---

## Summary Table

| Category | Count | Severity |
|----------|-------|----------|
| Test coverage gaps | 1 | High |
| Memory safety | 2 | High/Medium |
| Real-time performance | 2 | Medium |
| Serial logging | 1 | Medium |
| Security/authentication | 1 | Low |
| Configuration/API | 2 | Low |
| Hardware/clock sync | 1 | Medium |
| Maintainability | 2 | Low |
| **Total** | **12** | — |

## Recommended Priority Actions

1. **Investigate DSP swap test failures** — 12 failing tests indicate potential audio glitch issue
2. **Implement pre-flight heap checks for DSP** — prevent silent WiFi RX buffer starvation
3. **Integrate web pages gzip build step** — avoid silent frontend breakage
4. **Validate audio update rate bounds** — ensure signal detection time constant stays stable
5. **Add FreeRTOS stack overflow detection** — catch buffer overflows early

*Concerns audit: 2025-02-19*
