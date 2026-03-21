# External Integrations

**Analysis Date:** 2026-03-21

## APIs & External Services

**GitHub API:**
- Service: GitHub release checking and firmware download
- SDK/Client: `WiFiClientSecure` + `HTTPClient` (native ESP32 Arduino)
- Purpose: OTA firmware updates via `checkForFirmwareUpdate()` in `src/ota_updater.cpp`
- Endpoint: `api.github.com/repos/{GITHUB_REPO_OWNER}/{GITHUB_REPO_NAME}/releases/latest` (Schmackos/ALX_Nova_Controller_2)
- Auth: None (public API for release metadata)
- Rate limit: GitHub public API 60 req/hr per IP
- Certificate validation: Mozilla root CA bundle (automatic via ESP32 cert bundle library)
- Root CAs: Sectigo R46/E46 (2025 migration) + DigiCert G2 for CDN (valid to 2038-2046)

**Home Assistant MQTT Integration:**
- Service: MQTT broker (Home Assistant native support)
- SDK/Client: `PubSubClient@^2.8` library
- Purpose: Device discovery via Home Assistant MQTT protocol (hass.mqtt discovery prefix)
- HA Discovery: Separate JSON manifests published to `homeassistant/{component}/{node_id}/{object_id}/config` (in `src/mqtt_ha_discovery.cpp`)
- Components published: WiFi status, MQTT connection, amplifier relay, audio levels, DSP settings, device temperature

## Data Storage

**Databases:**
- Not applicable - no SQL database

**Local File Storage:**
- LittleFS - Embedded filesystem on ESP32-P4 flash
  - `/config.json` - Primary settings (WiFi, MQTT, audio, DSP, OTA, timezone, auth)
  - `/mqtt_config.txt` - Legacy MQTT settings (fallback, text format)
  - `/hal_config.json` - HAL device per-instance configuration (I2C/I2S/GPIO pin overrides)
  - `/diag_journal.bin` - Persistent diagnostic event ring buffer (800 entries, binary)
  - `/crash_log.txt` - Fatal crash stack trace
  - Client: ArduinoJson for JSON parsing; text parsing for legacy formats

**Flash Partition Layout:**
- `partitions_ota.csv` - OTA partition scheme (dual-slot firmware update, app recovery partition)
- Total flash: 16MB (16 MiB)

**RAM / PSRAM:**
- Internal SRAM: ~330KB worst-case usage (DMA buffers, FreeRTOS, network stack)
- PSRAM: ~77KB audio pipeline buffers + DSP delay lines + FFT buffers
  - Allocations via `heap_caps_calloc(MALLOC_CAP_SPIRAM)` with internal SRAM fallback
  - Heap budget tracking via `heap_budget.h` (per-subsystem allocation recorder)

**NVS (Non-Volatile Storage - Preferences):**
- `wifi-list` namespace: WiFi minimum security level
- `ota-prefs` namespace: OTA release channel selection
- `hal-prefs` namespace: HAL auto-discovery toggle
- Used for settings that must survive LittleFS format (WiFi credentials)

**Caching:**
- No external caching service (Redis, Memcached)
- In-memory state via `AppState` singleton (decomposed into 15 domain-specific headers in `src/state/`)
- Dirty flags for change detection (minimizes WebSocket broadcasts and MQTT publishes)

## Authentication & Identity

**Web UI Authentication:**
- Provider: Custom (on-device)
- Method: PBKDF2-SHA256 (10,000 iterations) via `mbedtls/pkcs5.h`
- Implementation: `src/auth_handler.cpp`
- Password storage: NVS (plaintext password set at first boot on TFT display / serial)
- Session management:
  - HTTP cookies: `sessionId` (HttpOnly flag set, no JS access)
  - Rate limiting: 5-minute lockout after 3 failed attempts
  - WebSocket auth: 60-second TTL tokens (16-slot pool) issued via `GET /api/ws-token`
- Access: HTTP Basic auth via username/password, or cookie session
- Protected endpoints: All REST API endpoints under `/api/*` require authentication

**Device Identity:**
- Serial number: Generated at runtime from ESP32 eFuse MAC address
- Firmware version: Defined in `src/config.h` (1.12.1)

**MQTT Authentication:**
- Broker: Configurable (URL, port, username, password via `/config.json` or web UI)
- Auth: Optional username/password for broker connection
- Client ID: Device serial number (auto-generated from MAC)
- Certificate validation: Configurable via `certValidation` flag (default enabled)

## Monitoring & Observability

**Error Tracking:**
- None (no external service)
- Local: Crash log in `/crash_log.txt` (stack trace on fatal fault)
- Diagnostic journal in `/diag_journal.bin` (800 error/warning events with timestamps)

**Logs:**
- Serial output: `Serial` (UART, 115200 baud)
  - Module-prefixed logs: `[ModuleName]` prefix extraction for frontend filtering
  - Levels: `LOG_D` (debug), `LOG_I` (info), `LOG_W` (warning), `LOG_E` (error)
  - Runtime level control via `applyDebugSerialLevel()` (HTTP API)
- WebSocket forwarding: Live log relay to connected web clients (filtered by level)
- Diagnostic events: `DIAG_*` codes broadcast via WebSocket and persisted to journal

**Metrics / Observability:**
- Hardware stats: heap usage, PSRAM/SRAM breakdown, task monitor (stack watermark), core utilization
- Audio stats: sample rate, ADC input levels (RMS, peak, VU), FFT spectrum, bit depth
- DSP stats: active stages, CPU load percentage, bypass status, preset metadata
- Network stats: WiFi RSSI, MAC address, IP address, gateway, DNS
- MQTT stats: connection state, broker uptime, message publish count
- Device diagnostics: temperature (internal sensor), I2C bus conflict detection, OTA update progress

## CI/CD & Deployment

**Hosting:**
- Embedded HTTP server (port 80) + WebSocket server (port 81) on ESP32-P4
- Web UI served from flash (gzip-compressed static assets embedded in firmware)
- REST API endpoints routed via `WebServer` class

**CI Pipeline:**
- GitHub Actions (`.github/workflows/tests.yml`)
- 4 parallel quality gates (all must pass before firmware build):
  1. cpp-tests: `pio test -e native -v` (Unity, 1732 tests across 75 modules)
  2. cpp-lint: cppcheck on `src/` (static analysis)
  3. js-lint: ESLint + duplicate/missing function checks on web UI
  4. e2e-tests: Playwright browser tests (26 tests across 19 specs)
- Separate `release.yml` workflow for tagged releases (same 4 gates)
- Artifact: Playwright HTML report (14-day retention on failure)

**OTA Updates:**
- Mechanism: GitHub API polling + `Update.begin()` / `Update.write()` / `Update.end()`
- Frequency: Configurable check interval (default 24h)
- Release channels: Stable / Beta (selectable via web UI, persisted to NVS)
- Verification: SHA256 hash comparison (downloaded vs published)
- Resume: On network interrupt, OTA can resume from last successful chunk (Update.seek)
- Rollback: Partition recovery via OTA partition scheme (dual-slot)

## Environment Configuration

**Required Environment Variables:**
- None (all config via web UI or `/config.json`)
- GitHub token NOT required (public API for release checking)

**Secrets Location:**
- Web UI password: NVS (on-device, displayed on first boot via serial or TFT)
- MQTT broker password: `/config.json` (not a secret file, stored plaintext)
- WiFi password: NVS (survives LittleFS format via separate namespace)
- Certificate validation: Configurable flag, uses Mozilla root bundle (no external secrets)

**Configuration Files (Committed to Git):**
- `platformio.ini` - Build configuration (pins, features, library versions)
- `src/config.h` - Compile-time constants (thresholds, ports, feature flags)
- `partition_ota.csv` - Flash partition layout
- `.githooks/pre-commit` - Pre-commit validation (JS linting, function checks)

**No Sensitive Files in Repo:**
- `.env` files: Not used
- Credentials files: Not checked in (secrets managed on-device only)
- API keys: None (public GitHub API, local MQTT)

## Webhooks & Callbacks

**Incoming Webhooks:**
- None exposed

**Outgoing Webhooks:**
- MQTT Home Assistant Discovery: Publish on startup to `homeassistant/{component}/{node_id}/{object_id}/config`
  - Components: Binary sensors (WiFi, MQTT, relay), sensors (audio levels, temp), numbers (volume), switches (amplifier)
  - Format: JSON per MQTT Discovery spec
- MQTT state publishes: Periodic refresh (configurable, default 10s) to `{base_topic}/{component}/{field}`

**Firmware Update Notifications:**
- GitHub API release polling (no webhook push, client-pull model)
- Update available indicated in web UI and MQTT `updateAvailable` flag

## Network Protocols

**WiFi:**
- 802.11a/b/g/n/ac/ax (WiFi 6) via ESP32-C6 co-processor
- Modes: Station (client) + AP (access point with captive portal)
- AP SSID: `ALX-Nova` (with optional password; default no auth for setup)
- Scan: Automatic on boot + manual via `POST /api/wifi/scan`
- Reconnection: Exponential backoff with 30s full-list retry interval
- Multi-network: Stores up to 5 SSIDs, retries last failed network once before scanning all

**Ethernet (ESP32-P4 Native):**
- 100Mbps Fast Ethernet (RMII PHY interface)
- Auto-negotiation (MDI/MDI-X)
- Fallback: If Ethernet disconnects, automatically switches to WiFi
- DHCP + static IP support

**MQTT:**
- Broker: Configurable endpoint (URL + port)
- QoS: 0 (fire-and-forget) for state publishes; QoS 1 for control commands (ack required)
- Client-side task: Core 0, 20 Hz polling, independent of main loop
- HA Discovery: Publishes to `homeassistant/` prefix for auto-discovery
- Reconnect: Automatic with exponential backoff on broker disconnect

**HTTP/REST API:**
- Port: 80
- Endpoints: 40+ under `/api/*` (devices, settings, WiFi, MQTT, DSP, audio, diagnostics)
- Content-Type: JSON (request/response)
- Auth: Required for all endpoints (session cookie or HTTP Basic auth)
- Response codes: 200 (OK), 400 (invalid), 401 (unauthorized), 403 (forbidden), 409 (conflict), 503 (unavailable/heap critical)

**WebSocket:**
- Port: 81
- Protocol: Text frames (JSON state broadcasts) + Binary frames (audio waveform/spectrum)
- Auth: Token-based (60s TTL, obtained from `GET /api/ws-token`)
- Subscriptions: Per-client audio streaming toggle, diagnostic journal filter
- Broadcast interval: Configurable (default 100ms), rate-halved at heap warning state
- Message types: 16 domain broadcasts (WiFi, MQTT, audio, DSP, HAL devices, etc.)

---

*Integration audit: 2026-03-21*
