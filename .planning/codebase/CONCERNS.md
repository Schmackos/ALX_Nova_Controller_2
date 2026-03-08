# Codebase Concerns

**Analysis Date:** 2026-03-08

**Context:** This is a REFRESH. Phases 1-6 of a concerns mitigation plan were executed since the previous audit (2026-03-07). Items marked [RESOLVED] were addressed by those phases. New concerns discovered during this audit are marked [NEW].

---

## Resolved Items (Phases 1-6)

The following concerns from the 2026-03-07 audit have been fully addressed:

- **[RESOLVED - Phase 1] Memory leak in `hal_pipeline_sync()` forEach context** -- `new int[2]` replaced with stack-allocated `int counts[2]` in `src/hal/hal_pipeline_bridge.cpp` line 292.
- **[RESOLVED - Phase 1] HAL I2C bus scan stub** -- `hal_i2c_scan_bus()` in `src/hal/hal_discovery.cpp` lines 127-182 is now fully implemented using Wire, scanning 0x08-0x77.
- **[RESOLVED - Phase 1] HAL Discovery SDIO conflict check ignores Ethernet** -- `src/hal/hal_discovery.cpp` line 32 now checks `appState.activeInterface == NET_WIFI` instead of cached `wifiConnectSuccess`.
- **[RESOLVED - Phase 1] Audio pipeline sink cap mismatch** -- `AUDIO_PIPELINE_MAX_OUTPUTS` raised to 8, matching `AUDIO_PIPELINE_MATRIX_SIZE`. `AUDIO_OUT_MAX_SINKS` is now `AUDIO_PIPELINE_MAX_OUTPUTS` (8).
- **[RESOLVED - Phase 2] Blocking `delay()` on login failure** -- Rate limiting in `src/auth_handler.cpp` is now non-blocking: `_nextLoginAllowedMs` gate returns HTTP 429 with `Retry-After` header immediately. No `delay()` in login path.
- **[RESOLVED - Phase 2] DAC toggle hardening** -- Validated setters `requestDacToggle()` / `requestEs8311Toggle()` in `src/app_state.h` lines 412-417 only accept -1, 0, 1. Direct writes to `_pendingDacToggle` / `_pendingEs8311Toggle` are documented as unsafe.
- **[RESOLVED - Phase 3] Session cookie missing HttpOnly flag** -- Cookie now has `HttpOnly`. WS authentication uses short-lived one-time tokens from `GET /api/ws-token` (16-slot pool, 60s TTL) in `src/auth_handler.cpp`.
- **[RESOLVED - Phase 3] SHA256 password hashing without salt** -- PBKDF2-SHA256 with 10,000 iterations, random 16-byte salt, stored as `"p1:<saltHex>:<keyHex>"`. Legacy SHA256 hashes auto-migrate on next login. First-boot random password (10 chars, ~57-bit entropy).
- **[RESOLVED - Phase 3] Default password hardcoded and shared** -- Per-device random password generated on first boot, displayed on TFT and serial. Regenerated on factory reset. `DEFAULT_AP_PASSWORD` in `src/config.h` line 177 is now only used as the WiFi AP passphrase (separate from web password).
- **[RESOLVED - Phase 4] Legacy line-based text persistence** -- Primary settings persistence is now `/config.json` with atomic write (`config.json.tmp` -> rename) in `src/settings_manager.cpp`. Legacy `settings.txt` loading retained for one-time migration only.
- **[RESOLVED - Phase 4] DSP float-int bridge** -- Pipeline DSP now operates natively on float32. `dsp_process_buffer_float()` at `src/audio_pipeline.cpp` line 247 replaces the old `_dspBridgeBuf` scratch. No int32 conversions in the DSP path.
- **[RESOLVED - Phase 5] `mqtt_handler.cpp` monolithic file (3819 lines)** -- Split into 3 files: `src/mqtt_handler.cpp` (1121 lines, core lifecycle), `src/mqtt_publish.cpp` (913 lines, publish + change-detection), `src/mqtt_ha_discovery.cpp` (1883 lines, HA discovery).
- **[RESOLVED - Phase 5] Change-detection shadow fields in AppState** -- `prevMqtt*` statics moved to `src/mqtt_publish.cpp`, `prevBroadcast*` statics moved to `src/smart_sensing.cpp`.
- **[RESOLVED - Phase 6] Test coverage gaps** -- Coverage expanded from 1271 to 1430+ tests across 62+ modules. `hal_audio_health_bridge`, `hal_pipeline_bridge` boot-time sync, and deferred toggle paths now have dedicated tests.
- **[RESOLVED] `hal_online_fetch` dead code** -- Files `hal_online_fetch.cpp`/`.h` have been removed. No references remain in `src/`.
- **[RESOLVED] `NUM_AUDIO_ADCS` deprecated** -- Fully removed. All references use `AUDIO_PIPELINE_MAX_INPUTS` (8).

---

## Tech Debt

**AppState `pipelineInputBypass` / `pipelineDspBypass` Array Size Mismatch:**
- Issue: `pipelineInputBypass[4]` and `pipelineDspBypass[4]` in `src/app_state.h` lines 487-488 are hardcoded to size 4, but `AUDIO_PIPELINE_MAX_INPUTS` is now 8. The pipeline loop in `pipeline_sync_flags()` at `src/audio_pipeline.cpp` line 146-148 iterates all 8 lanes and reads `s.pipelineInputBypass[i]` / `s.pipelineDspBypass[i]` for i=4..7, causing out-of-bounds reads from adjacent AppState members.
- Files: `src/app_state.h` lines 487-488, `src/audio_pipeline.cpp` lines 146-148, 641-643
- Impact: Undefined behavior for lanes 4-7. Currently benign because the adjacent memory happens to be `pipelineMatrixBypass` (false) and `pipelineOutputBypass` (false), which read as 0/false. But any reordering of AppState members or compiler optimization change could cause lanes 4-7 to be randomly bypassed or active.
- Fix approach: Change both arrays to `bool pipelineInputBypass[AUDIO_PIPELINE_MAX_INPUTS]` and `bool pipelineDspBypass[AUDIO_PIPELINE_MAX_INPUTS]` with appropriate default initializers. Update the initializer lists to cover all 8 lanes.

**Legacy DAC Fallback Path in Pipeline:**
- Issue: `pipeline_write_output()` at `src/audio_pipeline.cpp` lines 391-401 contains a legacy fallback that calls `dac_output_is_ready()` / `dac_secondary_is_ready()` / `dac_output_write()` / `dac_secondary_write()` when `_sinkCount == 0`. With HAL-managed sinks, this path should never execute in production -- but it creates a hidden dependency on the old dac_hal global functions.
- Files: `src/audio_pipeline.cpp` lines 391-401, `src/dac_hal.h` lines 77, 123
- Impact: If a bug causes all sinks to be removed (e.g., bridge race condition), the legacy path silently takes over with different routing. This masks the real problem and could produce unexpected audio output.
- Fix approach: Replace the legacy fallback with a LOG_W and silence output when `_sinkCount == 0`. Or remove the fallback entirely once HAL sink management is proven stable.

**Legacy `settings.txt` / `mqtt_config.txt` Read Paths Still Present:**
- Issue: `loadSettingsLegacy()` in `src/settings_manager.cpp` lines 167-289 still reads from `/settings.txt` using positional line indexing. `loadMqttSettings()` in `src/mqtt_handler.cpp` line 37 still reads from `/mqtt_config.txt`. Both are retained for one-time migration from pre-Phase-4 firmware.
- Files: `src/settings_manager.cpp` lines 167-289, `src/mqtt_handler.cpp` lines 37-97
- Impact: ~200 lines of dead-after-first-boot code. The positional line parser is fragile and has the fixed 30-slot `lines[]` array (no overflow protection). After enough firmware versions have shipped with JSON migration, these paths should be removed.
- Fix approach: Add a version check -- if `/config.json` version >= 2 on disk, skip legacy loading entirely. After 2-3 release cycles, remove `loadSettingsLegacy()` and the `settings.txt` parser.

**Legacy Flat AppState ADC Accessors:**
- Issue: This concern was present in the previous audit and remains. The alias block for `audioRmsLeft`, `audioVuLeft`, etc. has been removed, but 8 `AdcState audioAdc[AUDIO_PIPELINE_MAX_INPUTS]` elements (each with 17 fields) are allocated even though typically only lanes 0-1 are used for hardware ADCs. The comment at line 206 still references `{true, true}` initializer for 2 ADCs.
- Files: `src/app_state.h` lines 188, 206
- Impact: Minor memory waste (~1.6KB for unused AdcState entries). More importantly, health diagnostics, WS broadcasts, and MQTT publishes hardcode lane 0 and lane 1, not iterating over `activeInputCount`.
- Fix approach: Iterate `activeInputCount` in broadcast/publish paths instead of hardcoding lanes 0-1.

**`mqtt_ha_discovery.cpp` Remains Very Large (1883 Lines):**
- Issue: The HA discovery payload generation file is the largest module after `web_pages.cpp` and `web_pages_gz.cpp`. Each HA entity requires a dedicated function generating a JSON payload with ~30 fields.
- Files: `src/mqtt_ha_discovery.cpp`
- Impact: Compile-time for this single file is disproportionately long. Adding new HA entities requires duplicating substantial boilerplate.
- Fix approach: Introduce a template-based HA entity builder that constructs the common JSON structure and only takes entity-specific fields as parameters. This could reduce the file by 40-50%.

**`websocket_handler.cpp` Large Broadcast Functions (2407 Lines):**
- Issue: This file handles all WS message routing, CPU monitoring, hardware stats, audio broadcast, DSP config, HAL device list, and initial-state dispatch. It is the third-largest non-generated source file.
- Files: `src/websocket_handler.cpp`
- Impact: Long compile times; high cognitive load for any modification. The deferred initial-state queue (`_pendingInitState`) with 15 bit flags adds complexity.
- Fix approach: Extract WS broadcast functions into domain-specific files (e.g., `ws_audio_broadcast.cpp`, `ws_hal_broadcast.cpp`). The CPU monitoring hooks could move to a `cpu_monitor.cpp`.

**Static SRAM Allocation for 8-Lane Pipeline:**
- Issue: With `AUDIO_PIPELINE_MAX_INPUTS=8`, the static buffers in `src/audio_pipeline.cpp` consume significant internal SRAM: `_rawBuf[8][512]` = 16KB, `_sinkBuf[8][512]` = 16KB, `_dacBuf[512]` = 2KB. Total ~34KB of internal SRAM for DMA buffers alone.
- Files: `src/audio_pipeline.cpp` lines 37-38, 88
- Impact: With ESP32-P4 having limited internal SRAM (~512KB shared with WiFi RX buffers), 34KB is 6.6% of total. Adding more sinks or inputs pushes closer to the 40KB heap critical threshold.
- Fix approach: Allocate `_rawBuf` and `_sinkBuf` only for lanes/slots that have registered sources/sinks. Use lazy allocation with PSRAM fallback (DMA on P4 can access PSRAM via GDMA, unlike S3).

**16x16 Matrix Gain Table (1KB):**
- Issue: `_matrixGain[16][16]` at `src/audio_pipeline.cpp` line 63 is a 16x16 float matrix (1024 bytes). The actual pipeline uses 8 stereo inputs (16 mono channels) to 16 output channels, but the inner loop at line 290 iterates all 256 gain cells per output channel per buffer.
- Files: `src/audio_pipeline.cpp` lines 63, 287-296
- Impact: 256 float comparisons per output channel per 5.33ms buffer. With 16 output channels, that is 4096 gain checks per buffer iteration. Most cells are 0.0f and short-circuit, but the iteration overhead remains.
- Fix approach: Maintain a sparse representation (e.g., per-output list of non-zero input gains). This eliminates the O(N^2) scan.

---

## Known Bugs

**Noise Gate Hardcoded to ADC Lanes 0 and 1:**
- Symptoms: The noise gate in `pipeline_to_float()` at `src/audio_pipeline.cpp` lines 204-239 only activates for `i == AUDIO_SRC_LANE_ADC1 || i == AUDIO_SRC_LANE_ADC2` (lanes 0, 1). Any future hardware ADC on lane 2+ will not benefit from noise gating, and its noise floor will pass through to the DAC.
- Files: `src/audio_pipeline.cpp` lines 205-206
- Trigger: Register a third PCM1808 ADC via HAL; its noise floor reaches the DAC without gating.
- Workaround: None currently. The HAL bridge assigns ADC devices to sequential lanes starting from 0.
- Fix approach: Gate on `_sources[i].read != NULL && (effective device type is hardware ADC)` rather than hardcoded lane indices. Or add a `noiseGateEnabled` flag to `AudioInputSource`.

**Per-Input DSP Only Applied to Lanes 0-1:**
- Symptoms: `pipeline_run_dsp()` at `src/audio_pipeline.cpp` line 245 hardcodes `for (int lane = 0; lane < 2; lane++)`. Lanes 2-7 never have per-input DSP applied, even if DSP bypass is false for those lanes.
- Files: `src/audio_pipeline.cpp` line 245
- Trigger: Register a DSP-enabled input source on lane 2+; DSP is silently skipped.
- Workaround: Use output DSP (post-matrix) instead.
- Fix approach: Change the loop bound to `AUDIO_PIPELINE_MAX_INPUTS` and check `_dspBypass[lane]` for each.

---

## Security Considerations

**Web Interface Served Over Plain HTTP:**
- Risk: The web server runs on port 80 with no TLS. Credentials (session cookies, password change requests, PBKDF2-hashed passwords) are transmitted in plaintext on the local network.
- Files: `src/main.cpp` (WebServer initialization), `src/config.h` line 124
- Current mitigation: WiFiClientSecure used for outbound connections (OTA). Inbound is unencrypted. `SameSite=Strict` + `HttpOnly` on cookies. WS tokens are single-use with 60s TTL. All mitigations are defense-in-depth but do not address the core transport issue.
- Recommendations: Consider self-signed TLS certificate for the web server. ESP32-P4 has the CPU budget for mbedTLS. At minimum, document the HTTP-only limitation.

**WiFi AP Password Remains Static Default:**
- Risk: `DEFAULT_AP_PASSWORD "12345678"` in `src/config.h` line 177 is used for the WiFi AP. Unlike the web password (now per-device random), the AP password is still a known static value shared across all devices.
- Files: `src/config.h` line 177, `src/app_state.h` line 96
- Current mitigation: AP mode is disabled by default. The AP SSID contains the device serial number, making the device uniquely identifiable.
- Recommendations: Generate a unique AP password per device (e.g., from eFuse MAC) on first boot, display it on TFT alongside the web password.

---

## Performance Bottlenecks

**`pipeline_to_float()` Noise Gate Copies to PSRAM on Every Open Buffer:**
- Problem: When the noise gate is open (audio present), `pipeline_to_float()` copies the entire float buffer (256 samples x 2 channels x 4 bytes = 2KB) to PSRAM `_gatePrevL[i]` / `_gatePrevR[i]` at `src/audio_pipeline.cpp` lines 219-220. This happens every 5.33ms while audio is playing. PSRAM writes on ESP32-P4 are slower than internal SRAM.
- Files: `src/audio_pipeline.cpp` lines 219-220
- Cause: The fade-out algorithm needs the last "good" buffer before the gate closes. A copy is the simplest approach.
- Improvement path: Use a double-buffer ring (ping-pong) and swap pointers instead of copying. Only copy when the gate actually closes.

**WS Broadcast Rebuilds Full JSON on Every Dirty Flag:**
- Problem: `broadcastHardwareStats()`, `broadcastAudioState()`, and `broadcastHalDeviceList()` in `src/websocket_handler.cpp` rebuild complete JSON documents from scratch each call. The HAL device list serializes all 16 device slots (descriptor, config, state, capabilities) even when only one device changed.
- Files: `src/websocket_handler.cpp`
- Cause: Dirty-flag pattern gates the frequency but not the scope of broadcasts.
- Improvement path: Cache serialized JSON for stable sub-objects. Only re-serialize the changed device (identified by `markHalDeviceDirty` slot parameter, which currently doesn't exist).

---

## Fragile Areas

**DAC Toggle Deferred-Toggle Mechanism:**
- Files: `src/app_state.h` lines 407-417, `src/main.cpp` lines 1205-1225
- Why fragile: DAC init/deinit is deferred to the main loop via volatile `int8_t` flags (`_pendingDacToggle`, `_pendingEs8311Toggle`). The validated setters `requestDacToggle()` / `requestEs8311Toggle()` hardened this in Phase 2, but the mechanism is still implicit -- any code path that needs to toggle DAC output must know to use the deferred flag, not call `dev->deinit()` directly. The `hal_api.cpp` `hal_apply_config()` function checks `HAL_CAP_DAC_PATH` and uses the deferred path, but a future contributor adding a new toggle endpoint could bypass it.
- Safe modification: Always use `appState.requestDacToggle(1)` or `appState.requestEs8311Toggle(-1)`. Never call `dac_output_deinit()` or `dac_secondary_deinit()` from HTTP handlers or MQTT callbacks.
- Test coverage: `test/test_deferred_toggle/` covers the flag mechanism. `test/test_dac_hal/` covers init/deinit but not the deferred dispatch.

**AppState as God Object (553-line Header):**
- Files: `src/app_state.h`
- Why fragile: All application state (60+ fields spanning WiFi, MQTT, OTA, audio, display, DSP, USB, HAL, diagnostics) in a single singleton. Every module includes it, creating a full recompile cascade on any field addition. The singleton has ~1,883 reference sites across 48+ files (documented in MEMORY.md).
- Safe modification: New state fields should be added in the appropriate `#ifdef` section with dirty-flag methods. Do not access AppState from `audio_pipeline_task` (Core 1 real-time) except through the approved volatile fields: `audioPaused`, `audioTaskPausedAck`, `adcEnabled`, `audioSampleRate`.
- Test coverage: AppState is exercised indirectly by most test modules but has no dedicated unit test for dirty-flag semantics or thread-safety invariants.

**FreeRTOS `vTaskSuspendAll()` in Audio Pipeline Source/Sink Management:**
- Files: `src/audio_pipeline.cpp` lines 717, 729, 827, 853
- Why fragile: `vTaskSuspendAll()` is used to atomically install or remove sources/sinks while the audio task is running on Core 1. If any suspended code path calls a FreeRTOS API that would block (e.g., `xSemaphoreTake`), it will trigger an assert. Current callers (HTTP handlers on main loop, bridge callbacks) are safe. This invariant must be maintained.
- Safe modification: Never call `audio_pipeline_set_source()`, `audio_pipeline_remove_source()`, `audio_pipeline_set_sink()`, or `audio_pipeline_remove_sink()` from within an ISR or from a task holding a FreeRTOS semaphore.
- Test coverage: `test/test_sink_slot_api/` covers the API on native (no FreeRTOS scheduling).

**I2S Dual-Master Clock Coordination:**
- Files: `src/i2s_audio.cpp` (functions `i2s_configure_adc1()`, `i2s_configure_adc2()`)
- Why fragile: Both I2S_NUM_0 and I2S_NUM_1 are masters with identical divider chains. Any change to sample rate or clock config for one ADC must be replicated to the other, or BCK frequencies desynchronize and ADC2 produces corrupted data.
- Safe modification: Sample rate changes must update both ADCs atomically using the `audioPaused` semaphore handshake.
- Test coverage: `test/test_i2s_audio/` tests on native with mocked I2S; no hardware clock coordination tests.

**HAL Pipeline Bridge Slot Mapping Tables:**
- Files: `src/hal/hal_pipeline_bridge.cpp` lines 41-42
- Why fragile: `_halSlotToSinkSlot[16]` and `_halSlotToAdcLane[16]` map HAL device slots to pipeline slots/lanes. The mapping is modified by `on_device_available()` and `on_device_removed()`. If a device is removed and re-added, the slot assignment may differ from the original (first-free-slot algorithm). This could cause matrix routing to point at the wrong output.
- Safe modification: When re-enabling a device, verify the sink slot assignment matches the matrix configuration. Consider persisting the HAL-slot-to-sink-slot mapping alongside the matrix.
- Test coverage: `test/test_hal_bridge/` covers add/remove cycles but does not verify matrix routing consistency across re-add.

---

## Scaling Limits

**HAL Device Registry (16 Slots):**
- Current capacity: `HAL_MAX_DEVICES = 16` in `src/hal/hal_types.h` line 9.
- Current usage: ~14 builtin devices registered at boot (PCM5102A, ES8311, PCM1808 x2, NS4150B, TempSensor, Display, Encoder, Buzzer, LED, Relay, Button, SignalGen, plus SigGen HAL + USB Audio HAL being added). Approaching the limit.
- Limit: Adding 3+ more expansion devices (MCP4725, external DAC, etc.) exceeds the cap. Registration silently returns -1.
- Scaling path: Increase `HAL_MAX_DEVICES` to 24 or 32. Also increases `_halSlotToSinkSlot`/`_halSlotToAdcLane` and `HalPinAlloc` arrays. Verify total SRAM cost.

**DSP Preset Slots (32):**
- Current capacity: `DSP_PRESET_MAX_SLOTS = 32` in `src/config.h` line 95.
- Limit: `dspPresetNames[32][21]` in AppState = 672 bytes permanently allocated.
- Scaling path: Load preset names lazily from LittleFS.

**USB Audio Ring Buffer (20ms):**
- Current capacity: `USB_AUDIO_RING_BUFFER_MS = 20` at 48kHz = 960 stereo frames (~7.5KB PSRAM).
- Limit: Underruns if audio pipeline task is preempted >20ms. DMA runway is ~64ms (safe margin).
- Scaling path: Increase `USB_AUDIO_RING_BUFFER_MS` if `usbAudioBufferUnderruns` grows.

**Event Group Bits (24 Total, 15 Assigned):**
- Current capacity: 24 usable bits in `EVT_ANY` (bits 24-31 reserved by FreeRTOS). 15 bits assigned, 9 spare.
- Limit: Each new dirty-flag domain needs a bit. At current growth rate (~2 bits per major feature), headroom is sufficient for 4-5 more features.
- Scaling path: Combine low-frequency events into a single bit (e.g., `EVT_MISC`) with a secondary discriminator flag.

---

## Dependencies at Risk

**PlatformIO ESP32-P4 Platform (Pinned Archive URL):**
- Risk: `platformio.ini` pins the platform to a specific GitHub release archive URL. If deleted or moved, all builds fail.
- Impact: CI and fresh developer builds blocked.
- Migration plan: Mirror the archive to the project's GitHub releases.

**WebSockets Library Requires Patch Script:**
- Risk: `pre:tools/patch_websockets.py` modifies the `links2004/WebSockets` library after install. If the library updates or the patch silently fails, WebSocket behavior breaks on ESP32-P4 RISC-V.
- Files: `platformio.ini` (pre-script), `tools/patch_websockets.py`
- Impact: WebSocket disconnections or frame corruption.
- Migration plan: Fork the library, apply the patch permanently, reference the fork in `lib_deps`.

---

## Missing Critical Features

**HAL Online Device Database Fetch Non-Functional:**
- Problem: The `hal_online_fetch.cpp`/`.h` files have been removed, but the planned "fetch unknown device descriptor from cloud registry" workflow has no replacement. The HAL device DB (`src/hal/hal_device_db.cpp`) only contains builtin entries plus LittleFS persistence.
- Blocks: Auto-discovery of third-party expansion boards without manual config.

**Dynamic Matrix/Web UI Adaptation (Phase 7):**
- Problem: The web UI audio matrix is not yet dynamically driven by HAL device registration. Adding/removing devices does not automatically update the matrix UI or create/remove channel strips.
- Blocks: Full multi-DAC routing from the web UI without firmware-side matrix presets.

---

## Test Coverage Gaps

**`pipelineInputBypass`/`pipelineDspBypass` Array Bounds:**
- What's not tested: No test verifies that `pipeline_sync_flags()` correctly reads bypass flags for lanes 4-7. The hardcoded size-4 arrays cause OOB reads.
- Files: `src/app_state.h` lines 487-488, `src/audio_pipeline.cpp` lines 146-148
- Risk: Undefined behavior on lanes 4+ if adjacent memory changes. Currently masked by struct layout.
- Priority: **Critical** -- this is a real out-of-bounds access bug, not just a coverage gap.

**HAL SigGen and USB Audio Device Lifecycle:**
- What's not tested: `test/test_hal_siggen/` and `test/test_hal_usb_audio/` exist but are new. End-to-end flow through bridge (register HAL device -> bridge assigns lane -> pipeline reads source -> metering updates) is not covered as an integration test.
- Files: `src/hal/hal_siggen.cpp`, `src/hal/hal_usb_audio.cpp`, `src/hal/hal_pipeline_bridge.cpp`
- Risk: Regression in the bridge's source registration path for software devices. The `getInputSource()` virtual dispatch is a new code path.
- Priority: Medium.

**Pipeline Per-Source VU Metering for Non-ADC1 Lanes:**
- What's not tested: `pipeline_update_metering()` at `src/audio_pipeline.cpp` lines 461-493 iterates lanes 1-7 for per-source VU. No test verifies that VU values are correctly written to `_sources[lane].vuL`/`.vuR` for HAL-managed sources (SigGen, USB).
- Files: `src/audio_pipeline.cpp` lines 461-493
- Risk: VU meters for SigGen/USB could show stale -90dBFS values in the web UI without any test catching it.
- Priority: Medium.

**Matrix Persistence with 16x16 Expansion:**
- What's not tested: `audio_pipeline_save_matrix()` / `audio_pipeline_load_matrix()` now serialize a 16x16 matrix. No test verifies backward compatibility when loading an old 8x8 matrix JSON (row count mismatch causes the loader at line 910-911 to reject the file and fall back to defaults).
- Files: `src/audio_pipeline.cpp` lines 877-923
- Risk: Users upgrading from firmware with 8x8 matrix lose their saved routing on first boot.
- Priority: Medium.

**`hal_audio_health_bridge.cpp` Expanded Lanes:**
- What's not tested: Flap guard and diagnostic rules now iterate `AUDIO_PIPELINE_MAX_INPUTS` (8) lanes. Tests in `test/test_audio_health_bridge/` were written for 2-lane scenarios and may not exercise lane indices 2-7.
- Files: `src/hal/hal_audio_health_bridge.cpp` lines 181, 266
- Risk: Off-by-one or uninitialized state for lanes 2-7 could emit spurious diagnostics.
- Priority: Low.

**WebSocket `broadcastAudioChannelMap()` for Dynamic Inputs:**
- What's not tested: The `audioChannelMap` WS message (broadcasting available input/output devices from HAL) has no E2E test verifying the web UI renders channel strips for dynamically added devices.
- Files: `src/websocket_handler.cpp`, `web_src/js/05-audio-tab.js`
- Risk: Web UI could fail to display new HAL-managed audio devices without any test catching it.
- Priority: Low -- blocked by Phase 7 (dynamic matrix/web UI adaptation).

---

*Concerns audit: 2026-03-08*
