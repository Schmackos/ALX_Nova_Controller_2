# External Integrations

**Analysis Date:** 2026-03-22

## APIs & External Services

**GitHub API:**
- Service: GitHub Releases API (REST v3)
- What it's used for: OTA firmware update checking and binary download
- SDK/Client: HTTPClient + WiFiClientSecure (Arduino built-in)
- Implementation: `src/ota_updater.cpp`
- Endpoint: `https://api.github.com/repos/Schmackos/ALX_Nova_Controller_2/releases`
- Auth: Public API (no token; rate-limited to 60 req/hour unauthenticated)
- Artifact format: `firmware.bin` + `SHA256` checksum file in release assets
- Config: Repo owner/name in `src/config.h` (`GITHUB_REPO_OWNER`, `GITHUB_REPO_NAME`)

**NTP (Network Time Protocol):**
- Service: pool.ntp.org (public time servers)
- What it's used for: Synchronizing system time for OTA timestamps and logging
- Implementation: `src/ota_updater.cpp` via `configTime()` (Arduino built-in)
- Auth: None (public, connectionless UDP)

## Data Storage

**Databases:**
- **LittleFS** (Embedded flash filesystem)
  - Location: `spiffs` partition on 16 MB SPI NOR flash (7.875 MB allocated)
  - Partition config: `partitions_ota.csv`
  - Files stored:
    - `/config.json` — Primary settings (atomic write via temp+rename) — `src/settings_manager.cpp`
    - `/hal_config.json` — HAL device overrides and custom device schemas — `src/hal/hal_settings.cpp`
    - `/mqtt_config.txt` — Legacy MQTT configuration (fallback, read-only after first boot)
    - `/settings.txt` — Legacy settings format (fallback only)
    - `/dsp_*.json` — DSP preset storage (up to `DSP_PRESET_MAX_SLOTS=32`) — `src/dsp_pipeline.cpp`
    - `/sig_gen_config.txt` — Signal generator settings
    - `/input_names.json` — Custom input channel labels
    - `/diag_journal.bin` — Binary diagnostic event ring buffer (800 entries, 64KB) — `src/diag_journal.h`
    - `/crash_log.bin` — Binary crash diagnostics — `src/crash_log.cpp`
  - Atomic operations: `/config.json` uses temp file + rename (crash-safe)
  - Journal config: `DIAG_JOURNAL_MAX_ENTRIES=800`, `DIAG_FLUSH_INTERVAL_MS=60000` in `src/config.h`

- **NVS** (Non-Volatile Storage, via Arduino Preferences API)
  - Namespace `"device"` — Device serial number (eFuse MAC derived), firmware version
  - Namespace `"auth"` — Web password hash (PBKDF2-SHA256, 50k iterations current / 10k legacy)
  - Namespace `"mqtt"` — MQTT broker credentials (persist through LittleFS format)
  - Partition: 20 KB at 0x9000 (`partitions_ota.csv`)
  - Survives factory reset independently of LittleFS

**File Storage:**
- Local filesystem only — All config/presets on-device via LittleFS
- No remote file storage, cloud sync, or S3 integration
- DSP convolution IRs loaded from LittleFS JSON or user WAV uploads (in-memory processing)

**Caching:**
- None for persistence — All config read from LittleFS on startup
- PSRAM used for runtime audio buffers (~330 KB) and DSP delay lines, not config caching
- Internal SRAM used for DMA buffers (16 x 2KB = 32KB, eagerly pre-allocated at boot)

## Authentication & Identity

**Auth Provider:**
- Custom implementation (no external OAuth/SSO/LDAP)
- Implementation: `src/auth_handler.cpp`
- Hashing: PBKDF2-SHA256 via mbedtls (`<mbedtls/pkcs5.h>`)
  - Current format (`p2:`): 50,000 iterations (`PBKDF2_ITERATIONS` in `src/config.h`)
  - Legacy format (`p1:`): 10,000 iterations (`PBKDF2_ITERATIONS_V1`), auto-migrated on login
- Timing-safe comparison: `timingSafeCompare()` prevents timing attacks

**Session Management:**
- HTTP: Cookie-based (`HttpOnly` flag, 1-hour TTL, 5-session pool `MAX_SESSIONS`)
- WebSocket: Token-based (`GET /api/ws-token` returns UUID token, 60s TTL, 16-slot pool `WS_TOKEN_SLOTS`)
- Default password: Random 12-char generated on first boot, printed to serial + TFT GUI

**Rate Limiting:**
- Login: HTTP 429 on failed attempts, 5-minute cooldown (`LOGIN_COOLDOWN_US = 300000000`)
- Implementation: `_loginFailCount` + `_nextLoginAllowedMs` in `src/auth_handler.cpp`

**HTTP Security Headers:**
- `X-Frame-Options: DENY` — Clickjacking prevention
- `X-Content-Type-Options: nosniff` — MIME-sniffing prevention
- Applied via `http_add_security_headers()` in `src/http_security.h`

## Monitoring & Observability

**Error Tracking:**
- **Internal diagnostic journal** (`src/diag_journal.h`) — Binary ring buffer (32 hot entries in PSRAM + 800 persistent on LittleFS)
- **Diagnostic error codes** (`src/diag_error_codes.h`) — 50+ codes across 7 subsystems (0x01xx System, 0x10xx HAL General, 0x11xx HAL Discovery, 0x20xx Audio, 0x30xx DSP, 0x40xx WiFi, 0x50xx MQTT)
- **WebSocket broadcast** — Real-time diagnostic events pushed to connected web clients via `sendDiagEvent()`
- **REST API** — `GET /api/diag/snapshot` returns current diagnostic state + heap budget
- **Crash logging** (`src/crash_log.cpp`) — Exception handler captures stack trace to `/crash_log.bin`
- **Coredump partition** — 64 KB at 0xFF0000 for ESP-IDF core dumps
- No external error tracking service (no Sentry, Rollbar, etc.)

**Logs:**
- **Serial output** — Filtered by `LOG_D`/`LOG_I`/`LOG_W`/`LOG_E` macros (`src/debug_serial.h`)
- **Runtime level control** — Configurable via `applyDebugSerialLevel()` and REST API
- **Web forwarding** — Serial logs forwarded to WebSocket clients via `broadcastLine()` in `src/debug_serial.cpp`
- **Module-prefixed** — Each log line includes `[ModuleName]` prefix for category filtering
- **Frontend filtering** — Debug Console in web UI: module chip filtering, search/highlight, entry count badges
- No log persistence to file (serial output only, transient)

**Health Monitoring:**
- **Heap pressure** — 3-state graduated (Normal / Warning <50KB / Critical <40KB) — `src/config.h` thresholds
- **PSRAM pressure** — 3-state graduated (Normal / Warning <1MB / Critical <512KB)
- **Task monitor** — FreeRTOS task enumeration (stack usage, priority, core) — `src/task_monitor.cpp`
- **Audio health** — Per-ADC health status (OK/NO_DATA/NOISE_ONLY/CLIPPING/HW_FAULT) — `src/i2s_audio.cpp`
- **HAL health** — Per-device `healthCheck()` with flapping detection — `src/hal/hal_device_manager.cpp`
- **PSRAM tracking** — `psram_get_stats()` lifetime fallback/failure counts — `src/psram_alloc.cpp`
- **Heap budget** — Per-subsystem allocation tracker (32 entries) — `src/heap_budget.cpp`
- **REST endpoints** — `GET /api/psram/status`, `GET /api/diag/snapshot`
- **WebSocket** — `hardwareStats` broadcast includes heap/PSRAM/budget telemetry

## CI/CD & Deployment

**Hosting:**
- ESP32-P4 embedded device (no cloud server)
- Web UI served from embedded gzip-compressed HTML/CSS/JS (`src/web_pages_gz.cpp`)
- Entirely self-contained — no external CDN, offline-capable

**CI Pipeline (GitHub Actions):**
- `.github/workflows/tests.yml` — 5 parallel quality gates on push/PR to main/develop:
  1. `cpp-tests` — `pio test -e native -v` (~2316 tests, Ubuntu, Python 3.11)
  2. `cpp-lint` — cppcheck on `src/` (excludes `src/gui/`)
  3. `js-lint` — ESLint + find_dups.js + check_missing_fns.js + diagram-validation.js
  4. `e2e-tests` — Playwright (107 tests, Chromium, Node 20)
  5. `doc-coverage` — `node tools/check_mapping_coverage.js`
- `build` job runs after all 5 gates pass — `pio run -e esp32-p4`, uploads firmware artifact (30-day retention)
- Playwright HTML report uploaded as artifact on failure (14-day retention)

**Release Pipeline:**
- `.github/workflows/release.yml` — Manual workflow_dispatch
- Inputs: version bump (patch/minor/major), channel (stable/beta)
- Runs same 5 quality gates, then builds and creates GitHub Release with `firmware.bin`

**Documentation Pipeline:**
- `.github/workflows/docs.yml` — Docusaurus build + deploy to GitHub Pages
- Requires `ANTHROPIC_API_KEY` for automated doc regeneration via `tools/generate_docs.js`

**OTA Update Flow:**
1. Device checks GitHub Releases API (configurable interval: `OTA_CHECK_INTERVAL = 300000` ms, with exponential backoff)
2. Compares release tag against `FIRMWARE_VERSION` in `src/config.h`
3. Downloads `firmware.bin` from release asset over HTTPS (ESP32CertBundle for TLS)
4. Verifies SHA256 checksum against release checksum file
5. Streams firmware via `Update.begin()` to OTA partition (`app1`)
6. On success: `Update.end()` + reboot into new partition
7. On failure: HTTP 500 to web UI, no reboot, diagnostic event emitted

## Environment Configuration

**Required env vars:**
- None for firmware (all configuration in `/config.json` + NVS on-device)
- `ANTHROPIC_API_KEY` — Optional, for CI automated doc generation (`tools/generate_docs.js`)
- `UPLOAD_PORT` — COM port for firmware upload (default COM8, in `platformio.ini`)

**Secrets location:**
- NVS namespace `"auth"` — Web password hash (PBKDF2-SHA256 with salt)
- NVS namespace `"mqtt"` — MQTT broker username/password
- `.env` file: **Not used** — All secrets in NVS, survives LittleFS format
- No checked-in credentials or API keys in the repository

## Webhooks & Callbacks

**Incoming (device receives):**
- MQTT command subscriptions: `audio/*/enabled/set`, `amp/enabled/set`, `system/reboot`, etc. — `src/mqtt_handler.cpp`
- REST API endpoints: `/api/dsp`, `/api/dac`, `/api/pipeline`, `/api/hal/*` (PUT/POST/DELETE handlers)
- WebSocket commands: JSON-encoded state change requests from web UI on port 81

**Outgoing (device sends):**
- MQTT state publishes: Audio levels, device mode, telemetry — `src/mqtt_publish.cpp`
- Home Assistant auto-discovery: `homeassistant/*/alx-nova-*/config` topics — `src/mqtt_ha_discovery.cpp`
- GitHub Releases API polls: OTA check (non-blocking, configurable interval) — `src/ota_updater.cpp`
- WebSocket broadcasts: Real-time state, audio data, diagnostics to connected clients — `src/websocket_handler.cpp`

**No outgoing webhooks to external services** — All callbacks are internal (dirty flags, FreeRTOS event group `src/app_events.h`)

## MQTT Integration (Home Assistant)

**Client:** PubSubClient 2.8 (`src/mqtt_handler.cpp`)
**Task:** Dedicated FreeRTOS task on Core 0, priority 2 (`src/mqtt_task.cpp`)
**Reconnect:** 5-second interval (`MQTT_RECONNECT_INTERVAL`), socket timeout 5s (`MQTT_SOCKET_TIMEOUT_MS`)
**TLS:** Optional (configurable, defaults to unencrypted port 1883)

**Publisher (device -> broker):**
- `audio/adc{1,2}/enabled` — ADC enable state
- `audio/dac/enabled` — DAC enable state
- `audio/usb/connected`, `.streaming`, `.enabled`, `.volume` — USB audio status
- `amp/enabled` — Amplifier relay state
- `system/mode` — Device FSM state
- `system/uptime`, `.heapFree`, `.psramFree` — Hardware telemetry
- `wifi/rssi`, `.ssid`, `.quality` — WiFi signal metrics
- `audio/level/input{1-4}`, `.output{1-8}` — Metering (VU, RMS, peak)

**Subscriber (broker -> device):**
- `audio/adc{1,2}/enabled/set` — Enable/disable ADC
- `audio/dac/enabled/set` — Enable/disable DAC
- `audio/usb/enabled/set` — Enable/disable USB audio
- `amp/enabled/set` — Relay control
- `system/updateCheck` — Trigger manual OTA check
- `system/reboot` — Trigger device reboot
- `dsp/*/config` — DSP preset load (if `DSP_ENABLED`)

**Home Assistant Auto-Discovery:**
- Publishes device config to `homeassistant/switch/alx-nova-{feature}/config`
- Auto-creates HA entities: switches, sensors, buttons
- Discovery messages include entity IDs, icons, command/state topics
- Implementation split across: `src/mqtt_handler.cpp`, `src/mqtt_publish.cpp`, `src/mqtt_ha_discovery.cpp`

## SSL/TLS & Certificates

**Trust Store:**
- **ESP32CertBundle** — Automatic Mozilla CA bundle (built into Arduino core)
- No manual certificate pinning or custom CA chains
- Used for: GitHub API HTTPS, OTA firmware downloads

**Transport Security:**
- MQTT over TLS: Optional (configurable per-broker, defaults to unencrypted port 1883)
- WebSocket: Plaintext on port 81 (no WSS)
- Web UI: HTTP on port 80 (no HTTPS — local network device)
- Cookie: `HttpOnly` flag set, `Secure` flag omitted (HTTP-only transport)

## Network Interfaces

**WiFi:**
- ESP32-C6 co-processor via SDIO interface to P4 host
- Multi-network client with AP mode fallback
- I2C Bus 0 (GPIO 48/54) shares SDIO pins — disabled when WiFi active
- Implementation: `src/wifi_manager.cpp`

**Ethernet:**
- 100 Mbps full-duplex via EMAC + RTL8201F PHY
- Implementation: `src/eth_manager.cpp`
- Dual-stack: WiFi and Ethernet can coexist

**USB:**
- Native USB OTG on ESP32-P4
- TinyUSB UAC2 speaker device (guarded by `USB_AUDIO_ENABLED`)
- Lock-free ring buffer (1024 frames, PSRAM)
- Implementation: `src/usb_audio.cpp`

---

*Integration audit: 2026-03-22*
