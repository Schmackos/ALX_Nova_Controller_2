# Release Notes

## Version 1.3.0

## New Features
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- [2026-02-03] fix: Properly handle WiFi disconnection and AP mode when removing current network

When removing the currently connected WiFi network:
- Detect if the removed network is currently connected
- Disconnect from WiFi gracefully
- Attempt to connect to remaining saved networks
- Start AP mode with correct SSID (ALX-******) if no networks available
- Broadcast WiFi status update to frontend so AP toggle reflects correct state

This fixes the issue where removing the current network would start AP mode with default ESP SSID instead of the configured ALX SSID, and the AP toggle would not update correctly.

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`9394684`)


## Technical Details
- Version bump to 1.3.0

## Breaking Changes
None

## Known Issues
- None

## Version 1.2.12

## New Features
- [2026-02-03] feat: Enhance WiFi management with improved network configuration workflow

Add comprehensive WiFi network management improvements:
- Auto-populate connection form when selecting saved networks with SSID, password placeholder, and Static IP settings
- Add "Save Settings" button to save network configurations without connecting
- Display IP configuration type (Static IP/DHCP) in connection status
- Create new /api/wifisave endpoint for save-only operations
- Enhance backend to detect and report Static IP usage for connected networks

These changes improve the user experience by making it easier to manage multiple WiFi networks and their configurations without requiring reconnection.

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`e413c93`)
- [2026-02-03] feat: Enhance WiFi management with improved network configuration workflow

Add comprehensive WiFi network management improvements:
- Auto-populate connection form when selecting saved networks with SSID, password placeholder, and Static IP settings
- Add "Save Settings" button to save network configurations without connecting
- Display IP configuration type (Static IP/DHCP) in connection status
- Create new /api/wifisave endpoint for save-only operations
- Enhance backend to detect and report Static IP usage for connected networks

These changes improve the user experience by making it easier to manage multiple WiFi networks and their configurations without requiring reconnection.

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`325a232`)
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- None

## Technical Details
- Version bump to 1.2.12

## Breaking Changes
None

## Known Issues
- None

## Version 1.2.11

## Documentation
- [2026-02-03] docs: Document dual-source release notes feature and fix template structure

- Add documentation for complete commit list in releases
- Fix duplicate Technical Details section in RELEASE_NOTES.md
- Add missing New Features section to version 1.2.11
- Document firmware details table enhancement with commit count

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`5c86001`)
- [2026-02-03] docs: Document dual-source release notes feature and fix template structure

- Add documentation for complete commit list in releases
- Fix duplicate Technical Details section in RELEASE_NOTES.md
- Add missing New Features section to version 1.2.11
- Document firmware details table enhancement with commit count

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`39e4e07`)

## New Features
- None

## Improvements
- None

## Bug Fixes
- [2026-02-03] fix: Simplify regex patterns to match conventional commit colons only

- Remove character class [:(] which was causing syntax errors
- Use simple colon matching since conventional commits always use colons
- Fixes "unexpected EOF while looking for matching ')'" error

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`cdc5fd7`)
- [2026-02-03] fix: Complete regex pattern fix by restoring wildcard matchers

- Add back .* after [:(] to match the rest of commit messages
- Fixes shell syntax error in release notes preparation

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`f48eac8`)
- [2026-02-03] fix: Complete regex pattern fix by restoring wildcard matchers

- Add back .* after [:(] to match the rest of commit messages
- Fixes shell syntax error in release notes preparation

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`277a03c`)
- [2026-02-03] fix: Correct release workflow to fetch tags and fix regex syntax errors

- Add fetch-depth: 0 to checkout step to fetch all tags
- Fix regex patterns to use proper OR syntax (feat|feature) instead of invalid bracket expressions

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`879bad8`)
- [2026-02-03] fix: Correct release workflow to fetch tags and fix regex syntax errors

- Add fetch-depth: 0 to checkout step to fetch all tags
- Fix regex patterns to use proper OR syntax (feat|feature) instead of invalid bracket expressions

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`6d05d73`)


## Technical Details
- [2026-02-03] chore: Enhance release notes generation and update formatting in release.yml (`fa98455`)

## Breaking Changes
None

## Known Issues
- None

## Version 1.2.8

### New Features
- **Multi-WiFi Support**: The device can now remember and automatically connect to up to 5 saved WiFi networks
  - Priority system: Most recently successful network is tried first
  - 12-second connection timeout per network
  - Networks are stored in ESP32 NVS (Non-Volatile Storage) using Preferences library
  - One-time automatic migration from old single-network storage (LittleFS) to new multi-network system
  - Add/update networks via existing WiFi configuration form (maximum 5 networks)
  - Duplicate SSID detection prevents saving the same network twice

- **Automatic AP Fallback**: Device automatically enters Access Point mode if no saved WiFi networks are available or if all connection attempts fail
  - Seamless fallback ensures device is always accessible
  - No manual intervention required

- **Saved Networks Management UI**: New web interface for managing saved WiFi networks
  - Visual list showing all saved networks with priority badges
  - Network count indicator (X/5) shows how many slots are used
  - One-click removal of saved networks with confirmation dialog
  - Real-time list updates when WiFi status changes
  - Responsive design with hover effects
  - Empty state message when no networks are saved

### Technical Details
- New API endpoints:
  - `GET /api/wifilist` - Returns list of saved networks (SSIDs only, no passwords)
  - `POST /api/wifiremove` - Remove network by index
- Backend functions in `wifi_manager.cpp`:
  - `migrateWiFiCredentials()` - One-time migration from LittleFS to Preferences
  - `connectToStoredNetworks()` - Try all saved networks with automatic AP fallback
  - `saveWiFiNetwork()` - Add/update network with duplicate checking
  - `removeWiFiNetwork()` - Remove network by index
  - `getWiFiNetworkCount()` - Return saved network count
- Frontend JavaScript functions in `web_pages.cpp`:
  - `loadSavedNetworks()` - Fetch and display saved networks
  - `removeNetwork(index)` - Remove network with confirmation
- Configuration constants in `config.h`:
  - `MAX_WIFI_NETWORKS = 5` - Maximum number of saved networks
  - `WIFI_CONNECT_TIMEOUT = 12000` - Connection timeout per network (12 seconds)
- Firmware size increase: ~27KB (backend + frontend)

## Version 1.2.4

## Bug Fixes
- **Smart Auto Sensing Timer Logic**: Fixed timer countdown behavior in smart auto sensing mode. Timer now correctly stays at full value when voltage is detected and only counts down when no voltage is present. If voltage is detected again during countdown, timer resets to full value.

## Version 1.2.3

## Performance Improvements
- **CPU Usage Optimization**: Added delays in the main loop and implemented rate limiting for voltage readings in the smart sensing logic, significantly reducing CPU overhead and improving overall system efficiency.

## Code Quality
- **Utility Functions Refactoring**: Moved utility functions to dedicated `utils.h` and `utils.cpp` files for better code organization and maintainability.
- **Code Cleanup**: Removed unused functions from `mqtt_handler` and `ota_updater` modules, reducing code complexity and binary size.

## Technical Details
- Optimized `main.cpp` with proper delay handling in the main loop
- Refactored `smart_sensing.cpp` with rate-limited voltage readings to prevent excessive CPU usage
- Consolidated shared utility functions into a dedicated utils module
- Cleaned up dead code across multiple handler files
