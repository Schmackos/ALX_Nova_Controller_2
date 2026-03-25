# Technology Stack

**Analysis Date:** 2026-03-25

## Languages

**Primary:**
- C++ (C++11) — All firmware in `src/`, unit tests in `test/`. Standard: `-std=c++11` (native), Arduino/ESP-IDF default (target).
- C — DSP biquad coefficient generator (`src/dsp_biquad_gen.c`), safe SNR/SFDR calculation (`src/safe_snr_sfdr.c`). Mixed C/C++ compilation.
- JavaScript (ES2020) — Web UI frontend (`web_src/js/`, 25 files), build tools (`tools/`), E2E test infrastructure (`e2e/`).

**Secondary:**
- Python 3.11 — On-device test harness (`device_tests/`), PlatformIO build system, RISC-V toolchain fix (`tools/fix_riscv_toolchain.py`).
- HTML/CSS — Self-contained single-page web UI (`web_src/index.html`, `web_src/css/` with 7 stylesheets). No JS framework.
- MDX/Markdown — Documentation site (`docs-site/`).

## Runtime

**Target Hardware:**
- ESP32-P4 (RISC-V dual-core, 360MHz)
- Board: Waveshare ESP32-P4-WiFi6-DEV-Kit (`board=esp32-p4`)
- Flash: 16MB (`board_upload.flash_size=16MB`)
- PSRAM: Available (allocations via `psram_alloc()` wrapper in `src/psram_alloc.h/.cpp`)

**Firmware Framework:**
- Arduino framework on ESP-IDF v5.1+ (`src/idf_component.yml`: `idf: '>=5.1'`)
- PlatformIO platform: pioarduino `platform-espressif32` release 55.03.37

**Partition Table** (`partitions_ota.csv`):
- Dual OTA: `app0` (4MB @ 0x10000) + `app1` (4MB @ 0x410000)
- NVS: 20KB @ 0x9000
- LittleFS: ~7.9MB @ 0x810000 (mapped as `spiffs` type in CSV)
- Coredump: 64KB @ 0xFF0000

**Build Environments:**
- `esp32-p4` — Main firmware (Arduino + ESP-IDF, all features enabled)
- `p4_hosted_update` — Minimal build for hosted OTA testing
- `native` — Host-side unit tests (Unity framework, no hardware required)

## Frameworks

**Core:**
- Arduino (ESP-IDF 5.1+ based) — Primary firmware framework
- FreeRTOS — RTOS kernel. Core 1 reserved for audio pipeline (priority 3), Core 0 for GUI/MQTT/USB/OTA tasks.

**Audio DSP:**
- Custom DSP pipeline (`src/dsp_pipeline.h/.cpp`) — 31 stage types: biquad (LPF/HPF/BPF/notch/PEQ/shelf/allpass/Linkwitz), FIR, limiter, compressor, multi-band compressor, noise gate, gain, delay, polarity, mute, decimator, convolution, tone control, speaker protection, stereo width, loudness compensation, bass enhancement
- ASRC engine (`src/asrc.h/.cpp`) — Polyphase FIR interpolation, 160 phases x 32 taps. Supports 44.1k/48k/88.2k/96k/176.4k/192k input rates to 48kHz output. Zero-cost passthrough when rates match. DSD lanes bypassed.
- Output DSP (`src/output_dsp.h/.cpp`) — Per-output mono DSP (up to 8 channels, 12 stages each)
- ESP-DSP — Pre-built `libespressif__esp-dsp.a` for target; `lib/esp_dsp_lite/` ANSI C fallback for native tests
- DoP DSD detection in `audio_pipeline_check_format()` — identifies DSD-over-PCM markers and signals `EVT_FORMAT_CHANGE`

**GUI:**
- LVGL v9.4 (`lib_deps: lvgl/lvgl@^9.4`) — Guarded by `-D GUI_ENABLED`
- LovyanGFX v1.2.0+ (`lib_deps: lovyan03/LovyanGFX@^1.2.0`) — ST7735S 128x160 TFT at 40MHz SPI
- 15 GUI screens in `src/gui/screens/` (home, control, devices, DSP, settings, WiFi, MQTT, debug, siggen, support, boot animation, desktop, keyboard, menu, value edit)
- Core 0 via `gui_task` (priority 1, stack 10240)

**Web UI:**
- Vanilla JavaScript (ES2020, no framework) — 25 concatenated files in `web_src/js/`
- Vanilla CSS — 7 stylesheets in `web_src/css/` (design token pipeline from `src/design_tokens.h`)
- Icons: Inline SVG from Material Design Icons (MDI), no external CDN
- Build: `node tools/build_web_assets.js` -> `src/web_pages.cpp` + `src/web_pages_gz.cpp` (gzipped)

**Networking:**
- WebServer (Arduino) — HTTP on port 80
- WebSockets v2.7.3 (vendored `lib/WebSockets/`) — Port 81, 16 clients max, token auth, binary+JSON frames
- PubSubClient v2.8+ — MQTT 3.1.1 with optional TLS
- WiFi (ESP32 WiFi6) — Multi-network STA + AP mode, power save disabled
- Ethernet (ESP32-P4 EMAC) — Internal PHY, static/DHCP, 60s confirm timer for static IP
- DNSServer — Captive portal for AP mode
- HTTPClient — GitHub API for OTA
- TinyUSB — USB Audio Class 2 (UAC2) device, PCM16/PCM24, SPSC ring buffer

**Testing:**
- Unity — C++ unit tests (3701 tests / 125 modules in `test/`)
- Playwright v1.50+ — E2E browser tests (358 tests / 57 specs in `e2e/tests/`)
- pytest v7.4-9.0 — On-device hardware tests (206 tests / 21 modules in `device_tests/tests/`)
- axe-core/playwright — Accessibility audit
- Express mock server (`e2e/mock-server/`) — 14 route modules, WS state machine

**Documentation:**
- Docusaurus v3.7+ — Static site with Mermaid diagrams, KaTeX math, local search, PWA support

## Key Dependencies

**Critical (Firmware):**
- `bblanchon/ArduinoJson@^7.4.2` — JSON for settings, WS messages, REST API, MQTT, HAL config
- `knolleary/PubSubClient@^2.8` — MQTT client (Home Assistant integration)
- `WebSockets@2.7.3` — Vendored in `lib/WebSockets/`, patched with `#ifndef` guard for `WEBSOCKETS_MAX_DATA_SIZE`

**Conditional (Firmware):**
- `lvgl/lvgl@^9.4` — GUI framework (requires `GUI_ENABLED`)
- `lovyan03/LovyanGFX@^1.2.0` — Display driver (requires `GUI_ENABLED`)

**Test-only:**
- `kosme/arduinoFFT@^2.0` — FFT for native test environment

**ESP-IDF Built-ins (no explicit version pin):**
- `WiFi`, `WiFiClientSecure`, `WebServer`, `DNSServer`, `HTTPClient`, `Update`
- `LittleFS`, `Preferences` (NVS)
- `ETH`, `Network` — Ethernet (ESP32-P4 only)
- `esp_task_wdt.h` — Watchdog timer (30s timeout)
- `mbedtls/md.h` — PBKDF2-SHA256 password hashing
- `ESP32CertBundle` — Mozilla root CA bundle for SSL validation
- TinyUSB — USB device stack for UAC2

**E2E Test Dependencies (`e2e/package.json`):**
- `@playwright/test@^1.50.0` — Browser automation
- `@axe-core/playwright@^4.11.1` — Accessibility checks
- `express@^4.21.0` — Mock HTTP server
- `cookie-parser@^1.4.7` — Mock auth cookies
- `eslint@^8.57.0` — JavaScript linting

**Documentation Site (`docs-site/package.json`):**
- `@docusaurus/core@^3.7.0`, `@docusaurus/preset-classic@^3.7.0`
- `@docusaurus/theme-mermaid@^3.7.0` — Architecture diagrams
- `@docusaurus/plugin-pwa@^3.9.2` — Progressive Web App
- `@easyops-cn/docusaurus-search-local@^0.44.5` — Offline search
- `rehype-katex@^6.0.3`, `remark-math@^5.1.1` — Math rendering
- `medium-zoom@^1.1.0` — Image zoom
- `react@^18.0.0`, `react-dom@^18.0.0` — Docusaurus runtime

**On-Device Tests (`device_tests/requirements.txt`):**
- `pytest>=7.4,<9.0`, `pytest-timeout>=2.2`, `pytest-html>=4.0`
- `pyserial>=3.5` — Serial communication
- `requests>=2.31` — REST API testing
- `websocket-client>=1.6` — WS protocol testing

## Configuration

**Build Configuration:**
- `platformio.ini` — Build flags, pin assignments, library deps, upload/monitor ports
- `src/config.h` — Compile-time constants: firmware version (`FIRMWARE_VERSION "1.12.3"`), pin defaults, task sizes, DSP/HAL/ASRC limits, auth iterations, WS protocol version, thresholds

**Feature Flags (Build-time `-D` defines):**
- `DSP_ENABLED` — DSP pipeline (biquad, FIR, limiter, gain, delay, convolution, etc.)
- `DAC_ENABLED` — DAC/HAL device manager subsystem
- `USB_AUDIO_ENABLED` — TinyUSB UAC2 speaker device on native USB
- `GUI_ENABLED` — LVGL TFT GUI on Core 0
- `HEALTH_CHECK_ENABLED` — Health check module with 10 categories
- `TEST_MODE` — **SECURITY-SENSITIVE**: Disables rate limiting, uses fixed password. Never commit uncommented.

**Key Capacity Constants:**
| Constant | Value | Location |
|----------|-------|----------|
| `HAL_MAX_DEVICES` | 32 | `src/hal/hal_types.h` |
| `HAL_MAX_DRIVERS` | 48 | `src/hal/hal_types.h` |
| `HAL_MAX_PINS` | 56 | `src/hal/hal_types.h` |
| `AUDIO_PIPELINE_MAX_INPUTS` | 8 | `src/config.h` |
| `AUDIO_PIPELINE_MAX_OUTPUTS` | 16 | `src/config.h` |
| `AUDIO_PIPELINE_MATRIX_SIZE` | 32 | `src/config.h` |
| `DSP_MAX_STAGES` | 24 | `src/config.h` |
| `DSP_MAX_CHANNELS` | 4 | `src/config.h` |
| `ASRC_MAX_TAPS` | 32 | `src/config.h` |
| `ASRC_MAX_PHASES` | 160 | `src/config.h` |
| `OUTPUT_DSP_MAX_CHANNELS` | 8 | `src/config.h` |
| `MAX_WS_CLIENTS` | 16 | `src/websocket_handler.h` |
| `DIAG_JOURNAL_HOT_ENTRIES` | 32 | `src/config.h` |
| `DIAG_JOURNAL_MAX_ENTRIES` | 800 | `src/config.h` |

**Pin Assignments (defined in `platformio.ini`, defaults in `src/config.h`):**
- LED=1, Amp=27, Buzzer=45, Reset Button=46
- I2S ADC1: BCK=20, DOUT=23, LRC=21, MCLK=22; ADC2: DOUT2=25
- I2S DAC TX: DOUT=24
- DAC I2C (Bus 0): SDA=48, SCL=54
- ES8311 I2C (Bus 1): SDA=7, SCL=8
- Expansion I2C (Bus 2): SDA=28, SCL=29
- ES8311 Onboard I2S: DSDIN=9, LRCK=10, ASDOUT=11, SCLK=12, MCLK=13
- TFT: MOSI=2, SCLK=3, CS=4, DC=5, RST=6, BL=26
- Encoder: A=32, B=33, SW=36
- SigGen PWM: GPIO=47

## Build Tools & Tooling

**PlatformIO:**
- Platform: pioarduino `platform-espressif32` 55.03.37
- Framework: Arduino
- Filesystem: LittleFS
- Upload port: COM8
- Monitor: 115200 baud, direct filter
- Extra scripts: `pre:tools/fix_riscv_toolchain.py`

**Node.js Tools (`tools/`):**
- `build_web_assets.js` — Compiles `web_src/` to C++ header files (gzipped)
- `find_dups.js` — Detects duplicate JS declarations (CI gate)
- `check_missing_fns.js` — Verifies all referenced JS functions exist (CI gate)
- `extract_tokens.js` — Design token pipeline (`src/design_tokens.h` -> CSS)
- `update_certs.js` — OTA SSL certificate updater
- `diagram-validation.js` — Validates Mermaid architecture diagrams (CI gate)
- `check_mapping_coverage.js` — Doc-to-code mapping completeness (CI gate)
- `extract_api.js` — API endpoint extraction from source
- `deep_check_fns.js` — Deep function reference check
- `check_hal_debug_contract.sh` — HAL debug contract validation

**Static Analysis:**
- cppcheck — C++ lint (`--enable=warning,performance`, `--std=c++11`, excludes `src/gui/`)
- ESLint v8.57 — JavaScript lint (config: `web_src/.eslintrc.json`)

**Pre-commit Hooks (`.githooks/pre-commit`):**
- `find_dups.js` + `check_missing_fns.js` + ESLint
- Activation: `git config core.hooksPath .githooks`

## CI/CD

**GitHub Actions Workflows:**

| Workflow | File | Purpose |
|----------|------|---------|
| Quality Gates | `.github/workflows/tests.yml` | 7 jobs: cpp-tests, cpp-lint, js-lint, security-check, e2e-tests, doc-coverage, build |
| Release | `.github/workflows/release.yml` | Manual dispatch, semver (major/minor/patch), stable/beta channels |
| Docs | `.github/workflows/docs.yml` | Documentation site build/deploy to GitHub Pages |
| Device Tests | `.github/workflows/device-tests.yml` | On-device pytest harness |
| Review Automation | `.github/workflows/review-automation.yml` | PR review automation |
| Issue Triage | `.github/workflows/issue-triage.yml` | Issue management |
| Issue Fix | `.github/workflows/issue-fix.yml` | Automated issue resolution |
| Claude Review | `.github/workflows/claude-review.yml` | AI-assisted code review |

**Quality Gates (all must pass before build):**
1. `cpp-tests` — `pio test -e native -v` (3701 tests)
2. `cpp-lint` — cppcheck on `src/` (excludes `src/gui/`)
3. `js-lint` — ESLint + `find_dups.js` + `check_missing_fns.js` + `diagram-validation.js` + web_pages.cpp direct-edit guard
4. `security-check` — Blocks `TEST_MODE` in production builds
5. `e2e-tests` — Playwright Chromium (excludes `@visual` in CI, retries=2)
6. `doc-coverage` — `check_mapping_coverage.js`
7. `build` — Full firmware build (depends on all 6 above)

**Release Pipeline (`release.yml`):**
- Semantic versioning with beta channel (`1.12.3-beta.1`)
- Runs same 4 quality gates before release
- SHA256 checksum for firmware binary integrity
- GitHub Release with auto-generated categorized release notes

## Security

**Authentication:**
- Web UI: PBKDF2-SHA256, 50k iterations (`p2:` format), auto-migrates from legacy `p1:` (10k) and raw SHA256
- Sessions: HttpOnly cookies, 1 hour timeout, 5 concurrent max (`src/auth_handler.h/.cpp`)
- WebSocket: Short-lived token (60s TTL) via `GET /api/ws-token`
- Rate limiting: 5 login attempts/minute (disabled in TEST_MODE)

**Transport:**
- TLS via mbedtls for HTTPS (GitHub OTA), optional for MQTT
- HTTP-only for local web UI (ports 80/81) — no LAN encryption
- Security headers: `X-Frame-Options: DENY`, `X-Content-Type-Options: nosniff` on all responses (`src/http_security.h`)
- Filename sanitization: `sanitize_filename()` prevents path traversal

**Certificates:**
- Sectigo R46/E46 + DigiCert G2 root CAs in `src/ota_certs.h`
- Mozilla root CA bundle via ESP32CertBundle for general SSL
- Update tool: `node tools/update_certs.js`

**OTA Integrity:**
- SHA256 hash verification (no code signing)
- Dual OTA partitions for safe rollback

## Platform Requirements

**Development:**
- PlatformIO Core (Python 3.11)
- Node.js >= 18 (E2E tests, tools, docs site)
- `PYTHONIOENCODING=utf-8` on Windows for upload (esptool Unicode fix)
- COM8 serial port (configurable in `platformio.ini`)

**Production:**
- ESP32-P4 with 16MB flash + PSRAM
- WiFi6 or Ethernet connectivity
- Internal SRAM: minimum 40KB free for WiFi RX buffers
- PSRAM: minimum 512KB free (critical threshold)

## Memory Architecture

**Internal SRAM:**
- DMA buffers: 12 x 256 frames (pre-allocated at boot)
- Heap reserve: 40KB for WiFi RX, warning at 50KB, critical at 40KB
- Stack sizes: Audio 12KB (Core 1), GUI 10KB, Web 8KB, MQTT/Sensing 4KB each

**PSRAM:**
- All large allocations via `psram_alloc()` (PSRAM-first, SRAM fallback capped at 64KB)
- DSP delays, FIR taps, convolution buffers, TDM deinterleave
- ASRC history buffers (~1.5KB per active lane)
- Diagnostic journal hot ring buffer (2KB)
- Warning at 1MB free, critical at 512KB free

**LittleFS (~7.9MB):**
- `/config.json` — Primary user settings
- `/hal_config.json` — HAL device configurations
- `/hal/custom/*.json` — User-created custom device schemas
- `/diag_journal.bin` — Persistent diagnostic ring (64KB cap)
- Legacy files auto-migrated: `mqtt_config.txt`, `settings.txt`

**NVS (20KB):**
- WiFi credentials (up to 5 networks)
- OTA channel, HAL auto-discovery toggle
- Password hash, diagnostic sequence counter
- Survives LittleFS format

---

*Stack analysis: 2026-03-25*
