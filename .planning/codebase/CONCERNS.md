# Codebase Concerns

**Analysis Date:** 2026-03-10

## Tech Debt

**DEBT-6: Dual Registry (DacRegistry + HalDriverRegistry) Not Yet Unified:**
- Issue: Two parallel device registries remain. `DacRegistry`/`DacDriver`/`HalDacAdapter` are planned for deletion but DEBT-6 has not started. The bridge does not yet own sink lifecycle exclusively.
- Files: `src/hal/hal_driver_registry.h`, `src/hal/hal_driver_registry.cpp`, `src/hal/hal_pipeline_bridge.cpp`, `src/dac_hal.cpp`
- Impact: Circular dependency between bridge and dac_hal; duplicated device lookup code; sink ownership split across modules. Adding new DAC-path devices requires touching both registries.
- Fix approach: Follow `docs-internal/planning/debt-registry-unification.md` -- 4-phase plan. Extract `sink_write_utils.h`, make bridge sole sink owner, delete DacRegistry/DacDriver/HalDacAdapter.

**7 Deprecated WebSocket Handlers Still Present:**
- Issue: `setDacEnabled`, `setDacVolume`, `setDacMute`, `setDacFilter`, `setEs8311Enabled`, `setEs8311Volume`, `setEs8311Mute` -- all log `LOG_W` deprecation warnings but still execute full device-specific logic (~150 lines). These are redundant with the generic `PUT /api/hal/devices` endpoint.
- Files: `src/websocket_handler.cpp` (lines 961-1100)
- Impact: Maintenance burden -- every HAL config change must be duplicated in these legacy handlers. Any future device type (MCP4725, etc.) would bypass them, creating inconsistency.
- Fix approach: Delete the 7 deprecated handlers and their `_halSlotForCompatible()` helper. Frontend already uses HAL API. Add a catch-all LOG_W for unknown legacy commands.

**Deprecated Flat Audio Fields in WS/REST Broadcasts:**
- Issue: `smart_sensing.cpp`, `websocket_handler.cpp` emit both per-ADC arrays (`adc[]`, `adcs[]`) and flat `audioRms1`/`audioVu1`/etc. fields for backward compatibility with pre-v1.14 clients.
- Files: `src/smart_sensing.cpp` (lines 84-94), `src/websocket_handler.cpp` (lines 2153-2156, 2455-2465)
- Impact: ~30 extra JSON fields per broadcast, increased serialization cost and bandwidth. Doubles testing surface for audio level data.
- Fix approach: Remove flat fields in a future major version. Coordinate with any external MQTT/HA consumers that may use the old field names.

**Duplicate `#include "globals.h"` in 14 Files:**
- Issue: 14 source files include `globals.h` twice in succession. The include guard prevents ODR violations, but the duplication is a sign of sloppy automated edits.
- Files: `src/main.cpp` (lines 2-3), `src/websocket_handler.cpp` (lines 5-6), `src/mqtt_handler.cpp` (lines 3-4), `src/mqtt_publish.cpp` (lines 7-8), `src/mqtt_ha_discovery.cpp` (lines 13-14), `src/settings_manager.cpp` (lines 3-4), `src/ota_updater.cpp` (lines 4-5), `src/auth_handler.cpp` (lines 3-4), `src/smart_sensing.cpp` (lines 3-4), `src/wifi_manager.cpp` (lines 3-4), `src/mqtt_task.cpp` (lines 4-5), `src/dsp_api.cpp` (lines 10-11), `src/pipeline_api.cpp` (line 9), `src/dac_hal.cpp` (line 6)
- Impact: Cosmetic only -- no runtime effect. Confusing for new contributors.
- Fix approach: Remove duplicate include lines across all files in a single commit.

**Excessive `extern` Forward Declarations in .cpp Files:**
- Issue: ~40 `extern` function declarations scattered inside `.cpp` files (notably `src/websocket_handler.cpp` with 25+ extern declarations for DSP functions). Functions like `saveDspSettingsDebounced()` are extern-declared in 12 separate locations.
- Files: `src/websocket_handler.cpp` (lines 456-952), `src/mqtt_handler.cpp` (lines 24-31), `src/dsp_api.cpp` (line 283), `src/ota_updater.cpp` (lines 96-97)
- Impact: No header contract -- callers can get signatures wrong silently. Removing or renaming a function does not produce compile errors in all consumers. Fragile to refactoring.
- Fix approach: Move all extern-declared functions into their proper headers (`dsp_api.h`, `dsp_pipeline.h`, `settings_manager.h`). Remove inline extern declarations.

**`websocket_handler.cpp` Monolith (2529 lines):**
- Issue: Single file handles WS authentication, CPU monitoring, all WS command dispatch (~50 message types), all broadcast functions, audio streaming, HAL state broadcasting, DSP config broadcasts, and deferred init state queue.
- Files: `src/websocket_handler.cpp`
- Impact: Difficult to navigate, test, or modify. Any change risks breaking unrelated broadcast logic. Command dispatch is a ~1000-line if/else chain.
- Fix approach: Split into `ws_commands.cpp` (command dispatch), `ws_broadcasts.cpp` (state broadcasts), `ws_audio.cpp` (audio streaming), keeping `websocket_handler.cpp` as the thin event handler.

**`main.cpp` Contains ~90 Route Registrations Inline:**
- Issue: HTTP API route registrations (lines 398-884) are inline lambdas in `setup()`, making `main.cpp` 1448 lines. Many routes are one-line forwarding lambdas (`if (!requireAuth()) return; handleXyz();`).
- Files: `src/main.cpp` (lines 398-884)
- Impact: Adding new API endpoints means editing main.cpp. No central API route table for documentation or testing.
- Fix approach: Extract route registration into `api_routes.cpp` with a single `registerAllApiEndpoints(WebServer& server)` function. Follow the pattern already used by `registerDacApiEndpoints()` and `registerHalApiEndpoints()`.

**MQTT Credentials Stored in Plaintext on LittleFS:**
- Issue: MQTT broker password is stored as plaintext in `/config.json` and legacy `/mqtt_config.txt`. While LittleFS is on-device flash (not network-accessible), a settings export (`/api/settings/export`) excludes the password, but the raw file on flash is unencrypted.
- Files: `src/settings_manager.cpp` (line 158), `src/mqtt_handler.cpp` (lines 89, 118)
- Impact: Physical access to the ESP32 flash could reveal MQTT credentials. Low risk for a local IoT device, but worth noting.
- Fix approach: Encrypt MQTT password at rest using a device-specific key derived from eFuse MAC. Store as `"p1:<encrypted>"` similar to web auth.

**`dsp_api.cpp` Duplicated Auth Pattern:**
- Issue: 40+ `if (!requireAuth()) return;` guards repeated in every handler instead of using middleware.
- Files: `src/dsp_api.cpp` (42 occurrences), `src/dac_api.cpp` (8 occurrences)
- Impact: Risk of forgetting auth check on new endpoints. Verbose code.
- Fix approach: Wrap route registration with an auth-required decorator or use `server.on()` with a shared handler that checks auth before dispatching.

## Known Bugs

**HalCoordState Toggle Queue Has No Atomicity:** FIXED
- Fix: Added `portMUX_TYPE` spinlock via `HAL_COORD_ENTER_CRITICAL()` / `HAL_COORD_EXIT_CRITICAL()` macros in `hal_coord_state.h`, backed by file-static spinlock in new `src/state/hal_coord_state.cpp`. Follows `diag_journal.cpp` pattern. Under `NATIVE_TEST`, macros are no-ops. `volatile` retained on data members for compiler barrier correctness.
- Files: `src/state/hal_coord_state.h` (macros + wrapped mutation methods), `src/state/hal_coord_state.cpp` (portMUX spinlock)
- Scope: `requestDeviceToggle()` and `clearPendingToggles()` are now atomic. Read-only accessors remain lock-free (single-task consumer on Core 1).
- Note: All 9 producer call sites currently run on Core 1 (loopTask), so no real race existed. The spinlock future-proofs against adding a Core 0 producer (e.g., mqtt_task).

**Default AP Password Used as Web Password on First Boot:**
- Symptoms: `WifiState::webPassword` is initialized to `DEFAULT_AP_PASSWORD` (a compile-time constant). On first boot, `initAuth()` generates a random password and stores it in NVS, but any code path that reads `appState.wifi.webPassword` before `initAuth()` completes sees the default.
- Files: `src/state/wifi_state.h` (line 19), `src/auth_handler.cpp` (lines 260-291)
- Trigger: Only on the very first boot, before NVS initialization. The window is milliseconds.
- Workaround: `initAuth()` runs early in `setup()`. The random password is generated and stored before the web server starts.

## Security Considerations

**HTTP Server Has No TLS:**
- Risk: All HTTP traffic (port 80) and WebSocket traffic (port 81) is unencrypted. Session cookies, WS auth tokens, and MQTT configuration changes travel in plaintext over the local network.
- Files: `src/main.cpp` (lines 91-92 -- `WebServer server(80)`, `WebSocketsServer webSocket = WebSocketsServer(81)`)
- Current mitigation: `HttpOnly` + `SameSite=Strict` cookies prevent XSS/CSRF. WS tokens are single-use with 60s TTL. Device is intended for local network use only.
- Recommendations: Add ESP32 HTTPS support using a self-signed certificate. Use `wss://` for WebSocket. Low priority since the device is not internet-facing.

**No CORS Headers on REST API:**
- Risk: Any webpage on the local network can make cross-origin requests to the device API. An attacker on the same network could craft a malicious page that silently changes device settings if the user has an active session.
- Files: `src/main.cpp` (all `server.on()` handlers)
- Current mitigation: `SameSite=Strict` cookies prevent cookie-based CSRF from cross-origin pages. But `fetch()` with `credentials: 'include'` from a same-site context would work.
- Recommendations: Add `Access-Control-Allow-Origin` header restricted to the device's own IP. Add CSRF tokens for state-changing POST/PUT endpoints.

**MQTT Connection Is Unencrypted (Plain TCP):**
- Risk: MQTT credentials (`appState.mqtt.username`, `appState.mqtt.password`) are sent in plaintext over TCP. `PubSubClient` does not support TLS.
- Files: `src/mqtt_handler.cpp` (lines 889-911)
- Current mitigation: MQTT typically runs on a local network. The `PubSubClient` library does not support `WiFiClientSecure`.
- Recommendations: Switch to a TLS-capable MQTT client (e.g., `AsyncMqttClient` with `WiFiClientSecure`) or document the plaintext limitation.

**Session Tokens Are Not Cryptographically Random (UUID Format):**
- Risk: WS auth tokens use UUID format (36 chars), generated by `esp_random()` which is a hardware TRNG on ESP32. This is actually secure, but the 16-slot pool with 60s TTL means only 16 concurrent WS connections can authenticate at once.
- Files: `src/auth_handler.cpp` (lines 25-33)
- Current mitigation: `esp_random()` on ESP32 is TRNG-backed. Pool size is sufficient for typical use (1-3 concurrent clients).
- Recommendations: No action needed. Document the 16-slot limit.

**First-Boot Default Password Displayed on TFT:**
- Risk: The randomly generated first-boot password is shown on the TFT display and serial output. Anyone with physical access to the device during first boot can see it.
- Files: `src/auth_handler.cpp` (line 291 -- `putString("default_pwd", defaultPwd)`)
- Current mitigation: Password is regenerated on factory reset. User is expected to change it.
- Recommendations: Clear the TFT password display after a timeout. Consider requiring physical button confirmation.

## Performance Bottlenecks

**JSON Serialization in Audio Level Broadcasts:**
- Problem: `sendAudioData()` serializes a full `JsonDocument` with per-ADC arrays (8 entries x 9 fields + deprecated flat fields + sink VU data) every audio update cycle. With 8 ADC lanes, this is ~100 JSON fields per broadcast.
- Files: `src/websocket_handler.cpp` (lines 2409-2529)
- Cause: ArduinoJson serialization is CPU-intensive on ESP32. Combined with binary waveform/spectrum data, this runs on Core 1's main loop cycle.
- Improvement path: Use binary WebSocket frames for audio levels (similar to waveform/spectrum). Pre-serialize static portions. Only include active ADC lanes (skip inactive).

**HA Discovery Publishes ~100 MQTT Entities on Connect:**
- Problem: `publishHADiscovery()` publishes ~100 MQTT discovery payloads sequentially on every broker reconnection. Each payload is a full JSON document with device info.
- Files: `src/mqtt_ha_discovery.cpp` (1898 lines, single function)
- Cause: Monolithic function that builds and publishes all entities synchronously. The 1024-byte MQTT buffer size limit means large payloads may be truncated.
- Improvement path: Publish in batches with `vTaskDelay()` between batches. Cache unchanged payloads. Only re-publish entities that changed since last connection.

**LittleFS Operations in Settings Save Path:**
- Problem: `saveSettings()` performs atomic write (write to `.tmp`, rename) which involves two flash erase/write cycles. Called on every settings change including volume slider movements.
- Files: `src/settings_manager.cpp` (60 `LittleFS.open` calls across the file)
- Cause: Debounced save (`saveSettingsDeferred()`) mitigates rapid-fire saves, but the underlying operation is still expensive.
- Improvement path: Batch settings changes and write at most once per second. Use NVS for frequently-changed values (volume, mute).

## Fragile Areas

**I2S Driver Init Order (ADC1/ADC2/TX):**
- Files: `src/i2s_audio.cpp` (lines 358-480)
- Why fragile: The dual-master I2S configuration requires exact init order (ADC2 first when using legacy path, or TX-first for IDF5 full-duplex). MCLK GPIO routing is sensitive to init/deinit ordering -- setting `MCLK=I2S_GPIO_UNUSED` in any re-init path clears the GPIO matrix routing, causing PCM1808 PLL loss.
- Safe modification: Never call `i2s_configure_adc1()` from the audio task loop. Always init TX before RX. Never set MCLK to UNUSED on either TX or RX config when the other channel is active. Test with `audio_periodic_dump()` to verify MCLK continuity.
- Test coverage: `test_i2s_audio/test_audio_rms.cpp` tests RMS computation but not driver init sequences (requires hardware).

**DAC Activate/Deactivate Semaphore Handshake:**
- Files: `src/dac_hal.cpp`, `src/hal/hal_pipeline_bridge.cpp` (line 247)
- Why fragile: DAC deinit sets `appState.audio.paused=true`, then waits for `taskPausedAck` semaphore (50ms timeout). The audio task must observe the flag and give the semaphore. If the audio task is blocked in `i2s_read()` (which can block up to DMA buffer duration), the 50ms timeout may expire, causing the driver to uninstall while the audio task is mid-read.
- Safe modification: Always check that `appState.audio.paused` acknowledgment succeeded before calling `i2s_driver_uninstall()`. Consider increasing timeout to 100ms.
- Test coverage: Semaphore handshake is not unit-tested (requires FreeRTOS).

**HAL Pipeline Bridge Slot Mapping Tables:**
- Files: `src/hal/hal_pipeline_bridge.cpp` (lines 59-60: `_halSlotToSinkSlot[]`, `_halSlotToAdcLane[]`)
- Why fragile: Two static arrays map HAL slots to pipeline sink/source slots. If a device is removed and re-added, the ordinal counting may assign a different slot/lane index, breaking WebSocket clients that cache slot numbers.
- Safe modification: Always test device add/remove/re-add cycles. Verify `_halSlotToSinkSlot` is -1 after removal. Use `hal_pipeline_get_sink_slot()` accessor, never direct array access.
- Test coverage: `test_hal_bridge` covers add/remove with mock counters.

**WebSocket Command Dispatch (1000+ line if/else chain):**
- Files: `src/websocket_handler.cpp` (lines 200-1100)
- Why fragile: All ~50 WS commands are dispatched via a single `if/else if/else if...` chain. Missing `else` or wrong string comparison silently drops commands. No command validation framework -- each handler does its own JSON parsing with different error handling patterns.
- Safe modification: Add new commands at the end of the chain. Always include `LOG_W` for unknown commands. Test via E2E Playwright specs.
- Test coverage: `test_websocket/` covers message parsing; E2E tests cover frontend-to-backend command flow.

**Cross-Core Volatile Fields Without Memory Barriers:**
- Files: `src/state/audio_state.h` (line 64: `volatile bool paused`), `src/hal/hal_device.h` (lines 54-55: `volatile bool _ready`, `volatile HalDeviceState _state`)
- Why fragile: `volatile` prevents compiler reordering but does not issue hardware memory barriers on RISC-V (ESP32-P4). Reads of `_ready` on Core 1 may see stale values from Core 0 writes. In practice, ESP32 cache coherency handles this for simple scalar fields, but the guarantee is implementation-dependent.
- Safe modification: For new cross-core communication, prefer FreeRTOS primitives (event groups, semaphores). Do not extend the volatile pattern to compound data structures.
- Test coverage: Cross-core timing is not testable in native tests.

## Scaling Limits

**WebSocket Client Limit:**
- Current capacity: `MAX_WS_CLIENTS` (default 5 in WebSocketsServer library). Auth tracking arrays are sized to this constant.
- Limit: 5 concurrent authenticated WS clients. The 6th connection attempt succeeds at TCP level but cannot authenticate.
- Scaling path: Increase `MAX_WS_CLIENTS` and update `wsAuthStatus[]`, `wsAuthTimeout[]`, `_audioSubscribed[]`, `_pendingInitState[]` arrays in `src/websocket_handler.cpp`.

**HAL Device Limit:**
- Current capacity: `HAL_MAX_DEVICES=24` devices, `HAL_MAX_PINS=56` pin allocations.
- Limit: 24 registered devices. With 11 builtin devices already registered, only 13 slots remain for expansion modules.
- Scaling path: Increase `HAL_MAX_DEVICES` in `src/hal/hal_types.h`. RAM cost is ~300 bytes per additional slot (config + retry state + pointer).

**Audio Pipeline Fixed Dimensions:**
- Current capacity: `AUDIO_PIPELINE_MAX_INPUTS=8` lanes, `AUDIO_PIPELINE_MATRIX_SIZE=16`, `AUDIO_OUT_MAX_SINKS=8`.
- Limit: 8 input sources, 8 output sinks, 16x16 routing matrix. Adding a 9th ADC or DAC requires recompilation.
- Scaling path: Defined as compile-time constants in `src/config.h` and `src/audio_pipeline.h`. Increase and rebuild. DMA buffer arrays scale linearly.

**MQTT PubSubClient Buffer Size:**
- Current capacity: 1024 bytes (`mqttClient.setBufferSize(1024)`).
- Limit: Any single MQTT publish payload >1024 bytes is silently truncated. HA discovery payloads with many entities may hit this limit.
- Scaling path: Increase buffer size in `src/mqtt_handler.cpp` (line 861). Memory cost is static allocation.

## Dependencies at Risk

**PubSubClient MQTT Library:**
- Risk: No TLS support. No async operation. Blocking `connect()` can stall the calling task for up to 1 second (mitigated by pre-connect TCP timeout). Library is mature but unmaintained (last release 2020).
- Impact: Cannot use encrypted MQTT connections. Any MQTT operation on the main loop blocks HTTP/WS serving.
- Migration plan: Move to `AsyncMqttClient` or ESP-IDF native MQTT client (`esp_mqtt`) for TLS + non-blocking operation.

**WebSockets Library (Vendored):**
- Risk: Vendored in `lib/WebSockets/` -- no upstream updates. Library does not support `wss://` (TLS WebSocket).
- Impact: WebSocket connections are always unencrypted. Any upstream security fixes require manual vendor update.
- Migration plan: Consider `ESPAsyncWebServer` + `AsyncWebSocket` for TLS support, or maintain the vendored fork with security patches.

**ArduinoJson v7:**
- Risk: Low -- actively maintained, widely used. JSON serialization is CPU-intensive but unavoidable for REST/WS APIs.
- Impact: Heap allocation for large documents (HA discovery ~100 entities). `JsonDocument` uses dynamic allocation.
- Migration plan: None needed. Monitor heap usage during large serializations.

## Missing Critical Features

**No Rate Limiting on REST API:**
- Problem: The REST API has no rate limiting except for login attempts. An attacker on the local network could flood the API with requests, starving the main loop.
- Blocks: Production deployment on shared networks.

**No Firmware Signature Verification:**
- Problem: OTA updates verify SHA256 hash of the firmware binary but do not verify a cryptographic signature. A MITM on the network (unlikely with TLS to GitHub) could serve a malicious firmware binary with a matching hash.
- Blocks: High-security deployments.

**No Backup/Restore for HAL Device Config:**
- Problem: Settings export/import (`/api/settings/export`, `/api/settings/import`) handles general settings but does not include `/hal_config.json`. Factory reset loses all HAL device configurations.
- Blocks: Easy device migration and recovery.

## Test Coverage Gaps

**I2S Driver Init/Deinit Sequences:**
- What's not tested: The full `i2s_configure_adc1()` / `i2s_configure_adc2()` init, the full-duplex TX+RX create/enable/disable/delete lifecycle, MCLK GPIO routing, and the `audioPaused` semaphore handshake.
- Files: `src/i2s_audio.cpp` (lines 358-530)
- Risk: Changes to I2S init order or GPIO routing could cause silent audio failures (noise floor, PLL loss) that only manifest on hardware.
- Priority: Medium -- hardware-specific, mitigated by manual testing on device.

**WebSocket Command Handler Integration:**
- What's not tested: The full WS command dispatch chain in `websocket_handler.cpp` is not unit-tested end-to-end. Individual message types are tested in `test_websocket/` and `test_websocket_messages/` but the actual `onMessage` dispatch function is too coupled to WebSocketsServer to test natively.
- Files: `src/websocket_handler.cpp` (lines 200-1100)
- Risk: New WS commands could be silently unreachable if placed incorrectly in the if/else chain.
- Priority: Low -- E2E Playwright tests cover the most critical commands via browser interaction.

**MQTT Callback Handler:**
- What's not tested: `mqttCallback()` in `src/mqtt_handler.cpp` (lines 630-845) parses topic strings and dispatches commands. The mock PubSubClient in tests does not simulate incoming MQTT messages through the callback.
- Files: `src/mqtt_handler.cpp` (lines 630-845)
- Risk: MQTT command handling could regress (e.g., wrong topic parsing, missing subscription) without detection.
- Priority: Medium -- MQTT is a key integration point for Home Assistant users.

**HAL Device Discovery on Hardware:**
- What's not tested: I2C bus scan, EEPROM probe, and multi-bus discovery (`hal_discovery.cpp`) are tested with mocks but not with real I2C hardware. The I2C Bus 0 SDIO conflict avoidance is only tested by checking the `WiFi.status()` guard in test mocks.
- Files: `src/hal/hal_discovery.cpp`, `test/test_hal_discovery/`
- Risk: Discovery could fail on new I2C devices or bus configurations without detection.
- Priority: Low -- hardware integration tests are performed manually.

**Settings Migration Paths:**
- What's not tested: The one-time migration from `/dac_config.json` to `/hal_config.json`, legacy `settings.txt` to `/config.json`, and legacy `mqtt_config.txt` to JSON config. These run once per device lifetime.
- Files: `src/dac_hal.cpp` (lines 307-349), `src/settings_manager.cpp` (lines 164-180)
- Risk: A migration failure could leave the device with default settings. Unlikely after initial deployment but untested for edge cases (corrupt files, partial writes).
- Priority: Low -- migrations are one-time operations.

**Output DSP Hot-Path Under Load:**
- What's not tested: `output_dsp_process()` under concurrent config swap + audio processing. The double-buffer swap mutex has a 5ms timeout -- behavior under sustained load (all 8 output channels with 12 DSP stages each) is not stress-tested.
- Files: `src/output_dsp.cpp` (lines 170-245)
- Risk: Swap failures under heavy DSP load could cause audio glitches.
- Priority: Medium -- DSP load is configurable and typically moderate.

---

*Concerns audit: 2026-03-10*
