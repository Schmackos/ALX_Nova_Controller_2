# Audio Tab Redesign v2 — Complete Design Document

## Design Decisions (from brainstorm 2026-03-25)

| Decision | Choice | Rationale |
|---|---|---|
| User configuration | Asymmetric — any combo of ADCs/DACs | Carrier platform, not fixed hardware |
| Matrix sizing | Dynamic with device grouping | Scales 2x2 to 16x32, grouped by HAL device |
| Channel strip layout | Hybrid console/stacked | Horizontal console on desktop, vertical cards on mobile |
| DSP access | DSP summary line + expandable drawer | At-a-glance status, click to expand full chain |
| PEQ interaction | Drag-on-graph + table (both synced) | Pro audio standard (REW, MiniDSP pattern) |
| Matrix orientation | Inputs on rows, Outputs on columns (always) | Industry standard, responsive with sticky headers |
| VU meter style | Segmented LED, vertical desktop / horizontal mobile | Pro-audio aesthetic, clip visibility |
| SigGen routing | Regular matrix input (no special routing) | One consistent routing model |
| Hot-plug behavior | Toast notification + auto-update (all sub-views) | Non-blocking awareness |
| Channel numbering | User-assignable labels with sequential auto-defaults | Pro users think in "Sub L" not "OUT 3" |
| Channel rename UX | Inline click-to-edit on strip | Most discoverable, pattern exists in codebase |
| Frequency range | 5 Hz - 20 kHz (infrasonic support) | Platform handles sub-bass/infrasonic |
| FIR/Convolution location | In per-channel DSP drawer (not separate sub-view) | One mental model: all DSP per channel |
| Empty state | Message + link to Devices tab (all sub-views) | Guides users on carrier platform setup |

---

## Architecture

### Single Audio Tab with 4 Sub-Views

```
[Inputs] [Matrix] [Outputs] [Signal Gen]
```

Everything is HAL-driven. No device names, channel counts, or capabilities are hardcoded. The `audioChannelMap` WS broadcast provides the full topology.

### Responsive Breakpoints

| Viewport | Layout | VU Meters | Matrix |
|---|---|---|---|
| Desktop (1024px+) | Horizontal console, strips flow L→R | Vertical segmented LED | Full grid visible |
| Tablet (768px+) | Same, horizontal scroll for overflow | Vertical | Sticky row headers, H-scroll |
| Mobile (480px+) | Vertical stacked cards | Horizontal segmented LED | Sticky row+col headers, H-scroll |

---

## Sub-View: Inputs

Channel strips grouped by HAL device. Device name from `HalDeviceDescriptor.name`, channel count from `audioChannelMap.inputs[]`.

### Per-Channel Controls
- Segmented LED VU meter (vertical desktop / horizontal mobile)
- Gain fader (-72 to +12 dB) — **FIX: wire `setInputGain` to `AudioInputSource.gainLinear`**
- Mute button — existing `setInputMute` works (calls `audio_pipeline_bypass_input`)
- Phase button — **FIX: wire `setInputPhase` to create DSP_POLARITY stage**
- Solo button
- Channel label (user-assignable, inline click-to-edit, persisted to NVS)

### Per-Device Group Controls
- Stereo link toggle (mirrors gain/mute/phase between L/R)
- Device status badge (HAL state: Available/Unavailable/Error)
- Capability-driven: PGA control (`HAL_CAP_PGA_CONTROL`), HPF toggle (`HAL_CAP_HPF_CONTROL`)

### DSP Summary Line (expandable to full DSP drawer)
- Input DSP has **4 channels x 24 stages** (10 PEQ region + 14 chain region)
- Summary: `PEQ: 4/10 bands | Gain: -3dB | FIR: 512 taps`
- Full drawer: see DSP Drawer section below

### Visualization Panel (below channel strips)
Preserved from current implementation:
- **Waveform**: 256-sample canvas per detected ADC, auto-scale toggle
- **Spectrum**: 16-band FFT canvas per detected ADC, LED segmented mode toggle, dominant freq readout, FFT window selector (6 types)
- **VU Meters**: Continuous bars or segmented PPM (user toggle), peak hold, clip indicator, signal detection dot
- All driven by binary WS frames (`0x01` waveform, `0x02` spectrum) and `audioLevels` JSON
- Enable/disable toggles per visualization type (firmware-controlled)
- **NEW: Add SNR/SFDR readout** (firmware tracks in AppState, needs broadcast addition)

### Empty State
"No input devices detected. Connect a mezzanine board or check the Devices tab." [Link to Devices tab]

---

## Sub-View: Matrix

Dynamic NxM grid. Inputs on rows (left), Outputs on columns (top). Both axes labeled "INPUTS" / "OUTPUTS" with device grouping.

### Headers
- Row: `INPUTS` axis label, device group headers with colored accent, channel label + auto-number (e.g., "IN 1 Mic L")
- Column: `OUTPUTS` axis label, device group headers, channel label + auto-number (e.g., "OUT 3 Sub L"), rotated 45 deg
- Column header shows device name in muted text

### Cell Interaction
- Empty ("--"): click to set gain (defaults 0 dB)
- Active: shows dB value, accent-colored background
- Click active cell: inline gain slider popup (-72 to +12 dB)
- Double-click: type exact value
- Long-press/right-click: remove route (set to off)

### Toolbar
- Quick presets: 1:1 Passthrough, Stereo -> All, Clear All
- Save/Load named presets (persisted to LittleFS)
- Reset to saved state

### REST API (existing, unchanged)
- `POST /api/pipeline/matrix/cell` — single cell update
- `POST /api/pipeline/matrix/save` / `load`
- `GET /api/pipeline/matrix`

### Viewport
- Desktop: full grid
- Tablet/Mobile: horizontal scroll, sticky first column (row headers), sticky top row (column headers)

### Empty State
"No audio devices detected. Connect a mezzanine board or check the Devices tab."

---

## Sub-View: Outputs

Same device-grouped layout as Inputs. Channels from `audioChannelMap.outputs[]`.

### Per-Channel Controls
- Segmented LED VU meter (post-matrix, post-DSP)
- Gain fader (-72 to +12 dB) — existing `setOutputGain` works
- Mute button — existing `setOutputMute` works; shows "HW Mute" for `HAL_CAP_MUTE` devices
- Polarity button — existing `setOutputPhase` works
- Solo button
- Delay input (ms) — **FIX: wire `setOutputDelay` to actually insert DSP_DELAY stage**
- Channel label (user-assignable, inline click-to-edit)

### Capability-Driven Extras
- `HAL_CAP_HW_VOLUME`: Hardware volume slider (0-100%) — **FIX: use HAL REST not orphaned WS command**
- `HAL_CAP_DSD`: DSD indicator badge when active
- `HAL_CAP_DPLL`/`HAL_CAP_APLL`: Clock lock icon (from Phase 3)

### DSP Summary Line (expandable to full DSP drawer)
- Output DSP has **8 channels x 12 stages** per mono output
- Output DSP supports: biquad filters, limiter, compressor, gain, polarity, mute
- Output DSP does **NOT** support: FIR, delay, convolution, noise gate, tone ctrl, speaker prot, stereo width, loudness, bass enhance, multiband comp
- Summary: `PEQ: 4 bands | XO: LR4 @ 2.5kHz | Comp: -6dB | Lim: -1dBFS`
- Full drawer: see DSP Drawer section below

### Empty State
"No output devices detected. Connect a mezzanine board or check the Devices tab."

---

## Sub-View: Signal Generator

Preserves all existing functionality from `13-signal-gen.js`. SigGen is a regular input lane in the matrix.

### Controls (existing, keep as-is)
- Enable toggle
- Output mode: Software Injection / PWM Output
- Waveform: Sine, Square, White Noise, Frequency Sweep
- Frequency: 1-22000 Hz (slider)
- Amplitude: -96 to 0 dBFS (slider)
- Channel: Ch1 / Ch2 / Both
- Sweep speed (shown for sweep mode only)
- Preset buttons: 440 Hz, 1 kHz, 100 Hz, 10 kHz, Noise, Sweep

### NEW: THD Measurement Tool
- "Measure THD" button (requires SigGen enabled at a stable frequency)
- Starts measurement via WS `startThdMeasurement` with current siggen frequency
- **FIX: add WS broadcast on measurement completion** (currently poll-only)
- Results display: THD+N %, THD+N dB, fundamental dBFS, harmonic levels (2nd-9th)
- Progress bar during averaging (framesProcessed / framesTarget)

---

## DSP Drawer (expanded from channel strip)

When the DSP summary line is clicked, expands to show the full DSP chain in signal-flow order.

### Input DSP Drawer (per input channel, 24 stages max)

**PEQ Region (stages 0-9, up to 10 bands):**
- Summary: "4/10 bands active"
- Action: opens PEQ overlay (drag-on-graph + table, 5 Hz - 20 kHz)

**Chain Region (stages 10-23, any type):**

| Stage | Summary | Action | Notes |
|---|---|---|---|
| Gain | "-3.0 dB" | Inline slider | |
| Delay | "2.50 ms" | Inline input | Max 100ms (4800 samples @ 48kHz) |
| Polarity | "Inverted" / "Normal" | Inline toggle | |
| Mute | "Muted" / "Active" | Inline toggle | |
| FIR | "512 taps loaded" | Upload overlay (.txt) | Max 256 taps, 2 concurrent slots |
| Convolution | "IR: room.wav (0.3s)" | Upload overlay (.wav) | Max 24576 samples (0.51s @ 48kHz), 2 slots |
| Compressor | "Thresh: -18dB 4:1" | Overlay (threshold/ratio/attack/release/knee/makeup) | **FIX: transmit params** |
| Limiter | "-1 dBFS" | Overlay (threshold/attack/release) | **FIX: transmit params** |
| Noise Gate | "Thresh: -60dB" | Overlay (threshold/attack/hold/release/range) | |
| Linkwitz Transform | "F0:40 Q0:0.7 > Fp:20" | Overlay (F0/Q0/Fp/Qp + response graph) | |
| Speaker Protection | "Thermal + Excursion" | Overlay (power/impedance/tau/excursion/diameter/maxTemp) | |
| Tone Controls | "Bass +3 Mid 0 Treble -2" | Inline 3 sliders | |
| Loudness | "On @ -30dB ref" | Inline toggle + reference | |
| Bass Enhance | "On, 60 Hz" | Inline toggle + freq | |
| Stereo Width | "120%" | Inline slider | Only on linked stereo pairs |
| Decimator | "4x downsample" | Inline select (2x/4x/8x) | |
| Multi-band Comp | "3 bands active" | Overlay (per-band crossover + compressor params) | **FIX: expose per-band params via API** |
| Baffle Step | "Width: 200mm" | Inline input + apply | Uses existing `applyBaffleStep` WS |
| Custom Biquad | "b0/b1/b2/a1/a2" | Overlay (5 coefficient inputs) | Used by miniDSP import |

**Add stage:** "+" button with dropdown grouped as Common / Advanced.
**Drag to reorder** within chain region.

### Output DSP Drawer (per output channel, 12 stages max)

Same pattern but **restricted stage types**: only biquad filters (PEQ/crossover), limiter, compressor, gain, polarity, mute.

**Crossover helper:** accessible from output drawer, uses existing `output_dsp_setup_crossover()` API. Types: LR2/LR4/LR8/LR12/LR16/LR24, Butterworth 1-8, Bessel 4/6/8.

---

## PEQ Overlay

Full-screen overlay, shared between input PEQ (10 bands max) and output PEQ (10 bands max).

### Frequency Response Graph (top half)
- **5 Hz - 20 kHz** log-frequency X axis
- -18 to +18 dB Y axis
- Per-band curves in distinct colors (semi-transparent)
- Combined response as bold line
- **Draggable control points**: circle at (freq, gain) per band
  - Drag horizontally: frequency
  - Drag vertically: gain
  - Scroll/pinch on point: adjust Q
  - Click empty area: add new band at that position
- Optional RTA overlay (spectrum data from existing binary frames)
- DPR-aware canvas rendering (adopt `resizeCanvasIfNeeded()` from `06-canvas-helpers.js`)

### Band Table (bottom half, synced with graph)
- Columns: #, Type (13 types), Freq, Gain, Q/BW, Enabled, Delete
- Selecting a band highlights in both graph and table
- Reuses existing `dspComputeCoeffs()` and `dspBiquadMagDb()`

### Toolbar
- Add Band / Reset All
- Import: REW (.txt APO format) / miniDSP (biquad coefficients)
- Export: REW / miniDSP (uses existing firmware endpoints)
- Copy from channel dropdown
- A/B Compare toggle
- Apply / Cancel

### Header
Shows channel context: "PEQ -- OUT 3 Sub L (ES9038Q2M) -- 4/10 bands"

---

## Preset Systems (two separate systems, both need UI)

### Full DSP Config Presets (32 slots)
- Saves entire 4-channel input DSP config + dspEnabled flag
- Accessible from a "Presets" button in the Audio tab header
- WS: `saveDspPreset`, `loadDspPreset`, `deleteDspPreset`, `renameDspPreset`
- REST: full CRUD at `/api/dsp/presets/*`

### Named PEQ Presets (up to 10, named)
- Saves PEQ bands for a single channel
- Accessible from PEQ overlay toolbar ("Save as..." / "Load preset")
- WS: `savePeqPreset`, `loadPeqPreset`, `deletePeqPreset`, `listPeqPresets`
- REST: CRUD at `/api/dsp/peq/presets*`

---

## Settings Integration (preserve existing)

- Audio update rate (Settings tab, 100/50/33/20ms)
- FFT window type (6 types, in spectrum visualization)
- VU mode toggle (continuous/segmented, localStorage)
- LED bar mode toggle (localStorage)
- Waveform auto-scale (localStorage)
- Graph enable/disable toggles (firmware WS-controlled)
- Settings export/import includes `dsp`, `pipeline`, `signalGenerator`, `inputChannelNames` sections

---

## Firmware Fixes Required (pre-requisites)

| Fix | File | Issue |
|---|---|---|
| Wire `setInputGain` to pipeline | `websocket_command.cpp` | Logged but `gainLinear` never updated |
| Wire `setInputPhase` to DSP polarity | `websocket_command.cpp` | Logged but no stage created |
| Wire `setOutputDelay` to output DSP | `websocket_command.cpp` | Logged but no delay stage inserted |
| Fix compressor/limiter param transmission | `06-peq-overlay.js` | Only `type` POSTed, not params |
| Remove orphaned `setOutputHwVolume` WS | `05-audio-tab.js` | Use HAL REST instead |
| Add THD completion WS broadcast | `thd_measurement.cpp` | Currently poll-only |
| Add SNR/SFDR to audioLevels broadcast | `websocket_broadcast.cpp` | Tracked but not broadcast |
| Expose multi-band comp per-band params | `websocket_command.cpp` | Only `numBands` settable |
| Add WAV IR upload REST endpoint | `dsp_api.cpp` | Parser exists, no endpoint |
| Wire output crossover to WS | `websocket_command.cpp` | REST exists but no WS path |

---

## Files Plan

### KEEP (no changes)
- `01-core.js` — WS core, `apiFetch()`, auth
- `03-app-state.js` — global state, `showToast()`
- `04-shared-audio.js` — shared audio arrays
- `06-canvas-helpers.js` — DPR resize, bg cache, color LUT
- `13-signal-gen.js` — SigGen controls (moves to sub-view)
- `15-hal-devices.js` — HAL device management (separate tab)
- `22-settings.js` — settings including audio rate
- All non-audio JS files

### REPLACE (rebuild in-place)
- `05-audio-tab.js` — full rewrite for new design
- `06-peq-overlay.js` — extend: drag-on-graph, fix comp/limiter, add FIR/convolution/advanced overlays
- `09-audio-viz.js` — adapt rendering functions for new layout

### MODIFY
- `02-ws-router.js` — update audio-specific routes, add THD broadcast handler
- `07-ui-core.js` — update audio tab subscription logic
- `28-init.js` — update init sequence
- `web_src/index.html` — update Audio tab HTML structure
- `web_src/css/03-components.css` — add new component styles
- `web_src/css/04-canvas.css` — update canvas/VU styles

### No files to DELETE yet
The old files (10-input-audio.js, 11-input-overview.js, etc.) were already removed in a prior cleanup. Current file set is already the redesign target.

---

## Implementation Phases (revised)

### Phase 0: Firmware Fixes (pre-requisite)
Fix the 10 firmware bugs/gaps listed above. No UI changes.

### Phase 1: Audio Tab Shell + Channel Discovery
- Sub-view navigation (inputs/matrix/outputs/siggen)
- `audioChannelMap` consumer with dynamic array sizing
- Empty states for all sub-views
- Hot-plug toast notifications

### Phase 2: Input Channel Strips
- Device-grouped strips with VU meters
- Gain/mute/phase/solo controls (wired to fixed firmware)
- User-assignable channel labels (inline edit, NVS persist)
- Stereo link toggle
- Capability-driven extras (PGA, HPF)

### Phase 3: Output Channel Strips
- Device-grouped strips with post-matrix VU
- All output controls (gain, mute, polarity, delay)
- HW volume via HAL REST (not orphaned WS)
- Capability badges (DSD, DPLL, clock lock)

### Phase 4: Routing Matrix
- Dynamic NxM grid with device grouping
- INPUTS/OUTPUTS axis labels
- Gain popup, presets, bass management
- Save/Load, responsive sticky headers

### Phase 5: PEQ Overlay (drag-on-graph)
- Hoist `fToX`/`dbToY`, add inverse functions
- Control point rendering + hit-testing
- Pointer event handlers (mouse + touch)
- DPR canvas fix
- 5 Hz lower bound
- REW import/export (wire to existing firmware parsers)

### Phase 6: DSP Drawer + All Overlays
- DSP summary line on all channel strips
- Expandable drawer with full stage inventory
- Fix compressor/limiter param transmission
- Crossover overlay with response graph
- FIR upload overlay
- Convolution (WAV IR) upload overlay
- All remaining stage type controls

### Phase 7: Presets + THD
- DSP config preset UI (32 slots)
- Named PEQ preset UI (10 named)
- THD measurement tool in SigGen sub-view

### Phase 8: Visualization Migration
- Move waveform/spectrum/VU viz into new Inputs layout
- Preserve all rendering functions and binary WS handlers
- SNR/SFDR readout (after firmware broadcast fix)

### Phase 9: CSS Polish + Responsive
- Console layout desktop, stacked mobile
- Segmented LED VU styling
- Matrix responsive with sticky headers
- Dark/light theme consistency
- Test all breakpoints (480/768/1024px)
