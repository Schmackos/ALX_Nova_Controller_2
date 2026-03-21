# Technology Stack

**Analysis Date:** 2026-03-10

## Languages

**Primary:**
- C++ (C++11) - All firmware source code in `src/`, compiled for ESP32-P4 (RISC-V) and native (host gcc/MinGW for tests)
- C - DSP coefficient computation (`src/dsp_biquad_gen.h/.c`), ESP-DSP lite fallback (`lib/esp_dsp_lite/`)

**Secondary:**
- JavaScript (ES2020) - Web UI frontend (`web_src/js/`), build tooling (`tools/`), E2E test infrastructure (`e2e/`)
- HTML/CSS - Web UI shell (`web_src/index.html`, `web_src/css/`)
- Python - PlatformIO build scripts (`tools/fix_riscv_toolchain.py`)
- YAML - GitHub Actions CI/CD (`.github/workflows/`)

## Runtime

**Firmware Environment:**
- ESP-IDF v5.x (via Arduino framework on pioarduino ESP32 platform)
- FreeRTOS - Core RTOS for task management, event groups, semaphores
- Target: ESP32-P4 (RISC-V dual-core, Waveshare ESP32-P4-WiFi6-DEV-Kit)
- Flash: 16MB with OTA dual-partition layout
- PSRAM: Available (used for DSP delay lines, USB audio ring buffers, diagnostic journal)

**Test Environment:**
- Native platform (host machine gcc/MinGW) for C++ unit tests
- Node.js >= 18.0 for E2E tests and build tooling

**Package Manager:**
- PlatformIO (firmware dependencies via `lib_deps` in `platformio.ini`)
- npm (E2E tests in `e2e/package.json`, docs site in `docs-site/package.json`)
- Lockfile: `e2e/package-lock.json` present, PlatformIO manages its own lock

## Frameworks

**Core:**
- Arduino (ESP-IDF based) - Framework for ESP32-P4, provides WiFi, WebServer, I2S, I2C, SPI, LittleFS, Preferences APIs
- pioarduino platform `55.03.37` - ESP32-P4 Arduino core (`platformio.ini` line 16)
- FreeRTOS - Task scheduling, event groups (`app_events.h`), semaphores, mutexes

**GUI:**
- LVGL v9.4 - GUI framework for TFT display (guarded by `-D GUI_ENABLED`)
- LovyanGFX v1.2.0 - TFT display driver for ST7735S 128x160 (landscape 160x128)

**Testing:**
- Unity - C++ unit test framework (native platform, 1621 tests across 70 modules)
- Playwright v1.50+ - Browser E2E tests (26 tests across 19 specs, Chromium only)
- Express v4.21 - Mock HTTP server for E2E tests (`e2e/mock-server/server.js`)

**Build/Dev:**
- PlatformIO - Build system, dependency management, test runner, upload/monitor
- Docusaurus v3 - Public documentation site (`docs-site/`)
- cppcheck - C++ static analysis (CI-enforced)
- ESLint v8.57 - JavaScript linting for `web_src/js/` (380+ globals for concatenated scope)

**Documentation Site:**
- Docusaurus v3.7 (`docs-site/`) with Mermaid diagrams, local search, MDX
- React 18 + MDX + Prism syntax highlighting
- KaTeX (math rendering via `rehype-katex` + `remark-math`)

## Key Dependencies

**Critical (firmware):**
- `ArduinoJson@^7.4.2` - JSON parsing/serialization throughout codebase (REST API, WebSocket, settings, MQTT)
- `WebSockets` - WebSocket server for real-time UI updates (vendored in `lib/WebSockets/`, NOT from registry)
- `PubSubClient@^2.8` - MQTT client for Home Assistant integration
- `lvgl@^9.4` - GUI framework for TFT display (guarded by `GUI_ENABLED`)
- `LovyanGFX@^1.2.0` - TFT display driver (replaced TFT_eSPI)

**Audio/DSP (firmware):**
- ESP-DSP pre-built library (`libespressif__esp-dsp.a`) - Assembly-optimized biquad IIR, FIR, Radix-4 FFT, window functions, vector math. Include paths via `-I` flags in `platformio.ini`. `lib/esp_dsp_lite/` provides ANSI C fallbacks for native tests only (`lib_ignore = esp_dsp_lite` on ESP32)
- `arduinoFFT@^2.0` - FFT spectrum analysis (native tests only; ESP32 uses ESP-DSP FFT)
- TinyUSB (bundled with IDF) - UAC2 USB Audio speaker device

**Infrastructure (firmware):**
- `LittleFS` (ESP-IDF built-in) - Filesystem for config, DSP presets, crash logs, diagnostic journal
- `Preferences` (ESP-IDF built-in) - NVS key-value storage for WiFi credentials, persistent counters
- `WiFiClientSecure` + mbedTLS (ESP-IDF built-in) - TLS for OTA downloads from GitHub
- `WebServer` (ESP-IDF built-in) - HTTP server on port 80
- `DNSServer` (ESP-IDF built-in) - Captive portal DNS in AP mode
- `HTTPClient` (ESP-IDF built-in) - HTTP client for GitHub API (OTA checks)
- `Update` (ESP-IDF built-in) - OTA firmware update mechanism

**E2E Testing (npm):**
- `@playwright/test@^1.50.0` - Browser automation
- `express@^4.21.0` - Mock server
- `cookie-parser@^1.4.7` - Session cookie handling in mock server
- `eslint@^8.57.0` - JavaScript linting

**Documentation Site (npm):**
- `@docusaurus/core@^3.7.0` - Site framework
- `@docusaurus/theme-mermaid@^3.7.0` - Mermaid diagram support
- `@easyops-cn/docusaurus-search-local@^0.44.5` - Offline local search
- `@docusaurus/plugin-pwa@^3.9.2` - Progressive Web App support
- `@anthropic-ai/sdk` - Claude API for automated doc generation (CI only)

## Configuration

**Build Configuration:**
- `platformio.ini` - Primary build config with 3 environments: `esp32-p4` (target), `p4_hosted_update` (minimal OTA test), `native` (host tests)
- `partitions_ota.csv` - Flash partition layout: NVS (20KB), OTA data (8KB), app0 (4MB), app1 (4MB), SPIFFS/LittleFS (7.875MB), coredump (64KB)
- Build flags control feature guards: `-D DSP_ENABLED`, `-D DAC_ENABLED`, `-D GUI_ENABLED`, `-D USB_AUDIO_ENABLED`
- Optimization: `-Os` (size-optimized)
- `tools/fix_riscv_toolchain.py` - Pre-build script for RISC-V toolchain path fixes

**Compile-Time Feature Guards:**
- `DSP_ENABLED` - DSP pipeline, biquad filters, FIR, crossover
- `DAC_ENABLED` - HAL framework, DAC/ADC drivers, device discovery, pipeline bridge
- `GUI_ENABLED` - LVGL TFT display, rotary encoder input, screen navigation
- `USB_AUDIO_ENABLED` - TinyUSB UAC2 speaker device on USB OTG
- `UNIT_TEST` + `NATIVE_TEST` - Native test environment (mock stubs)

**Runtime Configuration (LittleFS):**
- `/config.json` - Primary settings (JSON v1 format, atomic write via `.tmp` + rename)
- `/hal_config.json` - Per-device HAL configuration (I2C addr, pins, volume, mute, enabled)
- `/dsp_config.json` - DSP pipeline presets
- `/diag_journal.bin` - Persistent diagnostic event ring buffer (800 entries, CRC32 per entry)
- `/crashlog.bin` - Boot crash log ring buffer (10 entries)

**Runtime Configuration (NVS/Preferences):**
- WiFi credentials (survive LittleFS format)
- Diagnostic journal sequence counter
- Selected NVS settings

**Pin Configuration:**
- Pins defined as `-D` build flags in `platformio.ini` with fallback defaults in `src/config.h`
- HAL devices can override pins at runtime via `HalDeviceConfig` (gpioA/B/C/D, pinSda, pinScl, pinBck, etc.)

**Environment Variables (CI only):**
- `ANTHROPIC_API_KEY` - Required for documentation auto-generation in CI (`docs.yml`)
- Standard GitHub Actions secrets for release workflow

## Platform Requirements

**Development:**
- PlatformIO CLI or IDE (VS Code extension recommended)
- Python 3.11+ (PlatformIO dependency)
- Node.js >= 18 (E2E tests, build tooling)
- COM port access for upload/monitor (default: COM8)
- Git with `.githooks/pre-commit` support (activate: `git config core.hooksPath .githooks`)

**Production:**
- Waveshare ESP32-P4-WiFi6-DEV-Kit (ESP32-P4 RISC-V with ESP32-C6 WiFi co-processor)
- 16MB Flash, PSRAM
- ST7735S 128x160 TFT display (optional, guarded by GUI_ENABLED)
- USB OTG port (optional, for USB Audio)
- 100Mbps Ethernet (optional, built-in EMAC + PHY)

**CI/CD:**
- GitHub Actions (Ubuntu latest runners)
- 4 parallel quality gates: cpp-tests, cpp-lint, js-lint, e2e-tests
- Firmware build only proceeds after all 4 gates pass
- Release workflow: manual dispatch with version bump (patch/minor/major) and channel (stable/beta)
- Documentation deployment to GitHub Pages (`gh-pages` branch)

## Web UI Build Pipeline

**Source:** `web_src/index.html`, `web_src/css/*.css`, `web_src/js/*.js`
**Build:** `node tools/build_web_assets.js` concatenates and gzip-compresses into `src/web_pages.cpp` + `src/web_pages_gz.cpp`
**Output:** Embedded HTTP responses served directly from ESP32 flash (gzip-compressed, self-contained, works offline)
**Design Tokens:** `src/design_tokens.h` -> `tools/extract_tokens.js` -> CSS for web UI (`web_src/css/00-tokens.css`) AND docs site (`docs-site/src/css/tokens.css`)

## Vendored Libraries

**`lib/WebSockets/`:**
- WebSocket server library (fork/vendored copy, not from PlatformIO registry)
- `lib_ignore = WebSockets` in native env (not needed for tests)

**`lib/esp_dsp_lite/`:**
- ANSI C fallback implementations of ESP-DSP functions for native tests
- `lib_ignore = esp_dsp_lite` in ESP32 env (uses pre-built `.a` instead)

---

*Stack analysis: 2026-03-10*
