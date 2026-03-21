# Codebase Concerns

**Analysis Date:** 2026-03-21

## Architecture & Design Issues

### ~~High Memory Pressure — Audio Pipeline Allocation Strategy~~ — FIXED

**Status:** FIXED (March 2026). Actual worst-case footprint corrected from ~512KB to ~330KB PSRAM + 32KB internal SRAM.

**Resolution:** All 4 improvement items implemented:
1. **Heap saturation tracing**: Every heap state transition (normal/warning/critical) emits `diag_emit()` with `maxBlock`, `freeHeap`, `freePsram` values. PSRAM→SRAM fallback emits `DIAG_SYS_PSRAM_ALLOC_FAIL`. DMA allocation failures emit diagnostic events.
2. **Early-warning threshold**: `HEAP_WARNING_THRESHOLD` (50KB) / `HEAP_CRITICAL_THRESHOLD` (40KB) in `config.h`. `DebugState.heapWarning` flag + `EVT_HEAP_PRESSURE` event bit. Broadcast via WS/MQTT.
3. **Heap profiling**: `heap_budget.h/.cpp` — per-subsystem allocation tracker. Exposed via WS `heapBudget` array and REST `/api/diag/snapshot`.
4. **Graduated feature shedding**: Warning halves WS binary rate. Critical refuses DMA alloc, suppresses WS binary, refuses DSP stages.
- Files: `src/config.h`, `src/state/debug_state.h`, `src/diag_error_codes.h`, `src/app_events.h`, `src/app_state.h`, `src/main.cpp`, `src/audio_pipeline.cpp`, `src/dsp_pipeline.cpp`, `src/websocket_handler.cpp`, `src/mqtt_publish.cpp`, `src/heap_budget.h`, `src/heap_budget.cpp`
- Test coverage: 30 new tests in `test_heap_monitor` (15) + `test_heap_budget` (15). 1727 total tests passing.

---

### Deferred Device Toggle Queue Capacity Overflow

**Area:** HAL device lifecycle, state management
**Files:** `src/state/hal_coord_state.h`, `src/main.cpp` (drain loop)
**Problem:** `PENDING_TOGGLE_CAPACITY=8` is fixed. Rapid device enable/disable requests (e.g., web UI toggle spam, concurrent MQTT commands) may overflow. Overflow returns false silently; caller does not retry automatically.

**Current Behavior:**
- `requestDeviceToggle(halSlot, action)` returns false on overflow or invalid args
- Same-slot dedup prevents contradictory requests (e.g., enable→disable) within the queue
- Main loop drains all entries each tick, then clears
- No metrics on overflow frequency

**Risk:** Users see toggle requests ignored without feedback. Load test with concurrent rapid toggles not in CI.

**Improvement Path:**
1. Add overflow counter to `HalCoordState` for telemetry
2. Broadcast overflow event via WebSocket when queue full
3. Implement automatic retry in caller with exponential backoff (web handlers)
4. Profile typical concurrency: How often does queue exceed 2-3 entries in normal use?

---

**FIXED — Matrix Routing Bounds & Validation Gaps:**
- Resolution: Multi-layer bounds hardening with compile-time and runtime guards. `static_assert` in `audio_pipeline.cpp` enforces `MAX_INPUTS*2 <= MATRIX_SIZE` and `MAX_SINKS*2 <= MATRIX_SIZE` at build time. `audio_input_source.h` fallback constant fixed from 4 to 8 (eliminates include-order fragility). `pipeline_mix_matrix()` `inCh[]` array population changed from hardcoded 4-lane initializer to bounds-safe loop covering all 8 lanes (fixes silent audio discard for lanes 4-7). `audio_pipeline_set_sink()` validates `firstChannel + channelCount <= MATRIX_SIZE`. `audio_pipeline_set_source()` validates `lane*2+1 < MATRIX_SIZE`. All 3 HAL drivers (`hal_pcm5102a.cpp`, `hal_es8311.cpp`, `hal_mcp4725.cpp`) validate `firstChannel + channelCount <= MATRIX_SIZE` in `buildSink()`, returning false on overflow. `test_pipeline_output.cpp` stale constants updated from `MATRIX_SIZE=8, MAX_SINKS=4` to `16, 8`.
- Files: `src/audio_pipeline.cpp`, `src/audio_input_source.h`, `src/hal/hal_pcm5102a.cpp`, `src/hal/hal_es8311.cpp`, `src/hal/hal_mcp4725.cpp`, `test/test_matrix_bounds/`, `test/test_pipeline_output/`
- Test coverage: 24 new tests in `test_matrix_bounds` (dimension invariants, gain bounds, firstChannel overflow, registration bounds, inCh population, sink channel validation). 1693 total tests passing.

---

### Shared I2C Bus Contention During Discovery

**Area:** HAL discovery, I2C bus management
**Files:** `src/hal/hal_discovery.cpp`, `src/hal/hal_device_manager.cpp`
**Problem:** HAL discovery scans I2C Bus 0 (GPIO 48/54) **only when WiFi is NOT connected**. However, ES8311 post-boot rescan via `POST /api/hal/scan` is not guarded — can run while WiFi active, causing silent I2C hangs (SDIO conflict) and potential MCU reset.

**Current Protections:**
- `hal_discovery_scan()` checks WiFi state before Bus 0 scan
- `/api/hal/scan` endpoint does NOT enforce the same check — direct user-facing endpoint
- No mutex prevents concurrent scan + ES8311 driver transaction

**Risk:** User triggers rescan while WiFi active → I2C hangs or MCU resets. Silent failure, no user feedback.

**Improvement Path:**
1. Add WiFi state check in `/api/hal/scan` HTTP handler: return 409 if `WiFi.isConnected()` and `Bus 0 requested`
2. Add comment in HAL discovery: "Bus 0 SDIO shared — scan-guard required"
3. Implement optional background rescan with exponential backoff (skip if WiFi just connected)
4. Test: WiFi connect → immediate rescan → verify endpoint returns 409 or skips Bus 0

---

### MQTT Broker Reconfiguration Race Window

**Area:** MQTT subsystem, cross-task communication
**Files:** `src/mqtt_handler.cpp`, `src/mqtt_task.cpp`, `src/app_state.h`
**Problem:** Web UI sets broker credentials and flags `appState._mqttReconfigPending = true`. Main loop picks up the flag, broadcasts the update. Meanwhile, `mqtt_task` (Core 0) periodically checks `_mqttReconfigPending` to trigger reconnect. If reconfiguration happens mid-publish, the message may use old credentials or mix old+new connection state.

**Current Synchronization:**
- `volatile bool _mqttReconfigPending` flag only — no mutex or handshake
- `mqtt_task` polls at 20 Hz (50ms window)
- Main loop clears flag after broadcast, but does NOT wait for mqtt_task to ACK

**Risk:** Race: main-loop sets pending, mqtt_task reads old credentials mid-transaction, inconsistent state. Rare but possible under load.

**Improvement Path:**
1. Add binary semaphore `_mqttReconfigAck` (like `appState.audio.taskPausedAck`)
2. Main loop waits: `appState._mqttReconfigPending = true` → `xSemaphoreTake(reconfigAck, 100ms)` after timeout, abort
3. `mqtt_task` checks pending, executes disconnect + reconnect, gives semaphore
4. Add telemetry: log reconfiguration attempts + success/timeout count

---

## Security Issues

### WebSocket Authentication Token Reuse Window

**Area:** WebSocket auth, session management
**Files:** `src/websocket_handler.cpp`, `src/auth_handler.h/.cpp`
**Problem:** WS token pool has 16 slots with 60s TTL. Token issued via `GET /api/ws-token` (HTTP endpoint, rate-limited). Once token obtained, client can reconnect multiple times with same token until expiry. If token captured during initial WS connect, attacker can reuse it for 60s.

**Current Mitigations:**
- HTTP endpoint rate-limited (429 after N failed logins, 5min cooldown)
- Token `used` flag checked at WS connect
- HTTPOnly cookie on HTTP login prevents JS token leakage

**Risk:** Captured token = 60s of full WebSocket access (state reads, device controls, settings). Rate limiting only on HTTP endpoint, not on token validity itself.

**Improvement Path:**
1. Reduce token TTL to 10s (browser fetch → immediate WS connect overhead is <1s)
2. Mark token `used=true` on first WS connection, reject reuse
3. Implement token rotation: issue new token at each WS message if approaching TTL
4. Add telemetry: log token reuse attempts (security event)

---

### REST API Password Reset Lacks Rate Limiting on Serial Read

**Area:** Authentication, first-boot setup
**Files:** `src/auth_handler.cpp`, `src/main.cpp`
**Problem:** First-boot password is printed to serial (115200 baud, visible to anyone with physical serial access or USB traffic capture). No rate limiting on serial reads; attacker with board access can brute-force simple passwords if serial debugging is enabled.

**Current Protections:**
- First-boot password is random (32 bits of entropy, weak)
- Serial output at 115200 baud only
- PBKDF2-SHA256 (10k iterations) for stored password

**Risk:** Serial protocol is unencrypted. Attacker with USB access can capture setup password, then use it for HTTP/WS access.

**Improvement Path:**
1. Increase first-boot password entropy: use full 256-bit random value, encode as base64 (43 chars)
2. Print password ONCE only, hide from serial logs thereafter
3. Implement serial-port locking after first-boot setup (block further reads)
4. Add option: skip first-boot password printing if EEPROM already has hash

---

### Certificate Validation Bypass via Setting

**Area:** TLS, OTA updates
**Files:** `src/app_state.h`, `src/settings_manager.cpp`, `src/ota_updater.cpp`
**Problem:** User can disable certificate validation via `appState.general.enableCertValidation`. This flag affects **all outgoing HTTPS connections** (OTA downloads, API calls). Setting is persistent in `/config.json`. If compromised or misapplied, firmware updates can be MITM'd.

**Current Protections:**
- Setting defaults to true (enabled)
- Used in `HTTPClient` for OTA downloads and GitHub API calls
- No audit logging of setting changes

**Risk:** User accidentally disables cert validation to work around a CA issue, forgets to re-enable. Or malicious code/web page tricks user into disabling via settings API.

**Improvement Path:**
1. Add warning in web UI: "Disabling certificate validation is insecure. Re-enable before next OTA update."
2. Implement auto-restore: reset to `true` on each boot (opt-in via config)
3. Log all changes to this flag: timestamp, old/new value, trigger (web UI / MQTT)
4. Add feature: whitelist specific certificate pins for OTA, bypass CA bundle requirement

---

### MQTT Callback Side-Effects via Dirty Flags

**Area:** MQTT broker integration, thread safety
**Files:** `src/mqtt_handler.cpp`, `src/websocket_handler.cpp`
**Problem:** `mqttCallback()` runs in `mqtt_task` (Core 0) context. It sets dirty flags (e.g., `appState.setWifiDirty()`) to signal main loop changes. Dirty flags themselves are thread-safe (volatile bool), but the state they represent may not be read atomically by consumers.

**Example:** MQTT publishes `"dsp":{"enabled":true,"bypass":false}`. Callback sets `appState.dsp.enabled = true`, then `appState.dsp.bypass = false`. Main loop reads `appState.dsp.enabled` for one frame, then `appState.dsp.bypass` on next frame — seeing inconsistent state.

**Current Assumption:** State fields are single memory word (atomic on ARM Thumb). Compound structures (arrays, structs) are NOT atomic.

**Risk:** Rare race conditions in code that reads multi-field state without holding a lock. May cause state inconsistency, audio glitches, or missed settings application.

**Improvement Path:**
1. Audit all state structures: mark compound fields (arrays, >4 bytes) as "must lock to read"
2. Add `HalCoordState`-style spinlock pattern to frequently-accessed compound state (DSP settings, audio routing matrix)
3. Require dirty flag setters to snapshot the whole struct into a temp copy, publish temp
4. Add test: MQTT callback changes 2+ fields rapidly, verify main loop reads consistent snapshot

---

## Performance Bottlenecks

### WebSocket Broadcast Serialization Under Load

**Area:** WebSocket, web UI real-time updates
**Files:** `src/websocket_handler.cpp`
**Problem:** Every state change triggers `sendAudioState()`, `sendDspState()`, etc., which serialize entire state objects to JSON. With 8 audio lanes + 16 matrix + DSP config, each broadcast is ~5KB JSON. At 50ms intervals (20 Hz max), throughput is ~100KB/s. On slow networks (2.4GHz WiFi, AP mode), this can saturate bandwidth.

**Current Mitigations:**
- `wsAnyClientAuthenticated()` guard skips serialization if no WS clients
- Deferred init-state queue spreads burst across multiple ticks
- Dirty flags limit redundant broadcasts (state unchanged → no broadcast)

**Risk:** Multiple connected web UI clients (e.g., phone + browser) cause exponential bandwidth use. MQTT publishes compete with WS for WiFi TX queue. Audio pipeline glitches if WiFi TX blocks Core 1 preemption.

**Improvement Path:**
1. Implement incremental updates: send only changed fields in WS message (delta encoding)
2. Add per-client subscription filtering: allow UI to opt out of non-critical broadcasts (e.g., realtime waveform)
3. Rate-limit state broadcasts: coalesce multiple changes within 50ms window into single message
4. Add telemetry: measure WS queue depth and TX latency; log when queue backs up

---

### DSP Stage Allocation Fragmentation

**Area:** DSP subsystem, dynamic memory
**Files:** `src/dsp_pipeline.cpp`, `src/dsp_pipeline.h`
**Problem:** DSP stages (biquad, FIR, delay) are pool-allocated. When user loads DSP config with many stages, then loads a different config with fewer stages, pool fragments. Old stage objects remain allocated but unused, reducing space for future configs.

**Current Behavior:**
- `dsp_add_stage()` appends to pool on heap
- Config import loops through preset stages, calls `dsp_add_stage()` for each
- No explicit pool reset between configs — only on `dsp_init()`
- If import fails partway, partial stages remain in pool

**Risk:** After several config swaps, pool becomes 70% fragmented. User cannot load a config that requires >N stages, even though total allocation is less than maximum.

**Improvement Path:**
1. Implement "pool defragmentation": after config import, compact pool (remove gaps, rebuild stage list in order)
2. Add API: `dsp_clear_all_stages()` before config load to start fresh
3. Add telemetry: log pool utilization (used / total) after each config operation
4. Add test: load 10+ different DSP configs in sequence, verify pool doesn't fragment below 10% usable

---

### GPIO Pin Claim Table Not Bounds-Checked on Lookup

**Area:** HAL device manager, GPIO allocation
**Files:** `src/hal/hal_device_manager.h/.cpp`, `src/hal/hal_types.h`
**Problem:** HAL pin tracking uses `HalPinAlloc _pins[HAL_MAX_PINS]` with `HAL_MAX_PINS=56`. If a device tries to claim GPIO 99 (invalid on ESP32-P4), the lookup `_pins[99]` reads out-of-bounds.

**Current State:**
- Fixed in recent commit `eb4d8b3`: `claimPin()` now validates `gpio <= HAL_GPIO_MAX (54)` with LOG_W on overflow
- `isPinClaimed()` has guard: `if (gpio < 0 || gpio >= HAL_MAX_PINS) return false`
- One-time issue, now mitigated

**Status:** MITIGATED (fixed March 2026). Retained for historical reference.

---

## Test Coverage Gaps

### Audio Pipeline Lazy Allocation Not Tested for Underflow

**Area:** Audio pipeline, initialization
**Files:** `src/audio_pipeline.cpp`
**Problem:** Lane/channel buffers (`_laneL`, `_laneR`, `_outCh`, etc.) are allocated in `audio_pipeline_init()` but checked for nullptr in `pipeline_to_float()` and other processing functions. If allocation fails silently (returns nullptr), audio is muted but no error is logged.

**Current Check:**
```cpp
if (!_rawBuf[i] || !_laneL[i] || !_laneR[i]) continue;
```

**Test Gap:** Unit tests do not mock heap exhaustion to verify silent mute behavior. No integration test verifies "graceful degradation" when audio buffers fail to allocate.

**Risk:** User reports "no audio output" without visible error. Debugging is hard because the condition is silent.

**Improvement Path:**
1. Add unit test `test_audio_pipeline_alloc_failure`: mock `calloc()` to fail, verify pipeline logs and gracefully skips channels
2. Add integration test: simulate low-memory condition via heap quota, start audio, verify silent mute + diagnostic log
3. Implement `audio_health_get_alloc_failures()` API to expose this metric via REST

---

### MQTT HA Discovery Not Tested for Large Payload

**Area:** MQTT, Home Assistant discovery
**Files:** `src/mqtt_ha_discovery.cpp`
**Problem:** HA discovery publishes per-device configuration JSONs to MQTT (e.g., DSP config, device info). Payload sizes can exceed MQTT max-payload-size on some brokers (default 256KB, but some limit to 64KB). No test verifies payloads stay under limit.

**Test Gap:** E2E tests use mock MQTT server without payload size enforcement.

**Risk:** On some MQTT brokers, large DSP config publishes silently drop or disconnect the client.

**Improvement Path:**
1. Add unit test: generate max-complexity HA discovery JSON, measure size, assert < 256KB
2. Add telemetry: log payload size before each MQTT publish (HA discovery section)
3. Implement payload compression: if payload > threshold, use MQTT string compression (not in current roadmap)

---

### WebSocket Message Ordering Not Guaranteed Across Clients

**Area:** WebSocket protocol, broadcast consistency
**Files:** `src/websocket_handler.cpp`
**Problem:** WebSocket library may deliver messages to different clients in different orders or with different frame boundaries. If state changes rapidly (e.g., DSP preset load + filter enable), client A may receive `[dsp-preset-loaded, filter-enabled]` while client B receives `[filter-enabled, dsp-preset-loaded]` — different effective state.

**Current Assumption:** No explicit message sequencing; reliant on WebSocket library delivery order.

**Test Gap:** E2E tests do not verify message ordering across multiple concurrent clients.

**Risk:** Multi-client scenarios (e.g., two browser tabs, mobile app + desktop) see different effective state without any error indication.

**Improvement Path:**
1. Add sequence number to WS messages: `{"seq":42,"type":"dsp-state","data":{...}}`
2. Client-side: buffer out-of-order messages, reorder by seq before applying state
3. Add E2E test: open 2+ concurrent WS clients, trigger rapid state changes, verify both see same final state

---

## Known Bugs (Unresolved)

### MCLK Continuity Requirement Not Enforced

**Area:** I2S audio, PCM1808
**Files:** `src/i2s_audio.cpp`, `src/dac_hal.cpp`, `src/CLAUDE.md` (documented)
**Problem:** Comment in CLAUDE.md warns: "Never call `i2s_configure_adc1()` in the audio task loop — MCLK must remain continuous for PCM1808 PLL stability." However, no assertion enforces this. If a future change calls `i2s_configure_adc1()` from `audio_pipeline_task`, it will silently break audio (PLL loses lock).

**Current Safeguard:** Only called from `setup()` and `i2s_audio_set_sample_rate()` (main loop, safe).

**Risk:** Refactoring audio init/reinit could accidentally call this from wrong task context, hard to debug (silent PLL loss = intermittent audio dropout at 5-10min intervals).

**Improvement Path:**
1. Add assertion in `i2s_configure_adc1()`: `assert(xTaskGetCurrentTaskHandle() == xTaskGetHandle("loopTask"))`  or similar
2. Add documentation: "This function must only be called from Core 1 main loop"
3. Rename to `i2s_adc1_init_core1_only()` to make it obvious

---

### DSP Preset Import Partial Failure Leaves Inconsistent State

**Area:** DSP subsystem, configuration persistence
**Files:** `src/dsp_api.cpp`, `src/dsp_pipeline.cpp`
**Problem:** When importing a DSP preset from JSON, the import loop calls `dsp_add_stage()` for each stage. If import fails partway (e.g., invalid FIR coefficient), some stages are added but stage count and pool are not reset. The next preset import appends to this partial state.

**Example:** Load preset A (3 stages) → partial failure at stage 2 (2 added) → load preset B (3 stages) → results in 5 stages total (2 from A + 3 from B), not intended 3.

**Current Behavior:**
- No rollback mechanism: `dsp_add_stage()` commits immediately
- API returns error, but stages remain in pool

**Test Gap:** No test verifies partial-failure recovery.

**Risk:** User reports "DSP config wrong after import error". Audio processing uses stale + new stages in unpredictable order.

**Improvement Path:**
1. Implement transactional import: collect all stages in temp array, commit only if entire import succeeds
2. On import failure, log all stages added so far (for debugging)
3. Add API: `dsp_clear_all_stages()` to explicitly reset pool before import
4. Add test: trigger import failure at each stage index, verify state is either fully old or fully new (never mixed)

---

### HAL Device State Callbacks Fire During Iterator Loops

**Area:** HAL device manager, iteration safety
**Files:** `src/hal/hal_device_manager.cpp`, `src/hal/hal_pipeline_bridge.cpp`
**Problem:** HAL device state changes (e.g., DETECTED → AVAILABLE) invoke `_stateChangeCb`, which calls `hal_pipeline_bridge` to register sinks. If state change happens during `forEach()` iteration, the callback modifies the device list while iteration is in progress, causing undefined behavior.

**Current Safeguard:** State changes only happen in `main()` or `healthCheckAll()` (not concurrent with iteration). But no explicit guard prevents future code from triggering state change mid-iteration.

**Risk:** If refactoring changes when `hal_pipeline_bridge` callbacks fire, iteration becomes unsafe.

**Improvement Path:**
1. Make `forEach()` safe: snapshot device pointers at start, iterate snapshot only (tolerates concurrent state changes)
2. Or add re-entrancy guard: `if (iterating) { defer_callback(); return; }`
3. Add test: trigger state change during `forEach()`, verify iteration completes without crash/corruption

---

## Fragile Areas

### Audio Pipeline Swap Pending Flag Not Atomic

**Area:** Audio pipeline, DSP config swaps
**Files:** `src/audio_pipeline.cpp`, `src/dsp_api.cpp`
**Problem:** `_swapPending` is a non-volatile bool. When DSP config swap is triggered from main loop (`appState.setDspSwapDirty()`), it flags the swap and the audio task later executes it. Between flag set and execution, if another config change happens, the behavior is undefined.

**Current Code:**
```cpp
static bool _swapPending = false;
```

**Risk:** Rare race: Set swap_pending=true, load new config, set swap_pending=true again. Audio task executes swap with partially-loaded config.

**Improvement Path:**
1. Change to `volatile bool _swapPending = false`
2. Add timestamp field: `static uint32_t _swapPendingMs = 0`
3. Guard: ignore swap requests if a swap executed within last 10ms
4. Add test: rapid DSP swaps, verify only one swap per 10ms window

---

### GUI Screen Navigation Not Protected from Async State Changes

**Area:** GUI, navigation
**Files:** `src/gui/gui_navigation.cpp`, `src/gui/gui_manager.cpp`
**Problem:** GUI runs on Core 0 (separate from Core 1 audio pipeline). When user navigates to a screen, the screen init code reads `appState` fields (e.g., DSP config, audio levels). Meanwhile, audio task may update those fields. GUI reads inconsistent snapshot if fields are >4 bytes.

**Example:** Navigation reads `appState.dsp.stages[0]` (struct, 20+ bytes), while audio task updates `appState.dsp.stages[0].freq`. GUI sees partially-old + partially-new struct.

**Risk:** GUI displays wrong DSP parameters, crashes on null pointer (if struct contains pointers).

**Improvement Path:**
1. Implement "GUI state snapshot": main loop periodically copies `appState.dsp` / `appState.audio` to `guiSnapshot` under spinlock
2. GUI reads from `guiSnapshot` only, never directly from `appState`
3. Snapshot update interval = 100ms (slower than audio task, but fast enough for UI responsiveness)
4. Add test: rapid state changes, capture GUI during update, verify no corruption

---

### OTA Download Resume Not Tested for Interruption

**Area:** OTA updates, firmware download
**Files:** `src/ota_updater.cpp`
**Problem:** OTA download supports resume via HTTP Range header. If download is interrupted and resumed multiple times, the downloaded portion may be corrupted (bit flip during transmission, not re-verified). Resume logic does not re-validate the partial download before appending.

**Current Verification:**
- SHA256 check happens **after** full download completes
- Partial downloads during resume are not validated

**Risk:** User resumes failed OTA multiple times, final binary is corrupted, firmware boot fails.

**Improvement Path:**
1. Implement chunk-level SHA256: compute hash of each 512KB chunk as downloaded
2. On resume, verify chunks already present before appending new ones
3. Add test: simulate interruption at various points, resume, verify final SHA256 matches

---

## Scaling Limits

### WebSocket Concurrent Clients Hard-Capped at MAX_WS_CLIENTS=32

**Area:** WebSocket server, scalability
**Files:** `src/websocket_handler.cpp`, `src/config.h`
**Problem:** WS auth tracking uses fixed arrays (`wsAuthStatus[MAX_WS_CLIENTS]`). If more than 32 clients try to connect, excess clients are silently rejected or cause array bounds write.

**Current State:**
```cpp
#define MAX_WS_CLIENTS 32
bool wsAuthStatus[MAX_WS_CLIENTS] = {false};
```

**Risk:** Commercial installation with many remote users (cloud dashboard, multiple UI instances) hits limit and stops accepting WS connections.

**Improvement Path:**
1. Profile typical use case: How many simultaneous UI clients are expected? (likely 2-4 max, but future may support more)
2. If scaling needed, implement dynamic linked list for WS client state (instead of fixed array)
3. Add telemetry: log peak concurrent WS client count

---

### HAL Device Registry Hard-Capped at HAL_MAX_DEVICES=24

**Area:** HAL device manager, extensibility
**Files:** `src/hal/hal_device_manager.h`, `src/hal/hal_types.h`
**Problem:** Device registry has fixed array of 24 slots. With typical 14 onboard devices (PCM5102A, ES8311, 2×PCM1808, NS4150B, TempSensor, Display, Encoder, Buzzer, LED, Relay, Button, SigGen, USB Audio), 10 slots remain for add-ons. If user tries to add >10 expansion devices, registration fails.

**Current Usage:**
- 14 devices at boot (onboard + optional)
- ~10 free slots for EEPROM-discovered expansion

**Risk:** Modular platform's value prop is flexibility. If adding 12 expansion modules fails, user is frustrated.

**Improvement Path:**
1. Profile typical expansion scenarios: How many add-on devices in realistic systems? (likely 4-6 max)
2. If expansion beyond 24 needed, implement dynamic allocation (small overhead, large flexibility)
3. Add telemetry: log device count at boot and on hot-add
4. Document in HAL guide: "Up to 24 devices supported. Typical systems use 14-18. Plan accordingly for expansions."

---

## Missing or Deferred Features

### No Incremental Firmware Update Mechanism

**Area:** OTA updates, bandwidth efficiency
**Files:** `src/ota_updater.cpp`
**Problem:** Each OTA download fetches full firmware binary (500KB+). For users on metered/slow connections, this is expensive. No delta/patch mechanism exists (only full binary or resume-on-error).

**Current Approach:** Full binary download, SHA256 verification, flash.

**Risk:** Users on poor networks skip security updates due to data cost. No feature parity with modern firmware update systems (delta + patch).

**Improvement Path:**
1. Defer to v2 roadmap: Implement binary delta patching (requires host-side toolchain)
2. Near-term: Add compression option (GZ compressed firmware binary, saves ~30-40% size)
3. Document bandwidth requirement in user guide

---

### No Multi-User or Role-Based Access Control

**Area:** Authentication, security
**Files:** `src/auth_handler.cpp`, `src/websocket_handler.cpp`
**Problem:** Single password for all users. No concept of admin vs. guest roles, no per-user permission scopes, no audit log of who changed what.

**Current Model:** One password authenticates to full access (all state read/write, device control, settings, OTA).

**Risk:** If password is shared, cannot revoke access to one user without changing for all. Cannot enable "guest mode" (read-only access).

**Improvement Path:**
1. Design multi-user system (deferred to v2)
2. Near-term: Add per-token read-only flag (WS token can be issued read-only)
3. Implement basic audit log: log all state-changing REST/WS commands with timestamp + token id

---

### No Automatic Backup/Restore of Settings and HAL Config

**Area:** Data persistence, user experience
**Files:** `src/settings_manager.cpp`, `src/hal/hal_settings.cpp`
**Problem:** User spends time configuring HAL devices, DSP presets, WiFi networks. If device fails or is factory-reset, all config is lost. No export/restore mechanism exists.

**Current State:**
- Settings exported via REST: `GET /api/export` (JSON download)
- No auto-backup or cloud sync

**Risk:** Power user loses hours of work if device fails or is accidentally reset.

**Improvement Path:**
1. Near-term: Add import feature (`POST /api/import` with JSON body) to complement export
2. Add QR code export: encode small config as QR for quick phone backup
3. Defer cloud sync to v2 roadmap

---

### DSP Equalizer UI No Undo/Redo

**Area:** DSP UI, user experience
**Files:** `web_src/js/06-peq-overlay.js`, `src/dsp_api.cpp`
**Problem:** User adjusts PEQ parameters (15+ sliders), makes mistake, no undo button. Must manually revert all changes or reload saved preset.

**Current UI:** Save button commits to persistent storage; no draft mode.

**Risk:** Users avoid making fine adjustments due to fear of losing current good config.

**Improvement Path:**
1. Implement draft mode: in-memory edits until user clicks "Save" (no persistent save on slider change)
2. Add "Undo" button: revert to last saved version
3. Add "Reset to Default": clear all edits without persisting

---

## Recommendations Summary

**High Priority (next sprint):**
1. **MCLK continuity**: Add assertion in `i2s_configure_adc1()` to prevent task-context misuse
2. **MQTT reconfiguration race**: Implement binary semaphore handshake for broker setting changes
3. **I2C Bus 0 contention**: Guard `/api/hal/scan` with WiFi state check (409 on conflict)

**Medium Priority (next quarter):**
1. ~~**Audio pipeline allocation tracing**~~: FIXED — graduated heap pressure + heap budget tracker
2. **WebSocket message ordering**: Add sequence numbers and client-side reordering
3. **HAL state callback re-entrancy**: Make `forEach()` safe to iterate during state changes
4. **DSP preset partial-failure**: Implement transactional import with rollback

**Low Priority (roadmap, nice-to-have):**
1. **Incremental firmware updates**: Implement GZ compression for smaller OTA payloads
2. **Multi-user/RBAC**: Design and implement role-based access control
3. **Settings backup/restore**: Add import feature and QR export

---

*Concerns audit: 2026-03-21*
