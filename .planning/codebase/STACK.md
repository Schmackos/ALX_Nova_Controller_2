# Technology Stack

**Analysis Date:** 2026-03-09

## Languages

**Primary:**
- C/C++ (C++11) — Firmware source in `src/`, test mocks in `test/test_mocks/`
- JavaScript (ES5/ES6) — Web UI in `web_src/js/` (concatenated single-file scope), tooling in `tools/`
- C — DSP coefficient generation (`src/dsp_biquad_gen.c`), safe computation (`src/safe_snr_sfdr.c`)

**Secondary:**
- Python — PlatformIO CI runner, RISC-V toolchain fix script (`tools/fix_riscv_toolchain.py`)
- CSS — Web UI styling split across `web_src/css/00-tokens.css` through `06-health-dashboard.css`
- YAML — GitHub Actions workflows (`.github/workflows/`)
- JSX/MDX — Docusaurus documentation site (`docs-site/src/`)

## Runtime

**Target Firmware:**
- ESP32-P4 (RISC-V, dual-core) at 400 MHz
- ESP-IDF v5.1+ (Arduino framework via pioarduino platform 55.03.37)
- FreeRTOS (bundled with IDF5)
- PSRAM available; DSP delay lines allocated via `ps_calloc()`

**Host (native tests):**
- GCC/MinGW (Windows) or GCC (Linux/Ubuntu in CI)
- C++11 standard (`-std=c++11` in native env)
- Unity test framework (bundled with PlatformIO)

**Web UI:**
- Browser (self-hosted, served from ESP32 on port 80)
- No separate build step — raw HTML/CSS/JS assembled by `tools/build_web_assets.js` into `src/web_pages.cpp` and gzip-compressed into `src/web_pages_gz.cpp`

**Docs Site:**
- Node.js 18+ required (`docs-site/package.json` `engines.node: ">=18.0"`)

## Package Manager

**Firmware:**
- PlatformIO (installed via `pip install platformio`)
- Platform pinned by URL: `https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip`
- Library versions specified in `platformio.ini` `lib_deps`

**E2E Tests:**
- npm with lockfile at `e2e/package-lock.json`

**Docs Site:**
- npm with lockfile at `docs-site/package-lock.json` (Node 18+ minimum)

## Frameworks

**Core Firmware:**
- Arduino Framework (via pioarduino ESP32 platform, IDF5 base) — `framework = arduino` in `platformio.ini`
- FreeRTOS — task scheduling, binary semaphores, event groups (`src/app_events.h`)
- ESP-IDF 5 — IDF drivers used directly: `<driver/i2s_std.h>`, `<driver/temperature_sensor.h>`, `<driver/mcpwm_prelude.h>`, `<esp_task_wdt.h>`, `<mbedtls/md.h>`, `<mbedtls/pkcs5.h>`

**GUI:**
- LVGL v9.4 (`lvgl/lvgl@^9.4`) — embedded GUI framework, guarded by `-D GUI_ENABLED`
- LovyanGFX v1.2.0 (`lovyan03/LovyanGFX@^1.2.0`) — TFT display driver (ST7735S, synchronous SPI on P4)
- LVGL config in `src/gui/lgfx_config.h`

**Audio DSP:**
- ESP-DSP pre-built library (`libespressif__esp-dsp.a`) — S3-assembly-optimized biquad IIR, FIR, Radix-4 FFT, vector math. Include paths added via `-I` flags in `platformio.ini`. Not in `lib_deps` — linked as pre-built static archive
- `lib/esp_dsp_lite/` — ANSI C fallback for native test environment only (excluded from ESP32 builds via `lib_ignore = esp_dsp_lite`)
- arduinoFFT v2.0 (`kosme/arduinoFFT@^2.0`) — used in native test environment only

**Web Server:**
- `WebServer` (Arduino ESP32 built-in, port 80) — synchronous HTTP REST API
- `WebSocketsServer` v2.7.3 (vendored `lib/WebSockets/`) — real-time state broadcasting on port 81
- `DNSServer` (Arduino ESP32 built-in) — captive portal in AP mode

**Testing:**
- Unity (PlatformIO built-in) — C++ unit tests on native platform, 1614 tests across 70 modules
- Playwright v1.50.0 (`@playwright/test@^1.50.0`) — browser E2E tests in `e2e/`, 26 tests across 19 specs
- Express v4.21.0 (`express@^4.21.0`) — mock server for E2E tests (`e2e/mock-server/server.js`, port 3000)

**Documentation Site:**
- Docusaurus v3.7.0 (`@docusaurus/core@^3.7.0`, `@docusaurus/preset-classic@^3.7.0`)
- `@docusaurus/theme-mermaid@^3.7.0` — Mermaid diagram rendering
- `@easyops-cn/docusaurus-search-local@^0.44.5` — offline search (no external CDN)
- React v18 (`react@^18.0.0`) + MDX v3 (`@mdx-js/react@^3.0.0`) — Docusaurus rendering stack
- `prism-react-renderer@^2.0.0` — syntax highlighting
- `clsx@^2.0.0` — CSS class utilities
- Deployed to GitHub Pages (`gh-pages` branch) via `peaceiris/actions-gh-pages@v3`

## Key Dependencies

**Critical (Firmware):**
- `bblanchon/ArduinoJson@^7.4.2` — JSON parsing throughout firmware; REST API, MQTT, settings, HAL config. Both firmware and native environments.
- `knolleary/PubSubClient@^2.8` — MQTT client (`src/mqtt_handler.cpp`, `src/mqtt_publish.cpp`, `src/mqtt_ha_discovery.cpp`)
- `lovyan03/LovyanGFX@^1.2.0` — TFT display driver (ESP32 target only, `GUI_ENABLED`)
- `lvgl/lvgl@^9.4` — GUI framework (ESP32 target only, `GUI_ENABLED`)
- WebSockets v2.7.3 (vendored `lib/WebSockets/`) — WebSocket server, configured with `-D WEBSOCKETS_MAX_DATA_SIZE=4096`
- TinyUSB (bundled with Arduino ESP32 IDF5) — UAC2 USB audio device, guarded by `USB_AUDIO_ENABLED`

**Critical (Testing / Tooling):**
- `kosme/arduinoFFT@^2.0` — FFT in native test environment (replaces pre-built ESP-DSP)
- `@playwright/test@^1.50.0` — E2E browser test runner
- `eslint@^8.57.0` — JS linting (`web_src/.eslintrc.json` config, 380 globals for concatenated scope)
- `cookie-parser@^1.4.7` — session cookie parsing in E2E mock server
- `@anthropic-ai/sdk` — doc generation script (`tools/generate_docs.js`) calls Claude API to generate Docusaurus pages

**Infrastructure (Firmware):**
- `mbedtls` (IDF5 built-in) — SHA256 (`mbedtls/md.h`), PBKDF2-SHA256 10,000 iterations (`mbedtls/pkcs5.h`) for auth password hashing; SHA256 for OTA firmware verification
- `Preferences` (Arduino ESP32 built-in) — NVS key-value store for WiFi credentials, password hash, device serial
- `LittleFS` (Arduino ESP32 built-in, labeled `spiffs` in partition table) — `/config.json`, `/hal_config.json`, `/pipeline_matrix.json`, `/diag_journal.bin`, DSP presets
- `Update` (Arduino ESP32 built-in) — OTA firmware flash
- `WiFiClientSecure` (Arduino ESP32 built-in) — TLS connections to GitHub Releases API

## Configuration

**Build Flags (all in `platformio.ini` `[env:esp32-p4]`):**
- Feature gates: `-D DSP_ENABLED`, `-D DAC_ENABLED`, `-D USB_AUDIO_ENABLED`, `-D GUI_ENABLED`
- Pin assignments: `LED_PIN=1`, `BUZZER_PIN=45`, `I2S_BCK_PIN=20`, `I2S_DOUT_PIN=23`, `I2S_DOUT2_PIN=25`, `I2S_LRC_PIN=21`, `I2S_MCLK_PIN=22`, `SIGGEN_PWM_PIN=47`, `TFT_MOSI_PIN=2`, `TFT_SCLK_PIN=3`, `TFT_CS_PIN=4`, `TFT_DC_PIN=5`, `TFT_RST_PIN=6`, `TFT_BL_PIN=26`, `ENCODER_A_PIN=32`, `ENCODER_B_PIN=33`, `ENCODER_SW_PIN=36`, `DAC_I2C_SDA_PIN=48`, `DAC_I2C_SCL_PIN=54`, `I2S_TX_DATA_PIN=24`
- USB mode: `-D ARDUINO_USB_MODE=0 -D ARDUINO_USB_CDC_ON_BOOT=0` (native USB OTG for TinyUSB)
- Optimization: `-Os` (size-optimized)
- WebSocket buffer: `-D WEBSOCKETS_MAX_DATA_SIZE=4096`
- LVGL config: `-D LV_CONF_INCLUDE_SIMPLE -I src/gui`

**Fallback defaults:** All pin definitions have `#ifndef` guards in `src/config.h` so pins can be overridden via build flags without editing source.

**Firmware version string:**
- `FIRMWARE_VERSION` defined as `"1.12.1"` in `src/config.h` — bumped automatically by `release.yml` CI workflow

**Runtime persistent configuration:**
- `/config.json` (LittleFS) — primary settings (JSON v1 format with version field and atomic write via `.tmp` rename)
- `/hal_config.json` (LittleFS) — HAL per-device pin overrides and configuration
- `/pipeline_matrix.json` (LittleFS) — 16x16 audio routing matrix persistence
- `/diag_journal.bin` (LittleFS) — diagnostic event journal (800 entries, 64KB)
- NVS `authPrefs` namespace — web password hash (PBKDF2-SHA256 format `p1:<saltHex>:<keyHex>`)
- NVS WiFi namespaces — up to 5 saved WiFi networks

**Build artifacts:**
- `partitions_ota.csv` — 16 MB flash layout: NVS 20KB, OTA data 8KB, app0 4MB (ota_0), app1 4MB (ota_1), spiffs/LittleFS 7.875MB, coredump 64KB
- `tools/fix_riscv_toolchain.py` — pre-build script to patch RISC-V toolchain path on CI
- `tools/build_web_assets.js` — assembles `web_src/index.html`, `web_src/css/*.css`, `web_src/js/*.js` into `src/web_pages.cpp` + `src/web_pages_gz.cpp`

## Platform Requirements

**Development:**
- PlatformIO Core (installed via Python pip)
- Python 3.11+
- Node.js 20 (CI target), Node.js 18+ (docs site minimum)
- Serial port COM8 (upload and monitor at 115200 baud)
- Target board: Waveshare ESP32-P4-WiFi6-DEV-Kit

**Native test environment:**
- GCC/MinGW (Windows) or GCC (Linux)
- `UNIT_TEST` and `NATIVE_TEST` preprocessor flags active
- Arduino/WiFi/MQTT/Preferences/LittleFS/I2S mocked in `test/test_mocks/`

**Production:**
- Waveshare ESP32-P4-WiFi6-DEV-Kit
- 16 MB flash, PSRAM (used for DSP delay lines and USB audio ring buffer)
- WiFi via onboard ESP32-C6 co-processor (firmware v2.11.6)
- Ethernet via EMAC 100Mbps Full Duplex (`src/eth_manager.cpp`)
- Deployed as self-contained embedded appliance; web UI served from device, no cloud dependency at runtime

---

*Stack analysis: 2026-03-09*
