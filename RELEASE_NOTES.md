# Release Notes

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

## New Features
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
- Audio task moved from Core 0 to Core 1 so WiFi/MQTT/HTTP stay
  responsive even if audio hangs
- MQTT HA discovery: reset_reason, was_crash, heap_critical, heap_max_block
- Exception decoder enabled in platformio.ini monitor_filters
- 24 new tests (441 total), firmware builds at 44.0% RAM / 64.8% Flash

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`f5d9f64`)
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
- Audio task moved from Core 0 to Core 1 so WiFi/MQTT/HTTP stay
  responsive even if audio hangs
- MQTT HA discovery: reset_reason, was_crash, heap_critical, heap_max_block
- Exception decoder enabled in platformio.ini monitor_filters
- 24 new tests (441 total), firmware builds at 44.0% RAM / 64.8% Flash

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`95f7d64`)
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
- Audio task moved from Core 0 to Core 1 so WiFi/MQTT/HTTP stay
  responsive even if audio hangs
- MQTT HA discovery: reset_reason, was_crash, heap_critical, heap_max_block
- Exception decoder enabled in platformio.ini monitor_filters
- 24 new tests (441 total), firmware builds at 44.0% RAM / 64.8% Flash

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`736258a`)

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

---

## Version 1.5.4

## Documentation
- [2026-02-10] docs: Update RELEASE_NOTES.md for version 1.5.4

- Added new features including dual ADC support, detailed audio diagnostics, and enhanced signal generator functionality.
- Improved OTA update process with FreeRTOS tasks for non-blocking operations.
- Expanded test coverage, increasing total tests to 376. (`ce9bbd6`)

## New Features
- [2026-02-10] feat: Enhance audio processing and testing capabilities

- Added support for dual ADCs with new I2S audio configuration, enabling simultaneous audio input from two sources.
- Introduced detailed audio diagnostics for each ADC, including health status and noise floor measurements.
- Updated signal generator functionality to allow selection of target ADC for output.
- Enhanced OTA update process with FreeRTOS tasks for non-blocking operations and improved status broadcasting.
- Expanded test coverage with additional tests for new features, bringing total tests to 376. (`bf41307`)
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- [2026-02-10] fix: Debug tab visibility tied to master toggle, HW Stats hides only its sections

Debug tab now appears/disappears based on Debug Mode only. The Hardware
Stats toggle controls visibility of CPU, Memory, Storage, WiFi/System,
and Audio ADC cards rather than the entire Debug tab.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`400c99c`)


## Technical Details
- [2026-02-10] refactor: Move debug sub-toggles from Settings tab to Debug tab

Sub-toggles (HW Stats, I2S Metrics, Task Monitor, Serial Level) now
live on the Debug tab near their respective sections instead of being
grouped under Settings. Master Debug Mode toggle stays on Settings.
I2S and FreeRTOS cards hide entirely when their toggle is off.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`e4e26ca`)
- Version bump to 1.5.4

## Breaking Changes
None

## Known Issues
- None

## Version 1.5.3

## Documentation
- [2026-02-07] docs: Update release notes for signal generator and display dimming

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`b2a39ee`)

## New Features
- [2026-02-07] feat: Implement signal generator functionality and enhance display dimming features

- Add signal generator with configurable waveform, frequency, amplitude, and output mode.
- Introduce MQTT support for signal generator settings and state management.
- Enhance display dimming capabilities with new settings for dimming enabled state and brightness.
- Update web interface to include controls for signal generator and dimming features.
- Refactor dark mode terminology and settings for consistency across the application.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`b09640a`)
- [2026-02-06] feat: Add dim timeout, enhanced debug screen, and GUI improvements

- Add display dim timeout with configurable delay (AppState, settings, MQTT, Web, GUI)
- Enhance debug screen with detailed memory, CPU, storage, network, and system info
- Add GUI navigation guard (gui_nav_is_on_desktop) for context-aware behavior
- Expand pinout tests and add dim timeout test suite
- Misc GUI fixes: keyboard, menu, value editor, wifi screen refinements

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`4e6e570`)
- [2026-02-06] feat: Add PCM1808 24-bit I2S audio ADC integration (Phases 1-4)

Replace analogRead() voltage detection with PCM1808 stereo I2S ADC for
professional audio analysis. Native dBFS thresholds, VU metering with
industry-standard ballistics, 256-point waveform visualization, and
1024-point FFT with 16-band spectrum analysis.

Phase 1: Core I2S driver + DMA + RMS/dBFS detection + settings migration
Phase 2: VU metering (300ms attack/650ms decay) + peak hold (2s hold)
Phase 3: Waveform downsampling (256-pt uint8 snapshots at 10Hz)
Phase 4: FFT via arduinoFFT (1024-pt, 16 musically-spaced bands)

Adds dedicated Audio tab in web UI with oscilloscope waveform canvas,
spectrum analyzer, stereo VU meters, and audio settings. WebSocket
per-client subscription protocol for bandwidth-efficient streaming.

All interfaces updated: WebSocket, MQTT/HA discovery, REST API, GUI
screens. 34 new tests (272 total), all passing.

## Technical Details
- [2026-02-06] chore: Bump version to 1.5.3

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`a5e5829`)

## Improvements
- None

## Bug Fixes
- [2026-02-06] fix: Regenerate gzipped web assets and fix gui_manager build error

Regenerate web_pages_gz.cpp to include the new Audio tab added in the
PCM1808 integration. Fix declaration order in gui_manager.cpp where
last_applied_brightness was used before being declared.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`aefe8e7`)


## Technical Details

## Breaking Changes
None

## Known Issues
- None

## Version 1.5.2

## Documentation
- [2026-02-06] docs: Update release notes

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`2a4250f`)
- [2026-02-06] docs: Update release notes

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`e53e850`)

## New Features
- [2026-02-06] feat: Add backlight brightness control across GUI, Web, and MQTT

Add a persisted backlight brightness setting (10-100%) controllable
from all interfaces. Wake restores saved brightness instead of
hardcoded 255. Persisted as line 13 in /settings.txt.

- AppState: backlightBrightness field with dirty-flag setter
- GUI: Settings menu brightness cycle (10/25/50/75/100%), live apply
- gui_manager: screen_wake uses saved brightness, gui_task polls changes
- settings_manager: save/load line 13, JSON import/export/diagnostics
- WebSocket: setBrightness handler, backlightBrightness in displayState
- MQTT: display/brightness topic (percentage), HA discovery number entity
- Web UI: range slider in Appearance card with real-time sync

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`20efaac`)
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- [2026-02-06] fix: Replace brightness slider with dropdown and add MQTT voltage precision

- Replace unstyled range slider with styled select dropdown (10/25/50/75/100%)
  matching the Screen Timeout control in the Web UI Appearance card
- Add suggested_display_precision: 2 to MQTT voltage sensor HA discovery
  so Home Assistant displays voltage with 2 decimal places
- Regenerate gzipped web assets

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`5a17d59`)
- [2026-02-06] fix: Regenerate gzipped web assets with brightness slider

The device serves from web_pages_gz.cpp (gzip-compressed), which was
stale after the brightness feature was added to web_pages.cpp.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`443ae5a`)
- [2026-02-06] fix: Improve buzzer reliability and add shutdown melody

- Add BUZZ_SHUTDOWN pattern (reversed startup chime) played before
  all reboot/reset actions for audible feedback
- Fix buzzer PWM channel conflict (1→2) to use separate timer from
  backlight
- Detach buzzer pin from LEDC after playback to eliminate residual
  PWM noise
- Reorder buzzer_update() to sequence patterns before checking ISR
  flags, preventing missed tick sounds during active playback
- Remove silence gap from tick pattern for snappier encoder feedback
- Use buzzer_play_blocking() for button long/very-long press resets
- Update CLAUDE.md with GUI architecture docs and 237 test count
- Update release workflow test summary to reflect all 16 test modules

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`d3fde3e`)


## Technical Details
- Version bump to 1.5.2

## Breaking Changes
None

## Known Issues
- None

## Version 1.5.1

## New Features
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- None

## Technical Details
- Version bump to 1.5.1

## Breaking Changes
None

## Known Issues
- None

## Version 1.5.0

## New Features
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- None

## Technical Details
- Version bump to 1.5.0

## Breaking Changes
None

## Known Issues
- None

## Version 1.4.4

## New Features
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- None

## Technical Details
- Version bump to 1.4.4

## Breaking Changes
None

## Known Issues
- None

## Version 1.4.3

## New Features
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- None

## Technical Details
- Version bump to 1.4.3

## Breaking Changes
None

## Known Issues
- None

## Version 1.4.2

## Bug Fixes
- [2026-02-06] fix: Prevent GUI freeze on screen navigation and buzzer dying

Increase gui_task stack (8KB→16KB) and LVGL heap (32KB→48KB) to prevent
stack overflow and allocation failures during complex screen transitions.
Add FreeRTOS mutex to buzzer_update() and call it from both gui_task
(Core 1) and main loop (Core 0) so the buzzer keeps working if gui_task
stalls.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`49fd8e3`)
- [2026-02-06] fix: Prevent GUI freeze on screen navigation and buzzer dying

Increase gui_task stack (8KB→16KB) and LVGL heap (32KB→48KB) to prevent
stack overflow and allocation failures during complex screen transitions.
Add FreeRTOS mutex to buzzer_update() and call it from both gui_task
(Core 1) and main loop (Core 0) so the buzzer keeps working if gui_task
stalls.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`4559e5c`)
- [2026-02-06] fix: Prevent GUI freeze on screen navigation and buzzer dying

Increase gui_task stack (8KB→16KB) and LVGL heap (32KB→48KB) to prevent
stack overflow and allocation failures during complex screen transitions.
Add FreeRTOS mutex to buzzer_update() and call it from both gui_task
(Core 1) and main loop (Core 0) so the buzzer keeps working if gui_task
stalls.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`b7a23b6`)

## New Features
- [2026-02-06] feat: Add OTA update melody with blocking playback helper

Play a descending D-minor alert melody (D6→A5→F5→D5→A5) before
firmware flashing begins, giving audible feedback for both GitHub
download and manual upload OTA paths. Adds buzzer_play_blocking()
for synchronous melody playback in non-looping contexts.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`4f6f9df`)
- [2026-02-06] feat: Add OTA update melody with blocking playback helper

Play a descending D-minor alert melody (D6→A5→F5→D5→A5) before
firmware flashing begins, giving audible feedback for both GitHub
download and manual upload OTA paths. Adds buzzer_play_blocking()
for synchronous melody playback in non-looping contexts.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`209d435`)

## Documentation
- [2026-02-06] docs: Clean up release notes for v1.4.2

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`d579432`)

### New Features
- **Complete LVGL GUI System**: Full graphical interface on ST7735S 128x160 TFT with rotary encoder, built on LVGL v9.4 + TFT_eSPI. Desktop carousel with 7 swipeable cards, dedicated menus for Control, WiFi, MQTT, Settings, and Debug, plus reusable value editors and an LVGL keyboard for text input.
- **Home Status Screen**: Read-only dashboard displaying amplifier state, signal voltage, auto-off timer, WiFi/MQTT status, sensing mode, firmware version, and uptime. Refreshes at 1 Hz.
- **Boot Animations**: 6 selectable animation styles (Wave Pulse, Speaker Ripple, Waveform, Beat Bounce, Freq Bars, Heartbeat). Enable/disable and style selection persisted to flash.
- **Screen Sleep/Wake**: Configurable display timeout (off / 30s / 60s / 5m / 10m) with backlight toggle synced bidirectionally across GUI, Web UI, and MQTT.
- **Dark/Light Theme**: Orange accent theme with dark mode toggle available in GUI settings.
- **Buzzer Feedback**: Piezo buzzer support with enable/disable and 3 volume levels (Low / Medium / High). Real-time sync via MQTT and WebSocket. Configurable from GUI settings.
- **Support Tab**: In-GUI user manual and documentation access with search functionality.
- **MQTT Settings Sync**: Bidirectional MQTT configuration management (broker, port, username, discovery toggle) via WebSocket and GUI.
- **WiFi Retry Logic**: Intelligent retry mechanism for "Network not found" errors with periodic retries and failed SSID tracking.
- **Gzip Web Content**: Compressed HTML/CSS/JS delivery from ESP32, reducing transfer size.

### Improvements
- Standardized logging across all 22 source files: replaced all `DebugOut.*` and `Serial.print*` calls with `LOG_D/I/W/E` macros and consistent `[Module]` tags
- Cross-task state synchronization redesigned: GUI sets dirty flags, main loop handles all WebSocket/MQTT broadcasts (thread-safe)
- WiFi credentials refactored to structured configuration approach
- Enhanced debug console with log level filtering
- Navigation focus index persistence across screen transitions
- Encoder input via ISR-driven Gray code state machine

### Bug Fixes
- Fixed cross-task WebSocket/MQTT sync where GUI callbacks on gui_task caused silent broadcast failures and poisoned change tracking

### Technical Details
- 75 files changed, +13,009 lines added, -573 removed
- Unit tests expanded from 106 to 232 (new modules: buzzer, GUI home, GUI input, GUI navigation, pinout)
- RAM usage: 32.3% (105,880 / 327,680 bytes)
- Flash usage: 56.7% (1,894,921 / 3,342,336 bytes)
- FreeRTOS GUI task on Core 1, main loop on Core 0
- LVGL v9.4 with `LV_DRAW_SW_ASM_NONE` for ESP32-S3 compatibility
- Pin mapping: TFT (MOSI=11, SCLK=12, CS=10, DC=13, RST=14, BL=21), Encoder (A=5, B=6, SW=7), Buzzer (GPIO=8)

### Breaking Changes
None

### Known Issues
None

---

## Version 1.4.1

### New Features
- Comprehensive WiFi management system with password field, connection/save workflows, change detection, and success/failure modals
- Automatic WiFi reconnection with event-driven monitoring and smart retry logic
- Enhanced WiFi network removal with current-network warning modal and AP mode fallback
- `saveNetworkSettings()` for saving WiFi configurations without connecting
- Debug tab graph repositioning for better visual hierarchy

### Improvements
- Dynamic button labels based on user changes (Connect vs Connect & Update)
- AP mode monitoring after network removal (30s window with status polling)
- Explicit HTTP handlers for common browser auto-requests (favicon, manifest, robots.txt) to reduce console noise
- Consistent button sizing in network configuration section

### Bug Fixes
- Fixed duplicate `updateNetworkConfig()` that used `prompt()` for password input
- Fixed WiFi scan authentication errors by adding automatic credentials to all API calls
- Fixed favicon.ico 204 response causing WebServer content-length warning
- Fixed WiFi password update causing authentication failure on currently connected network

### Technical Details
- Added `onWiFiEvent()` handler for real-time WiFi disconnection/reconnection
- Enhanced `apiFetch()` wrapper with automatic credential injection
- Version bump to 1.4.1

---

## Version 1.4.0

### New Features
- Debug tab redesign with integrated CPU and memory graphs embedded in cards
- Y-axis labels (0-100%) and X-axis time labels (-60s, -30s, now) on all graphs
- PSRAM usage graph (auto-displayed when available)
- Reset reason display in WiFi & System card
- Compact 2-column grid layout (responsive single column on mobile)

### Technical Details
- Window resize handler for responsive graph redrawing
- Graph dimensions: 140px height, 35px left margin, 20px bottom margin
- Added `psramPercent` tracking to WebSocket history data
- Version bump to 1.4.0

---

## Version 1.3.0

### New Features
- Automated release via GitHub Actions

### Bug Fixes
- Fixed WiFi disconnection and AP mode handling when removing current network (correct ALX-****** SSID, AP toggle state sync)

### Technical Details
- WiFi interface reorganized: removed redundant Saved Networks section, consolidated into Network Configuration with Remove button
- Version bump to 1.3.0

---

## Version 1.2.12

### New Features
- Enhanced WiFi management with improved network configuration workflow
- Auto-populate connection form when selecting saved networks
- Save Settings button for saving without connecting
- Static IP/DHCP display in connection status
- New `/api/wifisave` endpoint for save-only operations

---

## Version 1.2.11

### Bug Fixes
- Fixed release workflow regex syntax errors for conventional commit parsing
- Added `fetch-depth: 0` to checkout step to fetch all tags

### Technical Details
- Enhanced release notes generation and formatting in release.yml

---

## Version 1.2.8

### New Features
- Multi-WiFi support: remember and auto-connect to up to 5 saved networks with priority system
- Automatic AP fallback when all connections fail
- Saved Networks management UI with visual list, priority badges, and one-click removal

### Technical Details
- New API endpoints: `GET /api/wifilist`, `POST /api/wifiremove`
- One-time migration from LittleFS to Preferences storage
- Configuration: `MAX_WIFI_NETWORKS = 5`, `WIFI_CONNECT_TIMEOUT = 12000`

---

## Version 1.2.4

### Bug Fixes
- Fixed smart auto sensing timer logic: timer stays at full value when voltage detected, counts down only when absent, resets on re-detection

---

## Version 1.2.3

### Improvements
- CPU usage optimization with main loop delays and rate-limited voltage readings
- Utility functions refactored to dedicated `utils.h`/`utils.cpp`
- Removed unused functions from mqtt_handler and ota_updater modules
