# Version 1.2.0 Release Notes

## 🚀 Major Architecture Refactoring

This release introduces a comprehensive rewrite of the firmware architecture to improve reliability, maintainability, and concurrency.

### Core Changes
- **AppState Singleton**: Centralized application state management with predictable access and dirty-flag change detection.
- **Finite State Machine (FSM)**: Implemented FSM to strictly manage application modes (Idle, Sensing, Web Config, OTA, Error).
- **FreeRTOS Integration**: Migrated main loop to dedicated FreeRTOS tasks:
  - `SmartSensing` (Core 0, High Priority): Audio detection and relay control.
  - `WebServer` (Core 1, Medium Priority): UI and API handling.
  - `MQTT` (Core 1, Medium Priority): Cloud connectivity.
  - `OTA` (Core 1, Low Priority): Update checks.

### Filesystem Migration (Breaking Change)
- **LittleFS**: Migrated from legacy SPIFFS to LittleFS for better performance and future-proofing.
- **⚠️ IMPORTANT**: This change formats the filesystem. **Saved settings (WiFi credentials, MQTT config) will be reset.** You will need to re-configure the device via the AP mode.

### Reliability Improvements
- **Exponential Backoff**: Implemented smart reconnection logic for WiFi and MQTT (1s -> 2s ... -> 60s) to reduce network spam.
- **Checksum Validation**: Added CRC checks for settings and SHA256 validation for OTA firmware updates.
- **Watchdog Timers**: Added task-specific watchdogs to detect and recover from freeze states.

### Debugging
- **Log Levels**: Added hierarchical logging (DEBUG, INFO, WARN, ERROR) with WebSocket broadcasting.
- **Color Coding**: Serial output is now color-coded for easier debugging.

## Technical Details
- Firmware version bumped to 1.2.0
- Pin definitions moved to build flags for easier board adaptation
- Refactored `main.cpp` to be a lightweight task scheduler

## Known Issues
- Web assets are currently served from PROGMEM (will be moved to LittleFS in v1.3.0).
