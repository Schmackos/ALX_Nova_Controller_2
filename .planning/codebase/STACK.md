# Technology Stack

**Analysis Date:** 2026-03-21

## Languages

**Primary:**
- C++ (C++11 minimum) — Firmware codebase (`src/`)
- JavaScript (ES6+) — Web UI (`web_src/`)
- HTML5 / CSS3 — Web pages and styling

**Secondary:**
- Python — Build tools and CI scripts (`tools/`)
- Markdown — Documentation (`docs-site/`, `docs-internal/`)

## Runtime

**Environment:**
- ESP32-P4 (Xtensa dual-core, 240 MHz, 8 MB RAM)
- Arduino framework v3.x (ESP32 3.x core via pioarduino platform-espressif32 v55.03.37)
- IDF5 (Espressif IoT Development Framework) — integrated via Arduino-as-HAL
- Node.js 18+ — Development, E2E tests, documentation build

**Package Manager:**
- PlatformIO (pio) — C++ firmware dependencies and build orchestration
- npm — JavaScript dependencies (web tests, documentation)

**Lockfiles:**
- `platformio.ini` — Present (library versions pinned, no separate lock file)
- `package-lock.json` — Present in `e2e/` and `docs-site/`

## Frameworks

**Core Firmware:**
- Arduino Core for ESP32 (v3.x) — GPIO, I2C, I2S, WiFi, UART APIs via abstracted HAL
- FreeRTOS (integrated in IDF5) — Task scheduling, core affinity, synchronization primitives (semaphores, mutexes, event groups)

**Audio Processing:**
- ESP-DSP (pre-built `libespressif__esp-dsp.a`) — S3 assembly-optimized biquad IIR, FIR convolution, Radix-4 FFT, window functions, vector math (used in production)
- arduinoFFT 2.0 — FFT spectrum analysis (native tests only, not on ESP32)

**GUI:**
- LVGL (LittleVGL) v9.4 — Embedded graphics library for ST7735S TFT display
- LovyanGFX v1.2.0 — Display driver abstraction for ST7735S (replaces TFT_eSPI)

**Web Server:**
- Arduino WebServer (built-in to Arduino core) — HTTP server on port 80
- WebSocketsServer (vendored in `lib/WebSockets/`) — WebSocket server on port 81 for real-time state updates

**Testing:**
- Unity (built-in to PlatformIO) — C++ unit test framework for native platform
- Playwright v1.50.0 — Browser E2E tests (Chromium)

**Build/Dev:**
- PlatformIO CLI — Firmware build, upload, test orchestration
- Node.js build scripts — Web asset compression, doc generation (`tools/`)

## Key Dependencies

**Critical (directly included in firmware):**
- ArduinoJson 7.4.2 — JSON parsing/serialization for REST API and configuration
- PubSubClient 2.8 — MQTT client (Home Assistant integration)
- LovyanGFX 1.2.0 — ST7735S display driver (full-duplex SPI)
- lvgl 9.4 — LVGL GUI framework (Core 0 task on P4)
- WebSockets (lib/WebSockets/, vendored) — WebSocket server for port 81 broadcasts

**Optional (conditionally compiled):**
- arduinoFFT 2.0 — FFT (native tests only; production uses ESP-DSP)
- Update — OTA firmware update support (Arduino built-in)
- WiFiClientSecure, HTTPClient — HTTPS/MQTT over TLS
- Preferences — NVS (Non-Volatile Storage) for credentials and serial number
- LittleFS — FAT alternative filesystem (16 MB SPI flash, OTA partitions)

**Infrastructure Libraries (ESP32 core):**
- mbedtls — TLS/SSL via ESP32CertBundle (automatic Mozilla CA validation)
- TinyUSB 0.20.1 — USB Audio UAC2 speaker device (native OTG)
- esp-idf — WiFi stack, Ethernet MAC driver, I2S/I2C/SPI drivers, task watchdog timer

**Development (npm):**
- Docusaurus v3.7.0 — Documentation site generation
- ESLint 8.57.0 — JavaScript linting for web UI
- Express 4.21.0 — Mock HTTP server for E2E tests
- cookie-parser 1.4.7 — Cookie middleware for test server

## Configuration

**Environment:**
- No `.env` file used; configuration persisted to:
  - `/config.json` (LittleFS, primary, atomic write via temp+rename)
  - `/mqtt_config.txt` (LittleFS, legacy format, read-only after first boot)
  - `/settings.txt` (legacy, fallback only)
  - `/hal_config.json` (HAL device overrides and custom configs)
  - NVS (via Preferences API) for device serial, auth tokens, crash logs

**Build Flags (platformio.ini):**
- `DSP_ENABLED` — Enables DSP pipeline and DSP API
- `DAC_ENABLED` — Enables audio output, DAC drivers, and HAL
- `GUI_ENABLED` — Enables LVGL TFT GUI (Core 0)
- `USB_AUDIO_ENABLED` — Enables TinyUSB UAC2 speaker device
- Standard compile: `-Os` (size optimization), `-D CORE_DEBUG_LEVEL=2` (info+ serial logs)

**Hardware-Specific Configuration:**
- Pin definitions passed as build flags (e.g., `LED_PIN=1`, `I2S_BCK_PIN=20`). Fallback defaults in `src/config.h`.
- I2S Audio: BCK=20, LRC=21, MCLK=22, DOUT=23 (ADC1), DOUT2=25 (ADC2)
- I2C Bus 0 (GPIO 48/54): Shared with WiFi SDIO — disabled when WiFi active
- I2C Bus 1 (GPIO 7/8): ES8311 onboard DAC (dedicated, always safe)
- I2C Bus 2 (GPIO 28/29): Expansion mezzanine connectors (always safe)

**Partition Table:**
- `partitions_ota.csv` — OTA-capable layout: bootloader + factory app + OTA app1 + OTA app2 + LittleFS (spiffs) for configs + NVS

## Platform Requirements

**Development:**
- Windows 11 / macOS / Linux
- Visual Studio Code (recommended with PlatformIO extension)
- PlatformIO CLI (`pio`)
- Python 3.6+ (PlatformIO requirement)
- Node.js 18+ (for E2E tests and docs)
- OpenSSL (HTTPS/SSL utilities)

**Hardware (Target):**
- Waveshare ESP32-P4-WiFi6-DEV-Kit (single official board target)
- Flash: 16 MB SPI NOR (supports PSRAM overflow)
- PSRAM: 8 MB (SPIRAM expansion)
- RAM: 8 MB internal SRAM
- USB: native USB OTG (not via CH340, no external serial adapter)
- Ethernet: 100 Mbps full-duplex via EMAC + RTL8201F PHY
- WiFi: ESP32-C6 co-processor (via SDIO to P4 host)

**Production:**
- Serial upload via USB or UART on COM8 (configurable in platformio.ini)
- OTA updates via HTTPS from GitHub Releases (firmware.bin + checksum validation)
- Web UI served from embedded HTML + CSS + JS (gzip-compressed, ~350 KB uncompressed)
- Configuration persisted to LittleFS — survives factory reset (WiFi creds in NVS)

## Build & Test Pipeline

**Firmware Build:**
```bash
pio run                          # Build for esp32-p4
pio run --target upload          # Upload to device
pio test -e native -v            # Run 1866 C++ unit tests
pio device monitor               # Serial monitor at 115200 baud
```

**Web Assets:**
```bash
node tools/build_web_assets.js   # Compress web_src/ → web_pages_gz.cpp
npx eslint web_src/js/           # Lint JavaScript
```

**E2E Tests:**
```bash
cd e2e && npm install
npx playwright test              # Run 44 browser tests against mock server
npx playwright test --headed     # With visible browser
```

**Documentation:**
```bash
cd docs-site && npm install && npm run build && npm run serve
```

---

*Stack analysis: 2026-03-21*
