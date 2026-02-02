# ALX Nova Controller - Test Suite

This directory contains automated tests for the ESP32 controller firmware.

## Test Structure

### Tier 1: Core Logic Tests (Unit Tests)
Located in `test_smart_sensing/`
- **test_smart_sensing_logic.cpp** - Tests for smart sensing timer logic, mode switching, voltage detection
- Tests run without hardware dependencies using mocks
- 10 test cases covering critical timer behavior

### Tier 2: API Tests (Integration Tests)
Located in `test_api/`
- **test_smart_sensing_api.cpp** - Tests for HTTP API endpoints
- Tests JSON parsing, validation, error handling
- 13 test cases covering all API scenarios

### Test Mocks
Located in `test_mocks/`
- **Arduino.h/cpp** - Mock Arduino functions (millis, analogRead, digitalWrite, etc.)
- Allows tests to run on native platform without ESP32 hardware

## Running Tests

### Run All Tests (Native Platform)
```bash
pio test -e native
```

### Run Specific Test
```bash
# Run only smart sensing logic tests
pio test -e native -f test_smart_sensing

# Run only API tests
pio test -e native -f test_api
```

### Run Tests on ESP32 Hardware
```bash
pio test -e esp32-s3-devkitm-1
```

### Verbose Output
```bash
pio test -e native -v
```

## Test Coverage

### Tier 1.1: Smart Sensing Logic (10 tests)
✓ Timer stays at full value when voltage detected
✓ Timer counts down when no voltage detected
✓ Timer resets when voltage reappears during countdown
✓ Amplifier turns OFF when timer reaches zero
✓ ALWAYS_ON mode keeps amplifier ON
✓ ALWAYS_OFF mode keeps amplifier OFF
✓ Voltage threshold detection
✓ Mode transitions
✓ Rapid voltage fluctuations
✓ Edge case - timer at 0 with voltage appearing

### Tier 2.1: HTTP API Endpoints (13 tests)
✓ GET /api/smartsensing returns current state
✓ POST /api/smartsensing updates mode
✓ POST /api/smartsensing updates timer duration
✓ POST /api/smartsensing validates timer duration (too low)
✓ POST /api/smartsensing validates timer duration (too high)
✓ POST /api/smartsensing updates voltage threshold
✓ POST /api/smartsensing validates voltage threshold (too low)
✓ POST /api/smartsensing validates voltage threshold (too high)
✓ POST /api/smartsensing handles invalid JSON
✓ POST /api/smartsensing handles missing body
✓ POST /api/smartsensing handles manual override
✓ POST /api/smartsensing rejects invalid mode
✓ POST /api/smartsensing handles multiple parameters

## CI/CD Integration

To run tests automatically on every commit, add this to `.github/workflows/tests.yml`:

```yaml
name: PlatformIO Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/cache@v3
        with:
          path: |
            ~/.cache/pip
            ~/.platformio/.cache
          key: ${{ runner.os }}-pio
      - uses: actions/setup-python@v4
        with:
          python-version: '3.9'
      - name: Install PlatformIO Core
        run: pip install --upgrade platformio
      - name: Run Tests
        run: pio test -e native
```

## Writing New Tests

### 1. Create a new test file
```cpp
#include <unity.h>
#include "../test_mocks/Arduino.h"

void setUp(void) {
    // Run before each test
    ArduinoMock::reset();
}

void tearDown(void) {
    // Run after each test
}

void test_my_feature(void) {
    // Arrange
    int expected = 42;

    // Act
    int actual = myFunction();

    // Assert
    TEST_ASSERT_EQUAL(expected, actual);
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_my_feature);
    return UNITY_END();
}

#ifdef NATIVE_TEST
int main(void) {
    return runUnityTests();
}
#endif
```

### 2. Run your test
```bash
pio test -e native -f test_my_feature
```

## Test Best Practices

1. **Use descriptive test names** - `test_timer_counts_down_without_voltage` is better than `test_1`
2. **Test one thing per test** - Each test should verify one specific behavior
3. **Arrange-Act-Assert pattern** - Set up, execute, verify
4. **Reset state in setUp()** - Each test should start with clean state
5. **Test edge cases** - Zero values, maximum values, invalid input
6. **Test error conditions** - Invalid JSON, out-of-range values, missing data

## Troubleshooting

### Tests won't compile
- Ensure you're using the native environment: `pio test -e native`
- Check that ArduinoJson is installed: listed in `lib_deps` in platformio.ini

### Tests fail unexpectedly
- Check that mocks are reset in setUp()
- Verify mock state isn't leaking between tests
- Use `-v` flag for verbose output

### Can't find test files
- Test files must be in subdirectories under `test/`
- Use `-f` flag to filter: `pio test -f test_smart_sensing`

## Next Steps

- Add tests for WiFi manager
- Add tests for MQTT handler
- Add tests for settings persistence
- Add hardware integration tests
- Set up GitHub Actions CI/CD
