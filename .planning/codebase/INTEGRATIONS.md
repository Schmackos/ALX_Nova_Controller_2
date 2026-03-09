# External Integrations

**Analysis Date:** 2026-03-09

## APIs & External Services

**GitHub Releases API:**
- Service: GitHub REST API (`api.github.com`)
- Purpose: OTA firmware update check, release list fetch, release notes retrieval
- Endpoints called from firmware:
  - `GET https://api.github.com/repos/{owner}/{repo}/releases/latest` — stable channel OTA check
  - `GET https://api.github.com/repos/{owner}/{repo}/releases?per_page=5` — release list
  - `GET https://api.github.com/repos/{owner}/{repo}/releases/tags/{version}` — release notes
- Firmware download from: `objects.githubusercontent.com` (CDN)
- Auth: None (public repo, unauthenticated requests)
- TLS: `WiFiClientSecure` with hardcoded root CA bundle (Sectigo R46 RSA, Sectigo E46 ECC, DigiCert Global G2) embedded in `src/ota_updater.cpp`
- Config: `GITHUB_REPO_OWNER` = `"Schmackos"`, `GITHUB_REPO_NAME` = `"ALX_Nova_Controller_2"` — defined in `src/config.h`
- Checksum verification: SHA256 verified via `mbedtls/md.h` after download
- SDK/Client: `WiFiClientSecure` + `HTTPClient` (Arduino ESP32 built-ins)
- Trigger: Periodic check every 5 minutes (`OTA_CHECK_INTERVAL = 300000` ms), or manual via `POST /api/checkUpdate`

**Anthropic Claude API:**
- Service: Anthropic API (`api.anthropic.com`)
- Purpose: AI-assisted documentation generation — `tools/generate_docs.js` calls Claude to write Docusaurus pages from firmware source files
- SDK: `@anthropic-ai/sdk` (installed in CI, not in repo)
- Auth: `ANTHROPIC_API_KEY` environment variable (GitHub Actions secret)
- Trigger: `docs.yml` workflow on push to `main` when source/docs files change, or `workflow_dispatch` with `regen_all` option
- Used only in CI, not at device runtime

## Data Storage

**File System (LittleFS):**
- Partition: `spiffs` (labeled spiffs subtype, used as LittleFS), 7.875 MB at offset `0x810000`
- Key files:
  - `/config.json` — primary settings (JSON v1+ format: `{ "version": 1, "settings": {...}, "mqtt": {...} }`)
  - `/hal_config.json` — HAL per-device config (pin overrides, I2C/I2S params)
  - `/pipeline_matrix.json` — 16x16 audio routing matrix
  - `/diag_journal.bin` — diagnostic event journal ring buffer (max 800 entries, 64KB)
  - `/pipeline_*.json` — DSP preset configs
  - `/crashlog.bin` — crash log with RTC timestamps
  - `/siggen.json` — signal generator settings
  - `/input_names.json` — custom input lane names
  - `config.json.tmp` — atomic write staging file (renamed to `config.json` on success)
- Legacy files (migration only, not created on new installs): `settings.txt`, `mqtt_config.txt`
- Access: `LittleFS` Arduino library throughout `src/`

**NVS (Non-Volatile Storage via ESP-IDF):**
- Client: `Preferences` Arduino library
- Namespaces used:
  - `auth` — web password hash (PBKDF2-SHA256 format: `"p1:<saltHex>:<keyHex>"`)
  - `wifi_n` (indexed 0-4) — up to 5 saved WiFi networks (SSID, password, static IP config)
  - `hal` — HAL device fault counters
  - `ota` — OTA success flag and previous version for post-update notification
- Partition: `nvs` (20KB at offset `0x9000`)

**Caching:**
- None (no Redis, Memcached, or similar)

**File Storage:**
- Local LittleFS filesystem only (no cloud storage)

## Authentication & Identity

**Web UI Authentication:**
- Type: Session-based with secure cookie
- Password storage: PBKDF2-SHA256 (10,000 iterations, 16-byte random salt), stored in NVS under `auth` namespace
- Legacy migration: SHA256 unsalted hashes auto-migrate to PBKDF2 on next successful login
- Default password: 10-character random (alphanumeric, ~57-bit entropy), generated on first boot, displayed on TFT and serial, regenerated on factory reset
- Cookie: `HttpOnly` flag set; `session=<token>` cookie
- Session: 5 concurrent sessions, 1-hour TTL (`SESSION_TIMEOUT_US = 3600000000`)
- Login rate limiting: Non-blocking; failed attempts set `_nextLoginAllowedMs`; excess returns HTTP 429 with `Retry-After` header
- Implementation: `src/auth_handler.cpp`

**WebSocket Authentication:**
- Flow: Client fetches `GET /api/ws-token` (returns short-lived token, 60s TTL), opens WebSocket to port 81, sends token in first auth message
- Token pool: 16 slots, single-use (`src/auth_handler.cpp`)

**OTA Downloads:**
- TLS certificate validation via hardcoded root CA bundle in `src/ota_updater.cpp` (Sectigo R46/E46, DigiCert G2)
- Integrity: SHA256 checksum verified post-download via `mbedtls`

## Network Protocols

**HTTP Server (port 80):**
- Library: `WebServer` (Arduino ESP32 built-in)
- Routes registered in `src/main.cpp`
- Handles: REST API (`/api/*`), static web pages (gzip), captive portal, firmware upload

**WebSocket Server (port 81):**
- Library: `WebSocketsServer` (vendored `lib/WebSockets/` v2.7.3)
- Real-time bidirectional state updates between device and browser
- Binary frames (`sendBIN`) for waveform (`WS_BIN_WAVEFORM = 0x01`) and spectrum (`WS_BIN_SPECTRUM = 0x02`) data
- JSON frames for all other state broadcasts
- Implementation: `src/websocket_handler.cpp`

**MQTT (configurable port, default 1883):**
- Library: `PubSubClient@^2.8` (via `WiFiClient`)
- Broker: User-configured (IP/hostname + port stored in `/config.json`)
- Reconnect interval: 5 seconds; runs in dedicated FreeRTOS task `mqtt_task` on Core 0
- TLS: Not used for MQTT (plain TCP)
- Base topic: Configurable via web UI, defaults to device serial
- Implementation split: `src/mqtt_handler.cpp` (core lifecycle), `src/mqtt_publish.cpp` (publish functions + change-detection statics), `src/mqtt_ha_discovery.cpp` (HA discovery payloads ~1880 lines)

**DNS (Captive Portal):**
- Library: `DNSServer` (Arduino ESP32 built-in, port 53)
- Active in AP mode to redirect all DNS queries to device IP

## Home Assistant Integration

**Protocol:** MQTT with Home Assistant discovery (`homeassistant/` topic prefix)
- Discovery topics published under: `homeassistant/{entity_type}/{device_id}/{entity}/config`
- Subscribes to: `homeassistant/status` (online/offline) to re-publish discovery on HA restart
- Entity types published: `switch`, `select`, `number`, `sensor`, `binary_sensor`, `button`
- Entities include: amplifier relay, AP mode, sensing mode, timer duration, audio threshold, audio level, WiFi RSSI, firmware version, DAC state, DSP state, USB audio state, crash diagnostics, debug mode
- Retained discovery messages: `true` (MQTT retain flag)
- Implementation: `src/mqtt_ha_discovery.cpp`
- No direct HA API calls — all via MQTT broker

## Monitoring & Observability

**Error Tracking:**
- None (no Sentry, Rollbar, or similar external service)

**Diagnostic Journal:**
- In-memory ring buffer: 32 hot entries (PSRAM)
- Persistent ring: 800 entries on LittleFS (`/diag_journal.bin`, flushed every 60s for WARN+ severity)
- Correlation IDs, error codes (`src/diag_error_codes.h`), self-healing retry events
- Accessible via REST: `GET /api/diagnostics` and WebSocket broadcast on new events
- Implementation: `src/diag_journal.cpp`

**Crash Log:**
- Stored in LittleFS, includes RTC timestamp (backfilled after NTP sync)
- Accessible via REST
- Implementation: `src/crash_log.cpp`

**Logs:**
- Serial output at 115200 baud via `debug_serial.h` macros (`LOG_D/I/W/E`)
- WebSocket log forwarding: `broadcastLine()` sends module-tagged log entries to browser
- Log level controlled at runtime via `applyDebugSerialLevel()`
- Build output, test reports saved to `logs/` directory

## Time Synchronization

**NTP:**
- Servers: `pool.ntp.org` (primary), `time.nist.gov` (fallback)
- Client: `configTime()` (Arduino ESP32 built-in)
- Triggered: On WiFi connect (`src/wifi_manager.cpp`), on settings load (`src/settings_manager.cpp`)
- Required for: SSL certificate validation (OTA), crash log timestamps
- Timezone offset: `appState.general.timezoneOffset` + `appState.general.dstOffset` (seconds, configured in web UI)
- Implementation: `syncTimeWithNTP()` in `src/utils.cpp`

## CI/CD & Deployment

**CI Platform:**
- GitHub Actions (`.github/workflows/tests.yml`, `release.yml`, `docs.yml`)
- Runs on `ubuntu-latest`

**Quality Gates (`.github/workflows/tests.yml`):**
- `cpp-tests`: `pio test -e native -v` (1614 Unity tests)
- `cpp-lint`: `cppcheck` on `src/` (excludes `src/gui/`)
- `js-lint`: `node tools/find_dups.js` + `node tools/check_missing_fns.js` + ESLint
- `e2e-tests`: Playwright (26 tests, Chromium only)
- `build`: `pio run -e esp32-p4` — firmware `.bin` uploaded as artifact (30-day retention)
- All 4 gates must pass before build job runs

**Release (`.github/workflows/release.yml`):**
- Trigger: `workflow_dispatch` with `version_bump` (patch/minor/major) and `channel` (stable/beta)
- Steps: Same 4 quality gates → version bump in `src/config.h` → firmware build → SHA256 checksum → generate release notes from `RELEASE_NOTES.md` + git log → GitHub Release with `firmware.bin` attachment
- GitHub Release action: `softprops/action-gh-release@v2`
- Auth: `secrets.GITHUB_TOKEN`

**Documentation Deployment (`.github/workflows/docs.yml`):**
- Trigger: Push to `main` when `src/**`, `docs-internal/**`, `docs-site/**`, or `tools/generate_docs.js` changes; also `workflow_dispatch`
- Steps: Detect changed doc sections (`tools/detect_doc_changes.js`) → call Anthropic Claude API to regenerate affected sections → Docusaurus build → deploy to GitHub Pages (`gh-pages` branch)
- Hosting: GitHub Pages at `https://schmackos.github.io/ALX_Nova_Controller_2/`
- Auth: `secrets.ANTHROPIC_API_KEY` (optional; skips generation but still builds/deploys if absent)
- Deploy action: `peaceiris/actions-gh-pages@v3`

**Pre-commit Hooks (`.githooks/pre-commit`):**
- `node tools/find_dups.js` — duplicate JS declarations
- `node tools/check_missing_fns.js` — undefined function references
- ESLint on `web_src/js/`
- Activate with: `git config core.hooksPath .githooks`

## Hardware Protocols (On-Device)

**I2S (audio):**
- ESP-IDF `<driver/i2s_std.h>` (new API, IDF5)
- Two instances: I2S_NUM_0 (PCM1808 ADC1, clock master, full-duplex with PCM5102A DAC TX), I2S_NUM_1 (PCM1808 ADC2, data-only)
- ES8311 codec on separate I2S2 TX

**I2C:**
- Bus 0 (GPIO 48/54): External/EEPROM — shares SDIO with WiFi, not scanned when WiFi active
- Bus 1 (GPIO 7/8): ES8311 onboard codec — always safe
- Bus 2 (GPIO 28/29): Expansion — always safe
- HAL discovery scans 0x08-0x77 address range

**USB (TinyUSB UAC2):**
- ESP32-P4 native USB OTG (GPIO 19/20)
- Presents as USB Audio Class 2 speaker device to host
- PID configurable via HAL `HalDeviceConfig.usbPid`
- Custom class driver registered via `usbd_app_driver_get_cb()` weak function
- Implementation: `src/usb_audio.cpp`

**EMAC (Ethernet):**
- ESP32-P4 built-in Ethernet MAC
- 100Mbps Full Duplex
- Implementation: `src/eth_manager.cpp`

## Webhooks & Callbacks

**Incoming:**
- None (no external webhook endpoints exposed)

**Outgoing:**
- GitHub API polling (pull-based, not webhook)
- MQTT publishes to broker (user-configured)

## Environment Configuration

**Device runtime configuration (no .env files):**
All runtime-configurable settings stored in LittleFS `/config.json` and NVS:
- WiFi credentials (NVS, up to 5 networks)
- MQTT broker IP/port/topic prefix (LittleFS `/config.json`)
- Web password (NVS, PBKDF2 hash)
- Timezone/DST offsets (LittleFS `/config.json`)
- Audio thresholds, sensing mode, timer duration (LittleFS `/config.json`)
- HAL device pin overrides (LittleFS `/hal_config.json`)

**CI secrets (GitHub Actions):**
- `GITHUB_TOKEN` — release creation, GitHub Pages deploy (auto-provided by Actions)
- `ANTHROPIC_API_KEY` — doc generation via Claude API (optional; docs workflow degrades gracefully)

**Build-time constants:**
- All build flags defined in `platformio.ini` `[env:esp32-p4]` `build_flags`
- No `.env` files used; firmware has no `.env` dependency

---

*Integration audit: 2026-03-09*
