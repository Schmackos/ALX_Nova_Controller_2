# Architecture

**Analysis Date:** 2026-02-15

## Pattern Overview

**Overall:** FreeRTOS-based embedded system with a centralized state singleton, modular subsystem architecture, and real-time multi-core task scheduling.

**Key Characteristics:**
- Centralized state management via `AppState` singleton with dirty flags for efficient broadcasting
- Modular handler/driver pattern — each subsystem is self-contained with minimal coupling
- Dual-core FreeRTOS task distribution (Core 0: main loop, Core 1: GUI + audio capture)
- Real-time I2S audio pipeline with dual ADC master configuration and DSP processing
- RESTful HTTP API + WebSocket server for web UI control
- MQTT integration with Home Assistant discovery
- Conditional compilation guards (`GUI_ENABLED`, `DSP_ENABLED`, `DAC_ENABLED`, `USB_AUDIO_ENABLED`) for optional features

## Layers

**Core State Layer:**
- Purpose: Centralized application state management and dirty-flag tracking
- Location: `src/app_state.h`, `src/app_state.cpp`
- Contains: FSM states, WiFi/MQTT/OTA config, audio metrics, smart sensing state, DSP/DAC/USB audio state
- Depends on: `config.h` (compile-time constants)
- Used by: All modules — accessed via `AppState::getInstance()` or macro `appState`

**Hardware Abstraction Layer (HAL):**
- Purpose: Device-specific interfaces for I2S audio, DAC output, USB audio, GPIO
- Location: `src/i2s_audio.h/.cpp`, `src/dac_hal.h/.cpp`, `src/usb_audio.h/.cpp`
- Contains: I2S driver configuration (dual master ADC setup), sample rate control, audio analysis (RMS/VU/peak/FFT), DAC plugin registry, USB UAC2 descriptor
- Depends on: ESP-IDF APIs, TinyUSB, pre-built ESP-DSP library
- Used by: Smart sensing, DSP pipeline, signal generator, WebSocket/MQTT broadcasting

**Processing Layer (Real-Time Tasks):**
- Purpose: CPU-intensive operations on dedicated FreeRTOS tasks
- Location: `src/smart_sensing.cpp`, `src/i2s_audio.cpp`, `src/gui/gui_manager.cpp`, `src/usb_audio.cpp`
- Contains: Audio capture task (Core 1, priority 3, 12KB stack), GUI task (Core 1, VSync-driven), USB audio polling task (Core 0, priority 1)
- Depends on: I2S HAL, DSP pipeline, AppState dirty flags
- Used by: Main loop via dirty-flag polling pattern (no direct task communication)

**DSP Pipeline Layer:**
- Purpose: Audio signal processing chain with 31+ stage types, routing matrix, and preset management
- Location: `src/dsp_pipeline.h/.cpp`, `src/dsp_biquad_gen.h/.c`, `src/dsp_crossover.cpp`, `src/dsp_rew_parser.cpp`
- Contains: Stage type enum (IIR biquad, FIR, compressor, limiter, noise gate, tone control, crossover, THD measurement), double-buffered config with glitch-free swap, PSRAM offloading for delay lines
- Depends on: Pre-built ESP-DSP library (S3 SIMD-optimized), ESP-DSP Lite (ANSI C fallback for native tests)
- Used by: Audio capture task (processes each I2S buffer), REST/WebSocket API

**Connectivity Layer:**
- Purpose: Network services — WiFi, MQTT, OTA updates
- Location: `src/wifi_manager.h/.cpp`, `src/mqtt_handler.h/.cpp`, `src/ota_updater.h/.cpp`
- Contains: Async WiFi connection with retry/backoff, MQTT broker connection + HA discovery, GitHub release checking with SHA256 verification, non-blocking OTA tasks
- Depends on: Arduino WiFi, PubSubClient, HTTPClient, mbedtls
- Used by: Main loop (5s throttled checks), OTA tasks (Core 1 one-shot), WebSocket commands trigger WiFi/MQTT operations

**Web Server Layer:**
- Purpose: HTTP REST API + WebSocket server for real-time remote control
- Location: `src/websocket_handler.h/.cpp`, `src/dsp_api.cpp`, `src/dac_api.cpp`, `src/main.cpp` (route registration)
- Contains: HTTP endpoints (port 80) for settings/diagnostics/WiFi/OTA, WebSocket (port 81) for real-time state broadcasts and audio data (binary waveform/spectrum frames), request auth via session tokens
- Depends on: WebServer (Arduino), WebSocketsServer, ArduinoJson
- Used by: Web UI (browser client), MQTT commands trigger REST calls

**Persistence Layer:**
- Purpose: Non-volatile configuration storage
- Location: `src/settings_manager.h/.cpp`, `src/dac_eeprom.h/.cpp`, `src/crash_log.h/.cpp`
- Contains: Preferences API (NVS) for settings, LittleFS for text configs (`/settings.txt`, `/smartsensing.txt`, `/siggen.txt`, `/dsp_preset_*.json`), crash log ring buffer (`/crashlog.bin`), DAC EEPROM programming (I2C slave at 0x50)
- Depends on: Preferences (ESP32), LittleFS, I2C (Wire)
- Used by: Main loop on boot (settings load) and dirty-flag broadcasts (deferred save)

**GUI Layer:**
- Purpose: LVGL 9.4 UI on ST7735S 160×128 display with rotary encoder input
- Location: `src/gui/gui_manager.cpp`, `src/gui/gui_input.cpp`, `src/gui/screens/scr_*.cpp`, `src/gui/gui_navigation.cpp`
- Contains: FreeRTOS GUI task (Core 1, VSync 60Hz), rotary encoder Gray code state machine, screen navigation stack, 13 UI screens (desktop carousel, home/control/WiFi/MQTT/settings/debug/DSP/etc), boot animations, theme system (dark/light mode, orange accent)
- Depends on: LVGL, TFT_eSPI, AppState dirty-flag polling for external updates (WiFi/MQTT/audio)
- Used by: User interaction via encoder clicks/rotations, displays state from AppState + remote updates

**Utility Layer:**
- Purpose: Cross-cutting concerns and helpers
- Location: `src/debug_serial.h/.cpp`, `src/button_handler.cpp`, `src/buzzer_handler.cpp`, `src/task_monitor.cpp`, `src/signal_generator.cpp`, `src/auth_handler.cpp`
- Contains: Log-level filtered serial output (LOG_D/I/W/E), button debouncing + press type detection, piezo buzzer sequencer (FreeRTOS mutex for dual-core safety), FreeRTOS task enumeration (stack usage, priority, core affinity), test signal injection (sine/square/noise/sweep), session token authentication
- Depends on: FreeRTOS, Arduino core, Preferences (auth tokens)
- Used by: All modules — utilities are imported throughout

## Data Flow

**Audio Signal Path (I2S → DSP → DAC/USB → WebSocket/MQTT):**

1. **Hardware Capture**: `audio_capture_task` (Core 1, priority 3) calls `i2s_read()` → 256-sample stereo frames from dual PCM1808 ADCs at 48kHz
2. **Analysis**: Per-buffer RMS/dBFS/VU/peak computed in `i2s_audio.cpp`, health diagnostics (NO_DATA/CLIPPING/I2S_ERROR detection)
3. **Signal Generator Injection**: Test waveform mixed if `appState.sigGenEnabled` (sine/square/noise at configurable frequency/amplitude)
4. **DSP Processing** (if `DSP_ENABLED`): Frame passed through DSP pipeline stages (biquad/FIR/compressor/etc) with glitch-free config swap, routing matrix SIMD acceleration
5. **Output Routes**:
   - **DAC Output** (if `DAC_ENABLED`): I2S full-duplex TX to PCM5102A with software volume ramping
   - **USB Audio** (if `USB_AUDIO_ENABLED`): Ring buffer → TinyUSB UAC2 speaker device (16/24-bit format conversion)
   - **Smart Sensing**: Max level across ADCs → threshold comparison → timer/relay logic
6. **Telemetry**: Audio metrics (RMS/VU/peak/spectrum) queued in `audioLevelsDirty` flag → main loop calls `sendAudioData()` → WebSocket binary frames (waveform 258B + spectrum 70B) + JSON metadata
7. **MQTT Publishing**: Dirty-flag pattern — main loop broadcasts `audio/level`, `audio/input1/level`, `settings/audio_update_rate`, etc. every 5s (throttled)

**State Synchronization (AppState Dirty-Flag Pattern):**

1. **Task Sets Flag**: Audio task sets `audioLevelsDirty`, GUI task sets `_displayDirty`, OTA task sets `otaInProgressDirty`
2. **Main Loop Polls**: Checks all dirty flags every 10ms iteration
3. **Broadcast**: If dirty, calls appropriate `send*()` function (WebSocket + MQTT)
4. **Clear Flag**: Main loop clears flag after broadcast
5. **External Update**: WebSocket command (e.g., `setWifiConfig`) → REST handler modifies AppState → handler sets flag → broadcasts immediately

**Device Control Flow (Web/MQTT → REST → AppState → Hardware):**

1. **User Action**: Web UI slider or MQTT topic publish
2. **REST Handler** (e.g., `/api/smartsensing`): Parses JSON → updates AppState fields → calls handler function (e.g., `setAmplifierState()`)
3. **Handler Logic**: Modifies hardware (GPIO relay, I2S config) → updates AppState → sets dirty flags
4. **Broadcast**: Main loop detects flag → sends WebSocket/MQTT confirmation
5. **GUI Update**: GUI task polls AppState on next VSync → refreshes display

**State Management:**

- **Persistent State**: Settings, WiFi credentials, DSP presets → loaded at boot from LittleFS/Preferences, saved on dirty flag via `settings_manager` (debounced 5s save window)
- **Volatile State**: Audio levels, timer countdown, WiFi connection status → exists only in `AppState`, lost on reboot
- **Transient State**: OTA download progress, WebSocket auth tokens → session-scoped in memory (not persisted)

## Key Abstractions

**AppState Singleton:**
- Purpose: Single source of truth for all application state
- Examples: `src/app_state.h` (400+ lines defining all fields and dirty flags)
- Pattern: Meyer's singleton with `getInstance()` static method; macro `appState` aliases it for brevity; copy/move constructors deleted to prevent duplication

**Subsystem Handlers:**
- Purpose: Encapsulate domain logic for WiFi/MQTT/OTA/audio/etc
- Examples: `src/wifi_manager.cpp`, `src/mqtt_handler.cpp`, `src/smart_sensing.cpp`
- Pattern: Pure functions + static local state; init function called once at boot; periodic update functions called from main loop; dirty-flag pattern for async events

**Plugin Architecture (DAC Drivers):**
- Purpose: Support multiple DAC implementations without coupling to main code
- Examples: `src/dac_hal.h` (abstract `DacDriver` class), `src/drivers/dac_pcm5102.cpp` (PCM5102A implementation), registry in `dac_registry.cpp`
- Pattern: Virtual `init()`, `setVolume()`, `getStatus()` methods; factory function `dac_create_driver(type)` returns appropriate subclass

**Dirty-Flag Broadcasting:**
- Purpose: Decouple task updates from network broadcasts, minimize WebSocket/MQTT traffic
- Examples: `audioLevelsDirty`, `_wifiStateDirty`, `otaProgressDirty` in AppState
- Pattern: Flag set to true by producer (audio task, handler), main loop polls all flags at 10ms granularity, broadcasts only changed state, clears flags; throttled 5s for MQTT to avoid storms

**Conditional Feature Gates:**
- Purpose: Minimize firmware size by excluding optional features at compile time
- Examples: `#ifdef GUI_ENABLED`, `#ifdef DSP_ENABLED`, `#ifdef DAC_ENABLED`, `#ifdef USB_AUDIO_ENABLED`
- Pattern: Feature config in `platformio.ini` or `src/config.h`, all feature code guarded by preprocessor

## Entry Points

**Main Entry Point:**
- Location: `src/main.cpp`
- Triggers: ESP32 power-on or reboot
- Responsibilities:
  1. Initialize all subsystems (I2S audio, WiFi, MQTT, settings, crash log, GUI)
  2. Set up FreeRTOS tasks (audio capture on Core 1, GUI on Core 1, USB audio on Core 0)
  3. Register HTTP endpoints and WebSocket handler
  4. Main loop: 10ms tick polling dirty flags, button input, WiFi/MQTT throttled checks, state broadcasts

**Audio Capture Task (`audio_capture_task`):**
- Location: `src/i2s_audio.cpp`
- Triggers: FreeRTOS task spawn during init
- Responsibilities:
  1. Poll I2S driver for 256-sample PCM buffers every ~5.3ms (at 48kHz)
  2. Compute RMS/VU/peak and health diagnostics
  3. Apply signal generator injection if enabled
  4. Run DSP pipeline if enabled
  5. Route to DAC or USB audio if enabled
  6. Set `audioLevelsDirty` flag for main loop to broadcast
  7. Check `appState.audioPaused` flag (protects against concurrent I2S driver restart from Core 0)

**GUI Task (`gui_task`):**
- Location: `src/gui/gui_manager.cpp`
- Triggers: FreeRTOS task spawn during init (if `GUI_ENABLED`)
- Responsibilities:
  1. Run LVGL tick handler at 60 Hz (calls `lv_timer_handler()`)
  2. Process rotary encoder input (Gray code decoding)
  3. Poll AppState dirty flags for external updates (WiFi connection, MQTT status, audio levels)
  4. Update screen content based on state changes
  5. Execute screen transitions (stack push/pop)
  6. Sleep/wake backlight on idle timeout or external command

**OTA Download Task:**
- Location: `src/ota_updater.cpp` (`startOTADownloadTask()`)
- Triggers: User clicks "Update" in web UI or auto-update timer fires
- Responsibilities:
  1. Download firmware binary from GitHub release URL
  2. Compute SHA256 checksum and verify
  3. Flash via Arduino `Update` API (built-in bootloader)
  4. Set `otaInProgressDirty` flag for main loop to broadcast progress
  5. Clean up and reboot on completion

**WebSocket Event Handler:**
- Location: `src/websocket_handler.cpp` (`webSocketEvent()`)
- Triggers: Client connects/disconnects or sends message to WebSocket server (port 81)
- Responsibilities:
  1. Authenticate new clients (session token in first message)
  2. Parse incoming command JSON (e.g., `{"cmd": "setLedState", "state": true}`)
  3. Dispatch to appropriate handler (modifies AppState)
  4. Return response with updated state

**HTTP Request Handlers:**
- Location: `src/main.cpp`, `src/dsp_api.cpp`, `src/dac_api.cpp` (route registration via `server.on()`)
- Triggers: Browser or client sends HTTP GET/POST/PUT to `/api/*` endpoint
- Responsibilities:
  1. Parse request query params or JSON body
  2. Validate authentication (session token from Cookie or Authorization header)
  3. Call handler function to modify AppState or read diagnostics
  4. Serialize response as JSON
  5. Set `*Dirty` flags if state changed

## Error Handling

**Strategy:** Multi-layered resilience with graceful degradation, watchdog protection, and diagnostic logging.

**Patterns:**

- **I2S Driver Restart Recovery**: Consecutive I2S read timeouts (10 retries with exponential backoff) trigger driver uninstall/reinstall. `appState.audioPaused` flag prevents Audio task from calling `i2s_read()` during restart. If recovery fails after 3 attempts, health status set to `AUDIO_I2S_ERROR` and broadcast to user.

- **Heap Low Condition**: `appState.heapCritical` flag set when `ESP.getMaxAllocHeap() < 40KB`. DSP delay allocation pre-flight checks this flag and skips PSRAM allocation if critical. Main loop broadcasts warning via WebSocket/MQTT every 30s.

- **WiFi Connection Failure**: Exponential backoff (500ms → 2s → 5s) between reconnect attempts. After 10 failed attempts, fall back to AP mode if `autoAPEnabled` is true. User can manually trigger AP via web UI or button triple-click.

- **MQTT Broker Unreachable**: Client reconnects with exponential backoff (5s → 30s). If `autoUpdateEnabled` is true and MQTT is down, OTA still proceeds via direct GitHub HTTPS. Status published to `homeassistant/status` when broker comes back online (HA discovery refresh).

- **OTA Checksum Mismatch**: Download aborts, user notified via WebSocket, log entry written to crash ring buffer. Firmware remains unchanged.

- **DSP Config Import Error**: Failed stages are skipped (logged), successful stages applied. User can retry import or revert to previous preset via web UI.

- **Task Stack Overflow**: Watchdog (15s timeout) triggered if FreeRTOS task starves (e.g., tight loop blocking other tasks). Reboot logged to crash ring buffer. Stack sizes tuned per task in `config.h` (`TASK_STACK_SIZE_*`).

- **Serial Logging in Audio Hot Path**: Forbidden — `Serial.print()` blocks UART TX buffer, starving I2S DMA and causing audio dropouts. Audio task uses `_dumpReady` flag, main loop calls `audio_periodic_dump()` for deferred logging.

## Cross-Cutting Concerns

**Logging:** All modules use `debug_serial.h` macros (`LOG_D`/`LOG_I`/`LOG_W`/`LOG_E`) with `[ModuleName]` prefix (e.g., `[Audio]`, `[WiFi]`, `[OTA]`). Level controlled at runtime via `appState.debugSerialLevel` (0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG). WebSocket forwarding logs to browser console.

**Validation:** HTTP handlers validate content-type (JSON), parse with `ArduinoJson` (bounds checking), verify auth token, range-check numeric inputs (e.g., slider bounds 0-100). DSP API validates stage type, channel index, parameter ranges. MQTT payload size capped at 512 bytes to avoid broker drops.

**Authentication:** Web UI and API endpoints require session token (stored in Preferences, cleared on logout). WebSocket clients must send token in first message within 10 seconds or connection is dropped. Password stored as SHA256 hash (computed during boot from plaintext default or user-set password).

**Thread Safety:** Dual-core access to AppState protected by declaring fields `volatile` (compiler ensures no register caching across reads). Audio task on Core 1 and main loop on Core 0 don't hold locks (lock-free via dirty flags). Buzzer handler uses FreeRTOS mutex to protect ISR state updates.

**Performance Monitoring:** Task stack watermarks enumerated every 5s via `task_monitor` module, CPU usage calculated from loop tick timing. Metrics broadcast to web UI and MQTT every 30s (throttled). Audio latency measured in `i2s_audio` (avg `i2s_read()` time µs).

---

*Architecture analysis: 2026-02-15*
