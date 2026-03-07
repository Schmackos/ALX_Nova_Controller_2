# Technology Stack

**Analysis Date:** 2026-03-07

## Languages

**Primary:**
- C++11 — All firmware source in `src/` (Arduino framework + IDF5 APIs)
- C — DSP coefficient computation in `src/dsp_biquad_gen.c`, `src/safe_snr_sfdr.c`

**Secondary:**
- JavaScript (ES2015+) — Web UI in `web_src/js/` (23 modules, concatenated single-scope bundle)
- CSS — Web UI styling in `web_src/css/` (5 split files by concern)
- Python — PlatformIO build scripts in `tools/fix_riscv_toolchain.py`, `tools/patch_websockets.py`
- Node.js — Web asset build tooling in `tools/build_web_assets.js`, static analysis in `tools/find_dups.js`, `tools/check_missing_fns.js`

## Runtime

**Firmware:**
- ESP-IDF 5.x (IDF5), accessed via Arduino-ESP32 3.x wrapper
- Platform package: `https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip`
- Target: Waveshare ESP32-P4-WiFi6-DEV-Kit (`esp32-p4` PlatformIO board)
- RTOS: FreeRTOS (built into ESP-IDF), dual-core — Core 0 for GUI/MQTT/USB, Core 1 for audio pipeline exclusively

**Test/Tooling:**
- Node.js 20 (CI-enforced via `actions/setup-node@v4`)
- Python 3.11 (CI-enforced via `actions/setup-python@v5`)
- GCC/MinGW (native platform for C++ unit tests)

## Package Manager

**Firmware:**
- PlatformIO — library management via `platformio.ini` `lib_deps`
- No lockfile (PlatformIO resolves semver ranges at build time)

**Web/Testing:**
- npm — `e2e/package.json` / `e2e/package-lock.json` (lockfile present, `npm ci` used in CI)

## Frameworks

**Core Firmware:**
- Arduino (framework = arduino) — HAL abstraction over ESP-IDF
- ESP-IDF 5.x — used directly for IDF5 APIs: `<driver/i2s_std.h>`, `<driver/temperature_sensor.h>`, `<driver/mcpwm_prelude.h>`, `<driver/gpio.h>`, `<esp_task_wdt.h>`, `<esp_heap_caps.h>`, `<freertos/FreeRTOS.h>`

**GUI:**
- LVGL 9.4 (`lvgl/lvgl@^9.4`) — embedded GUI framework, guarded by `-D GUI_ENABLED`
- LovyanGFX 1.2.0 (`lovyan03/LovyanGFX@^1.2.0`) — TFT driver for ST7735S 128x160, synchronous SPI on P4 (no DMA)

**Networking / Protocol:**
- WebSockets 2.7.2 (`links2004/WebSockets@^2.7.2`) — WebSocket server on port 81 (`WebSocketsServer`), patched via `pre:tools/patch_websockets.py`
- PubSubClient 2.8 (`knolleary/PubSubClient@^2.8`) — MQTT client (Home Assistant integration)
- Arduino `WebServer` — HTTP server on port 80 (bundled with Arduino-ESP32)
- Arduino `WiFi`, `WiFiClientSecure`, `HTTPClient`, `DNSServer`, `ETH` — networking (bundled)

**Data Serialization:**
- ArduinoJson 7.4.2 (`bblanchon/ArduinoJson@^7.4.2`) — JSON throughout REST API, WebSocket messages, settings persistence, MQTT payloads

**USB Audio:**
- TinyUSB 0.20.1 (bundled with Arduino-ESP32 P4) — UAC2 USB audio class device on native USB OTG, custom class driver registered via `usbd_app_driver_get_cb()` weak function; guarded by `-D USB_AUDIO_ENABLED`

**DSP / Audio:**
- ESP-DSP (pre-built `libespressif__esp-dsp.a`) — S3 assembly-optimized biquad IIR, FIR, Radix-4 FFT, vector math (`dsps_mulc_f32`, `dsps_add_f32`, `dsps_biquad`, `dsps_fir`); include paths added via `-I` in `platformio.ini`; used on ESP32 target only (`lib_ignore = esp_dsp_lite`)
- esp_dsp_lite (`lib/esp_dsp_lite/`) — ANSI C fallback for native unit tests; `lib_ignore`d on ESP32 environments
- arduinoFFT 2.0 (`kosme/arduinoFFT@^2.0`) — FFT spectrum analysis, included in `src/i2s_audio.cpp` and native test environment

**Crypto / Security:**
- mbedTLS (bundled with ESP-IDF) — SHA256 hashing for OTA firmware verification (`src/ota_updater.cpp`) and password hashing (`src/auth_handler.cpp`) via `<mbedtls/md.h>`
- `WiFiClientSecure` — TLS for HTTPS connections to GitHub API

**Storage:**
- LittleFS (`<LittleFS.h>`, Arduino-ESP32 bundled) — filesystem for settings, DSP config, HAL config, crash log, diagnostic journal; partition label `spiffs` (8 MB), `board_build.filesystem = littlefs`
- NVS Preferences (`<Preferences.h>`, Arduino-ESP32 bundled) — key-value NVS for WiFi credentials, MQTT settings, auth tokens, OTA flags, diagnostic journal sequence counter

**Testing:**
- Unity (PlatformIO built-in) — C++ unit test framework for native platform; 1271+ tests across 57 modules
- Playwright 1.50.0 (`@playwright/test@^1.50.0`) — E2E browser tests (26 tests, Chromium only)
- ESLint 8.57.0 (`eslint@^8.57.0`) — JS static analysis on `web_src/js/`
- Express 4.21.0 + cookie-parser 1.4.7 — mock server for Playwright E2E tests (`e2e/mock-server/server.js`)

## Key Dependencies

**Critical:**
- `links2004/WebSockets@^2.7.2` — Real-time UI updates; requires websocket patch script at build time
- `bblanchon/ArduinoJson@^7.4.2` — All JSON: REST API, WebSocket messages, MQTT payloads, settings files
- `knolleary/PubSubClient@^2.8` — MQTT client for Home Assistant integration, buffer size 1024 bytes
- `lvgl/lvgl@^9.4` — TFT GUI framework, FreeRTOS task on Core 0, `LVGL_CONF_INCLUDE_SIMPLE` flag required
- `lovyan03/LovyanGFX@^1.2.0` — TFT display driver; synchronous SPI only on P4 (LovyanGFX GDMA not wired for P4)

**Infrastructure:**
- `kosme/arduinoFFT@^2.0` — Native test FFT (ESP32 uses pre-built ESP-DSP FFT instead)
- `lib/esp_dsp_lite/` — ANSI C DSP fallback, native test-only (`platforms = native`)

## Configuration

**Environment:**
- All pin assignments are build flags in `platformio.ini` `[env:esp32-p4]` with fallback defaults in `src/config.h`
- Feature flags: `-D DSP_ENABLED`, `-D DAC_ENABLED`, `-D GUI_ENABLED`, `-D USB_AUDIO_ENABLED`
- No `.env` file — the firmware has no secret environment variables; MQTT credentials, WiFi passwords, and web password are stored in NVS at runtime
- Build flags also set: `WEBSOCKETS_MAX_DATA_SIZE=4096`, `CORE_DEBUG_LEVEL=2`, `LV_CONF_INCLUDE_SIMPLE`

**Build:**
- `platformio.ini` — primary build configuration
- `partitions_ota.csv` — custom partition table: nvs (20KB), otadata (8KB), app0/app1 (4MB each), spiffs (8MB), coredump (64KB); 16MB flash total
- `src/idf_component.yml` — IDF component manifest (`idf: '>=5.1'`)
- `src/gui/lv_conf.h` — LVGL configuration
- `src/gui/lgfx_config.h` — LovyanGFX display configuration

**Static Analysis (CI-enforced):**
- `web_src/.eslintrc.json` — ESLint config with 380 globals for concatenated JS scope
- cppcheck on `src/` (excludes `src/gui/`), `--std=c++11`

## Platform Requirements

**Development:**
- PlatformIO (Python-based) with `pio run`, `pio test -e native`, `pio device monitor`
- Upload port: COM8 at 115200 baud
- Node.js for web asset build: `node tools/build_web_assets.js` must be run after any `web_src/` edit
- RISC-V toolchain auto-patched by `tools/fix_riscv_toolchain.py` at build time

**Production:**
- Waveshare ESP32-P4-WiFi6-DEV-Kit, 16MB flash, PSRAM available
- WiFi via ESP32-C6 co-processor (firmware v2.11.6); Ethernet via EMAC + PHY at 100Mbps full duplex
- OTA update partitions: ota_0 and ota_1 (4MB each), firmware downloaded from GitHub Releases
- Serial monitor at 115200 baud, `monitor_filters = direct`

---

*Stack analysis: 2026-03-07*
