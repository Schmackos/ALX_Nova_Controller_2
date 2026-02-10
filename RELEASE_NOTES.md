# Release Notes

## Version 1.6.0

## New Features
- **Audio ADC Diagnostics**: Real-time health monitoring for the PCM1808 I2S audio ADC with automatic status detection (OK, NO_DATA, NOISE_ONLY, CLIPPING, I2S_ERROR). Diagnostics are exposed across all interfaces — GUI Debug screen, WebSocket heartbeat, REST API (`/api/diagnostics` v1.1), and MQTT with Home Assistant discovery (`audio/adc_status` + `audio/noise_floor` diagnostic entities). Pure testable `audio_derive_health_status()` function with priority-based status derivation.

- **Input Voltage (Vrms) Display**: Computed Vrms from I2S RMS data with configurable ADC reference voltage (1.0–5.0V). Displayed in the Web UI as an info-row after Audio Level, on the GUI Debug screen, and published via MQTT (`audio/input_vrms` sensor + `settings/adc_vref` number entity). REST API and WebSocket support for real-time monitoring. Persisted as line 5 in `/smartsensing.txt`.

- **Audio Graph Toggles**: Individual enable/disable switches for all three audio visualizations — VU Meter, Waveform, and Spectrum. When disabled, the I2S task skips the corresponding processing (VU ballistics, waveform accumulation, or FFT) and WebSocket stops sending disabled graph payloads, reducing CPU and bandwidth. Controls available via Web UI card header toggles, WebSocket commands, MQTT switch entities with Home Assistant discovery, and persisted in settings lines 18–20.

## Improvements
- **Audio Tab UI Polish**: Added "Enable" labels to each graph card header toggle, matching the existing label style of "LED", "Segmented", and "Auto-scale" toggles. Relocated the Audio Update Rate dropdown from the Waveform card header to the Audio Settings card, since it controls all three graphs equally.

- **Desktop Carousel Enhancements**: Updated card summaries to reflect new audio diagnostic data and graph toggle states.

- **Home Screen Updates**: Enhanced status formatting for audio-related fields.

## Bug Fixes
- None

## Technical Details
- 28 files changed, ~5,800 lines added, ~3,700 lines removed
- New test suites: `test_audio_diagnostics` (10 tests), `test_vrms` (8 tests)
- Expanded test suites: `test_gui_home`, `test_smart_sensing`
- Total unit tests: 350 (up from ~310 in 1.5.4), all passing
- RAM usage: 41.8% (137,060 / 327,680 bytes)
- Flash usage: 61.6% (2,059,721 / 3,342,336 bytes)
- `AudioHealthStatus` enum and `AudioDiagnostics` struct added to `i2s_audio.h`
- `audio_rms_to_vrms()` pure function for testable Vrms conversion
- MQTT: 7 new Home Assistant discovery entities (2 diagnostic, 1 sensor, 1 number, 3 switches)
- Settings persistence: 3 new lines in `/settings.txt` (audio graph toggles), 1 new line in `/smartsensing.txt` (ADC Vref)
- REST API: `/api/diagnostics` bumped to version 1.1 with `audioAdc` object

## Breaking Changes
None

## Known Issues
- None

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
- None

## Technical Details
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
