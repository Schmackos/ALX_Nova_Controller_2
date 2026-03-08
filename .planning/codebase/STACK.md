# Technology Stack

**Analysis Date:** 2026-03-08

## Languages

**Primary:**
- C++11 — Firmware source in `src/` (Arduino framework style; PlatformIO enforces `-std=c++11` on native env)
- C — Low-level DSP coefficient math in `src/dsp_biquad_gen.c`, `src/safe_snr_sfdr.c`

**Secondary:**
- JavaScript (ES2015+) — Web UI in `web_src/js/01-28-*.js` (concatenated single-file delivery)
- CSS — Web UI styling in `web_src/css/01-05-*.css`
- HTML — Web UI shell in `web_src/index.html`
- Python — Build tooling only (`tools/fix_riscv_toolchain.py` pre-script)

## Runtime

**Environment:**
- ESP32-P4 RISC-V dual-core MCU (Waveshare ESP32-P4-WiFi6-DEV-Kit)
- FreeRTOS (bundled with ESP-IDF 5.x via Arduino framework)
- Core 0: GUI task, MQTT task, USB Audio task, OTA tasks
- Core 1: Arduino main loop + audio pipeline task (isolated for real-time audio)

**Package Manager:**
- PlatformIO (CLI) — manages both ESP32 and native test environments
- npm — Node.js tooling only (`e2e/package.json`); not part of firmware build
- Lockfile: none explicit (PlatformIO resolves versions from `platformio.ini`)

## Frameworks

**Core:**
- Arduino framework (`framework = arduino`) — abstraction over ESP-IDF 5.x
- ESP-IDF 5.1+ (`idf: '>=5.1'` in `src/idf_component.yml`) — underlying RTOS, I2S, I2C, LEDC, EMAC drivers

**Display:**
- LVGL v9.4 — GUI framework (guarded by `-D GUI_ENABLED`), defined in `platformio.ini` `lib_deps`
- LovyanGFX v1.2.0 — TFT driver for ST7735S 128×160, synchronous SPI (no DMA on P4)

**Audio DSP:**
- ESP-DSP (pre-built, Xtensa/RISC-V assembly-optimised) — biquad IIR, Radix-4 FFT, FIR, vector math. Include paths added via `-I` in `platformio.ini`. Library binary at `libespressif__esp-dsp.a`
- `lib/esp_dsp_lite/` — ANSI C fallback for native test env only (`lib_ignore = esp_dsp_lite` on ESP32)
- arduinoFFT v2.0 — FFT used **in native test env only** (`kosme/arduinoFFT@^2.0` under `[env:native]`)

**USB:**
- TinyUSB v0.20.1 (bundled with Arduino-ESP32 3.x) — UAC2 speaker device on native USB OTG. Custom audio class driver registered via `usbd_app_driver_get_cb()` weak function (`src/usb_audio.cpp`)

**Testing:**
- Unity (C) — unit test framework; provided by PlatformIO native platform. Config: `test_framework = unity` in `[env:native]`
- Playwright — browser E2E tests in `e2e/tests/` (19 spec files, 26 tests). Config: `e2e/playwright.config.js`
- Express.js — mock server for E2E tests (`e2e/mock-server/server.js`, port 3000)

**Build/Dev:**
- PlatformIO — build, upload, test orchestration
- Node.js — web asset build pipeline (`tools/build_web_assets.js` → `src/web_pages.cpp` + `src/web_pages_gz.cpp`)
- ESLint — JS linting (`web_src/.eslintrc.json`)

## Key Dependencies

**Critical (firmware, via `platformio.ini` `lib_deps`):**
- `bblanchon/ArduinoJson@^7.4.2` — JSON parsing throughout: settings, API, MQTT, HAL config
- `knolleary/PubSubClient@^2.8` — MQTT client (lives in `src/mqtt_handler.cpp`, `src/mqtt_task.cpp`)
- `lovyan03/LovyanGFX@^1.2.0` — TFT display driver for ST7735S (replaces TFT_eSPI)
- `lvgl/lvgl@^9.4` — GUI framework for rotary encoder UI (guarded by `GUI_ENABLED`)
- `WebSockets@^2.7.2` — WebSocket server port 81 (`lib/WebSockets/`; vendored in repo)

**Critical (native test env):**
- `kosme/arduinoFFT@^2.0` — FFT computation in native tests

**Infrastructure (IDF built-ins, included via `<...>`):**
- `<WiFi.h>`, `<WiFiClientSecure.h>` — via ESP32-C6 co-processor (SDIO)
- `<HTTPClient.h>` — OTA downloads, GitHub API calls
- `<WebServer.h>` — HTTP server port 80
- `<WebSocketsServer.h>` — WebSocket server port 81
- `<LittleFS.h>` — filesystem for config, DSP presets, HAL config, crash log, diagnostic journal
- `<Preferences.h>` — NVS key-value store (WiFi credentials, auth, OTA flags)
- `<Update.h>` — OTA flash write
- `<DNSServer.h>` — captive portal in AP mode
- `<mbedtls/md.h>` — PBKDF2-SHA256 password hashing, SHA256 firmware verification
- `<driver/i2s_std.h>` — I2S audio driver (IDF5 new API)
- `<driver/temperature_sensor.h>` — Internal chip temperature (ESP32-P4 only)
- `<esp_task_wdt.h>` — Task watchdog timer (30s timeout configured at boot)
- `<freertos/FreeRTOS.h>` — RTOS primitives throughout

## Configuration

**Environment:**
- Build flags set all pin assignments in `platformio.ini` `build_flags` (e.g., `-D LED_PIN=1`)
- Fallback defaults in `src/config.h` via `#ifndef` guards
- Feature flags: `-D DSP_ENABLED`, `-D DAC_ENABLED`, `-D GUI_ENABLED`, `-D USB_AUDIO_ENABLED`
- Runtime settings stored in LittleFS at `/config.json` (primary) with atomic write via `.tmp` rename
- Legacy format: `/settings.txt` + `/mqtt_config.txt` — auto-migrated on first boot
- WiFi credentials and NVS settings survive LittleFS format (stored via `<Preferences.h>`)
- HAL device configs persisted to `/hal_config.json`
- DSP configs persisted to LittleFS per-channel JSON files (via `src/dsp_api.cpp`)

**Build:**
- `platformio.ini` — three environments: `esp32-p4` (primary), `p4_hosted_update` (hosted update test), `native` (unit tests)
- `partitions_ota.csv` — custom partition table: NVS 20KB, OTA data 8KB, app0 4MB, app1 4MB, spiffs 7.875MB, coredump 64KB
- Flash total: 16MB
- `tools/fix_riscv_toolchain.py` — pre-script fixing RISC-V toolchain path for ESP32-P4

## Platform Requirements

**Development:**
- PlatformIO CLI (or IDE plugin)
- Node.js (for `tools/build_web_assets.js` and `e2e/` tests)
- Windows host (upload port `COM8`, MSYS2/bash shell used in CI scripts)
- MinGW/GCC for native test compilation

**Production:**
- Waveshare ESP32-P4-WiFi6-DEV-Kit (ESP32-P4 + ESP32-C6 co-processor for WiFi6)
- 16MB flash, PSRAM available (DSP delay lines use `ps_calloc()` when present)
- Serial monitor at 115200 baud on COM8

---

*Stack analysis: 2026-03-08*
