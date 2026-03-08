# Technology Stack

**Analysis Date:** 2026-03-08

## Languages

**Primary:**
- C++11 — Firmware source (`src/`), HAL drivers (`src/hal/`), all FreeRTOS tasks
- C — DSP biquad coefficient generation (`src/dsp_biquad_gen.c`), safe SNR/SFDR (`src/safe_snr_sfdr.c`)

**Secondary:**
- JavaScript (ES5/ES6) — Web UI (`web_src/js/`), 23 concatenated modules served as single `<script>` block
- CSS — Web UI styles (`web_src/css/`), 6 files concatenated in load order
- HTML — Web UI shell template (`web_src/index.html`)
- Python — Build tooling: RISC-V toolchain fix (`tools/fix_riscv_toolchain.py`), PlatformIO CI scripts
- Node.js — Web asset build pipeline (`tools/build_web_assets.js`), JS static analysis tools (`tools/find_dups.js`, `tools/check_missing_fns.js`)

## Runtime

**Environment:**
- ESP32-P4 (RISC-V dual-core, 400 MHz), Waveshare ESP32-P4-WiFi6-DEV-Kit
- Arduino framework on top of ESP-IDF 5.x (`idf: '>=5.1'`)
- FreeRTOS (dual-core task scheduling, binary semaphores, event groups)
- PSRAM available — used for audio float buffers, DSP delay lines, ring buffers

**Package Manager:**
- PlatformIO — firmware dependencies and build system
- npm — E2E test dependencies (`e2e/package-lock.json`)
- Lockfile: `e2e/package-lock.json` present; PlatformIO uses `.pio/` cache

## Frameworks

**Core Firmware:**
- Arduino (pioarduino/platform-espressif32 v55.03.37) — HAL abstraction, WiFi, I2S, GPIO, LEDC
- ESP-IDF 5.x — I2S std API (`<driver/i2s_std.h>`), MCPWM (`<driver/mcpwm_prelude.h>`), temperature sensor (`<driver/temperature_sensor.h>`), FreeRTOS debug (`<esp_private/freertos_debug.h>`)

**GUI:**
- LVGL v9.4 — Widget toolkit for ST7735S TFT display; task runs on Core 0. Guarded by `-D GUI_ENABLED`
- LovyanGFX v1.2.0 — TFT display driver (synchronous SPI on P4, no DMA)

**DSP:**
- ESP-DSP (pre-built `libespressif__esp-dsp.a`) — RISC-V-optimized biquad IIR, FIR, Radix-4 FFT, window functions, SNR/SFDR. Included via `-I` flags; `lib/esp_dsp_lite/` used for native tests only
- esp_dsp_lite v1.0.0 (`lib/esp_dsp_lite/`) — ANSI C DSP fallback for `native` test environment; `lib_ignore`d on ESP32 targets

**Testing:**
- Unity — C++ unit test framework; 1,556 tests across 65 modules in `test/`
- Playwright v1.50.0 — E2E browser test framework; 26 tests across 19 specs in `e2e/tests/`
- Express v4.21.0 — Mock HTTP server for E2E tests (`e2e/mock-server/server.js`)

**Build/Dev:**
- PlatformIO — Multi-environment build system (`platformio.ini`)
- Node.js — Web asset assembly, gzip compression, JS linting tooling
- ESLint v8.57.0 — JS static analysis (`web_src/.eslintrc.json`)
- cppcheck — C++ static analysis (CI only, `src/` excluding `src/gui/`)
- GitHub Actions — CI/CD: 4 parallel quality gates gate firmware build (`.github/workflows/tests.yml`)

## Key Dependencies

**Critical (firmware):**
- `ArduinoJson@^7.4.2` — JSON parsing throughout: settings, MQTT, REST API, WebSocket messages
- `WebSockets@2.7.3` (vendored, `lib/WebSockets/`) — WebSocket server on port 81; vendored with `#ifndef` guard patch for `WEBSOCKETS_MAX_DATA_SIZE=4096`
- `PubSubClient@^2.8` — MQTT client for Home Assistant integration
- `LovyanGFX@^1.2.0` — TFT display driver
- `lvgl@^9.4` — GUI framework

**Testing / Development:**
- `arduinoFFT@^2.0` — FFT in `native` test env only (`src/i2s_audio.cpp` conditionally includes `<arduinoFFT.h>`)
- `@playwright/test@^1.50.0` — E2E tests
- `eslint@^8.57.0` — JS linting
- `cookie-parser@^1.4.7` — Mock server session handling

**IDF / Arduino built-ins (no separate install needed):**
- `<WiFi.h>`, `<WiFiClientSecure.h>` — WiFi client and TLS
- `<WebServer.h>` — HTTP server on port 80
- `<HTTPClient.h>` — Outbound HTTP (OTA checks, NTP)
- `<PubSubClient.h>` — MQTT via `knolleary/PubSubClient`
- `<LittleFS.h>` — Filesystem for settings, DSP presets, HAL config, diagnostic journal
- `<Preferences.h>` (NVS) — WiFi credentials, session secrets, device serial number, OTA flags
- `<Update.h>` — OTA firmware flashing
- `<mbedtls/md.h>`, `<mbedtls/pkcs5.h>` — PBKDF2-SHA256 password hashing, SHA256 firmware verification
- `<DNSServer.h>` — Captive portal DNS in AP mode
- `<Wire.h>` — I2C for ES8311, EEPROM, HAL discovery
- TinyUSB (built into Arduino ESP32 3.x) — UAC2 USB audio speaker device; `esp32-hal-tinyusb.h`, `tusb.h`
- `<freertos/FreeRTOS.h>`, `<freertos/task.h>`, `<freertos/semphr.h>` — Task management, binary semaphores, event groups
- `<esp_task_wdt.h>` — Task watchdog (30s timeout, reconfigured at setup)
- `<esp_heap_caps.h>` — PSRAM allocation (`MALLOC_CAP_SPIRAM`)

## Configuration

**Build Flags (defined in `platformio.ini`):**
- `-D DSP_ENABLED` — Enables DSP pipeline
- `-D DAC_ENABLED` — Enables HAL device framework and DAC drivers
- `-D GUI_ENABLED` — Enables LVGL GUI task
- `-D USB_AUDIO_ENABLED` — Enables TinyUSB UAC2 speaker device
- `-D ARDUINO_USB_MODE=0`, `-D ARDUINO_USB_CDC_ON_BOOT=0` — Required for TinyUSB OTG mode
- `-D WEBSOCKETS_MAX_DATA_SIZE=4096` — Overrides vendored WebSockets default buffer size
- `-D LV_CONF_INCLUDE_SIMPLE` — LVGL config via simple include path
- `-Os` — Optimize for size

**Environments:**
- `esp32-p4` — Production firmware (default)
- `p4_hosted_update` — OTA update test environment (debug build, minimal source filter)
- `native` — Host-machine unit tests (gcc/MinGW), no hardware required

**Partition Table (`partitions_ota.csv`):**
- `nvs`: 0x9000, 20KB — NVS key-value store
- `otadata`: 0xE000, 8KB — OTA boot selection
- `app0` (ota_0): 0x10000, 4MB — Active firmware slot
- `app1` (ota_1): 0x410000, 4MB — OTA update slot
- `spiffs` (LittleFS): 0x810000, ~8MB — Config files, DSP presets, HAL config, diagnostic journal
- `coredump`: 0xFF0000, 64KB — Core dump storage

**Key Configuration Files:**
- `platformio.ini` — Build environments, flags, library deps, upload port
- `src/config.h` — All firmware constants: version (`FIRMWARE_VERSION "1.12.1"`), GitHub repo, pins, timeouts, task stack/priority sizes
- `src/idf_component.yml` — IDF version constraint (`>=5.1`)
- `web_src/.eslintrc.json` — ESLint rules and 380 globals for concatenated JS scope

## Platform Requirements

**Development:**
- PlatformIO Core (Python 3.x)
- RISC-V ESP32 toolchain (`toolchain-riscv32-esp`) — auto-downloaded by `tools/fix_riscv_toolchain.py` on Windows/MSYS2
- Node.js (v20 recommended for CI; any LTS for local)
- Upload/monitor port: COM8 at 115,200 baud

**Production:**
- Target: Waveshare ESP32-P4-WiFi6-DEV-Kit
- Flash: 16MB (board_upload.flash_size = 16MB)
- PSRAM: present, used for audio buffers and DSP delay lines
- WiFi: via ESP32-C6 co-processor (SDIO, firmware v2.11.6)
- Deployment: OTA via GitHub Releases API or direct serial upload

---

*Stack analysis: 2026-03-08*
