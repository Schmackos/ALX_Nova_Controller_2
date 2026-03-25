# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-P4 open-source audio platform with modular mezzanine expansion. HAL-managed ADC/DSP/DAC pipeline, web UI, MQTT/HA integration, OTA updates. Built with PlatformIO + Arduino framework (esp-idf based). Firmware version in `src/config.h` (`FIRMWARE_VERSION`). Target board: Waveshare ESP32-P4-WiFi6-DEV-Kit (`board=esp32-p4`). Upload port: COM8. Full project mission: `docs-site/docs/about.md`.

## Build & Test Commands

```bash
# Build firmware for ESP32-P4
pio run

# Upload firmware (PYTHONIOENCODING needed on Windows — esptool Unicode fix)
PYTHONIOENCODING=utf-8 pio run --target upload

# Monitor serial output
pio device monitor

# Run all C++ unit tests (native platform, no hardware needed)
pio test -e native

# Run specific test module
pio test -e native -f test_wifi

# Run E2E browser tests
cd e2e && npm install && npx playwright install --with-deps chromium  # first time
cd e2e && npx playwright test

# Run on-device tests (requires connected ESP32-P4)
cd device_tests && pytest tests/ --device-port COM8 --device-ip <DEVICE_IP> -v

# Static analysis
cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json
node tools/find_dups.js && node tools/check_missing_fns.js

# Documentation site
cd docs-site && npm install && npm run build && npm run serve
```

## Architecture

### State Management — AppState Singleton
15 domain-specific headers under `src/state/`, composed into `AppState` singleton. Access via `appState.domain.field` (e.g., `appState.wifi.ssid`, `appState.audio.adcEnabled[i]`, `appState.dsp.enabled`). Dirty flags (e.g., `isBuzzerDirty()`) minimize WS/MQTT broadcasts. Every dirty setter also calls `app_events_signal(EVT_XXX)` so the main loop wakes immediately via `app_events_wait(5)`. Event group: 24 usable bits, 17 assigned (bits 0-18, with bits 5 and 13 freed), 7 spare (`src/app_events.h`). `EVT_FORMAT_CHANGE` (bit 18) signals sample rate mismatch or DoP DSD detection.

DAC device state (enabled, volume, mute, filterMode) lives in `HalDeviceConfig` via HAL manager — not in DacState. Device toggles use `appState.halCoord.requestDeviceToggle(halSlot, action)`.

### Module Map
**Audio** (`src/`): `audio_pipeline.h/.cpp` (8-lane→DSP→32x32 matrix→DSP→16-slot sink, float32; includes DoP DSD detection, format negotiation via `audio_pipeline_check_format()`), `i2s_audio.h/.cpp` (3 I2S ports, unified `I2sPortState _port[3]`), `i2s_port_api.cpp` (REST), `output_dsp.h/.cpp` (per-output mono DSP), `dsp_pipeline.h/.cpp` (4ch biquad/FIR/limiter/gain/delay/compressor), `signal_generator.h/.cpp`, `thd_measurement.h/.cpp`, `dsp_convolution.h/.cpp`

**HAL** (`src/hal/`): `hal_device_manager.h/.cpp` (32-slot singleton, pin tracking, NVS fault persistence), `hal_pipeline_bridge.h/.cpp` (sink/source registration), `hal_discovery.h/.cpp` (I2C scan→EEPROM→manual, async rescan), `hal_device_db.h/.cpp`, `hal_api.h/.cpp` (16 REST endpoints), `hal_custom_device.h/.cpp` (tier 1-3 user devices), `hal_i2c_bus.h/.cpp` (3 buses, per-bus mutex), `hal_eeprom_api.h/.cpp`, `hal_settings.h/.cpp`

**Network**: `wifi_manager.cpp` (multi-network, AP mode), `mqtt_handler.cpp` + `mqtt_publish.cpp` + `mqtt_ha_discovery.cpp` (TLS, HA auto-discovery), `mqtt_task.cpp` (Core 0, 20Hz), `ota_updater.cpp` (SHA256 verified)

**Web UI**: `web_pages.cpp`/`web_pages_gz.cpp` (**auto-generated — edit `web_src/` then run `node tools/build_web_assets.js`**). WebSocket: `websocket_command.cpp`, `websocket_broadcast.cpp`, `websocket_auth.cpp` (port 81, token auth, 16 clients max). REST API versioning: all endpoints available at both `/api/<path>` and `/api/v1/<path>`. Frontend `apiFetch()` in `01-core.js` auto-rewrites `/api/` → `/api/v1/` (except `/api/__test__/`). E2E tests use `**/api/v1/...` route patterns.

**Other**: `settings_manager.cpp` (JSON + NVS), `auth_handler.cpp` (PBKDF2-SHA256, rate limiting), `http_security.h` (`server_send()` wrapper, `sanitize_filename()`), `health_check.h/.cpp` (9 categories), `psram_alloc.h/.cpp` (PSRAM-first with SRAM fallback, 64KB cap), `smart_sensing.cpp`, `button_handler.cpp`, `buzzer_handler.cpp`, `debug_serial.h` (LOG_D/I/W/E macros)

**GUI**: LVGL v9.4 + LovyanGFX on ST7735S 128x160. Core 0 via `gui_task`. Guarded by `-D GUI_ENABLED`. Screens in `src/gui/screens/`.

### HAL Framework
Device lifecycle: UNKNOWN → DETECTED → CONFIGURING → AVAILABLE ⇄ UNAVAILABLE → ERROR / REMOVED / DISABLED. Cross-core atomic accessors: `setReady(bool)`/`isReady()` with `__ATOMIC_RELEASE`/`__ATOMIC_ACQUIRE`. **All drivers must use `setReady()` — never raw `_ready =`**. Config validation via `hal_validate_config()`. Capability flags: `uint16_t` (15 bits defined: 0-14). Key caps: `HAL_CAP_DSD` (11), `HAL_CAP_HP_AMP` (12), `HAL_CAP_POWER_MGMT` (13), `HAL_CAP_ASRC` (14, placeholder for hardware ASRC mezzanine). `HAL_REGISTER()` macro for driver registration. Detailed docs: `docs-site/docs/developer/hal/`.

26 expansion devices (9 ESS ADC + 12 ESS DAC + 5 Cirrus DAC) via 4 generic driver patterns: Pattern A (`hal_ess_adc_2ch`), Pattern B (`hal_ess_adc_4ch`), Pattern C (`hal_ess_dac_2ch`, `hal_cirrus_dac_2ch`), Pattern D (`hal_ess_dac_8ch`). 14 onboard devices at boot.

**I2C bus architecture** (all access via `HalI2cBus::get(busIndex)`):
- Bus 0 (EXT): GPIO 48/54 → **Wire1** — shares SDIO with WiFi, never access when WiFi active
- Bus 1 (ONBOARD): GPIO 7/8 → **Wire** — ES8311 dedicated, always safe
- Bus 2 (EXPANSION): GPIO 28/29 → **Wire2** — always safe

### FreeRTOS Tasks
**Core 1** (reserved for audio): `loopTask` (priority 1) + `audio_pipeline_task` (priority 3). No new tasks on Core 1.
**Core 0**: `gui_task`, `mqtt_task` (priority 2), `usb_audio_task`, OTA tasks.

**I2S driver safety**: Use `audio_pipeline_request_pause(timeout_ms)` before driver teardown and `audio_pipeline_resume()` after. **Never set `appState.audio.paused` directly.**

### I2S Port Architecture
3 ESP32-P4 I2S ports via unified `I2sPortState _port[3]` array. Each independently configurable for STD/TDM, TX/RX, any pins. Port-generic API: `i2s_port_enable_tx/rx()`, `i2s_port_disable_tx/rx()`, `i2s_port_write/read()`, `i2s_port_get_info()`. Per-port config: format (Philips/MSB/PCM), bit depth (16/24/32), MCLK multiple (128-1152).

### Heap Safety & PSRAM
All PSRAM allocations via `psram_alloc()` wrapper (PSRAM-first, SRAM fallback capped at 64KB). DMA buffers: 16x2KB internal SRAM, pre-allocated at boot. Heap thresholds: Warning 50KB / Critical 40KB. PSRAM thresholds: Warning 1MB / Critical 512KB (`config.h`). **WiFi RX needs ~40KB free internal SRAM** — below this, HTTP/WS packets silently dropped.

## Pin Configuration

Defined in `platformio.ini` with fallback defaults in `src/config.h`. Key pins: LED=1, Amp=27, Buzzer=45, Encoder=32/33/36, I2S ADC1=BCK20/DOUT23/LRC21/MCLK22, I2S ADC2=DOUT25, I2S DAC TX=DOUT24, SigGen PWM=47, ES8311 I2C=SDA7/SCL8, Expansion I2C=SDA28/SCL29. Full listing in `src/config.h`.

## Testing Conventions

### C++ Unit Tests (Unity, native)
- Arrange-Act-Assert pattern, `setUp()` resets state
- Mocks in `test/test_mocks/` (Arduino, WiFi, MQTT, NVS)
- Compiles with `-D UNIT_TEST -D NATIVE_TEST` (`test_build_src = no`)
- Each test module in its own directory

### E2E Tests (Playwright)
Mock Express server + `routeWebSocket()` interception. Infrastructure: `e2e/helpers/` (fixtures, WS assertions, selectors), `e2e/pages/` (19 POMs), `e2e/fixtures/` (WS + API), `e2e/mock-server/` (Express + WS state). Tags: `@smoke`, `@ws`, `@api`, `@hal`, `@audio`, `@settings`, `@error`.

Key patterns: WS interception via `page.routeWebSocket(/.*:81/, handler)` (capital `onMessage`/`onClose`). Tab navigation: `page.evaluate(() => switchTab('tabName'))`. CSS-hidden checkboxes: `toBeChecked()` not `toBeVisible()`. POM: `new DevicesPage(page); await devices.open()`.

### On-Device Tests (pytest)
`device_tests/` — pyserial + Python requests against real hardware. Markers: `boot`, `health`, `hal`, `audio`, `network`, `mqtt`, `settings`, `reboot`, `slow`.

### Mandatory Test Coverage Rules

**Every code change MUST keep tests green.** Before completing any task:

1. **C++ firmware** (`src/`): Run `pio test -e native -v`. New modules need `test/test_<module>/`. Changed signatures → update tests.
2. **Web UI** (`web_src/`): Run Playwright tests. New controls → test WS commands. New broadcasts → fixture + test. Changed IDs → update `selectors.js`. New JS globals → add to `web_src/.eslintrc.json`.
3. **WebSocket changes**: Update `e2e/fixtures/ws-messages/`, `ws-helpers.js` `buildInitialState()`/`handleCommand()`, `ws-state.js`.
4. **REST API changes**: Update matching `e2e/mock-server/routes/*.js` + fixtures.
5. **Health check changes**: New driver → onboard device list. New endpoint → smoke test list. New task → expected task list.

### Quality Gates (CI)
All 4 must pass: `cpp-tests`, `cpp-lint`, `js-lint`, `e2e-tests`. See `.github/workflows/tests.yml`.

### Pre-commit Hooks
`.githooks/pre-commit`: find_dups + check_missing_fns + ESLint. Activate: `git config core.hooksPath .githooks`

## Current Gotchas

- **I2C Bus 0 SDIO conflict**: GPIO 48/54 shared with WiFi SDIO — I2C transactions cause MCU reset. All I2C access must go through `HalI2cBus::get(busIndex)` which handles the SDIO guard automatically via `isSdioBlocked()`. HAL discovery skips Bus 0 via `hal_wifi_sdio_active()` (checks `connectSuccess`, `connecting`, AND `activeInterface`). `wifi_manager.cpp` sets `activeInterface = NET_WIFI` on connect and immediately clears `connectSuccess` + `activeInterface` in the `ARDUINO_EVENT_WIFI_STA_DISCONNECTED` handler — no 20s timeout needed
- **No logging in ISR/audio task**: `Serial.print` blocks on UART TX buffer full, starving DMA. Use dirty-flag pattern: task sets flag, main loop does Serial/WS output
- **Slot-indexed sink removal only**: Never call `audio_pipeline_clear_sinks()` from deinit paths — use `audio_pipeline_remove_sink(slot)` to remove only the specific device
- **I2S driver reinstall handshake**: Use `audio_pipeline_request_pause()`/`audio_pipeline_resume()` — never set `appState.audio.paused` directly. See FreeRTOS Tasks section
- **HAL slot capacity**: 14/32 onboard slots at boot + up to 2 expansion (ADC+DAC) = 16/32 (`HAL_MAX_DEVICES=32`). Driver registry: 44/48 (`HAL_MAX_DRIVERS=48`). Device DB: 44/48 (`HAL_DB_MAX_ENTRIES=48`). Pin tracking: `HAL_MAX_PINS=56` with `HAL_GPIO_MAX=54` validation
- **Board config**: Use `board=esp32-p4` (360MHz, ES variant). `esp32-p4_r3` crashes on Waveshare ECO2 silicon. `board_upload.flash_size=16MB` override handles 4MB default mismatch
- **Heap 40KB reserve**: WiFi RX buffers need ~40KB free internal SRAM. DSP/audio allocations must use PSRAM or be heap-guarded
- **MCLK continuity**: Never call `i2s_configure_adc1()` in the audio task loop — MCLK must remain continuous for PCM1808 PLL stability
- **Capacity constants when adding features**:
  | Feature Added | Constants to Check |
  |---|---|
  | New HAL driver (`HAL_REGISTER()`) | `HAL_MAX_DRIVERS` (48), `HAL_DB_MAX_ENTRIES` (48), `HAL_BUILTIN_DRIVER_COUNT` in `hal_builtin_devices.cpp` |
  | New event bit | `EVT_*` in `app_events.h` (24 usable bits, 17 assigned, 7 spare) |
  | New pipeline sink/source | `AUDIO_PIPELINE_MAX_OUTPUTS` (16), `AUDIO_PIPELINE_MATRIX_SIZE` (32) |
  | New HAL device instance at boot | `HAL_MAX_DEVICES` (32, 14 onboard + expansion) |

## Logging Conventions

- `LOG_I` for state transitions and significant events. `LOG_D` for high-frequency details
- **Never log in ISR or audio task** — use dirty-flag pattern
- Log transitions, not repetitive state (use static `prev` variables)
- Save `.log` files to `logs/` directory

## Icons

All web UI icons use inline SVG from [Material Design Icons (MDI)](https://pictogrammers.com/library/mdi/). No external CDN — page is self-contained/offline.

```html
<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true">
  <path d="<paste MDI path here>"/>
</svg>
```

Use `fill="currentColor"`, explicit `width`/`height`, `aria-hidden="true"` on decorative icons, `aria-label` on interactive icon-only elements.

## Key Dependencies

- `ArduinoJson@^7.4.2`, `PubSubClient@^2.8`, `lvgl@^9.4` (guarded `GUI_ENABLED`), `LovyanGFX@^1.2.0`
- `WebSockets` — vendored in `lib/WebSockets/`
- **ESP-DSP** pre-built `libespressif__esp-dsp.a` (S3 optimized). `lib/esp_dsp_lite/` for native test fallback

## Commit Convention

```
feat: / fix: / docs: / refactor: / test: / chore:
```

**IMPORTANT**: Never add `Co-Authored-By` trailers to commit messages.

## Workflow Rules

- After code fixes, update concerns.md/MEMORY.md to mark resolved before commit/push/merge
- After implementation, automatically commit → push → PR → merge → delete branch. Show final commit hash on main
- When performing git ops: confirm branch, show hash after push, delete remote branch after merge
- Never modify `platformio.ini` board configuration unless explicitly asked
- After multi-agent workflows, clean up tool artifacts before committing
- When CI gates fail after merge, fix immediately in same session

## Documentation Site (Docusaurus v3)

`docs-site/` — deployed to GitHub Pages. Design token pipeline: `src/design_tokens.h` → `tools/extract_tokens.js` → CSS for web UI + docs site. MDX: escape `\{variable\}` outside code blocks. Internal docs: `docs-internal/`.

Site structure: 3 sidebars (`userSidebar`, `devSidebar`, `enterpriseSidebar`). Blog enabled at `/blog` (release notes). Showcase page at `/showcase`. Local search via `@easyops-cn/docusaurus-search-local` (15 results, blog+pages indexed). Enterprise section: `docs/enterprise/` (overview, oem-integration, production-deployment, certification, support-tiers).
