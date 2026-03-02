# Plan: Web Audio API Integration — High-Res Spectrum + Waterfall + THD+N + PCM Export

## Context

User wants to elevate the ALX Nova Controller 2 from a "16-bar spectrum display" to a genuine
audio measurement and visualization platform. Inspired by atomic14's esp32-usb-uac-experiments
3D visualizer. Research confirms three capability gaps:

1. **Spectrum resolution**: ESP32 runs 1024-point FFT internally but transmits only 16 aggregated
   bands. Sending 256 bins unlocks 60× more frequency resolution.
2. **Time-frequency history**: No waterfall exists — can't see how spectrum evolves over time.
3. **Distortion measurement**: Signal generator + high-res FFT enables THD+N readout.
4. **Audio capture**: In-browser playback and WAV recording require a PCM stream alongside FFT.

### User's Choices
- **Goal**: Both enhanced visualizations AND measurement tools
- **Primary data**: ESP32 sends 256-bin FFT magnitude array (new WS binary frame)
- **3D library**: Canvas 2D only (no Three.js — keep page lean)
- **Must-have features**: Waterfall spectrogram, THD+N, in-browser playback + WAV export

---

## Research Summary

### What We Have Now (Current State)

| Visualization | Resolution | Data Source | Format |
|---|---|---|---|
| Waveform | 256 uint8 samples (~5ms @ 48kHz) | ESP32 binary WS (0x01) | 258 bytes |
| Spectrum | **16 logarithmic bands only** | ESP32 binary WS (0x02) | 70 bytes |
| VU Meters | float 0–1 L/R per ADC | ESP32 JSON WS | ~200 bytes |
| RTA (DSP tab) | Same 16 bands, ADC0 only | Reuses 0x02 | — |

**Key constraint**: ESP32 runs a 1024-point FFT internally but squashes it into 16 band
magnitudes before sending. The full FFT resolution exists on chip, just never transmitted.

**Key asset already in firmware**: USB Audio (TinyUSB UAC2) is implemented as a **speaker
device** — host streams audio TO the ESP32. The browser does NOT receive audio from it yet.

### What Web Audio API Provides

The browser's built-in audio graph (`AudioContext` + nodes):

#### AnalyserNode — the core analysis primitive
- FFT size options: 32 up to 32768 points → up to **16384 frequency bins**
- `getByteFrequencyData()` — 0–255 per bin, fast
- `getFloatFrequencyData()` — actual dB values per bin, precise
- `getByteTimeDomainData()` — oscilloscope waveform samples
- `smoothingTimeConstant` (0–1) — built-in exponential frame averaging
- `minDecibels` / `maxDecibels` — configurable dB mapping range

#### Processing Nodes
- `BiquadFilterNode` — LP/HP/peaking/notch (mirrors ESP32 DSP)
- `ConvolverNode` — convolution reverb from IR WAV files
- `DynamicsCompressorNode` — brick-wall compression
- `PannerNode` — 3D spatial audio (x/y/z positioning)
- `AudioWorkletNode` — custom DSP on dedicated audio thread (HTTPS required)
- `OfflineAudioContext` — non-realtime render-to-buffer for export

#### Audio Source Options

| Source | How to get audio in | Firmware change? | Quality |
|---|---|---|---|
| Browser microphone | `getUserMedia({audio:true})` | None | Room audio, not amp |
| WebSocket PCM stream | New WS binary type; ESP32 sends int16 PCM chunks | Minor (new send path) | 48kHz from amp |
| UAC2 capture mode | Add TinyUSB microphone endpoint | Significant | 48kHz stereo, USB |
| Audio file playback | `AudioContext.decodeAudioData(arrayBuffer)` | None | File only |

#### Browser Support
Excellent across Chrome/Firefox/Safari/Edge/mobile. HTTPS required for AudioWorklet.
Autoplay restriction: AudioContext must be resumed after a user gesture.

### What atomic14 Built (Patterns to Learn From)

#### frontend/ — 4-in-1 TypeScript Analyzer
All four analyzers fed from a single `onPcm(Int16Array)` callback:

1. **Oscilloscope** — canvas waveform from raw PCM
2. **Waterfall spectrogram** — custom Radix-2 FFT + Hann window, time scrolls via
   `ctx.drawImage(canvas, 1, 0, ...)` shift trick, scientific colormaps (turbo/viridis/inferno)
3. **LED bar spectrum** — quadratic pseudo-log frequency scale, asymmetric decay
4. **Pitch tuner** — autocorrelation with parabolic interpolation, cent-offset needle gauge

Audio sources: Web Audio microphone OR Web Serial (USB-CDC from ESP32).

#### 3d-visualiser/ — Three.js 3D Waterfall
- `AnalyserNode` with `fftSize=1024`, `smoothingTimeConstant=0.8`
- `BufferGeometry` mesh: Z-axis = frequency bins, X-axis = time history, Y-axis = magnitude
- Time scroll: shift height buffer leftward each frame, insert new FFT column on right edge
- 4 colormaps: turbo / viridis / inferno / jet (polynomial approximations)
- OrbitControls for mouse rotation + zoom
- `setMaxFrequencyHz()`, `setGamma()`, `setIntensity()` adjustable at runtime
- Three.js v0.179, ~500KB gzipped; Vite + TypeScript build

**Their audio pipeline**: browser mic → `getUserMedia` → `AudioContext` → `AnalyserNode`
→ `getByteFrequencyData()` → vertex Y heights in Three.js mesh

### Architectural Decision: Audio Source

```
Option A (Tier 2): ESP32 sends more FFT bins via WebSocket        ← CHOSEN
   ESP32 → 256-bin magnitude array → WS binary → browser renders at full resolution
   + Simple, low bandwidth (~1KB per frame at 20Hz)
   + Works at full post-DSP chain resolution
   - Still not "real audio in browser" (no playback, no recording, no pitch detection)

Option B (Tier 3): WebSocket PCM streaming                        ← Phase 3 add-on
   ESP32 → int16 PCM chunks → WS binary → ScriptProcessorNode ring buffer
   + Full Web Audio API feature set: FFT, filters, recording, playback, tuner
   + 256-bin spectrum is automatic via AnalyserNode
   - Adds ~48–96 kbps bandwidth; ~100ms latency; ring buffer management

Option C (Tier 4): UAC2 capture endpoint                          ← Future
   TinyUSB microphone endpoint → getUserMedia() → AnalyserNode
   + True real-time, zero added latency, standard OS-level device
   - Significant TinyUSB descriptor + driver work; USB bandwidth budget

Option D (Tier 1 only): No firmware changes
   Use existing 16-band data for waterfall/3D/colormaps
   + Zero firmware risk
   - Limited resolution; no new audio analysis capabilities
```

---

## Implementation Plan (3 Phases)

### Phase 1 — 256-Bin FFT Streaming + High-Res Spectrum + Waterfall

#### Firmware Changes (`src/`)

**New WS binary frame type `0x03` — High-Res Spectrum**
```
Byte 0:     0x03 (WS_BIN_SPECTRUM_HIRES)
Byte 1:     ADC index (0 or 1)
Bytes 2-5:  Dominant frequency (float32 LE, Hz)  [keep for compatibility]
Bytes 6-1029: 256 × float32 magnitude bins (LE, 0.0–1.0 normalized)
```
Total: 1030 bytes per frame. At 48kHz with 1024-point FFT, bin spacing = 46.875 Hz.

**File: `src/websocket_handler.cpp`**
- Add constant `WS_BIN_SPECTRUM_HIRES = 0x03` (also in `websocket_handler.h`)
- Add function `broadcastSpectrumHiRes(uint8_t adcIdx)`:
  - Retrieve `i2s_audio_get_spectrum_bins(adcIdx, bins, 256)` — new function
  - Pack `[0x03][adcIdx][dominantFreq f32][256 × f32 bins]` into buffer
  - Call `webSocket.broadcastBIN(buf, 1030)`
- Call `broadcastSpectrumHiRes()` alongside existing `broadcastSpectrum()` when
  `audioSubscribed` is true (or gate behind a separate client flag if bandwidth is concern)

**File: `src/i2s_audio.h` / `src/i2s_audio.cpp`**
- Add `bool i2s_audio_get_spectrum_bins(uint8_t adc, float *out, int count)`:
  - Copy first `count` bins from the existing internal FFT magnitude array
  - Already computed in `i2s_audio_compute_fft()`; just expose the raw bins

#### Frontend Changes (`web_src/js/`)

**File: `web_src/js/02-ws-router.js`**
- In `handleBinaryMessage()`, add case for type `0x03`:
  ```js
  } else if (msgType === 0x03) {
      const adc = data[1];
      const dv = new DataView(event.data);
      targetDominantFreq[adc] = dv.getFloat32(2, true);
      const bins = new Float32Array(event.data, 6, 256);
      spectrumHiResTarget[adc] = bins;   // new state var
      waterfallPush(adc, bins);          // feed waterfall
  }
  ```

**File: `web_src/js/04-shared-audio.js`**
- Add new state vars:
  ```js
  let spectrumHiResCurrent = [new Float32Array(256), new Float32Array(256)];
  let spectrumHiResTarget  = [new Float32Array(256), new Float32Array(256)];
  let waterfallHistory = [[], []];          // rolling array of Float32Array(256) snapshots
  const WATERFALL_HISTORY = 200;            // frames (200 × 50ms = 10s of history)
  let waterfallImageData = [null, null];    // ImageData cache for waterfall canvas
  ```

**File: `web_src/js/09-audio-viz.js`**
- Add `drawSpectrumHiRes(ctx, adcIdx)` — renders 256 bars on existing canvas
  (or upgrade existing `drawSpectrum()` to switch between 16/256 based on data availability)
- Add `drawWaterfall(ctx, adcIdx)` — canvas 2D waterfall:
  - New canvas element `audioWaterfallCanvas{0,1}` (to be added to index.html)
  - Each frame: shift existing ImageData left by 1 column, paint new rightmost column
    using colormap mapped to bin magnitude
  - Frequency axis: logarithmic Y scale (low at bottom, high at top)
  - X axis: time scrolling left (newest = right edge)
  - Colormap: turbo (blue → cyan → yellow → red), encoded as a 256-entry RGB LUT
    added to `06-canvas-helpers.js`

**File: `web_src/index.html`**
- Add `<canvas id="audioWaterfallCanvas0">` and `audioWaterfallCanvas1` in the
  audio section alongside existing waveform/spectrum canvases
- Add "Waterfall" toggle button to show/hide waterfall panels

---

### Phase 2 — THD+N Measurement

No additional firmware changes. Uses 256-bin data from Phase 1.

**File: `web_src/js/27-thd.js`** (new module)

`computeTHD(bins, fundamentalHz, sampleRate=48000, fftSize=1024)`:
1. Locate fundamental bin: `fundBin = round(fundamentalHz / (sampleRate / fftSize))`
2. Collect fundamental power: `P_f = bins[fundBin]²`
3. Sum harmonic powers (2nd–10th): `P_harmonics = Σ bins[n × fundBin]²`
4. Estimate noise floor (median of quiet bins, or average of non-harmonic bins)
5. `THD% = sqrt(P_harmonics) / bins[fundBin] × 100`
6. `THD+N% = sqrt(P_total - P_f) / bins[fundBin] × 100`

UI: Small readout panel overlaid on or below the high-res spectrum:
```
Fund: 1000 Hz  Level: -12.3 dBFS
THD:   0.12%   THD+N: 0.34%
H2: -52 dBc  H3: -58 dBc  H4: -64 dBc
```
Enable automatically when signal generator is active at a known frequency.

---

### Phase 3 — WebSocket PCM Stream + In-Browser Playback + WAV Export

#### Firmware Changes

**New WS binary frame type `0x04` — PCM Stream (opt-in, client-requested)**
```
Byte 0:     0x04 (WS_BIN_PCM)
Byte 1:     ADC index (0 or 1)
Bytes 2-3:  Sequence counter (uint16 LE)
Bytes 4+:   N × int16 samples (LE, signed, 48kHz mono L channel)
```
Frame size: 4 + N×2 bytes. Recommended N=256 (512 bytes per frame, ~20Hz = ~80 kbps).

**Files: `src/websocket_handler.cpp/.h`**
- Add `appState.pcmStreamEnabled` flag (default false, toggled by WS command)
- Add WS command handler: `{ "type": "pcmStream", "enabled": true }`
- Add `broadcastPcmFrame(adcIdx, int16_t *samples, int count)` called from audio task
  (via dirty flag or direct call from `i2s_audio` when `pcmStreamEnabled`)

**Files: `src/i2s_audio.cpp`**
- When `appState.pcmStreamEnabled`: after each DMA read, extract mono L channel,
  downcast float32 → int16, post to a small ring buffer, set dirty flag for WS broadcast

#### Frontend Changes

> **HTTPS Note**: `AudioWorklet.addModule()` requires a secure context (HTTPS).
> The ESP32 HTTP server is plain HTTP. Two options:
> - **ScriptProcessorNode** (deprecated but works over HTTP) for the ring buffer — sufficient for this use case
> - **WAV export** needs no AudioContext at all — accumulate chunks, prepend RIFF header, `URL.createObjectURL(blob)` download
> - **Scheduled playback** via `AudioBufferSourceNode` works over plain HTTP; only the Worklet module loader is HTTPS-gated
>
> Recommended: use `ScriptProcessorNode` as the ring buffer driver (no HTTPS required),
> implement WAV export as pure JS file creation (no AudioContext needed).

**File: `web_src/js/28-pcm-stream.js`** (new module)

```js
// ScriptProcessorNode ring buffer (works over HTTP, deprecated but functional)
// Ring buffer: 4096 samples at 48kHz = ~85ms buffer

let pcmAudioCtx = null;
let pcmWorkletNode = null;
let pcmRecordingChunks = [];
let pcmRecording = false;

function initPcmAudio() { ... }   // Create AudioContext + ScriptProcessorNode
function startPcmStream(adc) { ... }  // Send WS command, start playback
function stopPcmStream() { ... }
function startRecording() { pcmRecording = true; pcmRecordingChunks = []; }
function stopRecordingAndExport() {
    // Concatenate Int16Array chunks, build RIFF WAV header, trigger download
}
```

**File: `web_src/js/02-ws-router.js`**
- Add case `0x04`: extract Int16Array, push to ScriptProcessorNode port + recording buffer

**File: `web_src/index.html`**
- Add "Listen" toggle button and "Record / Export WAV" button in audio section

---

## Critical Files

| File | Phase | Change |
|---|---|---|
| `src/websocket_handler.h` | 1, 3 | Add `WS_BIN_SPECTRUM_HIRES`, `WS_BIN_PCM` constants |
| `src/websocket_handler.cpp` | 1, 3 | Add `broadcastSpectrumHiRes()`, `broadcastPcmFrame()` |
| `src/i2s_audio.h` | 1, 3 | Add `i2s_audio_get_spectrum_bins()`, PCM export hook |
| `src/i2s_audio.cpp` | 1, 3 | Implement above functions |
| `src/app_state.h` | 3 | Add `pcmStreamEnabled` flag |
| `web_src/js/02-ws-router.js` | 1, 3 | Handle 0x03 and 0x04 frame types |
| `web_src/js/04-shared-audio.js` | 1, 3 | New state: hi-res bins, waterfall history |
| `web_src/js/06-canvas-helpers.js` | 1 | Turbo colormap LUT |
| `web_src/js/09-audio-viz.js` | 1 | Hi-res spectrum + waterfall canvas drawing |
| `web_src/js/27-thd.js` | 2 | THD+N computation + readout UI (new file) |
| `web_src/js/28-pcm-stream.js` | 3 | ScriptProcessorNode playback + WAV export (new file) |
| `web_src/index.html` | 1, 3 | Waterfall canvas, Listen/Record buttons |

---

## Implementation Order

1. **Phase 1** — Delivers the most value first. High-res spectrum alone transforms the UX.
   Waterfall is driven from same 0x03 data. No risky PCM streaming yet.
2. **Phase 2** — Piggybacks on Phase 1's data. Pure frontend math, no firmware changes.
3. **Phase 3** — Opt-in PCM stream. Larger firmware change but well-isolated.

---

## Verification

1. `pio run` — firmware builds cleanly after each phase
2. `pio test -e native` — all tests pass (add tests for `i2s_audio_get_spectrum_bins`)
3. Browser: spectrum canvas shows 256 bars (or zoom-able continuous line)
4. Browser: waterfall scrolls smoothly at canvas FPS, log frequency axis renders correctly
5. Phase 2: Enable signal generator at 1kHz → THD readout appears; verify math with known
   values from signal generator (pure sine should show <0.1% THD from DAC)
6. Phase 3: Click "Listen" → hear audio; click "Record" + "Export" → valid WAV file plays back
7. `node tools/build_web_assets.js` — web assets rebuild cleanly after all HTML/JS changes

---

## References

- [Web Audio API — MDN](https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API)
- [atomic14/esp32-usb-uac-experiments](https://github.com/atomic14/esp32-usb-uac-experiments)
- [3D Visualiser](https://github.com/atomic14/esp32-usb-uac-experiments/tree/main/3d-visualiser)
- [Frontend Analyzers](https://github.com/atomic14/esp32-usb-uac-experiments/tree/main/frontend)
