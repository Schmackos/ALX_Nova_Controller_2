# External Integrations

**Analysis Date:** 2026-03-21

## APIs & External Services

**GitHub API:**
- Service: Release checking and firmware download
- What it's used for: OTA firmware updates via GitHub Releases API
- SDK/Client: WiFiClientSecure + HTTPClient + ArduinoJson (custom REST calls)
- Endpoint: `https://api.github.com/repos/Schmackos/ALX_Nova_Controller_2/releases/latest`
- Auth: No authentication required (public repo)
- TLS: Root CA certificates for api.github.com, github.com, objects.githubusercontent.com (Sectigo R46/E46 RSA/ECC, DigiCert G2 — valid until 2038-2046)
- Implementation: `src/ota_updater.cpp`, `src/ota_updater.h`
- SSL verify: Configurable via `appState.general.enableCertValidation` (default: enabled)

**MQTT Broker (Home Assistant integration):**
- Service: Message broker (MQTT 3.1.1)
- What it's used for: Home Assistant auto-discovery, remote device control, telemetry publishing
- SDK/Client: PubSubClient v2.8
- Configuration:
  - Broker address: User-configurable (stored in `/config.json` or legacy `mqtt_config.txt`)
  - Broker port: Default 1883 (user-editable)
  - Username/Password: Optional, stored in `/config.json`
  - TLS/Encryption: Not implemented (plaintext MQTT only)
- HA Discovery: Sends MQTT discovery payloads on boot to `homeassistant/` prefix
- Published Topics (65+ topics):
  - Device status: `audio/device/name`, `audio/device/version`, `audio/device/uptime`
  - Audio inputs: `audio/input/*/enabled`, `audio/input/*/level`, `audio/input/*/peakLevel`
  - Audio outputs/DAC: `audio/output/*/enabled`, `audio/output/*/volume`, `audio/output/*/mute`, `audio/output/*/clipCount`
  - DSP pipeline: `audio/dsp/enabled`, `audio/dsp/bypass`
  - WiFi: `network/wifi/ssid`, `network/wifi/connected`, `network/wifi/signal`
  - Ethernet: `network/ethernet/connected`, `network/ethernet/ip`
  - Diagnostics: `system/thermal/cpu`, `system/heap/free`, `system/uptime`, `system/errors`
  - Control topics (with `/set` suffix for commands): WiFi mode, DSP bypass, device enable/disable
- Implementation: `src/mqtt_handler.cpp`, `src/mqtt_publish.cpp` (change-detection statics), `src/mqtt_ha_discovery.cpp`
- Task: Dedicated FreeRTOS task on Core 0 (`mqtt_task`) polls at 20 Hz; does NOT wait on event group
- Reconnect: Automatic with exponential backoff; watches `appState._mqttReconfigPending` for broker setting changes

## Data Storage

**Databases:**
- Not applicable — embedded device with no traditional database

**File Storage:**
- **LittleFS (Flash filesystem):**
  - Primary: `/config.json` (main device settings, atomic write via tmp+rename)
  - Fallback: `/settings.txt` (legacy format, loaded on first boot only)
  - Audio pipeline: `/pipeline_matrix.json` (16×16 routing matrix, auto-saved)
  - DSP config: `/dsp_global.json` (DSP settings, debounced save at 2s)
  - HAL devices: `/hal_config.json` (device-specific I2C/GPIO overrides)
  - HAL device database: `/hal_auto_devices.json` (built-in device registry)
  - Signal generator: `/siggen.txt` (waveform parameters)
  - Input channel names: `/inputnames.txt`
  - Smart sensing: `/smartsensing.txt` (auto-off threshold)
  - Diagnostics: `/crashlog.bin` (binary crash event journal, max 256 entries)
  - Legacy: `/mqtt_config.txt` (MQTT settings, skipped if `/config.json` present)
  - Implementation: `src/settings_manager.cpp`, `src/settings_manager.h`, plus individual module save functions
  - Atomic writes: Dual-phase with `.tmp` suffix and rename to prevent corruption
  - Format: JSON (primary), plaintext key=value (legacy)

**EEPROM (Onboard DAC metadata):**
- Devices: External DACs (PCM5102A via I2S, ES8311 via I2C, MCP4725)
- Purpose: Hardware identification, firmware version, calibration data
- Protocol: I2C (address 0x50 for most DAC EEPROMs)
- Implementation: `src/dac_eeprom.h/.cpp`, `src/hal/hal_device_db.cpp`
- API endpoint: `GET/POST /api/dac/eeprom` (read/write raw EEPROM blocks)
- EEPROM v3 format: Standardized header with compatibility string matching

**Non-Volatile Storage (NVS):**
- Implementation: Arduino Preferences API (wrapper around ESP-IDF NVS)
- Stored settings:
  - WiFi credentials: `wifiMinSec` (minimum security level) in namespace `"wifi-list"`
  - OTA settings: `otaChannel` (stable/testing/nightly) in namespace `"ota-prefs"`
  - HAL settings: `halAutoDisco` (auto-discovery enable) in namespace `"hal-prefs"`
  - Device serial: `serial`, `fw_ver` in namespace `"device"`
  - Auth password: PBKDF2 hash in namespace `"auth"` (10k iterations, 100-byte output)
  - Session tokens: Not persisted (ephemeral)
- Survives LittleFS format (separate partition)
- Implementation: `src/auth_handler.cpp`, `src/settings_manager.cpp`, `src/ota_updater.cpp`

**Caching:**
- Change-detection shadows in `mqtt_publish.cpp`: `prevMqtt*` statics (MQTT change detection)
- Change-detection shadows in `smart_sensing.cpp`: `prevBroadcast*` statics (amplifier state, signal detection)
- I2S PIN config cache: `_cachedAdcCfg[2]` statics in `i2s_audio.cpp` (pin override persistence)
- DSP swap double-buffer: `_dspConfigBuffers[2]` in `dsp_pipeline.cpp` (glitch-free atomic swap)
- None persistent across boot — all ephemeral state loss is acceptable

## Authentication & Identity

**Auth Provider:**
- **Custom (internal):**
  - Implementation: PBKDF2-SHA256 (10,000 iterations) password hashing
  - Storage: NVS (Preferences) in namespace `"auth"`, hash stored as 100-byte binary
  - Legacy support: SHA256 plaintext hashes migrated to PBKDF2 on first auth attempt
  - First-boot: Random 8-character alphanumeric password generated, printed to serial + TFT
  - Rate limiting: 5-minute cooldown after 3 failed login attempts (non-blocking gate)
  - Session cookies: HttpOnly, Secure flags (if TLS enabled), 60-minute default expiry
  - WebSocket auth: Short-lived tokens (60s TTL) from `GET /api/ws-token` (16-slot pool, UUID format)
  - Implementation: `src/auth_handler.cpp`, `src/auth_handler.h`
  - Timing-safe comparison: Constant-time string comparison prevents timing attacks

**Device Identity:**
- Serial number: `ALX-{MAC}` (6-byte hex from eFuse, generated at boot, stored in NVS)
- Model: "ALX Audio Controller"
- Firmware version: Semantic versioning (e.g., `1.12.1`, stored in `src/config.h`)

## Monitoring & Observability

**Error Tracking:**
- Not applicable — no external error tracking (embedded device)
- Local crash logging: `/crashlog.bin` (binary event journal, 256-entry ring buffer)
- Implementation: `src/crash_log.h`, `src/diag_journal.h`, `src/diag_event.h`
- Persistent across reboots (stored in LittleFS)

**Logs:**
- **Serial output (115200 baud, COM8):**
  - Level control: Runtime adjustable via debug serial level (LOG_NONE/LOG_ERROR/LOG_WARN/LOG_INFO/LOG_DEBUG)
  - Categories: Module-prefixed (e.g., `[WiFi]`, `[MQTT]`, `[Audio]`, `[HAL]`, `[DSP]`)
  - No logging in real-time paths: ISR and audio task use dirty-flag pattern (task sets flag, main loop logs)
  - Implementation: `src/debug_serial.h`, `src/debug_serial.cpp`

- **WebSocket forwarding:**
  - Sent to all authenticated WS clients on port 81
  - JSON format: `{ "module": "ModuleName", "level": "INFO", "message": "...", "timestamp": "..." }`
  - Category filtering supported on frontend (module chips)
  - Client count guard: `wsAnyClientAuthenticated()` skips broadcast when no clients
  - Implementation: `src/websocket_handler.cpp` `broadcastLine()`

- **Diagnostics/Health:**
  - Audio health: FFT analysis, RMS metering, peak hold, THD+N measurement
  - Device health: Temperature sensor, heap stats, uptime, WiFi signal strength
  - Published via MQTT + WebSocket `healthStatus` broadcast
  - Dashboard: Health Dashboard UI in web frontend with device grid, event log, error counters
  - Implementation: `src/hal/hal_audio_health_bridge.h/.cpp`, `scr_health_dashboard.cpp`

## CI/CD & Deployment

**Hosting:**
- **Device:** Firmware runs on Waveshare ESP32-P4-WiFi6-DEV-Kit (embedded)
- **Web UI:** Served from ESP32 on port 80 (gzip-compressed, embedded in binary)
- **Documentation:** GitHub Pages (`gh-pages` branch, Docusaurus v3)

**OTA Updates:**
- Mechanism: GitHub Releases API polling, firmware binary download with SHA256 verification
- Process: Check → Download → Verify hash → Flash → Reboot
- Task: Dedicated FreeRTOS task on Core 0 (`startOTACheckTask`, `startOTADownloadTask`)
- Partition: OTA partition table (`partitions_ota.csv`) with 2×8MB app slots
- Watchdog: 5-minute download timeout per file with exponential backoff
- User control: Auto-check enable/disable, channel selection (stable/testing/nightly)
- Implementation: `src/ota_updater.cpp`, `src/ota_updater.h`, `src/ota_task.h`

**CI Pipeline:**
- **Trigger:** GitHub Actions on push/PR to `main` and `develop` branches
- **Quality gates (4 parallel):**
  1. `cpp-tests` — Unity framework (~1620 tests across 70 modules): `pio test -e native -v`
  2. `cpp-lint` — cppcheck on `src/` (GUI excluded)
  3. `js-lint` — find_dups.js + check_missing_fns.js + ESLint on `web_src/js/`
  4. `e2e-tests` — Playwright browser tests (26 tests across 19 specs)
- **Build:** Only proceeds if all 4 gates pass
- **Artifact:** Playwright HTML report uploaded on E2E failure (14-day retention)
- **Documentation:** Incremental change detection + Claude API generation + gh-pages deploy
- **Workflows:** `.github/workflows/tests.yml`, `.github/workflows/release.yml`, `.github/workflows/docs.yml`

## Environment Configuration

**Required env vars (secrets in GitHub Actions):**
- `ANTHROPIC_API_KEY` — Used by docs CI for incremental doc generation (gates `generate_docs.js` step)
- No firmware-level env vars — all configuration via `/config.json` or NVS

**Secrets location:**
- GitHub repo settings: `Settings → Secrets and variables → Actions`
- Local development: None required for firmware build; docs generation optional

## Webhooks & Callbacks

**Incoming:**
- None — device cannot receive external webhooks (embedded device behind firewall)
- MQTT commands: Subscribe to `audio/*/set` topics (subscriptions set up in `mqtt_ha_discovery.cpp`)
- HTTP REST API: 40+ endpoints on port 80 (protected by PBKDF2 auth)
- WebSocket commands: 66+ message types received from authenticated web clients (port 81)

**Outgoing:**
- MQTT publishes: 65+ topics to broker (initiated by device state changes + periodic 20 Hz MQTT task)
- HTTP OTA check: Polling GitHub Releases API (~hourly if enabled, configurable)
- HTTP REST requests: Firmware version comparison calls in OTA update flow
- WebSocket broadcasts: Real-time state updates to authenticated clients (port 81)
  - Binary frames: Waveform data (`WS_BIN_WAVEFORM=0x01`), spectrum (`WS_BIN_SPECTRUM=0x02`)
  - JSON frames: State broadcasts (AppState diffs, device updates, health status)
  - Frequency: Event-driven via dirty-flag pattern; idle fallback 5 ms tick

## REST API

**HTTP Server:**
- Port: 80 (main web server)
- Auth: PBKDF2 password (session cookie HttpOnly)
- Endpoints: 40+ (documented in `docs-site/docs/developer/api/`)
  - Audio pipeline CRUD: `GET|PUT /api/pipeline/*`
  - Audio inputs/ADC: `GET /api/audio/*`
  - DAC/outputs: `GET|POST /api/dac/*`
  - DSP configuration: `GET|POST|PUT|DELETE /api/dsp/*`
  - HAL device CRUD: `GET|POST|PUT|DELETE /api/hal/devices*` (13 endpoints)
  - WiFi: `GET|POST /api/wifi/*`
  - Ethernet: `GET /api/ethernet/*`
  - MQTT: `GET|POST /api/mqtt/*`
  - Settings: `GET|POST /api/settings/*`
  - OTA: `GET /api/ota/*`
  - Diagnostics: `GET /api/health/*`, `GET /api/diagnostics/*`
- Implementation: Router lambdas in `src/main.cpp`, plus dedicated API modules (`dac_api.cpp`, `dsp_api.cpp`, `hal/hal_api.cpp`, etc.)

**WebSocket Server:**
- Port: 81
- Auth: Token-based (60s TTL, 16-slot pool from `GET /api/ws-token`)
- Protocol: JSON + binary frames
- Commands: 66+ message types (toggle devices, adjust parameters, request diagnostics)
- Broadcasts: Real-time state diffs, health updates, input/output metering
- Implementation: `src/websocket_handler.cpp` (event router), `web_src/js/02-ws-router.js` (frontend dispatcher)

---

*Integration audit: 2026-03-21*
