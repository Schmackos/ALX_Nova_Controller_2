# Technology Stack

**Analysis Date:** 2026-03-21

## Languages

**Primary:**
- C++ (C++11 and later) — Firmware core, all `src/` modules
- C — Hardware drivers, DSP component integration, HAL layer
- JavaScript/HTML/CSS — Web UI in `web_src/` (concatenated into single `<script>` block during build)

**Secondary:**
- Python — Build tools (`tools/fix_riscv_toolchain.py`), documentation generation (`tools/generate_docs.js` via Node.js), CI scripts

## Runtime

**Environment:**
- **Embedded:** ESP32-P4 (Waveshare ESP32-P4-WiFi6-DEV-Kit) with IDF5 (Espressif IoT Development Framework 5.x)
- **Development/Testing:** Native platform (host machine GCC/MinGW) for unit tests
- **Documentation:** Node.js 18+ for build tools and Docusaurus

**Package Manager:**
- **Firmware:** PlatformIO (platform-espressif32 55.03.37)
- **Web UI:** npm/Node.js (local tools)
- **Docs:** npm (Docusaurus v3)
- **E2E Tests:** npm (Playwright)

**Lockfiles:**
- `package-lock.json` — Present in `docs-site/`, `e2e/` directories

## Frameworks

**Core:**
- Arduino Framework — Abstraction layer for ESP32 peripherals
- FreeRTOS — Dual-core task scheduling (Core 0: WiFi/MQTT/OTA, Core 1: Audio pipeline exclusively)

**Web/GUI:**
- LVGL v9.4 — TFT display GUI framework (ST7735S 160×128, landscape mode), guarded by `-D GUI_ENABLED`
- LovyanGFX v1.2.0 — ST7735S display driver (replaced TFT_eSPI), synchronous SPI on ESP32-P4 (no DMA)
- Docusaurus v3 — Static documentation site (React-based, MDX support, Mermaid diagrams, local search, dark mode)

**Testing:**
- Unity Framework — C++ unit test runner on native platform (~1620 tests across 70 modules)
- Playwright v1.50.0 — Browser automation for E2E tests (26 tests across 19 specs, Express mock server)

**Real-Time Communication:**
- WebSockets — WebSocketsServer library (vendored in `lib/WebSockets/`, port 81) for real-time state broadcasts to web clients

**MQTT/Messaging:**
- PubSubClient v2.8 — MQTT 3.1.1 client for Home Assistant integration and device control

## Key Dependencies

**Critical (firmware):**
- ArduinoJson v7.4.2 — JSON parsing throughout (config, REST API, WebSocket messages)
- PubSubClient v2.8 — MQTT client (Home Assistant discovery, device commands)
- LovyanGFX v1.2.0 — TFT ST7735S driver (synchronous SPI, no DMA on P4)
- lvgl v9.4 — GUI framework (guarded by `-D GUI_ENABLED`)
- WebSockets (vendored) — WebSocket server on port 81

**Cryptography & Security:**
- mbedtls — Crypto library included with IDF5 for PBKDF2-SHA256 (10k iterations, password hashing), SHA256 (firmware verification), TLS (WiFiClientSecure)
- esp_task_wdt — Task watchdog timer (IDF5, configured for 30s timeout at boot)

**DSP & Audio:**
- **Pre-built ESP-DSP library** (`libespressif__esp-dsp.a`) — S3 assembly-optimized biquad IIR, FIR convolution, Radix-4 FFT, window functions, vector math (mulc/mul/add), dot product, SNR/SFDR (ESP32 only; native tests use `lib/esp_dsp_lite/` ANSI C fallback via `lib_ignore`)
- arduinoFFT v2.0 — FFT analysis (native tests only; ESP32 uses pre-built ESP-DSP)

**Platform/Hardware:**
- TinyUSB 0.20.1 — USB UAC2 speaker device on native USB OTG (guarded by `-D USB_AUDIO_ENABLED`)
- LittleFS — Filesystem for persistent JSON config (atomic write via tmp+rename pattern)
- Preferences (NVS) — Non-volatile storage for WiFi credentials, auth hash, device serial, MQTT settings

**Web/Documentation:**
- Express v4.21.0 — Mock HTTP server for E2E tests
- cookie-parser v1.4.7 — HTTP cookie parsing in E2E mock server
- ESLint v8.57.0 — JS linting on `web_src/js/` (380-file globals for concatenated scope)
- @docusaurus/core v3.7.0 — Documentation site core
- @docusaurus/theme-mermaid v3.7.0 — Mermaid diagram support in docs
- @easyops-cn/docusaurus-search-local v0.44.5 — Full-text search plugin

## Configuration

**Environment:**
- Firmware version: `1.12.1` (defined in `src/config.h` as `FIRMWARE_VERSION`)
- GitHub repo: `Schmackos/ALX_Nova_Controller_2` (OTA release checking)
- Board target: ESP32-P4 (`esp32-p4` in PlatformIO)
- Build flags: 58 compile-time constants (pins, DSP config, feature gates) in `platformio.ini`

**Key Compile-Time Settings:**
```
-D DSP_ENABLED                      # Enable 24-stage audio DSP pipeline
-D DAC_ENABLED                      # Enable DAC/HAL device framework
-D USB_AUDIO_ENABLED                # Enable USB UAC2 speaker device
-D GUI_ENABLED                      # Enable LVGL-based TFT GUI
-D WEBSOCKETS_MAX_DATA_SIZE=4096    # WebSocket frame size
```

**Pin Configuration (from `src/config.h`):**
- **Core:** LED=1, Amplifier=27, Reset Button=46, Buzzer=45
- **TFT Display:** MOSI=2, SCLK=3, CS=4, DC=5, RST=6, BL=26
- **Encoder:** A=32, B=33, SW=36
- **I2S Audio ADC:** BCK=20, DOUT=23, DOUT2=25, LRC=21, MCLK=22
- **I2S DAC TX:** GPIO 24
- **DAC I2C Bus 0:** SDA=48, SCL=54 (shared with WiFi SDIO — scan disabled when WiFi active)
- **Onboard ES8311 I2C Bus 1:** SDA=7, SCL=8 (dedicated, always safe)
- **Expansion I2C Bus 2:** SDA=28, SCL=29 (always safe)
- **Signal Generator PWM:** GPIO 47

**Build System:**
- `platformio.ini` — Main build config with 3 environments: `esp32-p4` (firmware), `native` (unit tests), `p4_hosted_update` (OTA)
- `src/idf_component.yml` — IDF component metadata
- `tools/fix_riscv_toolchain.py` — Pre-build script (custom RISC-V toolchain path fix for ESP32-P4)

**Web Build:**
- `web_src/` — Source files (HTML, CSS modules, JS modules in load order)
- `tools/build_web_assets.js` — Generates `src/web_pages.cpp` and `src/web_pages_gz.cpp` (gzip-compressed, embedded)
- `web_src/.eslintrc.json` — ESLint config with 380 globals (concatenated JS scope)
- Docusaurus: `docs-site/docusaurus.config.js`, `docs-site/sidebars.js`

## Platform Requirements

**Development:**
- PlatformIO CLI (Python-based, handles compiler/IDF installation)
- GCC/MinGW (native test compilation)
- Node.js 18+ (build tools, Docusaurus, Playwright)
- ESP32-P4 Dev Kit (Waveshare model) on USB COM8 (hardcoded in `platformio.ini`)

**Production/Device:**
- Waveshare ESP32-P4-WiFi6-DEV-Kit with 16 MB flash, PSRAM
- Partition table: `partitions_ota.csv` (OTA-capable: 2×8MB app partitions + spiffs)
- WiFi + Ethernet 100Mbps (via onboard PHY)
- I2S, I2C, GPIO, MCPWM, USB OTG, LEDC PWM peripherals

---

*Stack analysis: 2026-03-21*
