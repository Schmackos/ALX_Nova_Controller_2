# External Integrations

**Analysis Date:** 2026-02-15

## APIs & External Services

**GitHub API:**
- Service: Firmware release checking and download
  - SDK/Client: HTTPClient + WiFiClientSecure (Arduino)
  - Endpoint: `https://api.github.com/repos/{GITHUB_REPO_OWNER}/{GITHUB_REPO_NAME}/releases/latest`
  - Auth: Public access (no token required)
  - Use case: OTA update checks, SHA256 verification, release notes retrieval
  - Implementation: `src/ota_updater.cpp` - `getLatestReleaseInfo()`, `performOTAUpdate()`, custom root CA certificates for api.github.com

**NTP Time Sync:**
- Service: Network Time Protocol for timestamp synchronization
  - Implementation: `src/ota_updater.cpp` - `syncTimeWithNTP()`
  - Used for: Firmware update validation, logging timestamps

## Data Storage

**Databases:**
- **Type:** None (non-SQL embedded device)
- **Persistence:** Hybrid approach with LittleFS and NVS

**File Storage:**
- **LittleFS** (Flash-based file system):
  - Mount point: `/` (root)
  - Used for: Settings, signal generator config, input names, DSP presets, crash logs
  - Key files:
    - `/settings.txt` - Main application settings (lines 1-25: theme, WiFi, MQTT, audio params, display timeout, debug flags)
    - `/siggen.txt` - Signal generator state (disabled by default)
    - `/inputnames.txt` - Per-ADC input channel names
    - `/dsp_preset_{0-3}.json` - Named DSP filter configuration presets
    - `/crashlog.bin` - Ring buffer of crash diagnostics (10 entries)
  - Implementation: `src/settings_manager.cpp`, `src/crash_log.cpp`

**NVS (Encrypted Key-Value Storage):**
- Namespace: "device" - Device serial number, firmware version tracking
- Namespace: "wifi_net" - Multi-network WiFi credentials (SSID, password, static IP config)
- Namespace: "mqtt" - MQTT broker settings (host, port, username, password, base topic)
- Namespace: "auth" - Web session tokens, hashed web password
- Implementation: `src/wifi_manager.cpp`, `src/mqtt_handler.cpp`, `src/auth_handler.cpp`, `src/settings_manager.cpp`

**Caching:**
- In-memory only (AppState singleton)
- No Redis/Memcached; state is broadcast to clients via WebSocket for real-time sync

## Authentication & Identity

**Auth Provider:**
- Custom implementation (local authentication)
  - Mechanism: Web password + session tokens
  - Implementation: `src/auth_handler.h` / `src/auth_handler.cpp`
  - Session storage: NVS namespace "auth"
  - Password hashing: mbedTLS MD5/SHA1 (configurable via `HASH_ALGORITHM`)
  - Token generation: 32-byte secure random (via `esp_random`)
  - Session timeout: 3600s (1 hour) defined in `src/auth_handler.h`
  - Max sessions: 5 concurrent sessions

**WebSocket Authentication:**
- Per-client session validation via session cookie or JSON token payload
- Timeout: 3600s (revalidated per message)
- Implementation: `src/websocket_handler.cpp` - `webSocketEvent()`

**SSL/TLS:**
- Certificate validation: Mozilla certificate bundle (ESP32CertBundle)
- Used for: HTTPS firmware downloads from GitHub, MQTT broker TLS (if enabled)
- Root CAs: Custom certificates embedded in `src/ota_updater.cpp` (USERTrust ECC/RSA, DigiCert Global Root G2)

## Monitoring & Observability

**Error Tracking:**
- Custom crash log implementation (not external service)
  - Ring buffer: 10 entries in `/crashlog.bin` (LittleFS)
  - Watchdog: ESP32-S3 hardware WDT, 15s timeout via `CONFIG_ESP_TASK_WDT_TIMEOUT_S`
  - Implementation: `src/crash_log.h` / `src/crash_log.cpp`
  - Broadcast: MQTT `device/crash_diagnostics` topic, WebSocket `hardware_stats`

**Logs:**
- Serial output: UART0 via CH343 USB-to-UART bridge (115200 baud)
- Log levels: LOG_NONE, LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG (configurable at runtime via `debugSerialLevel`)
- Implementation: `src/debug_serial.h` / `src/debug_serial.cpp`
- WebSocket forwarding: Debug logs sent via `debugLogs` WebSocket message type
- Modules use consistent `[ModuleName]` prefixes (e.g., `[WiFi]`, `[MQTT]`, `[Audio]`)

## CI/CD & Deployment

**Hosting:**
- Embedded on ESP32-S3 device itself
- REST API server: Port 80 (HTTP)
- WebSocket server: Port 81 (WebSockets)
- Access Point (AP) mode: Standalone WiFi network when not connected to a router
- Update method: OTA (Over-The-Air) via GitHub releases

**CI Pipeline:**
- GitHub Actions (`.github/workflows/tests.yml`)
- Triggers: Push/PR to `main` and `Dev` branches
- Steps: Native unit tests (Unity, 754+ tests), then ESP32-S3 firmware build
- Release workflow: `.github/workflows/release.yml` for automated releases

**Firmware Repository:**
- GitHub: `https://github.com/{GITHUB_REPO_OWNER}/{GITHUB_REPO_NAME}` (default: `Schmackos/ALX_Nova_Controller_2`)
- Repository owner/name defined in `src/config.h`:
  - `GITHUB_REPO_OWNER` = "Schmackos"
  - `GITHUB_REPO_NAME` = "ALX_Nova_Controller_2"

## Environment Configuration

**Required env vars:**
- None hardcoded in firmware (all config stored in NVS or LittleFS)
- WiFi SSID/password: Persisted in NVS `wifi_net` namespace
- MQTT credentials: Persisted in NVS `mqtt` namespace
- Web password: Persisted in NVS `auth` namespace (hashed)

**Secrets location:**
- NVS (encrypted key-value storage, hardware-protected)
- No `.env` files on device (not used in embedded context)
- Secrets NEVER transmitted in plain text (MQTT password over TLS only)

## Webhooks & Callbacks

**Incoming:**
- None (device is client-only, not a webhook receiver)
- Command ingress: REST API endpoints on port 80, MQTT subscription topics, WebSocket messages

**Outgoing:**
- MQTT publishes: Home Assistant discovery, state updates, crash diagnostics
- WebSocket broadcasts: Real-time state to connected web clients
- GitHub API calls: OTA update checks (non-blocking FreeRTOS task)
- NTP: Time sync for timestamps

## Home Assistant Integration

**MQTT Discovery:**
- Protocol: Home Assistant MQTT Discovery (MQTT v3.1.1)
- Topic pattern: `homeassistant/{device_class}/{device_id}/{object_id}/config`
- Entities: LED state, blinking, WiFi status, MQTT connection, OTA update available, firmware version, hardware stats, audio levels, crash diagnostics, DSP config, DAC state, USB audio status, display backlight, signal generator, debug mode
- Implementation: `src/mqtt_handler.cpp` - `publishHADiscovery()`
- Device info: Serial number (eFuse MAC-derived), manufacturer name, model name

**MQTT Topics:**
- Base topic: Configurable (default: `alx_audio_controller`)
- Device ID: Derived from device serial number
- Subscriptions:
  - `{base_topic}/led/set` - LED on/off
  - `{base_topic}/blinking/set` - Blink pattern
  - `{base_topic}/amplifier/set` - Relay control
  - `{base_topic}/timer/set` - Auto-off timer duration
  - `{base_topic}/threshold/set` - Audio detection threshold
  - And 30+ other control topics (audio graphs, DSP, DAC, WiFi, debug, etc.)
- Publications:
  - `{base_topic}/led` - LED state
  - `{base_topic}/audio/level` - Audio level in dBFS
  - `{base_topic}/crash_diagnostics` - Crash/health status
  - And 40+ sensor/diagnostics topics

**Keep-Alive:**
- MQTT heartbeat: 60s interval (configurable via `hardwareStatsInterval`)
- Implementation: `src/mqtt_handler.cpp` - `publishMqttSystemStatus()`, `publishMqttHardwareStats()`

## USB Audio Integration

**Protocol:**
- TinyUSB UAC2 (USB Audio Class 2) speaker device
- Format: PCM16 or PCM24, stereo, 48kHz (default)
- Descriptor: Registered via `usbd_app_driver_get_cb()` weak function callback
- Data path: USB → SPSC ring buffer → I2S ADC task → DSP pipeline → audio analysis

**USB Endpoints:**
- Control endpoint: Feature Unit (volume, mute)
- Isochronous OUT: Audio streaming from host
- Ring buffer: 1024 frames (20ms @ 48kHz), PSRAM-based, lock-free

**Format Conversion:**
- USB PCM16 stereo → left-justified int32 (bits [31:8])
- USB PCM24 stereo (3-byte packed) → left-justified int32
- Volume control: Host sends UAC2 volume (-32767 to 0 in 1/256 dB units), converted to linear gain (0.0-1.0)

**Implementation:**
- `src/usb_audio.h` / `src/usb_audio.cpp` - Ring buffer, format conversion, state machine
- Task: `usb_audio` on Core 0, priority 1, adaptive poll (100ms idle, 1ms streaming)
- Guarded by: `-D USB_AUDIO_ENABLED`

## DAC Output Integration

**DAC Drivers:**
- Plugin architecture: Abstract `DacDriver` class in `src/dac_hal.h`
- Supported DACs:
  - PCM5102A (Texas Instruments) - I2S-only, no I2C control, software volume via log curve
  - ES9038Q2M (ESS Technology) - I2S with I2C control
  - ES9842 (ESS Technology) - I2S with I2C control
- Auto-detection: Boot-time I2C scan, EEPROM device ID check, fallback to config

**I2S Output:**
- I2S_NUM_0 (full-duplex with ADC): GPIO 40 (TX data), shared BCK/WS/MCLK with ADC
- Format: 32-bit stereo interleaved, left-justified (bits [31:8])
- Sample rates: 48kHz (default), configurable per DAC

**I2C Control (EEPROM + DAC I2C):**
- SDA: GPIO 41, SCL: GPIO 42
- EEPROM: Stores device ID, programming sequence (page-aware writes 8B/5ms)
- DAC control: I2C address varies by DAC type (PCM5102A = 0x00 = no I2C)

**Software Volume:**
- Logarithmic perceptual curve (0-100% → dBFS)
- Applied via `dsps_mulc_f32` (SIMD on ESP32)
- Implementation: `src/dac_hal.cpp` - `dac_apply_software_volume()`

**Implementation:**
- `src/dac_hal.h` / `src/dac_hal.cpp` - HAL driver abstraction
- `src/dac_registry.cpp` - Driver registration and factory
- `src/dac_eeprom.cpp` - EEPROM serialization, I2C bus scan
- `src/dac_api.cpp` - REST API for DAC control, EEPROM programming
- Guarded by: `-D DAC_ENABLED`

## I2S Audio ADC Integration

**PCM1808 Codec (Dual Channel):**
- I2S0 (ADC1): GPIO 16 (BCK), GPIO 18 (WS), GPIO 3 (MCLK), GPIO 17 (DOUT)
- I2S1 (ADC2): GPIO 16 (BCK), GPIO 18 (WS), GPIO 3 (MCLK - shared), GPIO 9 (DOUT2)
- Both configured as I2S master RX (slave mode not viable due to ESP32-S3 I2S driver limitations)
- Format: 24-bit stereo, left-justified in 32-bit I2S frame (>> 8 to extract audio)
- Sample rate: 48kHz fixed

**Audio Processing:**
- RMS calculation: Per-ADC and combined
- VU metering: 300ms attack, 300ms decay (industry standard)
- Peak hold: 2s hold, 300ms decay after hold expires
- dBFS: -96 to 0 range, relative to 1.0 linear RMS
- Waveform: 256-point downsampling (uint8 quantized, -1→0, 0→128, +1→255)
- FFT: 1024-point Radix-4 (ESP-DSP on ESP32, arduinoFFT on native tests)
- Health diagnostics: NO_DATA, NOISE_ONLY, CLIPPING, I2S_ERROR detection

**Input Voltage (Vrms):**
- Conversion: `audio_rms_to_vrms(rms_linear, vref)` where vref = 3.3V (configurable)
- Range: 0.0-5.0V (clamped)
- Persisted: Line 5 in `/smartsensing.txt`

**Implementation:**
- `src/i2s_audio.h` / `src/i2s_audio.cpp` - I2S driver, RMS, VU, peak, FFT, diagnostics
- Task: `audio_capture_task` on Core 1, priority 3, 12288-byte stack
- DMA: 8×256 sample buffers (42ms runway)
- Diagnostic APIs: `src/smart_sensing.cpp` - Signal detection, threshold comparison

## Network APIs

**REST API (HTTP Port 80):**
- Endpoints: `/api/led`, `/api/blinking`, `/api/amplifier`, `/api/smartsensing`, `/api/mqtt`, `/api/ota`, `/api/settings`, `/api/dsp`, `/api/dac`, `/api/usb_audio`, etc.
- Auth: Session-based (cookie or bearer token in Authorization header)
- Response format: JSON (ArduinoJson)
- Implementation: Multiple handlers in `src/main.cpp` and `src/*_handler.cpp`

**WebSocket API (Port 81):**
- Message types: JSON object commands (`setLed`, `setBlinking`, etc.) and state broadcasts (`ledState`, `audioLevels`, `hardwareStats`)
- Binary frames: Waveform (258B), spectrum (70B) for bandwidth optimization
- Auth: Per-client session validation, revalidated each message
- Broadcast interval: configurable (default 1000ms for state, adaptive for audio)
- Implementation: `src/websocket_handler.cpp` - `webSocketEvent()`

---

*Integration audit: 2026-02-15*
