# Technology Stack

**Analysis Date:** 2026-03-23

## Languages

**Primary:**
- C++ 11 - Core firmware and HAL drivers (ESP32-P4 target)
- C - Low-level utilities and mocks

**Secondary:**
- JavaScript (ES5+) - Web UI frontend (`web_src/js/`)
- HTML - Web UI shell (`web_src/index.html`)
- CSS - Web UI styling (`web_src/css/`)

## Runtime

**Embedded System:**
- ESP32-P4 microcontroller (Xtensa dual-core with RISC-V coprocessor)
- Arduino framework on top of ESP-IDF 5.x
- FreeRTOS kernel (2 cores: Core 0 for UI/network, Core 1 for audio)

**Web Testing:**
- Node.js 18+ (for E2E tests and Docusaurus)

## Build & Deployment

**Build System:**
- PlatformIO 6.x
- Platform: Espressif32 v55.03.37 with Arduino framework
- LittleFS filesystem for configuration storage (16MB flash)

**Package Managers:**
- Arduino package manager (via PlatformIO) - C++ libraries
- npm - JavaScript dependencies (test runner, Docusaurus, web tools)

**Target Board:**
- Waveshare ESP32-P4-WiFi6-DEV-Kit (16MB flash, PSRAM, Ethernet 100Mbps)

## Frameworks & Core Libraries

**Embedded Audio/DSP:**
- pre-built `libespressif__esp-dsp.a` - S3 assembly-optimized FFT, biquad IIR, FIR, vector math, SNR/SFDR (ESP32 only)
- `arduinoFFT@^2.0` - FFT implementation (native unit tests only; `lib_ignore`d on ESP32)

**Web & Networking:**
- `WebSockets@v2.7.3` - WebSocket server (vendored in `lib/WebSockets/`, not from registry)
- `WebServer` - Built-in Arduino HTTP server (port 80)
- `WiFi` - WiFi client/AP mode via Arduino WiFi API
- `DNSServer` - Captive portal for AP mode
- `HTTPClient` - HTTPS client for OTA/GitHub API calls
- `ETH` - Ethernet interface (100Mbps, ESP32-P4 only)

**JSON & Data:**
- `ArduinoJson@^7.4.2` - JSON parsing/generation for config, API, MQTT, WebSocket

**Messaging & Integration:**
- `PubSubClient@^2.8` - MQTT 3.1.1 client for Home Assistant integration
- `TinyUSB 0.20.1` (native) - USB Audio Class 2 (UAC2) speaker device on native USB OTG

**Display & UI:**
- `LovyanGFX@^1.2.0` - TFT display driver for ST7735S 128x160 (replaces legacy TFT_eSPI)
- `lvgl@^9.4` - Light and Versatile Graphics Library (LVGL v9.4 on Core 0)

**Storage:**
- `LittleFS` - Flash filesystem for `/config.json`, `/hal_config.json`, HAL device database
- `Preferences` - NVS (Non-Volatile Storage) for WiFi credentials, OTA settings, HAL prefs

**Cryptography:**
- `mbedtls` - TLS/SSL library via IDF5 (certificate validation for HTTPS)
- `ESP32CertBundle` - Mozilla certificate bundle for automatic SSL validation

**Real-Time Operating System:**
- FreeRTOS v10 (ESP-IDF 5) - Task scheduling, semaphores, queues, event groups

## Web Frontend Technologies

**Testing:**
- `@playwright/test@^1.50.0` - Browser automation E2E tests
- `eslint@^8.57.0` - JavaScript linting

**Server (E2E Mock):**
- `express@^4.21.0` - Mock HTTP server for E2E tests
- `cookie-parser@^1.4.7` - Cookie middleware for E2E tests

**Documentation:**
- `@docusaurus/core@^3.7.0` - Static documentation site generator
- `@docusaurus/preset-classic@^3.7.0` - Documentation theme and presets
- `@docusaurus/theme-mermaid@^3.7.0` - Mermaid diagram support
- `@easyops-cn/docusaurus-search-local@^0.44.5` - Local search

## Configuration

**Build Configuration:**
- `platformio.ini` - PlatformIO project config with build flags for pin assignments, DSP/DAC/GUI features, memory settings
- `src/config.h` - C++ compile-time constants (firmware version, pin definitions, task sizes, DSP/HAL limits)
- `.githooks/pre-commit` - Git hooks for code quality checks

**Pin Assignments (in src/config.h and platformio.ini):**
- Core: LED_PIN=1, AMPLIFIER_PIN=27, RESET_BUTTON_PIN=46, BUZZER_PIN=45
- TFT Display: MOSI=2, SCLK=3, CS=4, DC=5, RST=6, BL=26
- Rotary Encoder: A=32, B=33, SW=36
- I2S Audio ADC1: BCK=20, DOUT=23, LRC=21, MCLK=22
- I2S Audio ADC2: DOUT2=25 (shares BCK/LRC/MCLK with ADC1)
- I2S DAC TX: DOUT=24
- DAC I2C: SDA=48, SCL=54
- Signal Generator PWM: GPIO=47
- ES8311 Onboard DAC I2C: SDA=7, SCL=8
- NS4150B Amp Enable: GPIO=53
- I2C Bus 2 (Expansion): SDA=28, SCL=29

**Feature Flags (Build):**
- `DSP_ENABLED` - Enable DSP pipeline (biquad, FIR, limiter, gain, delay)
- `DAC_ENABLED` - Enable DAC support and HAL device manager
- `USB_AUDIO_ENABLED` - Enable TinyUSB UAC2 speaker device on native USB
- `GUI_ENABLED` - Enable LVGL-based TFT GUI on Core 0

**Environment Files:**
- `.env` file patterns: WiFi credentials, MQTT broker config, OTA settings stored in LittleFS `/config.json` (not .env directly)

**Other Config Files:**
- `src/ota_certs.h` - Embedded SSL certificate chain (Sectigo R46/E46, DigiCert G2) for GitHub API HTTPS validation

## External Service Dependencies

**Version Control & Releases:**
- GitHub API (`api.github.com`) - OTA update checking and firmware download
- GitHub Releases - Binary firmware distribution
- GitHub Pages - Documentation site deployment (`gh-pages` branch)

**Home Automation:**
- MQTT broker (configurable, default empty) - Home Assistant integration via MQTT Discovery protocol

**Time Synchronization:**
- NTP (via WiFi) - System time for logging and timestamps

## Memory Architecture

**RAM:**
- Internal SRAM: 32KB DMA buffers (pre-allocated at boot), ~40KB heap reserve for WiFi RX
- PSRAM: 2MB (Waveshare board) - Preferred for large allocations (DSP delays, FIR, convolution, TDM buffers)

**Storage:**
- LittleFS: `/config.json` (primary settings), `/hal_config.json` (HAL device config), `/hal/custom/*.json` (custom devices), `/hal_db.json` (device database cache)
- NVS: WiFi credentials, OTA channel, HAL auto-discovery toggle (survives LittleFS format)

## Compiler & Optimization

**Compiler:**
- GCC/G++ (Xtensa and RISC-V toolchains from ESP-IDF)

**Optimization:**
- `-Os` - Size optimization for firmware
- `c++11` standard on native tests
- ESP32-specific: Optimized math via pre-built ESP-DSP library

## Security

**Transport:**
- TLS 1.2/1.3 via mbedtls for HTTPS (GitHub OTA, WebSocket WSS potential)
- HTTP only for local web UI (port 80, 81) — no encryption over LAN

**Credentials:**
- WiFi password: NVS encrypted storage
- MQTT broker credentials: LittleFS config (plaintext on device, encrypted in transit to MQTT)
- WebSocket auth: PBKDF2-SHA256 (50k iterations, `p2:` format, auto-migrated from legacy `p1:` 10k iterations)
- OTA: Binary signed with SHA256 verification (no code signing, binary integrity only)

**Certificate Management:**
- Mozilla Root CA bundle embedded in firmware (`ota_certs.h`)
- Automatic validation for GitHub API HTTPS calls
- Certificate update utility: `node tools/update_certs.js`

## Continuous Integration

**CI/CD Platform:**
- GitHub Actions
- Parallel quality gates (`.github/workflows/tests.yml`):
  1. C++ unit tests (`pio test -e native`)
  2. C++ static analysis (`cppcheck`)
  3. JavaScript linting (`eslint`, `find_dups.js`, `check_missing_fns.js`)
  4. E2E browser tests (`playwright test`)
- Documentation site build and deploy to GitHub Pages

---

*Stack analysis: 2026-03-23*
