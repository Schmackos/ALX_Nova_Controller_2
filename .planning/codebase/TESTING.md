# Testing Patterns

**Analysis Date:** 2026-02-15

## Test Framework

**Runner:**
- PlatformIO Unity framework (Unity 2.5 via PlatformIO)
- Native environment: `pio test -e native` compiles tests with gcc/MinGW
- Build flags: `-D UNIT_TEST -D NATIVE_TEST` enable conditional compilation in test code
- Total tests: 754 native unit tests (all passing)
- ESP32 environment: `test_ignore = *` — all tests run on native, not on device

**Assertion Library:**
- Unity assertions: `TEST_ASSERT_*` macros
- Common assertions:
  - `TEST_ASSERT_TRUE(condition)`
  - `TEST_ASSERT_FALSE(condition)`
  - `TEST_ASSERT_EQUAL(expected, actual)`
  - `TEST_ASSERT_EQUAL_STRING(expected, actual)`
  - `TEST_ASSERT_FLOAT_WITHIN(tolerance, expected, actual)`
  - `TEST_ASSERT_EQUAL_HEX8(expected, actual)` for bytewise comparisons

**Run Commands:**
```bash
pio test -e native                    # Run all tests
pio test -e native -f test_wifi       # Run specific test module
pio test -e native -f test_mqtt       # Run MQTT tests
pio test -e native -f test_auth       # Run auth/security tests
pio test -e native -v                 # Verbose test output
```

## Test File Organization

**Location:**
- `test/test_<module>/test_<module>.cpp` — one test module per directory
- Mocks in `test/test_mocks/` — shared across all tests
- `test_build_src = no` in `platformio.ini`: Tests don't compile `src/` directly; they include specific headers and mock implementations

**Naming:**
- Directory: `test_<feature>` (e.g., `test_wifi`, `test_auth`, `test_dsp`)
- File: `test_<feature>.cpp` (single file per test module)
- Functions: `void test_<scenario>(void)` (e.g., `test_version_comparison_equal`, `test_session_creation_empty_slot`)

**Structure:**
```
test/
├── test_mocks/              # Shared mock implementations
│   ├── Arduino.h/.cpp       # Mock millis(), GPIO, analogRead()
│   ├── Preferences.h/.cpp   # Mock NVS/preferences
│   ├── WiFi.h/.cpp          # Mock WiFi library
│   ├── PubSubClient.h       # Mock MQTT client
│   ├── esp_random.h         # Mock random number generation
│   ├── esp_task_wdt.h       # Mock watchdog
│   └── mbedtls/md.h         # Mock SHA256
├── test_utils/              # Utility functions (version compare, RSSI, etc.)
├── test_auth/               # Auth handler tests (session, password, security)
├── test_wifi/               # WiFi manager tests (connection, AP mode)
├── test_mqtt/               # MQTT handler tests
├── test_settings/           # Settings persistence
├── test_ota/                # OTA updater tests
├── test_ota_task/           # Non-blocking OTA task tests
├── test_button/             # Button press detection (short/long/multi-click)
├── test_websocket/          # WebSocket message handlers
├── test_i2s_audio/          # I2S audio ADC driver
├── test_fft/                # FFT spectrum analysis (256-pt, 16-band)
├── test_dsp/                # DSP pipeline (144 tests)
├── test_dsp_rew/            # REW file import/export (22 tests)
├── test_esp_dsp/            # ESP-DSP library features (23 tests)
├── test_gui_home/           # GUI home screen
├── test_gui_navigation/      # GUI screen navigation
├── test_gui_input/           # Rotary encoder input (Gray code)
├── test_smart_sensing/      # Audio detection, auto-off timer
├── test_signal_generator/   # Test signal generator (sine, square, noise, sweep)
├── test_buzzer/             # Buzzer patterns and ISR safety
├── test_audio_diagnostics/  # Per-ADC health status detection
├── test_vrms/               # Input voltage (Vrms) calculation
├── test_api/                # REST API endpoints
├── test_crash_log/          # Crash log ring buffer (24 tests)
├── test_task_monitor/       # FreeRTOS task enumeration (17 tests)
├── test_debug_mode/         # Debug serial level filtering (24 tests)
├── test_usb_audio/          # USB UAC2 speaker device (26 tests)
└── test_mocks/              # Shared mock implementations
```

Test modules with most tests:
- `test_dsp/` — 144 tests (biquad coefficients, filtering, routing matrix, gain, delay, convolution, crossover)
- `test_i2s_audio/` — 24 tests (sampling, dBFS, VU metering, peak hold, waveform, FFT)
- `test_auth/` — 27+ tests (session creation, password hashing, timing-safe compare, NVS migration)
- `test_crash_log/` — 24 tests (ring buffer, persistence, recovery)
- `test_debug_mode/` — 24 tests (log level filtering, state consistency)

## Test Structure

**Suite Organization:**
```cpp
// Standard test structure (example from test_utils.cpp)
#include <unity.h>
#include "../test_mocks/Arduino.h"

// Namespace for test-local state
namespace TestUtilsState {
  void reset() {
    // Reset all mock state
    mockResetReason = ESP_RST_POWERON;
#ifdef NATIVE_TEST
    ArduinoMock::reset();
#endif
  }
}

// Implementation source (sometimes pulled in for native testing)
// #include "../../src/utils.cpp"  // OR inline implementations

// Test setup/teardown
void setUp(void) {
  TestUtilsState::reset();  // Run before each test
}

void tearDown(void) {
  // Clean up after each test (usually empty)
}

// Test functions
void test_version_comparison_equal(void) {
  TEST_ASSERT_EQUAL(0, compareVersions("1.0.0", "1.0.0"));
}

void test_version_comparison_less(void) {
  TEST_ASSERT_EQUAL(-1, compareVersions("1.0.0", "1.0.1"));
}
```

**Patterns:**
- **Setup**: Each test begins with `setUp()` that resets all module state via a namespace function
- **Teardown**: Usually empty or used for cleanup of allocated resources
- **Arrange-Act-Assert**: Tests follow clear pattern:
  1. Arrange: Set up test state (e.g., mock WiFi connection)
  2. Act: Call function under test
  3. Assert: Verify expected outcome

**Example Pattern:**
```cpp
void test_dsp_biquad_lpf_gain(void) {
    // Arrange: Initialize biquad with known parameters
    DspBiquadParams p;
    dsp_init_biquad_params(p);
    p.frequency = 1000.0f;
    p.Q = 0.707f;
    dsp_compute_biquad_coeffs(p, DSP_BIQUAD_LPF, 48000);

    // Act: Compute DC gain from coefficients
    float dcGain = (p.coeffs[0] + p.coeffs[1] + p.coeffs[2]) /
                   (1.0f + p.coeffs[3] + p.coeffs[4]);

    // Assert: Verify LPF passes DC (gain = 1.0)
    TEST_ASSERT_FLOAT_WITHIN(COEFF_TOL, 1.0f, dcGain);
    TEST_ASSERT_TRUE(p.coeffs[0] > 0.0f);
}
```

## Mocking

**Framework:** Custom mocks in `test/test_mocks/` directory

**Mock Implementations:**
- `Arduino.h/.cpp` — Mock `millis()`, `micros()`, `delay()`, `analogRead()`, `digitalWrite()`
  - `ArduinoMock::mockMillis` allows advancing time in tests
  - GPIO state tracked per pin
- `Preferences.h/.cpp` — NVS/EEPROM mock using in-memory map
  - Namespace/key-value storage matching hardware API
  - `Preferences::reset()` clears all entries between tests
- `WiFi.h/.cpp` — WiFi library stubs (SSID, password, status)
- `PubSubClient.h` — MQTT client mock
- `esp_random.h` — Deterministic random generation for reproducible tests
- `mbedtls/md.h` — SHA256 hashing mock
- `esp_task_wdt.h` — Watchdog mock (no-op in tests)

**Mock State Reset Pattern:**
```cpp
// Example from test_auth.cpp
namespace TestAuthState {
  void reset() {
    // Clear all sessions
    for (int i = 0; i < MAX_SESSIONS; i++) {
      activeSessions[i].sessionId = "";
    }
    mockWebPassword = "default_password";
    Preferences::reset();           // Clear NVS mock
    EspRandomMock::reset();         // Reset random state
    ArduinoMock::reset();           // Reset time/GPIO
  }
}

void setUp(void) { TestAuthState::reset(); }
```

**What to Mock:**
- All hardware: GPIO, ADC, timers, I2S, USB, NVS
- Network libraries: WiFi, MQTT (stubs, no actual connections)
- Random number generation: Make deterministic for reproducible tests
- Time: Advance manually via `ArduinoMock::mockMillis += ms`

**What NOT to Mock:**
- STL containers: Use real `String`, `vector`, etc.
- Cryptographic algorithms: For auth tests, include real SHA256 via `#include <mbedtls/md.h>`
- DSP math: Include real DSP source files (e.g., `#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"`)
- JSON parsing: Include real `ArduinoJson.h` (native tests support it)

## Fixtures and Factories

**Test Data:**
- Inline in test files using simple C++ structs and helper functions
- Example from `test_dsp.cpp`:
  ```cpp
  void test_lpf_coefficients(void) {
      DspBiquadParams p;
      dsp_init_biquad_params(p);        // Factory function
      p.frequency = 1000.0f;
      p.Q = 0.707f;
      // ... test continues
  }
  ```

- Per-ADC audio fixture pattern:
  ```cpp
  // Initialize dual ADC test state
  void test_dual_adc_diagnostics(void) {
      appState.audioAdc[0].healthStatus = AUDIO_OK;
      appState.audioAdc[1].healthStatus = AUDIO_NO_DATA;
      appState.numAdcsDetected = 2;
      // ... test continues
  }
  ```

**Location:**
- Test data defined within test functions (no separate fixture files)
- Reset happens in `setUp()` namespace reset function
- Large reference data (e.g., FFT windows, DSP coefficients) computed inline or pulled from source

## Coverage

**Requirements:** Not enforced (no coverage gates in CI)

**View Coverage:** Not detected in codebase (no gcov integration)

**High-Coverage Modules:**
- Auth handler: 27+ tests covering all session paths, password hashing, NVS migration
- DSP pipeline: 144 tests covering all 31 stage types, routing matrix, coefficient generation
- Crash log: 24 tests covering ring buffer, persistence, recovery
- Debug mode: 24 tests covering state consistency and log level filtering

**Low-Coverage Areas:**
- GUI rendering: 3 test modules (home, input, navigation) but limited rendering tests
- Real-time audio task: No tests (real-time constraints + DMA make this hardware-dependent)
- WebSocket binary frames: Tested via WebSocket handler tests but frame parsing limited
- MQTT discovery: Basic tests, full Home Assistant discovery not deeply tested

## Test Types

**Unit Tests:**
- Scope: Single function or module in isolation
- Approach: Mock all dependencies, test inputs/outputs
- Examples: `test_version_comparison_*`, `test_session_creation_*`, `test_lpf_coefficients`
- Framework: Unity assertions

**Integration Tests:**
- Scope: Multiple modules working together (still via mocks)
- Approach: Call module A which calls module B, verify state changes
- Examples: `test_dsp_stage_signal_flow` (DSP → audio → conversion), `test_websocket_audio_broadcast`
- Framework: Unity assertions + mock state inspection

**E2E Tests:**
- Not used in this codebase
- Hardware tests require actual device (CH343 USB-to-UART, ESP32-S3 board)
- Wokwi simulation available via `wokwi.toml` but not in CI

## Common Patterns

**Async Testing:**
- Time advancement: `ArduinoMock::mockMillis += 5000` to fast-forward
- State polling: Call function, check state flag, repeat
- Example from OTA task tests:
  ```cpp
  void test_ota_task_completion(void) {
      startOTACheckTask();
      // Advance time to allow task to complete
      ArduinoMock::mockMillis += 10000;
      TEST_ASSERT_TRUE(otaCheckTaskRunning);

      // Simulate task completion
      otaCheckTaskRunning = false;
      // Verify state updated
      TEST_ASSERT_EQUAL_STRING("idle", otaStatus.c_str());
  }
  ```

**Error Testing:**
- Boolean returns: `TEST_ASSERT_FALSE(createSession(...))` when out of space
- State transitions: Verify error state set correctly (e.g., `wifiConnectError` populated)
- Example from auth tests:
  ```cpp
  void test_session_creation_full_eviction(void) {
      // Fill all 5 slots
      for (int i = 0; i < 5; i++) {
          createSession(sessionIds[i]);
      }
      // Verify 6th creation evicts oldest
      String newId;
      bool created = createSession(newId);
      TEST_ASSERT_TRUE(created);
      TEST_ASSERT_NOT_EQUAL_STRING(sessionIds[0].c_str(), newId.c_str());
  }
  ```

**Float Tolerance Testing:**
- Use `TEST_ASSERT_FLOAT_WITHIN(tolerance, expected, actual)`
- DSP tests typically use `FLOAT_TOL = 0.001f` for precision math
- Audio level tests use larger tolerance (0.1f dB range) for dynamic measurements
- Example:
  ```cpp
  void test_audio_rms_calculation(void) {
      float rms = audio_calculate_rms(testBuffer, 256);
      TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.707f, rms);  // ±0.01 tolerance
  }
  ```

**Dirty Flag Testing:**
- Set flag, call function, verify flag cleared
- Example from AppState tests:
  ```cpp
  void test_fsm_state_dirty_flag(void) {
      appState.setFSMState(STATE_SIGNAL_DETECTED);
      TEST_ASSERT_TRUE(appState.isFSMStateDirty());
      appState.clearFSMStateDirty();
      TEST_ASSERT_FALSE(appState.isFSMStateDirty());
  }
  ```

**Multi-Step Sequences:**
- Tests that verify state machine transitions
- Example from smart sensing tests:
  ```cpp
  void test_signal_detection_timer_sequence(void) {
      // Step 1: Detect signal
      appState.currentMode = AUTO_OFF;
      TEST_ASSERT_TRUE(detectSignal());
      TEST_ASSERT_TRUE(appState.amplifierState);

      // Step 2: Advance time past timer
      ArduinoMock::mockMillis += DEFAULT_TIMER_DURATION + 1000;
      updateSmartSensingLogic();

      // Step 3: Verify auto-off
      TEST_ASSERT_FALSE(appState.amplifierState);
  }
  ```

## Special Testing Considerations

**UNIT_TEST Flag:**
- `#ifdef UNIT_TEST` guards code that's only compiled for native tests
- Allows conditional implementation paths (e.g., mock vs real for some functions)
- Example: `#ifdef UNIT_TEST` in some modules enables direct state access

**NATIVE_TEST Flag:**
- `#ifdef NATIVE_TEST` guards native-specific mock includes
- Allows same header to include real or mock depending on target
- Example: `#ifdef NATIVE_TEST #include "../test_mocks/Arduino.h" #else #include <Arduino.h> #endif`

**Build Configuration for Tests:**
- Compile flags in native env: `-D UNIT_TEST -D NATIVE_TEST -D DSP_ENABLED`
- Exclusions: `lib_compat_mode = off` (needed for arduinoFFT), `lib_ignore = esp_dsp_lite` (uses native ANSI C fallback for DSP)
- Test discovery: PlatformIO scans `test/test_*/test_*.cpp` for `void test_*()` functions

---

*Testing analysis: 2026-02-15*
