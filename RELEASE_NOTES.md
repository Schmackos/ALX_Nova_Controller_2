# Release Notes

## Version 1.7.1

## New Features
- [2026-02-12] feat: ESP-DSP extended features, decouple debug toggle gating, version 1.7.1

ESP-DSP optimizations:
- Radix-4 FFT (dsps_fft4r_fc32) replaces Radix-2 for 20-27% speedup on S3
- 6 selectable FFT window types (Hann, Blackman, Blackman-Harris, Blackman-Nuttall, Nuttall, Flat-Top)
- SNR/SFDR per-ADC analysis via dsps_snr_f32/dsps_sfdr_f32
- Vector math SIMD (dsps_mulc_f32) replaces manual gain/polarity loops in DSP pipeline
- Routing matrix SIMD (dsps_mulc_f32 + dsps_add_f32) replaces per-sample inner loops
- Spectrum band edges start at 0 Hz (bin 0 included)
- Native test fallbacks in lib/esp_dsp_lite/ (23 new tests)
- MQTT HA discovery entities for FFT window select, per-ADC SNR/SFDR sensors

Debug toggle decoupling:
- Each debug sub-toggle now independently gates its own WebSocket data
- Hardware Stats toggle gates CPU/memory/storage/WiFi/ADC/crash data
- I2S Metrics toggle independently gates I2S config and runtime data
- Task Monitor already correctly gated
- Main loop broadcasts when any sub-toggle is active (not just HW Stats)
- Fixed "ADC 2 (Slave)" label to "ADC 2 (Master)" in web UI (`6a6ab60`)
- [2026-02-12] feat: ESP-DSP extended features, decouple debug toggle gating, version 1.7.1

ESP-DSP optimizations:
- Radix-4 FFT (dsps_fft4r_fc32) replaces Radix-2 for 20-27% speedup on S3
- 6 selectable FFT window types (Hann, Blackman, Blackman-Harris, Blackman-Nuttall, Nuttall, Flat-Top)
- SNR/SFDR per-ADC analysis via dsps_snr_f32/dsps_sfdr_f32
- Vector math SIMD (dsps_mulc_f32) replaces manual gain/polarity loops in DSP pipeline
- Routing matrix SIMD (dsps_mulc_f32 + dsps_add_f32) replaces per-sample inner loops
- Spectrum band edges start at 0 Hz (bin 0 included)
- Native test fallbacks in lib/esp_dsp_lite/ (23 new tests)
- MQTT HA discovery entities for FFT window select, per-ADC SNR/SFDR sensors

Debug toggle decoupling:
- Each debug sub-toggle now independently gates its own WebSocket data
- Hardware Stats toggle gates CPU/memory/storage/WiFi/ADC/crash data
- I2S Metrics toggle independently gates I2S config and runtime data
- Task Monitor already correctly gated
- Main loop broadcasts when any sub-toggle is active (not just HW Stats)
- Fixed "ADC 2 (Slave)" label to "ADC 2 (Master)" in web UI (`387ea8b`)
- [2026-02-12] feat: ESP-DSP extended features, decouple debug toggle gating, version 1.7.1

ESP-DSP optimizations:
- Radix-4 FFT (dsps_fft4r_fc32) replaces Radix-2 for 20-27% speedup on S3
- 6 selectable FFT window types (Hann, Blackman, Blackman-Harris, Blackman-Nuttall, Nuttall, Flat-Top)
- SNR/SFDR per-ADC analysis via dsps_snr_f32/dsps_sfdr_f32
- Vector math SIMD (dsps_mulc_f32) replaces manual gain/polarity loops in DSP pipeline
- Routing matrix SIMD (dsps_mulc_f32 + dsps_add_f32) replaces per-sample inner loops
- Spectrum band edges start at 0 Hz (bin 0 included)
- Native test fallbacks in lib/esp_dsp_lite/ (23 new tests)
- MQTT HA discovery entities for FFT window select, per-ADC SNR/SFDR sensors

Debug toggle decoupling:
- Each debug sub-toggle now independently gates its own WebSocket data
- Hardware Stats toggle gates CPU/memory/storage/WiFi/ADC/crash data
- I2S Metrics toggle independently gates I2S config and runtime data
- Task Monitor already correctly gated
- Main loop broadcasts when any sub-toggle is active (not just HW Stats)
- Fixed "ADC 2 (Slave)" label to "ADC 2 (Master)" in web UI (`8c75527`)

## Improvements
- None

## Bug Fixes
- None

## Technical Details
- None

## Breaking Changes
None

## Known Issues
- None

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
