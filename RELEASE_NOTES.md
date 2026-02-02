# Release Notes

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
