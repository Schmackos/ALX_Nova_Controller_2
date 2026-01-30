# Version 1.1.23 Release Notes

## New Features

### Control Page Redesign
- **Control Status Card**: New status section at the top of the Control page displaying:
  - WebSocket connection status (Connected/Disconnected with color coding)
  - Voltage Detected indicator (Yes/No)
  - Current Reading display (real-time voltage value)
- **Collapsible Smart Auto Settings**: Timer and Voltage Threshold settings are now in a collapsible section that only appears when "Smart Auto Sensing" mode is selected, reducing UI clutter

### Update Notifications
- **Post-Update Reconnection Notification**: Shows "Device is back online after update!" when the device reconnects after a firmware update
- **Firmware Version Notification**: Displays "Firmware updated: X.X.X → Y.Y.Y" showing the previous and new version numbers
- **General Reconnection Notification**: Shows "Device reconnected" when WebSocket reconnects after any disconnection

### Enhanced OTA Progress Display
- OTA downloads now show percentage complete alongside downloaded/total size (e.g., "Downloading: 45% (230 / 512 KB)")
- Matches the progress display style used for manual firmware uploads

## Improvements

### UI/UX Enhancements
- Control page layout now matches the consistent card-based design used in WiFi and MQTT tabs
- Removed Base Topic from MQTT Connection Status for a cleaner interface (still available in MQTT Settings)
- Smart Sensing mode selection now immediately shows/hides the settings panel based on the selected mode

### Real-time Updates
- Voltage reading updates every 1 second for responsive monitoring
- Voltage tolerance reduced to 0.02V for more frequent and accurate updates

## Bug Fixes
- Fixed Smart Sensing mode radio buttons not reflecting the actual device mode
- Fixed immediate voltage evaluation when switching to "Smart Auto Sensing" mode
- Fixed voltage reading not updating in the web UI
- Fixed duplicate "timer started" messages in serial terminal
- Fixed OTA progress showing 5% increments instead of 1%
- Fixed manual upload progress bar interference with WebSocket updates

## Technical Details
- Added `wasDisconnectedDuringUpdate` and `hadPreviousConnection` flags for reconnection tracking
- `updateSmartAutoSettingsVisibility()` function controls settings panel visibility
- `toggleSmartAutoSettings()` implements collapsible behavior with CSS transitions
- Enhanced `handleUpdateStatus()` to display bytes downloaded with percentage

## Breaking Changes
None

## Known Issues
None
