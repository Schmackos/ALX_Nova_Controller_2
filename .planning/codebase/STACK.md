# Technology Stack

**Analysis Date:** 2026-03-21

## Languages

**Primary:**
- C++ (C++11 standard) - Firmware implementation for ESP32-P4
- C - HAL driver implementations, DSP coefficient generation, audio processing

**Secondary:**
- JavaScript (ES6+) - Web frontend UI in `web_src/js/` (concatenated into single scope)
- HTML - Web UI served from `web_src/index.html` (embedded and gzip-compressed in firmware)
- CSS - Responsive design split across multiple files in `web_src/css/`
- Markdown - Documentation (public in `docs-site/`, internal in `docs-internal/`)

## Runtime

**Environment:**
- ESP32-P4 (Waveshare ESP32-P4-WiFi6-DEV-Kit) - Target hardware
- Arduino framework (esp-idf based via pioarduino/platform-espressif32)
- IDF5 compatible (ESP-IDF 5.x)
- FreeRTOS with dual-core architecture (Core 0: WiFi/MQTT/USB; Core 1: audio pipeline)
- TinyUSB 0.20.1 - USB audio speaker device (UAC2, native USB OTG)

**Build Tools:**
- PlatformIO - Project/build management
- Platform: `https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip` (version 55.03.37)
- LittleFS - Filesystem for configuration persistence
- OTA partition scheme (`partitions_ota.csv`)

**Package Manager:**
- PlatformIO lib_deps registry
- npm (for web UI testing and documentation)

## Frameworks

**Core Firmware:**
- Arduino (ESP32 Arduino core v3.x, IDF5-compatible)
- FreeRTOS (dual-core task scheduling)

**Testing:**
- Unity - C++ unit test framework (1732 tests across 75 modules, native platform)
- Playwright - Browser E2E tests for web UI (26 tests across 19 specs)
- Express.js - Mock HTTP/WebSocket server for E2E test environment

**Build/Dev:**
- PlatformIO with custom pre-scripts (`tools/fix_riscv_toolchain.py`)
- Node.js (v18+) - Web asset building, test runners, documentation generation

**UI/Graphics:**
- LVGL v9.4 - GUI framework for ST7735S TFT display (128x160 landscape, guarded by `-D GUI_ENABLED`)
- LovyanGFX v1.2.0 - Synchronous SPI display driver for ST7735S (replaces TFT_eSPI)

**Documentation:**
- Docusaurus v3.7.0 - Static documentation site (26 pages: 9 user + 17 developer)
- Mermaid - Diagrams (10 architecture diagrams in `docs-internal/architecture/`)

## Key Dependencies

**Critical Firmware Libraries:**
- `ArduinoJson@^7.4.2` - JSON parsing/generation (used throughout for settings, API, WebSocket)
- `PubSubClient@^2.8` - MQTT client (Home Assistant integration)
- `LovyanGFX@^1.2.0` - TFT display driver for ST7735S (no DMA on P4, synchronous SPI)
- `lvgl@^9.4` - LVGL GUI framework (guarded by `GUI_ENABLED`)
- `WebSockets` - WebSocket server (vendored in `lib/WebSockets/`, not from registry)

**ESP32 Built-in:**
- `<driver/i2s_std.h>` - IDF5 I2S driver (replaces legacy `<driver/i2s.h>`)
- `<driver/temperature_sensor.h>` - Internal chip temperature sensor (ESP32-P4 only)
- `<esp_wifi.h>`, `<WiFi.h>` - WiFi connectivity
- `<ETH.h>` - Ethernet connectivity (100Mbps, ESP32-P4 only)
- `<Update.h>` - OTA firmware update interface
- `<Preferences.h>` - Non-volatile storage (NVS)
- `<LittleFS.h>` - LittleFS filesystem
- `<mbedtls/md.h>`, `<mbedtls/pkcs5.h>` - PBKDF2-SHA256 password hashing (10k iterations)
- `<esp_task_wdt.h>` - Task watchdog timer (configured for 30s timeout)
- `<DNSServer.h>` - DNS captive portal for AP mode
- `<HTTPClient.h>`, `<WebServer.h>` - HTTP/HTTPS clients and server

**ESP-DSP (Pre-built Assembly-Optimized):**
- `libespressif__esp-dsp.a` - Pre-compiled S3 assembly-optimized FFT, biquad IIR, FIR, window functions, vector math (included in platform)
- `lib/esp_dsp_lite/` - ANSI C fallback for native tests only (ignored in ESP32 builds via `lib_ignore`)

**Testing Libraries:**
- `arduinoFFT@^2.0` - FFT library for native tests only (not used on ESP32; uses pre-built ESP-DSP)
- `@playwright/test@^1.50.0` - E2E test framework
- `eslint@^8.57.0` - JavaScript linting
- `express@^4.21.0` - Mock HTTP server for tests
- `cookie-parser@^1.4.7` - Cookie handling in mock server

**Documentation Dependencies:**
- `@docusaurus/core@^3.7.0`, `@docusaurus/preset-classic@^3.7.0` - Docusaurus framework
- `@docusaurus/theme-mermaid@^3.7.0` - Mermaid diagram support
- `@easyops-cn/docusaurus-search-local@^0.44.5` - Local full-text search
- `remark-math@^5.1.1`, `rehype-katex@^6.0.3` - Math rendering support

## Configuration

**Firmware Build Configuration:**
- `platformio.ini` - PlatformIO project manifest with 3 environments (esp32-p4, p4_hosted_update, native)
- `src/config.h` - Compile-time constants: firmware version (1.12.1), pin assignments, DSP/DAC/GUI features, thresholds
- Build flags: `-D DSP_ENABLED -D DAC_ENABLED -D USB_AUDIO_ENABLED -D GUI_ENABLED` enable subsystems
- Custom pre-script: `tools/fix_riscv_toolchain.py` patches toolchain compatibility

**Pin Configuration (Build Flags):**
```
Core: LED=1, Reset Button=46, Amplifier Relay=27, Buzzer=45
I2S Audio ADC: BCK=20, DOUT=23, DOUT2=25, LRC=21, MCLK=22
I2S DAC TX: DOUT=24
I2C DAC: SDA=48, SCL=54
Signal Gen PWM: GPIO=47
TFT Display: MOSI=2, SCLK=3, CS=4, DC=5, RST=6, BL=26
Encoder: A=32, B=33, SW=36
ES8311 Codec: SDA=7, SCL=8, PA=53
I2C Expansion Bus: SDA=28, SCL=29
```

**Runtime Configuration:**
- `/config.json` - Primary settings file (atomic write via tmp+rename)
- `/mqtt_config.txt` - Legacy MQTT settings (fallback only)
- `/hal_config.json` - HAL device per-device I2C/I2S/GPIO pin overrides
- `/diag_journal.bin` - Persistent diagnostic event journal (800 entry ring, LittleFS, 64KB)
- `/crash_log.txt` - Last fatal crash stack trace (persisted across reset)
- NVS (Preferences) for WiFi credentials survival across LittleFS format (3 namespaces: wifi-list, ota-prefs, hal-prefs)

**Server Configuration:**
- HTTP: port 80
- WebSocket: port 81
- UART: 115200 baud (serial monitoring)

**Debug Configuration:**
- `CORE_DEBUG_LEVEL=2` (info level logging, configurable at runtime via `applyDebugSerialLevel()`)
- `-D CONFIG_ARDUHAL_LOG_COLORS=1` - Color-coded serial output
- Build flags include optimization: `-Os` (size-optimized)

## Platform Requirements

**Development:**
- PlatformIO CLI or IDE
- Python 3.x (for build scripts)
- Node.js v18+ (for web testing and doc generation)
- C++ toolchain (included in platform via pioarduino)
- Git

**Deployment Hardware:**
- Waveshare ESP32-P4-WiFi6-DEV-Kit
- 16MB Flash (16 MiB, `board_upload.maximum_size`)
- PSRAM (supports SPIRAM allocation)
- ST7735S TFT display (128x160, SPI interface)
- Optional: Ethernet module (100Mbps, ESP32-P4 native support)
- Optional: Expansion I2C devices (DACs, ADCs, sensors via Bus 0/1/2)

**Production:**
- WiFi network (2.4/5 GHz via ESP32-C6 co-processor, WiFi 6)
- Optional: Ethernet connection (falls back to WiFi if disconnected)
- Optional: MQTT broker (Home Assistant or compatible)
- Optional: GitHub account for OTA firmware updates
- USB Type-C (power + native audio UAC2 device via TinyUSB)

---

*Stack analysis: 2026-03-21*
