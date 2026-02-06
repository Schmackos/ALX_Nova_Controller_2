# Release Notes

## Version 1.4.2

## Documentation
- [2026-02-06] docs: Update release notes hook additions

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`4c922c2`)
- [2026-02-06] docs: Clean up RELEASE_NOTES.md formatting

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`62fd92f`)

## Technical Details
- [2026-02-06] Update RELEASE_NOTES.md to consolidate and clarify documentation for buzzer functionality, including state management, volume control, and real-time updates via MQTT and WebSocket. (`fa91080`)

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
