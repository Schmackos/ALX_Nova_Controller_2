# Unified Audio/DSP Mixer Dashboard

## Context

The Audio and DSP WebApp pages have grown into two separate, feature-heavy tabs. The user wants a single cohesive "Mixer" page where inputs/outputs with dBFS levels are visible at a glance, with quick access to all DSP controls (PEQ, Compressor, Limiter, Delays, Crossover, Routing). This replaces both existing tabs with one unified page plus slide-over drawers for detail editing.

**Agreed design decisions from interview:**
- Single page replacing Audio + DSP tabs, with expandable detail drawers
- Dashboard card grid layout with 4 equal channel strips (L1, R1, L2, R2)
- Meters + quick controls (gain, mute) always visible per channel
- Stacked input/output level meters per channel (needs backend: pre-DSP RMS)
- Processing badges (PEQ:5, Comp, Delay:2ms) → clickable → slide-over drawer
- Visualizations (waveform, spectrum) behind a collapsible toggle
- Utilities (signal gen, audio settings, input names, import/export) in a collapsible section
- Freq response graph inside PEQ drawer only
- Responsive: desktop (4-across), tablet (2×2), mobile (stack)
- Drawer: slides from right on desktop/tablet, from bottom on mobile

## Key Finding: Current Levels Are Post-DSP

`i2s_audio.cpp:472-481` — DSP processes the buffer FIRST, then RMS/VU are computed. So current `audioLevels` WebSocket data already represents **output levels** (post-DSP). For the stacked input/output meters, we need to add pre-DSP input RMS computation before `dsp_process_buffer()` at line 475.

---

## Phase 1: Dashboard Skeleton + Tab Replacement + Input Meters

### 1A. Backend — Pre-DSP Input Level Metering

**`src/app_state.h`** — Add to `AdcState`:
```cpp
float inputRms1 = 0.0f;   // Pre-DSP RMS left
float inputRms2 = 0.0f;   // Pre-DSP RMS right
float inputVu1 = 0.0f;    // Pre-DSP VU left (with ballistics)
float inputVu2 = 0.0f;    // Pre-DSP VU right
```

**`src/i2s_audio.cpp`** — Before `dsp_process_buffer()` (between line 470 and 472):
- Compute pre-DSP RMS: `audio_compute_rms(buffer, stereo_frames, 0/1, 2)`
- Apply VU ballistics to input levels (separate static state vars `_inputVuL[NUM_AUDIO_ADCS]`, `_inputVuR[NUM_AUDIO_ADCS]`)
- Store in `_analysis.adc[a].inputRms1/2`, `_analysis.adc[a].inputVu1/2`

**`src/websocket_handler.cpp`** — Add `inputVu1/2` to per-ADC JSON in `sendAudioData()`

**`src/smart_sensing.cpp`** — Copy `inputVu1/2` from analysis to `appState.audioAdc[a]` alongside existing VU copies

### 1B. Frontend — Replace Audio + DSP Tabs

**`src/web_pages.cpp`**:

1. **Sidebar + mobile tabs**: Remove `audio` and `dsp` tab buttons. Add single `mixer` tab.

2. **Remove** `<section id="audio">` and `<section id="dsp">`. Replace with `<section id="mixer">` containing:
   - **Status bar**: DSP enable toggle, bypass toggle, CPU % bar, sample rate
   - **Channel strip grid** (`div.mixer-grid`, 4 children rendered by JS)
   - **Crossover/routing summary** card with Edit buttons
   - **Visualizations** collapsible (waveform + spectrum canvases, toggles)
   - **Utilities** collapsible (signal gen, input names, audio settings, import/export)
   - **Drawer overlay** + **drawer panel** (empty shell, content rendered dynamically)

3. **New CSS**: `.mixer-grid` (CSS grid: 4 cols desktop, 2 tablet, 1 mobile), `.mixer-strip`, `.mixer-meter-*`, `.mixer-badge`, `.mixer-drawer`, `.mixer-drawer-overlay`

4. **Update `switchTab()`**: Remove audio/dsp blocks, add mixer block (subscribe audio, cache DOM refs, init strips)

5. **Update `currentActiveTab` checks**: All existing references to `'audio'` and `'dsp'` → `'mixer'` (in `handleBinaryMessage`, `audioLevels` handler, `dspDrawFreqResponse`, etc.)

### 1C. Channel Strip Rendering

New `mixerRenderStrips()` JS function generates 4 strips with:
- Editable channel name (click to rename via inline input or drawer)
- Dual meter bars: green-yellow-red gradient (input), blue-orange-red (output)
- Peak hold markers (thin white lines)
- dBFS readout
- Gain slider (horizontal range input, maps to first DSP_GAIN stage in channel)
- Mute button (maps to first DSP_MUTE stage)
- Processing badges row (PEQ:N, Comp, Limiter, Delay, Polarity, FIR)

New `mixerUpdateMeters(data)` function called from `audioLevels` handler:
- Maps `data.adc[a].inputVu1/2` → input meter fill bars (channels a*2, a*2+1)
- Maps `data.adc[a].vu1/2` → output meter fill bars
- Updates dBFS readout, clip indicators

New `mixerRenderBadges()` reads `dspState.channels[c].stages[]` to generate clickable badge pills.

### Desktop Wireframe (4 strips side-by-side)
```
┌──────────────────────────────────────────────────────────────────┐
│ DSP [●On] [Bypass] │ CPU: 12.3% [████░░░░░░░] │ 48kHz │ ⚙     │
├────────────┬────────────┬────────────┬───────────────────────────┤
│    L1      │    R1      │    L2      │    R2                     │
│ "Woofer L" │ "Woofer R" │ "Sub"      │ "Sub 2"                  │
│            │            │            │                           │
│ In ▓▓▓▓░░░│ In ▓▓▓░░░░│ In ▓▓░░░░░│ In ░░░░░░░               │
│ Out▓▓▓░░░░│ Out▓▓░░░░░│ Out▓▓▓░░░░│ Out░░░░░░░               │
│ -12.3 dBFS│ -18.4 dBFS│ -24.1 dBFS│ -inf                      │
│            │            │            │                           │
│ ◄━━━●━━━► │ ◄━━━●━━━► │ ◄━━━●━━━► │ ◄━━━●━━━►                │
│  +0.0 dB  │  -3.0 dB  │  +2.0 dB  │   0.0 dB                  │
│            │            │            │                           │
│   [Mute]  │   [Mute]  │   [Mute]  │   [Mute]                  │
│            │            │            │                           │
│ PEQ:5 Comp│ PEQ:3 Lim │ PEQ:2     │ PEQ:0                     │
│ Delay:2ms │            │            │                           │
├────────────┴────────────┴────────────┴───────────────────────────┤
│ Crossover: LR4 @ 2kHz [Edit]  │  Routing: 1:1 Direct [Edit]    │
├─────────────────────────────────────────────────────────────────-┤
│ ▸ Visualizations                                                 │
│ ▸ Utilities                                                      │
└──────────────────────────────────────────────────────────────────┘
```

---

## Phase 2: Slide-Over Drawer System + Migrate Editors

### 2A. Drawer Shell

HTML at end of `<section id="mixer">`:
- `div.mixer-drawer-overlay` — semi-transparent backdrop, click to close
- `div.mixer-drawer` — fixed panel, 480px wide from right (desktop/tablet), bottom sheet 90vh (mobile)
- Header: close button, title (e.g. "PEQ — L1"), channel tabs (L1/R1/L2/R2)
- Body: scrollable content area, dynamically filled

CSS transitions: `transform: translateX(100%)` → `translateX(0)` (desktop), `translateY(100%)` → `translateY(0)` (mobile). Duration: 300ms cubic-bezier.

### 2B. Drawer Types and Content

| Drawer Type | Title | Content | Existing Functions Reused |
|---|---|---|---|
| `peq` | PEQ — {ch} | Freq response canvas + graph layer toggles + L/R link + band strip + band detail + presets | `peqRenderBandStrip()`, `peqRenderBandDetail()`, `dspDrawFreqResponse()`, canvas mouse handlers |
| `stages` | Processing — {ch} | Stage list + add stage menu | `dspRenderStages()`, `dspParamSliders()`, `dspDrawCompressorGraph()` |
| `crossover` | Crossover | Freq/slope/role inputs + apply button | `dspApplyCrossover()` |
| `routing` | Routing | Preset buttons + matrix grid | `dspLoadRouting()`, `dspRenderRouting()` |

**Key reuse strategy**: The drawer body creates DOM elements with the SAME IDs used by existing functions (`peqBandStrip`, `peqBandDetail`, `dspFreqCanvas`, `dspStageList`, `dspRoutingGrid`). Since the old tabs no longer exist, there's no ID conflict. Existing render functions work unchanged — they query by ID.

### 2C. Channel Context

`openMixerDrawer(type, channel, stageIdx)` sets `dspCh = channel` (the global DSP channel variable), so all existing DSP functions automatically operate on the correct channel. Channel tabs in the drawer header allow switching without closing.

### 2D. Canvas Timing

Freq response canvas starts at 0px width during slide animation. Solution: `requestAnimationFrame` + check `getBoundingClientRect().width > 0` before drawing, with a fallback `setTimeout(350ms)`.

---

## Phase 3: Gain & Mute Integration

The gain slider and mute button on each strip map to DSP stages:

- **Gain slider**: Finds the first `DSP_GAIN` (type 14) stage in the channel. If none exists and user moves slider, auto-inserts one via `dspAddStage(14)`. Pending value stored until `dspState` confirms insertion.
- **Mute button**: Finds the first `DSP_MUTE` (type 17) stage. Toggle creates/enables one if missing.
- **Sync from backend**: When `dspHandleState()` fires, update gain slider positions and mute button states from the stage data.

---

## Phase 4: Visualizations + Utilities Collapsibles

### 4A. Visualizations (collapsed by default)
Move from old Audio tab into `#mixerVisContent`:
- Waveform canvases (dual ADC, existing IDs)
- Spectrum canvases (dual ADC, existing IDs)
- Enable toggles, auto-scale, LED mode, FFT window selector, SNR/SFDR readouts
- Canvas redraw triggered after expand animation completes

### 4B. Utilities (collapsed by default)
Move from old Audio + DSP tabs into `#mixerUtilContent`:
- Input names editor (4 text fields + save)
- Audio settings (update rate, sample rate)
- Signal generator card
- Import/export buttons (REW, JSON)

### 4C. Audio Subscription
Mixer tab subscribes to audio streaming whenever active (even when visualizations collapsed) — the channel strip meters need live data.

---

## Phase 5: Responsive Polish

- Desktop (1024px+): 4 strips in a row, drawer 480px from right
- Tablet (768px-1023px): 2×2 grid, drawer 400px from right
- Mobile (<768px): single column stack, drawer as bottom sheet (90vh)
- Touch targets: minimum 44px for buttons and badges
- Escape key closes drawer
- Drawer backdrop click closes drawer

---

## Files Modified

| File | Changes | Phase |
|---|---|---|
| `src/web_pages.cpp` | Remove audio+dsp tabs, add mixer section, drawer system, channel strips, new CSS/JS | 1-5 |
| `src/i2s_audio.cpp` | Pre-DSP input RMS + VU before `dsp_process_buffer()` at line 475 | 1 |
| `src/app_state.h` | `inputRms1/2`, `inputVu1/2` in `AdcState` | 1 |
| `src/websocket_handler.cpp` | `inputVu1/2` in `audioLevels` per-ADC JSON | 1 |
| `src/smart_sensing.cpp` | Copy `inputVu1/2` from analysis to AppState | 1 |
| `src/web_pages_gz.cpp` | Auto-regenerated | All |
| `test/test_i2s_audio/` | Tests for pre-DSP RMS capture | 1 |

**No changes to**: `dsp_pipeline.cpp/h`, `dsp_api.cpp`, `dsp_crossover.cpp/h`, `mqtt_handler.cpp`, `settings_manager.cpp`, `main.cpp`, `config.h`, GUI screens.

---

## Mockup

Interactive HTML mockup: `mockup_mixer.html` (project root)

---

## Verification

1. `pio test -e native` — all tests pass (including new pre-DSP RMS tests)
2. `pio run` — firmware builds
3. On device: single Mixer tab replaces Audio + DSP tabs
4. All 4 channel strips show stacked input/output meters with live dBFS
5. Click PEQ badge → drawer slides in with freq response graph + band editor
6. Click Comp badge → drawer shows compressor transfer graph + sliders
7. Gain slider + mute button work per channel
8. Crossover/routing summary cards → drawers with full editors
9. Visualizations collapsible → waveform/spectrum canvases render correctly
10. Responsive: test on 1440px, 800px, 375px viewports
