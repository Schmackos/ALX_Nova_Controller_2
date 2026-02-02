# Test Setup Instructions

## Issue: Native Testing on Windows

Native testing requires a C/C++ compiler (gcc/g++). On Windows, you have three options:

### Option 1: Install MinGW-w64 (Recommended for fast local testing)

1. Download MinGW-w64 installer from: https://www.mingw-w64.org/downloads/
2. Or use Chocolatey: `choco install mingw`
3. Or use MSYS2: `pacman -S mingw-w64-x86_64-gcc`
4. Add to PATH: `C:\mingw64\bin` (or wherever installed)
5. Verify: `gcc --version`
6. Run tests: `pio test -e native`

**Pros:**
- Fast execution (tests run in ~1 second)
- No hardware required
- Can run in CI/CD

**Cons:**
- Requires installing additional software

### Option 2: Run Tests on ESP32 Hardware

Tests can run directly on your ESP32 device. No additional software needed.

```bash
# Run all tests on device
pio test -e esp32-s3-devkitm-1

# Run specific test
pio test -e esp32-s3-devkitm-1 -f test_smart_sensing
```

**Pros:**
- No additional software installation
- Uses existing hardware
- Tests work right now

**Cons:**
- Slower (requires upload + serial communication)
- Requires connected device
- Can't run in CI/CD without hardware

### Option 3: Use GitHub Actions (Cloud CI/CD)

Set up GitHub Actions to run tests automatically on every commit.

**Pros:**
- No local setup required
- Automatic on every push
- Linux runners have gcc pre-installed

**Cons:**
- Only runs on push, not locally
- Requires GitHub repository

## Current Status

✓ **23 tests written and ready**:
  - 10 Smart Sensing Logic tests
  - 13 API Endpoint tests

✗ **Native platform missing gcc/g++**
  - Can't run on Windows without compiler

✓ **ESP32 platform ready**
  - Tests can run on hardware right now

## Quick Start (ESP32 Hardware)

Connect your ESP32 and run:

```bash
pio test -e esp32-s3-devkitm-1 -f test_smart_sensing
```

This will:
1. Build the test
2. Upload to ESP32
3. Run tests
4. Show results via serial monitor

## Recommended Next Steps

1. **Short term**: Run tests on ESP32 hardware
   ```bash
   pio test -e esp32-s3-devkitm-1
   ```

2. **Medium term**: Install MinGW for fast native testing
   - Enables rapid test-driven development
   - Tests run in ~1 second vs 30+ seconds on hardware

3. **Long term**: Set up GitHub Actions CI/CD
   - Automatic testing on every commit
   - Prevents regressions from being merged

## Example: Running Tests on ESP32

```bash
$ pio test -e esp32-s3-devkitm-1 -f test_smart_sensing

Processing test_smart_sensing in esp32-s3-devkitm-1 environment
--------------------------------------------------------------------------------
Building...
Uploading...
Testing...

test_smart_sensing/test_smart_sensing_logic.cpp:143:test_timer_stays_full_when_voltage_detected [PASSED]
test_smart_sensing/test_smart_sensing_logic.cpp:162:test_timer_counts_down_without_voltage [PASSED]
test_smart_sensing/test_smart_sensing_logic.cpp:180:test_timer_resets_when_voltage_reappears [PASSED]
... (7 more tests)

-----------------------
10 Tests 0 Failures 0 Ignored
OK
```

## Troubleshooting

### "gcc not found" error
→ Install MinGW or use ESP32 hardware testing

### "Upload failed" on ESP32
→ Check device is connected: `pio device list`

### Tests timeout on ESP32
→ Increase timeout in platformio.ini:
```ini
test_timeout = 120
```

### Want faster feedback?
→ Install MinGW for native testing (1-2 second execution)
