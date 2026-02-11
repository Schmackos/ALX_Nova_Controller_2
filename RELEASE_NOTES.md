# Release Notes

## Version 1.6.6

## New Features
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- None

## Technical Details
- Version bump to 1.6.6

## Breaking Changes
None

## Known Issues
- None

## Version 1.6.5

## New Features
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- None

## Technical Details
- Version bump to 1.6.5

## Breaking Changes
None

## Known Issues
- None

## Version 1.6.4

## New Features
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- None

## Technical Details
- Version bump to 1.6.4

## Breaking Changes
None

## Known Issues
- None

## Version 1.6.3

### Bug Fixes
- **Performance regression fix**: Crash resilience changes caused device to be unusably slow — web app took ~15s to populate, LED blink in slow motion, TFT carousel laggy, and WDT reboots.
  - Audio task moved back to Core 0 (was starving GUI on Core 1 at priority 3 vs 1)
  - Removed double `esp_task_wdt_init()` that corrupted Arduino's WDT subscriber list → false watchdog reboots
  - GUI watchdog feed moved to top of loop (before LVGL processing, not after delay)
  - WDT timeout set to 15s via `CONFIG_ESP_TASK_WDT_TIMEOUT_S` build flag
- **MQTT reconnect blocking**: TCP connect timeout reduced from 3s to 1s via pre-connect; initial backoff increased from 1s to 5s. Previously could block the main loop for 3s every 1-2 seconds when broker unreachable.
- **MQTT audio level storm**: 0.5 dBFS fluctuation no longer triggers full state publish (65+ MQTT publishes). Audio level changes now only publish smart sensing + audio diagnostics (3-4 publishes).
- **Unnecessary LED broadcasts**: Removed `sendLEDState()` from blink toggle loop (2 WS broadcasts/second eliminated). Client animates locally from blinkingEnabled state.

### New Features
- [2026-02-11] feat: MQTT HA integration overhaul — availability, OTA progress, new entities

Infrastructure fixes:
- Subscribe to homeassistant/status for HA restart re-discovery
- Add update_percentage to OTA state JSON for HA progress bar
- Replace blocking performOTAUpdate with startOTADownloadTask in MQTT callback
- Fix LWT topic to use getEffectiveMqttBaseTopic() instead of raw mqttBaseTopic
- Add availability block to all HA discovery entities
- Add configuration_url to device info (links to web UI)
- Complete removeHADiscovery() cleanup list (was missing 20+ entities)

New HA entities:
- Factory Reset button, Timezone Offset number (-12 to +14h)
- Signal Generator Sweep Speed number (0.1-10.0 Hz/s)
- 4 Input Name read-only diagnostic sensors
- Boot Animation switch + style select (GUI_ENABLED guarded)

- [2026-02-11] feat: Crash resilience — watchdog, I2S timeout recovery, heap monitor, crash log

Overnight freeze revealed zero crash forensics and no watchdog protection.
Root cause: i2s_read() with portMAX_DELAY blocks forever if DMA hangs,
starving the main loop on the same core.

- Crash log: 10-entry ring buffer persisted to /crashlog.bin, records
  reset reason + heap stats on every boot, backfills NTP timestamp
- Task watchdog: 15s TWDT on main loop, audio task, and GUI task;
  OTA download task unsubscribes during long transfers
- I2S timeout: 500ms timeout replaces portMAX_DELAY, auto-recovers
  after 10 consecutive timeouts by reinstalling the I2S driver
- Heap monitor: checks largest free block every 30s, flags critical
  when below 20KB, published via MQTT (always-on, not debug-gated)
- MQTT HA discovery: reset_reason, was_crash, heap_critical, heap_max_block
- Exception decoder enabled in platformio.ini monitor_filters

### Technical Details
- 461 unit tests, all passing
- RAM: 44.0%, Flash: 65.1%

---

## Version 1.6.2.1

### Bug Fixes
- **Dual ADC I2S Fix**: Fixed ADC2 (I2S_NUM_1) never receiving data due to ESP32-S3 slave mode DMA issues. Root cause: the legacy I2S driver always calculates `bclk_div = 4` (below hardware minimum of 8), and the LL layer hard-codes `rx_clk_sel = 2` (D2CLK 160MHz) regardless of APLL settings, making all slave-mode workarounds ineffective. Solution: configure both I2S peripherals as master RX — I2S0 outputs clocks, I2S1 reads data only with no clock output. Both share the same 160MHz D2CLK with identical dividers, giving frequency-locked sampling.

### Improvements
- **Binary WebSocket Audio Data**: Waveform (258 bytes) and spectrum (70 bytes) sent as binary frames instead of JSON (~83% bandwidth reduction for dual ADC at 50Hz).
- **Audio WebSocket Optimizations**: Canvas dimension caching, offscreen background grid cache, pre-computed spectrum color LUT, DOM reference caching, adaptive LERP scaling with update rate.
- **VU Meter Tuning**: Decay time reduced from 650ms to 300ms for snappier digital response.
- **Smart Sensing Rate Fix**: `detectSignal()` now uses `appState.audioUpdateRate` instead of hardcoded 50ms, with dynamically computed smoothing alpha.

### Technical Details
- 417 unit tests, all passing
- RAM: 43.8%, Flash: 64.6%

---

## Version 1.6.2

### New Features
- **FreeRTOS Task Manager Enhancements**: Sortable columns (click any header), CPU utilization display (per-core + total), loop frequency metric, and stack usage percentage column with color-coded indicators. Moved to top of Debug page under Debug Controls.

### Improvements
- **Dark Mode Flash Fix**: Main page and login page now read localStorage immediately on load to apply dark/light theme before first paint, eliminating the light→dark flash on page load.
- **Audio Channel Label Rename**: Replaced Left/Right stereo naming with numbered inputs (Ch1/Ch2) and Input→ADC naming for hardware ADC references across all interfaces (WS, REST, MQTT, GUI, Web UI). MQTT topic paths changed (`audio/input1/*` → `audio/adc1/*`).
- **Default Audio Threshold**: Changed from -40 dBFS to -60 dBFS; web UI input field prefilled with new default.

### Breaking Changes
- MQTT topic paths changed: `audio/input1/*` → `audio/adc1/*`, `audio/input2/*` → `audio/adc2/*` — requires Home Assistant re-discovery.
- WebSocket JSON keys renamed: `vuL/vuR` → `vu1/vu2`, `peakL/peakR` → `peak1/peak2`, `rmsL/rmsR` → `rms1/rms2`, etc.
- Signal generator channel enum: `SIGCHAN_LEFT/RIGHT` → `SIGCHAN_CH1/CH2`.

### Technical Details
- 417 unit tests, all passing
- RAM: 43.8%, Flash: 64.2%

---

## Version 1.6.1

### New Features
- **Dual ADC Expansion**: Second PCM1808 I2S ADC (slave on I2S_NUM_1) sharing BCK/LRC/MCLK with ADC1 master, only new pin DOUT2=GPIO 9. Per-ADC VU meters, waveform canvases, spectrum analyzers, diagnostics, and input names across all interfaces (Web UI, WebSocket, REST, MQTT, GUI). Auto-detection hides ADC2 panels when only one ADC is connected.
- **FreeRTOS Task Monitor**: Real-time monitoring of FreeRTOS task stack watermarks, priorities, core affinity, and loop timing. Web UI "FreeRTOS Tasks" card on Debug tab, MQTT diagnostic sensors, and GUI Debug screen section. Runs on a dedicated 5s timer, opt-in via `debugTaskMonitor` toggle.
- **Debug Mode Toggle System**: Master debug gate with 4 per-feature sub-toggles (HW Stats, I2S Metrics, Task Monitor, Serial Level). Master OFF forces all sub-features off and serial to LOG_ERROR. Controls available in Web UI Debug tab, GUI Settings menu, MQTT, and WebSocket.
- **Audio ADC Diagnostics**: Per-ADC health monitoring (OK, NO_DATA, NOISE_ONLY, CLIPPING, I2S_ERROR) with noise floor tracking.
- **Input Voltage (Vrms) Display**: Computed Vrms from I2S RMS data with configurable ADC reference voltage (1.0–5.0V).
- **Audio Graph Toggles**: Individual enable/disable for VU Meter, Waveform, and Spectrum visualizations.
- **Non-Blocking OTA**: OTA check and download run as one-shot FreeRTOS tasks on Core 1.

### Bug Fixes
- **I2S Slave DMA Timeout**: Fixed ADC2 slave never receiving data — `i2s_configure_slave()` was passing `I2S_PIN_NO_CHANGE` for BCK/WS which skipped internal I2S peripheral clock path initialization.
- **ADC2 Diagnostics on Read Failure**: Slave read failures now properly increment `consecutiveZeros` and recompute health status.
- **Debug Tab Visibility**: Debug tab tied to master `debugMode` toggle; HW Stats toggle controls individual card visibility rather than the entire tab.

### Technical Details
- 417 unit tests (up from ~310 in 1.5.4)
- New test suites: `test_audio_diagnostics` (14), `test_vrms` (8), `test_task_monitor` (17), `test_debug_mode` (24)
- New source modules: `task_monitor.h/.cpp`
