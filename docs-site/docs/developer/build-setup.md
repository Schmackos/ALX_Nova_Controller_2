---
title: Build Setup
sidebar_position: 3
description: Setting up the development environment and building the ALX Nova firmware.
---

# Build Setup

This page covers everything needed to build, flash, and test ALX Nova firmware from source. The project uses [PlatformIO](https://platformio.org/) as the build system and targets the Waveshare ESP32-P4-WiFi6-DEV-Kit.

---

## Prerequisites

### Required Tools

| Tool | Version | Purpose |
|---|---|---|
| Python | 3.8+ | PlatformIO dependency |
| PlatformIO Core | latest | Build system and package manager |
| Node.js | 18+ | Web asset build scripts and E2E tests |
| Git | any | Version control |

### Installing PlatformIO

PlatformIO is available as a VS Code extension (recommended) or as a standalone CLI:

```bash
# Standalone CLI via pip
pip install platformio

# Verify installation
pio --version
```

For VS Code, install the **PlatformIO IDE** extension. It installs PlatformIO Core automatically.

### Activating Pre-commit Hooks

The project ships with pre-commit hooks that run fast JS quality checks before every commit. Activate them once per clone:

```bash
git config core.hooksPath .githooks
```

The hook runs `node tools/find_dups.js`, `node tools/check_missing_fns.js`, and ESLint on `web_src/js/`. The commit is blocked if any check fails.

---

## Build Environments

`platformio.ini` defines three environments:

| Environment | Target | Purpose |
|---|---|---|
| `esp32-p4` | Waveshare ESP32-P4-WiFi6-DEV-Kit | Production firmware build |
| `native` | Host machine (gcc/MinGW) | Unit tests, no hardware needed |
| `p4_hosted_update` | ESP32-P4 | Minimal hosted OTA update test harness |

The default environment is `esp32-p4`.

---

## Building the Firmware

### Full Build

```bash
# Build firmware for ESP32-P4 (uses default env)
pio run
```

First build downloads the pioarduino ESP32 platform and all library dependencies. This takes several minutes. Subsequent builds are incremental.

### Build for a Specific Environment

```bash
pio run -e esp32-p4
pio run -e native
```

### Verbose Build Output

```bash
pio run -v
```

Build output (`.log` files) should be saved to the `logs/` directory — keep the project root clean.

---

## Uploading Firmware

The upload port is **COM8** (configured in `platformio.ini`). Connect the board via USB before running:

```bash
# Build and upload
pio run --target upload

# Upload only (skip rebuild if already built)
pio run --target upload -e esp32-p4
```

:::tip Changing the upload port
If your board appears on a different COM port, override it temporarily:
```bash
pio run --target upload --upload-port COM5
```
Or edit `upload_port` in `platformio.ini` permanently.
:::

### Filesystem Upload

The web UI assets are embedded as gzip bytes in `src/web_pages_gz.cpp` (auto-generated). The LittleFS filesystem partition holds config files (`/config.json`, `/hal_config.json`, DSP presets). To upload the filesystem image:

```bash
pio run --target uploadfs
```

:::warning Always regenerate web assets before building
If you edit any file in `web_src/`, you must run the asset builder before `pio run` — otherwise the old compiled bytes are flashed:

```bash
node tools/build_web_assets.js
pio run --target upload
```
:::

---

## Serial Monitoring

```bash
# Monitor serial output at 115200 baud
pio device monitor

# Monitor on a specific port
pio device monitor --port COM5
```

The monitor runs at **115200 baud**. Log output uses the `[ModuleName]` prefix convention documented in the Architecture page. The WebSocket console in the web UI also mirrors serial output at runtime.

---

## Key Build Flags

All build flags for the `esp32-p4` environment are defined in `platformio.ini`. The following table covers the most important ones:

### Feature Flags

| Flag | Default | Description |
|---|---|---|
| `-D DAC_ENABLED` | ON | Enables the HAL device framework, all audio device drivers, pipeline bridge, HAL REST API |
| `-D GUI_ENABLED` | ON | Enables LVGL GUI, TFT display (ST7735S), and rotary encoder input |
| `-D USB_AUDIO_ENABLED` | ON | Enables TinyUSB UAC2 speaker device (`alx,usb-audio` HAL driver) |
| `-D DSP_ENABLED` | ON | Enables the DSP pipeline, output DSP, and DSP REST API |

### Pin Assignment Flags

All GPIO pin assignments are defined as build flags, allowing board variants without source edits:

```ini
; Audio I2S (PCM1808 ADCs)
-D I2S_BCK_PIN=20
-D I2S_DOUT_PIN=23
-D I2S_DOUT2_PIN=25
-D I2S_LRC_PIN=21
-D I2S_MCLK_PIN=22

; TFT Display (ST7735S)
-D TFT_MOSI_PIN=2
-D TFT_SCLK_PIN=3
-D TFT_CS_PIN=4
-D TFT_DC_PIN=5
-D TFT_RST_PIN=6
-D TFT_BL_PIN=26

; Rotary Encoder
-D ENCODER_A_PIN=32
-D ENCODER_B_PIN=33
-D ENCODER_SW_PIN=36

; Peripheral
-D AMPLIFIER_PIN=27
-D BUZZER_PIN=45
-D RESET_BUTTON_PIN=46
-D SIGGEN_PWM_PIN=47
-D LED_PIN=1
```

Fallback defaults for all pins are also defined in `src/config.h` and are used if a build flag is absent.

### USB Audio Flags

USB Audio requires disabling the default Arduino USB CDC mode on the ESP32-P4:

```ini
build_unflags = -DARDUINO_USB_MODE -DARDUINO_USB_CDC_ON_BOOT
build_flags =
    -D ARDUINO_USB_MODE=0
    -D ARDUINO_USB_CDC_ON_BOOT=0
    -D USB_AUDIO_ENABLED
```

These flags are already set correctly in `platformio.ini`. Do not remove them if USB Audio is enabled.

---

## Native Test Environment

Unit tests run on your host machine (Windows/Linux/macOS) using the `native` PlatformIO environment. No hardware or ESP32 toolchain is required.

### Running Tests

```bash
# Run all 1,614 unit tests across 70 modules
pio test -e native

# Run a specific test module
pio test -e native -f test_wifi
pio test -e native -f test_mqtt
pio test -e native -f test_auth
pio test -e native -f test_audio_pipeline
pio test -e native -f test_hal_core

# Verbose output (shows individual test pass/fail)
pio test -e native -v
```

### Test Infrastructure

Tests compile with `-D UNIT_TEST -D NATIVE_TEST` flags. These guards allow source files to exclude hardware-specific code paths:

```cpp
#ifndef NATIVE_TEST
    #include <driver/i2s_std.h>  // excluded from native builds
#endif
```

Mock headers in `test/test_mocks/` simulate the Arduino environment:

| Mock | Simulates |
|---|---|
| `Arduino.h` | `millis()`, `analogRead()`, `pinMode()`, `digitalWrite()` |
| `WiFi.h` | `WiFiClass` scan and connect APIs |
| `PubSubClient.h` | MQTT client connect / publish / subscribe |
| `Preferences.h` | NVS key-value storage |
| `LittleFS.h` | File open / read / write / remove |
| `Wire.h` | I2C read / write |
| `i2s_std_mock.h` | IDF5 I2S channel APIs |

`test_build_src = no` in `platformio.ini` means the test environment does **not** compile the entire `src/` tree. Each test file includes only the specific headers it needs. This keeps test compile times fast and avoids pulling in hardware driver code.

### Test Module Structure

Each test module lives in its own directory under `test/`:

```
test/
├── test_mocks/             # Shared mock headers (not a test module)
├── test_wifi/
│   └── test_wifi_manager.cpp
├── test_mqtt/
│   └── test_mqtt_handler.cpp
├── test_audio_pipeline/
│   └── test_audio_pipeline.cpp
└── test_hal_core/
    └── test_hal_core.cpp
```

Each test file follows the Arrange-Act-Assert (AAA) pattern with a `setUp()` function that resets all state before each test:

```cpp
#include <unity.h>
#include "wifi_manager.h"

void setUp() {
    wifiManagerReset();  // reset to clean state
}

void tearDown() {}

void test_wifi_connects_to_first_ssid() {
    // Arrange
    wifiManagerSetCredentials("TestSSID", "password");

    // Act
    bool result = wifiManagerConnect();

    // Assert
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("TestSSID", wifiManagerGetSSID());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_wifi_connects_to_first_ssid);
    return UNITY_END();
}
```

---

## E2E Browser Tests

Playwright browser tests verify the web UI against a mock Express server. No ESP32 hardware or firmware is needed.

```bash
cd e2e

# First-time setup
npm install
npx playwright install --with-deps chromium

# Run all 26 browser tests across 19 specs
npx playwright test

# Run a single spec
npx playwright test tests/auth.spec.js

# Run with visible browser (for debugging)
npx playwright test --headed

# Open Playwright inspector (step-by-step debug)
npx playwright test --debug
```

The mock server runs on port 3000 and assembles HTML directly from `web_src/` — the same assembly logic as `tools/build_web_assets.js`. This means E2E tests always test the current source, not a stale compiled asset.

---

## Building Web Assets

The web UI source lives in `web_src/`. After any edit to those files, regenerate the C++ asset files before building firmware:

```bash
node tools/build_web_assets.js
```

This produces two auto-generated files — **do not edit them manually**:
- `src/web_pages.cpp` — uncompressed HTML/CSS/JS as C string
- `src/web_pages_gz.cpp` — gzip-compressed byte array served to browsers

### Web Asset Structure

```
web_src/
├── index.html              # HTML shell (no inline CSS or JS)
├── css/
│   ├── 01-variables.css    # CSS custom properties / design tokens
│   ├── 02-layout.css       # Page structure
│   ├── 03-components.css   # UI components
│   ├── 04-canvas.css       # Audio visualisation canvas styles
│   └── 05-responsive.css   # Breakpoints
└── js/
    ├── 01-core.js          # WebSocket connection and reconnect
    ├── 02-ws-router.js     # WS message dispatch
    ├── 04-shared-audio.js  # Dynamic lane arrays, resizeAudioArrays()
    ├── 05-audio-tab.js     # Audio tab: inputs, matrix, outputs, SigGen
    ├── 06-peq-overlay.js   # PEQ / crossover / compressor overlays
    ├── 15-hal-devices.js   # HAL device management UI
    ├── 27a-health-dashboard.js  # Diagnostics health dashboard
    └── ...                 # Other feature modules (01–28 load order)
```

All JS files are concatenated in filename order into a single `<script>` block — they share a single scope. Do not declare the same `let`/`const` name in two files. Run `node tools/find_dups.js` to check before committing.

---

## Static Analysis Tools

```bash
# Check for duplicate JS declarations across web_src/js/
node tools/find_dups.js

# Check for undefined JS function references
node tools/check_missing_fns.js

# ESLint on all web_src/js/ files
cd e2e
npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json
```

cppcheck runs in CI on `src/` (excluding `src/gui/`) and is not required locally, but failures block the firmware build.

---

## Partition Table

The ESP32-P4 is flashed with a custom OTA partition table (`partitions_ota.csv`):

| Partition | Type | Size | Purpose |
|---|---|---|---|
| `app0` (ota_0) | app | 4 MB | Primary firmware slot |
| `app1` (ota_1) | app | 4 MB | OTA update target slot |
| `spiffs` (LittleFS) | data | varies | Config files, DSP presets, HAL config |
| `nvs` | data | 16 KB | WiFi credentials, selected NVS settings (survives LittleFS format) |

Total flash: 16 MB. PSRAM: available (used for DSP delay lines, diagnostics ring buffer, USB audio ring buffer).
