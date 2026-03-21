# External Integrations

**Analysis Date:** 2026-03-21

## APIs & External Services

**GitHub API:**
- Service: GitHub Releases API
- What it's used for: OTA firmware update checking and download
- SDK/Client: HTTPClient + WiFiClientSecure (Arduino built-in)
- Implementation: `src/ota_updater.cpp` — Checks `https://api.github.com/repos/Schmackos/ALX_Nova_Controller_2/releases` for latest firmware
- Auth: Public API (no token required; rate-limited to 60 req/hour unauthenticated)
- Artifact format: `firmware.bin` + `SHA256` checksum in release assets

**NTP (Network Time Protocol):**
- Service: pool.ntp.org (public time servers)
- What it's used for: Synchronizing system time for OTA update timestamps and logging
- Implementation: `src/ota_updater.cpp` via `configTime()` (Arduino built-in)
- Auth: None (public, connectionless)

## Data Storage

**Databases:**
- **LittleFS** (Embedded)
  - Location: SPI NOR flash partition (on-device, not external)
  - Files stored:
    - `/config.json` — Primary settings (atomic write via temp+rename)
    - `/mqtt_config.txt` — Legacy MQTT configuration (fallback)
    - `/hal_config.json` — HAL device overrides and custom device schemas
    - `/dsp_*.json` — DSP preset storage (up to `DSP_PRESET_MAX_SLOTS=32`)
    - `/sig_gen_config.txt` — Signal generator settings
    - `/input_names.json` — Custom input channel labels
    - `/crash_log.bin` — Binary crash diagnostics journal
  - Atomic operations: /config.json uses temp file + rename (crash-safe)

- **NVS** (Non-Volatile Storage, via Preferences API)
  - Namespace `"device"` — Device serial number, firmware version
  - Namespace `"auth"` — Web password hash (PBKDF2-SHA256 with 10k iterations)
  - Namespace `"mqtt"` — MQTT broker credentials (persists through LittleFS format)
  - Lifespan: Survives factory reset (independent of LittleFS)

**File Storage:**
- **Local filesystem only** — All config/presets on-device via LittleFS; no cloud sync
- No remote file storage or S3 integration
- DSP convolution IRs loaded from `/dsp_*.json` or user WAV uploads (in-memory processing)

**Caching:**
- None (all config read from LittleFS on startup; no in-memory only caches for persistence)
- PSRAM used for audio buffers (~66 KB) and DSP delay lines (~77 KB), not for config caching

## Authentication & Identity

**Auth Provider:**
- Custom (no external OAuth/SSO)
- Implementation: `src/auth_handler.cpp` — PBKDF2-SHA256 password hashing (10k iterations)

**Session Management:**
- HTTP: Cookie-based (`HttpOnly` flag set, 1-hour TTL, 5-session pool)
- WebSocket: Token-based (`GET /api/ws-token` returns 60s TTL token, 16-slot pool)
- Default password: Random 12-char generated on first boot, printed to serial + TFT GUI

**Password Storage:**
- Hashed with PBKDF2-SHA256 in NVS namespace `"auth"`
- Never stored in plaintext
- Rate-limited login: HTTP 429 on 10+ failed attempts in 15-min window

**First Boot:** `handleFirstBootPassword()` generates random password, displays on UART (115200 baud) + TFT (if GUI enabled)

## Monitoring & Observability

**Error Tracking:**
- **Internal diagnostic journal** (`src/diag_journal.h`) — Binary event log (flash-persisted)
- **Diagnostic codes** (0x01xx–0x11xx range) — 44 defined codes for audio, HAL, system, WiFi, MQTT failures
- **WebSocket broadcast** — Real-time diagnostic events to connected web clients
- **Crash logging** (`src/crash_log.cpp`) — Exception handler captures stack trace to `/crash_log.bin`
- No external error tracking service (Sentry, Rollbar, etc.)

**Logs:**
- **Serial output** — Filtered by `LOG_D` / `LOG_I` / `LOG_W` / `LOG_E` macros
- **Debug level** — Configurable via `/api/debug/serial-level` (CORE_DEBUG_LEVEL in build)
- **Web forwarding** — Serial logs forwarded to WebSocket clients (via `broadcastLine()` in `src/debug_serial.cpp`)
- **Module-prefixed** — Each log line includes `[ModuleName]` prefix for filtering
- No log persistence to file (serial output only, transient)

## CI/CD & Deployment

**Hosting:**
- ESP32-P4 embedded device (no cloud server required)
- Web UI served from embedded HTML/CSS/JS (~350 KB gzip-compressed)
- Entirely self-contained — no external CDN for assets (offline-capable)

**CI Pipeline:**
- GitHub Actions (`.github/workflows/tests.yml`) — 4 parallel gates:
  1. `cpp-tests` — `pio test -e native -v` (~1866 C++ tests)
  2. `cpp-lint` — cppcheck on `src/`
  3. `js-lint` — ESLint + find_dups.js + check_missing_fns.js
  4. `e2e-tests` — Playwright (44 tests against mock Express server)
- OTA deployment via GitHub Releases (user clicks `Install` in web UI)
- Documentation deployment — Docusaurus → GitHub Pages (`.github/workflows/docs.yml`)

**OTA Update Flow:**
1. Device checks `https://api.github.com/repos/Schmackos/ALX_Nova_Controller_2/releases` (configurable interval with backoff)
2. Downloads `firmware.bin` from release asset
3. Verifies SHA256 checksum
4. Calls `Update.begin()` + streaming writes
5. On success: `Update.end()` + reboot into OTA app partition
6. On failure: HTTP 500 response to web UI, no reboot

## Environment Configuration

**Required env vars:**
- None for firmware (all configuration in `/config.json` + NVS)
- For CI: `ANTHROPIC_API_KEY` (optional, for automated doc generation)
- For build: `UPLOAD_PORT` (com port, default COM8) — configurable in `platformio.ini`

**Secrets location:**
- NVS namespace `"auth"` — Web password hash (PBKDF2 with salt)
- NVS namespace `"mqtt"` — MQTT broker username/password
- `.env` file: **Not used** (all secrets in NVS, survives LittleFS format)
- No checked-in credentials or API keys

## Webhooks & Callbacks

**Incoming:**
- MQTT command subscriptions: `audio/*/enabled/set`, `amp/enabled/set`, etc. (for Home Assistant control)
- REST API endpoints: `/api/dsp`, `/api/dac`, `/api/pipeline` (PUT/POST handlers)
- WebSocket commands: JSON-encoded state change requests from web UI

**Outgoing:**
- **MQTT Publish** — Device publishes all state changes to home assistant discovery topics (`homeassistant/*/alx-nova-*`)
- **Home Assistant Integration** — Automatic device discovery via MQTT (auto-generated entity IDs)
- **GitHub Releases** — OTA check polls releases API (non-blocking, configurable interval with exponential backoff)

**No webhooks to external services** — All callbacks are internal (cross-module dirty flags, FreeRTOS event group)

## MQTT Topics (Home Assistant)

**Publisher (device → broker):**
- `audio/adc{1,2}/enabled` — ADC enable state
- `audio/dac/enabled` — DAC enable state
- `audio/usb/connected`, `.streaming`, `.enabled`, `.volume` — USB audio status
- `amp/enabled` — Amplifier relay state
- `system/mode` — Device mode/state
- `system/uptime`, `.heapFree`, `.psramFree` — Hardware telemetry
- `wifi/rssi`, `.ssid`, `.quality` — WiFi signal metrics
- `audio/level/input{1-4}`, `.output{1-8}` — Metering (VU, RMS, peak)

**Subscriber (broker → device):**
- `audio/adc{1,2}/enabled/set` — Enable/disable ADC
- `audio/dac/enabled/set` — Enable/disable DAC
- `audio/usb/enabled/set` — Enable/disable USB audio
- `amp/enabled/set` — Relay control
- `system/updateCheck` — Trigger manual OTA check
- `system/reboot` — Trigger device reboot
- `dsp/*/config` — DSP preset load (if DSP_ENABLED)

**Home Assistant Auto-Discovery:**
- Publishes device definition to `homeassistant/switch/alx-nova-{feature}/config`
- Auto-creates Home Assistant entities for switches, sensors, buttons
- Discovery message includes entity IDs, icons, topics
- Home Assistant subscribes to discovery topics at startup

## SSL/TLS & Certificates

**Trust Store:**
- **ESP32CertBundle** — Automatic Mozilla CA bundle (built into Arduino core)
- No manual certificate pinning or custom CA chains
- Validates GitHub API HTTPS, NTP over UDP (no validation), OTA firmware downloads

**Authentication:**
- MQTT over TLS: Optional (configurable, defaults to unencrypted on port 1883)
- WebSocket over HTTPS: Not implemented (WebSocket on plaintext port 81)
- Web UI auth: Cookie-based (HttpOnly, Secure flag omitted for local HTTP on port 80)

---

*Integration audit: 2026-03-21*
