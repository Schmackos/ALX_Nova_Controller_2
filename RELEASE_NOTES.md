# Release Notes

## Improvements
- **Dual Connection Status Display**: Fixed a bug where the web interface would only display Access Point details when the device was operating in both Access Point and Client (Station) modes. The interface now correctly displays status, IP addresses, and details for both connections simultaneously.
- **Documentation**: Corrected the GitHub Release Workflow documentation to properly reference `RELEASE_NOTES.md`.

## Technical Details
- **Backend**: Updated `wifi_manager.cpp` to populate connection details for both interfaces without ensuring mutual exclusivity.
- **Frontend**: Rewrote the status display logic in `web_pages.cpp` to render both Client and AP sections when applicable.
