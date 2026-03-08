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
- **[RESOLVED - Concerns v3 Phase 1] Bypass array OOB** -- `pipelineInputBypass` and `pipelineDspBypass` arrays expanded to `[AUDIO_PIPELINE_MAX_INPUTS]` (8), default init covers all lanes.
- **[RESOLVED - Concerns v3 Phase 2] Legacy DAC fallback path** -- Legacy `dac_output_is_ready()` / `dac_output_write()` fallback in `pipeline_write_output()` replaced with silent output + `LOG_W` when `_sinkCount == 0`.
- **[RESOLVED - Concerns v3 Phase 2] Legacy settings paths guarded** -- JSON config version check added: skips legacy `settings.txt` / `mqtt_config.txt` loading entirely when `/config.json` v1+ is present on disk.
- **[RESOLVED - Concerns v3 Phase 3] Hardcoded lane 0-1 broadcasts** -- Flat `audioAdc[0]`/`audioAdc[1]` fields deprecated as of v1.14. WS broadcasts and MQTT publishes use per-ADC arrays with dynamic iteration over `activeInputCount`. Worst-case health aggregation across all active lanes.
- **[RESOLVED - Concerns v3 Phase 3] Static SRAM allocation** -- Pipeline DMA raw buffers and float working buffers use lazy allocation on `set_source()`/`set_sink()`, not statically allocated at init.
- **[RESOLVED - Concerns v3 Phase 4] HAL_MAX_DEVICES=16 limit** -- Increased to `HAL_MAX_DEVICES=24`. Bridge mapping tables `_halSlotToSinkSlot[]`/`_halSlotToAdcLane[]` updated accordingly.
- **[RESOLVED - Concerns v3 Phase 1] Noise gate hardcoded to lanes 0-1** -- Noise gate now uses `AudioInputSource.isHardwareAdc` flag instead of hardcoded lane indices. Any hardware ADC on any lane gets noise gating.
- **[RESOLVED - Concerns v3 Phase 1] Per-input DSP hardcoded to lanes 0-1** -- DSP loop iterates `AUDIO_PIPELINE_MAX_INPUTS` (8), applying per-input DSP to all active lanes.

---

## Tech Debt

**Legacy Flat AppState ADC Accessors:**
- Issue: This concern was present in the previous audit and remains. The alias block for `audioRmsLeft`, `audioVuLeft`, etc. has been removed, but 8 `AdcState audioAdc[AUDIO_PIPELINE_MAX_INPUTS]` elements (each with 17 fields) are allocated even though typically only lanes 0-1 are used for hardware ADCs. The comment at line 206 still references `{true, true}` initializer for 2 ADCs.
- Files: `src/app_state.h` lines 188, 206
- Impact: Minor memory waste (~1.6KB for unused AdcState entries). Health diagnostics now iterate `activeInputCount`, but some legacy paths may still reference specific lanes.
- Fix approach: Verify all broadcast/publish paths iterate dynamically. Remove hardcoded lane references in remaining edge cases.

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

**16x16 Matrix Gain Table (1KB):**
- Issue: `_matrixGain[16][16]` at `src/audio_pipeline.cpp` line 63 is a 16x16 float matrix (1024 bytes). The actual pipeline uses 8 stereo inputs (16 mono channels) to 16 output channels, but the inner loop at line 290 iterates all 256 gain cells per output channel per buffer.
- Files: `src/audio_pipeline.cpp` lines 63, 287-296
- Impact: 256 float comparisons per output channel per 5.33ms buffer. With 16 output channels, that is 4096 gain checks per buffer iteration. Most cells are 0.0f and short-circuit, but the iteration overhead remains.
- Fix approach: Maintain a sparse representation (e.g., per-output list of non-zero input gains). This eliminates the O(N^2) scan.

---

## Known Bugs

*No critical bugs currently open. Previously known bugs (noise gate hardcoded to lanes 0-1, per-input DSP limited to lanes 0-1) have been resolved -- see Resolved Items above.*

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
- Problem: `broadcastHardwareStats()`, `broadcastAudioState()`, and `broadcastHalDeviceList()` in `src/websocket_handler.cpp` rebuild complete JSON documents from scratch each call. The HAL device list serializes all 24 device slots (descriptor, config, state, capabilities) even when only one device changed.
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
- Why fragile: `_halSlotToSinkSlot[HAL_MAX_DEVICES]` and `_halSlotToAdcLane[HAL_MAX_DEVICES]` (24 entries) map HAL device slots to pipeline slots/lanes. The mapping is modified by `on_device_available()` and `on_device_removed()`. If a device is removed and re-added, the slot assignment may differ from the original (first-free-slot algorithm). This could cause matrix routing to point at the wrong output.
- Safe modification: When re-enabling a device, verify the sink slot assignment matches the matrix configuration. Consider persisting the HAL-slot-to-sink-slot mapping alongside the matrix.
- Test coverage: `test/test_hal_bridge/` covers add/remove cycles but does not verify matrix routing consistency across re-add.

---

## Scaling Limits

**HAL Device Registry (24 Slots) [RESOLVED]:**
- Current capacity: `HAL_MAX_DEVICES = 24` in `src/hal/hal_types.h` (increased from 16).
- Current usage: ~14 builtin devices registered at boot. Comfortable headroom for 10 more expansion devices.
- Note: Bridge mapping tables `_halSlotToSinkSlot[]`/`_halSlotToAdcLane[]` and `HalPinAlloc` arrays updated to match.

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

**PlatformIO ESP32-P4 Platform (Pinned Archive URL) [DEFERRED]:**
- Risk: `platformio.ini` pins the platform to a specific GitHub release archive URL. If deleted or moved, all builds fail.
- Impact: CI and fresh developer builds blocked.
- Mitigation: PlatformIO caching mitigates CI impact. Manual mirror to project's GitHub releases is optional.
- Status: Deferred -- PlatformIO caching provides adequate protection; manual archive mirror remains optional future work.

**WebSockets Library Requires Patch Script [RESOLVED]:**
- Resolution: Library vendored in `lib/WebSockets/` with patches applied permanently. Patch script deleted. No longer depends on external lib_deps registry or runtime patching.

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

**`pipelineInputBypass`/`pipelineDspBypass` Array Bounds [RESOLVED]:**
- Resolution: Arrays expanded to `[AUDIO_PIPELINE_MAX_INPUTS]` (8). `test_pipeline_bounds` module added to verify `pipeline_sync_flags()` reads bypass flags correctly for all 8 lanes. Default init covers all lanes.

**HAL SigGen and USB Audio Device Lifecycle [RESOLVED]:**
- Resolution: `isHardwareAdc` tests added to `test_hal_siggen` and `test_hal_usb_audio`. Tests verify that software sources (SigGen, USB) set `isHardwareAdc=false`, ensuring noise gate correctly skips them.

**Pipeline Per-Source VU Metering for Non-ADC1 Lanes [RESOLVED]:**
- Resolution: Lane 2 VU metering test added. Verifies that `pipeline_update_metering()` correctly writes VU values to `_sources[lane].vuL`/`.vuR` for HAL-managed sources beyond lane 0-1.

**Matrix Persistence with 16x16 Expansion [RESOLVED]:**
- Resolution: Backward compatibility migration implemented and tested. Loading an old 8x8 matrix JSON places it in the top-left corner of the 16x16 matrix with zero-fill for remaining cells.

**`hal_audio_health_bridge.cpp` Expanded Lanes [RESOLVED]:**
- Resolution: Lanes 2 and 7 added to health bridge test scenarios. Verifies flap guard and diagnostic rules work correctly for lane indices beyond 0-1.

**WebSocket `broadcastAudioChannelMap()` for Dynamic Inputs:**
- What's not tested: The `audioChannelMap` WS message (broadcasting available input/output devices from HAL) has no E2E test verifying the web UI renders channel strips for dynamically added devices.
- Files: `src/websocket_handler.cpp`, `web_src/js/05-audio-tab.js`
- Risk: Web UI could fail to display new HAL-managed audio devices without any test catching it.
- Priority: Low -- blocked by Phase 7 (dynamic matrix/web UI adaptation).

---

*Concerns audit: 2026-03-08*
