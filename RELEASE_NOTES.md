# Release Notes

## Version 1.8.3

## Documentation
- [2026-02-15] docs: map existing codebase (`824ba3e`)

## New Features
- [2026-02-16] feat: Add settings format versioning with VER= prefix

Refactor settings persistence to support format evolution. Load/save
functions now detect and handle versioned settings files.

Changes:
- loadSettings() reads lines into array, detects "VER=2" prefix
- dataStart offset skips version line when present
- Legacy files (no version) still load correctly with dataStart=0
- saveSettings() writes "VER=2" as first line

Backward compatibility: old firmware reads "VER=2" as autoUpdateEnabled
(toInt() returns 0), disabling auto-update on downgrade.

Array-based approach eliminates 28 individual String variables and makes
adding new settings fields trivial. (`a30ea6d`)
- [2026-02-16] feat: Add settings format versioning with VER= prefix

Refactor settings persistence to support format evolution. Load/save
functions now detect and handle versioned settings files.

Changes:
- loadSettings() reads lines into array, detects "VER=2" prefix
- dataStart offset skips version line when present
- Legacy files (no version) still load correctly with dataStart=0
- saveSettings() writes "VER=2" as first line

Backward compatibility: old firmware reads "VER=2" as autoUpdateEnabled
(toInt() returns 0), disabling auto-update on downgrade.

Array-based approach eliminates 28 individual String variables and makes
adding new settings fields trivial. (`e7bd55e`)
- [2026-02-15] feat: Auth security hardening and USB audio init fix

SHA256 password hashing with NVS migration from plaintext, timing-safe
comparison, progressive login rate limiting, session ID log truncation,
SameSite=Strict cookies. USB audio TinyUSB init guarded to prevent
double-init crashes on enable/disable toggle. (`2832507`)

## Improvements
- [2026-02-16] perf: Pool JSON allocations in DSP API handlers

Replace per-handler JsonDocument with static _dspApiDoc to reduce heap
fragmentation. All DSP API handlers now reuse a single document pool.

Safe because:
- All handlers run single-threaded on Core 0 HTTP server
- No handler stores JSON references beyond its return
- serializeJson() completes before handler exits

Changed 26 instances of:
  JsonDocument doc;
To:
  _dspApiDoc.clear();
  JsonDocument &doc = _dspApiDoc;

Reduces ~2KB heap allocation per API call and prevents long-term
fragmentation from repeated JSON alloc/free cycles. (`a7a3372`)
- [2026-02-16] perf: Pool JSON allocations in DSP API handlers

Replace per-handler JsonDocument with static _dspApiDoc to reduce heap
fragmentation. All DSP API handlers now reuse a single document pool.

Safe because:
- All handlers run single-threaded on Core 0 HTTP server
- No handler stores JSON references beyond its return
- serializeJson() completes before handler exits

Changed 26 instances of:
  JsonDocument doc;
To:
  _dspApiDoc.clear();
  JsonDocument &doc = _dspApiDoc;

Reduces ~2KB heap allocation per API call and prevents long-term
fragmentation from repeated JSON alloc/free cycles. (`d0bd171`)
- [2026-02-15] perf: Optimize audio capture task CPU usage on Core 1

Merge 6+ separate buffer loops into 2 passes in process_adc_buffer():
- Pass 1: diagnostics + DC offset + DC-blocking IIR in one loop
- Pass 2: RMS + waveform + FFT ring fill in one loop

Replace double-precision math with float (ESP32-S3 has no double FPU):
- audio_compute_rms(): double sum_sq → float, sqrt → sqrtf
- DC offset tracking: double sum → float

Pre-compute VU/peak exponential coefficients (3 expf calls instead of 12)
and inline VU/peak updates in both silence and active paths.

Fix test VU_DECAY_MS mismatch: test had 650ms but production header has
300ms. Updated test_vu_decay_ramp assertions accordingly. (`de7cf89`)

## Bug Fixes
- [2026-02-16] fix: DSP frame-aligned config swap with busy-wait

Prevent mid-frame config swaps that could cause transient glitches.
Add _processingActive flag to track when dsp_process_buffer() is
executing. dsp_swap_config() now busy-waits (max 50ms) until
processing completes before copying delay lines.

Changes:
- Add volatile _processingActive flag
- Set true at start of dsp_process_buffer(), clear at end
- Clear in all early-return paths (globalBypass, channel bounds)
- dsp_swap_config() waits up to 50ms for !_processingActive

On native tests: vTaskDelay is no-op, _processingActive always false
(single-threaded), so no wait occurs.

Typical wait: 0-1 iterations (~5ms processing << 50ms timeout). (`a6cd50f`)
- [2026-02-16] fix: DSP frame-aligned config swap with busy-wait

Prevent mid-frame config swaps that could cause transient glitches.
Add _processingActive flag to track when dsp_process_buffer() is
executing. dsp_swap_config() now busy-waits (max 50ms) until
processing completes before copying delay lines.

Changes:
- Add volatile _processingActive flag
- Set true at start of dsp_process_buffer(), clear at end
- Clear in all early-return paths (globalBypass, channel bounds)
- dsp_swap_config() waits up to 50ms for !_processingActive

On native tests: vTaskDelay is no-op, _processingActive always false
(single-threaded), so no wait occurs.

Typical wait: 0-1 iterations (~5ms processing << 50ms timeout). (`09615b1`)
- [2026-02-16] fix: DSP frame-aligned config swap with busy-wait

Prevent mid-frame config swaps that could cause transient glitches.
Add _processingActive flag to track when dsp_process_buffer() is
executing. dsp_swap_config() now busy-waits (max 50ms) until
processing completes before copying delay lines.

Changes:
- Add volatile _processingActive flag
- Set true at start of dsp_process_buffer(), clear at end
- Clear in all early-return paths (globalBypass, channel bounds)
- dsp_swap_config() waits up to 50ms for !_processingActive

On native tests: vTaskDelay is no-op, _processingActive always false
(single-threaded), so no wait occurs.

Typical wait: 0-1 iterations (~5ms processing << 50ms timeout). (`a9fcb31`)
- [2026-02-16] fix: DSP frame-aligned config swap with busy-wait

Prevent mid-frame config swaps that could cause transient glitches.
Add _processingActive flag to track when dsp_process_buffer() is
executing. dsp_swap_config() now busy-waits (max 50ms) until
processing completes before copying delay lines.

Changes:
- Add volatile _processingActive flag
- Set true at start of dsp_process_buffer(), clear at end
- Clear in all early-return paths (globalBypass, channel bounds)
- dsp_swap_config() waits up to 50ms for !_processingActive

On native tests: vTaskDelay is no-op, _processingActive always false
(single-threaded), so no wait occurs.

Typical wait: 0-1 iterations (~5ms processing << 50ms timeout). (`f88d6e1`)
- [2026-02-16] fix: DSP frame-aligned config swap with busy-wait

Prevent mid-frame config swaps that could cause transient glitches.
Add _processingActive flag to track when dsp_process_buffer() is
executing. dsp_swap_config() now busy-waits (max 50ms) until
processing completes before copying delay lines.

Changes:
- Add volatile _processingActive flag
- Set true at start of dsp_process_buffer(), clear at end
- Clear in all early-return paths (globalBypass, channel bounds)
- dsp_swap_config() waits up to 50ms for !_processingActive

On native tests: vTaskDelay is no-op, _processingActive always false
(single-threaded), so no wait occurs.

Typical wait: 0-1 iterations (~5ms processing << 50ms timeout). (`9d17f93`)
- [2026-02-16] fix: USB audio Windows driver compatibility via minimal BOS descriptor

Override framework's BOS descriptor that includes Microsoft OS 2.0 with
Compatible ID "WINUSB", which causes Windows to load WinUSB driver instead
of usbaudio2.sys. Our minimal BOS includes only USB 2.0 Extension capability
(required for bcdUSB=0x0210) with no MSOS2/WINUSB descriptors.

Changes:
- Add linker wrapper flag -Wl,--wrap=tud_descriptor_bos_cb to override strong symbol
- Implement __wrap_tud_descriptor_bos_cb() with 12-byte minimal BOS descriptor
- Change clock control to read-only (bmControls: 0x07 → 0x05)
- STALL SET_CUR requests for clock frequency (fixed 48kHz, not configurable)
- Change USB PID 0x4001 → 0x4002 to force Windows to re-enumerate

Fixes Windows incorrectly loading WinUSB driver and failing to recognize UAC2
audio device. Device now correctly appears as "ALX Nova Audio" speaker. (`cb11992`)
- [2026-02-16] fix: USB audio Windows driver compatibility via minimal BOS descriptor

Override framework's BOS descriptor that includes Microsoft OS 2.0 with
Compatible ID "WINUSB", which causes Windows to load WinUSB driver instead
of usbaudio2.sys. Our minimal BOS includes only USB 2.0 Extension capability
(required for bcdUSB=0x0210) with no MSOS2/WINUSB descriptors.

Changes:
- Add linker wrapper flag -Wl,--wrap=tud_descriptor_bos_cb to override strong symbol
- Implement __wrap_tud_descriptor_bos_cb() with 12-byte minimal BOS descriptor
- Change clock control to read-only (bmControls: 0x07 → 0x05)
- STALL SET_CUR requests for clock frequency (fixed 48kHz, not configurable)
- Change USB PID 0x4001 → 0x4002 to force Windows to re-enumerate

Fixes Windows incorrectly loading WinUSB driver and failing to recognize UAC2
audio device. Device now correctly appears as "ALX Nova Audio" speaker. (`ccea3c8`)
- [2026-02-16] fix: USB audio Windows driver compatibility via minimal BOS descriptor

Override framework's BOS descriptor that includes Microsoft OS 2.0 with
Compatible ID "WINUSB", which causes Windows to load WinUSB driver instead
of usbaudio2.sys. Our minimal BOS includes only USB 2.0 Extension capability
(required for bcdUSB=0x0210) with no MSOS2/WINUSB descriptors.

Changes:
- Add linker wrapper flag -Wl,--wrap=tud_descriptor_bos_cb to override strong symbol
- Implement __wrap_tud_descriptor_bos_cb() with 12-byte minimal BOS descriptor
- Change clock control to read-only (bmControls: 0x07 → 0x05)
- STALL SET_CUR requests for clock frequency (fixed 48kHz, not configurable)
- Change USB PID 0x4001 → 0x4002 to force Windows to re-enumerate

Fixes Windows incorrectly loading WinUSB driver and failing to recognize UAC2
audio device. Device now correctly appears as "ALX Nova Audio" speaker. (`d0ecfb7`)
- [2026-02-16] fix: USB audio Windows driver compatibility via minimal BOS descriptor

Override framework's BOS descriptor that includes Microsoft OS 2.0 with
Compatible ID "WINUSB", which causes Windows to load WinUSB driver instead
of usbaudio2.sys. Our minimal BOS includes only USB 2.0 Extension capability
(required for bcdUSB=0x0210) with no MSOS2/WINUSB descriptors.

Changes:
- Add linker wrapper flag -Wl,--wrap=tud_descriptor_bos_cb to override strong symbol
- Implement __wrap_tud_descriptor_bos_cb() with 12-byte minimal BOS descriptor
- Change clock control to read-only (bmControls: 0x07 → 0x05)
- STALL SET_CUR requests for clock frequency (fixed 48kHz, not configurable)
- Change USB PID 0x4001 → 0x4002 to force Windows to re-enumerate

Fixes Windows incorrectly loading WinUSB driver and failing to recognize UAC2
audio device. Device now correctly appears as "ALX Nova Audio" speaker. (`77a7e2a`)
- [2026-02-15] fix: Security hardening, robustness fixes, and 64-bit auth timestamps

Address CONCERNS.md audit findings across security, robustness, and test gaps:

Security:
- Use timingSafeCompare() for session ID lookup in validateSession/removeSession
- Atomic crash log ring buffer validation (reset all on any inconsistency)
- Cap OTA consecutive failure counter at 20 to prevent unbounded growth

Robustness:
- Add recursive mutex to all I2C EEPROM operations (dac_eeprom.cpp)
- Move 3 large DSP API buffers (4-8KB) from stack to PSRAM heap
- Gate WS binary sends (waveform/spectrum) on !heapCritical
- Block dsp_add_stage() when heap is critical

Auth migration:
- Session timestamps: millis() (32-bit ms) → esp_timer_get_time() (64-bit μs)
- SESSION_TIMEOUT → SESSION_TIMEOUT_US (3600000000 μs)
- Rate-limit state updated to 64-bit microseconds

Tests: 790 pass (+36 new: auth timing-safe, OTA backoff cap, crash log corruption) (`f0d0aa2`)
- [2026-02-15] fix: USB audio control transfer handling and clock rate validation

Remove redundant SET_INTERFACE handler (TinyUSB handles it internally
via driver open callback). Add SET_CUR clock rate validation in DATA
stage. Improve control request logging. (`6d0e343`)
- [2026-02-16] fix: USB audio Windows driver compatibility via minimal BOS descriptor

Override framework's BOS descriptor that includes Microsoft OS 2.0 with
Compatible ID "WINUSB", which causes Windows to load WinUSB driver instead
of usbaudio2.sys. Our minimal BOS includes only USB 2.0 Extension capability
(required for bcdUSB=0x0210) with no MSOS2/WINUSB descriptors.

Changes:
- Add linker wrapper flag `-Wl,--wrap=tud_descriptor_bos_cb` to override strong symbol
- Implement __wrap_tud_descriptor_bos_cb() with 12-byte minimal BOS descriptor
- Change clock control to read-only (bmControls: 0x07 → 0x05)
- STALL SET_CUR requests for clock frequency (fixed 48kHz, not configurable)
- Change USB PID 0x4001 → 0x4002 to force Windows to re-enumerate

Fixes Windows incorrectly loading WinUSB driver and failing to recognize UAC2
audio device. Device now correctly appears as "ALX Nova Audio" speaker.
- [2026-02-15] fix: WebSocket session re-validation and default password consistency

Re-validate session on every WS command to catch logout/expiry.
Previously, WS connections persisted after HTTP logout because
wsAuthStatus was only set once during auth handshake.

Also fix isDefaultPassword() to compare against runtime AP password
instead of hardcoded DEFAULT_AP_PASSWORD constant. (`df56634`)


## Technical Details
- Version bump to 1.8.3

## Breaking Changes
None

## Known Issues
- None

## Version 1.8.2

## Documentation
- [2026-02-15] docs: Update release notes for v1.8.2 (`03a9078`)

## New Features
- [2026-02-15] feat: Release version 1.8.2 with new DSP features and improvements

- Extended DSP stage types: Noise Gate, Tone Controls, Loudness Compensation, Bass Enhancement, Stereo Width, Speaker Protection, Multiband Compressor
- Added Bessel crossover filters with multiple order options
- Implemented baffle step correction for high-shelf compensation
- Introduced real-time THD+N measurement with harmonic level display
- Improved client-side frequency response graph for PEQ curve accuracy
- Reduced Core 1 idle CPU usage significantly
- Fixed various bugs including frequency response graph issues and undefined function calls
- Updated technical details and ensured 754 native tests are passing
- Version bumped to 1.8.2 (`90ce72c`)

### New Features
- **Extended DSP stage types**: Noise Gate, Tone Controls, Loudness Compensation, Bass Enhancement, Stereo Width, Speaker Protection, Multiband Compressor — full Web UI, WebSocket, MQTT, and REST support
- **Bessel crossover filters**: 2nd/4th/6th/8th order Bessel options in crossover preset UI
- **Baffle step correction**: Calculate and apply high-shelf compensation from baffle width
- **THD+N measurement**: Real-time total harmonic distortion measurement with harmonic level display

### Improvements
- **Client-side frequency response graph**: PEQ curve now computed from parameters (freq/gain/Q/type) using RBJ Audio EQ Cookbook formulas in JavaScript, eliminating dependency on server-sent coefficients. Curves always match control point handles during drag, slider, and step button interactions
- **Core 1 idle CPU reduced ~60% to ~15-20%**: Silence fast-path skips DC filter/DSP/RMS/FFT on zero buffers; audio task yield increased 1→2 ticks; GUI polls at 100ms when screen asleep; dashboard refresh skipped when display off

### Bug Fixes
- **Frequency response graph disconnected from EQ bands**: Curves now always pass through control point handles by computing coefficients client-side instead of relying on server-sent coefficients
- **Frequency response graph missing crossover types**: Filter types 19 (LPF 1st), 20 (HPF 1st), 21 (Linkwitz) now render correctly via `dspIsBiquad()` helper
- **Undefined wsSend() function**: Fixed 5 call sites (ADC enable, USB audio, baffle step, THD start/stop) that used nonexistent `wsSend()` — replaced with `ws.send()` with readyState guard
- **BaffleStepResult build error**: Removed redundant extern declaration in websocket_handler.cpp
- **ADC Input label alignment**: Flexbox layout for ADC toggle switches in Audio Settings

### Technical Details
- 754 native tests passing
- RAM: 44.0%, Flash: 77.8%
- Version bump to 1.8.2

### Breaking Changes
None

### Known Issues
- None

## Version 1.8.1

### New Features
- DSP config presets: 4 named slots, save/load full config + routing matrix
- Gain ramp: exponential per-sample smoothing (5ms tau) prevents audible clicks
- Linkwitz Transform biquad filter type for subwoofer alignment
- USB digital audio input via TinyUSB UAC2 on native USB OTG
- Per-ADC enable/disable toggle with I2S driver reinit safety (audioPaused flag)
- Delay alignment measurement module
- Convolution engine with ESP-DSP lite fallbacks (conv, corr, fird)
- DSP_MAX_STAGES bumped 20→24 for PEQ + chain headroom
- MQTT/WS/REST/GUI integration for all new features
- 708 native tests passing

### Bug Fixes
- Align ADC Input labels with toggle switches in Audio Settings
- Fix preset save crash (stack overflow): heap-allocate 8KB/4KB buffers, debounced save

### Technical Details
- Version bump to 1.8.1

### Breaking Changes
None

### Known Issues
- None

## Version 1.8.0

## Bug Fixes
- [2026-02-14] fix: Debug console scroll, move debug toggle to General, persist DSP bypass

Debug console: Only auto-scroll when user is at bottom of log. Scrolling
up to read older entries no longer gets interrupted by new messages.

Settings UI: Debug Mode toggle moved from standalone card into General
section. PEQ graph dots now show band numbers inside circles.

WebSocket: DSP enable/bypass changes now persist via saveDspSettingsDebounced().
- [2026-02-14] fix: remove noisy No ACK debug logs from I2C bus scan

The summary line already reports device count and EEPROM mask — per-address
NACK logs for 0x51-0x57 added no diagnostic value and cluttered output.
- [2026-02-14] fix: OTA exponential backoff, ADC health debounce, EEPROM scan cleanup

OTA: Exponential backoff after consecutive failures (5min → 15min → 30min
→ 60min). Heap pre-flight check (60KB) inside OTA task before TLS
handshake prevents transient HEAP CRITICAL dips.

ADC health: 3-second debounce on health state transition logging to
eliminate 33K+ lines/session from ADC2 oscillating at CLIPPING boundary.

EEPROM: dac_eeprom_scan() now accepts I2C bus scan mask — only probes
addresses that ACK'd, eliminating 7 Wire error lines per scan from
absent 0x51-0x57 addresses.
- [2026-02-14] fix: temperatureRead() SAR ADC spinlock crash + OTA heap guard

Cache temperatureRead() every 10s to avoid SAR ADC spinlock deadlock
with I2S ADC that caused Core 1 interrupt WDT crash after ~2.5h.
Skip OTA checks when heap is already critical to prevent recurring
HEAP CRITICAL from TLS buffer allocation.

### DAC Output HAL
- Plugin driver architecture with DacDriver abstract class and compile-time registry
- PCM5102A driver: I2S-only, software volume via log-perceptual curve
- I2S full-duplex on I2S_NUM_0 (TX+RX), GPIO 40 data out, shared clocks with ADC1
- Settings persistence in `/dac_config.json` on LittleFS
- Web UI: "DAC Output" card on Audio tab (enable, volume, mute, driver select)
- WebSocket: `sendDacState()`, commands for enable/volume/mute/filter
- REST API: GET/POST `/api/dac`, GET `/api/dac/drivers`

### EEPROM Programming & Diagnostics
- AT24C02 EEPROM auto-detect on I2C bus (0x50-0x57) with ALXD format
- Serialize, page-aware write (ACK polling), erase, raw read, full I2C bus scan
- I2C bus recovery (SCL toggling for stuck SDA lines)
- Web UI: "EEPROM Programming" card (Audio tab) with driver presets, form fields, program/erase
- Web UI: "EEPROM / I2C" diagnostics card (Debug tab) with parsed fields, hex dump, re-scan
- Clear EEPROM state display: Programmed (green), Empty (orange), Not Found (red)
- TFT GUI: EEPROM section on Debug screen
- REST API: GET/POST `/api/dac/eeprom`, erase, scan, presets endpoints
- WebSocket: eepromScan, eepromProgram, eepromErase commands
- 28 native tests (17 parse + 11 serialize)

### DSP Enhancements
- 10-band parametric EQ (PEQ) per channel with dedicated stage slots
- Crossover bass management and 4x4 routing matrix presets
- DSP settings export/import via REST API

### Bug Fixes
- **Core 1 WDT crash**: `temperatureRead()` SAR ADC spinlock deadlock with I2S ADC caused interrupt watchdog timeout after ~2.5h of runtime. Fixed by caching temperature every 10s instead of on every HW stats broadcast.
- **OTA heap pressure**: OTA checks (TLS ~16KB allocation) now skip when heap is already critical, preventing recurring HEAP CRITICAL warnings every 5 minutes.

### Other Improvements
- GUI navigation and DSP screen enhancements

---

## Version 1.7.1

### ESP-DSP Optimizations
- Radix-4 FFT (dsps_fft4r_fc32) replaces Radix-2 for 20-27% speedup on S3
- 6 selectable FFT window types (Hann, Blackman, Blackman-Harris, Blackman-Nuttall, Nuttall, Flat-Top)
- SNR/SFDR per-ADC analysis via dsps_snr_f32/dsps_sfdr_f32
- Vector math SIMD (dsps_mulc_f32) replaces manual gain/polarity loops in DSP pipeline
- Routing matrix SIMD (dsps_mulc_f32 + dsps_add_f32) replaces per-sample inner loops
- Spectrum band edges start at 0 Hz (bin 0 included)
- Native test fallbacks in lib/esp_dsp_lite/ (23 new tests)
- MQTT HA discovery entities for FFT window select, per-ADC SNR/SFDR sensors

### Debug Toggle Decoupling
- Each debug sub-toggle now independently gates its own WebSocket data
- Hardware Stats toggle gates CPU/memory/storage/WiFi/ADC/crash data
- I2S Metrics toggle independently gates I2S config and runtime data
- Task Monitor already correctly gated
- Main loop broadcasts when any sub-toggle is active (not just HW Stats)
- Fixed "ADC 2 (Slave)" label to "ADC 2 (Master)" in web UI

## Version 1.7.0

### New Features
- **Audio DSP Pipeline**: Full 4-channel configurable DSP engine for active crossover and speaker management. Processes dual PCM1808 I2S ADC inputs into 4 mono channels with up to 20 processing stages each.
  - **Core engine** (Phase 1): Double-buffered DspState with glitch-free config swap, biquad IIR filters (LPF, HPF, BPF, Notch, PEQ, Low/High Shelf, Allpass, Custom), FIR convolution with pool-allocated taps/delay, peak limiter with envelope follower, gain stage, CPU load metrics
  - **Extended processing** (Phase 2): Delay lines (up to 100ms @48kHz), polarity inversion, mute, compressor with makeup gain. Crossover presets (LR2/LR4/LR8, Butterworth), bass management, 4x4 routing matrix
  - **Web UI** (Phase 3): DSP tab with stage list, add/remove/reorder/enable, client-side frequency response graph (biquad transfer function), crossover preset UI, routing matrix UI, import/export (REW APO text, JSON backup)
  - **ESP-DSP S3 assembly** (Phase 4): Pre-built `libespressif__esp-dsp.a` replaces ANSI C biquad/FIR processing (~30% biquad, 3-4x FIR speedup). ESP-DSP FFT replaces arduinoFFT for spectrum analysis. Renamed coefficient gen (`dsp_gen_*`) avoids symbol conflicts
  - **MQTT/HA + TFT** (Phase 5): HA discovery entities for DSP enable/bypass/CPU load + per-channel bypass/stages/limiter GR. TFT DSP menu screen + desktop carousel card
  - **Import/Export**: Equalizer APO text (REW), miniDSP biquad coefficients, FIR text, WAV impulse response (16/32-bit PCM, IEEE float), full JSON backup
  - **Integration**: 18 REST API endpoints, WebSocket real-time sync, LittleFS persistence with 5s debounced save

### Improvements
- **PSRAM delay line allocation**: DSP delay buffers now use `ps_calloc()` for the 8MB PSRAM when available, keeping internal SRAM free for WiFi/MQTT buffers
- **Heap safety hardening**: `dsp_add_stage()` rolls back on pool exhaustion instead of creating broken stages. Config imports skip stages on allocation failure. WebSocket sends `dspError` message to client on failure. Pre-flight heap check blocks delay allocation when free heap would drop below 40KB reserve

### Bug Fixes
- **Web server unreachable**: Static 76.8KB delay line pool consumed nearly all heap (9KB left), starving WiFi RX buffers. Converted to dynamic allocation — saves 76.8KB when no delay stages are in use
- **WDT crash (OTA check)**: TLS handshake during OTA firmware check took 5-10s of CPU on Core 0 without yielding, starving IDLE0 watchdog. Added `esp_task_wdt_delete()` to OTA check task
- **WDT crash (IDLE0 starvation)**: audio_cap (priority 3) + OTA_CHK on Core 0 prevented IDLE0 from feeding WDT. Unsubscribed IDLE0 from WDT — all important tasks have their own entries
- **Heap critical threshold**: Raised from 20KB to 40KB to match actual WiFi + HTTP + MQTT minimum requirements

### Technical Details
- 540 unit tests (57 DSP core + 22 REW parser + existing), all passing
- RAM: 51.6% (169008 / 327680 bytes)
- Flash: 68.9% (2302989 / 3342336 bytes)
- ESP-DSP pre-built library for S3 assembly-optimized processing (native tests use ANSI C fallbacks)

### Breaking Changes
None

### Known Issues
- None
