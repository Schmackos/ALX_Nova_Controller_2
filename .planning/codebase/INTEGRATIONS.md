# External Integrations

**Analysis Date:** 2026-03-22

## APIs & External Services

**GitHub Repository API:**
- **Service**: GitHub Releases API
- **What it's used for**: Automatic firmware update checking and release notes retrieval
  - Client: `HTTPClient` (Arduino library)
  - Authentication: None (public GitHub API)
  - Endpoints:
    - `https://api.github.com/repos/Schmackos/ALX_Nova_Controller_2/releases/latest` — Get latest release
    - `https://api.github.com/repos/Schmackos/ALX_Nova_Controller_2/releases?per_page=5` — Get release list
  - File: `src/ota_updater.cpp` (functions `getLatestReleaseInfo()`, `fetchReleaseList()`)
  - Auth field: `GITHUB_REPO_OWNER` and `GITHUB_REPO_NAME` in `src/config.h`

**GitHub CDN / Raw Content:**
- **Service**: GitHub raw content API / GitHub Pages CDN
- **What it's used for**: User manual and documentation links
  - URLs: `https://raw.githubusercontent.com/Schmackos/ALX_Nova_Controller_2/main/USER_MANUAL.md`
  - File: `src/web_pages.cpp` (embedded in web UI)
  - Load type: On-demand (user clicks "View Manual")

**jsDelivr CDN:**
- **Service**: Public npm CDN (jsDelivr)
- **What it's used for**: Optional JavaScript libraries loaded on-demand in web UI
  - QR Code Library: `https://cdn.jsdelivr.net/npm/qrcodejs@1.0.0/qrcode.min.js` (for WiFi QR code generation)
  - Markdown Parser: `https://cdn.jsdelivr.net/npm/marked/marked.min.js` (for release notes rendering)
  - File: `src/web_pages.cpp` (embedded JS in HTML)
  - Load type: On-demand (lazy-loaded when user navigates to WiFi or OTA screens)

## Data Storage

**Databases:**
- Not used. No remote database integration.

**File Storage:**

| Storage | Provider | Purpose | Location in Code |
|---------|----------|---------|------------------|
| **LittleFS** | Onboard SPI flash (8MB partition) | Configuration files, DSP presets, diagnostic journal, crash logs | `LittleFS.open()` throughout codebase |
| **NVS** | Onboard flash (ESP-IDF5 Preferences) | Serial number, firmware version tracking, WiFi credentials | `Preferences` class in `src/main.cpp`, `src/wifi_manager.cpp` |
| **EEPROM** (Expansion) | AT24C02 (I2C 0x50-0x57) on mezzanine modules | Device identification and EEPROM v3 format for expansion DAC/ADC modules | `src/dac_eeprom.h`, `src/hal/hal_discovery.cpp` |

**Caching:**
- WebSocket state cache: Dirty flags in `AppState` (`src/app_state.h`) for selective broadcasts
- Memory-only: No persistent cache backend

**Filesystem Structure (LittleFS):**
- `/config.json` — Primary settings file (WiFi, MQTT, DAC, DSP, system config)
- `/mqtt_config.txt` — Legacy MQTT settings (fallback, auto-migrated to JSON)
- `/signal_gen_presets.json` — Signal generator waveform configs
- `/dsp_presets/` — DSP filter chain presets (directory)
- `/hal_config.json` — HAL device configurations (I2C pins, I2S routing, custom overrides)
- `/diag_journal.bin` — Diagnostic event ring buffer (800 entries, 64KB)
- `/crashlog.bin` — Last crash details (binary format)

## Authentication & Identity

**Auth Provider:**
- Custom HTTP Basic Auth (no external OAuth)
  - Implementation: PBKDF2-SHA256 password hashing (50k iterations, `p2:` format)
  - File: `src/auth_handler.cpp`
  - Backward-compatible with legacy `p1:` (10k iterations) and SHA256 hashes — auto-migrated on login
  - Credentials: Stored in `/config.json` under `auth.password_hash`

**WebSocket Token Auth:**
- Custom short-lived bearer tokens (60s TTL)
- Token pool: 16 simultaneous tokens (tracked in `WebSocketsServer` context)
- File: `src/websocket_handler.cpp` (functions `wsAuthStatus[]`, `wsAuthTimeout[]`)
- Generation: `GET /api/ws-token` endpoint

**Serial Number Generation:**
- Derived from ESP32-P4 eFuse MAC address + firmware version
- Format: `ALX-XXXXXX` (6 MAC bytes, uppercase hex)
- Storage: NVS namespace `"device"` via `Preferences` class
- Regeneration: On firmware version change
- File: `src/main.cpp` (function `initSerialNumber()`)

## Monitoring & Observability

**Error Tracking:**
- **Diagnostic Journal**: In-memory ring buffer (800 entries max) persisted to LittleFS at `/diag_journal.bin`
- **Crash Log**: Last crash details saved to `/crashlog.bin` (binary format)
- **Structured logging**: `debug_serial.h` macros with `[ModuleName]` prefixes sent to serial and WebSocket
- Files: `src/diag_journal.h`, `src/diag_event.h`, `src/crash_log.h`

**Logs:**
- **Serial Output**: UART at 115200 baud via `debug_serial.h` macros (`LOG_D`, `LOG_I`, `LOG_W`, `LOG_E`)
- **WebSocket Broadcast**: Real-time log lines forwarded to web UI with category/level filtering
- **Storage**: None (logs are ephemeral, except diagnostic journal)
- Files: `src/debug_serial.cpp`, `src/websocket_handler.cpp`

## CI/CD & Deployment

**Hosting:**
- **Self-hosted**: Embedded web server on ESP32-P4 (HTTP port 80, WebSocket port 81)
- **No cloud hosting**: Entire UI and API run on-device

**CI Pipeline:**
- **GitHub Actions** (`.github/workflows/tests.yml`)
  - 4 parallel quality gates: `cpp-tests` (Unity), `cpp-lint` (cppcheck), `js-lint` (ESLint/find_dups), `e2e-tests` (Playwright)
  - Triggers: Push/PR to `main` and `develop` branches
  - Release workflow: Same 4 gates before release
  - Reports: Playwright HTML artifact on failure (14-day retention)

**OTA Updates:**
- Non-blocking FreeRTOS task on Core 0
- Download verification: SHA256 hash check against hardcoded value in release notes
- Rollback: OTA success flag (previous version stored, reverted on boot failure)
- Files: `src/ota_updater.cpp`, `src/ota_updater.h`, `src/ota_certs.h`

## Environment Configuration

**Required Environment Variables:**
- None for production device operation
- **Development/CI only**:
  - `ANTHROPIC_API_KEY` — For documentation site regeneration via Claude API

**Secrets Location:**
- Production: Device stores only `/config.json` on LittleFS (contains WiFi SSID/password, MQTT credentials, auth hash)
- CI/CD: GitHub Actions secrets (none currently documented in workflow; API key is optional)
- Development: `.env` files ignored by `.gitignore`

**Configuration Sources:**
- Web UI: `GET /api/settings` and `PUT /api/settings` REST endpoints
- Settings file: `/config.json` (primary) or legacy `/mqtt_config.txt`
- HAL configuration: `/hal_config.json` (device-specific I2C/I2S overrides)

## Webhooks & Callbacks

**Incoming Webhooks:**
- None. Device does not expose incoming webhooks.

**Outgoing Webhooks (MQTT):**

| Topic | Format | Triggered By |
|-------|--------|--------------|
| `audio/status` | JSON | Audio pipeline state changes |
| `smartsensing/amplifier` (set) | ON/OFF | User command (Web UI / Home Assistant) |
| `wifi/status` | JSON | WiFi connection state change |
| `mqtt/status` | JSON | MQTT connection state change |
| `system/hardware_stats` | JSON | Periodic 5s tick (heap, temperature, core usage) |
| `system/diagnostics` | JSON | Diagnostic events (errors, warnings) |
| `audio/usb/*` (conditional) | JSON | USB Audio state (guarded by `-D USB_AUDIO_ENABLED`) |
| `audio/dsp/*` (conditional) | JSON | DSP pipeline metrics (guarded by `-D DSP_ENABLED`) |

**MQTT Broker Connection:**
- **Protocol**: MQTT 3.1.1 over TLS (if broker URL starts with `mqtt://` or `mqtts://`)
- **Auth**: Username/password (stored encrypted in `/config.json`)
- **Client**: PubSubClient v2.8 (`src/mqtt_handler.cpp`)
- **Task**: Dedicated FreeRTOS task on Core 0 (file: `src/mqtt_task.cpp`) polling at 20 Hz
- **Socket Timeout**: Capped at 5s via `mqttWifiClient.setTimeout(MQTT_SOCKET_TIMEOUT_MS)` to prevent blocking on unreachable brokers
- **Availability Topic**: Device publishes online/offline status to `{base_topic}/status` for Home Assistant availability

**Home Assistant MQTT Auto-Discovery:**
- **Type**: Home Assistant MQTT Discovery protocol (homeassistant/ prefix)
- **Coverage**: ~100+ entity configs (switches, sensors, buttons, number inputs, binary sensors, select entities)
- **Generic HAL Devices**: `binary_sensor` availability entities auto-published for new expansion DAC/ADC modules
- **File**: `src/mqtt_ha_discovery.cpp` (functions `publishHADiscovery()`, `removeHADiscovery()`)
- **Triggers**: MQTT connects, HAL device discovery, firmware boot

## REST API Endpoints

**All endpoints served on HTTP port 80, `/api/` prefix:**

| Endpoint | Method | Purpose | Auth | File |
|----------|--------|---------|------|------|
| `/api/settings` | GET/PUT | Load/save device configuration | Required | `src/settings_manager.cpp` |
| `/api/ws-token` | GET | Generate short-lived WebSocket token (60s) | Required | `src/websocket_handler.cpp` |
| `/api/hal/devices` | GET/POST/PUT/DELETE | HAL device CRUD and reinit | Required | `src/hal/hal_api.cpp` |
| `/api/hal/scan` | POST | Discover I2C/EEPROM devices (409 guard) | Required | `src/hal/hal_api.cpp` |
| `/api/hal/db` | GET | HAL device database presets | Required | `src/hal/hal_api.cpp` |
| `/api/dsp/*` | GET/POST/DELETE | DSP filter chain CRUD (guarded by `-D DSP_ENABLED`) | Required | `src/dsp_api.cpp` |
| `/api/dac/*` | GET/PUT | DAC state, volume, enable/disable (guarded by `-D DAC_ENABLED`) | Required | `src/dac_api.cpp` |
| `/api/pipeline/*` | GET/POST/PUT | Audio pipeline matrix routing CRUD | Required | `src/pipeline_api.cpp` |
| `/api/diag/snapshot` | GET | Diagnostic snapshot (events, heap, PSRAM, task status) | Required | `src/diag_journal.cpp` |
| `/api/psram/status` | GET | PSRAM health and allocation tracking | Required | `src/psram_api.cpp` |
| `/api/ota/check` | GET | Check for firmware updates | Required | `src/ota_updater.cpp` |
| `/api/ota/start` | POST | Start OTA download | Required | `src/ota_updater.cpp` |
| `/api/reboot` | POST | Reboot device | Required | `src/settings_manager.cpp` |
| `/api/factory-reset` | POST | Factory reset device | Required | `src/settings_manager.cpp` |

**Security:**
- All API endpoints require HTTP Basic Auth (password hash in `/config.json`)
- Responses include security headers: `X-Frame-Options: DENY`, `X-Content-Type-Options: nosniff` (via `http_security.h`)
- Rate limiting: HTTP 429 on repeated failed auth attempts

## WebSocket Protocol (Port 81)

**Authentication:**
- Token-based: `GET /api/ws-token` returns a 60-second bearer token (JSON: `{"token": "..."}`)
- Client sends token in first frame: `{"type": "auth", "token": "..."}`
- Server validates and marks client as authenticated (stored in `wsAuthStatus[]`)

**Message Types (JSON):**
- `updateStatus` — OTA check/download progress
- `displayState` — GUI screen/control state
- `hardwareStats` — System metrics (heap, temperature, core usage, PSRAM)
- `dspState` / `dspMetrics` — DSP filter chain state and CPU usage
- `halDeviceState` — HAL device capabilities and lifecycle
- `usbAudioState` — USB audio connection state and stats
- `debugLog` — Structured debug logs with `[ModuleName]` prefixes

**Message Types (Binary):**
- `0x01` — Audio waveform (256-sample I16 chunks per ADC)
- `0x02` — FFT spectrum (16 frequency bins as float32 LE)

**Broadcast Rate Adaptation:**
- Client count: 1 (skip factor 1), 2 (skip 2), 3-4 (skip 4), 5-7 (skip 6), 8+ (skip 8)
- Heap pressure: Binary data suppressed at 40KB critical threshold
- Max clients: 16 (`WEBSOCKETS_SERVER_CLIENT_MAX`)

---

*Integration audit: 2026-03-22*
