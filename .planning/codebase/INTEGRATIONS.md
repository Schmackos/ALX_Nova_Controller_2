# External Integrations

**Analysis Date:** 2026-03-08

## APIs & External Services

**GitHub (OTA Updates):**
- Service: GitHub REST API v3 (`api.github.com`) + GitHub CDN (`objects.githubusercontent.com`)
- Purpose: Fetch latest release metadata, download firmware binary, retrieve release notes and release list
- SDK/Client: `<HTTPClient.h>` + `<WiFiClientSecure.h>` (TLS)
- Auth: None (public repo; repo owner/name in `src/config.h` as `GITHUB_REPO_OWNER` / `GITHUB_REPO_NAME`)
- TLS: Three hardcoded root CA certificates in `src/ota_updater.cpp` — Sectigo R46 (RSA), Sectigo E46 (ECC), DigiCert Global Root G2. Valid until 2038-2046
- API endpoints used: `GET https://api.github.com/repos/{owner}/{repo}/releases/latest`, `GET https://api.github.com/repos/{owner}/{repo}/releases`
- Firmware download: direct URL from release asset, SHA256 verified post-download via `<mbedtls/md.h>`

**NTP Time Synchronization:**
- Service: NTP pool (system default, via ESP-IDF SNTP)
- Purpose: Backfill crash log timestamps after boot
- SDK/Client: `<time.h>` (POSIX), ESP-IDF SNTP stack
- Called from: `src/ota_updater.cpp` `syncTimeWithNTP()`

## Data Storage

**Databases:**
- None external. All persistence is on-device:
  - LittleFS (spiffs partition, 7.875MB): `/config.json`, `/hal_config.json`, `/diag_journal.bin`, `/crashlog.bin`, DSP preset JSON files, IR WAV files for convolution
  - NVS (Preferences): WiFi credentials, auth password hash, OTA success flag, select runtime settings that survive LittleFS format
  - Storage layer: `src/settings_manager.cpp`, `src/hal/hal_settings.cpp`, `src/dsp_api.cpp`, `src/crash_log.cpp`, `src/diag_journal.cpp`

**File Storage:**
- Local LittleFS only. No cloud file storage.

**Caching:**
- None external. In-memory ring buffer (`DIAG_JOURNAL_HOT_ENTRIES=32`) for diagnostics, flushed to LittleFS every 60s

## Authentication & Identity

**Web UI Auth:**
- Custom session cookie system (`src/auth_handler.cpp`)
- Password hashed with PBKDF2-SHA256 (10,000 iterations, 16-byte random salt), stored as `"p1:<saltHex>:<keyHex>"` in NVS
- Legacy unsalted SHA256 hashes auto-migrate on next successful login
- First-boot random password (10 chars, ~57-bit entropy) displayed on TFT and serial
- Session TTL: 1 hour (`SESSION_TIMEOUT_US = 3600000000`)
- Max 5 concurrent sessions (`MAX_SESSIONS = 5`)
- Cookie: `HttpOnly` flag set
- Login rate limiting: non-blocking; failed attempts set `_nextLoginAllowedMs`; excess requests return HTTP 429 with `Retry-After` header
- WS auth: short-lived token from `GET /api/ws-token` (60s TTL, 16-slot pool, single-use). Client sends token in first WebSocket message

## Hardware Interfaces & Protocols

**WiFi (via ESP32-C6 co-processor):**
- Protocol: WiFi 6 (802.11ax) — SDIO interface between ESP32-P4 and ESP32-C6
- Multi-network client: up to 5 saved networks (`MAX_WIFI_NETWORKS=5`), stored in NVS
- AP mode: captive portal for initial config (DNSServer on port 53)
- Static IP support per network
- Power save: `WIFI_PS_NONE` enforced post-`WiFi.mode()` call
- SDIO conflict: GPIO 48/54 (I2C Bus 0) must not be accessed when WiFi is active
- Implementation: `src/wifi_manager.cpp`

**Ethernet (100Mbps, onboard EMAC):**
- Protocol: IEEE 802.3 100BASE-TX Full Duplex
- Purpose: Alternative/redundant network path; set as default route when link + IP present
- SDK/Client: ESP-IDF EMAC driver via `src/eth_manager.cpp`
- Checked alongside WiFi for OTA connectivity

**MQTT (Home Assistant integration):**
- Protocol: MQTT 3.1.1 over TCP (plaintext)
- Default port: 1883 (`DEFAULT_MQTT_PORT`)
- Client library: `PubSubClient@^2.8`
- Broker: user-configurable (host, port, user, password, base topic) via web UI → `/config.json`
- Topic structure: `<baseTopic>/audio/*`, `<baseTopic>/system/*`, `<baseTopic>/wifi/*`, etc.
- Home Assistant discovery: publishes HA MQTT discovery payloads on connect (`src/mqtt_ha_discovery.cpp`, ~1880 lines)
- Dedicated FreeRTOS task on Core 0, priority 2 (`src/mqtt_task.cpp`) — never blocks main loop
- Heartbeat: mandatory full state publish every 60s
- Reconnect: 1–3s blocking TCP connect inside MQTT task only
- Change detection: `prevMqtt*` file-local statics in `src/mqtt_publish.cpp` (not in AppState)
- USB audio topics (when `USB_AUDIO_ENABLED`): `audio/usb/connected`, `audio/usb/streaming`, `audio/usb/enabled`, `audio/usb/sampleRate`, `audio/usb/volume`, `audio/usb/overruns`, `audio/usb/underruns`

**I2S Audio (PCM1808 ADC + PCM5102A/ES8311 DAC):**
- Protocol: I2S (TDM-compatible)
- ADC1 (PCM1808 #1): I2S_NUM_0 master RX, BCK=GPIO20, LRC=GPIO21, MCLK=GPIO22, DATA=GPIO23
- ADC2 (PCM1808 #2): I2S_NUM_1 master RX (no clock output), DATA=GPIO25 only (shares BCK/LRC/MCLK with ADC1)
- DAC TX (PCM5102A): I2S_NUM_0 full-duplex TX, DATA=GPIO24
- ES8311 Codec: I2S_NUM_2 TX, DATA=GPIO9, LRC=GPIO10, BCK=GPIO12, MCLK=GPIO13
- Both ADCs run as master with identical D2CLK divider chains for frequency-locked BCK
- Sample rate: 48kHz default (`DEFAULT_AUDIO_SAMPLE_RATE`), 256-frame DMA buffers × 12 count
- Implementation: `src/i2s_audio.cpp`, `src/hal/hal_pcm1808.cpp`, `src/hal/hal_pcm5102a.cpp`, `src/hal/hal_es8311.cpp`

**I2C Buses:**
- Bus 0 (EXT): SDA=GPIO48, SCL=GPIO54 — external expansion; NEVER scan when WiFi active (SDIO conflict causes MCU reset)
- Bus 1 (ONBOARD): SDA=GPIO7, SCL=GPIO8 — ES8311 dedicated; always safe
- Bus 2 (EXPANSION): SDA=GPIO28, SCL=GPIO29 — always safe
- ES8311 I2C address: probed at boot via HAL discovery (`src/hal/hal_discovery.cpp`)
- EEPROM probe: AT24C02 compatible EEPROM on expansion bus for HAL device identification (EEPROM v3 format in `src/hal/hal_eeprom_v3.cpp`)
- MCP4725 DAC: I2C DAC on expansion bus (`src/hal/hal_mcp4725.cpp`)

**USB Audio (TinyUSB UAC2):**
- Protocol: USB 2.0 HS, USB Audio Class 2.0 speaker device (PID 0x4004)
- Physical: native USB OTG on ESP32-P4 GPIO 19/20
- Formats: PCM16 (primary), PCM24
- Requires build unflags: `-DARDUINO_USB_MODE -DARDUINO_USB_CDC_ON_BOOT`
- SPSC lock-free ring buffer: 1024 frames in PSRAM (`USB_AUDIO_RING_BUFFER_FRAMES`)
- Host volume/mute applied inline in source read callback
- FreeRTOS task on Core 0, priority 1 (`USB_AUDIO_TASK_CORE=0`)
- Implementation: `src/usb_audio.cpp`, `src/hal/hal_usb_audio.cpp`

**SPI Display (ST7735S TFT):**
- Protocol: SPI2/FSPI synchronous (no DMA on P4 — GDMA completion not wired in LovyanGFX for P4)
- Pins: MOSI=GPIO2, SCLK=GPIO3, CS=GPIO4, DC=GPIO5, RST=GPIO6, BL=GPIO26
- Resolution: 128×160, landscape orientation (160×128), 40MHz write frequency
- Driver: LovyanGFX, config in `src/gui/lgfx_config.h`
- GUI task: FreeRTOS on Core 0, priority 1 (`TASK_CORE_GUI=0`)

**Rotary Encoder (EC11):**
- Protocol: Gray code, ISR-driven
- Pins: A=GPIO32, B=GPIO33, SW=GPIO36
- Implementation: `src/gui/gui_input.cpp`, `src/hal/hal_encoder.cpp`

**GPIO-Controlled Hardware:**
- Amplifier relay: GPIO27 (`AMPLIFIER_PIN`) — controlled by `src/smart_sensing.cpp`
- Passive buzzer: GPIO45 (`BUZZER_PIN`) — LEDC PWM, implementation `src/buzzer_handler.cpp`
- NS4150B class-D amp enable: GPIO53 — shared with ES8311 PA pin, implementation `src/hal/hal_ns4150b.cpp`
- Signal generator PWM: GPIO47 (`SIGGEN_PWM_PIN`) — MCPWM group 0, 1MHz resolution
- Board LED: GPIO1 (`LED_PIN`) — stays LOW permanently (LED blink feature removed)
- Reset button: GPIO46 (`RESET_BUTTON_PIN`) — factory reset trigger (10s hold)

**Internal Sensors:**
- ESP32-P4 chip temperature: IDF5 `<driver/temperature_sensor.h>`, range -10 to +80°C, guarded by `CONFIG_IDF_TARGET_ESP32P4`, implementation `src/hal/hal_temp_sensor.cpp`

## Monitoring & Observability

**Error Tracking:**
- Diagnostic journal: ring buffer (32 hot entries in memory, up to 800 persistent on LittleFS at `/diag_journal.bin`), flushed every 60s (`DIAG_FLUSH_INTERVAL_MS`). Implementation: `src/diag_journal.cpp`, `src/diag_event.h`
- Crash log: last 10 boot entries (reset reason, heap stats, timestamp) at `/crashlog.bin`. Implementation: `src/crash_log.cpp`
- MQTT diagnostic event publish: `publishMqttDiagEvent()` in `src/mqtt_publish.cpp`

**Logs:**
- `debug_serial.h` macros (`LOG_D/I/W/E`) with `[ModuleName]` prefix
- Runtime log level control via `applyDebugSerialLevel()`
- WebSocket log forwarding: `broadcastLine()` sends `"module"` as separate JSON field for frontend filtering
- All `.log` files go to `logs/` directory

## CI/CD & Deployment

**Hosting:**
- On-device HTTP server (port 80) + WebSocket server (port 81)
- Web UI served as gzip-compressed single-file bundle from ESP32 flash (LittleFS)
- OTA updates: self-hosted via GitHub Releases (dual OTA partitions, each 4MB)

**CI Pipeline:**
- GitHub Actions (`.github/workflows/tests.yml` + `release.yml`)
- 4 parallel quality gates: `cpp-tests` (Unity, 1503+), `cpp-lint` (cppcheck), `js-lint` (ESLint + find_dups + check_missing_fns), `e2e-tests` (Playwright)
- Firmware build only proceeds when all 4 gates pass
- Playwright HTML report uploaded as artifact on failure (14-day retention)

## Webhooks & Callbacks

**Incoming:**
- HTTP REST API on port 80 (registered in `src/main.cpp`): `/api/settings`, `/api/wifi*`, `/api/mqtt*`, `/api/ota*`, `/api/hal/*`, `/api/dsp/*`, `/api/pipeline/*`, `/api/ws-token`, `/api/login`, `/api/logout`, `/api/auth-status`
- WebSocket on port 81: real-time commands from web UI (amplifier toggle, settings changes, signal gen control, DSP config)
- Manual OTA upload: `POST /api/firmware/upload` chunked multipart

**Outgoing:**
- MQTT publishes to broker (all state, HA discovery, heartbeat)
- GitHub API polling every 5 minutes (`OTA_CHECK_INTERVAL=300000ms`) for new releases
- WebSocket broadcasts to all authenticated connected clients (JSON + binary frames)

## Environment Configuration

**Required configuration (set via web UI, persisted in `/config.json` + NVS):**
- `wifiSSID` / `wifiPassword` — WiFi credentials (NVS)
- `mqttBroker`, `mqttPort`, `mqttUser`, `mqttPassword`, `mqttBaseTopic` — MQTT broker
- Web UI password (NVS, PBKDF2 hash)

**Build-time config:**
- All GPIO pin assignments via `platformio.ini` `-D` flags (fallback defaults in `src/config.h`)
- Feature flags: `DSP_ENABLED`, `DAC_ENABLED`, `GUI_ENABLED`, `USB_AUDIO_ENABLED`
- GitHub repo: `GITHUB_REPO_OWNER = "Schmackos"`, `GITHUB_REPO_NAME = "ALX_Nova_Controller_2"` in `src/config.h`

**Secrets location:**
- Web password hash stored in NVS (never in LittleFS or source)
- MQTT credentials stored in `/config.json` on LittleFS (device-local only)
- No secrets committed to repository

---

*Integration audit: 2026-03-08*
