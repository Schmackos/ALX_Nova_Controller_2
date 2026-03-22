# Technology Stack

**Analysis Date:** 2026-03-22

## Languages

**Primary:**
- C++ (C++11) — Firmware codebase (`src/`, ~90 modules)
- JavaScript (ES6+) — Web UI (`web_src/js/`, 22 concatenated files in shared scope)
- HTML5 / CSS3 — Web pages and styling (`web_src/index.html`, `web_src/css/`, 7 CSS files)

**Secondary:**
- Python — Build tooling (`tools/fix_riscv_toolchain.py`)
- Node.js — Build scripts, linting tools, doc generation (`tools/*.js`, 13 scripts)
- Markdown / MDX — Documentation site (`docs-site/`, 26 pages)

## Runtime

**Environment:**
- ESP32-P4 (RISC-V dual-core, 400 MHz, 8 MB SRAM, 8 MB PSRAM)
- Arduino framework v3.x (ESP-IDF 5 underneath) via pioarduino platform-espressif32 v55.03.37
- FreeRTOS (integrated in IDF5) — Multi-core task scheduling, mutexes, semaphores, event groups
- Node.js 18+ — Development tooling, E2E tests, documentation build

**Package Manager:**
- PlatformIO (pio) — C++ firmware dependencies and build orchestration
- npm — JavaScript dependencies (E2E tests in `e2e/`, docs in `docs-site/`)

**Lockfiles:**
- `platformio.ini` — Library versions pinned with `@^` semver (no separate lock file)
- `e2e/package-lock.json` — Present
- `docs-site/package-lock.json` — Present

## Frameworks

**Core Firmware:**
- Arduino Core for ESP32 v3.x — GPIO, I2C, I2S, WiFi, UART, Preferences (NVS), LittleFS APIs
- FreeRTOS (IDF5 integrated) — Task scheduling, core affinity, binary semaphores, event groups, mutex

**Audio Processing:**
- ESP-DSP (pre-built `libespressif__esp-dsp.a`) — Assembly-optimized biquad IIR, FIR convolution, Radix-4 FFT, window functions, vector math. Include paths added via `-I` flags in `platformio.ini`. Used on ESP32 only.
- arduinoFFT 2.0 — FFT spectrum analysis (native tests only; `lib_ignore = esp_dsp_lite` on ESP32 env)
- `lib/esp_dsp_lite/` — ANSI C fallback of ESP-DSP for native test compilation

**GUI:**
- LVGL v9.4 — Embedded graphics library for ST7735S 128x160 TFT (landscape 160x128)
- LovyanGFX v1.2.0 — Display driver abstraction for ST7735S (SPI, no DMA on P4)

**Web Server:**
- Arduino WebServer (built-in) — HTTP server on port 80 with REST API
- WebSocketsServer (vendored in `lib/WebSockets/`) — Real-time state push on port 81, binary + JSON frames

**Testing:**
- Unity (PlatformIO built-in) — C++ unit test framework (~2316 tests across 87 modules on native platform)
- Playwright v1.50.0 — Browser E2E tests (107 tests across 22 Chromium specs)

**Build/Dev:**
- PlatformIO CLI — Firmware build, upload, test orchestration
- Node.js build scripts (`tools/`) — Web asset gzip compression, doc generation, linting, cert updates
- cppcheck — C++ static analysis (CI-enforced, excludes `src/gui/`)
- ESLint 8.57.0 — JavaScript linting with 380+ globals for concatenated scope (`web_src/.eslintrc.json`)

**Documentation:**
- Docusaurus v3.7.0 — Static site generator, deployed to GitHub Pages (`docs-site/`)
- Mermaid — Architecture diagrams in MDX pages (`@docusaurus/theme-mermaid`)

## Key Dependencies

**Critical (firmware, in `platformio.ini` lib_deps):**
- ArduinoJson 7.4.2 — JSON parsing/serialization for REST API, config, WebSocket messages
- PubSubClient 2.8 — MQTT client for Home Assistant integration (`src/mqtt_handler.cpp`)
- LovyanGFX 1.2.0 — ST7735S TFT display driver (SPI, guarded by `GUI_ENABLED`)
- lvgl 9.4 — LVGL GUI framework (Core 0 task, guarded by `GUI_ENABLED`)

**Vendored (in `lib/`):**
- WebSockets (`lib/WebSockets/`) — WebSocket server for port 81 broadcasts. Vendored, not from registry. Build flags: `WEBSOCKETS_MAX_DATA_SIZE=4096`, `WEBSOCKETS_SERVER_CLIENT_MAX=16`

**Conditionally compiled:**
- arduinoFFT 2.0 — FFT for native tests only (production uses ESP-DSP)
- TinyUSB 0.20.1 — USB Audio UAC2 speaker device (guarded by `USB_AUDIO_ENABLED`)

**ESP32 core libraries (no explicit lib_deps, built-in):**
- mbedtls — TLS/SSL (PBKDF2-SHA256 for auth, ESP32CertBundle for HTTPS)
- WiFiClientSecure / HTTPClient — OTA firmware download over HTTPS
- Preferences — NVS (Non-Volatile Storage) for credentials, serial number
- LittleFS — Config/preset persistence on SPI flash
- Update — OTA firmware streaming + partition swap
- esp-idf drivers — I2S, I2C, SPI, MCPWM, temperature sensor, EMAC (Ethernet)

**Development (npm, `e2e/package.json`):**
- @playwright/test ^1.50.0 — Browser E2E test runner
- ESLint ^8.57.0 — JavaScript static analysis
- Express ^4.21.0 — Mock HTTP server for E2E tests
- cookie-parser ^1.4.7 — Cookie middleware for test server

**Documentation (npm, `docs-site/package.json`):**
- @docusaurus/core ^3.7.0 — Static site generator
- @docusaurus/plugin-pwa ^3.9.2 — Progressive Web App support
- @docusaurus/theme-mermaid ^3.7.0 — Mermaid diagram rendering
- @easyops-cn/docusaurus-search-local ^0.44.5 — Offline-capable local search
- react ^18.0.0 / react-dom ^18.0.0 — Docusaurus rendering engine
- rehype-katex / remark-math — LaTeX math rendering in docs

## Configuration

**Environment:**
- No `.env` file used; all configuration persisted on-device:
  - `/config.json` (LittleFS, primary — atomic write via temp+rename) — `src/settings_manager.cpp`
  - `/hal_config.json` (LittleFS) — HAL device overrides and custom configs — `src/hal/hal_settings.cpp`
  - `/mqtt_config.txt` (LittleFS, legacy format, read-only after first boot)
  - `/settings.txt` (legacy fallback, first boot only)
  - NVS namespaces: `"device"` (serial), `"auth"` (password hash), `"mqtt"` (broker creds)
- NVS survives LittleFS format; WiFi credentials persist through factory reset

**Build Flags (`platformio.ini`):**
- `DSP_ENABLED` — Enables DSP pipeline, DSP API endpoints, DSP presets
- `DAC_ENABLED` — Enables audio output, DAC drivers, HAL framework
- `GUI_ENABLED` — Enables LVGL TFT GUI (Core 0 task)
- `USB_AUDIO_ENABLED` — Enables TinyUSB UAC2 speaker device
- `UNIT_TEST` / `NATIVE_TEST` — Native test compilation flags
- `-Os` — Size optimization
- `-D CORE_DEBUG_LEVEL=2` — Info+ serial logging
- Pin definitions passed as `-D` flags (e.g., `-D LED_PIN=1`, `-D I2S_BCK_PIN=20`)
- `WEBSOCKETS_MAX_DATA_SIZE=4096`, `WEBSOCKETS_SERVER_CLIENT_MAX=16`

**Partition Table (`partitions_ota.csv`):**
- `nvs` — 20 KB (0x9000)
- `otadata` — 8 KB (0xe000)
- `app0` (ota_0) — 4 MB (0x10000) — Active firmware
- `app1` (ota_1) — 4 MB (0x410000) — OTA staging
- `spiffs` (LittleFS) — 7.875 MB (0x810000) — Config, presets, diagnostics
- `coredump` — 64 KB (0xFF0000) — Crash diagnostics

**Hardware-Specific Pin Configuration:**
- I2S Audio: BCK=20, LRC=21, MCLK=22, DOUT=23 (ADC1), DOUT2=25 (ADC2), TX=24 (DAC)
- I2C Bus 0: SDA=48, SCL=54 (shared with WiFi SDIO — disabled when WiFi active)
- I2C Bus 1: SDA=7, SCL=8 (ES8311 onboard, dedicated, always safe)
- I2C Bus 2: SDA=28, SCL=29 (expansion mezzanine, always safe)
- TFT: MOSI=2, SCLK=3, CS=4, DC=5, RST=6, BL=26
- Encoder: A=32, B=33, SW=36
- GPIO: LED=1, Amplifier=27, Reset=46, Buzzer=45, SigGen PWM=47, NS4150B/PA=53

## Platform Requirements

**Development:**
- Windows 11 / macOS / Linux
- Visual Studio Code (recommended with PlatformIO extension)
- PlatformIO CLI (`pio`) — Python 3.11+ (CI uses 3.11)
- Node.js 18+ (for E2E tests, docs, build scripts)
- Git (with optional `.githooks/pre-commit` hook activation: `git config core.hooksPath .githooks`)

**Hardware (Target):**
- Waveshare ESP32-P4-WiFi6-DEV-Kit (single official board target)
- Flash: 16 MB SPI NOR
- PSRAM: 8 MB SPIRAM
- RAM: 8 MB internal SRAM
- USB: Native USB OTG (TinyUSB UAC2, no CH340 serial adapter)
- Ethernet: 100 Mbps full-duplex via EMAC + RTL8201F PHY
- WiFi: ESP32-C6 co-processor (via SDIO to P4 host)
- Display: ST7735S 128x160 TFT (SPI)

**Production:**
- Upload: USB serial on COM8 (configurable in `platformio.ini`)
- OTA updates via HTTPS from GitHub Releases (firmware.bin + SHA256 checksum)
- Web UI: Embedded HTML/CSS/JS (gzip-compressed in `src/web_pages_gz.cpp`, ~350 KB uncompressed)
- All configuration persisted to LittleFS + NVS — fully self-contained, no cloud dependency

## Build & Test Pipeline

**Firmware Build:**
```bash
pio run                          # Build for esp32-p4
pio run --target upload          # Upload to device (COM8)
pio device monitor               # Serial monitor at 115200 baud
```

**Unit Tests (native, no hardware):**
```bash
pio test -e native -v            # Run ~2316 C++ unit tests (87 modules)
pio test -e native -f test_mqtt  # Run single test module
```

**Web Assets:**
```bash
node tools/build_web_assets.js   # Compress web_src/ -> web_pages_gz.cpp
node tools/find_dups.js          # Check duplicate JS declarations
node tools/check_missing_fns.js  # Check undefined function references
cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json  # Lint JS
```

**E2E Tests:**
```bash
cd e2e && npm install            # First time only
npx playwright install --with-deps chromium  # First time only
npx playwright test              # Run 107 browser tests (22 specs)
npx playwright test --headed     # With visible browser
```

**Documentation:**
```bash
cd docs-site && npm install && npm run build && npm run serve
node tools/extract_tokens.js     # Sync design tokens to CSS
```

**CI Pipeline (`.github/workflows/tests.yml`):**
5 parallel quality gates, all must pass before firmware build:
1. `cpp-tests` — `pio test -e native -v` (Ubuntu, Python 3.11)
2. `cpp-lint` — cppcheck on `src/` (excludes `src/gui/`)
3. `js-lint` — find_dups + check_missing_fns + ESLint + diagram validation
4. `e2e-tests` — Playwright browser tests (Chromium, Node 20)
5. `doc-coverage` — `node tools/check_mapping_coverage.js`

**Release Pipeline (`.github/workflows/release.yml`):**
Manual workflow_dispatch with version bump (patch/minor/major) and channel (stable/beta). Runs same 5 gates, then builds firmware and creates GitHub Release with `firmware.bin`.

---

*Stack analysis: 2026-03-22*
