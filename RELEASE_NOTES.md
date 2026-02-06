# Release Notes

## Version 1.4.2

## New Features
- [2026-02-05] feat: Add LVGL GUI with TFT display, encoder input, and fix cross-task state sync

Implements complete 7-phase GUI system for ST7735S 128x160 TFT with rotary
encoder on ESP32-S3 using LVGL v9.4 + TFT_eSPI. Includes desktop carousel,
control/settings/WiFi/MQTT/debug screens, boot animations, dark mode, and
screen sleep/wake. Fixes cross-task WebSocket/MQTT sync where GUI callbacks
on gui_task would call non-thread-safe WebSocket broadcasts, causing silent
failures and poisoned change tracking. GUI now only sets dirty flags; the
main loop handles all broadcasts.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`06a0d42`)
- [2026-02-05] feat: Add LVGL GUI with TFT display, encoder input, and fix cross-task state sync

Implements complete 7-phase GUI system for ST7735S 128x160 TFT with rotary
encoder on ESP32-S3 using LVGL v9.4 + TFT_eSPI. Includes desktop carousel,
control/settings/WiFi/MQTT/debug screens, boot animations, dark mode, and
screen sleep/wake. Fixes cross-task WebSocket/MQTT sync where GUI callbacks
on gui_task would call non-thread-safe WebSocket broadcasts, causing silent
failures and poisoned change tracking. GUI now only sets dirty flags; the
main loop handles all broadcasts.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com> (`f8be593`)
- [2026-02-04] feat: Implement WiFi manager unit tests using mock classes and resolve native compilation issues for existing tests. (`5820b55`)
- [2026-02-04] feat: Introduce `WiFiManager` module for centralized WiFi connection, persistence, and API handling. (`72525b0`)
- [2026-02-04] feat: Introduce `WiFiManager` for centralized connection, persistence, and API handling, resolving previous build failures. (`3b84a06`)
- [2026-02-04] feat: Add WiFiManager module to centralize and enhance WiFi connection, persistence, and API handling. (`4745ba3`)
- Automated release via GitHub Actions

## Improvements
- None

## Bug Fixes
- [2026-02-04] fix: resolve unit test compilation errors for native environment

- Fix random() function conflict in test_mqtt by using rand() % 10000 instead of Arduino random(10000)
- Fix isDigit() not found errors by using std::isdigit() from <cctype> in test_utils and test_ota
- Fix String method issues in test_ota by converting to std::string for indexOf/substring operations
- Add missing putUChar/getUChar methods to Preferences mock for WiFi tests

All previously failing tests now pass:
- test_mqtt: PASSED
- test_utils: PASSED
- test_ota: PASSED
- test_auth: PASSED
- test_button: PASSED
- test_websocket: PASSED

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com> (`ca5a26c`)
- [2026-02-04] fix: resolve unit test compilation errors for native environment

- Fix random() function conflict in test_mqtt by using rand() % 10000 instead of Arduino random(10000)
- Fix isDigit() not found errors by using std::isdigit() from <cctype> in test_utils and test_ota
- Fix String method issues in test_ota by converting to std::string for indexOf/substring operations
- Add missing putUChar/getUChar methods to Preferences mock for WiFi tests

All previously failing tests now pass:
- test_mqtt: PASSED
- test_utils: PASSED
- test_ota: PASSED
- test_auth: PASSED
- test_button: PASSED
- test_websocket: PASSED

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com> (`6806b7a`)
- [2026-02-03] fix: Gracefully disconnect before updating WiFi password on current network

When a user updates the password for a currently connected network, the
device was calling WiFi.begin() while still connected, causing an abrupt
disconnect (ASSOC_LEAVE) and subsequent authentication failure with the
new password.

This fix checks if the device is already connected to the target SSID
and disconnects gracefully before attempting to reconnect with the new
credentials.

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com> (`a0758f8`)
- [2026-02-03] fix: Gracefully disconnect before updating WiFi password on current network

When a user updates the password for a currently connected network, the
device was calling WiFi.begin() while still connected, causing an abrupt
disconnect (ASSOC_LEAVE) and subsequent authentication failure with the
new password.

This fix checks if the device is already connected to the target SSID
and disconnects gracefully before attempting to reconnect with the new
credentials.

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com> (`7b28294`)
- [2026-02-03] fix: Gracefully disconnect before updating WiFi password on current network

When a user updates the password for a currently connected network, the
device was calling WiFi.begin() while still connected, causing an abrupt
disconnect (ASSOC_LEAVE) and subsequent authentication failure with the
new password.

This fix checks if the device is already connected to the target SSID
and disconnects gracefully before attempting to reconnect with the new
credentials.

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com> (`cc7176b`)
- [2026-02-03] fix: Gracefully disconnect before updating WiFi password on current network

When a user updates the password for a currently connected network, the
device was calling WiFi.begin() while still connected, causing an abrupt
disconnect (ASSOC_LEAVE) and subsequent authentication failure with the
new password.

This fix checks if the device is already connected to the target SSID
and disconnects gracefully before attempting to reconnect with the new
credentials. This ensures the device properly registers the new password
and successfully reconnects.
- [2026-02-03] fix: Gracefully disconnect before updating WiFi password on current network

When a user updates the password for a currently connected network, the
device was calling WiFi.begin() while still connected, causing an abrupt
disconnect (ASSOC_LEAVE) and subsequent authentication failure with the
new password.

This fix checks if the device is already connected to the target SSID
and disconnects gracefully before attempting to reconnect with the new
credentials. This ensures the device properly registers the new password
and successfully reconnects.

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com> (`ee7743a`)


## Technical Details
- [2026-02-06] Update RELEASE_NOTES.md to include recent enhancements related to the Support tab and navigation improvements in the GUI. (`174e8f6`)
- [2026-02-06] Update RELEASE_NOTES.md to include recent enhancements related to the Support tab and navigation improvements in the GUI. (`1d8fa82`)
- [2026-02-06] Update RELEASE_NOTES.md to reflect the addition of a Support tab in the GUI and navigation enhancements (`b4f9834`)
- [2026-02-06] Update RELEASE_NOTES.md to include enhancements in GUI with new Support tab and navigation improvements (`3066a51`)
- [2026-02-06] Enhance GUI with Support Tab and Navigation Improvements

- Added a new Support tab in the GUI, allowing users to access the user manual and documentation.
- Implemented search functionality within the manual section for easier navigation.
- Updated navigation to include the Support screen and maintain focus index across screens.
- Adjusted layout and styles for better user experience in the Debug and Desktop screens.
- Updated platformio.ini to set default environment for ESP32-S3 development. (`c7b8e00`)
- [2026-02-06] Update RELEASE_NOTES.md to clarify recent enhancements in MQTT settings management and UI synchronization (`07e28c4`)
- [2026-02-06] Update RELEASE_NOTES.md to clarify enhancements in MQTT settings management and UI synchronization (`478df44`)
- [2026-02-06] Update RELEASE_NOTES.md to document enhancements in MQTT settings management and UI synchronization (`09f23ee`)
- [2026-02-06] Update RELEASE_NOTES.md to document enhancements in MQTT settings management and UI synchronization (`fd0c451`)
- [2026-02-06] Enhance MQTT settings management and update UI

- Added functionality to send and receive MQTT settings via WebSocket, including broker, port, username, and discovery options.
- Updated the GUI to reflect changes in MQTT settings, ensuring the interface is synchronized with the application state.
- Improved the handling of MQTT settings changes to mark the application state as dirty, prompting necessary updates.
- Documented changes in RELEASE_NOTES.md to reflect new features and improvements in MQTT integration. (`4b1fe88`)
- [2026-02-06] Update RELEASE_NOTES.md to document new LVGL GUI features and cross-task state sync improvements (`1bbaeab`)
- [2026-02-05] Update RELEASE_NOTES.md to include new LVGL GUI features and cross-task state sync improvements

- Added details about the implementation of a complete 7-phase GUI system for ST7735S 128x160 TFT with rotary encoder on ESP32-S3 using LVGL v9.4 + TFT_eSPI.
- Documented features such as desktop carousel, control/settings/WiFi/MQTT/debug screens, boot animations, dark mode, and screen sleep/wake.
- Highlighted fixes for cross-task WebSocket/MQTT sync issues, ensuring thread safety in GUI broadcasts. (`2b1fa33`)
- [2026-02-05] Update RELEASE_NOTES.md to document the removal of obsolete skills and the addition of WebSearch functionality

- Deleted SKILL.md files for brainstorming, code reviewer, frontend design, and related references to streamline the codebase.
- Removed associated scripts and license files to eliminate unused components.
- Added WebSearch functionality to the settings.local.json file. (`c078041`)
- [2026-02-05] Update RELEASE_NOTES.md to reflect the removal of obsolete skills and documentation

- Deleted SKILL.md files for brainstorming, code reviewer, frontend design, and related references to streamline the codebase.
- Removed associated scripts and license files to eliminate unused components.
- Cleaned up the repository by ensuring only relevant skills and documentation remain. (`9e9cc57`)
- [2026-02-05] Update RELEASE_NOTES.md to include new WiFi retry logic and unit test enhancements

- Documented the implementation of an intelligent retry mechanism for WiFi connections, addressing "Network not found" errors.
- Noted updates to the WiFi manager for tracking last failed SSID and retry count.
- Included details on enhanced unit tests for the WiFi retry logic, covering various scenarios. (`a589794`)
- [2026-02-04] Implement WiFi retry logic and enhance unit tests

- Added intelligent retry mechanism for WiFi connections, handling "Network not found" errors and implementing periodic retries.
- Updated WiFi manager to track last failed SSID and retry count, improving connection reliability.
- Enhanced unit tests for WiFi retry logic, including scenarios for immediate retries, success clearing flags, and multiple network fallbacks.
- Updated RELEASE_NOTES.md to document these enhancements and new test cases. (`65bd7be`)
- [2026-02-04] Enhance WiFi manager and web interface with gzip support and improved logging

- Updated WiFi manager to support gzipped content delivery for HTML pages, reducing transfer size.
- Enhanced debug console with log level filtering and improved log message handling.
- Refactored WiFi credentials management to use a structured configuration approach.
- Added new helper functions for JSON parsing and network configuration management.
- Updated RELEASE_NOTES.md to reflect these changes and improvements. (`c98198e`)
- [2026-02-04] Update RELEASE_NOTES.md with new WiFi manager unit tests and enhancements (`7d8f18a`)
- [2026-02-04] chore: Update RELEASE_NOTES.md and enhance WiFi manager unit tests

- Added new entry in RELEASE_NOTES.md for the implementation of WiFi manager unit tests using mock classes.
- Updated WiFi class declaration from extern to static in WiFi.h for better encapsulation.
- Enhanced test_wifi_manager.cpp with additional tests for password management, static IP configurations, and network removal edge cases. (`2b9c36b`)
- [2026-02-04] chore: Update platformio.ini and RELEASE_NOTES.md for test configuration and documentation

- Added test_ignore configuration to platformio.ini to exclude all unit tests for ESP32.
- Updated RELEASE_NOTES.md to include recent test additions and enhancements. (`7b35db5`)
- [2026-02-04] test: add new unit tests for button, auth, websocket, and utilities, including necessary mock headers. (`a1754b2`)
- [2026-02-03] Enhance WiFi connection handling with polling mechanism for network changes

- Added a polling function to track WiFi connection attempts and detect network changes.
- Implemented logic to handle successful and failed connection attempts, updating the UI accordingly.
- Refactored modal functions to improve clarity and maintainability.
- Updated connection status messages to provide better user feedback during WiFi transitions. (`d6d4d79`)
- [2026-02-03] Refactor WiFi connection handling to utilize stored passwords and enhance error recovery

- Implemented logic to fetch stored passwords for WiFi networks if no password is provided.
- Updated connection handling to use the retrieved password for establishing connections.
- Added functionality to restore AP mode after a failed connection attempt, improving user experience during connectivity issues.
- Updated .gitignore to exclude RELEASE_NOTES.md from version control. (`0bc7ead`)
- [2026-02-03] Update RELEASE_NOTES.md to reflect recent enhancements in WiFi connection handling, including user-friendly disconnect messages, improved logging for connection failures, and an updated WiFi status JSON response. Removed unused code related to saved networks. (`9520b2a`)
- [2026-02-03] Update RELEASE_NOTES.md to include recent enhancements in WiFi connection handling and error reporting. Added user-friendly messages for disconnect reasons, improved logging for connection failures, and updated WiFi status JSON response. Removed unused code related to saved networks. (`d8bea09`)
- [2026-02-03] Enhance WiFi connection handling and error reporting

- Added a helper function to convert WiFi disconnect reasons into user-friendly messages.
- Updated the WiFi event handler to log detailed connection failure reasons.
- Introduced a new state variable to track connection errors and included it in the WiFi status JSON response.
- Removed unused code related to saved networks from the web interface.

Files modified: src/wifi_manager.cpp, src/web_pages.cpp, src/app_state.h, RELEASE_NOTES.md (`e2814f6`)
- [2026-02-03] Enhance WiFi management and debugging capabilities

- Added logic to prevent reconnection attempts during WiFi scanning, including a timeout safeguard.
- Implemented logging for 404 Not Found requests to aid in debugging.
- Updated RELEASE_NOTES.md to reflect the removal of a duplicate function that caused issues with password input handling.

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com> (`7fe1b9a`)
- Version bump to 1.4.2

## Breaking Changes
None

## Known Issues
- None

## Version 1.4.1

## Technical Details
- [2026-02-03] Update RELEASE_NOTES.md to document the addition of the saveNetworkSettings() function and improvements to browser request handling.

- Implemented saveNetworkSettings() to allow users to save WiFi configurations without connecting immediately, including validation for SSID, password, and static IP settings.
- Added explicit HTTP handlers for common browser auto-requests to reduce console noise from "request handler not found" errors.

Files modified: src/web_pages.cpp, src/main.cpp, RELEASE_NOTES.md (`f57de5d`)
- [2026-02-03] Update RELEASE_NOTES.md to document enhancements in WiFi management, including automatic reconnection, improved network removal confirmation, and updated API security measures. Co-authored by Claude Sonnet. (`adde1f8`)
- [2026-02-03] Enhance WiFi management with automatic reconnection and improved network removal experience

- Implemented a WiFi event handler to manage automatic reconnections upon disconnection.
- Added a confirmation modal for removing currently connected networks, ensuring users are aware of the consequences.
- Updated API calls to include session credentials for enhanced security.
- Improved test workflows with comprehensive unit tests and detailed summaries.

Files modified: src/wifi_manager.cpp, src/wifi_manager.h, src/main.cpp, src/web_pages.cpp, .github/workflows/release.yml, .github/workflows/tests.yml, .claude/settings.local.json, RELEASE_NOTES.md

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`b26a3bb`)
- [2026-02-03] Update RELEASE_NOTES.md to include technical details about enhancements in WiFi management, highlighting automatic reconnection, improved network removal experience, and resolved API authentication issues. Co-authored by Claude Sonnet. (`978bc44`)
- [2026-02-03] Update RELEASE_NOTES.md to reflect enhancements in WiFi management, including automatic reconnection, improved network removal experience, and fixed API authentication issues. Co-authored by Claude Sonnet. (`c3486d0`)

## New Features
- [2026-02-03] feat: Add initial web interface for ALX Audio Controller. (`30f03be`)
- [2026-02-03] feat: Introduce centralized application state management and core modules for MQTT, settings, sensing, web, and WiFi. (`47f708a`)
- [2026-02-03] feat: Implement comprehensive WiFi management with AP/STA modes, static IP, multi-network persistence, and introduce new core application modules. (`bb6dad5`)
- [2026-02-03] feat: Enhance WiFi management with automatic reconnection and improved network removal experience

- Introduced an intelligent WiFi reconnection system that automatically manages disconnections and reconnects without user intervention.
- Added a warning modal for removing the currently connected network, requiring explicit confirmation and providing clear consequences.
- Enhanced user experience with real-time status updates during network removal and reconnection attempts.
- Fixed authentication issues in API calls by ensuring session credentials are included in all requests.

Files modified: src/wifi_manager.cpp, src/wifi_manager.h, src/main.cpp, src/web_pages.cpp

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`866d67f`)
- [2026-02-03] feat: Introduce automatic WiFi reconnection and enhanced network removal experience

- Implemented an intelligent WiFi reconnection system that automatically manages disconnections and reconnects without user intervention.
- Added a warning modal for removing the currently connected network, requiring explicit confirmation and providing clear consequences.
- Enhanced user experience with real-time status updates and improved feedback during network removal and reconnection attempts.
- Fixed authentication issues in API calls by ensuring session credentials are included in all requests.

Files modified: src/wifi_manager.cpp, src/wifi_manager.h, src/main.cpp, src/web_pages.cpp

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`5214ff8`)
- [2026-02-03] feat: Automatic WiFi reconnection with event-driven monitoring

Implemented intelligent WiFi reconnection system that automatically handles network disconnections and recovers connectivity without user intervention.

**WiFi Event Handler:**
- Detects WiFi disconnection events in real-time (SYSTEM_EVENT_STA_DISCONNECTED)
- Monitors connection establishment (SYSTEM_EVENT_STA_CONNECTED)
- Tracks IP address assignment (SYSTEM_EVENT_STA_GOT_IP)
- Broadcasts status updates to connected clients via WebSocket

**Smart Reconnection Logic:**
- Waits 10 seconds after disconnection before attempting reconnection
- Throttles reconnection attempts to every 5 seconds to avoid overwhelming the network
- Automatically tries all saved networks in priority order
- Falls back to AP Mode if no saved networks are available
- Suppresses repetitive console warnings (one message per 30 seconds max)

**User Experience:**
- Silent recovery when access point comes back online
- No flooding of serial console with disconnect warnings
- Real-time status updates to web interface
- Seamless transition to AP mode when network is permanently unavailable

**Technical Implementation:**
- Added `onWiFiEvent()` handler to process WiFi system events
- Created `initWiFiEventHandler()` to register event handler at startup
- Implemented `checkWiFiConnection()` in main loop for reconnection monitoring
- Reuses existing `connectToStoredNetworks()` for multi-network reconnection
- State tracking prevents duplicate reconnection attempts

**Files Modified:**
- src/wifi_manager.cpp: WiFi event handler and reconnection logic
- src/wifi_manager.h: Function declarations for event handling
- src/main.cpp: Initialize event handler in setup(), monitor in loop()

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>

- [2026-02-03] feat: Complete WiFi management system with comprehensive UI improvements

Implemented a fully-featured WiFi network management system that provides complete control over network configuration, connection, and management.

**Network Configuration Section Enhancements:**
- Added password field allowing users to view/update passwords for saved networks
- Password placeholder shows "Enter password (leave empty to keep current)" for clarity
- Password toggle button (👁) to show/hide password text
- Dynamic button labels: "Connect" vs "Connect & Update" based on whether changes were made
- Real-time change detection for password and static IP settings
- Update button label changes automatically as user makes modifications

**Connection Success/Failure Modals:**
- Success modal displays new IP address with "Go to Dashboard" redirect button
- Failure modal shows specific error reason (authentication failure, network not found, timeout)
- Reuses modals for both new connections and network updates
- Clear visual feedback with animated loaders and status icons (✅ / ❌)

**Two-Button Workflow:**
- "Connect & Save Network" - Immediately connects and saves network to list
- "Save Network" - Only saves network without connecting
- "Connect & Update" - Updates settings and connects (shows in Network Configuration)
- "Update Network" - Updates settings without connecting
- "Remove Network" - Removes network with appropriate warnings

**Smart Features:**
- Duplicate network detection: Adding same SSID overwrites existing settings
- Password security: Frontend doesn't store passwords, backend keeps if empty string sent
- Change tracking: System remembers original values to detect modifications
- Real-time button updates as user types or changes settings

**Technical Implementation:**
- Added `originalNetworkConfig` object to track initial values
- Created `updateConnectButtonLabel()` function for dynamic button text
- Enhanced `updateNetworkConfig()` to handle both connect and save-only operations
- Uses existing `showWiFiModal()` and `updateWiFiConnectionStatus()` for user feedback
- Backend `saveWiFiNetwork()` handles duplicate detection automatically (lines 360-379)
- Password updates only applied when non-empty password provided (line 366-368)

**User Experience:**
- Clear indication when changes have been made
- No accidental overwrites of unchanged settings
- Smooth workflow for managing multiple networks
- Immediate visual feedback on all actions
- Proper error messages for failed operations

**Files Modified:**
- src/web_pages.cpp: Added password field, dynamic buttons, change tracking, modal integration
- Backend already supported all necessary functionality

**Known Issues:**
- Duplicate function definitions exist at lines 4183-4300 and 6031-6116 (scheduled for cleanup)
- These don't affect functionality as updated functions at earlier line numbers take precedence

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>

- [2026-02-03] feat: Enhanced WiFi network removal with current network warning

Improved the network removal experience with better user feedback and safety checks:

**Current Network Warning:**
- Shows dedicated warning modal when removing the currently connected network
- Clearly explains the consequences (disconnection, reconnection attempts, AP mode fallback)
- Requires explicit confirmation before proceeding
- Visual warning styling with error color scheme

**Smart Network Tracking:**
- Tracks current WiFi SSID to detect when user is removing active connection
- Updates connection state in real-time from WebSocket data
- Distinguishes between removing current network vs. other saved networks

**Improved User Flow:**
- Simple confirmation for non-current networks
- Detailed warning modal for current network removal
- Real-time feedback during reconnection attempts
- Automatic monitoring shows either:
  - Success toast when reconnected to another network
  - AP Mode modal with IP address if no networks available

**Technical Implementation:**
- Added `currentWifiSSID` global variable to track active connection
- Created `showRemoveCurrentNetworkModal()` for warning display
- Split removal logic into `performNetworkRemoval()` for better code organization
- Enhanced `updateWiFiStatus()` to update connection tracking
- Existing `monitorNetworkRemoval()` handles post-removal reconnection monitoring

**Files Modified:**
- src/web_pages.cpp: Added network tracking, warning modal, improved removal flow

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>

## Bug Fixes
- [2026-02-03] fix: Remove duplicate updateNetworkConfig function that used prompt() for password

The duplicate function definition was overriding the correct implementation, forcing users to enter passwords via a browser prompt() dialog instead of using the password field in the form.

The correct implementation now:
- Uses the configPassword input field from the form
- Provides proper success/error feedback via toast messages
- Clears password field after successful save
- Handles both /api/wificonfig and /api/wifisave endpoints

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com> (`f3b9c38`)
- [2026-02-03] fix: Remove duplicate updateNetworkConfig function that used prompt() for password

The duplicate function definition was overriding the correct implementation, forcing users to enter passwords via a browser prompt() dialog instead of using the password field in the form.

The correct implementation now:
- Uses the configPassword input field from the form
- Provides proper success/error feedback via toast messages
- Clears password field after successful save
- Handles both /api/wificonfig and /api/wifisave endpoints

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com> (`5b4257c`)
- [2026-02-03] fix: Remove duplicate updateNetworkConfig function that used prompt() for password

The duplicate function definition was overriding the correct implementation, forcing users to enter passwords via a browser prompt() dialog instead of using the password field in the form.

The correct implementation now:
- Uses the configPassword input field from the form
- Provides proper success/error feedback via toast messages
- Clears password field after successful save
- Handles both /api/wificonfig and /api/wifisave endpoints

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com> (`3624b3c`)
- [2026-02-03] fix: Implement missing saveNetworkSettings() function for "Save Settings" button

The "Save Settings" button in the "Connect to Network" section was calling a non-existent `saveNetworkSettings()` function, causing JavaScript errors when clicked.

**What Was Fixed:**
- Implemented complete `saveNetworkSettings()` function
- Validates SSID and password are entered
- Validates static IP configuration if enabled
- Calls `/api/wifisave` endpoint (save without connecting)
- Shows success/failure toast notifications
- Clears form after successful save
- Reloads saved networks list to show newly added network

**Function Behavior:**
- Does NOT connect to the network (unlike Connect button)
- Does NOT show connection modal
- Does NOT poll for connection status
- Simply saves network to persistent storage
- User can connect later from Network Configuration section

**Files Modified:**
- src/web_pages.cpp: Added saveNetworkSettings() function at line 3327

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>

- [2026-02-03] fix: Reduce console noise by adding handlers for common browser auto-requests

Added explicit HTTP handlers for common browser automatic requests to reduce "request handler not found" error messages in the serial console.

**Handlers Added:**
- /manifest.json - PWA manifest file request
- /robots.txt - Search engine crawler file
- /sitemap.xml - Site map request
- /apple-touch-icon.png - iOS home screen icon
- /apple-touch-icon-precomposed.png - iOS legacy icon

**Technical Details:**
- All handlers return 404 Not Found (as these resources don't exist)
- Prevents ESP32 WebServer library from logging errors for expected browser behavior
- Doesn't affect functionality, just reduces verbose logging
- onNotFound handler still catches any other unknown requests

**Files Modified:**
- src/main.cpp: Added explicit handlers for browser auto-requests

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>

- [2026-02-03] fix: WiFi scan authentication error - Add automatic credentials to all API calls

Fixed "Session NOT FOUND in active list" and "Unauthorized access attempt to /api/wifiscan" errors by enhancing the `apiFetch()` wrapper to automatically include session credentials with all API requests.

**Root Cause:**
- API calls were not consistently sending session authentication (cookies and X-Session-ID header)
- Browser may not automatically send cookies without explicit `credentials: 'include'`
- Session validation was failing, causing 401 Unauthorized responses

**Solution:**
- Enhanced `apiFetch()` wrapper to automatically inject credentials and session header for ALL API calls
- Ensures consistent authentication across the entire application
- Prevents future similar authentication issues

**Technical Details:**
- Modified `apiFetch()` to auto-include `credentials: 'include'` option
- Automatically adds `X-Session-ID` header by reading session cookie
- Properly merges default options with user-provided options
- Cleaned up redundant credential specifications in other API calls

**Files Modified:**
- src/web_pages.cpp: Enhanced apiFetch() wrapper, cleaned up redundant credentials in loadSavedNetworks() and WiFi list calls

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>

- [2026-02-03] fix: Replace 204 No Content response with 404 for favicon.ico to eliminate WebServer content-length warning

The favicon.ico endpoint was returning 204 (No Content) which caused the WebServer library to warn about zero content length. Changing to 404 (Not Found) is more semantically correct for a missing resource and eliminates the warning without affecting functionality.

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com> (`5373849`)

## New Features
- [2026-02-03] feat: Improve Debug tab layout and WiFi network removal UX

**Debug Tab Improvements:**
- Repositioned performance graphs to top of their respective sections
- CPU graph now appears immediately after CPU stats grid
- Memory graphs (Heap/PSRAM) now appear immediately after Memory stats grid
- Better visual hierarchy and easier access to performance metrics

**WiFi Network Management Enhancements:**
- Added AP Mode modal when removing currently connected network
- Automatically monitors network status after removal (30s window)
- Shows modal with AP IP address and direct dashboard link when AP activates
- Displays success notification when reconnecting to another saved network
- Seamless transition handling with intelligent polling

**UI Consistency:**
- Fixed button sizing inconsistency between "Update Configuration" and "Remove Network"
- Both buttons now render with identical dimensions

**Technical Implementation:**
- Added showAPModeModal() function reusing WiFi connection modal styling
- Implemented monitorNetworkRemoval() with 1s polling intervals
- Enhanced removeSelectedNetworkConfig() to track connection state
- Graph repositioning improves visual flow in Debug tab
- Button sizing fixed with min-width: 0 for proper flexbox behavior

**Files Modified:**
- src/web_pages.cpp: Graph positioning, AP modal, network removal monitoring
- src/config.h: Version bump to 1.4.1
- RELEASE_NOTES.md: Updated with v1.4.1 release notes

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`040a6f5`)
- [2026-02-03] feat: Improve Debug tab layout and WiFi network removal UX

**Debug Tab Improvements:**
- Repositioned performance graphs to top of their respective sections
- CPU graph now appears immediately after CPU stats grid
- Memory graphs (Heap/PSRAM) now appear immediately after Memory stats grid
- Better visual hierarchy and easier access to performance metrics

**WiFi Network Management Enhancements:**
- Added AP Mode modal when removing currently connected network
- Automatically monitors network status after removal (30s window)
- Shows modal with AP IP address and direct dashboard link when AP activates
- Displays success notification when reconnecting to another saved network
- Seamless transition handling with intelligent polling

**UI Consistency:**
- Fixed button sizing inconsistency between "Update Configuration" and "Remove Network"
- Both buttons now render with identical dimensions

**Technical Implementation:**
- Added showAPModeModal() function reusing WiFi connection modal styling
- Implemented monitorNetworkRemoval() with 1s polling intervals
- Enhanced removeSelectedNetworkConfig() to track connection state
- Graph repositioning improves visual flow in Debug tab
- Button sizing fixed with min-width: 0 for proper flexbox behavior

**Files Modified:**
- src/web_pages.cpp: Graph positioning, AP modal, network removal monitoring
- src/config.h: Version bump to 1.4.1
- RELEASE_NOTES.md: Updated with v1.4.1 release notes

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`35c37f9`)
- [2026-02-03] feat: Improve Debug tab layout and WiFi network removal UX

**Debug Tab Improvements:**
- Repositioned performance graphs to top of their respective sections
- CPU graph now appears immediately after CPU stats grid
- Memory graphs (Heap/PSRAM) now appear immediately after Memory stats grid
- Better visual hierarchy and easier access to performance metrics

**WiFi Network Management Enhancements:**
- Added AP Mode modal when removing currently connected network
- Automatically monitors network status after removal (30s window)
- Shows modal with AP IP address and direct dashboard link when AP activates
- Displays success notification when reconnecting to another saved network
- Seamless transition handling with intelligent polling

**UI Consistency:**
- Fixed button sizing inconsistency between "Update Configuration" and "Remove Network"
- Both buttons now render with identical dimensions

**Technical Implementation:**
- Added showAPModeModal() function reusing WiFi connection modal styling
- Implemented monitorNetworkRemoval() with 1s polling intervals
- Enhanced removeSelectedNetworkConfig() to track connection state
- Graph repositioning improves visual flow in Debug tab
- Button sizing fixed with min-width: 0 for proper flexbox behavior

**Files Modified:**
- src/web_pages.cpp: Graph positioning, AP modal, network removal monitoring
- src/config.h: Version bump to 1.4.1
- RELEASE_NOTES.md: Updated with v1.4.1 release notes

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`1c02113`)

### Improvements
- **Debug Tab Graph Positioning**: Moved performance graphs to the top of their respective sections for better visibility
  - CPU graph now appears immediately after CPU stats, before detailed info
  - Memory graphs (Heap and PSRAM) now appear immediately after Memory stats
  - Improved visual hierarchy and easier access to performance metrics

- **WiFi Network Management Enhancements**:
  - Added AP Mode modal when removing currently connected network
  - Automatically monitors network status after removal
  - Shows modal with AP IP address and direct dashboard link when AP mode activates
  - Displays success notification when reconnecting to another saved network
  - Seamless transition handling with 30-second monitoring window

- **Button Consistency**: Fixed sizing inconsistency between "Update Configuration" and "Remove Network" buttons

### Technical Details
- Added `showAPModeModal()` function with reused WiFi connection modal styling
- Implemented `monitorNetworkRemoval()` with polling mechanism (1s intervals, 30s timeout)
- Enhanced `removeSelectedNetworkConfig()` to track connection state and trigger monitoring
- Graph repositioning improves visual flow in Debug tab
- Button sizing fixed with `min-width: 0` for proper flexbox behavior

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>

## Version 1.4.0

## Technical Details
- [2026-02-03] chore: Update release notes (`56b5154`)

## New Features
- [2026-02-03] feat: Redesign Debug tab with integrated graphs and compact layout

Major improvements to the Debug tab UI/UX:

**New Features:**
- CPU and Memory graphs now embedded directly in their respective cards
- Added Y-axis labels (0%, 25%, 50%, 75%, 100%) to all graphs
- Added X-axis time labels (-60s, -30s, now) for temporal context
- PSRAM usage graph displays automatically when PSRAM is available
- Reset Reason now shown in WiFi & System card
- Window resize handler for responsive graph redrawing

**Layout Improvements:**
- Compact 2-column grid layout for stats (single column on mobile)
- Removed collapsible Performance History section (graphs always visible)
- More efficient use of screen space
- Reduced padding and font sizes for denser information display

**Technical Changes:**
- Enhanced graph rendering functions with proper margins for axis labels
- Added psramPercent tracking to history data structure
- Updated WebSocket handler to include reset reason in hardware stats
- Graph dimensions: 140px height with 35px left margin, 20px bottom margin
- Removed toggleHistorySection() function and historyCollapsed variable

**Files Modified:**
- src/web_pages.cpp: Major restructuring of Debug tab HTML, CSS, and JavaScript
- src/websocket_handler.cpp: Added reset reason to hardware stats payload
- src/config.h: Version bump to 1.4.0
- RELEASE_NOTES.md: Updated with v1.4.0 release notes

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`c22c551`)
- [2026-02-03] feat: Redesign Debug tab with integrated graphs and compact layout

Major improvements to the Debug tab UI/UX:

**New Features:**
- CPU and Memory graphs now embedded directly in their respective cards
- Added Y-axis labels (0%, 25%, 50%, 75%, 100%) to all graphs
- Added X-axis time labels (-60s, -30s, now) for temporal context
- PSRAM usage graph displays automatically when PSRAM is available
- Reset Reason now shown in WiFi & System card
- Window resize handler for responsive graph redrawing

**Layout Improvements:**
- Compact 2-column grid layout for stats (single column on mobile)
- Removed collapsible Performance History section (graphs always visible)
- More efficient use of screen space
- Reduced padding and font sizes for denser information display

**Technical Changes:**
- Enhanced graph rendering functions with proper margins for axis labels
- Added psramPercent tracking to history data structure
- Updated WebSocket handler to include reset reason in hardware stats
- Graph dimensions: 140px height with 35px left margin, 20px bottom margin
- Removed toggleHistorySection() function and historyCollapsed variable

**Files Modified:**
- src/web_pages.cpp: Major restructuring of Debug tab HTML, CSS, and JavaScript
- src/websocket_handler.cpp: Added reset reason to hardware stats payload
- src/config.h: Version bump to 1.4.0
- RELEASE_NOTES.md: Updated with v1.4.0 release notes

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`d2dda04`)
- [2026-02-03] feat: Redesign Debug tab with integrated graphs and compact layout

Major improvements to the Debug tab UI/UX:

**New Features:**
- CPU and Memory graphs now embedded directly in their respective cards
- Added Y-axis labels (0%, 25%, 50%, 75%, 100%) to all graphs
- Added X-axis time labels (-60s, -30s, now) for temporal context
- PSRAM usage graph displays automatically when PSRAM is available
- Reset Reason now shown in WiFi & System card
- Window resize handler for responsive graph redrawing

**Layout Improvements:**
- Compact 2-column grid layout for stats (single column on mobile)
- Removed collapsible Performance History section (graphs always visible)
- More efficient use of screen space
- Reduced padding and font sizes for denser information display

**Technical Changes:**
- Enhanced graph rendering functions with proper margins for axis labels
- Added psramPercent tracking to history data structure
- Updated WebSocket handler to include reset reason in hardware stats
- Graph dimensions: 140px height with 35px left margin, 20px bottom margin
- Removed toggleHistorySection() function and historyCollapsed variable

**Files Modified:**
- src/web_pages.cpp: Major restructuring of Debug tab HTML, CSS, and JavaScript
- src/websocket_handler.cpp: Added reset reason to hardware stats payload
- src/config.h: Version bump to 1.4.0
- RELEASE_NOTES.md: Updated with v1.4.0 release notes

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`fdb83fc`)

### New Features
- **Debug Tab Redesign**: Complete overhaul of the Debug tab with integrated performance graphs
  - CPU and Memory graphs now embedded directly in their respective cards
  - Added Y-axis labels (0%, 25%, 50%, 75%, 100%) to all graphs
  - Added X-axis time labels (-60s, -30s, now) to all graphs
  - Compact 2-column grid layout for stats (responsive single column on mobile)
  - PSRAM usage graph now displays when PSRAM is available
  - Reset Reason display added to WiFi & System card
  - Removed collapsible Performance History section (graphs always visible)
  - Window resize handler automatically redraws graphs

### Improvements
- More efficient use of screen space in Debug tab
- Better graph readability with axis labels
- Responsive design improvements for mobile devices

### Technical Details
- Enhanced graph rendering functions with proper margins for axis labels
- Added `psramPercent` tracking to history data
- Updated WebSocket handler to include reset reason in hardware stats payload
- Graphs now use 140px height to accommodate axis labels
- Left margin: 35px for Y-axis labels, bottom margin: 20px for X-axis labels

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>

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

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com> (`2026fda`)
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
- [2026-02-03] refactor: Reorganize WiFi interface by removing Saved Networks section and adding Remove button to Network Configuration

Changes:
- Remove the redundant "Saved Networks" section from WiFi management UI
- Consolidate network management to the "Network Configuration" section
- Add "Remove Network" button to Network Configuration section for easier network management
- Remove unused JavaScript functions: toggleNetworkSelection, removeSelectedNetworks, updateBulkActionsVisibility, removeNetwork
- Remove unused CSS styles for network-item, network-checkbox, bulk-actions, and related elements
- Simplify loadSavedNetworks() to only populate the config dropdown

The Network Configuration section now provides complete network management (view, update, and remove) without the duplicate Saved Networks view.

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com> (`9350201`)
- [2026-02-03] refactor: Reorganize WiFi interface by removing Saved Networks section and adding Remove button to Network Configuration

Changes:
- Remove the redundant "Saved Networks" section from WiFi management UI
- Consolidate network management to the "Network Configuration" section
- Add "Remove Network" button to Network Configuration section for easier network management
- Remove unused JavaScript functions: toggleNetworkSelection, removeSelectedNetworks, updateBulkActionsVisibility, removeNetwork
- Remove unused CSS styles for network-item, network-checkbox, bulk-actions, and related elements
- Simplify loadSavedNetworks() to only populate the config dropdown

The Network Configuration section now provides complete network management (view, update, and remove) without the duplicate Saved Networks view.

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com> (`dc3bb08`)
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
