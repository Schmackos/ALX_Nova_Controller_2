# Release Notes

## Version 1.8.8

## Technical Details
- [2026-02-19] chore: bump version to 1.8.8 (`b63d01b`)

## Improvements
- None

## Bug Fixes
- [2026-02-20] fix: move TWDT reconfigure to top of setup() before WiFi init

esp_task_wdt_reconfigure() was called after connectToStoredNetworks(),
which meant the WiFi/lwIP task (tiT) could auto-subscribe to the old
5-second WDT during the connection attempt and then log spurious
'task not found' errors once the reconfigure cleared the subscriber list.

Moving reconfigure() to the very top of setup() (before any WiFi, GUI,
or audio task creation) ensures all tasks start against the correct
30-second timeout with idle_core_mask=0 from the beginning. (`7e2b30e`)


## Technical Details

## Breaking Changes
None

## Known Issues
- None

## Version 1.8.7

## New Features
- [2026-02-19] feat: add JS syntax validation to web asset build script (Item F)

Add validateJsSyntax() to tools/build_web_assets.js that extracts every
<script>...</script> block from the HTML assets, writes each to a temp
file, and runs `node --check` (V8 syntax check, zero deps) before the
gzip step. Build exits with code 1 and a descriptive error (asset name,
block number, V8 message with HTML-offset line number) on failure.

Add tools/.eslintrc.json with minimal browser/ES2020 config for optional
manual ESLint runs. (`8a0cf20`)
- [2026-02-19] feat: WiFi RX watchdog — force reconnect when heap critical >2min

When ESP32 internal SRAM heap stays below ~40KB for more than 2 minutes,
the WiFi stack's RX buffer allocations silently fail, dropping all incoming
packets (HTTP, WebSocket, ping) while outgoing MQTT publishes continue to
work. The watchdog detects this condition and forces a WiFi disconnect;
wifi_manager then reconnects with a fresh allocation pool.

- src/wifi_watchdog.h: pure inline helper wifi_watchdog_should_reconnect()
  (heapCritical + connected + !otaInProgress + criticalDuration >= 120s)
- src/app_state.h: add wifiRxWatchdogRecoveries (uint32_t) and
  heapCriticalSinceMs (unsigned long) to Heap Health section
- src/main.cpp: include wifi_watchdog.h; extend heap monitor block with
  heapCriticalSinceMs tracking and watchdog trigger (WiFi.disconnect())
- src/websocket_handler.cpp: emit wifiRxWatchdogRecoveries in sendHardwareStats()
- src/mqtt_handler.cpp: publish system/wifi_rx_watchdog_recoveries on heartbeat;
  add HA discovery sensor (state_class: total_increasing, mdi:wifi-refresh)
- test/test_wifi_watchdog/test_wifi_watchdog.cpp: 5 unit tests for the pure
  helper function (not critical, under 2min, at 2min, OTA guard, not connected) (`ac8c0ad`)
- [2026-02-19] feat: WiFi RX watchdog — force reconnect when heap critical >2min

When ESP32 internal SRAM heap stays below ~40KB for more than 2 minutes,
the WiFi stack's RX buffer allocations silently fail, dropping all incoming
packets (HTTP, WebSocket, ping) while outgoing MQTT publishes continue to
work. The watchdog detects this condition and forces a WiFi disconnect;
wifi_manager then reconnects with a fresh allocation pool.

- src/wifi_watchdog.h: pure inline helper wifi_watchdog_should_reconnect()
  (heapCritical + connected + !otaInProgress + criticalDuration >= 120s)
- src/app_state.h: add wifiRxWatchdogRecoveries (uint32_t) and
  heapCriticalSinceMs (unsigned long) to Heap Health section
- src/main.cpp: include wifi_watchdog.h; extend heap monitor block with
  heapCriticalSinceMs tracking and watchdog trigger (WiFi.disconnect())
- src/websocket_handler.cpp: emit wifiRxWatchdogRecoveries in sendHardwareStats()
- src/mqtt_handler.cpp: publish system/wifi_rx_watchdog_recoveries on heartbeat;
  add HA discovery sensor (state_class: total_increasing, mdi:wifi-refresh)
- test/test_wifi_watchdog/test_wifi_watchdog.cpp: 5 unit tests for the pure
  helper function (not critical, under 2min, at 2min, OTA guard, not connected) (`f3b3fb7`)
- [2026-02-19] feat: add user-configurable device name (AP SSID override)

- AppState: add customDeviceName field (persisted, max 32 chars)
- settings_manager: expand lines[] to 35, persist customDeviceName at
  line index 31, include in export/import JSON as settings.customDeviceName
- main.cpp: apply customDeviceName to apSSID after loadSettings(); falls
  back to ALX-Nova-<serial> when blank
- websocket_handler: handle setDeviceName WS command, update apSSID
  live and broadcast wifiStatus to all clients
- wifi_manager: include customDeviceName in buildWiFiStatusJson so web
  UI can pre-populate the field on load
- mqtt_handler: subscribe settings/device_name/set, handle command,
  publish HA text entity via homeassistant/text/.../device_name/config
- web_pages: add Device Name text input in Access Point card with
  debounced save via saveDeviceName(); load value from wifiStatus WS
- test_settings: add 5 tests covering empty fallback, custom name usage,
  32-char truncation, save/load roundtrip, and clear-to-empty (`2601f2e`)
- [2026-02-19] feat: add user-configurable device name (AP SSID override)

- AppState: add customDeviceName field (persisted, max 32 chars)
- settings_manager: expand lines[] to 35, persist customDeviceName at
  line index 31, include in export/import JSON as settings.customDeviceName
- main.cpp: apply customDeviceName to apSSID after loadSettings(); falls
  back to ALX-Nova-<serial> when blank
- websocket_handler: handle setDeviceName WS command, update apSSID
  live and broadcast wifiStatus to all clients
- wifi_manager: include customDeviceName in buildWiFiStatusJson so web
  UI can pre-populate the field on load
- mqtt_handler: subscribe settings/device_name/set, handle command,
  publish HA text entity via homeassistant/text/.../device_name/config
- web_pages: add Device Name text input in Access Point card with
  debounced save via saveDeviceName(); load value from wifiStatus WS
- test_settings: add 5 tests covering empty fallback, custom name usage,
  32-char truncation, save/load roundtrip, and clear-to-empty (`e098abf`)
- [2026-02-19] feat: add user-configurable device name (AP SSID override)

- AppState: add customDeviceName field (persisted, max 32 chars)
- settings_manager: expand lines[] to 35, persist customDeviceName at
  line index 31, include in export/import JSON as settings.customDeviceName
- main.cpp: apply customDeviceName to apSSID after loadSettings(); falls
  back to ALX-Nova-<serial> when blank
- websocket_handler: handle setDeviceName WS command, update apSSID
  live and broadcast wifiStatus to all clients
- wifi_manager: include customDeviceName in buildWiFiStatusJson so web
  UI can pre-populate the field on load
- mqtt_handler: subscribe settings/device_name/set, handle command,
  publish HA text entity via homeassistant/text/.../device_name/config
- web_pages: add Device Name text input in Access Point card with
  debounced save via saveDeviceName(); load value from wifiStatus WS
- test_settings: add 5 tests covering empty fallback, custom name usage,
  32-char truncation, save/load roundtrip, and clear-to-empty (`bfb005a`)
- [2026-02-19] feat: add FreeRTOS stack overflow hook with flag-pattern handler

- Define vApplicationStackOverflowHook (extern C weak symbol) in main.cpp
  before setup(); stores detected flag + task name in AppState (safe for
  exception/interrupt context — no heap, no Serial)
- Add volatile stackOverflowDetected + char stackOverflowTaskName[16] to
  AppState in the audio/volatile field section
- loop() checks the flag each iteration, clears it, logs LOG_E, and records
  "stack_overflow:<taskName>" to the crash log ring buffer
- Add test/test_stack_overflow/ with 5 native tests covering: flag set,
  15-char truncation, null name handling, loop handler clears flag, and
  loop handler no-op when flag not set (`9cdaf49`)
- [2026-02-19] feat: replace single pending_pattern with 3-slot circular buzzer queue (Item D)

Replace the volatile BuzzerPattern pending_pattern singleton with a
circular queue of BUZZ_QUEUE_SIZE (3) slots:
- buzzer_play() enqueues patterns; when full, drops oldest and
  increments _buzzQueueDropped
- buzzer_update() dequeues only when not currently playing (FIFO order)
- portMUX_TYPE _buzzQueueMux guards queue head/tail/count in
  buzzer_play() (guarded by #ifndef UNIT_TEST for native builds)
- buzzer_init() resets queue state on startup

Adds 5 new tests to test_buzzer_handler.cpp:
- test_buzz_queue_3_in_order: FIFO order across 3 dequeue cycles
- test_buzz_queue_4th_drops_oldest: overflow evicts head, count stays 3
- test_buzz_queue_empty_returns_none: empty queue causes no playback
- test_buzz_queue_drop_counter: each overflow increments _buzzQueueDropped
- test_buzz_queue_sequential_playback: full play->complete->dequeue cycle

Updates test 6 (formerly test_new_pattern_overrides) to reflect new
queue semantics: second enqueued pattern waits for current to finish. (`e9cf3f4`)
- [2026-02-19] feat: post-connect roaming and captive portal auto-open

Post-connect roaming: after connecting to a non-hidden network, scan
for a better BSSID up to 3 times (every 5 min). Skips scan when RSSI
> -49 dBm (excellent) but still counts toward the limit. Roams via
WiFi.begin(ssid, pass, channel, bssid) when the same SSID is found
with +10 dB improvement. Non-roaming disconnects reset the counter.

Captive portal: replace single handleCaptivePortal() with per-platform
handlers (Android 302, Apple inline portal page, Windows/Firefox/Ubuntu
body-mismatch redirect, Samsung redirect). Expanded probe URL list from
6 to 11. Root route serves AP page without auth in AP-only mode. Added
fallback URL notice (http://192.168.4.1/) to AP setup page.

929/929 native tests pass including 15 new test_wifi_roaming tests. (`71a660f`)
- [2026-02-19] feat: WebSocket session IP binding — reject messages from mismatched IPs

- Add wsClientIP[MAX_WS_CLIENTS] array to track per-client remote IP
- On WStype_CONNECTED: store webSocket.remoteIP(num) in wsClientIP[num]
- On WStype_DISCONNECTED: clear wsClientIP[num] to IPAddress() default
- On auth success: confirm/update stored IP to webSocket.remoteIP(num)
- On every authenticated message: compare current remoteIP with stored IP;
  call webSocket.disconnect(num) and return on mismatch
- Export wsClientIP via websocket_handler.h for testing
- Add 5 unit tests covering match, mismatch, clear, auth update, and
  multi-client independence (`69e8222`)
- [2026-02-19] feat: captive portal for AP mode — all-OS probe URL handlers with host-header guard (`84defa9`)
- [2026-02-19] feat: passive ADC clock sync monitoring via cross-correlation
- [2026-02-19] feat: post-connect roaming — device scans for better AP up to 3 times after connecting (+10 dB threshold, 5 min interval, convergence limit, RSSI excellence gate at -49 dBm)
- [2026-02-19] feat: captive portal auto-open — platform-specific probe handlers for Android/Chrome, Apple iOS/macOS, Windows NCSI, Firefox, Ubuntu NetworkManager, Samsung; fallback URL notice in AP page

Add cross-correlation-based phase detection between the two PCM1808 I2S
ADCs. Every 5 seconds, when both ADCs report AUDIO_OK, the audio task
extracts 64+8 L-channel samples from each ADC buffer, searches for the
peak cross-correlation across a ±8 sample lag window, and stores the
result in a spinlock-protected AdcSyncDiag struct.

Key implementation details:
- compute_adc_sync_diag() is a pure function (no hardware deps, fully
  testable on native platform) that normalizes correlation peak by the
  RMS product of both signals, handling the silence case gracefully
- Sync check runs inside audio_capture_task on Core 1 with no Serial/LOG
  calls (dirty-flag pattern preserved)
- AdcSyncDiag state copied to AppState in detectSignal() for WS/MQTT
- WebSocket: syncOk/syncOffsetSamples/syncCorrelation added to audio
  object in sendHardwareStats()
- MQTT: audio/adc_sync_ok (binary ON/OFF) and audio/adc_sync_offset
  (float, samples) published when numAdcsDetected >= 2
- HA discovery: binary_sensor for sync status, sensor for phase offset
- 8 native unit tests all pass (test_adc_sync) (`d360d81`)
- [2026-02-19] feat: async ring buffer in DebugSerial to prevent I2S DMA blocking

LOG_* calls from FreeRTOS tasks now enqueue messages into a 16-slot ring
buffer (portMUX spinlock) instead of calling Serial.print() directly.
The main loop drains up to 4 entries per call to processQueue(), which is
invoked right after audio_periodic_dump(). On NATIVE_TEST the ring buffer
is compiled out and processQueue()/isQueueEmpty() remain no-ops so all
existing native tests are unaffected. Added test/test_debug_serial/ with 6
tests covering the no-op API contract and log-level filtering behaviour. (`ba7aae5`)

## Technical Details
- [2026-02-19] chore: update release notes (`de0c8ce`)
- [2026-02-19] test: add smoothing alpha and heap constant validation tests

- Add 9 tests to test_smart_sensing validating the EMA smoothing alpha
  formula (1 - exp(-interval/308)) at 20/33/50/100ms intervals and
  arbitrary fallback values, plus monotonicity and formula-vs-hardcoded
  consistency checks
- Add new test_config module with 8 tests verifying heap threshold
  constants (HEAP_CRITICAL, HEAP_WARNING, HEAP_OTA_ABORT, HEAP_TLS_*)
  are positive, in correct hierarchical order, and plausible for ESP32-S3
- Total test count: 954 -> 971 (17 new tests, 0 failures) (`46a2eed`)
- [2026-02-19] chore: update release notes for custom device name (`865ebe2`)
- [2026-02-19] chore: update release notes (`6c19dd4`)
- [2026-02-19] chore: update release notes (`5127a87`)
- [2026-02-19] refactor: add named heap threshold constants to config.h (Item A)

Replace all hardcoded heap size literals with named constants:
- HEAP_CRITICAL_THRESHOLD_BYTES (40000) — WiFi RX drops below this
- HEAP_WARNING_THRESHOLD_BYTES (60000) — early notice threshold
- HEAP_TLS_MIN_THRESHOLD_BYTES (30000) — minimum for TLS handshake
- HEAP_TLS_SECURE_THRESHOLD_BYTES (50000) — threshold for cert validation
- HEAP_OTA_ABORT_THRESHOLD_BYTES (10000) — abort OTA if below this
- HEAP_WIFI_RESERVE_BYTES (40000) — DSP alloc reserve for WiFi/MQTT/HTTP

Updated: src/main.cpp (heap monitor block), src/ota_updater.cpp (5 sites),
src/dsp_pipeline.cpp (delay alloc pre-flight check) (`df7e9e7`)
- [2026-02-19] chore: bump version to 1.8.7 (`660fcf0`)
- [2026-02-19] chore: update release notes (`977cb57`)
- [2026-02-19] test: unit tests for captive portal URL matching and host-header logic (`0d84dfa`)
- [2026-02-19] chore: update release notes (`4ee1dc2`)
- [2026-02-19] chore: update release notes (`1f83f34`)
- [2026-02-19] chore: update release notes (`53b21a5`)
- [2026-02-19] chore: update release notes (`1d21650`)
- [2026-02-19] test: fix all pre-existing test failures in test_dsp_swap and test_emergency_limiter

Five root causes fixed across four test files and one source file:

1. dsp_pipeline.cpp: Remove #ifndef NATIVE_TEST guard around emergency limiter
   call so the limiter actually runs in native test builds. Reset
   _emergencyLimiter state in dsp_init() with samplesSinceTrigger=UINT32_MAX/2
   so it doesn't appear falsely "active" before any signal has been processed.

2. test_dsp_swap: Fix four state-preservation tests that incorrectly set
   runtime state on the inactive config (which gets overwritten during swap).
   Redesigned to use a 2-swap setup: add stage → swap → set state on active →
   add matching stage to (now) inactive → swap → verify. Also fixed
   test_biquad_delay_preserved to set state on active config (both configs
   have matching PEQ stages from dsp_init_state).

3. test_emergency_limiter: Fix test_fast_attack_time to refill the input
   buffer before the second dsp_process_buffer call — the function modifies
   the buffer in-place (re-interleaves output), so the second call was
   processing the limited output (~0.501 of full scale) instead of new
   full-scale input. Fix test_release_time threshold from -3 dBFS to -6 dBFS:
   at -3 dBFS the envelope (tau=100ms) decays below threshold before the
   mid-check at 53ms; at -6 dBFS it stays above threshold until ~69ms then
   releases fully before 213ms.

4. test_dsp, test_peq: Disable emergency limiter in setUp() — the limiter's
   8-sample lookahead delay zeros the first 8 output samples, which breaks
   tests that process small buffers (2-4 frames) or check early samples. (`3d4c16c`)
- [2026-02-19] chore: update release notes (`eb10d60`)
- [2026-02-19] build: auto-rebuild web_pages_gz.cpp via PlatformIO pre-build hook (`f299ed8`)

## Documentation
- [2026-02-19] docs: update codebase concerns map (`5188c0c`)

## Bug Fixes
- [2026-02-19] fix: remove case-sensitive ESP.h include that broke Linux CI builds

`#include <ESP.h>` fails on Linux (case-sensitive filesystem) because
the actual header is `Esp.h`. The include was redundant anyway since
audio_quality.h includes Arduino.h which brings in Esp.h transitively. (`64b9f8b`)
- [2026-02-19] fix: add heapWarning threshold and 10s heap monitoring for earlier fragmentation detection (`c1982d4`)
- [2026-02-19] fix: WinMain linker error in test_emergency_limiter; extract dsp_swap_check_state() for testability

Add #pragma comment(linker, "/SUBSYSTEM:CONSOLE") guard in test_emergency_limiter.cpp to
fix the MinGW undefined reference to WinMain on Windows builds. The pragma is ignored on
Linux/CI (ubuntu-latest) so it does not break the GitHub Actions workflow.

Extract dsp_swap_check_state() as a pure testable function from dsp_swap_config() logic.
The function encodes the mutex/processing/timeout decision without FreeRTOS dependencies,
making the swap timeout path unit-testable natively. Four new tests added to test_dsp_swap
covering the mutex-busy, timeout, still-waiting, and success return codes. (`7de04ee`)
- Use esp_task_wdt_reconfigure() for IDF5 TWDT timeout (30s) instead of defunct build flag
- Fix IDF5/Arduino-ESP32 3.x API compatibility (I2S, FreeRTOS task monitor, TinyUSB UAC2)

---

## Version 1.8.5

## New Features
- [2026-02-19] feat: migrate to pioarduino IDF5 platform and Arduino-ESP32 3.x API

- Switch to pioarduino platform (IDF5 / Arduino-ESP32 3.x) for improved
  ESP32-S3 support and newer ESP-IDF features
- Migrate I2S driver: legacy driver/i2s.h → IDF5 driver/i2s_std.h with
  channel handles (_i2s0_rx, _i2s0_tx, _i2s1_rx) replacing port numbers
- Extract I2S TX management into i2s_audio.cpp: new i2s_audio_enable_tx(),
  i2s_audio_disable_tx(), i2s_audio_write_tx() API; dac_hal.cpp delegates
- Migrate LEDC API: ledcSetup+ledcAttachPin → ledcAttach (pin-based),
  ledcDetachPin → ledcDetach across buzzer, GUI backlight, and signal gen
- Fix task_monitor: use vTaskCoreAffinityGet (IDF5 bitmask) with version
  guard, falling back to xTaskGetAffinity on IDF4
- Refactor platformio.ini: shared [idf5_includes] section eliminates
  duplicate ESP-DSP include paths across environments
- Update Arduino.h mock to cover both 2.x and 3.x LEDC API signatures (`6266003`)
- [2026-02-17] feat: Migrate TFT driver to LovyanGFX and fix OTA TLS/WDT issues

Replace TFT_eSPI with LovyanGFX for DMA double-buffered display flush
on ST7735S. Add Sectigo intermediate cert for GitHub's updated chain,
skip cert validation when NTP hasn't synced, and suspend loopTask WDT
during TLS handshakes to prevent watchdog crashes on boot. (`87ce8e3`)
- [2026-02-17] feat: Migrate TFT driver to LovyanGFX and fix OTA TLS/WDT issues

Replace TFT_eSPI with LovyanGFX for DMA double-buffered display flush
on ST7735S. Add Sectigo intermediate cert for GitHub's updated chain,
skip cert validation when NTP hasn't synced, and suspend loopTask WDT
during TLS handshakes to prevent watchdog crashes on boot. (`2240223`)
- [2026-02-17] feat: Migrate TFT driver to LovyanGFX and fix OTA TLS/WDT issues

Replace TFT_eSPI with LovyanGFX for DMA double-buffered display flush
on ST7735S. Add Sectigo intermediate cert for GitHub's updated chain,
skip cert validation when NTP hasn't synced, and suspend loopTask WDT
during TLS handshakes to prevent watchdog crashes on boot. (`2728e5d`)
- [2026-02-17] feat: Migrate TFT driver to LovyanGFX and fix OTA TLS/WDT issues

Replace TFT_eSPI with LovyanGFX for DMA double-buffered display flush
on ST7735S. Add Sectigo intermediate cert for GitHub's updated chain,
skip cert validation when NTP hasn't synced, and suspend loopTask WDT
during TLS handshakes to prevent watchdog crashes on boot. (`979ce9c`)

## Documentation
- [2026-02-17] docs: create roadmap (8 phases, 25 plans, 49 requirements) (`a1eb065`)
- [2026-02-17] docs: create roadmap (8 phases, 25 plans, 49 requirements) (`7bdd662`)
- [2026-02-17] docs: create roadmap (8 phases, 25 plans, 49 requirements) (`969f1c8`)
- [2026-02-17] docs: create roadmap (8 phases, 25 plans, 49 requirements) (`5bd2c3c`)
- [2026-02-17] docs: define v1 requirements (`4201203`)
- [2026-02-17] docs: complete project research for Spotify Connect integration

Research files covering stack (cspot + dual-framework build), features
(P1/P2/P3 prioritization, anti-features), architecture (SPSC ring buffer
pattern, source priority enum, AppState extensions), and pitfalls (heap
exhaustion, flash overflow, mDNS double-init, I2S race, session death).
Summary synthesizes findings into 7-phase roadmap with gating criteria. (`625eeb2`)
- [2026-02-17] docs: initialize project (`7fa6371`)

## Technical Details
- [2026-02-18] chore: update release notes for build config changes (`f13c978`)
- [2026-02-18] chore: pin platform version, fix display rotation, update build config

- Pin espressif32@6.9.0 and toolchain-xtensa-esp32s3@12.2.0+20230208 for reproducible builds
- Fix lgfx_config.h offset_rotation to 2 (panel mounted inverted, 180° correction)
- Reformat platformio.ini build_flags to tab-indented style
- Update .vscode settings to use built-in PIO core/Python
- Add pioarduino VSCode extension recommendation (`a870a64`)
- [2026-02-18] chore: ignore lib/cspot stub left by OS file lock (`9e1132c`)
- [2026-02-17] chore: add project config (`91b13cd`)
- [2026-02-17] chore: Bump version to 1.8.5 (`d73ba60`)

## Improvements
- [2026-02-17] perf: Optimize TFT display with 40MHz SPI and internal SRAM draw buffer

Increase SPI clock from 27MHz to 40MHz for ~48% faster pixel transfers.
Move LVGL draw buffer from PSRAM to internal SRAM for ~12x faster CPU
rendering access. DMA was tested but reverted due to a TFT_eSPI v2.5.43
bug on ESP32-S3 (invalid register write in dma_end_callback). (`745f6a0`)
- [2026-02-17] perf: Optimize TFT display with 40MHz SPI and internal SRAM draw buffer

Increase SPI clock from 27MHz to 40MHz for ~48% faster pixel transfers.
Move LVGL draw buffer from PSRAM to internal SRAM for ~12x faster CPU
rendering access. DMA was tested but reverted due to a TFT_eSPI v2.5.43
bug on ESP32-S3 (invalid register write in dma_end_callback). (`c74cdb8`)
- [2026-02-17] perf: Optimize TFT display with 40MHz SPI and internal SRAM draw buffer

Increase SPI clock from 27MHz to 40MHz for ~48% faster pixel transfers.
Move LVGL draw buffer from PSRAM to internal SRAM for ~12x faster CPU
rendering access. DMA was tested but reverted due to a TFT_eSPI v2.5.43
bug on ESP32-S3 (invalid register write in dma_end_callback). (`0339cfa`)
- [2026-02-17] perf: Optimize TFT display with 40MHz SPI and internal SRAM draw buffer

Increase SPI clock from 27MHz to 40MHz for ~48% faster pixel transfers.
Move LVGL draw buffer from PSRAM to internal SRAM for ~12x faster CPU
rendering access. DMA was tested but reverted due to a TFT_eSPI v2.5.43
bug on ESP32-S3 (invalid register write in dma_end_callback). (`448fe70`)


## Bug Fixes
- [2026-02-19] fix: use esp_task_wdt_reconfigure for IDF5 TWDT timeout

Use esp_task_wdt_reconfigure() at runtime instead of the
CONFIG_ESP_TASK_WDT_TIMEOUT_S build flag, which has no effect on the
pre-built Arduino IDF5 .a files. Set idle_core_mask=0 to atomically
remove IDLE0 monitoring without corrupting the WDT subscriber linked
list in IDF5.5. Also update PlatformIO IDE settings and release notes. (`5dfbc9d`)
- [2026-02-19] fix: Update APIs for IDF5/Arduino-ESP32 3.x compatibility

- i2s_audio.h: add #include <stddef.h> for size_t declaration
- task_monitor.cpp: replace pxTaskGetNext+xTaskGetAffinity with
  xTaskGetNext(TaskIterator_t*)+xTaskGetCoreID for IDF5 FreeRTOS API
- usb_audio.cpp: rename AUDIO_* UAC2 constants to AUDIO20_* prefix
  and add is_isr=false parameter to usbd_edpt_xfer (TinyUSB API change) (`e29e7ae`)
- [2026-02-19] fix: Use esp_task_wdt_reconfigure for IDF5 TWDT timeout

Use esp_task_wdt_reconfigure() at runtime instead of CONFIG_ESP_TASK_WDT_TIMEOUT_S build
flag (which has no effect on the pre-built Arduino IDF5 .a files). Set idle_core_mask=0
to atomically remove IDLE0 monitoring, avoiding WDT subscriber list corruption in IDF5.5.
- [2026-02-17] fix: Redirect MbedTLS allocations to PSRAM to fix OTA version check TLS failure

OTA version checks failed with HTTP -1 at ~35KB free heap because MbedTLS
I/O buffers (~32KB) competed with I2S DMA for scarce internal SRAM. Instead
of tearing down audio during checks, use GCC --wrap to redirect all MbedTLS
allocations to PSRAM via heap_caps_calloc(MALLOC_CAP_SPIRAM). Audio runs
uninterrupted during OTA checks. Also includes OTA stream-parse optimization,
HTTP fallback for downloads, and I2S driver teardown safety for OTA downloads. (`11751d6`)
- [2026-02-17] fix: Redirect MbedTLS allocations to PSRAM to fix OTA version check TLS failure

OTA version checks failed with HTTP -1 at ~35KB free heap because MbedTLS
I/O buffers (~32KB) competed with I2S DMA for scarce internal SRAM. Instead
of tearing down audio during checks, use GCC --wrap to redirect all MbedTLS
allocations to PSRAM via heap_caps_calloc(MALLOC_CAP_SPIRAM). Audio runs
uninterrupted during OTA checks. Also includes OTA stream-parse optimization,
HTTP fallback for downloads, and I2S driver teardown safety for OTA downloads. (`0b26275`)
- [2026-02-17] fix: Redirect MbedTLS allocations to PSRAM to fix OTA version check TLS failure

OTA version checks failed with HTTP -1 at ~35KB free heap because MbedTLS
I/O buffers (~32KB) competed with I2S DMA for scarce internal SRAM. Instead
of tearing down audio during checks, use GCC --wrap to redirect all MbedTLS
allocations to PSRAM via heap_caps_calloc(MALLOC_CAP_SPIRAM). Audio runs
uninterrupted during OTA checks. Also includes OTA stream-parse optimization,
HTTP fallback for downloads, and I2S driver teardown safety for OTA downloads. (`01dd3a1`)
- [2026-02-17] fix: Redirect MbedTLS allocations to PSRAM to fix OTA version check TLS failure

OTA version checks failed with HTTP -1 at ~35KB free heap because MbedTLS
I/O buffers (~32KB) competed with I2S DMA for scarce internal SRAM. Instead
of tearing down audio during checks, use GCC --wrap to redirect all MbedTLS
allocations to PSRAM via heap_caps_calloc(MALLOC_CAP_SPIRAM). Audio runs
uninterrupted during OTA checks. Also includes OTA stream-parse optimization,
HTTP fallback for downloads, and I2S driver teardown safety for OTA downloads. (`91ceec1`)
- [2026-02-17] fix: Clean up LovyanGFX flush path with DMA and RGB565_SWAPPED format

Remove diagnostic stripe rendering and flush logging from gui_manager.
Switch to pushImageDMA/waitDMA for proper DMA-accelerated display flush
and use RGB565_SWAPPED color format to eliminate manual byte swapping.
Simplify buffer allocation to partial-render mode only. (`166e806`)
- [2026-02-17] fix: Clean up LovyanGFX flush path with DMA and RGB565_SWAPPED format

Remove diagnostic stripe rendering and flush logging from gui_manager.
Switch to pushImageDMA/waitDMA for proper DMA-accelerated display flush
and use RGB565_SWAPPED color format to eliminate manual byte swapping.
Simplify buffer allocation to partial-render mode only. (`43fc7ed`)
- [2026-02-17] fix: Clean up LovyanGFX flush path with DMA and RGB565_SWAPPED format

Remove diagnostic stripe rendering and flush logging from gui_manager.
Switch to pushImageDMA/waitDMA for proper DMA-accelerated display flush
and use RGB565_SWAPPED color format to eliminate manual byte swapping.
Simplify buffer allocation to partial-render mode only. (`51c3ae3`)


## Technical Details

## Breaking Changes
None

## Known Issues
- None

## Version 1.8.4

## Technical Details
- [2026-02-17] chore: Bump version to 1.8.4 (`00beba4`)

## Improvements
- None

## Bug Fixes
- [2026-02-17] fix: Combined dBFS readout now uses smoothed VU values matching per-channel

Combined dBFS was computed from raw unsmoothed RMS (backend) while
per-channel dBFS used smoothed VU values (client-side), causing the
total to appear lower than either channel during signal decay. (`3d72f9a`)
- [2026-02-17] fix: Combined dBFS readout now uses smoothed VU values matching per-channel

Combined dBFS was computed from raw unsmoothed RMS (backend) while
per-channel dBFS used smoothed VU values (client-side), causing the
total to appear lower than either channel during signal decay. (`c989eef`)


## Technical Details

## Breaking Changes
None

## Known Issues
- None

## Version 1.8.3

## Documentation
- [2026-02-16] docs: Add comprehensive testing guide for audio protection system

Create detailed testing guide covering all three phases of the audio
protection implementation (Emergency Limiter, DSP Swap Fixes, Audio
Quality Diagnostics).

Guide includes:
- Prerequisites and hardware setup
- 12 Phase 1 tests (emergency limiter functionality, attack/release)
- 3 Phase 2 tests (DSP swap synchronization, multi-ADC race conditions)
- 12 Phase 3 tests (glitch detection, event correlation, timing analysis)
- Integration test combining all three phases
- Troubleshooting section with common issues and fixes
- Performance baselines and validation checklist
- Unit test instructions and expected results

Located at: docs/development/audio-protection-testing-guide.md (`60cff1b`)
- [2026-02-16] docs: Update release notes with DC block preset and stability improvements (`4814730`)
- [2026-02-16] docs: Update release notes with DC block preset and stability improvements (`c3f23ef`)
- [2026-02-16] docs: Update release notes with DC block preset and stability improvements (`23fc90e`)
- [2026-02-16] docs: Update release notes with project structure reorganization

Document the refactoring of project structure into professional layout
with categorized docs/ subdirectories. (`c08ea6e`)
- [2026-02-16] docs: Update release notes with project structure reorganization

Document the refactoring of project structure into professional layout
with categorized docs/ subdirectories. (`4f5f351`)
- [2026-02-16] docs: Update release notes for v1.8.3 codebase concerns (`e2d9527`)
- [2026-02-16] docs: Update release notes for v1.8.3 codebase concerns (`05fa536`)
- [2026-02-16] docs: Add dual-ADC clock architecture summary comment

Add brief summary comment before i2s_audio_init() documenting the
dual I2S master architecture. Complements the detailed inline
documentation already present at lines 379-418.

Documents:
- Both ADCs use master mode (not slave due to ESP32-S3 DMA issues)
- I2S0 outputs clocks (BCK/WS/MCLK), I2S1 data-only (GPIO9)
- Init order: ADC2 first, then ADC1 (clock source)

Improves code readability for developers unfamiliar with the dual-master
workaround used to avoid ESP32-S3 slave-mode DMA issues. (`d69cf21`)
- [2026-02-16] docs: Add dual-ADC clock architecture summary comment

Add brief summary comment before i2s_audio_init() documenting the
dual I2S master architecture. Complements the detailed inline
documentation already present at lines 379-418.

Documents:
- Both ADCs use master mode (not slave due to ESP32-S3 DMA issues)
- I2S0 outputs clocks (BCK/WS/MCLK), I2S1 data-only (GPIO9)
- Init order: ADC2 first, then ADC1 (clock source)

Improves code readability for developers unfamiliar with the dual-master
workaround used to avoid ESP32-S3 slave-mode DMA issues. (`6c1dbc0`)
- [2026-02-16] docs: Add dual-ADC clock architecture summary comment

Add brief summary comment before i2s_audio_init() documenting the
dual I2S master architecture. Complements the detailed inline
documentation already present at lines 379-418.

Documents:
- Both ADCs use master mode (not slave due to ESP32-S3 DMA issues)
- I2S0 outputs clocks (BCK/WS/MCLK), I2S1 data-only (GPIO9)
- Init order: ADC2 first, then ADC1 (clock source)

Improves code readability for developers unfamiliar with the dual-master
workaround used to avoid ESP32-S3 slave-mode DMA issues. (`984ae22`)
- [2026-02-16] docs: Add dual-ADC clock architecture summary comment

Add brief summary comment before i2s_audio_init() documenting the
dual I2S master architecture. Complements the detailed inline
documentation already present at lines 379-418.

Documents:
- Both ADCs use master mode (not slave due to ESP32-S3 DMA issues)
- I2S0 outputs clocks (BCK/WS/MCLK), I2S1 data-only (GPIO9)
- Init order: ADC2 first, then ADC1 (clock source)

Improves code readability for developers unfamiliar with the dual-master
workaround used to avoid ESP32-S3 slave-mode DMA issues. (`7fa5595`)
- [2026-02-15] docs: map existing codebase (`824ba3e`)

## New Features
- [2026-02-17] feat: Restyle DSP presets with card design, status badge, and save button

Preset items now use styled cards with filled orange active state matching
channel tabs. Click row to load preset, pencil/x icon buttons for rename/delete.
Green "Saved" / red "Modified" badge in card title. Save button appears when
config is modified to overwrite last active preset. (`3a59978`)
- [2026-02-17] feat: Restyle DSP presets with card design, status badge, and save button

Preset items now use styled cards with filled orange active state matching
channel tabs. Click row to load preset, pencil/x icon buttons for rename/delete.
Green "Saved" / red "Modified" badge in card title. Save button appears when
config is modified to overwrite last active preset. (`472a09a`)
- [2026-02-17] feat: Restyle DSP presets with card design, status badge, and save button

Preset items now use styled cards with filled orange active state matching
channel tabs. Click row to load preset, pencil/x icon buttons for rename/delete.
Green "Saved" / red "Modified" badge in card title. Save button appears when
config is modified to overwrite last active preset. (`08c191d`)
- [2026-02-17] feat: Restyle DSP presets with card design, status badge, and save button

Preset items now use styled cards with filled orange active state matching
channel tabs. Click row to load preset, pencil/x icon buttons for rename/delete.
Green "Saved" / red "Modified" badge in card title. Save button appears when
config is modified to overwrite last active preset. (`7e8ac7d`)
- [2026-02-17] feat: Color-coded DSP stage labels and "Copy to" for Additional Processing

Add category-colored type badges on stage cards, colored category labels
in the Add Stage menu, and a "Copy to..." dropdown for copying chain
stages between channels.
- [2026-02-17] feat: Dynamic DSP preset management with unlimited slots

Replace fixed 4-slot preset system with dynamic list supporting up to 32 presets.
Users can now add, rename, and delete presets via intuitive list UI.

Backend changes:
- Increase DSP_PRESET_MAX_SLOTS from 4 to 32
- Add dsp_preset_rename() function for in-place name updates
- Modify dsp_preset_save() to auto-assign slots when slot=-1
- New REST endpoint: POST /api/dsp/presets/rename
- New WebSocket handler: renameDspPreset

Frontend changes:
- Replace horizontal preset bar with vertical list (like Additional Processing)
- Add "+ Add Preset" button with auto-slot-assignment
- Each preset shows Load/Rename/Delete buttons
- Active preset highlighted with .active class
- Dynamic preset count display

Integration updates:
- WebSocket: Send full preset array (index, name, exists) instead of names only
- MQTT: Update discovery to include all existing presets dynamically
- GUI: Update preset cycling to support 0-31 range

All preset operations (create/load/rename/delete) work across web UI, MQTT, and TFT GUI. (`ebef130`)
- [2026-02-17] feat: Dynamic DSP preset management with unlimited slots

Replace fixed 4-slot preset system with dynamic list supporting up to 32 presets.
Users can now add, rename, and delete presets via intuitive list UI.

Backend changes:
- Increase DSP_PRESET_MAX_SLOTS from 4 to 32
- Add dsp_preset_rename() function for in-place name updates
- Modify dsp_preset_save() to auto-assign slots when slot=-1
- New REST endpoint: POST /api/dsp/presets/rename
- New WebSocket handler: renameDspPreset

Frontend changes:
- Replace horizontal preset bar with vertical list (like Additional Processing)
- Add "+ Add Preset" button with auto-slot-assignment
- Each preset shows Load/Rename/Delete buttons
- Active preset highlighted with .active class
- Dynamic preset count display

Integration updates:
- WebSocket: Send full preset array (index, name, exists) instead of names only
- MQTT: Update discovery to include all existing presets dynamically
- GUI: Update preset cycling to support 0-31 range

All preset operations (create/load/rename/delete) work across web UI, MQTT, and TFT GUI. (`d6fb9d8`)
- [2026-02-17] feat: Dynamic DSP preset management with unlimited slots

Replace fixed 4-slot preset system with dynamic list supporting up to 32 presets.
Users can now add, rename, and delete presets via intuitive list UI.

Backend changes:
- Increase DSP_PRESET_MAX_SLOTS from 4 to 32
- Add dsp_preset_rename() function for in-place name updates
- Modify dsp_preset_save() to auto-assign slots when slot=-1
- New REST endpoint: POST /api/dsp/presets/rename
- New WebSocket handler: renameDspPreset

Frontend changes:
- Replace horizontal preset bar with vertical list (like Additional Processing)
- Add "+ Add Preset" button with auto-slot-assignment
- Each preset shows Load/Rename/Delete buttons
- Active preset highlighted with .active class
- Dynamic preset count display

Integration updates:
- WebSocket: Send full preset array (index, name, exists) instead of names only
- MQTT: Update discovery to include all existing presets dynamically
- GUI: Update preset cycling to support 0-31 range

All preset operations (create/load/rename/delete) work across web UI, MQTT, and TFT GUI. (`8348cb9`)
- [2026-02-16] feat: Improve EQ graph usability - equal spacing and increased height

Updated frequency response graph for better usability:

1. Redistributed PEQ default frequencies with perfect logarithmic spacing:
   - Old: [31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000]
   - New: [20, 43, 93, 200, 430, 930, 2000, 4300, 9300, 20000]
   - Equal spacing across full audio spectrum (20 Hz to 20 kHz)
   - Each band is 2.154x the previous (perfect geometric progression)

2. Increased graph canvas height for easier positioning:
   - Mobile (< 768px): 150px → 180px (+30px)
   - Base: 160px → 220px (+60px)
   - Tablet (768px+): 220px → 280px (+60px)
   - Larger clickable area for dragging EQ handles
   - Better vertical resolution for precise gain adjustments

Benefits:
- EQ balls now evenly distributed across frequency spectrum
- No clustering of handles in mid-range
- Easier to position and drag handles with taller graph
- Full coverage from 20 Hz to 20 kHz

Regenerated web_pages_gz.cpp (82.5 KB gzipped) (`eed96eb`)
- [2026-02-16] feat: Implement Phase 3 Audio Quality Diagnostics

Add comprehensive audio quality monitoring system with glitch detection,
timing analysis, and event correlation.

Core Module:
- src/audio_quality.h/.cpp: Glitch detection (4 types: discontinuity, DC
  offset, dropout, overload), timing histogram (20 buckets, 0-20ms),
  event correlation (DSP swap, WiFi, MQTT), memory snapshots (60-entry
  ring buffer)

Integration:
- i2s_audio: Scan buffers after ADC processing (both ADCs)
- dsp_pipeline: Mark DSP swap events for correlation tracking
- wifi_manager: Mark WiFi connect/disconnect events
- main.cpp: Init, 1s memory updates, 5s diagnostics broadcast, dirty
  flag handling
- app_state: audioQualityEnabled, audioQualityGlitchThreshold fields
- websocket_handler: sendAudioQualityState(), sendAudioQualityDiagnostics()
- mqtt_handler: publishMqttAudioQualityState()

Web UI:
- Audio Quality Diagnostics card on Debug tab (toggle, threshold,
  counters, correlation badges)
- Binary phase of testing, TFT GUI shows minimal display on Debug screen
- JS handlers: updateAudioQuality(), applyAudioQualityState(), applyAudioQualityDiag()

Features:
- 32-event glitch ring buffer with throttled logging
- Configurable discontinuity threshold (0.1-1.0, default 0.5)
- Rolling minute glitch counter with auto-decay
- 100ms correlation window for causal event detection
- Default OFF (opt-in to avoid performance impact)
- Real-time diagnostics via WebSocket (5s interval)

Phase 1+2 emergency limiter and DSP swap mutex fixes already committed.
Phase 3 completes the audio quality protection and monitoring suite. (`623af67`)
- [2026-02-16] feat: Implement Phase 3 Audio Quality Diagnostics

Add comprehensive audio quality monitoring system with glitch detection,
timing analysis, and event correlation.

Core Module:
- src/audio_quality.h/.cpp: Glitch detection (4 types: discontinuity, DC
  offset, dropout, overload), timing histogram (20 buckets, 0-20ms),
  event correlation (DSP swap, WiFi, MQTT), memory snapshots (60-entry
  ring buffer)

Integration:
- i2s_audio: Scan buffers after ADC processing (both ADCs)
- dsp_pipeline: Mark DSP swap events for correlation tracking
- wifi_manager: Mark WiFi connect/disconnect events
- main.cpp: Init, 1s memory updates, 5s diagnostics broadcast, dirty
  flag handling
- app_state: audioQualityEnabled, audioQualityGlitchThreshold fields
- websocket_handler: sendAudioQualityState(), sendAudioQualityDiagnostics()
- mqtt_handler: publishMqttAudioQualityState()

Web UI:
- Audio Quality Diagnostics card on Debug tab (toggle, threshold,
  counters, correlation badges)
- Binary phase of testing, TFT GUI shows minimal display on Debug screen
- JS handlers: updateAudioQuality(), applyAudioQualityState(), applyAudioQualityDiag()

Features:
- 32-event glitch ring buffer with throttled logging
- Configurable discontinuity threshold (0.1-1.0, default 0.5)
- Rolling minute glitch counter with auto-decay
- 100ms correlation window for causal event detection
- Default OFF (opt-in to avoid performance impact)
- Real-time diagnostics via WebSocket (5s interval)

Phase 1+2 emergency limiter and DSP swap mutex fixes already committed.
Phase 3 completes the audio quality protection and monitoring suite. (`3fdf2b0`)
- [2026-02-16] feat: Add emergency limiter and DSP swap synchronization fixes

Implement comprehensive audio protection system with two phases:

Phase 1 - Emergency Safety Limiter:
- Brick-wall limiter with 8-sample lookahead buffer
- 0.1ms attack, 100ms release, -0.5 dBFS default threshold
- WebSocket and MQTT integration with 5 HA entities
- Web UI card on Audio tab with real-time metrics
- 10 unit tests

Phase 2 - DSP Swap Synchronization:
- FreeRTOS mutex protection with 5ms timeout
- Multi-ADC coordination via _swapRequested flag
- Changed dsp_swap_config() to return bool for error handling
- Updated 46 call sites with retry logic
- Increased timeout from 50ms to 100ms
- Added swap diagnostics (failures/successes/timing)
- 9 unit tests

Prevents speaker damage and eliminates audio pops/crackles during config changes. (`e276565`)
- [2026-02-16] feat: Add emergency limiter and DSP swap synchronization fixes

Implement comprehensive audio protection system with two phases:

Phase 1 - Emergency Safety Limiter:
- Brick-wall limiter with 8-sample lookahead buffer
- 0.1ms attack, 100ms release, -0.5 dBFS default threshold
- WebSocket and MQTT integration with 5 HA entities
- Web UI card on Audio tab with real-time metrics
- 10 unit tests

Phase 2 - DSP Swap Synchronization:
- FreeRTOS mutex protection with 5ms timeout
- Multi-ADC coordination via _swapRequested flag
- Changed dsp_swap_config() to return bool for error handling
- Updated 46 call sites with retry logic
- Increased timeout from 50ms to 100ms
- Added swap diagnostics (failures/successes/timing)
- 9 unit tests

Prevents speaker damage and eliminates audio pops/crackles during config changes. (`eb5c00f`)
- [2026-02-16] feat: Add emergency limiter and DSP swap synchronization fixes

Implement comprehensive audio protection system with two phases:

Phase 1 - Emergency Safety Limiter:
- Brick-wall limiter with 8-sample lookahead buffer
- 0.1ms attack, 100ms release, -0.5 dBFS default threshold
- WebSocket and MQTT integration with 5 HA entities
- Web UI card on Audio tab with real-time metrics
- 10 unit tests

Phase 2 - DSP Swap Synchronization:
- FreeRTOS mutex protection with 5ms timeout
- Multi-ADC coordination via _swapRequested flag
- Changed dsp_swap_config() to return bool for error handling
- Updated 46 call sites with retry logic
- Increased timeout from 50ms to 100ms
- Added swap diagnostics (failures/successes/timing)
- 9 unit tests

Prevents speaker damage and eliminates audio pops/crackles during config changes. (`71fb6b8`)
- [2026-02-16] feat: Add settings format versioning with VER= prefix

Refactor settings persistence to support format evolution. Load/save
functions now detect and handle versioned settings files.

Changes:
- loadSettings() reads lines into array, detects "VER=2" prefix
- dataStart offset skips version line when present
- Legacy files (no version) still load correctly with dataStart=0
- saveSettings() writes "VER=2" as first line

Backward compatibility: old firmware reads "VER=2" as autoUpdateEnabled
(toInt() returns 0), disabling auto-update on downgrade.

Array-based approach eliminates 28 individual String variables and makes
adding new settings fields trivial. (`a30ea6d`)
- [2026-02-16] feat: Add settings format versioning with VER= prefix

Refactor settings persistence to support format evolution. Load/save
functions now detect and handle versioned settings files.

Changes:
- loadSettings() reads lines into array, detects "VER=2" prefix
- dataStart offset skips version line when present
- Legacy files (no version) still load correctly with dataStart=0
- saveSettings() writes "VER=2" as first line

Backward compatibility: old firmware reads "VER=2" as autoUpdateEnabled
(toInt() returns 0), disabling auto-update on downgrade.

Array-based approach eliminates 28 individual String variables and makes
adding new settings fields trivial. (`e7bd55e`)
- [2026-02-15] feat: Auth security hardening and USB audio init fix

SHA256 password hashing with NVS migration from plaintext, timing-safe
comparison, progressive login rate limiting, session ID log truncation,
SameSite=Strict cookies. USB audio TinyUSB init guarded to prevent
double-init crashes on enable/disable toggle. (`2832507`)

## Improvements
- [2026-02-16] perf: Pool JSON allocations in DSP API handlers

Replace per-handler JsonDocument with static _dspApiDoc to reduce heap
fragmentation. All DSP API handlers now reuse a single document pool.

Safe because:
- All handlers run single-threaded on Core 0 HTTP server
- No handler stores JSON references beyond its return
- serializeJson() completes before handler exits

Changed 26 instances of:
  JsonDocument doc;
To:
  _dspApiDoc.clear();
  JsonDocument &doc = _dspApiDoc;

Reduces ~2KB heap allocation per API call and prevents long-term
fragmentation from repeated JSON alloc/free cycles. (`b0312bf`)
- [2026-02-16] perf: Pool JSON allocations in DSP API handlers

Replace per-handler JsonDocument with static _dspApiDoc to reduce heap
fragmentation. All DSP API handlers now reuse a single document pool.

Safe because:
- All handlers run single-threaded on Core 0 HTTP server
- No handler stores JSON references beyond its return
- serializeJson() completes before handler exits

Changed 26 instances of:
  JsonDocument doc;
To:
  _dspApiDoc.clear();
  JsonDocument &doc = _dspApiDoc;

Reduces ~2KB heap allocation per API call and prevents long-term
fragmentation from repeated JSON alloc/free cycles. (`a7a3372`)
- [2026-02-16] perf: Pool JSON allocations in DSP API handlers

Replace per-handler JsonDocument with static _dspApiDoc to reduce heap
fragmentation. All DSP API handlers now reuse a single document pool.

Safe because:
- All handlers run single-threaded on Core 0 HTTP server
- No handler stores JSON references beyond its return
- serializeJson() completes before handler exits

Changed 26 instances of:
  JsonDocument doc;
To:
  _dspApiDoc.clear();
  JsonDocument &doc = _dspApiDoc;

Reduces ~2KB heap allocation per API call and prevents long-term
fragmentation from repeated JSON alloc/free cycles. (`d0bd171`)
- [2026-02-15] perf: Optimize audio capture task CPU usage on Core 1

Merge 6+ separate buffer loops into 2 passes in process_adc_buffer():
- Pass 1: diagnostics + DC offset + DC-blocking IIR in one loop
- Pass 2: RMS + waveform + FFT ring fill in one loop

Replace double-precision math with float (ESP32-S3 has no double FPU):
- audio_compute_rms(): double sum_sq → float, sqrt → sqrtf
- DC offset tracking: double sum → float

Pre-compute VU/peak exponential coefficients (3 expf calls instead of 12)
and inline VU/peak updates in both silence and active paths.

Fix test VU_DECAY_MS mismatch: test had 650ms but production header has
300ms. Updated test_vu_decay_ramp assertions accordingly. (`de7cf89`)

## Bug Fixes
- [2026-02-17] fix: Native test compilation and WebSocket JSON key naming

Add NATIVE_TEST guards to app_state.h, audio_quality.h, config.h so
tests can include them without ESP32 dependencies. Fix dsp_pipeline.cpp
app_state include order and guard audio_quality call. Fix smart_sensing
JSON keys that had erroneous "appState." prefix. Add new DSP preset
tests and expand DSP swap/emergency limiter test coverage. (`545f8e5`)
- [2026-02-17] fix: Native test compilation and WebSocket JSON key naming

Add NATIVE_TEST guards to app_state.h, audio_quality.h, config.h so
tests can include them without ESP32 dependencies. Fix dsp_pipeline.cpp
app_state include order and guard audio_quality call. Fix smart_sensing
JSON keys that had erroneous "appState." prefix. Add new DSP preset
tests and expand DSP swap/emergency limiter test coverage. (`f27ec56`)
- [2026-02-17] fix: Undefined escapeHtml breaking DSP UI and preset save rejecting auto-assign

escapeHtml() was called in dspRenderPresetList but never defined, causing
a ReferenceError that killed the entire dspHandleDspState handler and
prevented stages, PEQ bands, and presets from rendering. Also fixed
saveDspPreset WS handler rejecting slot=-1 (auto-assign). (`9e959b0`)
- [2026-02-16] fix: Use custom input names in EQ Bands "Copy to..." dropdown

Updated PEQ copy channel dropdown to display user-configured input names
instead of hardcoded L1/R1/L2/R2 labels:
- Added updatePeqCopyToDropdown() function to rebuild dropdown with current names
- Integrated with applyInputNames() for automatic updates when names change
- Dropdown now populated on DSP tab open for immediate correct labeling
- Fallback to default names (L1/R1/L2/R2) when custom names not set

Example: If user renamed inputs to "Sub L", "Sub R", "Top L", "Top R",
the dropdown now shows those names instead of generic L1/R1/L2/R2.

Regenerated web_pages_gz.cpp (82.4 KB gzipped) (`0a05161`)
- [2026-02-16] fix: Apply design system standards to frequency response toggles

Updated frequency response graph toggle buttons to follow design system:
- Replaced inline styles (padding:2px 6px; font-size:10px) with .btn-small class
- Increased button gap from 4px to 8px (matches design system spacing)
- Buttons now use standard sizing: padding 6px 12px, font-size 13px

Benefits:
- Consistent button sizing across UI
- Better touch targets (larger clickable area)
- Follows established design system standards

Regenerated web_pages_gz.cpp (82.9 KB gzipped) (`9590998`)
- [2026-02-16] fix: DSP frame-aligned config swap with busy-wait

Prevent mid-frame config swaps that could cause transient glitches.
Add _processingActive flag to track when dsp_process_buffer() is
executing. dsp_swap_config() now busy-waits (max 50ms) until
processing completes before copying delay lines.

Changes:
- Add volatile _processingActive flag
- Set true at start of dsp_process_buffer(), clear at end
- Clear in all early-return paths (globalBypass, channel bounds)
- dsp_swap_config() waits up to 50ms for !_processingActive

On native tests: vTaskDelay is no-op, _processingActive always false
(single-threaded), so no wait occurs.

Typical wait: 0-1 iterations (~5ms processing << 50ms timeout). (`a6cd50f`)
- [2026-02-16] fix: DSP frame-aligned config swap with busy-wait

Prevent mid-frame config swaps that could cause transient glitches.
Add _processingActive flag to track when dsp_process_buffer() is
executing. dsp_swap_config() now busy-waits (max 50ms) until
processing completes before copying delay lines.

Changes:
- Add volatile _processingActive flag
- Set true at start of dsp_process_buffer(), clear at end
- Clear in all early-return paths (globalBypass, channel bounds)
- dsp_swap_config() waits up to 50ms for !_processingActive

On native tests: vTaskDelay is no-op, _processingActive always false
(single-threaded), so no wait occurs.

Typical wait: 0-1 iterations (~5ms processing << 50ms timeout). (`09615b1`)
- [2026-02-16] fix: DSP frame-aligned config swap with busy-wait

Prevent mid-frame config swaps that could cause transient glitches.
Add _processingActive flag to track when dsp_process_buffer() is
executing. dsp_swap_config() now busy-waits (max 50ms) until
processing completes before copying delay lines.

Changes:
- Add volatile _processingActive flag
- Set true at start of dsp_process_buffer(), clear at end
- Clear in all early-return paths (globalBypass, channel bounds)
- dsp_swap_config() waits up to 50ms for !_processingActive

On native tests: vTaskDelay is no-op, _processingActive always false
(single-threaded), so no wait occurs.

Typical wait: 0-1 iterations (~5ms processing << 50ms timeout). (`a9fcb31`)
- [2026-02-16] fix: DSP frame-aligned config swap with busy-wait

Prevent mid-frame config swaps that could cause transient glitches.
Add _processingActive flag to track when dsp_process_buffer() is
executing. dsp_swap_config() now busy-waits (max 50ms) until
processing completes before copying delay lines.

Changes:
- Add volatile _processingActive flag
- Set true at start of dsp_process_buffer(), clear at end
- Clear in all early-return paths (globalBypass, channel bounds)
- dsp_swap_config() waits up to 50ms for !_processingActive

On native tests: vTaskDelay is no-op, _processingActive always false
(single-threaded), so no wait occurs.

Typical wait: 0-1 iterations (~5ms processing << 50ms timeout). (`f88d6e1`)
- [2026-02-16] fix: DSP frame-aligned config swap with busy-wait

Prevent mid-frame config swaps that could cause transient glitches.
Add _processingActive flag to track when dsp_process_buffer() is
executing. dsp_swap_config() now busy-waits (max 50ms) until
processing completes before copying delay lines.

Changes:
- Add volatile _processingActive flag
- Set true at start of dsp_process_buffer(), clear at end
- Clear in all early-return paths (globalBypass, channel bounds)
- dsp_swap_config() waits up to 50ms for !_processingActive

On native tests: vTaskDelay is no-op, _processingActive always false
(single-threaded), so no wait occurs.

Typical wait: 0-1 iterations (~5ms processing << 50ms timeout). (`9d17f93`)
- [2026-02-16] fix: USB audio Windows driver compatibility via minimal BOS descriptor

Override framework's BOS descriptor that includes Microsoft OS 2.0 with
Compatible ID "WINUSB", which causes Windows to load WinUSB driver instead
of usbaudio2.sys. Our minimal BOS includes only USB 2.0 Extension capability
(required for bcdUSB=0x0210) with no MSOS2/WINUSB descriptors.

Changes:
- Add linker wrapper flag -Wl,--wrap=tud_descriptor_bos_cb to override strong symbol
- Implement __wrap_tud_descriptor_bos_cb() with 12-byte minimal BOS descriptor
- Change clock control to read-only (bmControls: 0x07 → 0x05)
- STALL SET_CUR requests for clock frequency (fixed 48kHz, not configurable)
- Change USB PID 0x4001 → 0x4002 to force Windows to re-enumerate

Fixes Windows incorrectly loading WinUSB driver and failing to recognize UAC2
audio device. Device now correctly appears as "ALX Nova Audio" speaker. (`cb11992`)
- [2026-02-16] fix: USB audio Windows driver compatibility via minimal BOS descriptor

Override framework's BOS descriptor that includes Microsoft OS 2.0 with
Compatible ID "WINUSB", which causes Windows to load WinUSB driver instead
of usbaudio2.sys. Our minimal BOS includes only USB 2.0 Extension capability
(required for bcdUSB=0x0210) with no MSOS2/WINUSB descriptors.

Changes:
- Add linker wrapper flag -Wl,--wrap=tud_descriptor_bos_cb to override strong symbol
- Implement __wrap_tud_descriptor_bos_cb() with 12-byte minimal BOS descriptor
- Change clock control to read-only (bmControls: 0x07 → 0x05)
- STALL SET_CUR requests for clock frequency (fixed 48kHz, not configurable)
- Change USB PID 0x4001 → 0x4002 to force Windows to re-enumerate

Fixes Windows incorrectly loading WinUSB driver and failing to recognize UAC2
audio device. Device now correctly appears as "ALX Nova Audio" speaker. (`ccea3c8`)
- [2026-02-16] fix: USB audio Windows driver compatibility via minimal BOS descriptor

Override framework's BOS descriptor that includes Microsoft OS 2.0 with
Compatible ID "WINUSB", which causes Windows to load WinUSB driver instead
of usbaudio2.sys. Our minimal BOS includes only USB 2.0 Extension capability
(required for bcdUSB=0x0210) with no MSOS2/WINUSB descriptors.

Changes:
- Add linker wrapper flag -Wl,--wrap=tud_descriptor_bos_cb to override strong symbol
- Implement __wrap_tud_descriptor_bos_cb() with 12-byte minimal BOS descriptor
- Change clock control to read-only (bmControls: 0x07 → 0x05)
- STALL SET_CUR requests for clock frequency (fixed 48kHz, not configurable)
- Change USB PID 0x4001 → 0x4002 to force Windows to re-enumerate

Fixes Windows incorrectly loading WinUSB driver and failing to recognize UAC2
audio device. Device now correctly appears as "ALX Nova Audio" speaker. (`d0ecfb7`)
- [2026-02-16] fix: USB audio Windows driver compatibility via minimal BOS descriptor

Override framework's BOS descriptor that includes Microsoft OS 2.0 with
Compatible ID "WINUSB", which causes Windows to load WinUSB driver instead
of usbaudio2.sys. Our minimal BOS includes only USB 2.0 Extension capability
(required for bcdUSB=0x0210) with no MSOS2/WINUSB descriptors.

Changes:
- Add linker wrapper flag -Wl,--wrap=tud_descriptor_bos_cb to override strong symbol
- Implement __wrap_tud_descriptor_bos_cb() with 12-byte minimal BOS descriptor
- Change clock control to read-only (bmControls: 0x07 → 0x05)
- STALL SET_CUR requests for clock frequency (fixed 48kHz, not configurable)
- Change USB PID 0x4001 → 0x4002 to force Windows to re-enumerate

Fixes Windows incorrectly loading WinUSB driver and failing to recognize UAC2
audio device. Device now correctly appears as "ALX Nova Audio" speaker. (`77a7e2a`)
- [2026-02-15] fix: Security hardening, robustness fixes, and 64-bit auth timestamps

Address CONCERNS.md audit findings across security, robustness, and test gaps:

Security:
- Use timingSafeCompare() for session ID lookup in validateSession/removeSession
- Atomic crash log ring buffer validation (reset all on any inconsistency)
- Cap OTA consecutive failure counter at 20 to prevent unbounded growth

Robustness:
- Add recursive mutex to all I2C EEPROM operations (dac_eeprom.cpp)
- Move 3 large DSP API buffers (4-8KB) from stack to PSRAM heap
- Gate WS binary sends (waveform/spectrum) on !heapCritical
- Block dsp_add_stage() when heap is critical

Auth migration:
- Session timestamps: millis() (32-bit ms) → esp_timer_get_time() (64-bit μs)
- SESSION_TIMEOUT → SESSION_TIMEOUT_US (3600000000 μs)
- Rate-limit state updated to 64-bit microseconds

Tests: 790 pass (+36 new: auth timing-safe, OTA backoff cap, crash log corruption) (`f0d0aa2`)
- [2026-02-15] fix: USB audio control transfer handling and clock rate validation

Remove redundant SET_INTERFACE handler (TinyUSB handles it internally
via driver open callback). Add SET_CUR clock rate validation in DATA
stage. Improve control request logging. (`6d0e343`)
- [2026-02-16] fix: USB audio Windows driver compatibility via minimal BOS descriptor

Override framework's BOS descriptor that includes Microsoft OS 2.0 with
Compatible ID "WINUSB", which causes Windows to load WinUSB driver instead
of usbaudio2.sys. Our minimal BOS includes only USB 2.0 Extension capability
(required for bcdUSB=0x0210) with no MSOS2/WINUSB descriptors.

Changes:
- Add linker wrapper flag `-Wl,--wrap=tud_descriptor_bos_cb` to override strong symbol
- Implement __wrap_tud_descriptor_bos_cb() with 12-byte minimal BOS descriptor
- Change clock control to read-only (bmControls: 0x07 → 0x05)
- STALL SET_CUR requests for clock frequency (fixed 48kHz, not configurable)
- Change USB PID 0x4001 → 0x4002 to force Windows to re-enumerate

Fixes Windows incorrectly loading WinUSB driver and failing to recognize UAC2
audio device. Device now correctly appears as "ALX Nova Audio" speaker.
- [2026-02-15] fix: WebSocket session re-validation and default password consistency

Re-validate session on every WS command to catch logout/expiry.
Previously, WS connections persisted after HTTP logout because
wsAuthStatus was only set once during auth handshake.

Also fix isDefaultPassword() to compare against runtime AP password
instead of hardcoded DEFAULT_AP_PASSWORD constant. (`df56634`)


## Technical Details
- [2026-02-17] test: Add regression tests for WebSocket message key format

Adds 4 comprehensive tests to prevent recurrence of the timer display
bug where backend sent "appState.timerDuration" but frontend expected
"timerDuration", causing the web UI to show "-- min" instead of the
actual timer value.

Test coverage:
- test_smart_sensing_websocket_message_keys: Verifies correct JSON keys
  without "appState." prefix and explicit regression checks
- test_websocket_message_consistency: Validates consistent key naming
- test_timer_display_values: Tests timer value formatting scenarios
- test_timer_active_flag: Tests timerActive boolean logic

All 4 tests passing in native environment. (`785954a`)
- [2026-02-16] refactor: Move Crossover Presets from card to Add Stage dropdown modal

Follows same pattern as Baffle Step and THD+N - replaces the standalone
collapsible card with a modal launched from the + Add Stage menu. (`b489d82`)
- [2026-02-16] refactor: Move Crossover Presets from card to Add Stage dropdown modal

Follows same pattern as Baffle Step and THD+N - replaces the standalone
collapsible card with a modal launched from the + Add Stage menu. (`6699633`)
- [2026-02-16] refactor: Move Crossover Presets from card to Add Stage dropdown modal

Follows same pattern as Baffle Step and THD+N - replaces the standalone
collapsible card with a modal launched from the + Add Stage menu. (`03fe031`)
- [2026-02-16] refactor: Move Crossover Presets from card to Add Stage dropdown modal

Follows same pattern as Baffle Step and THD+N - replaces the standalone
collapsible card with a modal launched from the + Add Stage menu. (`0c87620`)
- [2026-02-16] refactor: Move Crossover Presets from card to Add Stage dropdown modal

Follows same pattern as Baffle Step and THD+N - replaces the standalone
collapsible card with a modal launched from the + Add Stage menu. (`fff0208`)
- [2026-02-16] refactor: Reorganize DSP UI - move Baffle Step and THD+N to menu

Moved Baffle Step Correction and THD+N Measurement from standalone
collapsible cards into the DSP menu as modal options:
- Added "Baffle Step..." menu item under Utility category
- Added new "Analysis" category with "THD+N Measurement..." menu item
- Removed 2 standalone collapsible card sections (~60 lines)

Benefits:
- Cleaner DSP tab layout with less scrolling
- Consistent modal-based UI for all DSP utilities
- Better categorization (Analysis vs Processing)

Regenerated web_pages_gz.cpp (82.8 KB gzipped) (`85228f3`)
- [2026-02-16] refactor: Reorganize DSP UI - move Baffle Step and THD+N to menu

Moved Baffle Step Correction and THD+N Measurement from standalone
collapsible cards into the DSP menu as modal options:
- Added "Baffle Step..." menu item under Utility category
- Added new "Analysis" category with "THD+N Measurement..." menu item
- Removed 2 standalone collapsible card sections (~60 lines)

Benefits:
- Cleaner DSP tab layout with less scrolling
- Consistent modal-based UI for all DSP utilities
- Better categorization (Analysis vs Processing)

Regenerated web_pages_gz.cpp (82.8 KB gzipped) (`c5363c0`)
- [2026-02-16] refactor: Remove dead code - test helpers and timing histogram

Removed two categories of unused/incomplete code:

1. Signal generator test helpers (~18 lines)
   - siggen_test_set_active() and siggen_test_set_params()
   - Marked as test helpers but never used by any unit tests
   - Not exported in header file

2. Audio quality timing histogram feature (~60 lines)
   - TimingHistogram struct and implementation removed
   - Function always received 0 for latencyUs (incomplete feature)
   - Removed from WebSocket messages (avgLatencyMs, maxLatencyMs)
   - Removed from MQTT publish (buffer_latency_avg)
   - Removed TODO comments from i2s_audio.cpp

Total dead code removed: ~78 lines across 6 files
No functional impact - features were either unused or non-functional (`af9d3b8`)
- [2026-02-16] refactor: Remove dead code - test helpers and timing histogram

Removed two categories of unused/incomplete code:

1. Signal generator test helpers (~18 lines)
   - siggen_test_set_active() and siggen_test_set_params()
   - Marked as test helpers but never used by any unit tests
   - Not exported in header file

2. Audio quality timing histogram feature (~60 lines)
   - TimingHistogram struct and implementation removed
   - Function always received 0 for latencyUs (incomplete feature)
   - Removed from WebSocket messages (avgLatencyMs, maxLatencyMs)
   - Removed from MQTT publish (buffer_latency_avg)
   - Removed TODO comments from i2s_audio.cpp

Total dead code removed: ~78 lines across 6 files
No functional impact - features were either unused or non-functional (`bdd09fb`)
- [2026-02-16] refactor: Remove incomplete delay alignment feature

Removed ~200 lines of non-functional delay alignment code:
- src/delay_alignment.h/.cpp (153 lines) - cross-correlation algorithm never called
- AppState fields: delayAlignSamples/Ms/Confidence/Valid + dirty flag
- 3 API endpoints: GET /api/dsp/align, POST align/measure, POST align/apply
- WebSocket message handlers: measureDelayAlignment, applyDelayAlignment
- Test include in test_dsp.cpp

Analysis revealed the feature was incomplete - measurement function was never
invoked from main loop or any handler, dirty flag had no consumer, and the
cross-correlation algorithm remained dormant. No functional impact on users. (`351fcf6`)
- [2026-02-16] refactor: Remove incomplete delay alignment feature

Removed ~200 lines of non-functional delay alignment code:
- src/delay_alignment.h/.cpp (153 lines) - cross-correlation algorithm never called
- AppState fields: delayAlignSamples/Ms/Confidence/Valid + dirty flag
- 3 API endpoints: GET /api/dsp/align, POST align/measure, POST align/apply
- WebSocket message handlers: measureDelayAlignment, applyDelayAlignment
- Test include in test_dsp.cpp

Analysis revealed the feature was incomplete - measurement function was never
invoked from main loop or any handler, dirty flag had no consumer, and the
cross-correlation algorithm remained dormant. No functional impact on users. (`ad0ee41`)
- [2026-02-16] refactor: Remove legacy DC Block feature (dead code)

Remove DC Block feature entirely as it's been superseded by DSP highpass
filter stages and was partially broken (no WebSocket handler).

Deleted Code (~200 lines):
- app_state.h: dcBlockEnabled field + dirty flag
- dsp_pipeline.h/cpp: dsp_is_dc_block_enabled(), dsp_enable_dc_block(),
  dsp_disable_dc_block() implementations
- mqtt_handler.h: publishMqttDcBlockState() declaration (already removed)
- settings_manager.cpp: Load/save code replaced with comments
- web_pages.cpp: "+ DC Block" button, dspAddDCBlock() function

Why removed:
- Marked as "legacy - use DSP stage instead" since v1.7.x
- Web UI sent 'setDcBlockEnabled' message with NO WebSocket handler
- Feature was silently broken - toggle did nothing
- Users should use DSP highpass filter stage (10 Hz, Q=0.707) instead

Backward Compatibility:
- Settings file line 30 reserved but writes "0"
- Old configs with dcBlockEnabled are ignored on load
- No migration needed - feature was already broken

Files modified:
- src/app_state.h
- src/dsp_pipeline.h/cpp
- src/settings_manager.cpp
- src/web_pages.cpp
- src/web_pages_gz.cpp (rebuilt) (`a9c0ada`)
- [2026-02-16] refactor: Reorganize project structure into professional layout

- Move 41 documentation files into categorized docs/ subdirectories
- Create docs/{user,development,hardware,planning,archive} structure
- Move build script to tools/ directory
- Delete 8 temporary build/log files
- Update .gitignore for logs and build artifacts
- Standardize naming: UPPERCASE for root, lowercase-with-dashes for docs
- Add MIGRATION.md tracking all file movements

User Documentation (docs/user/):
- Quick start, user manual, feature guides

Development Documentation (docs/development/):
- CI/CD integration, testing, release processes

Hardware Documentation (docs/hardware/):
- PCM1808 integration, hardware diagnostics

Planning Documentation (docs/planning/):
- Feature roadmaps, DSP plans, future work

Archive (docs/archive/):
- Historical summaries and completed features

Root directory now contains only essential files:
README.md, CLAUDE.md, RELEASE_NOTES.md, MIGRATION.md (`b256c9f`)
- [2026-02-16] refactor: Reorganize project structure into professional layout

- Move 41 documentation files into categorized docs/ subdirectories
- Create docs/{user,development,hardware,planning,archive} structure
- Move build script to tools/ directory
- Delete 8 temporary build/log files
- Update .gitignore for logs and build artifacts
- Standardize naming: UPPERCASE for root, lowercase-with-dashes for docs
- Add MIGRATION.md tracking all file movements

User Documentation (docs/user/):
- Quick start, user manual, feature guides

Development Documentation (docs/development/):
- CI/CD integration, testing, release processes

Hardware Documentation (docs/hardware/):
- PCM1808 integration, hardware diagnostics

Planning Documentation (docs/planning/):
- Feature roadmaps, DSP plans, future work

Archive (docs/archive/):
- Historical summaries and completed features

Root directory now contains only essential files:
README.md, CLAUDE.md, RELEASE_NOTES.md, MIGRATION.md (`28b9391`)
- [2026-02-16] refactor: Reorganize project structure into professional layout

- Move 41 documentation files into categorized docs/ subdirectories
- Create docs/{user,development,hardware,planning,archive} structure
- Move build script to tools/ directory
- Delete 8 temporary build/log files
- Update .gitignore for logs and build artifacts
- Standardize naming: UPPERCASE for root, lowercase-with-dashes for docs
- Add MIGRATION.md tracking all file movements

User Documentation (docs/user/):
- Quick start, user manual, feature guides

Development Documentation (docs/development/):
- CI/CD integration, testing, release processes

Hardware Documentation (docs/hardware/):
- PCM1808 integration, hardware diagnostics

Planning Documentation (docs/planning/):
- Feature roadmaps, DSP plans, future work

Archive (docs/archive/):
- Historical summaries and completed features

Root directory now contains only essential files:
README.md, CLAUDE.md, RELEASE_NOTES.md, MIGRATION.md (`a687a00`)
- [2026-02-16] refactor: Remove legacy macro aliases for AppState members

Remove 122 legacy macro definitions and replace all usages with direct
appState.memberName syntax. This eliminates a layer of indirection and
makes code more explicit and maintainable.

Changes:
- Deleted 50 macros from src/main.cpp (lines 79-131)
- Deleted 72 macros from src/app_state.h (lines 519-623)
- Kept only #define appState AppState::getInstance()
- Replaced all bare macro usages across 13+ source files

Affected files:
- src/main.cpp, settings_manager.cpp, mqtt_handler.cpp
- src/smart_sensing.cpp, wifi_manager.cpp, ota_updater.cpp
- src/websocket_handler.cpp, web_pages.cpp
- src/gui/screens/scr_desktop.cpp, scr_mqtt.cpp, scr_home.cpp, scr_control.cpp

Verification: ESP32-S3 firmware builds successfully, all 790 native
tests pass. Zero behavioral change - macros were identity mappings. (`41d83bb`)
- [2026-02-16] refactor: Remove legacy macro aliases for AppState members

Remove 122 legacy macro definitions and replace all usages with direct
appState.memberName syntax. This eliminates a layer of indirection and
makes code more explicit and maintainable.

Changes:
- Deleted 50 macros from src/main.cpp (lines 79-131)
- Deleted 72 macros from src/app_state.h (lines 519-623)
- Kept only #define appState AppState::getInstance()
- Replaced all bare macro usages across 13+ source files

Affected files:
- src/main.cpp, settings_manager.cpp, mqtt_handler.cpp
- src/smart_sensing.cpp, wifi_manager.cpp, ota_updater.cpp
- src/websocket_handler.cpp, web_pages.cpp
- src/gui/screens/scr_desktop.cpp, scr_mqtt.cpp, scr_home.cpp, scr_control.cpp

Verification: ESP32-S3 firmware builds successfully, all 790 native
tests pass. Zero behavioral change - macros were identity mappings. (`fae861c`)
- [2026-02-16] refactor: JSON allocation pooling for DSP API handlers

Replace per-handler JsonDocument stack allocations with a single static
document pool (_dspApiDoc). All DSP API handlers run single-threaded on
Core 0 HTTP server and no handler stores JSON references beyond its
return, making shared document safe.

This eliminates heap fragmentation from repeated 4-8KB allocations during
DSP config operations and prevents stack overflow risk when handlers are
called from nested MQTT/WebSocket contexts.

Changes:
- Add static JsonDocument _dspApiDoc at file scope
- Replace all 'JsonDocument doc;' with '_dspApiDoc.clear(); JsonDocument &doc = _dspApiDoc;'
- Updated in loadRoutingMatrix, saveRoutingMatrix, loadDspSettings, dsp_preset_save, dsp_preset_load, and all API endpoint handlers

Tests: 156 DSP tests pass, no behavioral changes (`9b95943`)
- [2026-02-16] refactor: JSON allocation pooling for DSP API handlers

Replace per-handler JsonDocument stack allocations with a single static
document pool (_dspApiDoc). All DSP API handlers run single-threaded on
Core 0 HTTP server and no handler stores JSON references beyond its
return, making shared document safe.

This eliminates heap fragmentation from repeated 4-8KB allocations during
DSP config operations and prevents stack overflow risk when handlers are
called from nested MQTT/WebSocket contexts.

Changes:
- Add static JsonDocument _dspApiDoc at file scope
- Replace all 'JsonDocument doc;' with '_dspApiDoc.clear(); JsonDocument &doc = _dspApiDoc;'
- Updated in loadRoutingMatrix, saveRoutingMatrix, loadDspSettings, dsp_preset_save, dsp_preset_load, and all API endpoint handlers

Tests: 156 DSP tests pass, no behavioral changes (`ff1bfc6`)
- [2026-02-16] refactor: JSON allocation pooling for DSP API handlers

Replace per-handler JsonDocument stack allocations with a single static
document pool (_dspApiDoc). All DSP API handlers run single-threaded on
Core 0 HTTP server and no handler stores JSON references beyond its
return, making shared document safe.

This eliminates heap fragmentation from repeated 4-8KB allocations during
DSP config operations and prevents stack overflow risk when handlers are
called from nested MQTT/WebSocket contexts.

Changes:
- Add static JsonDocument _dspApiDoc at file scope
- Replace all 'JsonDocument doc;' with '_dspApiDoc.clear(); JsonDocument &doc = _dspApiDoc;'
- Updated in loadRoutingMatrix, saveRoutingMatrix, loadDspSettings, dsp_preset_save, dsp_preset_load, and all API endpoint handlers

Tests: 156 DSP tests pass, no behavioral changes (`b42d50a`)
- Version bump to 1.8.3

## Breaking Changes
None

## Known Issues
- None

## Version 1.8.2

## Documentation
- [2026-02-15] docs: Update release notes for v1.8.2 (`03a9078`)

## New Features
- [2026-02-15] feat: Release version 1.8.2 with new DSP features and improvements

- Extended DSP stage types: Noise Gate, Tone Controls, Loudness Compensation, Bass Enhancement, Stereo Width, Speaker Protection, Multiband Compressor
- Added Bessel crossover filters with multiple order options
- Implemented baffle step correction for high-shelf compensation
- Introduced real-time THD+N measurement with harmonic level display
- Improved client-side frequency response graph for PEQ curve accuracy
- Reduced Core 1 idle CPU usage significantly
- Fixed various bugs including frequency response graph issues and undefined function calls
- Updated technical details and ensured 754 native tests are passing
- Version bumped to 1.8.2 (`90ce72c`)

### New Features
- **Extended DSP stage types**: Noise Gate, Tone Controls, Loudness Compensation, Bass Enhancement, Stereo Width, Speaker Protection, Multiband Compressor — full Web UI, WebSocket, MQTT, and REST support
- **Bessel crossover filters**: 2nd/4th/6th/8th order Bessel options in crossover preset UI
- **Baffle step correction**: Calculate and apply high-shelf compensation from baffle width
- **THD+N measurement**: Real-time total harmonic distortion measurement with harmonic level display

### Improvements
- **Client-side frequency response graph**: PEQ curve now computed from parameters (freq/gain/Q/type) using RBJ Audio EQ Cookbook formulas in JavaScript, eliminating dependency on server-sent coefficients. Curves always match control point handles during drag, slider, and step button interactions
- **Core 1 idle CPU reduced ~60% to ~15-20%**: Silence fast-path skips DC filter/DSP/RMS/FFT on zero buffers; audio task yield increased 1→2 ticks; GUI polls at 100ms when screen asleep; dashboard refresh skipped when display off

### Bug Fixes
- **Frequency response graph disconnected from EQ bands**: Curves now always pass through control point handles by computing coefficients client-side instead of relying on server-sent coefficients
- **Frequency response graph missing crossover types**: Filter types 19 (LPF 1st), 20 (HPF 1st), 21 (Linkwitz) now render correctly via `dspIsBiquad()` helper
- **Undefined wsSend() function**: Fixed 5 call sites (ADC enable, USB audio, baffle step, THD start/stop) that used nonexistent `wsSend()` — replaced with `ws.send()` with readyState guard
- **BaffleStepResult build error**: Removed redundant extern declaration in websocket_handler.cpp
- **ADC Input label alignment**: Flexbox layout for ADC toggle switches in Audio Settings

### Technical Details
- 754 native tests passing
- RAM: 44.0%, Flash: 77.8%
- Version bump to 1.8.2

### Breaking Changes
None

### Known Issues
- None

## Version 1.8.1

### New Features
- DSP config presets: 4 named slots, save/load full config + routing matrix
- Gain ramp: exponential per-sample smoothing (5ms tau) prevents audible clicks
- Linkwitz Transform biquad filter type for subwoofer alignment
- USB digital audio input via TinyUSB UAC2 on native USB OTG
- Per-ADC enable/disable toggle with I2S driver reinit safety (audioPaused flag)
- Delay alignment measurement module
- Convolution engine with ESP-DSP lite fallbacks (conv, corr, fird)
- DSP_MAX_STAGES bumped 20→24 for PEQ + chain headroom
- MQTT/WS/REST/GUI integration for all new features
- 708 native tests passing

### Bug Fixes
- Align ADC Input labels with toggle switches in Audio Settings
- Fix preset save crash (stack overflow): heap-allocate 8KB/4KB buffers, debounced save

### Technical Details
- Version bump to 1.8.1

### Breaking Changes
None

### Known Issues
- None

## Version 1.8.0

## Bug Fixes
- [2026-02-14] fix: Debug console scroll, move debug toggle to General, persist DSP bypass

Debug console: Only auto-scroll when user is at bottom of log. Scrolling
up to read older entries no longer gets interrupted by new messages.

Settings UI: Debug Mode toggle moved from standalone card into General
section. PEQ graph dots now show band numbers inside circles.

WebSocket: DSP enable/bypass changes now persist via saveDspSettingsDebounced().
- [2026-02-14] fix: remove noisy No ACK debug logs from I2C bus scan

The summary line already reports device count and EEPROM mask — per-address
NACK logs for 0x51-0x57 added no diagnostic value and cluttered output.
- [2026-02-14] fix: OTA exponential backoff, ADC health debounce, EEPROM scan cleanup

OTA: Exponential backoff after consecutive failures (5min → 15min → 30min
→ 60min). Heap pre-flight check (60KB) inside OTA task before TLS
handshake prevents transient HEAP CRITICAL dips.

ADC health: 3-second debounce on health state transition logging to
eliminate 33K+ lines/session from ADC2 oscillating at CLIPPING boundary.

EEPROM: dac_eeprom_scan() now accepts I2C bus scan mask — only probes
addresses that ACK'd, eliminating 7 Wire error lines per scan from
absent 0x51-0x57 addresses.
- [2026-02-14] fix: temperatureRead() SAR ADC spinlock crash + OTA heap guard

Cache temperatureRead() every 10s to avoid SAR ADC spinlock deadlock
with I2S ADC that caused Core 1 interrupt WDT crash after ~2.5h.
Skip OTA checks when heap is already critical to prevent recurring
HEAP CRITICAL from TLS buffer allocation.

### DAC Output HAL
- Plugin driver architecture with DacDriver abstract class and compile-time registry
- PCM5102A driver: I2S-only, software volume via log-perceptual curve
- I2S full-duplex on I2S_NUM_0 (TX+RX), GPIO 40 data out, shared clocks with ADC1
- Settings persistence in `/dac_config.json` on LittleFS
- Web UI: "DAC Output" card on Audio tab (enable, volume, mute, driver select)
- WebSocket: `sendDacState()`, commands for enable/volume/mute/filter
- REST API: GET/POST `/api/dac`, GET `/api/dac/drivers`

### EEPROM Programming & Diagnostics
- AT24C02 EEPROM auto-detect on I2C bus (0x50-0x57) with ALXD format
- Serialize, page-aware write (ACK polling), erase, raw read, full I2C bus scan
- I2C bus recovery (SCL toggling for stuck SDA lines)
- Web UI: "EEPROM Programming" card (Audio tab) with driver presets, form fields, program/erase
- Web UI: "EEPROM / I2C" diagnostics card (Debug tab) with parsed fields, hex dump, re-scan
- Clear EEPROM state display: Programmed (green), Empty (orange), Not Found (red)
- TFT GUI: EEPROM section on Debug screen
- REST API: GET/POST `/api/dac/eeprom`, erase, scan, presets endpoints
- WebSocket: eepromScan, eepromProgram, eepromErase commands
- 28 native tests (17 parse + 11 serialize)

### DSP Enhancements
- 10-band parametric EQ (PEQ) per channel with dedicated stage slots
- Crossover bass management and 4x4 routing matrix presets
- DSP settings export/import via REST API

### Bug Fixes
- **Core 1 WDT crash**: `temperatureRead()` SAR ADC spinlock deadlock with I2S ADC caused interrupt watchdog timeout after ~2.5h of runtime. Fixed by caching temperature every 10s instead of on every HW stats broadcast.
- **OTA heap pressure**: OTA checks (TLS ~16KB allocation) now skip when heap is already critical, preventing recurring HEAP CRITICAL warnings every 5 minutes.

### Other Improvements
- GUI navigation and DSP screen enhancements

---

## Version 1.7.1

### ESP-DSP Optimizations
- Radix-4 FFT (dsps_fft4r_fc32) replaces Radix-2 for 20-27% speedup on S3
- 6 selectable FFT window types (Hann, Blackman, Blackman-Harris, Blackman-Nuttall, Nuttall, Flat-Top)
- SNR/SFDR per-ADC analysis via dsps_snr_f32/dsps_sfdr_f32
- Vector math SIMD (dsps_mulc_f32) replaces manual gain/polarity loops in DSP pipeline
- Routing matrix SIMD (dsps_mulc_f32 + dsps_add_f32) replaces per-sample inner loops
- Spectrum band edges start at 0 Hz (bin 0 included)
- Native test fallbacks in lib/esp_dsp_lite/ (23 new tests)
- MQTT HA discovery entities for FFT window select, per-ADC SNR/SFDR sensors

### Debug Toggle Decoupling
- Each debug sub-toggle now independently gates its own WebSocket data
- Hardware Stats toggle gates CPU/memory/storage/WiFi/ADC/crash data
- I2S Metrics toggle independently gates I2S config and runtime data
- Task Monitor already correctly gated
- Main loop broadcasts when any sub-toggle is active (not just HW Stats)
- Fixed "ADC 2 (Slave)" label to "ADC 2 (Master)" in web UI

## Version 1.7.0

### New Features
- **Audio DSP Pipeline**: Full 4-channel configurable DSP engine for active crossover and speaker management. Processes dual PCM1808 I2S ADC inputs into 4 mono channels with up to 20 processing stages each.
  - **Core engine** (Phase 1): Double-buffered DspState with glitch-free config swap, biquad IIR filters (LPF, HPF, BPF, Notch, PEQ, Low/High Shelf, Allpass, Custom), FIR convolution with pool-allocated taps/delay, peak limiter with envelope follower, gain stage, CPU load metrics
  - **Extended processing** (Phase 2): Delay lines (up to 100ms @48kHz), polarity inversion, mute, compressor with makeup gain. Crossover presets (LR2/LR4/LR8, Butterworth), bass management, 4x4 routing matrix
  - **Web UI** (Phase 3): DSP tab with stage list, add/remove/reorder/enable, client-side frequency response graph (biquad transfer function), crossover preset UI, routing matrix UI, import/export (REW APO text, JSON backup)
  - **ESP-DSP S3 assembly** (Phase 4): Pre-built `libespressif__esp-dsp.a` replaces ANSI C biquad/FIR processing (~30% biquad, 3-4x FIR speedup). ESP-DSP FFT replaces arduinoFFT for spectrum analysis. Renamed coefficient gen (`dsp_gen_*`) avoids symbol conflicts
  - **MQTT/HA + TFT** (Phase 5): HA discovery entities for DSP enable/bypass/CPU load + per-channel bypass/stages/limiter GR. TFT DSP menu screen + desktop carousel card
  - **Import/Export**: Equalizer APO text (REW), miniDSP biquad coefficients, FIR text, WAV impulse response (16/32-bit PCM, IEEE float), full JSON backup
  - **Integration**: 18 REST API endpoints, WebSocket real-time sync, LittleFS persistence with 5s debounced save

### Improvements
- **PSRAM delay line allocation**: DSP delay buffers now use `ps_calloc()` for the 8MB PSRAM when available, keeping internal SRAM free for WiFi/MQTT buffers
- **Heap safety hardening**: `dsp_add_stage()` rolls back on pool exhaustion instead of creating broken stages. Config imports skip stages on allocation failure. WebSocket sends `dspError` message to client on failure. Pre-flight heap check blocks delay allocation when free heap would drop below 40KB reserve

### Bug Fixes
- **Web server unreachable**: Static 76.8KB delay line pool consumed nearly all heap (9KB left), starving WiFi RX buffers. Converted to dynamic allocation — saves 76.8KB when no delay stages are in use
- **WDT crash (OTA check)**: TLS handshake during OTA firmware check took 5-10s of CPU on Core 0 without yielding, starving IDLE0 watchdog. Added `esp_task_wdt_delete()` to OTA check task
- **WDT crash (IDLE0 starvation)**: audio_cap (priority 3) + OTA_CHK on Core 0 prevented IDLE0 from feeding WDT. Unsubscribed IDLE0 from WDT — all important tasks have their own entries
- **Heap critical threshold**: Raised from 20KB to 40KB to match actual WiFi + HTTP + MQTT minimum requirements

### Technical Details
- 540 unit tests (57 DSP core + 22 REW parser + existing), all passing
- RAM: 51.6% (169008 / 327680 bytes)
- Flash: 68.9% (2302989 / 3342336 bytes)
- ESP-DSP pre-built library for S3 assembly-optimized processing (native tests use ANSI C fallbacks)

### Breaking Changes
None

### Known Issues
- None
