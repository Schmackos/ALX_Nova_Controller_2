# Codebase Structure

**Analysis Date:** 2026-02-15

## Directory Layout

```
ALX_Nova_Controller/
├── src/                    # Firmware source code
│   ├── main.cpp            # Entry point, FreeRTOS task setup, main loop
│   ├── app_state.h/.cpp    # Centralized state singleton with dirty flags
│   ├── config.h            # Compile-time constants, pin definitions, stack sizes
│   ├── i2s_audio.h/.cpp    # Dual I2S ADC driver, RMS/VU/peak/FFT analysis
│   ├── smart_sensing.h/.cpp # Voltage detection, auto-off timer, relay control
│   ├── signal_generator.h/.cpp # Multi-waveform test signal injection
│   ├── dsp_pipeline.h/.cpp # 31+ stage DSP engine, double-buffered config
│   ├── dsp_biquad_gen.h/.c # RBJ biquad coefficient generation
│   ├── dsp_coefficients.h/.cpp # EQ coefficient computation utilities
│   ├── dsp_crossover.h/.cpp # Crossover presets, bass management
│   ├── dsp_rew_parser.h/.cpp # APO/miniDSP import, FIR/WAV loading
│   ├── dsp_convolution.h/.cpp # Partitioned convolution for room correction
│   ├── dac_hal.h/.cpp      # DAC plugin architecture, configuration
│   ├── dac_registry.h/.cpp  # DAC driver factory and registry
│   ├── dac_api.h/.cpp      # REST endpoints for DAC control
│   ├── dac_eeprom.h/.cpp   # I2C EEPROM programming for EEPROM-equipped DACs
│   ├── drivers/            # DAC driver implementations
│   │   └── dac_pcm5102.h/.cpp # PCM5102A driver (I2S only, software volume)
│   ├── usb_audio.h/.cpp    # TinyUSB UAC2 speaker device, format conversion
│   ├── wifi_manager.h/.cpp # WiFi async connection, AP mode, scan
│   ├── mqtt_handler.h/.cpp # MQTT broker, HA discovery, heartbeat publishing
│   ├── ota_updater.h/.cpp  # GitHub release checking, firmware download/verification
│   ├── settings_manager.h/.cpp # NVS/LittleFS persistence, export/import
│   ├── auth_handler.h/.cpp # Session token management, password auth
│   ├── websocket_handler.h/.cpp # WebSocket server (port 81), state broadcasting
│   ├── button_handler.h/.cpp # Button debouncing, press type detection
│   ├── buzzer_handler.h/.cpp # Piezo buzzer sequencer, pattern playback
│   ├── crash_log.h/.cpp    # Ring buffer crash logging to /crashlog.bin
│   ├── task_monitor.h/.cpp # FreeRTOS task enumeration, stack usage
│   ├── debug_serial.h/.cpp # Log-level filtered serial output
│   ├── web_pages.h/.cpp    # Embedded HTML/CSS/JS source (uncompressed)
│   ├── web_pages_gz.cpp    # Generated gzipped web assets (do NOT edit)
│   ├── gui/                # LVGL UI framework (guarded by GUI_ENABLED)
│   │   ├── gui_manager.h/.cpp # GUI task, screen initialization, sleep/wake
│   │   ├── gui_input.h/.cpp # Rotary encoder Gray code state machine
│   │   ├── gui_navigation.h/.cpp # Screen stack with push/pop transitions
│   │   ├── gui_theme.h/.cpp # Dark/light mode, color palette
│   │   ├── gui_config.h    # LVGL config, display dimensions
│   │   ├── gui_icons.h     # Icon data (encoded)
│   │   ├── lv_conf.h       # LVGL feature configuration
│   │   ├── User_Setup.h    # TFT_eSPI pin mappings (hardware)
│   │   ├── User_Setup_Wokwi.h # TFT_eSPI pin mappings (Wokwi sim)
│   │   └── screens/        # Individual screen implementations
│   │       ├── scr_boot_anim.h/.cpp # 6 boot animations
│   │       ├── scr_desktop.h/.cpp # Home carousel with gesture nav
│   │       ├── scr_home.h/.cpp # Dashboard (status, amp relay, timer)
│   │       ├── scr_control.h/.cpp # Amp control, audio settings
│   │       ├── scr_wifi.h/.cpp # WiFi connection, AP mode
│   │       ├── scr_mqtt.h/.cpp # MQTT broker, HA discovery status
│   │       ├── scr_settings.h/.cpp # Global settings, theme, language
│   │       ├── scr_debug.h/.cpp # Diagnostics, task monitor, crash log
│   │       ├── scr_dsp.h/.cpp # DSP stage editor, routing matrix
│   │       ├── scr_siggen.h/.cpp # Signal generator control panel
│   │       ├── scr_support.h/.cpp # About, firmware version
│   │       ├── scr_menu.h/.cpp # Navigation menu/list
│   │       ├── scr_keyboard.h/.cpp # Virtual keyboard for WiFi entry
│   │       └── scr_value_edit.h/.cpp # Generic numeric/string input
│   ├── thd_measurement.h/.cpp # THD+N analysis for DSP verification
│   ├── delay_alignment.h/.cpp # Cross-channel delay measurement/alignment
│   ├── safe_snr_sfdr.c # SNR/SFDR calculation (glue code for ESP-DSP)
│   ├── strings.h       # Localized UI strings (EN/FR/ES/DE)
│   ├── design_tokens.h # UI spacing, sizing constants
│   ├── utils.h/.cpp    # Miscellaneous helpers (checksums, version compare)
│   └── login_page.h    # Embedded login form HTML
│
├── test/               # Unit tests (PlatformIO native environment)
│   ├── test_mocks/     # Mock implementations for Arduino/WiFi/MQTT/Preferences
│   │   ├── Arduino.h    # GPIO/UART/millis/analogRead stubs
│   │   ├── WiFi.h      # WiFi client/server mocks
│   │   ├── PubSubClient.h # MQTT client mock
│   │   ├── Preferences.h # NVS storage mock
│   │   └── mbedtls/    # mbed TLS stubs (SHA256, etc)
│   ├── test_utils/     # String parsing, version comparison
│   ├── test_auth/      # Session token, password hash validation
│   ├── test_wifi/      # Connection logic, retry backoff
│   ├── test_mqtt/      # Publish/subscribe, HA discovery
│   ├── test_settings/  # Load/save, export/import, factory reset
│   ├── test_ota/       # Version comparison, checksum verification
│   ├── test_ota_task/  # Non-blocking OTA tasks, dirty flags
│   ├── test_button/    # Press type detection, debouncing
│   ├── test_buzzer/    # Pattern sequencing, ISR safety
│   ├── test_websocket/ # Auth, command parsing, broadcasts
│   ├── test_api/       # HTTP handler logic, JSON serialization
│   ├── test_i2s_audio/ # RMS/dBFS, VU attack/decay, peak hold
│   ├── test_fft/       # 1024-pt FFT, spectrum bands, window functions
│   ├── test_audio_diagnostics/ # NO_DATA, CLIPPING, I2S_ERROR detection
│   ├── test_smart_sensing/ # Voltage threshold, timer countdown, relay logic
│   ├── test_signal_generator/ # Waveform generation, frequency sweep
│   ├── test_dsp/       # IIR/FIR/gain/delay stages, glitch-free morphing
│   ├── test_dsp_rew/   # APO/miniDSP parsing, FIR import
│   ├── test_esp_dsp/   # ESP-DSP SIMD operations, Radix-4 FFT
│   ├── test_crash_log/ # Ring buffer, entry encoding
│   ├── test_task_monitor/ # Stack enumeration, timing measurements
│   ├── test_debug_mode/ # Level control, feature gating
│   ├── test_dim_timeout/ # Screen sleep timing
│   ├── test_vrms/      # ADC Vrms calculation
│   ├── test_peq/       # EQ band manipulation, coefficient calc
│   ├── test_pinout/    # GPIO assignments
│   ├── test_gui_home/  # Home screen state/transitions
│   ├── test_gui_input/ # Encoder input handling, state machine
│   ├── test_gui_navigation/ # Screen stack operations
│   ├── test_dac_hal/   # DAC plugin, registry, volume control
│   ├── test_dac_eeprom/ # EEPROM write/erase, page handling
│   └── test_usb_audio/ # UAC2 descriptor, ring buffer, format conversion
│
├── lib/                # Custom libraries
│   └── esp_dsp_lite/   # ANSI C fallback for ESP-DSP (native tests only)
│       ├── src/        # dsps_biquad, dsps_fft, etc (no SIMD)
│       └── include/    # Function declarations
│
├── include/            # Reserved for future use (empty)
├── docs/               # Technical documentation
├── .planning/codebase/ # GSD analysis documents (ARCHITECTURE.md, STRUCTURE.md, etc)
├── logs/               # Build/test logs
├── plans/              # Implementation phase plans
│
├── platformio.ini      # PlatformIO configuration, environments, build flags
├── CLAUDE.md           # Detailed architecture guide for Claude Code
├── README.md           # Quick start, build commands
├── wokwi.toml          # Wokwi simulator configuration
├── diagram.json        # Wokwi circuit diagram
├── build_web_assets.js # Script to regenerate web_pages_gz.cpp
└── [docs/release notes] # RELEASE_NOTES.md, various .md guides
```

## Directory Purposes

**src/:**
- Purpose: All firmware source code compiled for ESP32-S3 (Freenove FNK0085)
- Contains: Main executable, all subsystem handlers, GUI, DSP pipeline, network services
- Key files: `main.cpp` (entry), `app_state.h` (state), `config.h` (constants)

**src/gui/:**
- Purpose: LVGL-based UI system (guarded by `GUI_ENABLED`)
- Contains: GUI manager task, rotary encoder input, theme system, 13 screens
- Key files: `gui_manager.cpp` (task init), `gui_navigation.cpp` (screen stack)

**src/gui/screens/:**
- Purpose: Individual screen implementations (each is a separate UI view)
- Contains: Home, Control, WiFi, MQTT, Settings, Debug, DSP, Signal Gen, Boot animations, etc.
- Key files: `scr_desktop.cpp` (carousel nav), `scr_home.cpp` (main dashboard)

**src/drivers/:**
- Purpose: Hardware driver implementations (currently DAC drivers)
- Contains: Concrete driver subclasses that inherit from abstract `DacDriver`
- Key files: `dac_pcm5102.cpp` (PCM5102A I2S + software volume)

**test/:**
- Purpose: Unit tests for native platform (gcc/MinGW, no hardware)
- Contains: One directory per test module, each with its own `main()` and mocks
- Key files: `test_mocks/` (stub implementations), individual test directories

**test/test_mocks/:**
- Purpose: Mock implementations of Arduino framework, WiFi, MQTT, NVS
- Contains: Headers that replace real libraries during native compilation
- Key files: `Arduino.h`, `WiFi.h`, `PubSubClient.h`, `Preferences.h`

**lib/:**
- Purpose: Custom/fallback libraries (not from Arduino/PlatformIO registry)
- Contains: ESP-DSP Lite (ANSI C fallback, used only in native tests; `lib_ignore`d on ESP32)
- Key files: `esp_dsp_lite/src/` (dsps_biquad.c, dsps_fft.c)

**docs/:**
- Purpose: Technical documentation and implementation guides
- Contains: Feature-specific docs, architecture notes, troubleshooting

**.planning/codebase/:**
- Purpose: GSD analysis documents generated by Claude Code
- Contains: ARCHITECTURE.md, STRUCTURE.md, CONVENTIONS.md, TESTING.md, CONCERNS.md, STACK.md, INTEGRATIONS.md
- Key files: Created by `/gsd:map-codebase` command

## Key File Locations

**Entry Points:**
- `src/main.cpp`: Main entry, hardware init, main loop (Core 0)
- `src/main.cpp` `setup()`: Called once at boot, registers all HTTP routes
- `src/main.cpp` `loop()`: 10ms tick, polls dirty flags, broadcasts state
- `src/i2s_audio.cpp` `audio_capture_task()`: Audio task entry (Core 1, priority 3)
- `src/gui/gui_manager.cpp` `gui_task()`: GUI task entry (Core 1, VSync)
- `src/usb_audio.cpp` `usb_audio_task()`: USB polling task (Core 0, priority 1)

**Configuration:**
- `src/config.h`: Pin definitions (LED, encoder, TFT, I2S, DAC, USB), stack sizes, FreeRTOS priorities, DSP pool sizes, feature guards (`DSP_ENABLED`, `GUI_ENABLED`, `DAC_ENABLED`, `USB_AUDIO_ENABLED`)
- `platformio.ini`: Build environments, compiler flags, library dependencies, upload configuration
- `src/gui/gui_config.h`: LVGL feature config, display width/height
- `src/gui/User_Setup.h`: TFT_eSPI pin mappings (hardware)
- `src/gui/User_Setup_Wokwi.h`: TFT_eSPI pin mappings (simulator)

**Core Logic:**
- `src/app_state.h`: AppState class definition, all state fields, dirty flags
- `src/app_state.cpp`: AppState setters with flag management
- `src/smart_sensing.cpp`: Threshold logic, timer countdown, relay state
- `src/i2s_audio.cpp`: I2S driver init (dual master setup), RMS/VU/peak calculation, FFT
- `src/dsp_pipeline.cpp`: Stage processing loop, glitch-free config swap, routing matrix
- `src/websocket_handler.cpp`: Command parsing, state broadcasting, binary frame encoding

**Testing:**
- `test/test_mocks/`: All mock headers (read these to understand API contracts during tests)
- `test/test_smart_sensing/`: Threshold + timer logic tests
- `test/test_i2s_audio/`: RMS/VU/peak calculation tests (24 tests)
- `test/test_fft/`: Spectrum analysis tests (10 tests)
- `test/test_dsp/`: DSP stage processing tests (144 tests)

## Naming Conventions

**Files:**
- `.h` suffix: Header file (declarations)
- `.cpp` suffix: C++ implementation
- `.c` suffix: C implementation (e.g., `dsp_biquad_gen.c`, pure algorithm)
- `scr_*.cpp`: Screen implementation (e.g., `scr_home.cpp`)
- `test_*/`: Test module directories
- Files with same name as module: Header + implementation pair (e.g., `wifi_manager.h`, `wifi_manager.cpp`)

**Functions:**
- Underscore lowercase: `is_valid_ssid()`, `i2s_audio_init()`, `dsp_add_stage()`
- Public API functions: `void init_subsystem()` called once at boot
- Periodic handlers: `void handle_subsystem()` or `void update_subsystem()` called each main loop tick
- REST handlers: `void handleSubsystemGet()`, `void handleSubsystemPost()` (camelCase, naming legacy)
- Task functions: `void task_name(void *param)` (Core 1 = `gui_task`, `audio_capture_task`)

**Variables:**
- Class members: camelCase (e.g., `appState.wifiSSID`, `appState.audioLevel_dBFS`)
- Local variables: camelCase (e.g., `int sampleRate`, `float dBFS`)
- Constants/macros: UPPER_SNAKE_CASE (e.g., `FIRMWARE_VERSION`, `DEFAULT_TIMER_DURATION`, `MAX_DELAY_SAMPLES`)
- AppState dirty flags: `is*Dirty()` getter, `clear*Dirty()` setter (e.g., `isLedStateDirty()`, `clearLedStateDirty()`)
- Enums: PascalCase for enum name, UPPER_SNAKE_CASE for values (e.g., `enum SensingMode { ALWAYS_ON, MANUAL, AUTO_OFF_TIMER }`)

**Directories:**
- All lowercase, short names: `src/`, `test/`, `lib/`, `docs/`, `include/`
- Descriptive multi-word: `scr_` prefix for screens (e.g., `scr_desktop`, `scr_wifi`)
- Feature-based test dirs: `test_` prefix + module name (e.g., `test_i2s_audio`, `test_mqtt`)

## Where to Add New Code

**New Feature (e.g., Bluetooth Connectivity):**
- Primary code: Create new file `src/bluetooth_handler.h/.cpp` following the subsystem handler pattern
  - Public API: Init function `bool bluetooth_init()`, periodic update `void bluetooth_update()`, event handlers
  - State: Add fields to `AppState` for connection status, discovered devices, etc
  - Dirty flags: Add getter/setter pair in `AppState` for broadcasting
  - Error handling: Define state enum (e.g., `BluetoothState { IDLE, SCANNING, CONNECTED, ERROR }`)
- REST API: Create new file `src/bluetooth_api.cpp`, register routes in `main.cpp` via `server.on()`
- WebSocket: Add message type in `websocket_handler.cpp` case statement
- MQTT: Add topics in `mqtt_handler.cpp` (if needed)
- GUI: Create `src/gui/screens/scr_bluetooth.cpp` if user interaction required
- Tests: Create `test/bluetooth/` directory with mock header `test/test_mocks/Bluetooth.h`

**New Component/Module (e.g., Delay Alignment Measurement):**
- Implementation: Create `src/delay_alignment.h/.cpp` with pure functions `measure_delay()`, `apply_delay()`
- State persistence: Store result in `AppState` field (e.g., `delayMeasurement`)
- API endpoint: Register GET/POST in main.cpp for user-initiated measurements
- GUI: Update relevant screen to display result (e.g., DSP control screen)
- Tests: Create `test/test_delay_alignment/` with fixture data (reference measurements)

**Utilities/Helpers:**
- Shared helpers: `src/utils.h/.cpp` (string parsing, checksum, version compare)
- Module-specific: Keep utilities inside the module file if only used there
- Cross-cutting: `src/debug_serial.h` for logging, `src/task_monitor.h` for FreeRTOS enumeration

**New Screen/GUI Widget (if GUI_ENABLED):**
- Screen impl: `src/gui/screens/scr_feature_name.h/.cpp` with `lv_obj_t* create_scr_feature_name()`
- Screen init: Allocate LVGL objects, register callbacks, return screen object
- Navigation: Register in `gui_navigation.h` as entry in screen enum, add push logic in menu
- State updates: Poll `AppState` dirty flags in periodic handler to refresh display
- Input: If needs rotary encoder or button, handle in screen's key callback

**DSP Stage Type (if DSP_ENABLED):**
- Enum: Add new `DspStageType` value in `src/dsp_pipeline.h` (increment `DSP_STAGE_TYPE_COUNT`)
- Struct: Define corresponding `DspStageParams` union member in `src/dsp_pipeline.h`
- Processing: Implement `case DSP_NEW_STAGE:` in `dsp_process_channel()` in `src/dsp_pipeline.cpp`
- Initialization: Add init case in `dsp_add_stage()` constructor logic
- API: Register POST `/api/dsp/stage` handler to accept new stage type parameters
- Tests: Create tests in `test/test_dsp/` for stage processing with various parameters

**New Test Module:**
- Create directory: `test/test_new_module/`
- Create `src/test_new_module.cpp` with `void setUp()`, `void tearDown()`, test functions
- Create mock headers in `test/test_mocks/` if mocking external library
- Use Unity assertions: `TEST_ASSERT_EQUAL()`, `TEST_ASSERT_TRUE()`, etc.
- Run via `pio test -e native -f test_new_module`

## Special Directories

**build/:**
- Purpose: Intermediate build artifacts (generated by PlatformIO)
- Generated: Yes (created at compile time)
- Committed: No (in `.gitignore`)

**.pio/:**
- Purpose: PlatformIO cache, tool downloads, environment configs
- Generated: Yes (created by `pio` command)
- Committed: No (in `.gitignore`)

**.github/workflows/:**
- Purpose: CI/CD automation (GitHub Actions)
- Contents: `tests.yml` (run all native tests), `release.yml` (automated releases)
- Committed: Yes

**logs/:**
- Purpose: Build and test logs for debugging
- Generated: Yes (updated during build/test runs)
- Committed: No (in `.gitignore`)

**plans/:**
- Purpose: Implementation phase plan documents
- Generated: Yes (created by `/gsd:plan-phase` command)
- Committed: No (in `.gitignore`)

---

*Structure analysis: 2026-02-15*
