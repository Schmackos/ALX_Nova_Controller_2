# Audio DSP Pipeline — Implementation Plan

## Scope Summary

Based on the interview and research, the target DSP system is:

| Decision | Choice |
|---|---|
| **Use case** | Active crossover / speaker management + FIR |
| **Channels** | 4 independent (both stereo pairs from dual PCM1808) |
| **Filter chain** | 10-band PEQ + HPF + LPF + Limiter per channel |
| **Audio output** | I2S DAC output (hardware addition required) |
| **FIR filters** | Phase 2 (start with IIR/PEQ first) |
| **REW integration** | File import + export, architect for live bridge later |
| **Control interfaces** | WebApp/API first, then MQTT + TFT screen later |

---

## Architecture Overview

```
                         ┌─────────────────────────────────┐
                         │        ESP32-S3 (240 MHz)       │
                         │                                 │
  PCM1808 ADC1 ──I2S0──>│  ┌───────┐    ┌─────────────┐  │──I2S_TX──> DAC (PCM5102A)
    (L1/R1)              │  │ I2S   │    │ DSP Pipeline │  │           to amplifiers
                         │  │ Read  │───>│  per-channel │──│
  PCM1808 ADC2 ──I2S1──>│  │ Task  │    │             │  │
    (L2/R2)              │  └───────┘    │  HPF        │  │
                         │               │  PEQ x10    │  │
                         │               │  LPF        │  │
                         │               │  Limiter    │  │
                         │               │  (FIR Ph.2) │  │
                         │               └──────┬──────┘  │
                         │                      │         │
                         │               ┌──────▼──────┐  │
                         │               │  Analysis   │  │
                         │               │  RMS/VU/FFT │  │
                         │               └─────────────┘  │
                         │                                 │
                         │  WebSocket ◄── API ◄── Web UI  │
                         └─────────────────────────────────┘
```

### DSP Insertion Point

The DSP pipeline inserts into `process_adc_buffer()` in `src/i2s_audio.cpp` at **line ~414**, after the DC-blocking IIR filter and before RMS/VU/FFT analysis. This means:

1. All analysis (RMS, VU, waveform, FFT) reflects the **post-DSP** signal
2. The existing DC-blocking filter becomes the first stage of the DSP chain
3. Processing is per-ADC (called twice per buffer cycle, once for each stereo pair)
4. The buffer is `int32_t[]` with 24-bit audio — DSP will convert to `float`, process, convert back

### CPU Budget (ESP32-S3 @ 240 MHz, 48 kHz, 256 frames/buffer)

| Operation | Cycles/sample | Per buffer (256 frames) | % of 5.33ms budget |
|---|---|---|---|
| DC-blocking IIR (existing) | ~10 | 2,560 | 0.2% |
| HPF (2nd-order biquad) | ~17 | 4,352 | 0.3% |
| 10-band PEQ (10 biquads) | ~170 | 43,520 | 3.4% |
| LPF (2nd-order biquad) | ~17 | 4,352 | 0.3% |
| Limiter (envelope + gain) | ~50 | 12,800 | 1.0% |
| **Per-channel total** | **~264** | **67,584** | **5.3%** |
| **4 channels total** | | **270,336** | **21.1%** |
| Existing analysis (RMS/VU/FFT) | | ~200,000 | **15.7%** |
| **Grand total** | | **~470,336** | **36.8%** |

**Verdict**: ~63% headroom remaining on Core 0. FIR (Phase 2) with 128 taps would add ~14% for 4 channels, still leaving ~49% free.

---

## Library Selection

### Primary: ESP-DSP (Espressif Official)

**Why**: Assembly-optimized biquad (`dsps_biquad_f32_aes3`) achieves ~17 cycles/sample on ESP32-S3. Apache 2.0 license. Built-in coefficient generators for all needed filter types.

**Integration approach**: Include the ANSI C source files directly in the project's `lib/` directory. This guarantees PlatformIO/Arduino compatibility. The ANSI C biquad is ~24 cycles/sample (vs 17 for assembly) — still well within budget. If assembly optimization is needed later, build flags can enable it.

**Key functions used**:
- `dsps_biquad_f32()` — process samples through a biquad section
- `dsps_biquad_gen_lpf_f32()` — generate LPF coefficients
- `dsps_biquad_gen_hpf_f32()` — generate HPF coefficients
- `dsps_biquad_gen_peakingEQ_f32()` — generate PEQ coefficients
- `dsps_biquad_gen_highShelf_f32()` / `dsps_biquad_gen_lowShelf_f32()` — shelf filters
- `dsps_fir_f32()` — FIR processing (Phase 2)

### Secondary: Custom Limiter

ESP-DSP doesn't include dynamics processors. The limiter will be custom-implemented:
- Envelope follower (peak detection with attack/release)
- Gain computer (threshold, ratio, knee)
- Gain smoothing (ballistics)
- Gain application

This is straightforward (~50-80 lines of DSP code) and well-documented in audio engineering literature.

### Coefficient Math: Robert Bristow-Johnson Audio EQ Cookbook

The standard reference for biquad coefficient formulas. ESP-DSP's `dsps_biquad_gen_*` functions implement these internally. For REW import/export compatibility, we'll also implement direct coefficient loading (bypassing the generators).

---

## REW Integration Design

### Import Formats (Phase 1)

| Format | Extension | Parser Complexity | Use Case |
|---|---|---|---|
| **Equalizer APO** | `.txt` | Simple text parse | REW "Export filter settings as text" |
| **miniDSP biquad** | `.txt` | Simple text parse | Direct coefficient loading |

**Equalizer APO format example**:
```
Filter 1: ON PK Fc 63.00 Hz Gain -3.2 dB Q 4.32
Filter 2: ON PK Fc 125.00 Hz Gain 1.5 dB Q 2.10
Filter 3: ON HPQ Fc 30.00 Hz Q 0.707
Filter 4: ON LPQ Fc 18000.00 Hz Q 0.707
```

**miniDSP biquad format example**:
```
biquad1, b0=1.0012345, b1=-1.9876543, b2=0.9864321, a1=-1.9876543, a2=0.9876666
```
Note: a1/a2 signs are negated in miniDSP format vs. standard RBJ.

### Export Formats (Phase 1)

- Export current DSP config as Equalizer APO text
- Export as miniDSP biquad coefficients
- Export as JSON (native format for backup/restore)

### REW Bridge Architecture (Future — architected but not implemented)

```
  REW (PC)                    Bridge App (PC)              ESP32
  ─────────                   ────────────────             ─────
  REST API ◄──────────────► Python/Node listener ──MQTT──► DSP engine
  localhost:4735              (reads REW filters,           (applies in
                               converts to JSON,            real-time)
                               publishes via MQTT)
```

The bridge is a simple PC-side script (~100 lines) that:
1. Polls REW's REST API (`localhost:4735`) for filter changes
2. Converts to the JSON format the ESP32 already accepts
3. Publishes via MQTT (or WebSocket) to the ESP32

This is deferred but the ESP32 API will be designed to accept filter updates from any source (Web UI, MQTT, bridge), making it trivial to add later.

---

## Data Structures

### DSP Channel Configuration

```cpp
// Maximum filter stages per channel
#define DSP_MAX_PEQ_BANDS    10
#define DSP_MAX_FIR_TAPS     256   // Phase 2

// Biquad filter types (maps to ESP-DSP coefficient generators)
enum class BiquadType : uint8_t {
    LOWPASS,
    HIGHPASS,
    BANDPASS,
    NOTCH,
    PEAKING_EQ,
    LOW_SHELF,
    HIGH_SHELF,
    ALLPASS,
    CUSTOM          // Direct coefficient loading (REW import)
};

// Single biquad filter stage
struct DspBiquadStage {
    bool enabled = false;
    BiquadType type = BiquadType::PEAKING_EQ;
    float frequency = 1000.0f;    // Hz
    float gain = 0.0f;            // dB (for PEQ/shelf)
    float Q = 0.707f;             // Quality factor
    float coeffs[5] = {0};        // [b0, b1, b2, a1, a2] (computed)
    float delay[2] = {0};         // Biquad state (delay line)
};

// Limiter configuration
struct DspLimiter {
    bool enabled = false;
    float thresholdDb = 0.0f;     // dB threshold
    float attackMs = 5.0f;        // Attack time in ms
    float releaseMs = 50.0f;      // Release time in ms
    float ratio = 20.0f;          // Compression ratio (20:1 = limiting)
    // Runtime state
    float envelope = 0.0f;        // Current envelope value
    float gainReduction = 0.0f;   // Current gain reduction in dB
};

// FIR filter (Phase 2)
struct DspFirFilter {
    bool enabled = false;
    uint16_t numTaps = 0;
    float* coefficients = nullptr; // Heap-allocated tap array
    float* delayLine = nullptr;    // Heap-allocated delay line
};

// Complete DSP chain for one channel
struct DspChannelConfig {
    bool bypass = false;           // Bypass entire DSP chain
    float inputGain = 0.0f;       // Pre-DSP gain in dB
    float outputGain = 0.0f;      // Post-DSP gain in dB

    DspBiquadStage hpf;            // High-pass filter
    DspBiquadStage peq[DSP_MAX_PEQ_BANDS]; // Parametric EQ bands
    DspBiquadStage lpf;            // Low-pass filter
    DspLimiter limiter;            // Output limiter

    DspFirFilter fir;              // FIR filter (Phase 2)
};

// Global DSP state
struct DspState {
    bool globalBypass = false;
    uint32_t sampleRate = 48000;
    DspChannelConfig channels[4];  // L1, R1, L2, R2

    // Runtime metrics
    uint32_t processTimeUs = 0;    // Last processing time
    uint32_t maxProcessTimeUs = 0; // Peak processing time
    float cpuLoadPercent = 0.0f;   // DSP CPU usage estimate
};
```

### Memory Footprint

| Component | Per Channel | 4 Channels |
|---|---|---|
| HPF (1 biquad) | 44 bytes | 176 bytes |
| PEQ (10 biquads) | 440 bytes | 1,760 bytes |
| LPF (1 biquad) | 44 bytes | 176 bytes |
| Limiter | 28 bytes | 112 bytes |
| Channel config overhead | 12 bytes | 48 bytes |
| **IIR total** | **568 bytes** | **2,272 bytes** |
| FIR (128 taps, Phase 2) | 1,028 bytes | 4,112 bytes |
| **Grand total with FIR** | **1,596 bytes** | **6,384 bytes** |

Plus working buffers for float conversion: `float dspBuffer[512]` = 2,048 bytes (static, shared).

**Total DSP memory**: ~8.5 KB — negligible compared to the ~320 KB internal SRAM.

---

## Phase 1: Core DSP Engine (Foundation)

### Goal
Build the DSP processing pipeline, biquad filter engine, and basic API. No UI yet — controlled via REST API and WebSocket JSON commands.

### Files to Create

| File | Purpose |
|---|---|
| `src/dsp_pipeline.h` | DSP data structures, public API declarations |
| `src/dsp_pipeline.cpp` | DSP processing engine, biquad cascading, limiter |
| `src/dsp_coefficients.h` | Biquad coefficient computation (RBJ cookbook formulas) |
| `src/dsp_coefficients.cpp` | Implementation of coefficient generators |
| `lib/esp_dsp_lite/` | Extracted ESP-DSP biquad + FIR ANSI C sources |

### Files to Modify

| File | Changes |
|---|---|
| `src/i2s_audio.cpp` | Insert `dsp_process_buffer()` call after DC-blocking filter |
| `src/i2s_audio.h` | Add DSP enable/bypass flag access |
| `src/app_state.h` | Add DSP-related state members and dirty flags |
| `src/config.h` | Add DSP defaults (max bands, default Q, etc.) |
| `src/settings_manager.cpp` | Persist DSP config to NVS |
| `src/main.cpp` | Register DSP API endpoints |
| `platformio.ini` | Add ESP-DSP source include paths |

### Implementation Steps

#### 1.1 — Integrate ESP-DSP Library
- Extract `dsps_biquad.c`, `dsps_biquad_gen.c`, and `dsps_fir.c` (ANSI C versions) from the ESP-DSP repository
- Place in `lib/esp_dsp_lite/` with appropriate headers
- Add to `platformio.ini` lib_deps or lib_extra_dirs
- Verify compilation with `pio run`

#### 1.2 — DSP Data Structures and State
- Create `src/dsp_pipeline.h` with the structs defined above
- Add `DspState dspState` to AppState (or as a separate singleton)
- Add dirty flags: `isDspConfigDirty()`, `isDspMetricsDirty()`
- Add `DSP_ENABLED` build flag (default ON, allows compile-time exclusion)

#### 1.3 — Coefficient Computation Module
- Create `src/dsp_coefficients.cpp` implementing:
  - `dsp_compute_biquad_coeffs(DspBiquadStage& stage, uint32_t sampleRate)` — dispatches to the correct ESP-DSP generator based on `stage.type`
  - `dsp_load_custom_coeffs(DspBiquadStage& stage, float b0, b1, b2, a1, a2)` — for direct REW import
  - `dsp_recompute_all_coeffs(DspChannelConfig& ch, uint32_t sampleRate)` — recalculates all stages (called on parameter change or sample rate change)
- Coefficients are recomputed only when parameters change, not every buffer cycle

#### 1.4 — DSP Processing Core
- Create `src/dsp_pipeline.cpp` implementing:
  - `dsp_init()` — initialize all state, compute default coefficients
  - `dsp_process_buffer(int32_t* buffer, int stereoFrames, int adcIndex)` — the main entry point, called from `process_adc_buffer()`
  - Internal flow per channel:
    1. Convert int32_t → float (÷ 8388607.0f)
    2. Apply input gain
    3. HPF biquad (if enabled)
    4. PEQ biquads × 10 (each if enabled)
    5. LPF biquad (if enabled)
    6. Limiter (if enabled)
    7. Apply output gain
    8. Convert float → int32_t (× 8388607.0f, with clamp)
  - `dsp_update_config(int channel, const DspChannelConfig& newConfig)` — thread-safe config update with double-buffering
  - `dsp_get_metrics()` — return processing time, CPU load

#### 1.5 — Thread Safety (Double-Buffering)
- DSP config is modified from the main loop (API handler) but read from the audio task (Core 0, priority 3)
- Use **double-buffering**: two `DspState` copies, an `activeIndex` atomic flag
  - API writes to the inactive copy, then atomically swaps `activeIndex`
  - Audio task always reads from `active = &states[activeIndex]`
- This avoids locks in the audio ISR path (critical for low latency)

#### 1.6 — Integration into I2S Audio Pipeline
- In `process_adc_buffer()` at line ~414, after DC-blocking:
  ```cpp
  #ifdef DSP_ENABLED
  if (!appState.dspBypass) {
      dsp_process_buffer(buffer, stereo_frames, adc);
  }
  #endif
  ```
- Processing time is measured with `esp_timer_get_time()` and stored in metrics

#### 1.7 — REST API Endpoints
- `GET /api/dsp` — return full DSP config as JSON
- `PUT /api/dsp` — update full DSP config from JSON
- `PUT /api/dsp/channel/{0-3}` — update single channel config
- `PUT /api/dsp/channel/{0-3}/peq/{0-9}` — update single PEQ band
- `PUT /api/dsp/channel/{0-3}/hpf` — update HPF
- `PUT /api/dsp/channel/{0-3}/lpf` — update LPF
- `PUT /api/dsp/channel/{0-3}/limiter` — update limiter
- `POST /api/dsp/bypass` — toggle global bypass
- `GET /api/dsp/metrics` — return processing time / CPU load

#### 1.8 — NVS Persistence
- Save DSP config to NVS via settings_manager
- Namespace: `"dsp"` with keys like `"ch0_peq0_freq"`, `"ch0_peq0_gain"`, etc.
- Alternatively, serialize entire config as JSON blob (simpler, uses one NVS key per channel)
- Load on boot, apply coefficients before audio task starts

#### 1.9 — Unit Tests
- `test/test_dsp/test_dsp.cpp`:
  - Test coefficient computation for each filter type
  - Test biquad processing with known input/output (sine wave through LPF)
  - Test limiter behavior (signal above threshold gets attenuated)
  - Test bypass mode (output == input)
  - Test config double-buffering swap
  - Test REW APO format parsing
  - Test miniDSP biquad format parsing
  - Test coefficient normalization (a0 == 1.0)
  - Test edge cases: Q = 0, freq = 0, freq = Nyquist

### Phase 1 Deliverables
- Working 4-channel DSP pipeline (HPF + 10-band PEQ + LPF + Limiter)
- REST API for full control
- WebSocket broadcast of DSP metrics
- NVS persistence
- All unit tests passing
- Serial debug logging with `[DSP]` prefix

---

## Phase 2: REW Import/Export + FIR Filters

### Goal
Add REW file format support and FIR convolution for room correction.

### 2.1 — REW File Import (Equalizer APO Format)
- Create `src/dsp_rew_parser.h` / `.cpp`
- `dsp_parse_apo_filters(const char* text, DspChannelConfig& config)`:
  - Parse lines like `Filter 1: ON PK Fc 63.00 Hz Gain -3.2 dB Q 4.32`
  - Map filter types: `PK` → PeakingEQ, `LP`/`LPQ` → Lowpass, `HP`/`HPQ` → Highpass, `LS`/`LSC` → LowShelf, `HS`/`HSC` → HighShelf, `NO` → Notch, `AP` → Allpass
  - Populate PEQ bands in order, set HPF/LPF if HP/LP types found
  - Handle `ON`/`OFF` states
- Web UI upload: `POST /api/dsp/import/apo` with text body
- Apply to selected channel(s)

### 2.2 — REW File Import (miniDSP Biquad Coefficients)
- `dsp_parse_minidsp_biquads(const char* text, DspChannelConfig& config)`:
  - Parse: `biquad1, b0=1.001, b1=-1.987, b2=0.986, a1=-1.987, a2=0.987`
  - **Sign correction**: miniDSP negates a1/a2 vs standard form
  - Load as `BiquadType::CUSTOM` — bypass coefficient generators
- Web UI upload: `POST /api/dsp/import/minidsp` with text body

### 2.3 — REW File Export
- `GET /api/dsp/export/apo?channel={0-3}` — export as Equalizer APO text
- `GET /api/dsp/export/minidsp?channel={0-3}` — export as miniDSP biquad text
- `GET /api/dsp/export/json` — export full config as JSON

### 2.4 — FIR Filter Engine
- Implement `dsp_fir_process(DspFirFilter& fir, float* buffer, int len)`:
  - Uses ESP-DSP `dsps_fir_f32()` for optimized convolution
  - Tap count limited to `DSP_MAX_FIR_TAPS` (default 256, configurable)
  - Coefficients heap-allocated on import, freed on clear
- FIR inserts in the chain after PEQ, before limiter:
  `HPF → PEQ × 10 → LPF → FIR → Limiter`

### 2.5 — FIR Coefficient Import
- **WAV impulse response**: Parse 32-bit float mono WAV, extract samples as FIR taps
  - `POST /api/dsp/import/fir/wav?channel={0-3}` with binary WAV body
  - Validate: mono, 32-bit float or 16-bit PCM, sample rate matches current rate
  - Truncate or zero-pad to `DSP_MAX_FIR_TAPS`
- **Text coefficient list**: One coefficient per line
  - `POST /api/dsp/import/fir/text?channel={0-3}` with text body

### 2.6 — CPU Budget Check for FIR
- 128-tap FIR: ~1.7 cycles/tap × 128 = 218 cycles/sample × 4 channels = 872 cycles/sample
- 256-tap FIR: ~1.7 × 256 = 435 × 4 = 1,742 cycles/sample
- Budget impact: 128 taps = ~6.8%, 256 taps = ~13.6% — both within headroom
- For longer FIR (512+), consider overlap-save FFT convolution or offloading to Core 1

### 2.7 — Unit Tests for Phase 2
- REW APO parser: various filter types, ON/OFF, edge cases
- miniDSP parser: sign convention, multiple biquads
- FIR processing: impulse response, known convolution results
- WAV import: valid/invalid files, sample rate mismatch handling
- Export round-trip: import → export → import should produce identical config

### Phase 2 Deliverables
- REW Equalizer APO import/export
- miniDSP biquad coefficient import/export
- JSON config export
- FIR filter processing (up to 256 taps)
- WAV impulse response import
- Unit tests for all parsers

---

## Phase 3: Web UI for DSP Control

### Goal
Full web-based DSP configuration interface with real-time frequency response visualization.

### 3.1 — DSP Control Page (web_pages.cpp)
- New page accessible from the main web UI navigation
- Tabbed layout: one tab per channel (L1, R1, L2, R2) + "All" for linked editing
- Sections per channel:
  - **Input Gain** — slider, -20 to +20 dB
  - **HPF** — enable toggle, frequency (20-2000 Hz), Q (0.1-10), type dropdown
  - **PEQ Bands** — 10 rows, each with: enable toggle, frequency (20-20000 Hz), gain (-20 to +20 dB), Q (0.1-30), type dropdown (Peak/Shelf/Notch)
  - **LPF** — enable toggle, frequency (200-20000 Hz), Q (0.1-10), type dropdown
  - **FIR** — enable toggle, tap count display, import button, clear button
  - **Limiter** — enable toggle, threshold (-30 to 0 dB), attack (0.1-100 ms), release (10-1000 ms)
  - **Output Gain** — slider, -20 to +20 dB
  - **Bypass** — per-channel and global

### 3.2 — Frequency Response Graph
- Canvas-based frequency response visualization (20 Hz – 20 kHz, log scale)
- Draws the combined magnitude response of all enabled filter stages
- Updates in real-time as parameters are adjusted
- Implementation: compute magnitude response from biquad transfer functions in JavaScript:
  ```
  H(f) = Π [b0 + b1·z⁻¹ + b2·z⁻²] / [1 + a1·z⁻¹ + a2·z⁻²]
  where z = e^(j·2π·f/fs)
  ```
- Overlay individual filter curves (dimmed) and combined curve (bright)
- Y-axis: -24 dB to +24 dB

### 3.3 — REW Import/Export UI
- "Import" dropdown button:
  - "From REW (Equalizer APO)" → file picker → uploads, parses, populates PEQ
  - "From miniDSP (Biquad)" → file picker → uploads, populates as custom coefficients
  - "From FIR (WAV)" → file picker → uploads impulse response
  - "From JSON (Backup)" → file picker → restores full config
- "Export" dropdown button:
  - "As REW (Equalizer APO)" → downloads .txt file
  - "As miniDSP (Biquad)" → downloads .txt file
  - "As JSON (Backup)" → downloads .json file
- Channel selector for import target

### 3.4 — Channel Linking
- Option to link channels (L1+R1, L2+R2, or all 4)
- When linked, parameter changes apply to all linked channels simultaneously
- Independent gain offsets still possible when linked

### 3.5 — Presets
- Save/load named DSP presets (stored in NVS or SPIFFS)
- Built-in presets:
  - "Flat" (all bypass)
  - "Subsonic Filter" (HPF @ 25 Hz, Q 0.707)
  - "Speech" (HPF @ 80 Hz, PEQ boost 2-4 kHz)
  - "Loudness" (bass/treble shelf boost)
- User-defined presets (up to 5, stored as JSON blobs)

### 3.6 — WebSocket Real-Time Updates
- DSP metrics broadcast every 1s: processing time, CPU load, gain reduction (limiter activity)
- Parameter changes from web UI sent via WebSocket for instant response
- Multi-client sync: parameter change on one client broadcasts to all others

### 3.7 — Build Asset Pipeline
- After editing `web_pages.cpp`, run `node build_web_assets.js` to regenerate gzipped version
- All new JS/CSS for frequency response graph kept inline in the DSP page HTML

### Phase 3 Deliverables
- Full web UI for DSP control with per-channel configuration
- Real-time frequency response graph
- REW import/export via file upload/download
- Channel linking
- DSP presets (built-in + user-defined)
- WebSocket real-time DSP metrics

---

## Phase 4: I2S DAC Output

### Goal
Add I2S transmit (TX) path to output processed audio to an external DAC for active speaker management.

### 4.1 — Hardware Requirements
- **DAC**: PCM5102A (I2S input, 32-bit, up to 384 kHz, ~$3)
  - Or ES9023 / ES9038Q2M for higher quality
  - Stereo output per DAC chip; need 2 DACs for 4 output channels
- **I2S TX pins**: Need to assign 3 GPIO pins (BCK_OUT, WS_OUT, DOUT) per DAC
  - Option A: Reuse I2S0 in full-duplex mode (simultaneous RX + TX on same port)
  - Option B: Use a separate I2S peripheral if available
- **GPIO allocation**: ESP32-S3 has 2 I2S peripherals. Currently both are used for ADC input.
  - **Proposed change**: Use I2S0 in full-duplex (RX from ADC1 + TX to DAC1), use I2S1 for ADC2 input only. TX for channels 3-4 would need a TDM approach or external I2S multiplexer.
  - Alternative: Use ESP32-S3's I2S TDM mode (up to 16 channels on one port)

### 4.2 — I2S TX Driver
- Configure I2S0 TX with matching sample rate and format to RX
- Double-buffered DMA output: fill TX buffer from DSP output while previous buffer plays
- Latency target: 2 × 256 frames = 512 frames = ~10.7 ms at 48 kHz (input-to-output)
- Create `i2s_audio_output_init()` and `i2s_audio_write_output()` functions

### 4.3 — Output Routing Matrix
- Configurable routing: any input channel can feed any output channel
- Matrix: `outputRouting[4] = {INPUT_L1, INPUT_R1, INPUT_L2, INPUT_R2}` (default 1:1)
- Allows mono-summing, channel swapping, or creative routing
- Controlled via API: `PUT /api/dsp/routing`

### 4.4 — Crossover Integration (Future Extension)
- Crossover filters (Linkwitz-Riley 4th order = 2 cascaded Butterworth biquads)
- Each input channel can split into 2-3 frequency bands
- Each band routes to a different output
- This requires more output channels than inputs — may need additional DAC hardware

### 4.5 — Pin Assignment
- Proposed I2S TX pins (verify no conflicts with existing assignments):
  - DAC1: BCK_OUT=39, WS_OUT=40, DOUT=41 (or other available GPIOs)
  - DAC2: BCK_OUT=42, WS_OUT=45, DOUT=46 (if 4-channel output needed)
- **Must avoid GPIO 19/20** (USB pins)
- Final pin assignment requires hardware review of the ALX Nova PCB

### Phase 4 Deliverables
- I2S TX driver for PCM5102A DAC output
- Processed audio output with matched sample rate
- Output routing matrix
- ~10.7 ms input-to-output latency
- Hardware design guidance for DAC connection

---

## Phase 5: MQTT + TFT Screen DSP Control

### Goal
Extend DSP control to MQTT (Home Assistant) and the TFT display with rotary encoder.

### 5.1 — MQTT Integration
- **Discovery**: Publish Home Assistant MQTT discovery messages for DSP entities:
  - `number` entities for each PEQ band (freq, gain, Q)
  - `switch` entities for HPF/LPF/limiter enable and global bypass
  - `select` entity for preset selection
- **Command topics**: `alx_nova/dsp/channel/{n}/peq/{n}/set` with JSON payload
- **State topics**: `alx_nova/dsp/channel/{n}/state` — publish full channel config on change
- **Metrics topic**: `alx_nova/dsp/metrics` — processing time, limiter GR
- Follows existing MQTT handler pattern in `src/mqtt_handler.cpp`

### 5.2 — TFT Screen (LVGL)
- New LVGL screen: **DSP Overview**
  - Shows enabled filters per channel as compact icons/badges
  - Shows limiter gain reduction as bar meters
  - Shows DSP CPU load percentage
- New LVGL screen: **DSP Edit** (accessed from DSP Overview)
  - Rotary encoder navigates between: channel select → filter type → parameter
  - Parameters adjusted by rotating encoder, confirmed by click
  - Simplified view: shows one filter at a time with freq/gain/Q values
- Navigation: DSP Overview accessible from the desktop carousel
- All screens guarded by `#ifdef GUI_ENABLED`

### Phase 5 Deliverables
- Full MQTT control with Home Assistant auto-discovery
- TFT DSP overview screen with real-time metrics
- TFT DSP edit screen with rotary encoder control
- Bidirectional sync: changes from any interface reflected on all others

---

## Phase 6: Advanced Features & Optimization

### 6.1 — Assembly-Optimized ESP-DSP
- Replace ANSI C biquad with assembly-optimized `_aes3` variant
- Requires linking against the pre-compiled ESP-DSP library in the Arduino SDK
- Expected improvement: ~24 cycles/sample → ~17 cycles/sample per biquad
- Only pursue if CPU budget becomes tight

### 6.2 — Overlap-Save FFT Convolution for Long FIR
- For FIR filters > 256 taps, direct convolution becomes expensive
- Implement overlap-save method using ESP-DSP's FFT:
  - FFT the input block and the IR (pre-computed, stored in frequency domain)
  - Complex multiply
  - IFFT
  - Overlap-save bookkeeping
- Enables 512-2048 tap FIR within CPU budget

### 6.3 — REW Bridge Application
- Python script (`tools/rew_bridge.py`) that:
  - Connects to REW's REST API (localhost:4735)
  - Polls for filter changes every 500ms
  - Converts to ESP32 JSON format
  - Publishes via MQTT or WebSocket
- Packaged as a standalone tool in the `tools/` directory

### 6.4 — Auto-EQ Integration
- Import AutoEQ profiles (headphone/speaker correction curves)
- Parse AutoEQ's ParametricEQ.txt format (compatible with Equalizer APO)
- Community profile library accessible from web UI

### 6.5 — Dedicated DSP Chip Evaluation
If the ESP32-S3 CPU becomes the bottleneck (unlikely for the current scope):

| Chip | Cost | Channels | FIR Taps | Interface | Notes |
|---|---|---|---|---|---|
| ADAU1452 | ~$12 | 24×24 | 2048/ch | SPI/I2C | SigmaStudio GUI, 294MHz DSP |
| ADAU1701 | ~$5 | 4×4 | 512/ch | I2C | Older, still very capable |
| TAS3251 | ~$8 | 2 | 192/ch | I2C | Integrated Class-D amp |

**Recommendation**: The ESP32-S3 handles the defined scope (10-band PEQ + HPF + LPF + limiter + 256-tap FIR × 4 channels) at ~35% CPU. A dedicated DSP chip is not needed unless requirements grow significantly (e.g., 2048-tap FIR for all channels, or real-time convolution reverb).

---

## Implementation Priority & Timeline

| Phase | Description | Dependencies | Complexity |
|---|---|---|---|
| **Phase 1** | Core DSP engine + API | None | High (foundational) |
| **Phase 2** | REW import/export + FIR | Phase 1 | Medium |
| **Phase 3** | Web UI | Phase 1 | Medium-High |
| **Phase 4** | I2S DAC output | Phase 1, hardware | High (HW + SW) |
| **Phase 5** | MQTT + TFT control | Phase 1, Phase 3 | Medium |
| **Phase 6** | Advanced optimization | All | Low priority |

Phases 2 and 3 can be developed in parallel after Phase 1 is complete.
Phase 4 requires hardware decisions and can start design during Phase 2/3.
Phase 5 depends on Phase 3 (UI patterns) but the MQTT portion can start after Phase 1.

---

## Testing Strategy

### Unit Tests (native platform, no hardware)

| Test Module | Tests |
|---|---|
| `test_dsp` | Coefficient computation, biquad processing, limiter, bypass, gain, config double-buffer |
| `test_dsp_rew` | APO parser, miniDSP parser, export formatting, round-trip |
| `test_dsp_fir` | FIR processing, WAV import, tap count validation |

### Integration Tests (on hardware)

- Loopback test: inject known signal via signal_generator → DSP → measure output FFT
- Latency measurement: pulse injection → output detection
- Stress test: all 4 channels, all filters enabled, measure CPU load
- Long-running stability: 24h continuous operation

### Verification with REW

- Generate correction curve in REW from measurement
- Export as Equalizer APO text
- Import into ESP32 DSP
- Re-measure: verify correction applied correctly
- This is the ultimate end-to-end validation

---

## Risk Assessment

| Risk | Impact | Mitigation |
|---|---|---|
| ESP-DSP ANSI C integration issues with PlatformIO | Medium | Fall back to standalone biquad implementation (RBJ cookbook, ~100 lines) |
| CPU overload with all filters active on 4 channels | Low | Budget shows 35% usage; assembly optimization available as escape valve |
| I2S full-duplex complications (Phase 4) | High | Can use TDM mode or defer to separate TX-only I2S port |
| NVS space for DSP config | Low | ~2.3 KB per config; NVS partition is typically 20-24 KB |
| Web UI complexity (frequency response graph) | Medium | Start with parameter-only UI, add graph as enhancement |
| Thread safety between API and audio task | High | Double-buffering design eliminates locks in audio path |
