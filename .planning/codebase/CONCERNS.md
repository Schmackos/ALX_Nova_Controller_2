# Codebase Concerns

**Analysis Date:** 2026-03-08

---

## Known Bugs (Confirmed Active)

### Array Out-of-Bounds — Input Label Arrays (3 sites, crash risk)

**Severity: HIGH — undefined behavior at runtime on any HA discovery or input name load.**

Three separate arrays are sized for 4 entries but iterated up to `AUDIO_PIPELINE_MAX_INPUTS * 2 = 16`:

**Site 1 — `src/mqtt_ha_discovery.cpp:1414-1426`:**
```cpp
const char *inputLabels[]      = {"input1_name_l", "input1_name_r", "input2_name_l", "input2_name_r"}; // 4 entries
const char *inputDisplayNames[] = {"Input 1 Left Name", ..., "Input 2 Right Name"};                    // 4 entries
for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {   // loops i=0..15
    doc["name"] = inputDisplayNames[i];   // OOB at i=4+
```
- Impact: Reads stack garbage into MQTT HA discovery payloads for inputs 3-15. Publishes corrupt entity names to Home Assistant.

**Site 2 — `src/mqtt_publish.cpp:889-893`:**
```cpp
const char *labels[] = {"input1_name_l", ..., "input2_name_r"};  // 4 entries
for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {        // loops i=0..15
    mqttClient.publish((base + "/audio/" + labels[i]).c_str(), ...); // OOB at i=4+
```
- Impact: Corrupt topic strings for MQTT input name publishes on inputs 3-15.

**Site 3 — `src/settings_manager.cpp:539-549`:**
```cpp
static const char *INPUT_NAME_DEFAULTS[] = {"Subwoofer 1", "Subwoofer 2", "Subwoofer 3", "Subwoofer 4"}; // 4 entries
for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {  // loops i=0..15
    appState.inputNames[i] = INPUT_NAME_DEFAULTS[i];        // OOB at i=4+
```
- Impact: Default input names for inputs 3-15 are garbage strings from the data segment. Triggered every cold boot when `/inputnames.txt` is absent.

**Fix approach:** Expand all three arrays to 16 entries (`AUDIO_PIPELINE_MAX_INPUTS * 2`) with appropriate default strings, or cap the loop at `sizeof(array) / sizeof(array[0])`.

---

### Matrix `inCh` Array — Upper 8 Channels Always NULL

**Severity: MEDIUM — matrix inputs 8-15 are permanently unreachable.**

- File: `src/audio_pipeline.cpp:282-287`
- `inCh[AUDIO_PIPELINE_MATRIX_SIZE]` (size 16) is initialized with only 8 entries (lanes 0-3 L+R). Entries `inCh[8..15]` are zero-initialized (NULL).
- The inner matrix loop `for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++)` iterates all 16, but `if (!inCh[i]) continue` silently skips the upper 8.
- Architecture claims a 16×16 matrix (`AUDIO_PIPELINE_MATRIX_SIZE=16`) but only 8 input channels are wired. Inputs from lanes 4-7 (if HAL-assigned) are invisible to the matrix.
- Fix approach: Extend `inCh` population to cover all 8 lanes (`_laneL[0..7]`, `_laneR[0..7]`) for a true 16-channel matrix, or document the actual limit as 8.

---

### Missing REST Endpoints — Matrix Save/Load

**Severity: MEDIUM — UI buttons silently fail (404 response).**

- Files: `web_src/js/05-audio-tab.js:362,367` and `src/web_pages.cpp:5889,5894`
- The web UI calls `POST /api/pipeline/matrix/save` and `POST /api/pipeline/matrix/load`.
- Neither endpoint is registered in `src/pipeline_api.cpp` or `src/main.cpp`.
- The ESP HTTP server returns 404. The UI shows a toast error on load and silently fails on save.
- The deferred auto-save via `pipeline_api_check_deferred_save()` works correctly; only the explicit save/load buttons are broken.
- Fix approach: Register `server.on("/api/pipeline/matrix/save", HTTP_POST, ...)` calling `audio_pipeline_save_matrix()` and `server.on("/api/pipeline/matrix/load", HTTP_POST, ...)` calling `audio_pipeline_load_matrix()` in `src/pipeline_api.cpp`.

---

### Bad Bitmask Default — ArduinoJson `|` Operator Used for Defaults

**Severity: MEDIUM — boolean fields always default to `true`, ignoring the JSON value.**

- Files:
  - `src/websocket_handler.cpp:611` — `bool linked = doc["linked"] | true;`
  - `src/websocket_handler.cpp:684` — `bool en = doc["enabled"] | true;`
  - `src/websocket_handler.cpp:699` — `bool en = doc["enabled"] | true;`
  - `src/dsp_api.cpp:1413` — `bool linked = doc["linked"] | true;`
- In ArduinoJson v7, `doc["key"] | defaultValue` is the correct default-value operator. Using `| true` means if the JSON field is `false`, it evaluates as `false | true = true`. The bitwise OR with `true` overrides any `false` value.
- Impact: Disabling a PEQ band (`"enabled": false`) or unlinking a stereo pair (`"linked": false`) via WebSocket command has no effect — the boolean always resolves to `true`.
- Fix approach: Use `doc["enabled"].as<bool>()` with a prior `doc["enabled"].is<bool>()` guard, or use `doc["enabled"] | (bool)false` for a safe ArduinoJson default.

---

### Preprocessor Directive Inside Macro Argument

**Severity: LOW — compiler warning (extension), build may fail with strict compilers.**

- File: `src/hal/hal_dsp_bridge.cpp:45-52`
```cpp
void HalDspBridge::dumpConfig() {
    LOG_I("[HAL:DspBridge] DSP Pipeline bridge — dspEnabled=%d",
#ifndef NATIVE_TEST
          AppState::getInstance().dspEnabled
#else
          0
#endif
    );
}
```
- Using `#ifndef` inside a macro argument is undefined behavior per C++ standard. GCC/Clang accept it as an extension but cppcheck and some CI compilers reject it.
- Fix approach: Extract the conditional value before the macro call: `int val = 0; #ifndef NATIVE_TEST val = AppState::getInstance().dspEnabled; #endif LOG_I("...", val);`

---

### `cachedReleaseList` Not Initialized in Constructor

**Severity: LOW — `String` members default-construct correctly in C++, but cppcheck flags it.**

- File: `src/app_state.h:150`
- `ReleaseInfo cachedReleaseList[OTA_MAX_RELEASES]` is a POD-containing struct with `String` members. `String` members default-construct to `""`, and `bool isPrerelease` zero-inits via aggregate init. Actual risk is low.
- The private `AppState()` constructor is empty (`{}`); cppcheck reports this member as uninitialised because the constructor does not explicitly initialize it.
- Fix approach: Add `cachedReleaseListCount = 0;` or use `= {}` member-level initialization, and document that `String` members are default-constructed.

---

### `memset` on Structs Containing `float` Members

**Severity: LOW — technically valid on IEEE 754 targets (all ESP32 targets), but not portable and cppcheck warns.**

Six sites use `memset` on structs that contain `float` fields:
- `src/audio_pipeline.cpp:900` — `memset(&_sinks[slot], 0, sizeof(AudioOutputSink))` — `AudioOutputSink` has `float gainLinear, vuL, vuR, _vuSmoothedL, _vuSmoothedR`. Followed immediately by explicit float assignments (gainLinear=1.0f etc.), so runtime behavior is correct.
- `src/dsp_pipeline.cpp:104,106` — `memset(&_mbSlots[i], 0, sizeof(DspMultibandSlot))` — struct contains float fields.
- `src/hal/hal_siggen.cpp:64` — `memset(&_source, 0, sizeof(_source))` — `AudioInputSource` has float VU/gain fields. Followed by explicit float assignments.
- `src/hal/hal_usb_audio.cpp:62` — same as hal_siggen.
- `src/thd_measurement.cpp:29,41` — `memset(&_result, 0, sizeof(_result))` — `ThdResult` contains `float thdPlusNPercent`, `float thdPlusNDb`, `float fundamentalDbfs`, `float harmonicLevels[8]`.
- On ESP32 (IEEE 754 with `0x00000000 = 0.0f`), these are safe. Fix approach when refactoring: replace with explicit field initialization or `= {}` value-initialization.

---

## Tech Debt

### AppState Monolith — 553 Lines, ~2184 Cross-References

- File: `src/app_state.h`
- AppState is a single singleton with 553 lines covering WiFi, MQTT, OTA, DSP, HAL, GPIO, display, sensing, Ethernet, and audio state. Estimated 2184 references across 48+ files.
- The planned decomposition into subsystem classes (from `.claude/plans/`) was deferred — estimated 29-38 hours of work.
- Impact: Every new feature adds members to this file. Merge conflicts are common. Testing requires stubbing the entire singleton. Subsystem isolation is impossible.
- Safe modification: Add new fields with default values only. Do not rename existing fields — `#define` aliases for legacy names exist in `src/app_state.h` and some may be missed by search.
- Fix approach: Phase extraction starting with the most isolated subsystems (e.g., OTA state, Ethernet state). Each subsystem gets its own header with dirty flags and event signalling.

---

### MQTT HA Discovery — 92 Publish Calls Per Discovery Run

- File: `src/mqtt_ha_discovery.cpp` (1888 lines)
- Each call to `publishHADiscovery()` sends 92+ individual MQTT retain messages. On a busy broker this can take several seconds and block the `mqtt_task` loop.
- The 1024-byte MQTT buffer (`mqttClient.setBufferSize(1024)`) may be insufficient for the largest HA discovery payloads (some PEQ/crossover entity configs exceed 512 bytes serialized, but 1024 is the current limit).
- No rate limiting or chunking — all 92 messages are sent synchronously in one task iteration.
- Fix approach: Batch discovery behind a flag that runs one entity per `mqtt_task` loop iteration; increase buffer size to 2048 for complex payloads.

---

### Pipeline Matrix Mismatch — 8×8 Comment vs 16×16 Constant

- File: `src/audio_pipeline.cpp:281`, `src/pipeline_api.cpp:21`, `src/config.h:99`, `src/output_dsp.h:17`
- `AUDIO_PIPELINE_MATRIX_SIZE = 16` but comments and code say "8×8 matrix". `OUTPUT_DSP_MAX_CHANNELS = 8` matches only 8 of 16 output channels. The `inCh[]` array only wires 8 inputs (see Known Bugs above). The matrix is architecturally 16×16 but operationally 8×8.
- Fix approach: Either commit to 16×16 (expand `inCh`, `OUTPUT_DSP_MAX_CHANNELS`, and output DSP) or reduce `AUDIO_PIPELINE_MATRIX_SIZE` to 8 and update all comments.

---

### Pipeline API — Hardcoded Input Channel Names

- File: `src/pipeline_api.cpp:36-39`
```cpp
inputs.add("ADC1 L"); inputs.add("ADC1 R");
inputs.add("ADC2 L"); inputs.add("ADC2 R");
inputs.add("SigGen L"); inputs.add("SigGen R");
inputs.add("USB L"); inputs.add("USB R");
```
- Input names are static strings that do not reflect HAL-assigned device names from `appState.inputNames[]`. When devices are renamed or lanes are reassigned by the HAL bridge, the matrix UI shows stale/incorrect labels.
- Fix approach: Read from `appState.inputNames[0..15]` populated by `loadInputNames()` and updated by the HAL bridge.

---

### DSP Config Swap Failure — "Staged for Retry" Never Retried

- Files: `src/dsp_api.cpp` (10+ call sites), `src/websocket_handler.cpp` (3 call sites)
- All `dsp_swap_config()` failures log `"Swap failed, staged for retry"` and increment `appState.dspSwapFailures`. No retry mechanism is implemented — the "staged" comment is aspirational.
- Impact: If the audio task holds the DSP lock for too long (e.g., during a heavy FIR convolution), a WebSocket-driven EQ change is silently dropped. The user sees no error; the UI shows the new EQ setting but it is not applied.
- Fix approach: Add a `_pendingSwapConfig` flag in AppState and retry `dsp_swap_config()` in `main.cpp`'s event loop on `EVT_DSP_CONFIG`.

---

### MQTT Hardcoded ADC Enable Indices (Lanes 0 and 1 Only)

- Files: `src/mqtt_handler.cpp:562,569` and `src/mqtt_publish.cpp:698,700`
- MQTT commands and publish for ADC enable/disable are hardcoded to `adcEnabled[0]` and `adcEnabled[1]`. Inputs on HAL-assigned lanes 2-7 have no MQTT control or state reporting.
- Fix approach: Extend the MQTT schema to include per-lane enable commands using dynamic lane index from HAL.

---

### Legacy Settings Migration Path — `settings.txt` Still Active

- File: `src/settings_manager.cpp`
- The legacy `settings.txt` and `mqtt_config.txt` migration code (approximately 300 lines) runs on every cold boot if `/config.json` is absent. These files have not been used as primary storage since the JSON migration, but the code path remains fully active.
- Impact: Boot time is extended by LittleFS reads on first boot. The migration code is large and has its own separate parsing logic that diverges from JSON handling.
- Fix approach: After v1.x EOL, add a migration-complete marker and conditionally compile or remove the legacy path.

---

### `hal_dsp_bridge.cpp` — `dspGetOutputLevel()` Uses Input Analysis

- File: `src/hal/hal_dsp_bridge.cpp:94-107`
```cpp
float HalDspBridge::dspGetOutputLevel(uint8_t lane) const {
    AudioAnalysis analysis = i2s_audio_get_analysis();
    if (lane < 2) return analysis.adc[lane].rmsCombined;  // Input, not output!
```
- The output level query returns the ADC (input) RMS, not post-pipeline output. This is explicitly commented as a future improvement (`// Future: read from per-output VU metering when available`).
- Impact: Health dashboard and diagnostics show input levels for output metering, making output health checks unreliable.
- Fix approach: Read per-output VU from `audio_pipeline_get_sink(slot)->vuL/R` instead of ADC analysis.

---

## Security Considerations

### PBKDF2 Iteration Count — 10,000 Iterations (Below Modern Recommendation)

- File: `src/auth_handler.cpp:109`
- PBKDF2-SHA256 uses 10,000 iterations. NIST SP 800-132 (2023) recommends 600,000+ for SHA-256. On embedded hardware this count is a practical compromise, but should be documented as a known limitation.
- Login rate limiting (`_nextLoginAllowedMs`, HTTP 429) provides primary brute-force defense.
- Current mitigation: HttpOnly cookie, short-lived WS tokens (60s TTL, 16-slot pool), non-blocking rate limiting with exponential backoff (1s→30s).
- Recommendation: Document the iteration count as a deliberate embedded-hardware tradeoff in auth_handler.cpp. Consider raising to 50,000 if boot-time hash migration (one-time cost) is acceptable.

---

### Web UI — Input Names Passed to MQTT Topics Without Sanitization

- Files: `src/mqtt_publish.cpp:891-892` and `src/mqtt_ha_discovery.cpp:1420,1426`
- User-controlled `appState.inputNames[i]` strings are concatenated directly into MQTT topic paths and HA discovery payloads without sanitization.
- MQTT topic strings containing `/`, `#`, `+`, or null bytes are protocol violations that can corrupt broker topic trees.
- Fix approach: Sanitize `inputNames` on write (strip `/#+\0`) in `settings_manager.cpp` before storing.

---

## Performance Bottlenecks

### MQTT HA Discovery — Synchronous 92-Message Burst

- File: `src/mqtt_ha_discovery.cpp`
- See Tech Debt section above. The synchronous discovery burst blocks `mqtt_task` for an estimated 500ms-2s depending on broker latency, during which MQTT publishes from the main pipeline are queued.

### LittleFS Atomic Config Write — Two-Copy Pattern on Every Save

- File: `src/settings_manager.cpp`
- Every settings save writes a full JSON to `config.json.tmp` then renames. On a 4MB LittleFS partition with wear leveling, this is correct but slow (~30-60ms blocking). Multiple subsystems trigger saves independently (settings, DSP, HAL config, matrix).
- Impact: UI-driven rapid changes (e.g., dragging a PEQ slider) may queue many atomic writes. The debounced save in `dsp_api.cpp` (2s delay) mitigates this for DSP, but `settings_manager.cpp` has no debounce on its direct-save path.

---

## Fragile Areas

### Dual I2S Master Init Order Dependency

- File: `src/i2s_audio.cpp` — `i2s_audio_init()` function
- PCM1808 ADC2 **must** be initialized before ADC1. ADC2 uses `I2S_PIN_NO_CHANGE` for BCK/WS/MCLK; ADC1 provides the actual clocks. Reversing the order silences ADC2.
- This constraint is undocumented except in `CLAUDE.md`. The HAL init priority system (`HAL_PRIORITY_*`) does not enforce this ordering — it depends on the order PCM1808 instances are registered in `hal_builtin_devices.cpp`.
- Safe modification: Do not change the registration order of `pcm1808` entries in `src/hal/hal_builtin_devices.cpp` without verifying ADC2 clock dependency.

### I2C Bus 0 (GPIO 48/54) — SDIO Conflict Causes MCU Reset

- Files: `src/hal/hal_discovery.cpp`, `CLAUDE.md`
- I2C Bus 0 shares GPIO 48/54 with the ESP32-C6 WiFi SDIO interface. Any I2C transaction on Bus 0 while WiFi is active causes `sdmmc_send_cmd` errors and a watchdog reset.
- The HAL discovery skips Bus 0 when WiFi is connected (`hal_discovery.cpp`), but third-party code or future HAL devices configured to use Bus 0 will cause silent resets.
- Test coverage: Not tested in the native test suite (hardware dependency).

### Audio Pipeline — `vTaskSuspendAll()` During Sink/Source Registration

- File: `src/audio_pipeline.cpp:827,873,898`
- Sink registration/removal uses `vTaskSuspendAll()` to prevent audio task preemption during struct copy. This is correct but means the entire FreeRTOS scheduler is paused for the duration of a `memset` + struct copy + counter update.
- If a HAL state callback fires during WiFi reconnect and triggers sink registration while WiFi/MQTT are in a timing-sensitive retry window, the scheduler pause extends the jitter.
- Safe modification: Keep the `vTaskSuspendAll()` windows as short as possible. Do not add allocations, logging, or I2C calls inside the suspended region.

### DAC Toggle — Deferred via `requestDacToggle()` / `requestEs8311Toggle()`

- File: `src/app_state.h` — `requestDacToggle(int8_t)`, `requestEs8311Toggle(int8_t)`
- Direct writes to `_pendingDacToggle` / `_pendingEs8311Toggle` bypass validation (only -1, 0, 1 are legal). The validated setters exist but are not enforced at the field access level (fields are non-const members).
- Safe modification: Only call `requestDacToggle()` and `requestEs8311Toggle()`. Never write `_pendingDacToggle` or `_pendingEs8311Toggle` directly.

### HAL Discovery — No Concurrency Guard on `POST /api/hal/scan`

- File: `src/hal/hal_api.cpp`, `src/hal/hal_discovery.cpp`
- A double-click on the "Rescan" button sends two concurrent POST requests. The second scan starts while the first is running I2C transactions, potentially causing corrupted I2C state or duplicate device registration.
- A mutex guard was noted as required but is not yet implemented (documented in `.claude/memory/hal.md`).
- Fix approach: Set a `volatile bool _scanInProgress` flag; return HTTP 409 if already scanning.

---

## Scaling Limits

### HAL Device Slots — 24 Maximum

- File: `src/hal/hal_types.h:9` — `#define HAL_MAX_DEVICES 24`
- At boot, 9 builtin devices are registered (PCM5102A, ES8311, PCM1808 ×2, NS4150B, TempSensor, SigGen, USB Audio, MCP4725). Adding 15+ expansion I2C devices (likely with hub boards) hits the cap.
- Fix approach: Increase `HAL_MAX_DEVICES` to 32 or use dynamic allocation.

### MQTT Buffer — 1024 Bytes

- File: `src/mqtt_handler.cpp:859` — `mqttClient.setBufferSize(1024)`
- HA discovery payloads for complex entities (multi-band crossover, PEQ chains) can approach 1024 bytes. Any payload over the buffer size is silently dropped by PubSubClient.
- Fix approach: Increase to 2048 bytes.

### Event Group — 8 Spare Bits (Bits 16-23)

- File: `src/app_events.h` — currently 16 bits assigned (bits 0-15), 8 spare (bits 16-23)
- Bits 24-31 are reserved by FreeRTOS. The system is currently at 67% event bit utilization with active development adding new subsystems.
- Fix approach: No immediate action; note that adding more than 8 new event types requires refactoring (e.g., using a second event group or a queue-based approach).

---

## Test Coverage Gaps

### Matrix Save/Load Endpoints

- What's not tested: `POST /api/pipeline/matrix/save` and `POST /api/pipeline/matrix/load` endpoints do not exist in firmware (see Known Bugs above). No E2E test covers the save/load button behavior.
- Files: `web_src/js/05-audio-tab.js:361-377`
- Risk: Silent 404 failure for user-initiated matrix save/load goes undetected.
- Priority: High (unregistered routes + untested UI buttons).

### HA Discovery Array OOB in Native Tests

- What's not tested: The 3 array-OOB sites above are not caught by `test_mqtt` because the MQTT mock's `connected()` returns `false` by default, causing all `publishMqtt*()` functions to return early before reaching the OOB loops.
- Files: `src/mqtt_ha_discovery.cpp`, `src/mqtt_publish.cpp`, `src/settings_manager.cpp`
- Risk: Bugs survive CI. Only manifest on device with MQTT connected.
- Priority: High. Fix: Set `mqttClient.isConnected = true` in test setUp for publish tests.

### `hal_dsp_bridge.cpp` — No Test Module

- What's not tested: `HalDspBridge` init/deinit, `dspIsActive()`, `dspSetBypassed()`, `dspGetInputLevel()`, `dspGetOutputLevel()`. The `dumpConfig()` preprocessor-in-macro bug is also not exercised.
- Files: `src/hal/hal_dsp_bridge.cpp`, `src/hal/hal_dsp_bridge.h`
- Risk: Regression in DSP bridge HAL integration goes undetected.
- Priority: Medium.

### Matrix Input Channel Wiring (Channels 8-15)

- What's not tested: `test_audio_pipeline` does not verify that matrix inputs 8-15 (from lanes 4-7) are routable. The current test exercises only the first 8 channels.
- Files: `src/audio_pipeline.cpp:282-306`
- Risk: Silent null-channel issue on any system with more than 4 active input lanes.
- Priority: Medium.

### DSP Swap Failure Retry

- What's not tested: `test_dsp_swap` does not verify that a failed `dsp_swap_config()` is retried. There is no retry mechanism to test.
- Files: `src/dsp_api.cpp`, `src/websocket_handler.cpp`
- Risk: EQ changes silently dropped under audio task contention.
- Priority: Medium.

---

## Removed / Deferred Items (Do Not Re-Add)

- **AppState subsystem decomposition**: Deferred. Plan archived to `.claude/plans/` (Steps 2-4 of Phase 5b). Shadow field extraction (Step 1) is complete.
- **I/O Registry**: Removed in v1.12.0. Superseded by HAL device framework. Do not re-introduce `io_registry.h`.
- **LED blinking test**: Removed in commit `bc81718`. LED stays LOW permanently.
- **`audio_pipeline_clear_sinks()`** in HAL deinit paths: Removed. Bridge uses slot-indexed `audio_pipeline_remove_sink(slot)`. Calling `clear_sinks()` during a partial HAL removal would disconnect all other active sinks.

---

*Concerns audit: 2026-03-08*
