# Technology Stack

**Analysis Date:** 2026-03-22

## Languages

**Primary:**
- C++ (Arduino 3.x with ESP-IDF5) — Firmware for ESP32-P4 microcontroller (`src/`, `src/hal/`, `src/gui/`, `src/drivers/`)
- JavaScript (ES6) — Web UI (`web_src/js/`), E2E tests (`e2e/`), documentation site (`docs-site/`)

**Secondary:**
- Python — Build tooling and utilities (`tools/*.py`, `tools/*.js` orchestration)
- Bash — Shell scripts and CI/CD

## Runtime

**Environment:**
- **Firmware**: ESP32-P4 (Waveshare ESP32-P4-WiFi6-DEV-Kit) running Arduino/ESP-IDF5 bootloader
- **Web Frontend**: Node.js 18+ (development/deployment)

**Package Managers:**
- **Firmware**: PlatformIO (`.pio/`, `platformio.ini`)
- **Web UI**: npm (E2E tests and docs site)
  - E2E testing: `package.json` at `e2e/`
  - Documentation site: `package.json` at `docs-site/`

**Lockfiles:**
- `e2e/package-lock.json` — npm lock for E2E Playwright tests
- `docs-site/package-lock.json` — npm lock for Docusaurus site

## Frameworks

**Core Firmware:**
- Arduino Framework 3.x (C++ abstraction over ESP-IDF5)
- ESP-IDF5 (underlying peripheral drivers: I2S, I2C, WiFi, MQTT, USB)

**Firmware Components:**
- **WebServer** — `<WebServer.h>` (built-in Arduino library, port 80)
- **WebSocket** — `WebSocketsServer.h` (custom vendored library in `lib/WebSockets/`, port 81)
- **MQTT** — `PubSubClient` v2.8 (Home Assistant integration via `mqtt_handler.cpp`)
- **JSON** — `ArduinoJson` v7.4.2 (configuration parsing, API responses)
- **GUI** — LVGL v9.4 (ST7735S TFT display, runs on Core 0, guarded by `-D GUI_ENABLED`)
- **Display Driver** — LovyanGFX v1.2.0 (ST7735S 128x160 SPI driver, 40MHz sync)
- **USB Audio** — TinyUSB UAC2 speaker device (native USB OTG, guarded by `-D USB_AUDIO_ENABLED`)

**Web Frontend:**
- Playwright v1.50.0 (browser E2E testing, headless Chromium)
- Express.js v4.21.0 (mock API server for E2E tests)
- Docusaurus v3.7.0 (documentation site builder)

**Testing:**
- **Unity** — Unit test framework (native platform, C++)
- **Playwright** — Browser/E2E testing (113 tests, 22 specs)

**Build/Dev:**
- **PlatformIO** — Firmware compilation, upload, serial monitoring
- **ESLint** — JavaScript linting (E2E tests and web UI)
- **Node.js tools** — Custom tooling: `tools/build_web_assets.js`, `tools/update_certs.js`, `tools/extract_tokens.js`

## Key Dependencies

**Firmware (platformio.ini):**

| Package | Version | Purpose | Location |
|---------|---------|---------|----------|
| ArduinoJson | ^7.4.2 | JSON parsing/serialization | Settings, API, MQTT payloads |
| PubSubClient | ^2.8 | MQTT client (Home Assistant) | `mqtt_handler.cpp`, `mqtt_task.cpp` |
| LovyanGFX | ^1.2.0 | TFT SPI display driver | `src/gui/`, ST7735S 128x160 |
| lvgl | ^9.4 | UI framework (LVGL9) | `src/gui/screens/` |
| WebSockets | vendored | Real-time client updates | `lib/WebSockets/`, port 81 |
| ESP-DSP pre-built | N/A | S3 assembly-optimized FFT, biquad IIR, FIR | `libespressif__esp-dsp.a` (not pkg-installed) |
| arduinoFFT | ^2.0 | FFT for native tests only | `test/` (native platform, not ESP32) |

**Development Dependencies (E2E):**

| Package | Version | Purpose |
|---------|---------|---------|
| @playwright/test | ^1.50.0 | Browser testing |
| eslint | ^8.57.0 | JavaScript linting |
| express | ^4.21.0 | Mock server (tests only) |
| cookie-parser | ^1.4.7 | Session cookie handling |

**Documentation Site (Docusaurus):**

| Package | Version | Purpose |
|---------|---------|---------|
| @docusaurus/core | ^3.7.0 | Site framework |
| @docusaurus/theme-mermaid | ^3.7.0 | Diagram rendering |
| @easyops-cn/docusaurus-search-local | ^0.44.5 | Offline search |
| react | ^18.0.0 | UI components |
| prism-react-renderer | ^2.0.0 | Code syntax highlighting |

## Configuration

**Environment:**
- Persistent storage: LittleFS (`board_build.filesystem = littlefs` in `platformio.ini`)
- OTA partition scheme: `partitions_ota.csv` (16MB flash, supports OTA updates)
- NVS namespace: `"device"` via `Preferences` (serial number, firmware version tracking)

**Build Flags (platformio.ini):**

| Flag | Value | Purpose |
|------|-------|---------|
| `-D DSP_ENABLED` | enabled | Digital signal processing pipeline |
| `-D DAC_ENABLED` | enabled | DAC hardware abstraction layer (HAL) |
| `-D USB_AUDIO_ENABLED` | enabled | TinyUSB UAC2 speaker input |
| `-D GUI_ENABLED` | enabled | LVGL TFT GUI on Core 0 |
| `-D DAC_I2C_SDA_PIN=48` | 48 | I2C expansion DAC/ADC data line |
| `-D DAC_I2C_SCL_PIN=54` | 54 | I2C expansion DAC/ADC clock line |
| `-D I2S_TX_DATA_PIN=24` | 24 | DAC I2S TX output |
| `-D WEBSOCKETS_MAX_DATA_SIZE=4096` | 4096 | WebSocket frame max size |
| `-D WEBSOCKETS_SERVER_CLIENT_MAX=16` | 16 | Max concurrent WebSocket clients |

**Pin Assignments (config.h):**
- LED: GPIO 1
- Amplifier Relay: GPIO 27
- Reset Button: GPIO 46
- Buzzer: GPIO 45
- I2S ADC1 (PCM1808): GPIO 20 (BCK), 21 (WS), 22 (MCLK), 23 (DATA)
- I2S ADC2 (PCM1808): GPIO 25 (DATA, shared clocks)
- I2S DAC TX: GPIO 24
- DAC I2C Bus: GPIO 48 (SDA), 54 (SCL)
- TFT Display: GPIO 2 (MOSI), 3 (SCLK), 4 (CS), 5 (DC), 6 (RST), 26 (BL)
- Encoder: GPIO 32 (A), 33 (B), 36 (SW)
- Signal Gen PWM: GPIO 47
- ES8311 Onboard DAC I2C: GPIO 7 (SDA), 8 (SCL)

## Platform Requirements

**Development:**
- Windows 10/11 or macOS with ARM support
- Python 3.7+ (PlatformIO CLI, build scripts)
- Visual Studio Code with PlatformIO extension (recommended)
- Node.js 18+ (documentation, E2E tests)
- Git

**Production (Hardware):**
- **Microcontroller**: Waveshare ESP32-P4-WiFi6-DEV-Kit (400MHz single-core RISC-V, 16MB flash, 4MB+ PSRAM)
- **Network**: WiFi6 (802.11ax via ESP32-C6) or Ethernet 100Mbps (onboard module)
- **Storage**: 16MB SPI flash (8MB firmware OTA, 8MB filesystem)
- **Memory**: 4MB SRAM + 8MB PSRAM
- **Audio I/O**:
  - PCM1808 dual-channel I2S ADC (onboard, 16-bit, 48kHz)
  - ES8311 codec DAC (onboard, I2C control)
  - Optional: Expansion mezzanine DAC/ADC modules via I2C + I2S TDM (21 supported drivers)
  - USB Audio: TinyUSB UAC2 speaker input via native USB OTG
- **Display**: ST7735S TFT 128x160 (SPI, 40MHz)
- **GPIO**: 54 assignable GPIO pins (configurable pin allocation via HAL)

## Deployment

**Firmware Distribution:**
- GitHub Releases (Schmackos/ALX_Nova_Controller_2)
- OTA update checks via GitHub API: `https://api.github.com/repos/Schmackos/ALX_Nova_Controller_2/releases/latest`
- SHA256 firmware verification (hardcoded hash in release notes)
- Root CA certificates in `src/ota_certs.h` (Sectigo R46/E46, DigiCert G2) — updated via `tools/update_certs.js`

**Web Pages:**
- HTML/CSS/JS embedded in firmware (gzip-compressed in `src/web_pages_gz.cpp`)
- Generated from `web_src/` via `tools/build_web_assets.js` (run before each build)
- Hosted on port 80 (HTTP), port 81 (WebSocket)

**Documentation:**
- Docusaurus v3 site deployed to GitHub Pages (`docs-site/`)
- Build: `npm run build` in `docs-site/`
- Design tokens pipeline: `src/design_tokens.h` → `tools/extract_tokens.js` → CSS for TFT + web UI + docs

## External Resources (CDN)

**Loaded from Web:**
- QR code library: `https://cdn.jsdelivr.net/npm/qrcodejs@1.0.0/qrcode.min.js` (optional, loaded on-demand)
- Markdown parser: `https://cdn.jsdelivr.net/npm/marked/marked.min.js` (optional, loaded on-demand)
- GitHub raw: `https://raw.githubusercontent.com/Schmackos/ALX_Nova_Controller_2/main/USER_MANUAL.md` (optional, loaded on-demand)

---

*Stack analysis: 2026-03-22*
