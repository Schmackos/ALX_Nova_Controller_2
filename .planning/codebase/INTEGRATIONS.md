# External Integrations

**Analysis Date:** 2026-03-23

## APIs & External Services

**GitHub:**
- Service: GitHub Releases API (`api.github.com`)
- What it's used for: Firmware OTA update checking, version comparison, release notes fetching, binary firmware download
- SDK/Client: `HTTPClient` (Arduino) with `WiFiClientSecure`
- Auth: None (public API, rate limited by IP)
- Endpoints:
  - `GET https://api.github.com/repos/{owner}/{repo}/releases/latest` - Latest release info
  - `GET https://api.github.com/repos/{owner}/{repo}/releases?per_page=5` - Release history
  - `GET https://api.github.com/repos/{owner}/{repo}/releases/tags/{version}` - Specific release details
- Implementation: `src/ota_updater.cpp`
- Binary Download: HTTPS with SHA256 verification (no code signing)
- Certificate: Sectigo R46/E46 (covers api.github.com) embedded in `src/ota_certs.h`

**GitHub Web URLs:**
- Custom device contribution: `https://github.com/{owner}/{repo}/issues/new` (browser redirect)
- User manual: `https://raw.githubusercontent.com/{owner}/{repo}/main/USER_MANUAL.md` (content fetch, frontend only)
- Documentation: `https://github.com/{owner}/{repo}/blob/main/` (browser navigation)

## Data Storage

**Databases:**
- None (no remote database)

**File Storage:**
- LittleFS local filesystem (on-device flash)
  - Primary: `/config.json` (user settings, WiFi SSID, MQTT config, audio settings)
  - HAL: `/hal_config.json` (device configurations, pin overrides)
  - HAL DB: `/hal/custom/*.json` (user-created custom device schemas)
  - Legacy: `mqtt_config.txt`, `settings.txt` (auto-migrated to JSON on first boot)
  - Connection: Via `LittleFS` Arduino library with atomic write (temp file + rename)

**Caching:**
- In-memory JSON parsing (ArduinoJson v7 heap-allocates tree nodes)
- DSP configuration double-buffering (glitch-free atomic swap)
- HAL device cache in memory (no persistent cache beyond `/hal_db.json`)

## Authentication & Identity

**Web UI Password:**
- Auth Provider: Custom (on-device)
- Implementation: PBKDF2-SHA256 with 50k iterations (`p2:` format in storage)
- Backward compatibility: Auto-migrates legacy `p1:` (10k) and SHA256 hashes on login
- Cookie: `HttpOnly`, session-based
- Endpoint: `GET /login`, `POST /login` (REST + cookie)
- WS Auth: Short-lived token (60s TTL) from `GET /api/ws-token` (16-slot pool)
- Location: `src/auth_handler.cpp`

**MQTT Broker Authentication:**
- Auth Provider: MQTT 3.1.1 username/password (configurable)
- Implementation: Plain text credentials in MQTT CONNECT (TLS recommended by user)
- Storage: `/config.json` (plaintext on device, encrypted in MQTT transit optional)
- Configuration: Web UI under WiFi/Network settings
- Location: `src/mqtt_handler.cpp`

**Device Serial Number:**
- Generated from eFuse MAC address at first boot
- Format: `ALX-{MAC_HEX}` (e.g., `ALX-AABBCCDDEEFF`)
- Storage: NVS namespace `device` key `serial`
- Used for: MQTT device ID, Home Assistant unique_id, telemetry
- Location: `src/main.cpp` `initSerialNumber()`

## Home Automation Integration

**Home Assistant MQTT Auto-Discovery:**
- Protocol: MQTT topic-based discovery (Home Assistant convention)
- Base topic: Configurable via web UI (default: `alx/{serial}`)
- Discovery topics: `homeassistant/{entity_type}/{device_id}/{entity_id}/config`
- Entities published: ~100+ including switches (amplifier, DSP, features), sensors (temperature, audio metrics), numbers (volume, gain), selects (mode switches, presets)
- Availability: `{base_topic}/status` with `online`/`offline` payloads
- Generic HAL device discovery: Binary sensors for expansion mezzanines without dedicated HA entities (auto-populated when new devices detected)
- Implementation: `src/mqtt_ha_discovery.cpp`
- Broker config: Web UI under WiFi/Network → MQTT settings
- Disable option: `MQTT → Disable HA Discovery` toggle

## Monitoring & Observability

**Error Tracking:**
- Local: `DIAG_SYS_*`, `DIAG_HAL_*`, `DIAG_AUDIO_*` event codes (0x2000-0x7FFF)
- Journal: In-memory ring buffer with 256-entry cap (persisted briefly, not flash)
- Export: REST `GET /api/diagnostics/journal` + WebSocket `diagnostics` message
- No remote error tracking (all local)

**Logs:**
- Local serial output (115200 baud via UART)
- Module-prefixed: `[WiFi]`, `[MQTT]`, `[Audio]`, `[HAL]`, etc.
- Levels: LOG_D (debug), LOG_I (info), LOG_W (warning), LOG_E (error)
- Forwarding: WebSocket broadcasts to web UI debug console (real-time log streaming)
- Runtime control: `debugSerialLevel` configurable via web UI

**Hardware Monitoring:**
- Internal heap stats (free, fragmentation)
- PSRAM status (free, fallback count, allocation failures)
- WiFi RSSI (signal strength)
- CPU usage per task (task_monitor via FreeRTOS stats)
- Broadcast interval: Configurable 1-10s (default 5s) via web UI
- Endpoints: `GET /api/diag/snapshot`, WebSocket `hardwareStats`

## CI/CD & Deployment

**Hosting:**
- None (embedded device, no cloud hosting)
- Web UI: Served from embedded web server (port 80)
- WebSocket: Port 81 (real-time updates)

**CI Pipeline:**
- GitHub Actions
- Triggers: Push/PR to `main` and `develop` branches
- Workflows:
  - `tests.yml` - 4 parallel gates (cpp-tests, cpp-lint, js-lint, e2e-tests)
  - `release.yml` - Same 4 gates before release build
  - `docs.yml` - Docusaurus site build and GitHub Pages deploy
- Output artifacts: Playwright HTML report (failure cases, 14-day retention)

**Firmware Distribution:**
- GitHub Releases (public, no authentication)
- OTA update check interval: 5 min (with exponential backoff on failures)
- Download: HTTPS with SHA256 verification
- Installation: In-place via ESP32 Update partition (OTA-safe)

**Documentation:**
- Docusaurus v3 static site
- Deployment: GitHub Pages (`gh-pages` branch, auto-deploy on successful doc build)
- Build trigger: CI pipeline on `main`/`develop` commits
- Base URL: Public docs site (auto-generated from `docs-site/` directory)

## Environment Configuration

**WiFi Credentials:**
- Storage: NVS (survives LittleFS format)
- Retrieval: At boot via `loadWifiNetworks()` or web UI scan/add
- Supported modes: Client (multi-network) and AP (fallback)
- Configuration: Web UI under WiFi tab, persisted to `/config.json`

**MQTT Broker Configuration:**
- Parameters: Broker URL/hostname, port (default 1883), username, password, base topic, HA discovery toggle
- Storage: `/config.json` with fallback legacy `mqtt_config.txt`
- Encryption: Optional (depends on broker config, typically TLS 8883)
- Authentication: PBKDF2 or plain text (user-configured)
- Default topic structure: `alx/{device_serial}/{entity}`

**Ethernet Configuration (ESP32-P4 only):**
- DHCP (default) or static IP
- Parameters: IP, subnet mask, gateway, DNS1, DNS2, hostname
- Storage: `/config.json`
- Link speed: 100Mbps (Waveshare board hardwired)

**Critical Environment Variables:**
- None (embedded system, no .env files at runtime)
- Build-time: PIN definitions in `platformio.ini` and `src/config.h`
- Secrets: None externalized (embedded certificates, no API keys)

**Secrets Location:**
- Embedded: SSL certs in `src/ota_certs.h` (public CAs, no private keys)
- Device: WiFi/MQTT credentials in NVS (encrypted by ESP32 HAL) and LittleFS config
- No external secrets management

## Webhooks & Callbacks

**Incoming Webhooks:**
- None (embedded device, does not receive webhooks)

**Outgoing Webhooks:**
- None (no egress webhook delivery)

**MQTT Command Subscriptions (inbound):**
- `{base_topic}/smartsensing/amplifier/set` - Amplifier control
- `{base_topic}/dsp/enabled/set` - DSP enable/disable
- `{base_topic}/audio/volume/set` - Volume control (per output)
- `{base_topic}/audio/mute/set` - Mute control
- `{base_topic}/signal_generator/enabled/set` - Signal gen start/stop
- `{base_topic}/systemctl/restart` - Device restart
- `{base_topic}/systemctl/factory_reset` - Factory reset
- Full list: `src/mqtt_handler.cpp` `mqttCallback()`

**MQTT Publish Topics (outbound, periodic):**
- Status: `{base_topic}/status` (online/offline)
- Audio: `{base_topic}/audio/*` (volume, mute, RMS levels, THD)
- WiFi: `{base_topic}/wifi/*` (SSID, RSSI, IP)
- Sensing: `{base_topic}/smartsensing/*` (amplifier state, signal detection)
- DSP: `{base_topic}/dsp/*` (enabled, preset, CPU usage)
- HAL: `{base_topic}/hal/*` (device states, temperatures)
- OTA: `{base_topic}/ota/*` (version, update status)
- Change-based (dirty flag): `{base_topic}/{entity}/set` responses

**WebSocket Real-Time Events (browser):**
- Bidirectional JSON + binary frames over port 81
- Binary frames: Waveform (0x01, audio samples), Spectrum (0x02, FFT bins)
- Commands: Settings changes, DSP config, WiFi/MQTT commands
- Broadcasts: 17 state update types (audio, DSP, WiFi, MQTT, HAL, system)
- Token auth: 60s TTL, 16 concurrent clients
- Protocol details: `docs-site/docs/developer/websocket.md`

## REST API Endpoints

**Host:** `http://{device_ip}:80/`

**Auth:** Cookie + PBKDF2 password, rate-limited to 5 attempts/minute

**Core Endpoints:**
- `GET /` - Serve web UI (HTML + assets)
- `GET /login`, `POST /login` - Authentication
- `GET /api/ws-token` - WebSocket auth token (60s TTL)

**Audio Control:**
- `GET/POST /api/dac` - DAC state, volume, enable/disable (slot-indexed)
- `GET /api/i2s/ports` - I2S port status (mode, format, pins, sample rate)

**DSP:**
- `GET/POST /api/dsp` - DSP settings (enable, bypass, presets)
- `GET/POST /api/dsp/config` - DSP stage configuration (biquad, FIR, gain, delay, etc.)

**HAL (Hardware Abstraction Layer):**
- `GET /api/hal/devices` - List all detected/configured devices
- `POST /api/hal/scan` - Trigger I2C device discovery (409 guard for duplicate scans)
- `GET /api/hal/db` - Device database (built-in drivers)
- `GET /api/hal/db/presets` - Device presets by compatible string
- `PUT /api/hal/devices/{slot}` - Update device config (pins, I2S port, sample rate)
- `DELETE /api/hal/devices/{slot}` - Remove device
- `POST /api/hal/devices/reinit` - Reinitialize specific device
- `GET/POST/DELETE /api/hal/devices/custom` - Custom device schemas (Tier 1-3 support)
- `GET /api/hal/scan/unmatched` - Unmatched I2C addresses for custom creator

**Pipeline:**
- `GET /api/pipeline/matrix` - 8×32 input-to-output routing matrix
- `POST /api/pipeline/matrix` - Update matrix slot routing
- `GET /api/pipeline/analysis` - Real-time audio analysis (RMS, peak, THD)

**Settings:**
- `GET/POST /api/settings` - Load/save all settings (`/config.json`)
- `POST /api/settings/export` - Export settings JSON
- `POST /api/settings/import` - Import settings (version-aware, v1 base or v2 full)
- `GET /api/settings/preview` - Preview import changes before applying
- `POST /api/settings/factory-reset` - Erase all settings

**Diagnostics:**
- `GET /api/diagnostics` - Full diagnostic snapshot
- `GET /api/diagnostics/journal` - Event journal (256 entries)
- `DELETE /api/diagnostics/journal` - Clear journal
- `GET /api/diag/snapshot` - Current heap, PSRAM, task stats

**Signal Generator:**
- `GET/POST /api/signalgenerator` - Config (waveform, freq, amplitude, mode)

**WiFi/Network:**
- `GET /api/wifi/networks` - Scan results
- `POST /api/wifi/connect` - Join SSID with password
- `POST /api/wifi/ap` - Enable/disable AP mode
- `GET /api/wifi/status` - Current connection status
- `POST /api/wifi/scan` - Trigger WiFi scan (deferred, non-blocking)
- `GET /api/eth/status` - Ethernet connection info (ESP32-P4 only)

**MQTT:**
- Configuration via `/api/settings` (broker URL, port, credentials)
- Test connection: `POST /api/mqtt/test`

**OTA/Updates:**
- `GET /api/ota/check` - Check for firmware updates
- `POST /api/ota/update` - Download and install firmware
- `GET /api/ota/history` - Release history (5 latest)

**PSRAM:**
- `GET /api/psram/status` - PSRAM health (free, usage %, fallback count, budget breakdown)

## Error Codes & Diagnostics

**Diagnostic Event Codes** (`src/diag_event.h`):
- `0x2000-0x3FFF` - System diagnostics (heap, PSRAM, WiFi)
- `0x4000-0x4FFF` - WiFi/network errors
- `0x5000-0x5FFF` - MQTT errors
- `0x6000-0x6FFF` - OTA/firmware errors
- `0x7000-0x7FFF` - Audio/DSP errors
- `0x1000-0x1FFF` - HAL (Hardware Abstraction Layer) diagnostics
- Each code has severity (INFO, WARNING, ERROR), facility, and human-readable message

**Example codes:**
- `DIAG_HAL_I2C_BUS_CONFLICT` (0x1101) - Bus 0 SDIO conflict when WiFi active
- `DIAG_HAL_PROBE_RETRY_OK` (0x1105) - I2C device probe succeeded on retry
- `DIAG_SYS_HEAP_WARNING` (0x2001) - Internal heap < 50KB
- `DIAG_SYS_PSRAM_CRITICAL` (0x2004) - PSRAM < 512KB

---

*Integration audit: 2026-03-23*
