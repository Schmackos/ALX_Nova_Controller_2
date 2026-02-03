# Unit Test Execution Guide

## Quick Start

Run all native tests (native platform is where tests execute on the host machine without ESP32 hardware):

```bash
pio test -e native
```

## Run Tests by Phase

### Phase 1: Utils & Auth (20 tests)
```bash
pio test -e native -f test_utils
pio test -e native -f test_auth
```

### Phase 2: WiFi & MQTT (30 tests)
```bash
pio test -e native -f test_wifi
pio test -e native -f test_mqtt
```

### Phase 3: Settings, OTA, Button & WebSocket (41 tests)
```bash
pio test -e native -f test_settings
pio test -e native -f test_ota
pio test -e native -f test_button
pio test -e native -f test_websocket
```

## Run Individual Test Module

```bash
# Utils module - version comparison, RSSI, reset reasons
pio test -e native -f test_utils

# Auth handler - session management, password storage
pio test -e native -f test_auth

# WiFi manager - network storage, static IP, scanning
pio test -e native -f test_wifi

# MQTT handler - broker connection, publishing, HA discovery
pio test -e native -f test_mqtt

# Settings manager - persistence, factory reset
pio test -e native -f test_settings

# OTA updater - version comparison, SHA256 verification
pio test -e native -f test_ota

# Button handler - state machine, debouncing, clicks
pio test -e native -f test_button

# WebSocket handler - broadcasting, client management
pio test -e native -f test_websocket
```

## Test Counts

| Module | Tests | File |
|--------|-------|------|
| Utils | 15 | `test/test_utils/test_utils.cpp` |
| Auth | 15 | `test/test_auth/test_auth_handler.cpp` |
| WiFi | 17 | `test/test_wifi/test_wifi_manager.cpp` |
| MQTT | 13 | `test/test_mqtt/test_mqtt_handler.cpp` |
| Settings | 8 | `test/test_settings/test_settings_manager.cpp` |
| OTA | 15 | `test/test_ota/test_ota_updater.cpp` |
| Button | 10 | `test/test_button/test_button_handler.cpp` |
| WebSocket | 13 | `test/test_websocket/test_websocket_handler.cpp` |
| **TOTAL** | **106** | **8 test files** |

## Test Output

When tests run, you'll see:
```
Testing: test_utils
test_version_comparison_equal ..... PASS
test_version_comparison_less ..... PASS
test_version_comparison_greater ..... PASS
...
Tests run: 15
PASS: 15
FAIL: 0
```

## Existing Tests

The project already has Smart Sensing tests:
- `test/test_smart_sensing/test_smart_sensing_logic.cpp` - 10 logic tests
- `test/test_api/test_smart_sensing_api.cpp` - 13 API tests

Run them with:
```bash
pio test -e native -f test_smart_sensing_logic
pio test -e native -f test_smart_sensing_api
```

## Mock Infrastructure

All tests use mock libraries (no hardware required):

### Location: `test/test_mocks/`
- `Arduino.h` - GPIO and timing mocks
- `Preferences.h` - NVS storage mock
- `WiFi.h` - WiFi library mock
- `IPAddress.h` - IP address class
- `PubSubClient.h` - MQTT client mock
- `esp_random.h` - Random number generation

These mocks are automatically included via:
```cpp
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
// ... etc
#endif
```

## Platform Configuration

Tests run on the native platform (host machine) for speed and simplicity:

```ini
[env:native]
platform = native
test_framework = unity
```

## Expected Results

### All Tests Pass ✅
```
Total tests run: 106
PASS: 106
FAIL: 0
Time: < 10 seconds
```

### Code Coverage
Tests cover approximately 65-70% of critical code:
- ✅ Core business logic
- ✅ HTTP/MQTT API handlers
- ✅ Input validation
- ✅ Error handling
- ✅ State management

### Not Covered (by design)
- Hardware-specific ESP32 features (reserved for device tests)
- Network I/O operations (mocked)
- Real MQTT broker connections (mocked)
- Actual firmware downloads (mocked)

## Debugging Failed Tests

If a test fails, check:

1. **Test output** - Shows which assertion failed
2. **Mock state** - Verify mocks are reset in `setUp()`
3. **Test logic** - Review Arrange-Act-Assert structure
4. **Platform** - Ensure running with `-e native`

Example failure:
```
Testing: test_version_comparison_equal
test_version_comparison_equal ..... FAIL at /home/user/test/test_utils/test_utils.cpp:123
Expected: 0
Actual: 1
```

Check the version comparison logic and mock reset.

## Adding New Tests

To add tests for a module:

1. Create test file: `test/test_module/test_module.cpp`
2. Include mocks: `#include "../test_mocks/Arduino.h"` etc.
3. Define test state namespace
4. Implement `setUp()` and `tearDown()`
5. Write test functions: `void test_something(void)`
6. Add to test runner: `RUN_TEST(test_something);`
7. Provide both Arduino and native platform support

## CI/CD Integration

These tests are ready for GitHub Actions or other CI systems:

```yaml
- name: Run Unit Tests
  run: pio test -e native
```

## Test Execution Time

- Total: < 10 seconds
- Per module: < 2 seconds
- Suitable for pre-commit hooks and CI/CD

## Resources

- Unity Test Framework: http://www.throwtheswitch.org/unity
- PlatformIO Testing: https://docs.platformio.org/en/latest/advanced/test-framework.html
- Project test docs: `test/README.md`

## Need Help?

Check the test files for examples:
- Session management: `test/test_auth/test_auth_handler.cpp`
- NVS storage: `test/test_mqtt/test_mqtt_handler.cpp`
- State machines: `test/test_button/test_button_handler.cpp`
- JSON handling: `test/test_websocket/test_websocket_handler.cpp`
