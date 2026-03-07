# External Integrations

**Analysis Date:** 2026-03-07

## APIs & External Services

**GitHub Releases API:**
- GitHub REST API (`api.github.com`) — OTA firmware update check and release list
  - Endpoints: `GET /repos/{owner}/{repo}/releases/latest` (stable channel), `GET /repos/{owner}/{repo}/releases` (beta channel, 5 per page), `GET /repos/{owner}/{repo}/releases/tags/{version}` (release notes)
  - Firmware download: `objects.githubusercontent.com` CDN
  - Client: `WiFiClientSecure` + `HTTPClient` in `src/ota_updater.cpp`
  - Auth: None (public repository, unauthenticated)
  - SSL: Three bundled root CAs in `src/ota_updater.cpp` — Sectigo R46 (RSA), Sectigo E46 (ECC), DigiCert Global Root G2; valid until 2038-2046
  - Firmware integrity: SHA256 checksum verified via mbedTLS before flashing (embedded in release notes or as `.sha256` asset)
  - Config: `GITHUB_REPO_OWNER` and `GITHUB_REPO_NAME` defined in `src/config.h`

**GitHub Raw CDN (HAL Device Database):**
- Fetches device YAML descriptors on demand when a HAL device is not in the local DB
  - Base URL: `https://raw.githubusercontent.com/alx-audio/hal-devices/main/devices/`
  - Client: `WiFiClientSecure` + `HTTPClient` in `src/hal/hal_online_fetch.cpp`
  - Auth: None (public repository)
  - ETag caching in NVS; rate limit (429) handling with backoff
  - Guarded by: `DAC_ENABLED` build flag and heap check (~44KB required for TLS)

**NTP (Network Time Protocol):**
- Time synchronization for OTA SSL certificate validation and crash log timestamps
  - Servers: `pool.ntp.org` (primary), `time.nist.gov` (secondary)
  - Client: Arduino `configTime()` in `src/utils.cpp`
  - Called once after WiFi/Ethernet connection is established; backfills crash log timestamps via `crashlog_update_timestamp()`

## Data Storage

**Filesystems:**
- LittleFS — Primary persistent filesystem, mounted from the `spiffs` partition (8 MB)
  - Files stored: `/hal_config.json` (HAL device configs), `/dsp_*.json` (DSP presets), `/crashlog.bin` (crash log ring buffer), `/diag_journal.bin` (diagnostic journal), WiFi and MQTT settings JSON files
  - Client: `<LittleFS.h>` (Arduino-ESP32 bundled)

**NVS (Non-Volatile Storage):**
- Key-value store for small, frequently-read settings
  - Namespaces used: auth (password hash, session), WiFi networks (up to 5 SSIDs with static IP configs), MQTT broker settings, OTA flags, signal generator settings, diagnostic journal sequence counter, HAL ETag cache
  - Client: `<Preferences.h>` (Arduino-ESP32 bundled) throughout `src/settings_manager.cpp`, `src/auth_handler.cpp`, `src/mqtt_handler.cpp`, `src/hal/hal_online_fetch.cpp`

**PSRAM (External RAM):**
- Used for large allocations: DSP delay lines (`ps_calloc()`), audio float working buffers, USB audio ring buffer (1024 frames), diagnostic journal hot ring buffer (32 entries)
  - Accessed via `esp_heap_caps_malloc(MALLOC_CAP_SPIRAM)` in `src/dsp_pipeline.cpp`, `src/audio_pipeline.cpp`, `src/diag_journal.cpp`
  - Falls back to internal heap with pre-flight 40KB reserve check when PSRAM unavailable

**File Storage:**
- Local LittleFS only — no cloud file storage

**Caching:**
- NVS ETag cache for HAL device YAML (in `src/hal/hal_online_fetch.cpp`)
- In-memory: 32-entry PSRAM hot ring buffer for diagnostic journal (`DIAG_JOURNAL_HOT_ENTRIES`)

## Authentication & Identity

**Web UI Authentication:**
- Custom session-cookie implementation in `src/auth_handler.cpp`
  - Sessions: up to 5 concurrent (`MAX_SESSIONS = 5`), 1-hour timeout
  - Password stored as SHA256 hash in NVS (`pwd_hash` key); legacy plaintext migrated on first boot
  - Session IDs: randomly generated via `esp_random()`
  - Rate limiting: login rate limit reset via `resetLoginRateLimit()`
  - Timing-safe comparison: `timingSafeCompare()` prevents timing attacks
  - Cookie name: `session` (HTTP-only)
  - Endpoints: `POST /login`, `POST /logout`, `GET /api/auth/status`, `POST /api/password`

**MQTT Authentication:**
- User-configurable broker credentials (username + password) stored in NVS
  - Set via web UI MQTT settings (`web_src/js/21-mqtt-settings.js`)
  - Passed to `PubSubClient.setCredentials()` in `src/mqtt_handler.cpp`

**No OAuth / Third-Party Identity Provider** — self-contained authentication only.

## Monitoring & Observability

**Error Tracking:**
- Diagnostic journal (`src/diag_journal.cpp`) — structured events with severity, error code, device slot, correlation ID
  - Hot ring: 32 entries in PSRAM, spinlock-protected
  - Persistent ring: up to 800 entries in `/diag_journal.bin` on LittleFS, per-entry CRC32
  - Flushed every 60s (`DIAG_FLUSH_INTERVAL_MS`) for WARN+ events
  - Exposed via WebSocket and MQTT dirty-flag broadcasts (`EVT_DIAG`)

**Crash Log:**
- `src/crash_log.cpp` — 10-entry ring buffer in `/crashlog.bin` on LittleFS
  - Records: reset reason, heap free at boot, min heap lifetime, ISO 8601 timestamp (backfilled after NTP)
  - Exposed via `/api/diagnostics` REST endpoint and MQTT `publishMqttCrashDiagnostics()`

**Serial Logging:**
- `src/debug_serial.cpp` — `LOG_D/I/W/E` macros with `[ModuleName]` prefix; runtime log level control
  - WebSocket forwarding via `broadcastLine()` — sends module name as separate JSON field for frontend filtering
  - Never called from audio task or ISR paths (UART TX blocks, causes audio dropouts)
  - Level configured at runtime via `applyDebugSerialLevel()`

**Task Monitor:**
- `src/task_monitor.cpp` — FreeRTOS task enumeration, stack watermarks, priority, core affinity
  - Runs on 5s timer in main loop; opt-in via `debugTaskMonitor` (default off)
  - Uses `<esp_private/freertos_debug.h>` + `TaskIterator_t`

**Hardware Stats:**
- Memory, CPU, WiFi RSSI, uptime broadcast every 2s via WebSocket and MQTT
  - Interval: `HARDWARE_STATS_INTERVAL = 2000ms`

## Network Connectivity

**WiFi:**
- Multi-network client (up to 5 saved SSIDs), AP mode, and concurrent STA+AP
  - Via ESP32-C6 co-processor (firmware v2.11.6) on the Waveshare board
  - Client: Arduino `WiFi`, `WiFiClientSecure` in `src/wifi_manager.cpp`
  - AP mode: captive portal with DNS server on port 53 (`DNSServer`)
  - Static IP, subnet, gateway, dual DNS supported per network
  - Power save disabled (`WIFI_PS_NONE`) to prevent packet drops
  - I2C Bus 0 (GPIO 48/54) shares SDIO with WiFi — HAL discovery skips this bus when WiFi is active

**Ethernet:**
- 100Mbps full duplex EMAC + PHY via `ETH.begin()` in `src/eth_manager.cpp`
  - Client: Arduino `ETH` (bundled with Arduino-ESP32)
  - Becomes default route when link up and IP assigned
  - Hostname set to `alx-nova`

## Home Automation

**MQTT / Home Assistant:**
- MQTT broker: user-configurable host/port (default port 1883) stored in NVS
  - Client: `PubSubClient@^2.8` in `src/mqtt_handler.cpp`, dedicated FreeRTOS task (`src/mqtt_task.cpp`) on Core 0, priority 2
  - Buffer size: 1024 bytes (increased for Home Assistant discovery payloads)
  - Reconnect interval: 5s; heartbeat publish: every 60s
  - HA auto-discovery: publishes to `homeassistant/{type}/{deviceId}/{entity}/config` topics on connect and on HA restart (`homeassistant/status` birth message)
  - Entity types discovered: `switch` (amplifier, AP), `select` (mode), `number` (timer, threshold), `sensor` (audio level, timer remaining, RSSI, and more)
  - Device ID: derived from eFuse MAC address via `getMqttDeviceId()`
  - Thread safety: `mqttCallback()` uses dirty flags only — no direct WebSocket/LittleFS/WiFi calls

## CI/CD & Deployment

**Hosting:**
- GitHub Releases — firmware `.bin` distributed as release assets with SHA256 checksums
- Self-hosted on ESP32-P4 device (no cloud hosting for firmware runtime)

**CI Pipeline:**
- GitHub Actions (`.github/workflows/tests.yml`, `.github/workflows/release.yml`)
  - 4 parallel quality gates (all must pass before firmware build proceeds):
    1. `cpp-tests` — `pio test -e native -v` (1271+ Unity tests)
    2. `cpp-lint` — cppcheck on `src/` (`--std=c++11`, excludes `src/gui/`)
    3. `js-lint` — `find_dups.js` + `check_missing_fns.js` + ESLint on `web_src/js/`
    4. `e2e-tests` — Playwright browser tests (26 tests, Chromium)
  - Firmware artifact: `firmware.bin` uploaded for 30 days on successful build
  - Playwright HTML report: uploaded on failure for 14 days
  - Triggers: push/PR to `main` and `develop` branches

**Pre-commit Hooks:**
- `.githooks/pre-commit` — `find_dups.js`, `check_missing_fns.js`, ESLint (fast, JS-only)
- Activate with: `git config core.hooksPath .githooks`

**OTA Update Flow:**
- Device polls GitHub API every 5 minutes (`OTA_CHECK_INTERVAL = 300000ms`) via `startOTACheckTask()` on Core 0
- Download runs in a separate one-shot FreeRTOS task via `startOTADownloadTask()`
- Channels: stable (`/releases/latest`) and beta (`/releases` list including prereleases)
- SHA256 verification before flashing; `Update.h` Arduino OTA library used for flashing
- 15s boot grace period; 30s auto-update countdown; NTP required for SSL cert validation

## Webhooks & Callbacks

**Incoming:**
- WebSocket server port 81 — real-time bidirectional with web UI; binary frames for waveform (0x01) and spectrum (0x02) data; JSON for all other messages
- HTTP server port 80 — REST API under `/api/`, captive portal under `/` in AP mode

**Outgoing:**
- MQTT publishes to configured broker — state, sensor values, HA discovery, heartbeat
- GitHub API requests (HTTPS) — OTA check, release notes, HAL device YAML fetch

## Environment Configuration

**Required runtime configuration (set via web UI, stored in NVS):**
- WiFi SSID(s) and password(s)
- MQTT broker host, port, username, password (optional)
- Web UI password (default set at first boot)

**Required at compile time:**
- All configuration is in `platformio.ini` build flags and `src/config.h` defaults
- GitHub repo owner/name in `src/config.h`: `GITHUB_REPO_OWNER "Schmackos"`, `GITHUB_REPO_NAME "ALX_Nova_Controller_2"`
- No `.env` file exists or is needed

**Secrets location:**
- WiFi passwords, MQTT credentials, web password hash: NVS on device
- GitHub root CA certificates: hardcoded in `src/ota_updater.cpp` (public CAs, not secrets)

---

*Integration audit: 2026-03-07*
