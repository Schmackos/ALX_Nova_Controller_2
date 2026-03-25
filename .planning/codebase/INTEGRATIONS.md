# External Integrations

**Analysis Date:** 2026-03-25

## APIs & External Services

**GitHub API (OTA Updates):**
- Service: GitHub Releases API (`api.github.com`)
- Purpose: Firmware OTA update checking, version comparison, release notes, binary download
- SDK/Client: `HTTPClient` (Arduino) + `WiFiClientSecure`
- Auth: None (public API, rate limited by IP)
- Endpoints consumed:
  - `GET /repos/{owner}/{repo}/releases/latest` — Latest release info
  - `GET /repos/{owner}/{repo}/releases?per_page=5` — Release list
  - `GET /repos/{owner}/{repo}/releases/tags/{version}` — Specific release
- Binary download: HTTPS from `objects.githubusercontent.com` with SHA256 verification
- Certificate chain: Sectigo R46/E46 + DigiCert G2 embedded in `src/ota_certs.h`
- Check interval: 5 min default (`OTA_CHECK_INTERVAL` in `src/config.h`), exponential backoff on failure
- Implementation: `src/ota_updater.cpp`, `src/ota_updater.h`
- Timeouts: Connect 20s, stall 20s, per-read 5s (all < 30s TWDT)

**GitHub Web URLs (Frontend-only):**
- User manual: `https://raw.githubusercontent.com/{owner}/{repo}/main/USER_MANUAL.md`
- Custom device contribution: `https://github.com/{owner}/{repo}/issues/new`
- Documentation: `https://github.com/{owner}/{repo}/blob/main/`
- Repo config: `GITHUB_REPO_OWNER`/`GITHUB_REPO_NAME` in `src/config.h`

**NTP (Time Synchronization):**
- Protocol: SNTP via ESP-IDF
- Purpose: System time for logging timestamps, OTA version date comparison
- Implementation: `syncTimeWithNTP()` in `src/ota_updater.cpp`
- Triggered: After WiFi connection established

## Data Storage

**On-Device Filesystems:**

| Storage | Type | Purpose |
|---------|------|---------|
| LittleFS | Flash filesystem (~7.9MB) | Settings, HAL config, diagnostics, custom devices |
| NVS | Key-value (20KB) | WiFi credentials, password hash, OTA channel, HAL prefs |

**LittleFS Files:**
- `/config.json` — Primary user settings (audio, display, smart sensing, network)
- `/hal_config.json` — HAL device configurations (pin overrides, I2C addresses, sample rates)
- `/hal/custom/*.json` — User-created custom device schemas (Tier 1-3)
- `/diag_journal.bin` — Persistent diagnostic ring buffer (800 entries max, CRC32 per entry)
- `/dsp_preset_*.json` — DSP configuration presets (up to 32 slots)
- `/matrix.json` — Audio routing matrix state
- `/siggen.json` — Signal generator settings
- `/input_names.json` — User-defined input channel labels
- Legacy files (auto-migrated on first boot): `mqtt_config.txt`, `settings.txt`

**NVS Namespaces:**
- `device` — Serial number, firmware version
- `wifi` — Up to 5 stored network credentials (SSID, password, static IP config)
- `mqtt` — MQTT broker config (migrated from file)
- `hal` — Auto-discovery toggle, HAL preferences
- `diag` — Monotonic sequence counter, boot ID

**Caching:**
- DSP double-buffering: Active + inactive config, atomic swap via `dsp_swap_config()`
- ASRC coefficient table: 5120 floats in flash (const, not PSRAM)
- HAL device DB: In-memory cache loaded from `hal_device_db.cpp`
- WS binary rate scaling: Adaptive frame skip based on authenticated client count

**File Storage:**
- Local filesystem only (no remote/cloud storage)

## Authentication & Identity

**Web UI Authentication (`src/auth_handler.h/.cpp`):**
- Method: PBKDF2-SHA256 password hashing
- Iterations: 50,000 (`p2:` format), auto-migrates from legacy 10,000 (`p1:`) and raw SHA256
- Cookie: HttpOnly, session-based, 1 hour timeout (`SESSION_TIMEOUT_US`)
- Max sessions: 5 concurrent (`MAX_SESSIONS`)
- Rate limiting: 5 login attempts/minute (disabled when `TEST_MODE` defined)
- Password management: Default generated from MAC, change via `POST /api/auth/password`
- Timing-safe comparison: `timingSafeCompare()` prevents timing attacks

**WebSocket Authentication (`src/websocket_auth.cpp`):**
- Token: Short-lived (60s TTL) from `GET /api/ws-token`
- Per-client auth state: `wsAuthStatus[16]` array
- Unauthenticated clients receive no broadcasts (skip serialization entirely)

**Device Identity:**
- Serial number: Generated from eFuse MAC at first boot, format `ALX-{MAC_HEX}`
- Storage: NVS namespace `device` key `serial`
- Used for: MQTT device ID, Home Assistant `unique_id`, telemetry identification
- Implementation: `initSerialNumber()` in `src/main.cpp`

**MQTT Broker Auth:**
- Protocol: MQTT 3.1.1 username/password in CONNECT packet
- TLS: Optional (user-configured, typically port 8883)
- Credentials: Stored in `/config.json` (plaintext on device)

## Home Automation Integration

**Home Assistant MQTT Auto-Discovery (`src/mqtt_ha_discovery.cpp`):**
- Protocol: MQTT Discovery (Home Assistant convention)
- Base topic: Configurable via web UI (default: `alx/{serial}`)
- Discovery prefix: `homeassistant/{entity_type}/{device_id}/{entity_id}/config`
- Availability: `{base_topic}/status` with `online`/`offline` payloads
- Entities published (~100+):
  - Switches: Amplifier, DSP enable, feature toggles
  - Sensors: Temperature, audio metrics (RMS, peak, THD), heap/PSRAM stats
  - Numbers: Volume, gain levels
  - Selects: Mode switches, presets
  - Binary sensors: HAL device presence, USB audio connected
- HAL device entities: Auto-populated when expansion mezzanines detected
- Disable option: Web UI toggle under MQTT settings

**MQTT Task Architecture (`src/mqtt_task.cpp`):**
- Dedicated FreeRTOS task on Core 0 (priority 2, 20Hz loop)
- Socket timeout: 2000ms (`MQTT_SOCKET_TIMEOUT_MS`)
- Reconnect interval: 5s
- Publish interval: 1s (dirty-flag driven)
- Heartbeat: 60s mandatory full state publish

## Communication Protocols

**WebSocket (Port 81, `src/websocket_handler.h`, `src/websocket_command.cpp`, `src/websocket_broadcast.cpp`):**
- Library: arduinoWebSockets v2.7.3 (vendored)
- Max clients: 16 (`WEBSOCKETS_SERVER_CLIENT_MAX`)
- Max frame size: 4096 bytes (`WEBSOCKETS_MAX_DATA_SIZE`)
- Protocol version: `"1.0"` (`WS_PROTOCOL_VERSION`)
- Auth: Token-based (60s TTL)
- Binary frames:
  - `0x01` Waveform: `[type:1][adc:1][samples:256]` = 258 bytes
  - `0x02` Spectrum: `[type:1][adc:1][freq:f32LE][bands:16xf32LE]` = 70 bytes
- Binary rate scaling: Adaptive skip factor based on client count (2/4/6/8x skip for 2/3-4/5-7/8+ clients)
- JSON broadcasts: 17+ state update types (audio, DSP, WiFi, MQTT, HAL, system, diagnostics, health, USB audio, I2S ports, Ethernet, power management, clock status)
- JSON commands: Settings changes, DSP config, pipeline control, WiFi/MQTT config, device management

**MQTT (Port 1883/8883, `src/mqtt_handler.cpp`, `src/mqtt_publish.cpp`):**
- Client: PubSubClient v2.8
- Protocol: MQTT 3.1.1
- TLS: Optional via `WiFiClientSecure` (user-configured)
- QoS: 0 (at most once) for all publishes
- Command subscriptions (inbound):
  - `{base_topic}/smartsensing/amplifier/set` — Amplifier control
  - `{base_topic}/dsp/enabled/set` — DSP toggle
  - `{base_topic}/audio/volume/set` — Volume control
  - `{base_topic}/audio/mute/set` — Mute control
  - `{base_topic}/signal_generator/enabled/set` — Signal gen start/stop
  - `{base_topic}/systemctl/restart` — Device restart
  - `{base_topic}/systemctl/factory_reset` — Factory reset
  - `{base_topic}/audio/usb/enabled/set` — USB audio toggle
  - Full list: `mqttCallback()` in `src/mqtt_handler.cpp`
- Publish topics (outbound):
  - `{base_topic}/status` — online/offline
  - `{base_topic}/audio/*` — Volume, mute, RMS, THD, USB audio
  - `{base_topic}/wifi/*` — SSID, RSSI, IP
  - `{base_topic}/smartsensing/*` — Amplifier, signal detection
  - `{base_topic}/dsp/*` — Enable state, preset, CPU usage
  - `{base_topic}/hal/*` — Device states, temperatures, clock status
  - `{base_topic}/ota/*` — Version, update status

**I2C (3 buses via `src/hal/hal_i2c_bus.h/.cpp`):**
- Bus 0 (EXT): GPIO 48/54 -> Wire1 — **SDIO conflict with WiFi**: Never access when WiFi active. Guarded by `isSdioBlocked()`.
- Bus 1 (ONBOARD): GPIO 7/8 -> Wire — ES8311 dedicated, always safe
- Bus 2 (EXPANSION): GPIO 28/29 -> Wire2 — Expansion mezzanines, always safe
- Per-bus mutex for thread safety
- Speed: 400kHz (default), configurable per device
- Probe retry: 2 attempts with 50ms backoff (`HAL_PROBE_RETRY_COUNT`)

**I2S (3 ports via `src/i2s_audio.h/.cpp`):**
- Port-generic API: `i2s_port_enable_tx/rx()`, `i2s_port_write/read()`, `i2s_port_get_info()`
- Per-port config: STD/TDM mode, TX/RX direction, any pin assignment
- Format: Philips/MSB/PCM, 16/24/32 bit depth
- MCLK multiplier: 128-1152 (configurable per port)
- DMA: 12 buffers x 256 frames (`I2S_DMA_BUF_COUNT`, `I2S_DMA_BUF_LEN`)
- DoP DSD detection integrated in pipeline format check

**USB Audio (`src/usb_audio.h/.cpp`):**
- Standard: USB Audio Class 2 (UAC2) via TinyUSB
- Formats: PCM16 stereo, PCM24 stereo
- Default: 48kHz / 16-bit
- Ring buffer: SPSC lock-free, 20ms capacity (power-of-2 frames)
- Volume: UAC2 host volume control (1/256 dB units)
- Task: Core 0, priority 1
- Format negotiation: Dynamic rate/depth detection via `usb_audio_get_negotiated_rate/depth()`

**SPI:**
- ST7735S TFT display: 40MHz SPI (GPIO 2/3/4/5/6/26)
- Used via LovyanGFX driver, no direct SPI access needed

**Ethernet (`src/eth_manager.h/.cpp`):**
- Interface: ESP32-P4 internal EMAC
- Config: DHCP (default) or static IP with 60s confirm timer
- Events: `ARDUINO_EVENT_ETH_*` handlers for link state
- Priority routing: Ethernet preferred when link is up
- REST: `GET /api/ethstatus`, `POST /api/ethconfig`, `POST /api/ethconfig/confirm`

## REST API (Exposed)

**Host:** `http://{device_ip}:80/`
**Auth:** Cookie + PBKDF2 password (rate-limited)
**Versioning:** All endpoints at `/api/<path>` and `/api/v1/<path>`. Frontend `apiFetch()` auto-rewrites `/api/` to `/api/v1/`.

**Core:**
- `GET /` — Serve web UI (gzipped HTML + JS + CSS)
- `GET /login`, `POST /login` — Authentication
- `GET /api/ws-token` — WebSocket auth token
- `POST /api/auth/password` — Password change
- `GET /api/auth/status` — Session validity

**Audio Pipeline (`src/pipeline_api.h/.cpp`):**
- `GET /api/pipeline/matrix` — 32x32 routing matrix
- `POST /api/pipeline/matrix` — Update matrix gains
- `GET /api/pipeline/analysis` — Real-time audio analysis

**DSP (`src/dsp_api.h/.cpp`):**
- `GET/POST /api/dsp` — DSP settings (enable, bypass)
- `GET/POST /api/dsp/config` — Stage configuration (per channel)
- Preset CRUD: save/load/export/import

**HAL (`src/hal/hal_api.h/.cpp`, 16 endpoints):**
- `GET /api/hal/devices` — All devices with state, config, clock status, dependencies
- `POST /api/hal/scan` — Trigger I2C discovery (409 if scan in progress)
- `GET /api/hal/db` — Built-in device database
- `GET /api/hal/db/presets` — Device presets by compatible string
- `PUT /api/hal/devices/{slot}` — Update device config
- `DELETE /api/hal/devices/{slot}` — Remove device
- `POST /api/hal/devices/reinit` — Reinitialize device
- `POST /api/hal/devices/{slot}/power` — Power management (standby/off/wake/on)
- `GET /api/hal/devices/{slot}/clock` — Clock status for DPLL-capable devices
- `GET/POST/DELETE /api/hal/devices/custom` — Custom device schemas
- `GET /api/hal/scan/unmatched` — Unmatched I2C addresses
- `GET /api/hal/settings` — HAL global settings

**EEPROM (`src/hal/hal_eeprom_api.h/.cpp`):**
- `GET /api/hal/eeprom/scan` — Scan for AT24C02 EEPROMs
- `POST /api/hal/eeprom/program` — Program EEPROM
- `POST /api/hal/eeprom/erase` — Erase EEPROM
- `GET /api/hal/eeprom/presets` — EEPROM preset list
- `GET /api/hal/eeprom/diag` — EEPROM diagnostic dump

**I2S Ports (`src/i2s_port_api.h/.cpp`):**
- `GET /api/i2s/ports` — All 3 port states (mode, format, pins, sample rate)

**Signal Generator (`src/siggen_api.h/.cpp`):**
- `GET/POST /api/signalgenerator` — Config (waveform, freq, amplitude, mode)

**WiFi/Network:**
- `GET /api/wifi/status` — Connection status + firmware version
- `POST /api/wifi/connect` — Join network
- `POST /api/wifi/scan` — Trigger WiFi scan
- `GET /api/wifi/networks` — Scan results
- `GET /api/wifilist` — Saved networks (no passwords)
- `POST /api/wifisave` — Save network config
- `POST /api/wifiremove` — Remove saved network
- `POST /api/wifi/ap` — Toggle AP mode
- `GET/POST /api/ap/config` — AP configuration

**Ethernet:**
- `GET /api/ethstatus` — Ethernet connection info
- `POST /api/ethconfig` — Update Ethernet settings
- `POST /api/ethconfig/confirm` — Confirm static IP config

**MQTT:**
- `GET /api/mqtt` — MQTT settings
- `POST /api/mqtt` — Update MQTT settings

**Settings (`src/settings_manager.h/.cpp`):**
- `GET/POST /api/settings` — Load/save all settings
- `POST /api/settings/export` — Export settings JSON
- `POST /api/settings/import` — Import settings (section-selective)
- `POST /api/settings/factory-reset` — Erase all
- `POST /api/reboot` — Device restart

**OTA/Updates (`src/ota_updater.h/.cpp`):**
- `GET /api/ota/check` — Check for updates (non-blocking task)
- `POST /api/ota/update` — Start OTA download
- `GET /api/ota/status` — Update progress
- `GET /api/ota/releases` — Release list
- `POST /api/ota/install` — Install specific release
- `POST /api/ota/upload` — Manual firmware upload (chunked)
- `GET /api/ota/release-notes` — Release notes for current version

**Diagnostics:**
- `GET /api/diagnostics` — Full diagnostic snapshot
- `GET /api/diagnostics/journal` — Diagnostic event journal
- `DELETE /api/diagnostics/journal` — Clear journal
- `GET /api/diag/snapshot` — Current system stats

**Health Check (`src/health_check_api.h/.cpp`):**
- `GET /api/health` — 10-category health report (heap, storage, DMA, I2C, HAL, I2S, network, MQTT, tasks, audio, clock)

**PSRAM (`src/psram_api.h/.cpp`):**
- `GET /api/psram/status` — PSRAM health and budget breakdown

## Monitoring & Observability

**Diagnostic Journal (`src/diag_journal.h/.cpp`):**
- Hot ring buffer: 32 entries in PSRAM, spinlock-protected
- Persistent ring: 800 entries in LittleFS (`/diag_journal.bin`), CRC32 per entry
- Flush interval: 60s (WARN+ severity only persisted)
- Event codes (`src/diag_error_codes.h`): 0x1000-0x7FFF across 6 facilities
- Emit: `diag_emit()` writes to ring, sets dirty flag, signals `EVT_DIAG`, prints `[DIAG]` JSON on Serial
- Broadcast: WS `diagnostics` message + MQTT `{base_topic}/diagnostics`

**Health Check (`src/health_check.h/.cpp`):**
- 10 categories: heap, storage, DMA, I2C buses, HAL devices, I2S ports, network, MQTT, tasks, audio, clock
- Two phases: Immediate (heap/storage/DMA at boot) + Deferred (30s delay for HAL/network readiness)
- Staleness guard: 5s cache to avoid repeated runs
- Results: Pass/Warn/Fail/Skip per check item (up to 32 items)
- Triggers: REST `GET /api/health`, serial `HEALTH_CHECK\n`, periodic timer

**Logging:**
- Framework: Custom `DebugSerial` class (`src/debug_serial.h/.cpp`)
- Levels: `LOG_D` (debug), `LOG_I` (info), `LOG_W` (warning), `LOG_E` (error)
- Serial output: 115200 baud UART
- WS forwarding: Real-time log streaming to web UI debug console
- Module prefixes: `[WiFi]`, `[MQTT]`, `[Audio]`, `[HAL]`, `[DSP]`, `[OTA]`, `[ETH]`, etc.
- Runtime control: `debugSerialLevel` configurable via web UI

**Hardware Monitoring (WS broadcast `hardwareStats`):**
- Internal heap: free bytes, fragmentation
- PSRAM: free bytes, fallback count, allocation failures
- WiFi RSSI (dBm)
- CPU usage per core (FreeRTOS idle task tracking via `src/websocket_cpu_monitor.cpp`)
- Task monitor: Stack high-water marks (`src/task_monitor.h/.cpp`)
- Broadcast interval: Configurable 1-10s (default 2s)

**Clock Quality Diagnostics:**
- `ClockStatus` struct: `available`, `locked`, `description` fields
- Reported by devices with `HAL_CAP_DPLL` capability (ESS SABRE DAC family)
- Surfaced via: WS `clockStatus` field in device state, REST `GET /api/hal/devices/{slot}/clock`
- Health check: `clock` category reports PLL lock state
- UI: Lock icon displayed on device cards when clock data available

## Third-Party Hardware Support

**HAL Device Ecosystem (`src/hal/`):**
- 32-slot device manager with 44 registered drivers
- 14 onboard devices at boot + expansion slots
- Device lifecycle: UNKNOWN -> DETECTED -> CONFIGURING -> AVAILABLE <-> UNAVAILABLE -> ERROR/REMOVED/DISABLED
- Power management: Active/Standby/Off states for devices with `HAL_CAP_POWER_MGMT`
- Dependency graph: `_dependsOn` bitmask, topological sort (Kahn's BFS) in `initAll()`

**Supported Audio Chips (via 4 generic driver patterns):**

| Pattern | Driver | Chips |
|---------|--------|-------|
| A | `hal_ess_adc_2ch` | ES9820, ES9821, ES9822PRO, ES9823PRO, ES9826, ES9840, ES9841, ES9842PRO, ES9843PRO |
| B | `hal_ess_adc_4ch` | (4-channel ESS ADC variants) |
| C | `hal_ess_dac_2ch` / `hal_cirrus_dac_2ch` | ES9017, ES9020, ES9033Q, ES9038Q2M, ES9039Q2M, ES9069Q, ES9081, ES9082, ES9027PRO, ES9028PRO, ES9038PRO, ES9039PRO / CS4398, CS4399, CS43130, CS43131, CS43198 |
| D | `hal_ess_dac_8ch` | (8-channel ESS DAC variants) |

**Onboard Devices (built-in at boot):**
- ES8311 codec (I2C Bus 1) — `src/hal/hal_es8311.h/.cpp`
- NS4150B speaker amp (GPIO) — `src/hal/hal_ns4150b.h/.cpp`
- PCM1808 dual ADC (I2S) — `src/hal/hal_pcm1808.h/.cpp`
- PCM5102A DAC (I2S) — `src/hal/hal_pcm5102a.h/.cpp`
- MCP4725 DAC (I2C) — `src/hal/hal_mcp4725.h/.cpp`
- Internal temp sensor — `src/hal/hal_temp_sensor.h/.cpp`
- ST7735S display (SPI) — `src/hal/hal_display.h/.cpp`
- EC11 rotary encoder (GPIO) — `src/hal/hal_encoder.h/.cpp`
- Buzzer (PWM) — `src/hal/hal_buzzer.h/.cpp`
- LED (GPIO) — `src/hal/hal_led.h/.cpp`
- Relay (GPIO) — `src/hal/hal_relay.h/.cpp`
- Button (GPIO) — `src/hal/hal_button.h/.cpp`
- Signal generator (MCPWM) — `src/hal/hal_signal_gen.h/.cpp`
- USB Audio device (TinyUSB) — `src/hal/hal_usb_audio.h/.cpp`

**Custom Devices (`src/hal/hal_custom_device.h/.cpp`):**
- Tier 1: Generic I2C register-based (user creates via web UI)
- Tier 2: JSON schema import/export
- Tier 3: Community contribution via GitHub issue
- Storage: `/hal/custom/*.json` on LittleFS

**EEPROM Discovery (`src/hal/hal_eeprom_v3.h/.cpp`):**
- AT24C02 EEPROM on expansion mezzanines
- Auto-discovery during I2C scan
- Programmable via REST API (`/api/hal/eeprom/*`)

## Event System

**FreeRTOS Event Group (`src/app_events.h`):**
- 24 usable bits (FreeRTOS limit), 17 assigned (bits 0-18, bits 5+13 freed), 7 spare
- `EVT_FORMAT_CHANGE` (bit 18): Sample rate mismatch or DoP DSD detection
- `EVT_DIAG`: Diagnostic journal entry written
- Main loop: `app_events_wait(5)` with 5ms timeout, wakes on any dirty flag

**Dirty Flag Pattern:**
- AppState singleton (`src/app_state.h`) with 15 domain-specific state headers in `src/state/`
- Each domain has `isDirty()` methods that minimize WS/MQTT broadcast overhead
- Dirty setters call `app_events_signal(EVT_XXX)` for immediate main loop wake

## Error Codes & Diagnostics

**Diagnostic Error Codes (`src/diag_error_codes.h`):**
- `0x1000-0x1FFF` — HAL diagnostics (I2C conflict, probe retry, device init failure)
- `0x2000-0x3FFF` — System diagnostics (heap warning, PSRAM critical, boot events)
- `0x4000-0x4FFF` — WiFi/network errors
- `0x5000-0x5FFF` — MQTT errors
- `0x6000-0x6FFF` — OTA/firmware errors
- `0x7000-0x7FFF` — Audio/DSP errors (rate mismatch, DSD detection, pipeline errors)

**Severity Levels:** INFO, WARNING, ERROR
**Correlation IDs:** Group related events (e.g., retry sequences)

## CI/CD & Deployment

**Firmware Distribution:**
- GitHub Releases (public, no authentication required for download)
- Channels: `stable` and `beta`
- OTA check interval: 5 min with exponential backoff on failure
- Download: HTTPS with SHA256 checksum verification
- Installation: ESP32 OTA partition swap (rollback-safe)
- Manual upload: Chunked firmware upload via REST API

**Documentation Site:**
- Docusaurus v3 on GitHub Pages
- Auto-deploy from `docs-site/` on main branch
- ~57 pages, 3 sidebars (user, developer, enterprise)
- Blog, showcase page, local search plugin

**No Remote Services Required:**
- Device operates fully offline after initial WiFi setup
- GitHub API only for OTA (optional)
- MQTT broker only for Home Assistant (optional)
- NTP only for accurate timestamps (optional)

---

*Integration audit: 2026-03-25*
