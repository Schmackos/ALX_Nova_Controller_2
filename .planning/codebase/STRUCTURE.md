# Project Structure

**Analysis Date:** 2026-03-25

## Directory Layout

```
ALX_Nova_Controller_2/
├── src/                        # Firmware source (C/C++)
│   ├── state/                  # 15 domain-specific state headers + enums
│   ├── hal/                    # Hardware Abstraction Layer (86 files)
│   ├── drivers/                # Chip register maps (ESS SABRE, Cirrus, ES8311)
│   └── gui/                    # TFT GUI (LVGL + LovyanGFX)
│       └── screens/            # 15 screen implementations
├── web_src/                    # Web UI source (edit these, not web_pages.cpp)
│   ├── js/                     # 25 JavaScript files (numbered prefix)
│   ├── css/                    # CSS stylesheets
│   └── index.html              # HTML template
├── test/                       # C++ unit tests (Unity, native platform)
│   ├── test_<module>/          # 125 test module directories
│   └── test_mocks/             # Arduino/WiFi/MQTT/NVS mock implementations
├── e2e/                        # End-to-end browser tests (Playwright)
│   ├── tests/                  # 57 spec files
│   ├── pages/                  # 19 Page Object Models
│   ├── helpers/                # Fixtures, WS assertions, selectors
│   ├── fixtures/               # WS + API fixture data
│   └── mock-server/            # Express + WS state mock
├── device_tests/               # On-device tests (pytest + pyserial)
│   ├── tests/                  # 21 test modules
│   └── utils/                  # Device communication utilities
├── lib/                        # Vendored libraries
│   ├── WebSockets/             # Vendored WebSockets library
│   └── esp_dsp_lite/           # ESP-DSP fallback for native tests
├── tools/                      # Build/dev tooling (Node.js, Python, shell)
├── docs-site/                  # Docusaurus v3 documentation site
│   └── docs/                   # User, developer, enterprise docs
├── docs-internal/              # Internal design docs (not published)
│   ├── architecture/           # Architecture decision records
│   ├── hardware/               # Hardware design notes
│   ├── planning/               # Feature planning docs
│   └── development/            # Development guides
├── .planning/                  # GSD planning documents
│   └── codebase/               # Codebase analysis (this file)
├── .github/                    # CI/CD workflows
│   └── workflows/              # 7 workflow files
├── .githooks/                  # Git pre-commit hooks
├── logs/                       # Runtime log files (gitignored)
├── platformio.ini              # PlatformIO build configuration
├── partitions_ota.csv          # ESP32 partition table (OTA dual-slot)
└── CLAUDE.md                   # AI coding assistant instructions
```

## Module Boundaries

### `src/` — Firmware Source

**Audio Pipeline Core:**
| File | Purpose |
|------|---------|
| `src/audio_pipeline.h` | Public API: init, source/sink registration, matrix, format negotiation, ASRC lane config, pause/resume |
| `src/audio_pipeline.cpp` | Pipeline task (Core 1), 8-lane I/O, float32 processing, DoP DSD detection, atomic sentinel sink/source ops |
| `src/asrc.h` | ASRC public API: init, set_ratio, process_lane, bypass |
| `src/asrc.cpp` | Polyphase FIR engine (160 phases x 32 taps), per-lane state in PSRAM |
| `src/audio_input_source.h` | Input source struct: name, lane, read(), isActive(), getSampleRate(), isDsd, halSlot |
| `src/audio_output_sink.h` | Output sink struct: name, firstChannel, write(), isReady(), sampleRate, supportsDsd, halSlot |
| `src/sink_write_utils.h/.cpp` | Shared sink write helpers |

**I2S Driver:**
| File | Purpose |
|------|---------|
| `src/i2s_audio.h` | I2S types, port state, unified 3-port API declarations |
| `src/i2s_audio.cpp` | 3 ESP32-P4 I2S ports, port-generic STD/TDM config, RX/TX enable/disable |
| `src/i2s_port_api.h/.cpp` | REST endpoints for I2S port configuration |

**DSP Engine:**
| File | Purpose |
|------|---------|
| `src/dsp_pipeline.h` | 4-channel pre-matrix DSP: biquad, FIR, limiter, compressor, delay, gain — types, API |
| `src/dsp_pipeline.cpp` | DSP processing implementation (109KB, largest non-generated file) |
| `src/output_dsp.h/.cpp` | 8-channel post-matrix per-output DSP |
| `src/dsp_biquad_gen.c/.h` | RBJ EQ Cookbook biquad coefficient generator |
| `src/dsp_coefficients.cpp/.h` | Coefficient management and presets |
| `src/dsp_convolution.cpp/.h` | FIR convolution engine |
| `src/dsp_crossover.cpp/.h` | Crossover filter implementation |
| `src/dsp_rew_parser.cpp/.h` | REW (Room EQ Wizard) import parser |
| `src/dsp_api.h/.cpp` | REST endpoints for DSP configuration (63KB) |

**State Management:**
| File | Purpose |
|------|---------|
| `src/app_state.h` | AppState singleton — composes all domain state structs, dirty flags |
| `src/app_state.cpp` | AppState method implementations (setError, backoff, display setters) |
| `src/app_events.h` | Event bit definitions (17 assigned, 7 spare) |
| `src/app_events.cpp` | FreeRTOS event group init/signal/wait |
| `src/state/audio_state.h` | AudioState: ADC data, VU, format negotiation, ASRC, DSD flags |
| `src/state/hal_coord_state.h` | HalCoordState: 8-slot spinlock-protected device toggle queue |
| `src/state/hal_coord_state.cpp` | portMUX spinlock implementation for HalCoordState |
| `src/state/dsp_state.h` | DspSettingsState: enabled, bypass, preset index, CPU metrics |
| `src/state/wifi_state.h` | WifiState: SSID, AP mode, connection |
| `src/state/mqtt_state.h` | MqttState: broker, port, TLS, HA discovery |
| `src/state/dac_state.h` | DacState: EEPROM dirty tracking |
| `src/state/usb_audio_state.h` | UsbAudioState: connection, format, VU |
| `src/state/health_check_state.h` | HealthCheckState: last check time, overall status |
| `src/state/enums.h` | Shared enums (SensingMode, FftWindowType, AppFSMState) |
| `src/state/ethernet_state.h` | EthernetState: link, IP, static/DHCP |
| `src/state/general_state.h` | GeneralState: device name, serial number |
| `src/state/ota_state.h` | OtaState: update state, auto-update, version info |
| `src/state/display_state.h` | DisplayState: backlight, timeout, brightness |
| `src/state/buzzer_state.h` | BuzzerState: enabled, volume |
| `src/state/signal_gen_state.h` | SignalGenState: frequency, waveform, amplitude |
| `src/state/debug_state.h` | DebugState: log level, heap stats, serial level |

**HAL Framework** (`src/hal/`):

*Core:*
| File | Purpose |
|------|---------|
| `src/hal/hal_types.h` | Enums, structs, constants, ClockStatus, HalDeviceConfig, capability flags |
| `src/hal/hal_device.h` | HalDevice abstract base: lifecycle, getClockStatus(), dependency graph, power management, atomic setReady() |
| `src/hal/hal_device_manager.h/.cpp` | 32-slot singleton: register, remove, initAll (topo sort), healthCheckAll (self-healing), pin tracking, NVS faults |
| `src/hal/hal_driver_registry.h/.cpp` | Compatible-string to factory mapping (48 max entries) |
| `src/hal/hal_device_db.h/.cpp` | Device descriptor database (44 entries, 26 expansion devices) |
| `src/hal/hal_discovery.h/.cpp` | 3-tier discovery: I2C scan, EEPROM probe, manual. Rescan. Unmatched address tracking |
| `src/hal/hal_pipeline_bridge.h/.cpp` | HAL <-> audio pipeline synchronization: sink/source registration, activation/deactivation, slot mapping, correlation IDs |
| `src/hal/hal_builtin_devices.h/.cpp` | Register 14 onboard devices + `hal_wire_builtin_dependencies()` for dependency graph |
| `src/hal/hal_custom_device.h/.cpp` | Tier 1-3 user-defined custom devices |
| `src/hal/hal_settings.h/.cpp` | Per-device config persistence to LittleFS JSON |
| `src/hal/hal_api.h/.cpp` | 16 REST endpoints for HAL management |
| `src/hal/hal_eeprom_api.h/.cpp` | EEPROM REST endpoints |
| `src/hal/hal_init_result.h` | Init result struct (success/failure with reason string) |
| `src/hal/hal_i2c_bus.h/.cpp` | 3-bus I2C abstraction with per-bus mutex, SDIO guard for Bus 0 |
| `src/hal/hal_eeprom.h` | EEPROM type definitions |
| `src/hal/hal_eeprom_v3.h/.cpp` | EEPROM v3 format reader/matcher |

*Audio Device Abstractions:*
| File | Purpose |
|------|---------|
| `src/hal/hal_audio_device.h` | HalAudioDevice: adds configure(), setVolume(), setMute() to HalDevice |
| `src/hal/hal_audio_interfaces.h` | HalAudioDacInterface: DAC-specific operations |
| `src/hal/hal_audio_health_bridge.h/.cpp` | HAL audio health integration with flap guard |

*Bridge Modules:*
| File | Purpose |
|------|---------|
| `src/hal/hal_i2s_bridge.h/.cpp` | HAL <-> I2S port configuration bridge |
| `src/hal/hal_dsp_bridge.h/.cpp` | HAL <-> DSP pipeline bridge |

*ESS SABRE Family (shared base + generic patterns):*
| File | Purpose |
|------|---------|
| `src/hal/hal_ess_sabre_dac_base.h/.cpp` | Base class for all ESS SABRE DACs: I2C helpers, buildSink(), I2S TX lifecycle |
| `src/hal/hal_ess_sabre_adc_base.h/.cpp` | Base class for all ESS SABRE ADCs: I2C helpers, config, probe |
| `src/hal/hal_ess_dac_2ch.h/.cpp` | Pattern C: Generic 2ch ESS DAC (8 models) |
| `src/hal/hal_ess_dac_8ch.h/.cpp` | Pattern D: Generic 8ch ESS DAC (4 models) |
| `src/hal/hal_ess_adc_2ch.h/.cpp` | Pattern A: Generic 2ch ESS ADC (5 models) |
| `src/hal/hal_ess_adc_4ch.h/.cpp` | Pattern B: Generic 4ch ESS ADC with TDM (4 models) |

*Cirrus Logic Family:*
| File | Purpose |
|------|---------|
| `src/hal/hal_cirrus_dac_base.h/.cpp` | Base class for Cirrus Logic DACs: 8-bit + 16-bit paged I2C, DSD/DoP mode switching |
| `src/hal/hal_cirrus_dac_2ch.h/.cpp` | Pattern C: Generic 2ch Cirrus DAC (5 models, incl. DSD support) |

*TDM Support:*
| File | Purpose |
|------|---------|
| `src/hal/hal_tdm_deinterleaver.h/.cpp` | TDM 4/8ch -> stereo pair de-interleaving for multi-channel ADCs |
| `src/hal/hal_tdm_interleaver.h/.cpp` | Stereo pair -> TDM 4/8ch interleaving for multi-channel DACs |

*Onboard Device Drivers:*
| File | Purpose |
|------|---------|
| `src/hal/hal_pcm5102a.h/.cpp` | TI PCM5102A DAC (primary onboard DAC) |
| `src/hal/hal_es8311.h/.cpp` | Everest ES8311 codec (DAC+ADC, onboard) |
| `src/hal/hal_pcm1808.h/.cpp` | TI PCM1808 ADC (dual onboard ADCs) |
| `src/hal/hal_mcp4725.h/.cpp` | MCP4725 12-bit DAC for analog trim/bias |
| `src/hal/hal_ns4150b.h/.cpp` | NS4150B mono class-D amplifier (GPIO control) |
| `src/hal/hal_temp_sensor.h/.cpp` | ESP32-P4 internal temperature sensor |
| `src/hal/hal_display.h/.cpp` | ST7735S TFT display (SPI) |
| `src/hal/hal_encoder.h/.cpp` | Rotary encoder with push button |
| `src/hal/hal_buzzer.h/.cpp` | Passive buzzer (PWM) |
| `src/hal/hal_led.h/.cpp` | Board LED (GPIO) |
| `src/hal/hal_relay.h/.cpp` | Amplifier relay (GPIO) |
| `src/hal/hal_button.h/.cpp` | Factory reset button (GPIO) |
| `src/hal/hal_siggen.h/.cpp` | Signal generator HAL wrapper |
| `src/hal/hal_signal_gen.h/.cpp` | Hardware signal generator (MCPWM) |
| `src/hal/hal_usb_audio.h/.cpp` | USB Audio HAL wrapper (TinyUSB UAC2) |

**Chip Register Maps** (`src/drivers/`):
| Pattern | Files |
|---------|-------|
| ESS SABRE DAC | `es9017_regs.h`, `es9020_dac_regs.h`, `es9027pro_regs.h`, `es9028pro_regs.h`, `es9033q_regs.h`, `es9038pro_regs.h`, `es9038q2m_regs.h`, `es9039pro_regs.h`, `es9039q2m_regs.h`, `es9069q_regs.h`, `es9081_regs.h`, `es9082_regs.h` |
| ESS SABRE ADC | `es9820_regs.h`, `es9821_regs.h`, `es9822pro_regs.h`, `es9823pro_regs.h`, `es9826_regs.h`, `es9840_regs.h`, `es9841_regs.h`, `es9842pro_regs.h`, `es9843pro_regs.h` |
| Cirrus Logic | `cs43130_regs.h`, `cs43131_regs.h`, `cs43198_regs.h`, `cs4398_regs.h`, `cs4399_regs.h` |
| Shared | `ess_sabre_common.h`, `cirrus_dac_common.h` |
| Onboard | `es8311_regs.h` |

**WebSocket + Network:**
| File | Purpose |
|------|---------|
| `src/websocket_command.cpp` | WS incoming message handler (JSON command dispatch, 63KB) |
| `src/websocket_broadcast.cpp` | All sendXxxState() broadcast functions (43KB) |
| `src/websocket_auth.cpp` | WS token authentication |
| `src/websocket_cpu_monitor.cpp` | CPU utilization tracking for WS broadcast |
| `src/websocket_handler.h` | WS handler declarations, binary message types |
| `src/websocket_internal.h` | Internal WS helpers |
| `src/wifi_manager.cpp/.h` | Multi-network WiFi, AP mode, captive portal |
| `src/eth_manager.cpp/.h` | Ethernet with static/DHCP, config revert timer |
| `src/mqtt_handler.cpp/.h` | MQTT connection, subscribe, TLS |
| `src/mqtt_publish.cpp` | MQTT publish functions |
| `src/mqtt_ha_discovery.cpp` | Home Assistant auto-discovery (76KB) |
| `src/mqtt_task.cpp/.h` | MQTT task on Core 0, 20Hz |

**Other Modules:**
| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point: setup(), loop(), route registration (57KB) |
| `src/config.h` | Constants, pin definitions, build flags, capacity limits |
| `src/globals.h` | Global extern declarations |
| `src/settings_manager.cpp/.h` | JSON + NVS settings load/save (85KB) |
| `src/auth_handler.cpp/.h` | PBKDF2-SHA256 auth, session management, rate limiting |
| `src/http_security.h` | Security headers, sanitize_filename, json_response envelope |
| `src/health_check.h/.cpp` | 10-category health check system (2 phases) |
| `src/health_check_api.h/.cpp` | Health check REST endpoint |
| `src/smart_sensing.cpp/.h` | Audio-triggered amplifier control (always-on, timer, signal-detect modes) |
| `src/signal_generator.cpp/.h` | Software signal generator (sine, square, saw, noise) |
| `src/thd_measurement.cpp/.h` | THD+N measurement |
| `src/psram_alloc.h/.cpp` | PSRAM-first allocator with SRAM fallback (64KB cap) |
| `src/heap_budget.h/.cpp` | Heap pressure tracking and alerts |
| `src/task_monitor.cpp/.h` | FreeRTOS task monitoring (stack watermarks, loop timing) |
| `src/debug_serial.h/.cpp` | LOG_D/I/W/E macros with configurable level |
| `src/crash_log.h/.cpp` | Boot crash ring buffer (LittleFS) |
| `src/diag_journal.h/.cpp` | 32-hot + 800-persistent diagnostic event ring |
| `src/diag_event.h` | Diagnostic event type enumeration |
| `src/diag_error_codes.h` | Error code taxonomy |
| `src/diag_api.h/.cpp` | Diagnostic REST endpoints |
| `src/ota_updater.cpp/.h` | OTA firmware update (SHA256, backoff retry) |
| `src/ota_certs.h` | TLS certificate chains |
| `src/button_handler.cpp/.h` | Multi-press button detection |
| `src/buzzer_handler.cpp/.h` | Buzzer tone patterns |
| `src/usb_audio.cpp/.h` | USB Audio class (TinyUSB UAC2) |
| `src/design_tokens.h` | Design token constants for web/docs CSS pipeline |
| `src/strings.h` | Shared string constants (19KB) |
| `src/utils.cpp/.h` | Utility functions |
| `src/rate_limiter.cpp/.h` | Generic rate limiter |
| `src/login_page.h` | Login page HTML (pre-gzipped byte array) |
| `src/dac_eeprom.cpp/.h` | EEPROM read/write for mezzanine board identification |
| `src/dac_hal.cpp/.h` | DAC abstraction layer (legacy, being replaced by HAL) |
| `src/safe_snr_sfdr.c` | Safe SNR/SFDR calculation |
| `src/idf_component.yml` | ESP-IDF component descriptor |

### `web_src/` — Web Frontend Source

**JS Files (numbered for concatenation order):**
| File | Purpose |
|------|---------|
| `web_src/js/01-core.js` | apiFetch() with auto /api/ to /api/v1/ rewrite, switchTab(), initTheme() |
| `web_src/js/02-ws-router.js` | WebSocket connection, message dispatch router |
| `web_src/js/03-app-state.js` | Client-side state management |
| `web_src/js/04-shared-audio.js` | Shared audio utilities |
| `web_src/js/05-audio-tab.js` | Audio tab UI (input/output controls, matrix, VU meters) |
| `web_src/js/06-canvas-helpers.js` | Canvas drawing utilities |
| `web_src/js/06-peq-overlay.js` | Parametric EQ visualization overlay |
| `web_src/js/07-ui-core.js` | Core UI helpers (toasts, modals, confirm dialogs) |
| `web_src/js/08-ui-status.js` | Status bar, WiFi/MQTT indicators |
| `web_src/js/09-audio-viz.js` | Audio visualization (waveform, spectrum, VU) |
| `web_src/js/13-signal-gen.js` | Signal generator controls |
| `web_src/js/15-hal-devices.js` | HAL device cards, dependency badges, clock status icons (75KB) |
| `web_src/js/15a-yaml-parser.js` | YAML parser for custom device config |
| `web_src/js/19-ethernet.js` | Ethernet configuration UI |
| `web_src/js/20-wifi-network.js` | WiFi network management UI |
| `web_src/js/21-mqtt-settings.js` | MQTT settings UI |
| `web_src/js/22-settings.js` | General settings UI |
| `web_src/js/23-firmware-update.js` | OTA update UI |
| `web_src/js/24-hardware-stats.js` | Hardware stats/debug UI |
| `web_src/js/25-debug-console.js` | Debug console UI |
| `web_src/js/26-support.js` | Support page |
| `web_src/js/27-auth.js` | Authentication UI |
| `web_src/js/27a-health-dashboard.js` | Health check dashboard UI |
| `web_src/js/28-init.js` | App initialization |
| `web_src/js/29-i2s-ports.js` | I2S port configuration UI |

**Other:**
- `web_src/css/` — CSS stylesheets
- `web_src/index.html` — HTML template (138KB)
- `web_src/.eslintrc.json` — ESLint config for frontend JS (17KB, includes globals list)

### `test/` — C++ Unit Tests

- 125 test module directories (`test/test_<module>/`)
- Each contains a single `test_<module>.cpp` file with Unity test runner
- `test/test_mocks/` — Mock implementations for Arduino, WiFi, MQTT, NVS, I2C Wire, FreeRTOS, LittleFS
- Compiled with `-D UNIT_TEST -D NATIVE_TEST` on `native` platform
- `test_build_src = no` — tests compile standalone (no `src/` linkage)

### `e2e/` — End-to-End Browser Tests

- `e2e/tests/` — 57 spec files (Playwright)
- `e2e/pages/` — 19 Page Object Models (one per tab/dialog)
- `e2e/helpers/` — Test fixtures, WS assertion helpers, CSS selectors
- `e2e/fixtures/` — WebSocket message fixtures, API response fixtures
- `e2e/mock-server/` — Express + WS mock server that simulates firmware
- `e2e/playwright.config.js` — Playwright configuration
- `e2e/package.json` — Dependencies (Playwright, Express, ws)

### `device_tests/` — On-Device Tests

- `device_tests/tests/` — 21 pytest test modules
- `device_tests/conftest.py` — Fixtures, device communication setup
- `device_tests/utils/` — Serial and HTTP device communication utilities
- `device_tests/pytest.ini` — Pytest configuration with markers
- `device_tests/requirements.txt` — Python dependencies

## Entry Points

**Firmware:**
- `src/main.cpp` — Arduino `setup()` (line 173) and `loop()` (line 863)

**Web UI Build:**
- `node tools/build_web_assets.js` — Concatenates JS/CSS/HTML into `src/web_pages.cpp` + `src/web_pages_gz.cpp`

**Unit Tests:**
- `pio test -e native` — Run all 125 C++ test modules (3701 tests)

**E2E Tests:**
- `cd e2e && npx playwright test` — Run all 57 browser test specs (358 tests)

**On-Device Tests:**
- `cd device_tests && pytest tests/ --device-port COM8 --device-ip <IP>` — 21 modules (206 tests)

**Documentation Site:**
- `cd docs-site && npm run build` — Build Docusaurus site

**Static Analysis:**
- `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json` — Lint frontend JS
- `node tools/find_dups.js` — Find duplicate function declarations
- `node tools/check_missing_fns.js` — Find missing function implementations

## Configuration Files

| File | Purpose |
|------|---------|
| `platformio.ini` | PlatformIO build config — 3 environments: `esp32-p4` (firmware), `p4_hosted_update` (OTA test), `native` (unit tests) |
| `partitions_ota.csv` | ESP32 partition table with dual OTA slots |
| `src/config.h` | Compile-time constants: pins, capacity limits, thresholds, version |
| `web_src/.eslintrc.json` | ESLint rules for frontend JS (includes all global function declarations) |
| `e2e/playwright.config.js` | Playwright test configuration |
| `device_tests/pytest.ini` | Pytest config with custom markers |
| `docs-site/docusaurus.config.ts` | Docusaurus site configuration |
| `.github/workflows/tests.yml` | CI pipeline: cpp-tests, cpp-lint, js-lint, e2e-tests |
| `.githooks/pre-commit` | Pre-commit: find_dups + check_missing_fns + ESLint |
| `src/gui/lv_conf.h` | LVGL v9.4 configuration |
| `src/gui/lgfx_config.h` | LovyanGFX ST7735S display config |

## Generated Files (DO NOT EDIT)

| File | Source | Regenerate With |
|------|--------|-----------------|
| `src/web_pages.cpp` | `web_src/` (JS + CSS + HTML) | `node tools/build_web_assets.js` |
| `src/web_pages_gz.cpp` | `web_src/` (gzipped version) | `node tools/build_web_assets.js` |
| `src/web_pages.h` | `web_src/` (declarations) | `node tools/build_web_assets.js` |
| `compile_commands.json` | PlatformIO build | `pio run` (auto-generated) |
| `tools/doc-mapping.json` | Source code analysis | `node tools/generate_docs.js` |

## Key File Index

**Most Important Files (by architectural significance):**

| File | Lines | Purpose |
|------|-------|---------|
| `src/main.cpp` | ~1400 | Firmware entry: setup(), loop(), all route registration |
| `src/audio_pipeline.cpp` | ~1600 | Core 1 audio pipeline: read -> ASRC -> DSP -> matrix -> sink, atomic ops |
| `src/dsp_pipeline.cpp` | ~2800 | 4ch pre-matrix DSP engine (largest non-generated file) |
| `src/hal/hal_device_manager.cpp` | ~500 | HAL singleton: registration, topo sort initAll, self-healing healthCheckAll |
| `src/hal/hal_pipeline_bridge.cpp` | ~700 | HAL<->pipeline sync: activate/deactivate, slot mapping, correlation |
| `src/hal/hal_discovery.cpp` | ~560 | 3-tier device discovery engine |
| `src/hal/hal_api.cpp` | ~900 | 16 HAL REST endpoints |
| `src/hal/hal_device.h` | ~160 | HalDevice base: lifecycle, clock diagnostics, dependency graph, power management |
| `src/hal/hal_types.h` | ~290 | All HAL enums, structs, capability flags, ClockStatus, config validation |
| `src/app_state.h` | ~256 | AppState singleton: 15 domains, dirty flags, event signals |
| `src/app_events.h` | ~38 | Event bit definitions (17 assigned, 7 spare) |
| `src/config.h` | ~200 | All compile-time constants: pins, limits, thresholds |
| `src/asrc.h` | ~105 | Polyphase ASRC API and capacity constants |
| `src/websocket_command.cpp` | ~1500 | WS command handler (JSON dispatch) |
| `src/websocket_broadcast.cpp` | ~1050 | All state broadcast functions |
| `src/settings_manager.cpp` | ~2100 | Settings load/save (JSON + NVS) |
| `src/http_security.h` | ~105 | Security headers, filename sanitization, JSON envelope |

## Naming Conventions

**Files:**
- C++ source: `snake_case.cpp/.h` (e.g., `audio_pipeline.cpp`, `hal_device_manager.h`)
- HAL drivers: `hal_<vendor>_<model>.cpp/.h` (e.g., `hal_ess_dac_2ch.cpp`, `hal_cirrus_dac_2ch.cpp`)
- HAL base classes: `hal_<vendor>_<family>_base.cpp/.h` (e.g., `hal_ess_sabre_dac_base.cpp`)
- Register maps: `<vendor><model>_regs.h` (e.g., `es9038pro_regs.h`, `cs43198_regs.h`)
- State headers: `<domain>_state.h` (e.g., `audio_state.h`, `wifi_state.h`)
- API files: `<module>_api.cpp/.h` (e.g., `hal_api.cpp`, `dsp_api.cpp`)
- Test directories: `test_<module>/` containing `test_<module>.cpp`
- JS files: `NN-<module>.js` (numbered prefix for concatenation order)
- E2E specs: `<feature>.spec.js`
- E2E pages: `<feature>.page.js`

**Directories:**
- `src/hal/` — All HAL code
- `src/state/` — All state structs
- `src/gui/screens/` — All GUI screen implementations
- `src/drivers/` — Chip register maps only (no logic)
- `web_src/js/` — Frontend JS (numbered prefix)
- `test/test_<module>/` — One test module per directory

## Where to Add New Code

**New HAL Device Driver:**
1. Register map: `src/drivers/<chip>_regs.h`
2. Driver header: `src/hal/hal_<chip>.h` (inherit from appropriate base: `HalEssSabreDacBase`, `HalCirrusDacBase`, `HalEssAdc2ch`, etc.)
3. Driver implementation: `src/hal/hal_<chip>.cpp`
4. Factory + registration: add entry in `src/hal/hal_builtin_devices.cpp`
5. Descriptor: add entry in `src/hal/hal_device_db.cpp`
6. Tests: `test/test_hal_<chip>/test_hal_<chip>.cpp`
7. Check capacity: `HAL_MAX_DRIVERS` (48), `HAL_DB_MAX_ENTRIES` (48)

**New REST API Endpoint:**
1. Implementation: `src/<module>_api.cpp` (or add to existing)
2. Route registration: `src/main.cpp` using `server_on_versioned("/api/<path>", HTTP_METHOD, handler)`
3. Use `requireAuth()` guard for protected endpoints
4. Use `server_send()` or `json_response()` for responses (security headers automatic)
5. E2E test: `e2e/tests/<feature>.spec.js`
6. Mock server route: `e2e/mock-server/routes/<feature>.js`

**New WebSocket Command:**
1. Handler: add case in `src/websocket_command.cpp` dispatch
2. State broadcast: add `sendXxxState()` in `src/websocket_broadcast.cpp`
3. Dirty flag: add `markXxxDirty()` / `isXxxDirty()` / `clearXxxDirty()` in `src/app_state.h`
4. Event bit: assign next free bit in `src/app_events.h` (7 spare: 19-23)
5. Main loop: add dirty-flag check in `src/main.cpp` `loop()`
6. WS fixture: update `e2e/fixtures/ws-messages/` and `e2e/helpers/ws-helpers.js`

**New Web UI Tab:**
1. JS file: `web_src/js/NN-<tab>.js` (pick appropriate number for load order)
2. HTML: add tab button + content div in `web_src/index.html`
3. Rebuild: `node tools/build_web_assets.js`
4. ESLint globals: add any new global functions to `web_src/.eslintrc.json`
5. E2E test: `e2e/tests/<tab>.spec.js` + POM in `e2e/pages/<tab>.page.js`

**New State Domain:**
1. Header: `src/state/<domain>_state.h`
2. Include: add to `src/app_state.h`
3. Field: add `<Domain>State <domain>;` to AppState class
4. Dirty flag: add `markXxxDirty()` / `isXxxDirty()` / `clearXxxDirty()` pattern
5. Event bit: assign in `src/app_events.h`

**New C++ Unit Test:**
1. Create directory: `test/test_<module>/`
2. Create file: `test/test_<module>/test_<module>.cpp`
3. Include Unity: `#include <unity.h>`
4. Add setUp()/tearDown()/main() with RUN_TEST() calls
5. Run: `pio test -e native -f test_<module>`

**New Feature (Full Stack):**
1. Firmware logic in `src/`
2. State in `src/state/<domain>_state.h`
3. REST endpoint in `src/<module>_api.cpp` + route in `src/main.cpp`
4. WS command in `src/websocket_command.cpp`
5. WS broadcast in `src/websocket_broadcast.cpp`
6. Web UI in `web_src/js/NN-<feature>.js`
7. Unit tests in `test/test_<module>/`
8. E2E tests in `e2e/tests/<feature>.spec.js`
9. Rebuild web: `node tools/build_web_assets.js`

## Special Directories

**`lib/`:**
- Purpose: Vendored libraries that cannot be installed via PlatformIO library manager
- `lib/WebSockets/` — Vendored WebSockets library (cannot use upstream due to ESP32-P4 patches)
- `lib/esp_dsp_lite/` — Minimal ESP-DSP API shim for native test builds (firmware uses pre-built `libespressif__esp-dsp.a`)
- Generated: No
- Committed: Yes

**`.pio/`:**
- Purpose: PlatformIO build cache, toolchains, downloaded libraries
- Generated: Yes (by `pio run`)
- Committed: No (gitignored)

**`logs/`:**
- Purpose: Runtime log files from device tests and debugging sessions
- Generated: Yes
- Committed: No (gitignored)

**`.planning/codebase/`:**
- Purpose: Codebase analysis documents used by GSD planning system
- Generated: Yes (by codebase mapper agents)
- Committed: Yes

**`docs-internal/`:**
- Purpose: Internal design documents, architecture decision records, feature planning
- Subdirectories: `architecture/`, `hardware/`, `planning/`, `development/`, `user/`, `archive/`
- Generated: No (hand-written)
- Committed: Yes

**`docs-site/`:**
- Purpose: Public documentation site (Docusaurus v3, deployed to GitHub Pages)
- Structure: `docs/user/`, `docs/developer/`, `docs/enterprise/`, blog, showcase
- Generated: Build output in `docs-site/build/` (gitignored)
- Committed: Source files yes, build output no

---

*Structure analysis: 2026-03-25*
