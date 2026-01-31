# Release Notes

## Improvements
- **Dual Connection Status Display**: Fixed a bug where the web interface would only display Access Point details when the device was operating in both Access Point and Client (Station) modes. The interface now correctly displays status, IP addresses, and details for both connections simultaneously.
- **Improved Version Display**: The interface now shows an "**Up-To-Date**" status in green when the device is running the latest firmware, automatically hiding the update button to simplify the user experience.
- **Debug Level Selection**: Added a new dropdown to the Debug Console, allowing users to select log levels (Debug, Info, Warn, Error) in real-time. The console now features color-coded messages for better readability.
- **Documentation**: Corrected the GitHub Release Workflow documentation to properly reference `RELEASE_NOTES.md`.

## Technical Details
- **Backend (`wifi_manager.cpp`)**: Updated `buildWiFiStatusJson` to populate connection details for both interfaces without ensuring mutual exclusivity.
- **Backend (`websocket_handler.cpp`)**: Added handling for the `setDebugLevel` WebSocket command to adjust `DebugSerial` filtering dynamically.
- **Frontend (`web_pages.cpp`)**: 
  - Rewrote the status display logic to render both Client and AP sections.
  - Implemented version comparison logic for the "Up-To-Date" status.
  - Added a debug level dropdown and updated log entry styling with color-coded classes.
