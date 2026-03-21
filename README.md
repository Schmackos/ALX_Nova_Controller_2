# ALX Nova Controller 2

![Tests](https://github.com/Schmackos/ALX_Nova_Controller_2/actions/workflows/tests.yml/badge.svg)
![Version](https://img.shields.io/badge/version-1.12.2-blue)
![Platform](https://img.shields.io/badge/platform-ESP32--P4-green)
![Tests](https://img.shields.io/badge/tests-2316%20passing-brightgreen)

ESP32-P4 based intelligent amplifier controller with a modular audio pipeline, HAL device framework, DSP engine, web configuration UI, LVGL touchscreen GUI, WiFi/Ethernet connectivity, MQTT/Home Assistant integration, and OTA firmware updates. Built with PlatformIO and the Arduino framework.

## Features

### Audio Pipeline
- 8-lane input: dual PCM1808 I2S ADC, ESS SABRE expansion ADC, software signal generator, USB Audio (TinyUSB UAC2)
- Per-input DSP: biquad IIR parametric EQ, FIR filters, limiter, compressor, gain, delay, polarity, mute
- 16x16 routing matrix with per-crosspoint gain
- Per-output DSP with double-buffered config for glitch-free updates
- Slot-indexed multi-sink dispatch to HAL-managed output devices
- Real-time RMS/VU/peak metering with WebSocket broadcast
- REW Equalizer APO and miniDSP import/export support

### HAL Device Framework
- Up to 24 managed devices with 56-pin tracking and GPIO conflict prevention
- Device lifecycle: UNKNOWN -> DETECTED -> CONFIGURING -> AVAILABLE <-> UNAVAILABLE -> ERROR / REMOVED / MANUAL
- 3-tier discovery: I2C bus scan, EEPROM probe, manual configuration
- ESS SABRE ADC expansion support: 9 ADC models with EEPROM auto-discovery on I2C Bus 2
- State change callbacks drive automatic audio pipeline sink management via HAL-Pipeline Bridge
- Per-device config persistence to LittleFS JSON
- REST API for device CRUD, scanning, and preset database
- Graduated heap and PSRAM memory pressure management

### Web Configuration
- Self-contained offline-capable web UI served from the ESP32
- Unified Audio tab: HAL-driven channel strips, 16x16 matrix routing, DSP overlays with frequency response graph
- HAL device management panel
- Health Dashboard with device diagnostics and memory monitoring
- WiFi, MQTT, OTA, and system settings
- Debug console with module filtering, search/highlight, and WebSocket log forwarding
- Binary WebSocket frames for waveform and spectrum data

### LVGL GUI
- LVGL v9.4 on ST7735S 128x160 TFT (landscape) with rotary encoder input
- Desktop carousel, status dashboard, device screens, WiFi/MQTT settings
- Screen stack navigation with transition animations
- Dark/light theme with orange accent

### Connectivity
- WiFi via ESP32-C6 co-processor (WiFi 6)
- 100 Mbps Ethernet (full duplex)
- MQTT with Home Assistant auto-discovery
- OTA firmware updates with SHA256 verification from GitHub releases

### Smart Auto-Sensing
- Automatic voltage detection and amplifier relay control
- Configurable auto-off timer (1-60 minutes)
- Adjustable voltage threshold
- Three modes: Always On, Always Off, Smart Auto

### Signal Generator
- Sine, square, noise, frequency sweep waveforms
- Software injection into audio pipeline (dynamically assigned HAL lane) and PWM output

### USB Audio
- TinyUSB UAC2 high-speed speaker device on native USB OTG
- PCM16 format, lock-free SPSC ring buffer (PSRAM)

## Hardware

| Component | Part | Interface |
|-----------|------|-----------|
| **Board** | Waveshare ESP32-P4-WiFi6-DEV-Kit | -- |
| **Audio ADC** | 2x PCM1808 | I2S (dual-master, shared clock) |
| **Expansion ADC** | ESS SABRE family (9 models) | I2C Bus 2 + I2S/TDM (mezzanine) |
| **Audio DAC** | PCM5102A | I2S |
| **Codec** | ES8311 | I2C Bus 1 (onboard, hardware volume) |
| **Amplifier** | NS4150B Class-D | GPIO 53 |
| **DAC** | MCP4725 12-bit | I2C |
| **Display** | ST7735S 128x160 TFT | SPI |
| **Input** | Rotary encoder (A/B/SW) | GPIO |
| **Other** | Piezo buzzer, status LED, amplifier relay, reset button | GPIO |

## Quick Start

### Build and Upload

```bash
pio run                           # Build firmware
pio run --target upload           # Upload to device (COM8)
pio device monitor                # Serial monitor (115200 baud)
pio test -e native                # Run all 2316 tests
pio test -e native -f test_wifi   # Run a specific test module
pio test -e native -v             # Verbose test output
```

### First-Time Setup

1. Power on the ESP32-P4
2. Connect to the WiFi AP: `ALX-XXXXXXXXXXXX` (password: `12345678`)
3. Navigate to `http://192.168.4.1`
4. Configure WiFi credentials
5. The device connects to your network and is accessible at its assigned IP

## Architecture

### Dual-Core FreeRTOS Design

**Core 1** is reserved exclusively for audio:
- `audio_pipeline_task` (priority 3) -- DMA-driven audio processing
- `loopTask` / Arduino main loop (priority 1) -- WebSocket broadcasts, periodic timers

**Core 0** handles everything else:
- `gui_task` -- LVGL display rendering
- `mqtt_task` -- dedicated MQTT reconnect and publish (20 Hz)
- `usb_audio_task` -- TinyUSB UAC2 polling
- One-shot OTA tasks

### Key Patterns

- **Event-driven state**: `AppState` singleton with dirty flags and a 24-bit FreeRTOS event group. `app_events_wait(5)` replaces `delay(5)` -- sub-microsecond wake on any state change, 5 ms fallback tick
- **HAL-Pipeline Bridge**: HAL device state changes fire callbacks that automatically create/remove audio pipeline sinks via slot-indexed API
- **Lock-free cross-core reads**: Volatile `_ready` flags on output sinks allow the audio task on Core 1 to skip unavailable devices without locks
- **Minimal broadcast traffic**: Dirty flags ensure WebSocket and MQTT only publish on actual state changes

### Architecture Diagrams

Eight Mermaid diagrams are available in [`docs-internal/architecture/`](docs-internal/architecture/):

- `system-architecture.mmd` -- High-level system overview
- `hal-lifecycle.mmd` -- HAL device state machine
- `hal-pipeline-bridge.mmd` -- HAL to audio pipeline integration
- `boot-sequence.mmd` -- Startup and initialization order
- `event-architecture.mmd` -- Event group and dirty flag flow
- `sink-dispatch.mmd` -- Audio output sink dispatch logic
- `memory-pressure.mmd` -- Graduated heap and PSRAM pressure states
- `ess-sabre-discovery.mmd` -- ESS SABRE ADC discovery and driver selection flow

## Pin Configuration

| Group | Pins |
|-------|------|
| **I2S ADC1** | BCK=20, DOUT=23, LRC=21, MCLK=22 |
| **I2S ADC2** | DOUT2=25 (shares BCK/LRC/MCLK with ADC1) |
| **I2S DAC TX** | DOUT=24 (full-duplex on I2S0) |
| **TFT Display** | MOSI=2, SCLK=3, CS=4, DC=5, RST=6, BL=26 |
| **Rotary Encoder** | A=32, B=33, SW=36 |
| **Control** | LED=1, Amplifier=27, Buzzer=45, Reset=46 |
| **ES8311 I2C** | SDA=7, SCL=8 (Bus 1, onboard) |
| **I2C Expansion** | SDA=28, SCL=29 (Bus 2) |
| **Signal Gen PWM** | GPIO 47 |
| **NS4150B Enable** | GPIO 53 |

Full pin definitions are in `platformio.ini` build flags with fallback defaults in `src/config.h`.

## API Endpoints

### Audio and DSP
- `GET/POST /api/audio/*` -- Pipeline config, DSP stages, routing matrix

### HAL Devices
- `GET /api/hal/devices` -- List all managed devices
- `POST /api/hal/scan` -- Trigger device discovery
- `PUT /api/hal/devices` -- Update device configuration
- `DELETE /api/hal/devices` -- Remove a device
- `POST /api/hal/devices/reinit` -- Reinitialize a device
- `GET /api/hal/db/presets` -- Device database presets
- `GET /api/psram/status` -- PSRAM health and allocation tracking

### Smart Sensing
- `GET/POST /api/smartsensing` -- State and settings

### WiFi
- `GET /api/wifistatus` -- Connection status
- `GET /api/wifiscan` -- Available networks
- `POST /api/wificonfig` -- Configure credentials

### MQTT
- `GET /api/mqttstatus` -- Broker connection status
- `POST /api/mqttconfig` -- Configure broker settings

### OTA Updates
- `GET /api/checkupdate` -- Check for new firmware
- `POST /api/startupdate` -- Begin OTA update
- `GET /api/updatestatus` -- Download progress

### System
- `GET/POST /api/settings` -- All settings (get/update)
- `GET /api/settings/export` -- Export configuration
- `POST /api/settings/import` -- Import configuration
- `POST /api/factoryreset` -- Reset to defaults
- `POST /api/reboot` -- Restart device

### Diagnostics
- `GET /api/diag/snapshot` -- Diagnostic event snapshot

### WebSocket (Port 81)
Real-time state updates, audio levels (JSON), waveform and spectrum data (binary frames).

## Project Structure

```
src/
  main.cpp                      Application entry, task orchestration
  app_state.h + state/          Singleton state with 15 domain-specific headers
  app_events.h                  FreeRTOS event group (24-bit)
  config.h                      Pin definitions, task config, version
  audio_pipeline.cpp/.h         8-lane input -> 16x16 matrix -> 8-slot sink dispatch
  audio_output_sink.h           Slot-indexed output sink API
  dac_hal.cpp                   I2S DAC driver + bus utilities
  i2s_audio.cpp/.h              Dual PCM1808 ADC, I2S configuration
  dsp_pipeline.cpp/.h           4-channel input DSP engine
  output_dsp.cpp/.h             Per-output mono DSP
  signal_generator.cpp/.h       Test signal generation
  smart_sensing.cpp/.h          Voltage detection, auto-off
  wifi_manager.cpp/.h           WiFi client/AP management
  mqtt_handler.cpp/.h           MQTT + Home Assistant discovery
  mqtt_task.cpp/.h              Dedicated MQTT FreeRTOS task
  ota_updater.cpp/.h            OTA with SHA256 verification
  websocket_handler.cpp/.h      Real-time WebSocket server
  settings_manager.cpp/.h       NVS persistence
  usb_audio.cpp/.h              TinyUSB UAC2 speaker device
  psram_alloc.cpp/.h            Unified PSRAM allocation with SRAM fallback
  heap_budget.cpp/.h            Per-subsystem allocation tracker
  sink_write_utils.cpp/.h       Shared float buffer utilities for sinks
  pipeline_api.cpp/.h           REST API for audio pipeline matrix
  dac_api.cpp/.h                REST API for DAC control
  psram_api.cpp/.h              REST API for PSRAM health monitoring
  hal/
    hal_device_manager.*        Device lifecycle management (24 slots)
    hal_pipeline_bridge.*       HAL -> audio pipeline integration
    hal_discovery.*             3-tier device discovery
    hal_device_db.*             Device database + config persistence
    hal_api.*                   REST API for device CRUD
    hal_ess_sabre_adc_base.*    Abstract base class for ESS SABRE ADC family
    hal_es9822pro.*             ESS ES9822PRO 2-channel SABRE ADC
    hal_es9843pro.*             ESS ES9843PRO 4-channel SABRE ADC (TDM)
    hal_relay.*                 GPIO relay for amplifier control
    hal_tdm_deinterleaver.*     TDM frame splitter for 4-channel ADCs
    hal_es8311.*                ES8311 codec driver
    hal_pcm5102a.*              PCM5102A DAC driver
    hal_pcm1808.*               PCM1808 ADC driver
    hal_ns4150b.*               NS4150B amplifier driver
    hal_mcp4725.*               MCP4725 12-bit DAC driver
    hal_temp_sensor.*           ESP32-P4 internal temperature sensor
    hal_builtin_devices.*       Driver registry (compatible string -> factory)
  gui/                          LVGL GUI (guarded by GUI_ENABLED)
    gui_manager.*               Init, FreeRTOS task, screen management
    gui_navigation.*            Screen stack with transitions
    gui_theme.*                 Orange accent theme, dark/light mode
    screens/                    Home, Control, WiFi, MQTT, Devices, etc.

web_src/                        Web UI source (edit here, not src/)
  index.html                    HTML shell
  css/01-05-*.css               Modular CSS (variables, layout, components)
  js/01-28-*.js                 JS modules in load order

test/                           Unity test modules (native platform)
  test_mocks/                   Arduino, WiFi, MQTT, NVS mock implementations
  test_*/                       87 test module directories

e2e/                            Playwright browser tests (107 tests, 22 specs)

docs-site/                      Docusaurus v3 documentation site (26 pages)

docs-internal/
  architecture/                 Mermaid diagrams + analysis documents
  planning/                     Feature plans and designs
  development/                  CI/CD, release process, OTA documentation
  hardware/                     PCM1808 integration, TFT setup
  user/                         Quick start guide, user manual
```

## Testing

2316 tests across 87 modules on the native platform using the Unity framework. Tests run on the host machine with gcc/MinGW -- no hardware required.

```bash
pio test -e native                # Run all tests
pio test -e native -f test_wifi   # Run a specific module
pio test -e native -v             # Verbose output
```

Mock implementations in `test/test_mocks/` simulate Arduino core functions, WiFi, MQTT (`PubSubClient`), NVS (`Preferences`), and I2C (`Wire`).

107 Playwright browser tests across 22 specs verify the web UI against a mock Express server -- no real hardware needed.

```bash
cd e2e && npx playwright test       # Run all browser tests
```

**CI**: GitHub Actions runs 4 parallel quality gates (C++ tests, C++ lint, JS lint, E2E browser tests) and builds ESP32-P4 firmware on every push and pull request to `main` and `develop`.

## Development

### Web UI Workflow

The web interface source lives in `web_src/`. The files `src/web_pages.cpp` and `src/web_pages_gz.cpp` are auto-generated -- never edit them directly.

```bash
# After editing any file in web_src/:
node tools/build_web_assets.js

# Then build firmware:
pio run
```

### Contributing

1. Create a feature branch
2. Make changes
3. Ensure tests pass: `pio test -e native`
4. Build firmware: `pio run`
5. Create a pull request

### Commit Convention

```
feat: Add new feature
fix: Fix bug
docs: Update documentation
refactor: Code refactoring
test: Add/update tests
chore: Maintenance tasks
```

## Version History

See [RELEASE_NOTES.md](RELEASE_NOTES.md) for the detailed changelog.

## License

Copyright 2024-2026 ALX Audio

## Links

- Repository: https://github.com/Schmackos/ALX_Nova_Controller_2
- Issues: https://github.com/Schmackos/ALX_Nova_Controller_2/issues
- Documentation: https://schmackos.github.io/ALX_Nova_Controller_2/docs
- Internal docs: [docs-internal/](docs-internal/)

---

Built with [PlatformIO](https://platformio.org/), [Arduino](https://www.arduino.cc/), [ArduinoJson](https://arduinojson.org/), [WebSockets](https://github.com/Links2004/arduinoWebSockets), [PubSubClient](https://github.com/knolleary/pubsubclient), [LVGL](https://lvgl.io/), [LovyanGFX](https://github.com/lovyan03/LovyanGFX), [ESP-DSP](https://github.com/espressif/esp-dsp), [TinyUSB](https://github.com/hathach/tinyusb)
