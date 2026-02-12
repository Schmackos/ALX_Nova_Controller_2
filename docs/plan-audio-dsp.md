# Audio DSP Pipeline — Implementation Plan

## Scope Summary

Based on the interview and research, the target DSP system is:

| Decision | Choice |
|---|---|
| **Use case** | Active crossover / speaker management + FIR |
| **Channels** | 4 independent (both stereo pairs from dual PCM1808) |
| **Filter chain** | Fully configurable — user adds/removes stages dynamically via web UI |
| **Audio output** | I2S DAC output (hardware addition required) |
| **FIR filters** | Included from Phase 1 (up to 256 taps, longer via FFT convolution in Phase 6) |
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
    (L2/R2)              │  └───────┘    │ Dynamic chain│  │
                         │               │ of stages:   │  │
                         │               │  ┌─ Gain ──┐ │  │
                         │               │  ├─ HPF    │ │  │
                         │               │  ├─ PEQ    │ │  │
                         │               │  ├─ LPF    │ │  │
                         │               │  ├─ Shelf  │ │  │
                         │               │  ├─ Notch  │ │  │
                         │               │  ├─ FIR    │ │  │
                         │               │  ├─ Limiter│ │  │
                         │               │  └─ Gain ──┘ │  │
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

### Fully Configurable Filter Chain

Instead of a fixed topology (HPF → PEQ × 10 → LPF → Limiter), each channel has an **ordered list of generic stages** that the user can add, remove, reorder, and enable/disable dynamically. This matches how tools like miniDSP, SigmaStudio, and REW work, and makes REW import more natural — just append parsed filters as stages instead of mapping into fixed slots.

**Safety guardrails**:
- Hard cap: `DSP_MAX_STAGES = 20` per channel (20 biquads × 4 channels = ~27% CPU — safe)
- Real-time CPU load monitoring in `dsp_get_metrics()` — warn in Web UI if >80% budget
- Budget display: "DSP CPU: 23%" updated live in the web UI and via WebSocket

### DSP Insertion Point

The DSP pipeline inserts into `process_adc_buffer()` in `src/i2s_audio.cpp` at **line ~414**, after the DC-blocking IIR filter and before RMS/VU/FFT analysis. This means:

1. All analysis (RMS, VU, waveform, FFT) reflects the **post-DSP** signal
2. The existing DC-blocking filter becomes the first stage of the DSP chain
3. Processing is per-ADC (called twice per buffer cycle, once for each stereo pair)
4. The buffer is `int32_t[]` with 24-bit audio — DSP will convert to `float`, process, convert back

### CPU Budget (ESP32-S3 @ 240 MHz, 48 kHz, 256 frames/buffer)

Worst-case scenario: 20 stages per channel × 4 channels:

| Operation | Cycles/sample | Per buffer (256 frames) | % of 5.33ms budget |
|---|---|---|---|
| DC-blocking IIR (existing) | ~10 | 2,560 | 0.2% |
| 1 biquad stage (ESP-DSP ANSI C) | ~24 | 6,144 | 0.5% |
| 20 biquad stages × 1 channel | ~480 | 122,880 | 9.6% |
| **20 stages × 4 channels** | | **491,520** | **38.5%** |
| 128-tap FIR × 1 channel | ~218 | 55,808 | 4.4% |
| **128-tap FIR × 4 channels** | | **223,232** | **17.5%** |
| Limiter × 4 channels | ~50 | 51,200 | 4.0% |
| Existing analysis (RMS/VU/FFT) | | ~200,000 | 15.7% |

| Scenario | Total CPU % |
|---|---|
| Light (5 biquads + FIR + limiter × 4ch) | ~28% |
| Medium (10 biquads + FIR + limiter × 4ch) | ~42% |
| Heavy (20 biquads + FIR + limiter × 4ch) | ~60% |
| Maximum theoretical (all maxed out) | ~76% |

**Verdict**: Even the maximum theoretical load leaves ~24% headroom for WiFi, MQTT, WebSocket, and sensing. Practical usage (10 stages/channel) sits at a comfortable ~42%.

---

## Library Selection

### Primary: ESP-DSP (Espressif Official)

**Why**: Assembly-optimized biquad (`dsps_biquad_f32_aes3`) achieves ~17 cycles/sample on ESP32-S3. Apache 2.0 license. Built-in coefficient generators for all needed filter types.

**Integration approach**: Include the ANSI C source files directly in the project's `lib/` directory. This guarantees PlatformIO/Arduino compatibility. The ANSI C biquad is ~24 cycles/sample (vs 17 for assembly) — still well within budget. If assembly optimization is needed later, build flags can enable it.

**Key functions used**:
- `dsps_biquad_f32()` — process samples through a biquad section
- `dsps_biquad_gen_lpf_f32()` — generate LPF coefficients
- `dsps_biquad_gen_hpf_f32()` — generate HPF coefficients
- `dsps_biquad_gen_bpf_f32()` — generate BPF coefficients
- `dsps_biquad_gen_notch_f32()` — generate Notch coefficients
- `dsps_biquad_gen_peakingEQ_f32()` — generate PEQ coefficients
- `dsps_biquad_gen_highShelf_f32()` / `dsps_biquad_gen_lowShelf_f32()` — shelf filters
- `dsps_fir_init_f32()` / `dsps_fir_f32()` — FIR filter init and processing

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

### Import Formats

| Format | Extension | Parser Complexity | Use Case |
|---|---|---|---|
| **Equalizer APO** | `.txt` | Simple text parse | REW "Export filter settings as text" |
| **miniDSP biquad** | `.txt` | Simple text parse | Direct coefficient loading |
| **WAV impulse response** | `.wav` | Binary header parse | FIR room correction from REW measurement |
| **Text FIR coefficients** | `.txt` | One float per line | FIR taps from any tool |

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

### Export Formats

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

### Fully Configurable DSP Chain

```cpp
// Safety limits
#define DSP_MAX_STAGES       20    // Max filter stages per channel
#define DSP_MAX_FIR_TAPS     256   // Max FIR taps (direct convolution)
#define DSP_MAX_CHANNELS     4     // L1, R1, L2, R2

// ─── Stage Types ───────────────────────────────────────────────
// Each stage is a generic processing block in the chain.
// The user can add any combination in any order.
enum class DspStageType : uint8_t {
    // Biquad-based filters (all use the same biquad processor)
    BIQUAD_LPF,           // Low-pass filter
    BIQUAD_HPF,           // High-pass filter
    BIQUAD_BPF,           // Band-pass filter
    BIQUAD_NOTCH,         // Notch (band-reject)
    BIQUAD_PEQ,           // Parametric EQ (peaking)
    BIQUAD_LOW_SHELF,     // Low shelf
    BIQUAD_HIGH_SHELF,    // High shelf
    BIQUAD_ALLPASS,       // Allpass (phase adjustment)
    BIQUAD_CUSTOM,        // Direct coefficients (REW/miniDSP import)

    // Non-biquad processors
    LIMITER,              // Peak limiter
    FIR,                  // FIR convolution filter
    GAIN,                 // Simple gain stage (dB)

    STAGE_TYPE_COUNT      // Sentinel for validation
};

// ─── Biquad Parameters ────────────────────────────────────────
struct DspBiquadParams {
    float frequency = 1000.0f;    // Center/corner frequency (Hz)
    float gain = 0.0f;            // Gain in dB (PEQ, shelf types)
    float Q = 0.707f;             // Quality factor

    // Computed coefficients (recalculated when params change)
    float coeffs[5] = {1.0f, 0, 0, 0, 0};  // [b0, b1, b2, a1, a2]
    float delay[2] = {0};                    // Biquad delay line (state)
};

// ─── Limiter Parameters ───────────────────────────────────────
struct DspLimiterParams {
    float thresholdDb = 0.0f;     // Threshold in dBFS
    float attackMs = 5.0f;        // Attack time (ms)
    float releaseMs = 50.0f;      // Release time (ms)
    float ratio = 20.0f;          // Compression ratio (20:1 ≈ limiting)

    // Runtime state (not persisted)
    float envelope = 0.0f;        // Current envelope level
    float gainReduction = 0.0f;   // Current GR in dB (for metering)
};

// ─── FIR Parameters ──────────────────────────────────────────
struct DspFirParams {
    uint16_t numTaps = 0;                     // Active tap count
    float coefficients[DSP_MAX_FIR_TAPS] = {0}; // Tap coefficients
    float delayLine[DSP_MAX_FIR_TAPS] = {0};    // FIR delay line (state)
    uint16_t delayPos = 0;                       // Circular buffer position
};

// ─── Gain Parameters ─────────────────────────────────────────
struct DspGainParams {
    float gainDb = 0.0f;          // Gain in dB
    float gainLinear = 1.0f;      // Pre-computed linear gain (10^(dB/20))
};

// ─── Generic DSP Stage ───────────────────────────────────────
// A single processing block in the chain. Uses a tagged union
// to hold type-specific parameters.
struct DspStage {
    bool enabled = true;              // Bypass this individual stage
    DspStageType type = DspStageType::BIQUAD_PEQ;
    char label[16] = "";              // User-defined label (e.g., "Sub HPF")

    union {
        DspBiquadParams biquad;
        DspLimiterParams limiter;
        DspFirParams fir;             // Note: large — only one FIR per channel recommended
        DspGainParams gain;
    };

    // Default constructor initializes the biquad variant
    DspStage() : biquad() {}
};

// ─── Channel Configuration ───────────────────────────────────
// Ordered list of stages. Processing flows from stages[0] → stages[stageCount-1].
struct DspChannelConfig {
    bool bypass = false;              // Bypass entire channel DSP
    uint8_t stageCount = 0;           // Number of active stages
    DspStage stages[DSP_MAX_STAGES];  // Ordered processing chain
};

// ─── Global DSP State ────────────────────────────────────────
struct DspState {
    bool globalBypass = false;          // Master bypass all channels
    uint32_t sampleRate = 48000;        // Current sample rate (Hz)
    DspChannelConfig channels[DSP_MAX_CHANNELS]; // L1, R1, L2, R2

    // Runtime metrics (updated by audio task, read by API/WebSocket)
    uint32_t processTimeUs = 0;         // Last buffer processing time (µs)
    uint32_t maxProcessTimeUs = 0;      // Peak processing time (µs)
    float cpuLoadPercent = 0.0f;        // DSP CPU usage estimate (%)
    float limiterGrDb[DSP_MAX_CHANNELS] = {0}; // Per-channel limiter GR (dB)
};
```

### Processing Loop

The core DSP loop is a simple iteration over the stage array:

```cpp
void dsp_process_channel(float* buffer, int len, DspChannelConfig& ch) {
    if (ch.bypass) return;

    for (int i = 0; i < ch.stageCount; i++) {
        DspStage& s = ch.stages[i];
        if (!s.enabled) continue;

        switch (s.type) {
            case DspStageType::BIQUAD_LPF:
            case DspStageType::BIQUAD_HPF:
            case DspStageType::BIQUAD_BPF:
            case DspStageType::BIQUAD_NOTCH:
            case DspStageType::BIQUAD_PEQ:
            case DspStageType::BIQUAD_LOW_SHELF:
            case DspStageType::BIQUAD_HIGH_SHELF:
            case DspStageType::BIQUAD_ALLPASS:
            case DspStageType::BIQUAD_CUSTOM:
                dsps_biquad_f32(buffer, buffer, len,
                                s.biquad.coeffs, s.biquad.delay);
                break;

            case DspStageType::LIMITER:
                dsp_limiter_process(s.limiter, buffer, len);
                break;

            case DspStageType::FIR:
                dsp_fir_process(s.fir, buffer, len);
                break;

            case DspStageType::GAIN:
                dsp_gain_process(s.gain, buffer, len);
                break;
        }
    }
}
```

### API Operations for Dynamic Chain

```
POST   /api/dsp/channel/{0-3}/stage          — Add a new stage (append or insert at position)
DELETE /api/dsp/channel/{0-3}/stage/{index}   — Remove a stage
PUT    /api/dsp/channel/{0-3}/stage/{index}   — Update stage parameters
POST   /api/dsp/channel/{0-3}/stage/reorder   — Reorder stages [newOrder: [3,0,1,2,...]]
GET    /api/dsp/channel/{0-3}                 — Get channel config (all stages)
```

### Memory Footprint

| Component | Size | Notes |
|---|---|---|
| DspBiquadParams | 36 bytes | 5 coeffs + 2 delay + freq/gain/Q |
| DspLimiterParams | 24 bytes | 4 params + 2 state |
| DspFirParams | 2,056 bytes | 256 coeffs + 256 delay + pos + count |
| DspGainParams | 8 bytes | dB + linear |
| DspStage (biquad) | ~56 bytes | type + enabled + label + union |
| DspStage (FIR) | ~2,076 bytes | Dominates due to tap arrays |
| **DspChannelConfig (no FIR)** | **~1,140 bytes** | 20 biquad stages worst case |
| **DspChannelConfig (1 FIR)** | **~3,196 bytes** | 19 biquads + 1 FIR |
| **4 channels (no FIR)** | **~4,560 bytes** | |
| **4 channels (1 FIR each)** | **~12,784 bytes** | |
| Double-buffer (×2) | **~25,568 bytes** | For thread-safe config swap |
| Float conversion buffer | 2,048 bytes | `float dspBuffer[512]` (static) |
| **Total DSP memory** | **~28 KB** | ~8.7% of 320 KB internal SRAM |

**Note**: FIR with `DSP_MAX_FIR_TAPS = 256` is the dominant memory consumer. If memory becomes tight, FIR arrays can be heap-allocated on demand instead of statically embedded in the union. For Phase 6 longer FIR (512+), heap allocation with PSRAM would be the path.

---

## Phase 1: Core DSP Engine + FIR (Foundation)

### Goal
Build the fully configurable DSP processing pipeline with all stage types (biquad variants, FIR, limiter, gain), REST API, and basic REW import/export. No Web UI yet — controlled via REST API and WebSocket JSON commands.

### Files to Create

| File | Purpose |
|---|---|
| `src/dsp_pipeline.h` | DSP data structures (DspStage, DspChannelConfig, DspState), public API |
| `src/dsp_pipeline.cpp` | DSP processing engine: stage loop, biquad, FIR, limiter, gain processors |
| `src/dsp_coefficients.h` | Biquad coefficient computation API |
| `src/dsp_coefficients.cpp` | RBJ cookbook coefficient generators, custom coefficient loading |
| `src/dsp_rew_parser.h` | REW import/export API |
| `src/dsp_rew_parser.cpp` | Equalizer APO parser, miniDSP parser, WAV IR parser, exporters |
| `lib/esp_dsp_lite/` | Extracted ESP-DSP biquad + FIR ANSI C sources |

### Files to Modify

| File | Changes |
|---|---|
| `src/i2s_audio.cpp` | Insert `dsp_process_buffer()` call after DC-blocking filter |
| `src/i2s_audio.h` | Add DSP enable/bypass flag access |
| `src/app_state.h` | Add DSP-related state members and dirty flags |
| `src/config.h` | Add DSP defaults (`DSP_MAX_STAGES`, `DSP_MAX_FIR_TAPS`, etc.) |
| `src/settings_manager.cpp` | Persist DSP config to NVS (JSON blob per channel) |
| `src/main.cpp` | Register DSP API endpoints |
| `platformio.ini` | Add ESP-DSP source include paths, `DSP_ENABLED` build flag |

### Implementation Steps

#### 1.1 — Integrate ESP-DSP Library
- Extract from the ESP-DSP repository (ANSI C versions only):
  - `dsps_biquad.c` / `dsps_biquad.h` — biquad filter processing
  - `dsps_biquad_gen.c` / `dsps_biquad_gen.h` — coefficient generation (LPF, HPF, BPF, Notch, PEQ, Shelf)
  - `dsps_fir.c` / `dsps_fir.h` — FIR filter processing
  - Supporting headers: `dsps_math.h`, `esp_dsp.h`, `dsp_err.h`
- Place in `lib/esp_dsp_lite/` with a `library.json` for PlatformIO discovery
- Add appropriate `#define` guards for ANSI-only compilation (no assembly)
- Verify compilation with `pio run`

#### 1.2 — DSP Data Structures and State
- Create `src/dsp_pipeline.h` with all structs as defined in the Data Structures section above
- Add to AppState:
  - `bool dspEnabled` — master enable
  - `bool dspBypass` — master bypass (pass-through)
  - Dirty flags: `isDspConfigDirty()`, `isDspMetricsDirty()`
- Add `DSP_ENABLED` build flag in `platformio.ini` (default ON)
- Add config defaults in `src/config.h`:
  ```cpp
  #define DSP_MAX_STAGES       20
  #define DSP_MAX_FIR_TAPS     256
  #define DSP_MAX_CHANNELS     4
  #define DSP_DEFAULT_Q        0.707f
  #define DSP_CPU_WARN_PERCENT 80.0f
  ```

#### 1.3 — Coefficient Computation Module
- Create `src/dsp_coefficients.cpp` implementing:
  - `dsp_compute_biquad_coeffs(DspBiquadParams& params, DspStageType type, uint32_t sampleRate)` — dispatches to the correct ESP-DSP generator based on type. Normalizes frequency to `f/sampleRate` as ESP-DSP expects.
  - `dsp_load_custom_coeffs(DspBiquadParams& params, float b0, b1, b2, a1, a2)` — for direct REW/miniDSP import, sets type to `BIQUAD_CUSTOM`
  - `dsp_recompute_channel_coeffs(DspChannelConfig& ch, uint32_t sampleRate)` — recalculates all biquad stages in a channel
  - `dsp_compute_gain_linear(DspGainParams& params)` — precompute `10^(dB/20)`
- Coefficients are recomputed **only** when parameters change (on API call), not every buffer cycle
- Coefficient recomputation happens on the inactive double-buffer copy, then swapped atomically

#### 1.4 — DSP Processing Core
- Create `src/dsp_pipeline.cpp` implementing:
  - `dsp_init()` — zero all state, set defaults, compute initial coefficients
  - `dsp_process_buffer(int32_t* buffer, int stereoFrames, int adcIndex)` — main entry point:
    1. Start timer (`esp_timer_get_time()`)
    2. Deinterleave stereo buffer into L and R float arrays (÷ 8388607.0f)
    3. For each channel (L, R mapped from adcIndex):
       - Call `dsp_process_channel(floatBuf, len, activeConfig.channels[ch])`
    4. Re-interleave float L/R back to int32_t stereo (× 8388607.0f with clamp)
    5. Record processing time in metrics
  - `dsp_process_channel()` — the stage loop as shown in Processing Loop section
  - `dsp_limiter_process(DspLimiterParams& lim, float* buf, int len)`:
    - Peak envelope follower with attack/release ballistics
    - Gain computation: `gainDb = min(0, threshold - envelope)` (simplified)
    - Smooth gain application to avoid zipper noise
  - `dsp_fir_process(DspFirParams& fir, float* buf, int len)`:
    - Direct-form FIR convolution using ESP-DSP `dsps_fir_f32()` or manual loop
    - Circular delay line management
  - `dsp_gain_process(DspGainParams& gain, float* buf, int len)`:
    - Simple multiply by `gainLinear`

#### 1.5 — Thread Safety (Double-Buffering)
- DSP config is modified from the main loop (API handler) but read from the audio task (Core 0, priority 3)
- Use **double-buffering**: two `DspState` instances, an `volatile int activeIndex` flag
  - API writes to `states[1 - activeIndex]` (inactive copy)
  - Recalculates coefficients on the inactive copy
  - Atomically sets `activeIndex = 1 - activeIndex`
  - Audio task always reads from `states[activeIndex]`
- Delay lines (`biquad.delay[]`, `fir.delayLine[]`) are in the active config and must NOT be swapped mid-stream — on swap, copy delay state from old active to new active to avoid clicks
- This avoids locks/mutexes in the audio processing path (critical for deterministic latency)

#### 1.6 — Integration into I2S Audio Pipeline
- In `process_adc_buffer()` in `src/i2s_audio.cpp` at line ~414, after DC-blocking:
  ```cpp
  #ifdef DSP_ENABLED
  if (!appState.dspBypass) {
      dsp_process_buffer(buffer, stereo_frames, adc);
  }
  #endif
  ```
- Include `dsp_pipeline.h` in `i2s_audio.cpp`
- Call `dsp_init()` from `i2s_audio_init()` before the audio capture task starts
- Processing time measured with `esp_timer_get_time()` bracketing the DSP call

#### 1.7 — Dynamic Chain REST API Endpoints
- **Global**:
  - `GET /api/dsp` — return full DSP config as JSON (all channels, all stages)
  - `PUT /api/dsp` — replace full DSP config from JSON
  - `POST /api/dsp/bypass` — toggle global bypass
  - `GET /api/dsp/metrics` — return processing time, CPU load, per-channel limiter GR
- **Per-channel**:
  - `GET /api/dsp/channel/{0-3}` — get channel config (bypass + all stages)
  - `PUT /api/dsp/channel/{0-3}/bypass` — toggle channel bypass
- **Stage CRUD (dynamic chain)**:
  - `POST /api/dsp/channel/{0-3}/stage` — add a new stage
    - Body: `{"type": "BIQUAD_PEQ", "enabled": true, "params": {"frequency": 1000, "gain": -3.0, "Q": 2.0}}`
    - Optional `"position"` field for insert-at-index (default: append)
    - Returns: new stage index and computed coefficients
    - Rejects if `stageCount >= DSP_MAX_STAGES`
  - `PUT /api/dsp/channel/{0-3}/stage/{index}` — update stage parameters
    - Recalculates coefficients if biquad params changed
  - `DELETE /api/dsp/channel/{0-3}/stage/{index}` — remove a stage
    - Shifts remaining stages down
  - `POST /api/dsp/channel/{0-3}/stage/reorder` — reorder stages
    - Body: `{"order": [3, 0, 1, 2, 4]}` — new index ordering
  - `POST /api/dsp/channel/{0-3}/stage/{index}/enable` — toggle stage enable
- **Import/Export**:
  - `POST /api/dsp/import/apo?channel={0-3}` — import Equalizer APO text (append as stages)
  - `POST /api/dsp/import/minidsp?channel={0-3}` — import miniDSP biquad coefficients
  - `POST /api/dsp/import/fir?channel={0-3}` — import FIR coefficients (text or WAV)
  - `GET /api/dsp/export/apo?channel={0-3}` — export as Equalizer APO text
  - `GET /api/dsp/export/minidsp?channel={0-3}` — export as miniDSP biquad text
  - `GET /api/dsp/export/json` — export full config as JSON

#### 1.8 — REW Import/Export Parsers

**Equalizer APO Import** (`dsp_parse_apo_filters`):
- Parse lines: `Filter N: ON|OFF TYPE Fc FREQ Hz Gain GAIN dB Q QVAL`
- Type mapping: `PK` → PEQ, `LP`/`LPQ` → LPF, `HP`/`HPQ` → HPF, `LS`/`LSC` → Low Shelf, `HS`/`HSC` → High Shelf, `NO` → Notch, `AP` → Allpass
- Each parsed filter becomes a new `DspStage` appended to the channel
- Respects `ON`/`OFF` → `stage.enabled`

**miniDSP Biquad Import** (`dsp_parse_minidsp_biquads`):
- Parse: `biquadN, b0=VAL, b1=VAL, b2=VAL, a1=VAL, a2=VAL`
- **Sign correction**: miniDSP negates a1/a2 relative to standard form
- Each biquad becomes a `BIQUAD_CUSTOM` stage with direct coefficients

**FIR Import** (`dsp_parse_fir_coefficients`):
- **Text format**: One float per line, parse into `DspFirParams.coefficients[]`
- **WAV format**: Parse RIFF/WAV header, extract PCM samples as taps
  - Support: 16-bit PCM (÷ 32768) and 32-bit float
  - Validate: mono, sample rate matches current DSP sample rate
  - Truncate to `DSP_MAX_FIR_TAPS` with warning if longer

**Export** (`dsp_export_apo`, `dsp_export_minidsp`, `dsp_export_json`):
- Reverse the import process — iterate stages and generate text
- BIQUAD_CUSTOM stages exported as miniDSP format (with sign negation)
- JSON export includes full DspState for backup/restore

#### 1.9 — NVS Persistence
- Serialize entire `DspChannelConfig` as a JSON blob per channel
- NVS keys: `"dsp_ch0"`, `"dsp_ch1"`, `"dsp_ch2"`, `"dsp_ch3"`, `"dsp_global"`
- JSON format matches the API format for consistency
- Load on boot in `dsp_init()`, recompute all coefficients
- Save triggered by dirty flag, debounced to avoid excessive NVS writes (save at most every 5 seconds)
- FIR coefficients stored separately due to size: `"dsp_fir0"` through `"dsp_fir3"` as binary blobs
- Factory reset clears all DSP keys

#### 1.10 — WebSocket Integration
- Broadcast DSP metrics every 1 second (when DSP is active):
  ```json
  {
    "type": "dspMetrics",
    "processTimeUs": 145,
    "cpuLoad": 12.3,
    "limiterGr": [-0.0, -0.0, -2.1, -0.0]
  }
  ```
- Broadcast config changes to all connected WebSocket clients when DSP config is modified via API
- Per-client subscription flag: `subscribeDspMetrics` (like existing `subscribeAudioLevels`)

#### 1.11 — Serial Debug Logging
- Prefix: `[DSP]`
- `LOG_I`: Init, stage add/remove, config load/save, import/export, bypass toggle
- `LOG_D`: Per-buffer processing time (periodic, not every buffer), coefficient dumps
- `LOG_W`: CPU load > 80%, FIR truncation, import parse warnings
- `LOG_E`: Config validation failures, NVS save errors

#### 1.12 — Unit Tests
- `test/test_dsp/test_dsp.cpp`:
  - Test coefficient computation for each biquad type (verify against known RBJ values)
  - Test biquad processing: sine wave through LPF → verify attenuation above cutoff
  - Test FIR processing: impulse → verify output matches coefficients; known convolution
  - Test limiter: signal above threshold → verify gain reduction; below threshold → unity gain
  - Test gain stage: verify dB-to-linear conversion accuracy
  - Test bypass mode: output buffer == input buffer (bitwise)
  - Test stage add/remove/reorder: verify chain integrity
  - Test max stage limit: reject when `stageCount >= DSP_MAX_STAGES`
  - Test double-buffer swap: verify delay line continuity
  - Test config serialization: JSON round-trip (serialize → deserialize → compare)
- `test/test_dsp_rew/test_dsp_rew.cpp`:
  - Test APO parser: all filter types (PK, LP, HP, LS, HS, NO, AP), ON/OFF, various formats
  - Test miniDSP parser: sign convention, multiple biquads, malformed input
  - Test FIR text parser: valid coefficients, empty lines, invalid values
  - Test WAV parser: valid 16-bit/32-bit WAV, wrong channel count, sample rate mismatch
  - Test APO export: round-trip (import → export → import → compare)
  - Test miniDSP export: sign negation correctness
  - Test JSON export: full config round-trip

### Phase 1 Deliverables
- Fully configurable 4-channel DSP pipeline (any combination of biquad filters, FIR, limiter, gain)
- Dynamic stage add/remove/reorder via REST API
- REW Equalizer APO import/export
- miniDSP biquad coefficient import/export
- FIR filter (up to 256 taps) with WAV and text import
- NVS persistence
- WebSocket DSP metrics broadcast
- All unit tests passing
- Serial debug logging with `[DSP]` prefix

---

## Phase 2: Web UI for DSP Control

### Goal
Full web-based DSP configuration interface with real-time frequency response visualization and drag-and-drop stage management.

### 2.1 — DSP Control Page (web_pages.cpp)
- New page accessible from the main web UI navigation
- Tabbed layout: one tab per channel (L1, R1, L2, R2) + "All" for linked editing
- **Dynamic stage list**:
  - Each stage rendered as a collapsible card showing: type icon, label, enabled toggle, parameters
  - "Add Filter" button at bottom with dropdown: HPF, LPF, PEQ, Low Shelf, High Shelf, Notch, Allpass, Limiter, FIR, Gain
  - Delete button (×) on each stage card
  - Drag handle for reorder (or up/down arrows for simpler implementation)
  - Stage count indicator: "7 / 20 stages" with CPU load badge
- **Per-stage parameter controls** (expand on click):
  - Biquad types: frequency slider/input, gain slider/input (PEQ/shelf), Q slider/input, type dropdown
  - Limiter: threshold, attack, release sliders
  - FIR: tap count display, import button (WAV/text), clear button, enable toggle
  - Gain: dB slider
- **Channel controls**:
  - Channel bypass toggle
  - Global bypass toggle (prominent, always visible)

### 2.2 — Frequency Response Graph
- Canvas-based frequency response visualization (20 Hz – 20 kHz, log scale X, dB linear Y)
- Draws the combined magnitude response of all enabled biquad/gain stages
- FIR response optionally overlaid (computed via DFT of tap coefficients in JS)
- Updates in real-time as parameters are adjusted (no round-trip to ESP32 needed — compute in browser)
- Implementation: compute magnitude response from biquad transfer functions in JavaScript:
  ```
  H(f) = Π [b0 + b1·z⁻¹ + b2·z⁻²] / [1 + a1·z⁻¹ + a2·z⁻²]
  where z = e^(j·2π·f/fs)
  ```
- Overlay individual filter curves (dimmed colors) and combined curve (bright)
- Y-axis: -24 dB to +24 dB (auto-scale if needed)
- Interactive: click on a curve to select that stage for editing

### 2.3 — REW Import/Export UI
- "Import" dropdown button:
  - "From REW (Equalizer APO)" → file picker → uploads, parses, appends stages
  - "From miniDSP (Biquad)" → file picker → uploads, appends as custom coefficients
  - "From FIR (WAV)" → file picker → uploads impulse response as FIR stage
  - "From FIR (Text)" → file picker → uploads text coefficients
  - "From JSON (Backup)" → file picker → restores full config
- "Export" dropdown button:
  - "As REW (Equalizer APO)" → downloads .txt file
  - "As miniDSP (Biquad)" → downloads .txt file
  - "As JSON (Backup)" → downloads .json file
- Channel selector for import target
- Option to "replace all" or "append" on import

### 2.4 — Channel Linking
- Link button options: L1+R1, L2+R2, or All 4
- When linked, adding/removing/modifying a stage on one channel mirrors to all linked channels
- Independent bypass still possible per channel when linked

### 2.5 — Presets
- Save/load named DSP presets (stored in NVS as JSON)
- Built-in presets:
  - "Flat" (all stages cleared)
  - "Subsonic Filter" (HPF @ 25 Hz, Q 0.707)
  - "Speech Enhancement" (HPF @ 80 Hz, PEQ boost 2-4 kHz)
  - "Loudness" (low shelf +6 dB @ 100 Hz, high shelf +3 dB @ 10 kHz)
- User-defined presets (up to 5, stored as JSON blobs in NVS)
- Preset API: `GET/POST/DELETE /api/dsp/presets`

### 2.6 — WebSocket Real-Time Updates
- DSP metrics broadcast every 1s: processing time, CPU load, gain reduction
- Parameter changes from web UI sent via WebSocket for instant response (sub-frame latency)
- Multi-client sync: parameter change on one client broadcasts to all others
- Stage add/remove events broadcast to keep all clients in sync

### 2.7 — Build Asset Pipeline
- After editing `web_pages.cpp`, run `node build_web_assets.js` to regenerate `web_pages_gz.cpp`
- All new JS/CSS for frequency response graph and dynamic stage UI kept inline

### Phase 2 Deliverables
- Full web UI for DSP control with dynamic stage management
- Real-time frequency response graph (client-side computed)
- REW import/export via file upload/download
- Channel linking
- DSP presets (built-in + user-defined)
- WebSocket real-time DSP metrics and multi-client sync

---

## Phase 3: I2S DAC Output

### Goal
Add I2S transmit (TX) path to output processed audio to an external DAC for active speaker management.

### 3.1 — Hardware Requirements
- **DAC**: PCM5102A (I2S input, 32-bit, up to 384 kHz, ~$3)
  - Or ES9023 / ES9038Q2M for higher quality
  - Stereo output per DAC chip; need 2 DACs for 4 output channels
- **I2S TX pins**: Need to assign 3 GPIO pins (BCK_OUT, WS_OUT, DOUT) per DAC
  - Option A: Reuse I2S0 in full-duplex mode (simultaneous RX + TX on same port)
  - Option B: Use a separate I2S peripheral if available
- **GPIO allocation**: ESP32-S3 has 2 I2S peripherals. Currently both are used for ADC input.
  - **Proposed change**: Use I2S0 in full-duplex (RX from ADC1 + TX to DAC1), use I2S1 for ADC2 input only. TX for channels 3-4 would need TDM mode or external I2S multiplexer.
  - Alternative: Use ESP32-S3's I2S TDM mode (up to 16 channels on one port)

### 3.2 — I2S TX Driver
- Configure I2S0 TX with matching sample rate and format to RX
- Double-buffered DMA output: fill TX buffer from DSP output while previous buffer plays
- Latency target: 2 × 256 frames = 512 frames = ~10.7 ms at 48 kHz (input-to-output)
- Create `i2s_audio_output_init()` and `i2s_audio_write_output()` functions

### 3.3 — Output Routing Matrix
- Configurable routing: any input channel can feed any output channel
- Matrix: `outputRouting[4] = {INPUT_L1, INPUT_R1, INPUT_L2, INPUT_R2}` (default 1:1)
- Allows mono-summing, channel swapping, or creative routing
- Controlled via API: `PUT /api/dsp/routing`

### 3.4 — Crossover Integration (Future Extension)
- Crossover filters (Linkwitz-Riley 4th order = 2 cascaded Butterworth biquads)
- Already supported by the dynamic stage chain — user adds LPF + HPF stages with matching Linkwitz-Riley Q values
- For true multi-way crossover: each input splits to multiple outputs, each with its own filter chain
- This requires more output channels than inputs — may need additional DAC hardware or TDM

### 3.5 — Pin Assignment
- Proposed I2S TX pins (verify no conflicts with existing assignments):
  - DAC1: BCK_OUT=39, WS_OUT=40, DOUT=41 (or other available GPIOs)
  - DAC2: BCK_OUT=42, WS_OUT=45, DOUT=46 (if 4-channel output needed)
- **Must avoid GPIO 19/20** (USB pins)
- Final pin assignment requires hardware review of the ALX Nova PCB

### Phase 3 Deliverables
- I2S TX driver for PCM5102A DAC output
- Processed audio output with matched sample rate
- Output routing matrix
- ~10.7 ms input-to-output latency
- Hardware design guidance for DAC connection

---

## Phase 4: MQTT + TFT Screen DSP Control

### Goal
Extend DSP control to MQTT (Home Assistant) and the TFT display with rotary encoder.

### 4.1 — MQTT Integration
- **Discovery**: Publish Home Assistant MQTT discovery messages for DSP entities:
  - `number` entities for key parameters (e.g., per-stage freq/gain/Q)
  - `switch` entities for stage enable, channel bypass, global bypass
  - `select` entity for preset selection
  - `sensor` entities for DSP CPU load and limiter gain reduction
- **Command topics**: `alx_nova/dsp/channel/{n}/stage/{n}/set` with JSON payload
- **State topics**: `alx_nova/dsp/channel/{n}/state` — publish full channel config on change
- **Metrics topic**: `alx_nova/dsp/metrics` — processing time, limiter GR
- Follows existing MQTT handler pattern in `src/mqtt_handler.cpp`

### 4.2 — TFT Screen (LVGL)
- New LVGL screen: **DSP Overview**
  - Shows enabled stage count per channel as compact badges
  - Shows limiter gain reduction as bar meters
  - Shows DSP CPU load percentage
  - Global/per-channel bypass indicators
- New LVGL screen: **DSP Edit** (accessed from DSP Overview)
  - Rotary encoder navigates: channel → stage → parameter
  - Parameters adjusted by rotating encoder, confirmed by click
  - Simplified view: shows one stage at a time with type + key parameters
  - Long-press to add/remove stages
- Navigation: DSP Overview accessible from the desktop carousel
- All screens guarded by `#ifdef GUI_ENABLED`

### Phase 4 Deliverables
- Full MQTT control with Home Assistant auto-discovery
- TFT DSP overview screen with real-time metrics
- TFT DSP edit screen with rotary encoder control
- Bidirectional sync: changes from any interface reflected on all others

---

## Phase 5: Advanced Features & Optimization

### 5.1 — Assembly-Optimized ESP-DSP
- Replace ANSI C biquad with assembly-optimized `_aes3` variant
- Requires linking against the pre-compiled ESP-DSP library in the Arduino SDK
- Expected improvement: ~24 cycles/sample → ~17 cycles/sample per biquad (~30% faster)
- Only pursue if CPU budget becomes tight (unlikely for most configurations)

### 5.2 — Overlap-Save FFT Convolution for Long FIR
- For FIR filters > 256 taps, direct convolution becomes expensive
- Implement overlap-save method using ESP-DSP's FFT:
  - FFT the input block and the IR (pre-computed, stored in frequency domain)
  - Complex multiply in frequency domain
  - IFFT back to time domain
  - Overlap-save bookkeeping for block boundaries
- Enables 512-2048 tap FIR within CPU budget
- Increase `DSP_MAX_FIR_TAPS` to 1024 or 2048 with heap-allocated storage

### 5.3 — REW Bridge Application
- Python script (`tools/rew_bridge.py`) that:
  - Connects to REW's REST API (localhost:4735)
  - Polls for filter changes every 500ms
  - Converts to ESP32 JSON format
  - Publishes via MQTT or WebSocket to the ESP32
- Also supports live measurement → auto-correction workflow:
  1. REW measures room response
  2. REW generates correction filters
  3. Bridge pushes filters to ESP32 in real-time
  4. REW re-measures to verify correction
- Packaged as a standalone tool in the `tools/` directory

### 5.4 — Auto-EQ Integration
- Import AutoEQ profiles (headphone/speaker correction curves)
- Parse AutoEQ's ParametricEQ.txt format (compatible with Equalizer APO)
- Community profile library accessible from web UI

### 5.5 — Dedicated DSP Chip Evaluation
If the ESP32-S3 CPU becomes the bottleneck (unlikely for the current scope):

| Chip | Cost | Channels | FIR Taps | Interface | Notes |
|---|---|---|---|---|---|
| ADAU1452 | ~$12 | 24×24 | 2048/ch | SPI/I2C | SigmaStudio GUI, 294MHz DSP |
| ADAU1701 | ~$5 | 4×4 | 512/ch | I2C | Older, still very capable |
| TAS3251 | ~$8 | 2 | 192/ch | I2C | Integrated Class-D amp |

**Recommendation**: The ESP32-S3 handles the defined scope (20 dynamic stages + 256-tap FIR + limiter × 4 channels) at ~60% CPU worst case. A dedicated DSP chip is not needed unless requirements grow significantly (e.g., 2048-tap FIR for all channels, or real-time convolution reverb).

---

## Implementation Priority & Dependencies

```
Phase 1: Core DSP Engine + FIR + REW Import/Export
    │
    ├──► Phase 2: Web UI (can start immediately after Phase 1)
    │
    ├──► Phase 3: I2S DAC Output (requires hardware decisions, can start design during Phase 2)
    │
    ├──► Phase 4: MQTT + TFT (MQTT can start after Phase 1, TFT after Phase 2 for UI patterns)
    │
    └──► Phase 5: Advanced Optimization (low priority, only if needed)
```

| Phase | Description | Dependencies | Complexity |
|---|---|---|---|
| **Phase 1** | Core DSP + FIR + REW parsers + API | None | High (foundational) |
| **Phase 2** | Web UI with dynamic stages + freq graph | Phase 1 | Medium-High |
| **Phase 3** | I2S DAC output | Phase 1, hardware | High (HW + SW) |
| **Phase 4** | MQTT + TFT control | Phase 1 (MQTT), Phase 2 (TFT) | Medium |
| **Phase 5** | Assembly opt, long FIR, REW bridge | All | Low priority |

---

## Testing Strategy

### Unit Tests (native platform, no hardware)

| Test Module | Test Count (est.) | Tests |
|---|---|---|
| `test_dsp` | ~25 | Coefficient computation (all biquad types), biquad processing, FIR processing, limiter, gain, bypass, stage CRUD, max stage limit, double-buffer swap, config JSON round-trip |
| `test_dsp_rew` | ~20 | APO parser (all types, ON/OFF, edge cases), miniDSP parser (sign convention), FIR text parser, WAV parser (16-bit, 32-bit, invalid), APO export round-trip, miniDSP export, JSON export |

### Integration Tests (on hardware)

- Loopback test: inject known signal via signal_generator → DSP → measure output FFT
- Latency measurement: pulse injection → output detection
- CPU load test: progressively add stages, verify metrics accuracy
- Stress test: all 4 channels, 20 stages each, verify no audio glitches
- Long-running stability: 24h continuous operation with active DSP

### Verification with REW

- Generate correction curve in REW from measurement
- Export as Equalizer APO text
- Import into ESP32 DSP via API
- Re-measure: verify correction applied correctly
- This is the ultimate end-to-end validation

---

## Risk Assessment

| Risk | Impact | Mitigation |
|---|---|---|
| ESP-DSP ANSI C integration issues with PlatformIO | Medium | Fall back to standalone biquad implementation (RBJ cookbook, ~100 lines) |
| CPU overload with many stages active on 4 channels | Medium | Hard cap at 20 stages/ch, real-time CPU monitoring, warning at 80%, refuse add at 90% |
| FIR memory consumption (256 taps × 4 channels × double-buffer) | Low | ~16 KB for FIR — well within 320 KB SRAM; longer FIR can use heap |
| I2S full-duplex complications (Phase 3) | High | Can use TDM mode or defer to separate TX-only I2S port |
| NVS space for variable-length DSP config | Low | JSON blobs ~1-3 KB per channel; NVS partition typically 20-24 KB; FIR stored separately |
| Web UI complexity (dynamic stages + freq graph) | Medium | Start with list-based UI, add drag-reorder and graph as enhancements |
| Thread safety between API and audio task | High | Double-buffering with delay line carry-over eliminates locks in audio path |
| Union-based DspStage size dominated by FIR | Low | FIR params are large (2 KB) but only one FIR per channel recommended; can heap-allocate if needed |
