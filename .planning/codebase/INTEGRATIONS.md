# External Integrations

**Analysis Date:** 2026-03-08

## APIs & External Services

**Firmware Update (OTA):**
- GitHub Releases API — Checks for new firmware versions and downloads assets
  - Check endpoint: `https://api.github.com/repos/{owner}/{repo}/releases/latest`
  - List endpoint: `https://api.github.com/repos/{owner}/{repo}/releases?per_page=5`
  - Specific release notes: `https://api.github.com/repos/{owner}/{repo}/releases/tags/{version}`
  - Download: `https://objects.githubusercontent.com/` (firmware binary asset)
  - Auth: None (public repo, unauthenticated API calls)
  - SSL: 3 hardcoded root CA certs in `src/ota_updater.cpp` — Sectigo R46 (RSA), Sectigo E46 (ECC), DigiCert Global Root G2 (CDN); valid through 2038-2046
  - SDK: `<WiFiClientSecure.h>` + `<HTTPClient.h>`
  - Implementation: `src/ota_updater.cpp`, `src/ota_updater.h`
  - Repo constants: `GITHUB_REPO_OWNER "Schmackos"`, `GITHUB_REPO_NAME "ALX_Nova_Controller_2"` (in `src/config.h`)

**NTP Time Synchronization:**
- Primary: `pool.ntp.org`
- Secondary: `time.nist.gov`
- SDK: `configTime()` (Arduino/IDF built-in)
- Triggered after WiFi connects and on settings load
- Implementation: `src/utils.cpp` `syncTimeWithNTP()`

## Data Storage

**Filesystems:**
- LittleFS (spiffs partition, ~8MB) — Primary config and data store
  - `/config.json` — Main settings (version 1, JSON format with `settings` + `mqtt` keys); safe atomic write via `.tmp` rename
  - `/hal_config.json` — HAL device per-device config overrides
  - `/dsp_*.json` — DSP preset slots (up to 32 presets per channel)
  - `/output_dsp_*.json` — Per-output DSP configs
  - `/pipeline.json` — 16x16 routing matrix state
  - `/signal_gen.json` — Signal generator settings
  - `/input_names.json` — User-assigned input channel names
  - `/diag_journal.bin` — Binary diagnostic journal ring buffer (up to 800 entries, 64KB)
  - Legacy fallback: `/settings.txt`, `/mqtt_config.txt` (only read when JSON v1+ absent)
  - Client: `<LittleFS.h>` (Arduino ESP32 built-in)

- NVS (`<Preferences.h>`) — Small key-value store, survives LittleFS format
  - Namespace `device` — device serial number, firmware version for serial regen
  - Namespace `wifi` — Multi-network credentials (up to 5 networks, SSIDs and passwords)
  - Namespace `auth` — Password hash (PBKDF2-SHA256 format `"p1:<saltHex>:<keyHex>"`)
  - Namespace `ota` — OTA success flag and previous firmware version

**File Storage:**
- Local filesystem (LittleFS) only — no cloud storage

**Caching:**
- None — No Redis/Memcache; AppState singleton holds all live state in RAM with dirty flags

## Authentication & Identity

**Web Auth:**
- Custom session-based auth — password stored in NVS as PBKDF2-SHA256 (10,000 iterations, 16-byte random salt)
- Format: `"p1:<saltHex>:<keyHex>"` — legacy unsalted SHA256 auto-migrates on next successful login
- First-boot random password (10 chars, ~57-bit entropy) displayed on TFT and serial; regenerated on factory reset
- Sessions: up to 5 concurrent sessions (`MAX_SESSIONS=5`), 1-hour TTL, stored in RAM
- Rate limiting: non-blocking — failed attempts set `_nextLoginAllowedMs`; excess returns HTTP 429 with `Retry-After` header
- Cookie: `HttpOnly` flag set
- Implementation: `src/auth_handler.cpp`, `src/auth_handler.h`

**WebSocket Auth:**
- Short-lived WS tokens: `GET /api/ws-token` issues a 16-byte random token (60s TTL, 16-slot pool)
- Client sends token in first WebSocket message on `onopen`; server validates via `validateWsToken()`
- Implementation: `src/auth_handler.cpp` (`generateWsToken()`, `validateWsToken()`)

**Device Identity:**
- Serial number generated from eFuse MAC at boot, stored in NVS; regenerated when firmware version changes
- Used in MQTT device ID and Home Assistant entity naming

## MQTT / Home Assistant Integration

**MQTT Broker:**
- Protocol: MQTT 3.1.1 (plaintext, port 1883 default)
- Client: `knolleary/PubSubClient@^2.8` via `<PubSubClient.h>`
- Connection: dedicated `mqtt_task` on Core 0 (priority 2, 50ms poll); reconnect with 5s backoff
- Config: broker host, port, user, password stored in `/config.json` (`mqtt` key) and NVS
- Base topic: configurable (default `alxnova/{deviceId}`)
- Implementation: `src/mqtt_handler.cpp`, `src/mqtt_publish.cpp`, `src/mqtt_ha_discovery.cpp`, `src/mqtt_task.cpp`

**Home Assistant Auto-Discovery:**
- Publishes ~100 entity configs to `homeassistant/{type}/{deviceId}/{entity}/config` (retained)
- Entities include: amplifier switch, signal state sensors, timer controls, audio threshold, WiFi status, OTA update sensors, DSP controls, HAL device states, USB audio state, debug mode, per-lane ADC health
- Device info payload includes: model, manufacturer, serial number, firmware version, `configuration_url` (device IP)
- Availability topic: `{base}/status` with payloads `online` / `offline`
- Implementation: `src/mqtt_ha_discovery.cpp` `publishHADiscovery()` (~1,880 lines)

**MQTT Topic Structure (published):**
- `{base}/state` — amplifier relay state
- `{base}/sensing/*` — smart sensing mode, signal detected, timer state
- `{base}/wifi/*` — SSID, IP, RSSI, mode
- `{base}/system/*` — uptime, heap, firmware version, update availability
- `{base}/audio/adc/{lane}/*` — per-lane ADC health, RMS, signal state (dynamic, iterates `activeInputCount`)
- `{base}/hardware/*` — chip temperature, CPU frequency
- `{base}/dsp/*` — DSP enabled/bypass per channel (guarded by `DSP_ENABLED`)
- `{base}/usb/*` — USB audio connected, streaming, volume, overruns/underruns (guarded by `USB_AUDIO_ENABLED`)
- `{base}/status` — `online` / `offline` (LWT)

## Network Connectivity

**WiFi:**
- Multi-network client: up to 5 saved networks (`MAX_WIFI_NETWORKS=5`), tries each on reconnect
- AP mode: captive portal with `DNSServer` (port 53) for initial config; SSID includes device short ID
- SDK: `<WiFi.h>` (Arduino ESP32 built-in)
- Static IP support per network profile
- Power save: `WIFI_PS_NONE` re-applied after `WiFi.mode()` resets it
- Implementation: `src/wifi_manager.cpp`, `src/wifi_manager.h`

**Ethernet (ESP32-P4 only):**
- 100Mbps Full Duplex via internal EMAC + PHY
- Acts as fallback/secondary route; `eth_manager_set_default_route()` prefers Ethernet when link is up
- OTA checks accept either WiFi OR Ethernet connectivity
- Implementation: `src/eth_manager.cpp`, `src/eth_manager.h`
- Guard: `#ifndef NATIVE_TEST` (not compiled for tests)

## Hardware Interfaces & Protocols

**I2S Audio (ESP-IDF 5 std API `<driver/i2s_std.h>`):**
- ADC1: PCM1808 dual-channel ADC on I2S_NUM_0 (master RX + TX bridge) — GPIO BCK=20, LRC=21, MCLK=22, DOUT=23
- ADC2: PCM1808 second ADC on I2S_NUM_1 (master RX, no clock output) — DOUT2=25 (INPUT_PULLDOWN)
- DAC TX: PCM5102A or ES8311 on I2S_NUM_0 full-duplex — DOUT=24
- ES8311: I2S_NUM_2 TX on internal PCB pins (GPIO 9/10/11/12/13)
- Both ADC masters coordinated: ADC2 init first (data only), ADC1 second (all clocks); frequency-locked BCK
- Implementation: `src/i2s_audio.cpp`, `src/i2s_audio.h`

**I2C (`<Wire.h>`):**
- Bus 0 (EXT): GPIO SDA=48, SCL=54 — EEPROM probe, expansion devices; **skip when WiFi active** (SDIO conflict with ESP32-C6)
- Bus 1 (ONBOARD): GPIO SDA=7, SCL=8 — ES8311 codec dedicated; always safe
- Bus 2 (EXPANSION): GPIO SDA=28, SCL=29 — expansion devices; always safe
- Devices: ES8311 (0x18), MCP4725, EEPROM (custom v3 format with CRC-16 and compatible string)
- HAL discovery: 3-tier scan — I2C address scan (0x08-0x77) → EEPROM v3 probe → manual config
- Implementation: `src/dac_eeprom.cpp`, `src/hal/hal_discovery.cpp`, `src/drivers/dac_es8311.cpp`

**MCPWM (ESP-IDF `<driver/mcpwm_prelude.h>`):**
- Signal generator PWM output on GPIO 47 (`SIGGEN_MCPWM_GROUP=0`, 1MHz prescaled resolution)
- Implementation: `src/signal_generator.cpp`

**LEDC (Arduino 3.x API):**
- Buzzer: `ledcWriteTone()` on GPIO 45 (`BUZZER_PWM_CHANNEL=2`, 8-bit resolution)
- TFT backlight: LEDC timer 0 on GPIO 26 (`TFT_BL_PIN`)
- API: `ledcAttach(pin, freq, res)` / `ledcDetach(pin)` — pin-based (Arduino ESP32 3.x style)

**SPI (Hardware SPI2/FSPI):**
- ST7735S TFT display: 128x160px, landscape 160x128 — GPIO MOSI=2, SCLK=3, CS=4, DC=5, RST=6
- Synchronous (no DMA on P4); LovyanGFX at 40MHz write frequency
- Config: `src/gui/lgfx_config.h`

**GPIO:**
- Amplifier relay: GPIO 27 (`AMPLIFIER_PIN`) — relay enable/disable
- NS4150B amplifier enable: GPIO 53 (shared with ES8311 PA pin)
- LED: GPIO 1 (`LED_PIN`) — stays LOW permanently (LED blink test removed)
- Reset button: GPIO 46 (`RESET_BUTTON_PIN`) — factory reset (hold 10s)

**Rotary Encoder:**
- Gray code state machine ISR on GPIO A=32, B=33, SW=36
- Implementation: `src/gui/gui_input.cpp` (ISR-driven), HAL device `src/hal/hal_encoder.cpp`

**USB Audio (TinyUSB UAC2):**
- Native USB OTG on ESP32-P4 GPIO 19/20
- UAC2 speaker device (2-channel, PCM16, up to 96kHz negotiated)
- Custom audio class driver registered via `usbd_app_driver_get_cb()` weak function
- SPSC lock-free ring buffer (1,024 frames, PSRAM)
- TinyUSB API: `tusb.h`, `device/usbd_pvt.h`, `class/audio/audio.h`, `esp32-hal-tinyusb.h`
- Build flags: `-D USB_AUDIO_ENABLED`, `-D ARDUINO_USB_MODE=0`
- Implementation: `src/usb_audio.cpp`, `src/usb_audio.h`

**ESP32-P4 Internal Temperature Sensor:**
- IDF5 `<driver/temperature_sensor.h>` — range -10 to +80°C
- Guarded by `CONFIG_IDF_TARGET_ESP32P4`
- Implementation: `src/hal/hal_temp_sensor.cpp`

## Web Server Endpoints

**HTTP Server (port 80, `<WebServer.h>`):**
- Static assets: `/`, `/login` — gzip-compressed HTML/CSS/JS from `src/web_pages_gz.cpp`
- Auth: `POST /api/auth/login`, `POST /api/auth/logout`, `GET /api/auth/status`, `POST /api/auth/change`
- WS token: `GET /api/ws-token`
- WiFi: `POST /api/wificonfig`, `POST /api/wifisave`, `GET /api/wifilist`, `POST /api/wifiremove`, `GET /api/wifistatus`, `POST /api/wifiscan`
- Settings: `GET /api/settings`, `POST /api/settings`, `GET /api/settings/export`, `POST /api/settings/import`, `POST /api/factoryreset`, `POST /api/reboot`
- MQTT: `GET /api/mqtt`, `POST /api/mqtt`
- OTA: `GET /api/checkupdate`, `POST /api/startupdate`, `GET /api/updatestatus`, `GET /api/releasenotes`, `GET /api/releaselist`, `POST /api/installrelease`, `POST /api/firmware` (manual upload)
- HAL: `GET /api/hal/devices`, `POST /api/hal/scan`, `PUT /api/hal/devices`, `DELETE /api/hal/devices`, `POST /api/hal/devices/reinit`, `GET /api/hal/db/presets`
- DSP: REST CRUD under `/api/dsp/`
- Pipeline: REST under `/api/pipeline/`
- Captive portal: `/generate_204`, `/hotspot-detect.html` — returns 200 to suppress OS captive portal detection

**WebSocket Server (port 81, vendored `WebSocketsServer`):**
- Real-time state push to web clients
- Auth: token validated in first message after connect
- Text frames (JSON): state broadcasts — hardware stats, WiFi status, MQTT state, sensing state, audio levels, HAL device map, channel map
- Binary frames: `WS_BIN_WAVEFORM` (0x01) — raw waveform samples; `WS_BIN_SPECTRUM` (0x02) — FFT spectrum data
- Log forwarding: `broadcastLine()` sends log entries with `module` field for category filtering
- Implementation: `src/websocket_handler.cpp`, `src/websocket_handler.h`

## CI/CD & Deployment

**Hosting:**
- Firmware runs self-hosted on device (no cloud hosting)
- GitHub Releases hosts firmware binary assets for OTA

**CI Pipeline (GitHub Actions):**
- `tests.yml` — 4 parallel quality gates on push/PR to `main`/`develop`:
  1. `cpp-tests` — `pio test -e native -v` (1,556 Unity tests)
  2. `cpp-lint` — cppcheck on `src/` (excluding `src/gui/`)
  3. `js-lint` — `find_dups.js` + `check_missing_fns.js` + ESLint
  4. `e2e-tests` — Playwright (26 browser tests); HTML report uploaded as artifact on failure (14-day retention)
  - All 4 gates must pass before `build` job runs
- `release.yml` — Same 4 gates before release publish

## Webhooks & Callbacks

**Incoming:**
- None — device is always the MQTT client, never the broker

**Outgoing:**
- MQTT publishes to broker (all topics under configurable base topic)
- Home Assistant MQTT auto-discovery publishes to `homeassistant/` prefix

## Environment Configuration

**Required configuration (stored in NVS / LittleFS, not env vars):**
- WiFi SSID + password (NVS `wifi` namespace)
- MQTT broker host, port, username, password (`/config.json` `mqtt` section)
- Web UI password (NVS `auth` namespace, PBKDF2-SHA256)

**No `.env` file** — all runtime config is managed through the web UI at `http://{device-ip}/` and persisted to LittleFS/NVS on the device.

---

*Integration audit: 2026-03-08*
