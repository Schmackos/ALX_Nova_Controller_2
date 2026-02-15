# Technology Stack

**Analysis Date:** 2026-02-15

## Languages

**Primary:**
- C++ - Embedded firmware for ESP32-S3, compiled with `-std=c++11` in native tests
- C - Core I2S drivers, DSP algorithms, audio processing low-level code

**Secondary:**
- JavaScript - Web UI frontend (embedded in firmware as gzip-compressed HTML/CSS/JS, served from ESP32)

## Runtime

**Environment:**
- Arduino Framework (esp32 variant) - `framework = arduino` in PlatformIO
- FreeRTOS - Dual-core task scheduler on ESP32-S3 (Core 0: main loop, Core 1: GUI/audio)
- esp_idf - ESP32-S3 hardware abstraction layer

**Package Manager:**
- PlatformIO v6.x - Build system, library management, test runner
- Lockfile: `lock` files managed by PlatformIO, dependencies in `platformio.ini`

## Frameworks

**Core:**
- Arduino Core for ESP32-S3 (Espressif fork) - GPIO, I2S, UART, WiFi stack, USB (TinyUSB)
- TinyUSB UAC2 - Native USB audio class device (speaker/UAC2), `ARDUINO_USB_MODE=0`

**Networking:**
- WiFi (Arduino/ESP-IDF) - Client and Access Point modes, multi-network support
- PubSubClient 2.8 - MQTT broker client for Home Assistant integration
- WebSockets 2.7.2 - Real-time bidirectional communication (port 81)
- HTTPClient (Arduino) - HTTPS downloads for OTA firmware updates
- WebServer (Arduino) - REST API server on port 80

**Audio/DSP:**
- I2S Driver (ESP-IDF) - Dual PCM1808 24-bit ADC input, DAC output
- ESP-DSP (pre-built libespressif__esp-dsp.a) - S3 assembly-optimized biquad IIR, FIR, Radix-4 FFT, window functions, vector math (mulc/mul/add), SNR/SFDR analysis
- arduinoFFT 2.0 - FFT implementation for native tests only (`lib_compat_mode = off`), unavailable on ESP32 (uses ESP-DSP instead)

**GUI:**
- LVGL 9.4 - Light & Versatile Graphics Library for 160x128 TFT display
- TFT_eSPI 2.5.43 - ST7735S display driver (SPI interface)
- Rotary Encoder - ISR-driven input on GPIO 5/6/7 (Gray code state machine)

**JSON & Data:**
- ArduinoJson 7.4.2 - JSON serialization/deserialization for REST APIs, WebSocket messages, MQTT payloads

**Cryptography & Security:**
- mbedTLS (ESP-IDF) - SHA256 firmware verification, MD5/SHA1 password hashing, TLS/SSL
- ESP32CertBundle (esp_tls) - Automatic Mozilla certificate bundle for HTTPS validation
- esp_random - Secure random number generation for session tokens

**Storage & Persistence:**
- LittleFS - Flash-based file system (includes settings, signal gen config, DSP presets, signal generator, input names)
- Preferences (NVS) - Key-value encrypted storage for WiFi credentials, MQTT settings, device serial, auth sessions

**Testing:**
- Unity - Unit test framework for native platform
- PlatformIO Test - Test runner and CI orchestration

## Key Dependencies

**Critical:**
- ArduinoJson@^7.4.2 - JSON throughout: REST endpoints, WebSocket state broadcasts, MQTT Home Assistant discovery
- WebSockets@^2.7.2 - Real-time UI updates (audio waveform/spectrum, state changes)
- PubSubClient@^2.8 - MQTT client for Home Assistant integration, heartbeat publishing
- TinyUSB (built-in with Arduino) - USB audio UAC2 speaker device

**Infrastructure:**
- lvgl@^9.4 - GUI framework (guarded by `-D GUI_ENABLED`)
- TFT_eSPI@^2.5.43 - TFT display driver for ST7735S
- arduinoFFT@^2.0 - FFT analysis (native tests; ESP32 uses pre-built ESP-DSP)

**Special Libraries (Pre-built ESP32 SDKs):**
- libespressif__esp-dsp.a - S3 assembly-optimized DSP (biquad, FIR, FFT)
- ESP-IDF I2S HAL - Dual I2S master RX configuration
- ArduinoTinyUSB - Custom UAC2 audio class via `usbd_app_driver_get_cb()` weak function

## Configuration

**Environment:**
- Build system: PlatformIO with ESP32-S3 target board `esp32-s3-devkitm-1` (closest match for Freenove ESP32-S3 WROOM FNK0085)
- Compiler: GCC (Espressif toolchain), C++11 standard
- Optimization: `-Os` (size optimization)
- Debug Level: `CORE_DEBUG_LEVEL=2` (INFO), colored output enabled

**Build Flags** (src: `platformio.ini`):
- Pin configuration: `LED_PIN=2`, `AMPLIFIER_PIN=4`, `BUZZER_PIN=8`, `RESET_BUTTON_PIN=15`
- I2S Audio: `I2S_BCK_PIN=16`, `I2S_DOUT_PIN=17`, `I2S_DOUT2_PIN=9`, `I2S_LRC_PIN=18`, `I2S_MCLK_PIN=3`
- DSP: `-D DSP_ENABLED` enables audio processing pipeline
- DAC: `-D DAC_ENABLED`, I2S TX on GPIO 40, I2C (SDA=41, SCL=42)
- USB Audio: `-D USB_AUDIO_ENABLED` enables TinyUSB UAC2 speaker
- GUI: `-D GUI_ENABLED` enables LVGL on TFT display
- USB Mode: `-D ARDUINO_USB_MODE=0` (TinyUSB, not CDC), `-DARDUINO_USB_CDC_ON_BOOT=0`
- Memory: `board_build.arduino.memory_type = qio_opi` (QIO Flash + OPI PSRAM for 8MB external PSRAM)
- Watchdog: `CONFIG_ESP_TASK_WDT_TIMEOUT_S=15`

**Build Defines for Tests** (native environment):
- `-D UNIT_TEST`, `-D NATIVE_TEST` for conditional test compilation
- `-D DSP_ENABLED` with reduced stage limits for native (24 stages, 10 PEQ bands, 256 FIR taps)
- `-D DAC_ENABLED`, `-D USB_AUDIO_ENABLED` for dual-platform testing

**Hardware:**
- Flash: 8MB QIO (3.3MB partition for app code at 77.6% utilization)
- RAM: 327,680 bytes internal SRAM (144KB used = 44%)
- PSRAM: 8MB OPI (used for LVGL draw buffers, QR image, DSP delay lines, DAC buffers)

**Partition Table:**
- Default ESP32-S3 partition scheme (LittleFS for app data)
- Optional: `huge_app.csv` for larger firmware (commented in `platformio.ini`)

## Platform Requirements

**Development:**
- PlatformIO CLI or IDE plugin
- CH343 USB-to-UART bridge driver (Freenove ESP32-S3 WROOM board)
- Python 3.x (PlatformIO requirement)
- C++ build tools (GCC via Espressif toolchain)

**Production:**
- ESP32-S3 microcontroller (specifically Freenove ESP32-S3 WROOM FNK0085)
- 8MB OPI PSRAM (critical for audio DSP delay lines)
- USB 2.0 Type-C connector (both serial via CH343 and native USB OTG)
- WiFi (802.11b/g/n, built-in to ESP32-S3)
- External hardware: PCM1808 I2S ADC (dual channel), optional DAC/amplifier, TFT display, rotary encoder, piezo buzzer

---

*Stack analysis: 2026-02-15*
