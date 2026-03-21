# External Integrations

**Analysis Date:** 2026-03-10

## APIs & External Services

**GitHub API (OTA Updates):**
- Purpose: Check for firmware updates, download firmware binaries, fetch release notes/list
- SDK/Client: `HTTPClient` + `WiFiClientSecure` (ESP-IDF built-in)
- Auth: None required (public repo API, unauthenticated)
- Endpoints used:
  - `GET https://api.github.com/repos/{owner}/{repo}/releases/latest` - Version check
  - `GET https://api.github.com/repos/{owner}/{repo}/releases` - Release list
  - `GET https://objects.githubusercontent.com/...` - Firmware binary download
- TLS: Root CA certificates embedded in `src/ota_updater.cpp` (Sectigo R46/E46 + DigiCert G2, valid 2038-2046)
- Config: Repo owner/name defined in `src/config.h` as `GITHUB_REPO_OWNER` ("Schmackos") and `GITHUB_REPO_NAME` ("ALX_Nova_Controller_2")
- Implementation: `src/ota_updater.h/.cpp`
- SHA256 verification on downloaded firmware (`calculateSHA256()`, `mbedtls/md.h`)

**NTP Time Sync:**
- Purpose: Set system clock for timestamps (crash logs, diagnostic journal, OTA)
- Client: ESP-IDF `configTime()` / `getLocalTime()`
- Servers: Standard NTP pool (system default)
- Called from: `syncTimeWithNTP()` in `src/ota_updater.cpp`

**Anthropic Claude API (CI/CD only):**
- Purpose: Automated documentation generation from source code
- SDK: `@anthropic-ai/sdk` (npm, installed at CI time)
- Auth: `ANTHROPIC_API_KEY` GitHub Actions secret
- Implementation: `tools/generate_docs.js`
- Scope: Only runs in `.github/workflows/docs.yml`, never on device

## MQTT Integration (Home Assistant)

**MQTT Broker:**
- Protocol: MQTT v3.1.1 via `PubSubClient@^2.8`
- Client: `WiFiClient mqttWifiClient` + `PubSubClient mqttClient` (instantiated in `src/main.cpp`)
- Connection: User-configurable broker address, port, username, password
- Config persistence: `/config.json` (primary) or legacy `/mqtt_config.txt`
- Default port: 1883 (defined in `src/config.h`)
- Base topic: User-configurable (default derived from device ID)
- Implementation: `src/mqtt_handler.h/.cpp` (core ~1120 lines), `src/mqtt_publish.cpp` (publish functions), `src/mqtt_ha_discovery.cpp` (HA discovery ~1880 lines)
- Task: Dedicated FreeRTOS task on Core 0, priority 2 (`src/mqtt_task.h/.cpp`), polls at 20 Hz

**Home Assistant Discovery:**
- Full MQTT auto-discovery support with device registration
- Publishes discovery messages for all device entities (sensors, switches, controls)
- Subscribe/command topics for bidirectional control
- Removable discovery via `removeHADiscovery()`
- Device ID: `getMqttDeviceId()` based on ESP32 MAC address

**MQTT Topics Published:**
- `state` - Amplifier/FSM state
- `smart_sensing/*` - Sensing mode, threshold, timer, signal status
- `wifi/*` - Connection status, SSID, signal strength
- `system/*` - Firmware version, uptime, heap stats
- `audio/*` - ADC levels, diagnostics, DSP state, input names
- `display/*` - Backlight, timeout, brightness
- `buzzer/*` - Enabled, volume
- `siggen/*` - Signal generator state
- `debug/*` - Debug mode, serial level
- `audio/usb/*` - USB audio connection, streaming, volume (guarded by `USB_AUDIO_ENABLED`)
- `diag/*` - Diagnostic events

**MQTT Topics Subscribed (Commands):**
- `*/set` suffix topics for writable entities
- Bidirectional control: sensing mode, amplifier, volume, mute, DSP bypass, etc.
- AP toggle command: sets `appState._pendingApToggle` flag (main loop executes WiFi mode change)

## Data Storage

**Filesystem (LittleFS):**
- Partition: `spiffs` type, 7.875MB (offset 0x810000, size 0x7E0000) in `partitions_ota.csv`
- Mounted at boot via `LittleFS.begin()`
- Key files:
  - `/config.json` - Primary settings (JSON v1, atomic write via `.tmp` rename)
  - `/hal_config.json` - HAL device configuration (per-device I2C, pins, volume, mute)
  - `/dsp_config.json` - DSP pipeline configuration and presets
  - `/crashlog.bin` - Binary ring buffer (10 boot entries with reset reason, heap stats)
  - `/diag_journal.bin` - Binary diagnostic event ring buffer (800 entries, CRC32 per entry, 64KB)
  - `/siggen_config.json` - Signal generator settings

**NVS (Non-Volatile Storage via Preferences):**
- Partition: `nvs` type, 20KB (offset 0x9000)
- Survives LittleFS format / factory reset
- Stores: WiFi credentials (multi-network, up to 5), diagnostic journal sequence counter, selected runtime settings

**EEPROM (I2C AT24C02, expansion modules):**
- Purpose: Identify hardware expansion modules via unique EEPROM data
- Format: v1 (92 bytes), v2 (94 bytes), v3 (128 bytes with compatible string + CRC-16)
- Address range: 0x50-0x57 on I2C bus
- Implementation: `src/dac_eeprom.h/.cpp`, `src/hal/hal_eeprom_v3.h/.cpp`

**No External Database:**
- All data stored locally on-device (LittleFS + NVS + expansion EEPROM)

**File Storage:**
- Local filesystem only (LittleFS on ESP32 flash)
- No cloud storage integration

**Caching:**
- In-memory state via `AppState` singleton (`src/app_state.h`)
- Dirty-flag pattern avoids redundant broadcasts
- DSP double-buffered configs for glitch-free swap
- Diagnostic journal: 32-entry PSRAM hot ring buffer with periodic flush to LittleFS

## Authentication & Identity

**Auth Provider: Custom (built-in)**
- Implementation: `src/auth_handler.h/.cpp`
- Password hashing: PBKDF2-SHA256 (10,000 iterations, random 16-byte salt)
- Storage format: `"p1:<saltHex>:<keyHex>"` in NVS/settings
- Legacy migration: Unsalted SHA256 hashes auto-migrate on next successful login
- First-boot: Random 10-char password (~57-bit entropy), displayed on TFT and serial
- Session management: 5 concurrent sessions, 1-hour timeout, `HttpOnly` cookie
- Rate limiting: Non-blocking, HTTP 429 with `Retry-After` header on excessive failures

**WebSocket Authentication:**
- Short-lived token from `GET /api/ws-token` (60s TTL, 16-slot pool, single-use)
- Client sends token in first WebSocket message after connection
- Server validates against token pool

**No External Auth Provider:**
- No OAuth, SAML, or external identity service
- Self-contained authentication on the device

## Web Server & WebSocket

**HTTP Server (port 80):**
- Framework: `WebServer` (ESP-IDF built-in), instantiated in `src/main.cpp`
- Serves: Gzip-compressed HTML/CSS/JS from flash (`src/web_pages_gz.cpp`)
- REST API: Registered endpoint handlers across modules
- Captive portal: `DNSServer` in AP mode redirects to device IP

**REST API Endpoints (organized by module):**
- WiFi: `GET/POST /api/wifi*`, `GET/POST /api/wifisave`, `GET /api/wifilist`, `POST /api/wifiremove`
- MQTT: `GET/POST /api/mqtt`
- Settings: `GET/POST /api/settings`, `GET /api/settings/export`, `POST /api/settings/import`, `POST /api/factoryreset`, `POST /api/reboot`
- OTA: `GET /api/checkupdate`, `POST /api/startupdate`, `GET /api/updatestatus`, `GET /api/releasenotes`, `GET /api/releases`, `POST /api/installrelease`, firmware upload
- Smart Sensing: `GET/POST /api/smartsensing`
- Auth: `POST /api/login`, `POST /api/logout`, `GET /api/auth/status`, `POST /api/password`, `GET /api/ws-token`
- HAL: `GET /api/hal/devices`, `POST /api/hal/scan`, `PUT /api/hal/devices`, `DELETE /api/hal/devices`, `POST /api/hal/devices/reinit`, `GET /api/hal/db/presets`
- DSP: CRUD for DSP config, presets (via `src/dsp_api.h/.cpp`)
- DAC: Volume, mute, filter mode (via `src/dac_api.h/.cpp`)
- Pipeline: Matrix routing config (via `src/pipeline_api.h/.cpp`)
- Diagnostics: `GET /api/diagnostics`

**WebSocket Server (port 81):**
- Framework: `WebSocketsServer` (vendored in `lib/WebSockets/`)
- Max clients: 10 (`MAX_WS_CLIENTS`)
- Purpose: Real-time state broadcasting to web UI clients
- Binary frames: Waveform (0x01) and spectrum (0x02) data for audio visualization
- JSON frames: All other state updates (levels, settings, diagnostics)
- Auth guard: `wsAnyClientAuthenticated()` skips serialization when no clients connected
- Implementation: `src/websocket_handler.h/.cpp`

## I2C Bus Architecture (ESP32-P4)

**Bus 0 (EXT):** GPIO 48 SDA / GPIO 54 SCL
- Shares SDIO with ESP32-C6 WiFi co-processor
- CRITICAL: I2C transactions while WiFi active cause `sdmmc_send_cmd` errors and MCU reset
- HAL discovery skips this bus when WiFi is connected
- Used for: External expansion modules (EEPROM 0x50-0x57, DAC I2C control)

**Bus 1 (ONBOARD):** GPIO 7 SDA / GPIO 8 SCL
- Dedicated to ES8311 codec (always safe)
- Used for: ES8311 I2C control, PA pin (GPIO 53)

**Bus 2 (EXPANSION):** GPIO 28 SDA / GPIO 29 SCL
- Always safe, no SDIO conflict
- Used for: Future expansion modules

## I2S Audio Interface

**I2S Port 0 (Primary RX/TX Full-Duplex):**
- RX: PCM1808 ADC #1 (BCK=20, LRC=21, MCLK=22, DOUT=23)
- TX: PCM5102A DAC (DOUT=24), full-duplex on same port
- Sample rate: 48kHz, 32-bit
- DMA: 12 buffers x 256 frames (~64ms runway)

**I2S Port 1 (Secondary RX):**
- RX: PCM1808 ADC #2 (DOUT2=25, shares BCK/LRC/MCLK with Port 0)
- Both ports configured as master RX (coordinated init, frequency-locked BCK)
- Init order: ADC2 first (no clock output), ADC1 second (all clock pins)

**I2S Port 2 (ES8311 TX):**
- TX: ES8311 onboard codec (DSDIN=9, LRCK=10, SCLK=12, MCLK=13)

## USB Audio

**TinyUSB UAC2 Speaker Device:**
- Interface: Native USB OTG (GPIO 19/20 on ESP32-P4)
- Protocol: USB Audio Class 2.0 (speaker device)
- Format: PCM16/PCM24 stereo, 48kHz default
- PID: 0x4004 (configurable via `HalDeviceConfig.usbPid`)
- Buffer: SPSC lock-free ring buffer (1024 frames, PSRAM)
- Task: FreeRTOS task on Core 0, adaptive poll rate (100ms idle / 1ms streaming)
- Feature guard: `-D USB_AUDIO_ENABLED`
- Custom audio class driver via `usbd_app_driver_get_cb()` weak function
- Implementation: `src/usb_audio.h/.cpp`

## Ethernet

**100Mbps Ethernet:**
- Interface: Built-in EMAC + PHY on ESP32-P4
- Implementation: `src/eth_manager.h/.cpp`
- Features: Link detection, IP assignment, default route management
- Can be used alongside WiFi

## Monitoring & Observability

**Error Tracking:**
- Crash log: Binary ring buffer in `/crashlog.bin` (last 10 boots with reset reason, heap stats, NTP timestamp)
- Diagnostic journal: 32-entry PSRAM hot buffer + 800-entry LittleFS persistence with CRC32, correlation IDs, error codes
- Audio health bridge: Monitors ADC health status, drives HAL state transitions, flap guard
- Implementation: `src/crash_log.h/.cpp`, `src/diag_journal.h/.cpp`, `src/hal/hal_audio_health_bridge.h/.cpp`
- Error codes: Enumerated in `src/diag_error_codes.h`

**Logs:**
- Serial output: Level-filtered (`LOG_D`/`LOG_I`/`LOG_W`/`LOG_E`/`LOG_NONE`) via `DebugSerial` class in `src/debug_serial.h/.cpp`
- WebSocket forwarding: Log lines forwarded to connected web clients as JSON with module name field
- Runtime level control: Adjustable via web UI and `applyDebugSerialLevel()`
- Baud rate: 115200

**Task Monitoring:**
- FreeRTOS task enumeration via `xTaskGetNext` in `src/task_monitor.h/.cpp`
- Stack watermark monitoring for known app tasks
- Opt-in via `debugTaskMonitor` flag (default off)
- 5-second polling interval

**Hardware Stats:**
- Free heap, min free heap, PSRAM usage, CPU temperature (ESP32-P4 internal sensor)
- CPU utilization tracking per core: `getCpuUsageCore0()`, `getCpuUsageCore1()`
- Broadcast every 2 seconds to WebSocket clients
- Heap critical flag: set when `ESP.getMaxAllocHeap() < 40KB`

## CI/CD & Deployment

**Hosting:**
- Firmware: Self-hosted on ESP32-P4 hardware (no cloud deployment)
- Documentation: GitHub Pages (Docusaurus static site on `gh-pages` branch)

**CI Pipeline (GitHub Actions):**
- `.github/workflows/tests.yml` - Quality Gates (push/PR to main/develop):
  1. `cpp-tests` - `pio test -e native -v` (1621 Unity tests, 70 modules)
  2. `cpp-lint` - cppcheck on `src/` (excluding `src/gui/`)
  3. `js-lint` - `find_dups.js` + `check_missing_fns.js` + ESLint
  4. `e2e-tests` - Playwright browser tests (26 tests, Chromium)
  5. `build` - PlatformIO firmware build (runs after all 4 gates pass)
  - Artifacts: firmware binary (30-day retention), Playwright report on failure (14-day retention)

- `.github/workflows/release.yml` - Release Firmware (manual dispatch):
  - Same 4 quality gates + firmware build
  - Version bump: patch/minor/major
  - Channel: stable/beta
  - Creates GitHub release with firmware binary

- `.github/workflows/docs.yml` - Documentation (push to main, path-filtered):
  - Incremental change detection (`tools/detect_doc_changes.js`)
  - Optional Claude API doc generation (`tools/generate_docs.js`)
  - Docusaurus build and deploy to `gh-pages`

**Pre-commit Hooks:**
- `.githooks/pre-commit` - 3 fast checks:
  1. Duplicate JS declarations (`node tools/find_dups.js`)
  2. Missing function references (`node tools/check_missing_fns.js`)
  3. ESLint on `web_src/js/`
- Activate: `git config core.hooksPath .githooks`

## OTA Update Mechanism

**Update Flow:**
1. Device polls GitHub API for latest release (every 5 min, with exponential backoff)
2. Compares remote version against `FIRMWARE_VERSION` in `src/config.h`
3. Downloads firmware binary via HTTPS (WiFiClientSecure with pinned root CAs)
4. SHA256 verification of downloaded binary
5. Writes to inactive OTA partition via ESP-IDF `Update` library
6. Reboots into new firmware
7. Success flag persisted across reboot for "just updated" notification

**Manual Upload:**
- HTTP endpoint for direct firmware upload (no GitHub required)
- Handlers: `handleFirmwareUploadComplete()`, `handleFirmwareUploadChunk()`

**Non-Blocking:**
- OTA check and download run on dedicated one-shot FreeRTOS tasks on Core 0
- Main loop and audio pipeline are not blocked during OTA

## Webhooks & Callbacks

**Incoming:**
- MQTT subscription topics accept commands from Home Assistant / MQTT clients
- REST API endpoints accept commands from web UI
- WebSocket accepts commands from connected web clients

**Outgoing:**
- MQTT publish to configured broker (state updates, HA discovery)
- No HTTP webhooks or callback URLs

## HAL Device Discovery

**3-Tier Discovery (`src/hal/hal_discovery.h/.cpp`):**
1. I2C bus scan (address range 0x08-0x77)
2. EEPROM probe at 0x50-0x57 (AT24C02 with ALXD magic, v1/v2/v3 format)
3. Manual configuration via web UI REST API

**Device Database (`src/hal/hal_device_db.h/.cpp`):**
- In-memory database with 7 builtin entries: PCM5102A, ES8311, PCM1808, NS4150B, TempSensor, SigGen, USB Audio
- LittleFS JSON persistence for user-added devices
- EEPROM v3 compatible string matching (e.g., "ti,pcm5102a", "evergrande,es8311")

**Driver Registry (`src/hal/hal_builtin_devices.h/.cpp`):**
- Compatible string to factory function mapping
- Registered drivers: PCM5102A, ES8311, PCM1808, DSP bridge, NS4150B, TempSensor, SigGen, USB Audio, MCP4725

## WiFi Integration

**WiFi Client:**
- Multi-network support (up to 5 saved networks)
- Async connection with retry/backoff
- Static IP configuration support
- Connection via ESP32-C6 co-processor (SDIO interface)
- Power save mode: forced `WIFI_PS_NONE` for low latency

**Access Point Mode:**
- Captive portal with DNS redirect
- Default password: "12345678" (defined in `src/config.h`)
- STA+AP concurrent mode supported

**Implementation:** `src/wifi_manager.h/.cpp`

---

*Integration audit: 2026-03-10*
