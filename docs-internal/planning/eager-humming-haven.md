# Web UI: Mobile Nav + Audio Visualization Improvements

## Context

The web UI mobile navigation bar sits at the top of the screen, which is less thumb-friendly on modern phones. Moving it to the bottom follows the iOS/Android native app convention. Additionally, the Audio tab's waveform and spectrum visualizations lack axis labels, gridlines, and polish — making them hard to interpret. This plan adds proper X/Y axes, visual effects, smooth animation, theme awareness, and an optional LED bar mode.

## Files Modified

| File | Change |
|------|--------|
| `src/web_pages.cpp` | All CSS, HTML, and JS changes (nav bar, visualizations, LED toggle) |
| `src/web_pages_gz.cpp` | Regenerate via `node build_web_assets.js` |

**Reference files (read-only):**
- `src/i2s_audio.cpp:146-148` — 16-band frequency edge array: `[20, 40, 80, 160, 315, 630, 1250, 2500, 5000, 8000, 10000, 12500, 14000, 16000, 18000, 20000, 24000]`

---

## 1. Move Mobile Nav Bar to Bottom

### 1.1 Tab bar CSS (line ~77)

| Property | Before | After |
|----------|--------|-------|
| `top` | `0` | *(remove)* |
| `bottom` | *(none)* | `0` |
| `height` | `calc(--tab-height + --safe-top)` | `calc(--tab-height + --safe-bottom)` |
| `padding-top` | `var(--safe-top)` | *(remove)* |
| `padding-bottom` | *(none)* | `var(--safe-bottom)` |
| `border-bottom` | `1px solid var(--border)` | *(remove)* |
| `border-top` | *(none)* | `1px solid var(--border)` |

### 1.2 Body padding (line ~72-73)

```css
/* Before */
padding-top: calc(var(--tab-height) + var(--safe-top));
padding-bottom: var(--safe-bottom);

/* After */
padding-top: var(--safe-top);
padding-bottom: calc(var(--tab-height) + var(--safe-bottom));
```

### 1.3 Active tab indicator (line ~118)

Move `::after` bar from `bottom: 0` → `top: 0`, flip border-radius from `3px 3px 0 0` → `0 0 3px 3px`.

### 1.4 Status bar (line ~1129-1131)

Change `top: calc(--tab-height + --safe-top)` → `top: var(--safe-top)` (status bar no longer displaced by nav bar above it).

### 1.5 `body.has-status-bar` (line ~1180)

Change `padding-top: calc(--tab-height + --safe-top + 36px)` → `padding-top: calc(--safe-top + 36px)`.

### 1.6 Desktop media query (line ~1585)

Add `padding-bottom: var(--safe-bottom);` to the desktop `body` rule to reset the bottom padding when tab-bar is hidden.

---

## 2. Waveform Visualization — `drawAudioWaveform()` (line ~3932)

### 2.1 Drawing margins

Reserve space for axis labels:
- Left: 36px (Y-axis labels like "+1.0")
- Bottom: 18px (X-axis time labels)
- Top/Right: 4px padding

Plot area: `plotX = 36, plotY = 4, plotW = w - 40, plotH = h - 22`

### 2.2 Night-mode colors

```javascript
const isNight = document.body.classList.contains('night-mode');
const bgColor = isNight ? '#1E1E1E' : '#F5F5F5';
const gridColor = isNight ? '#333333' : '#D0D0D0';
const labelColor = isNight ? '#999999' : '#757575';
```

### 2.3 Y-axis amplitude labels + gridlines

5 horizontal gridlines at amplitude values: +1.0, +0.5, 0, -0.5, -1.0. Labels right-aligned at left margin. 0.5px gridlines in `gridColor`.

### 2.4 X-axis time labels

Calculate time window from sample count (256) and sample rate (read from `audioSampleRateSelect` dropdown, default 48000). Draw ~5 evenly spaced labels: `0ms`, `1.3ms`, `2.7ms`, `4.0ms`, `5.3ms`.

### 2.5 Glow effect

Before drawing waveform stroke: `ctx.shadowColor = 'rgba(255,152,0,0.4)'; ctx.shadowBlur = 8;`. Reset after stroke.

### 2.6 Canvas CSS height

Increase from `140px` → `160px` to accommodate bottom axis labels.

---

## 3. Spectrum Visualization — `drawSpectrumBars()` (line ~3973)

### 3.1 Drawing margins

Same margin approach: left 32px, bottom 18px, top 4px, right 4px.

### 3.2 Y-axis dB labels + gridlines

Draw gridlines at dB values: 0, -12, -24, -36. Convert to linear position: `linearVal = Math.pow(10, db/20)`, then `yPos = plotY + plotH * (1 - linearVal)`. Label each gridline.

### 3.3 X-axis frequency labels

Mirror band edges from `i2s_audio.cpp`: `[20, 40, 80, 160, 315, 630, 1250, 2500, 5000, 8000, 10000, 12500, 14000, 16000, 18000, 20000, 24000]`. Compute geometric center for each band. Label every other band to avoid crowding, using `formatFreq()` helper: `28→28`, `113→113`, `446→446`, `1.8k`, `6.3k`, `11.2k`, `15k`, `19k`.

### 3.4 Peak hold indicators

Script-scope state arrays:
```javascript
let spectrumPeaks = new Float32Array(16);
let spectrumPeakTimes = new Float64Array(16);
```
- Hold duration: 1500ms
- After hold: linear decay at 0.002 per ms
- Render as 2px white horizontal lines across bar width

### 3.5 Rounded bar tops

Helper `drawRoundedBar(ctx, x, y, w, h, radius)` — uses `arcTo()` for top-left and top-right corners, flat bottom.

### 3.6 Canvas CSS height

Increase from `120px` → `140px` to accommodate bottom axis labels.

---

## 4. Smooth Animation (rAF interpolation)

### 4.1 State arrays (script scope)

```javascript
let waveformCurrent = null, waveformTarget = null;
let spectrumCurrent = new Float32Array(16), spectrumTarget = new Float32Array(16);
let currentDominantFreq = 0, targetDominantFreq = 0;
let audioAnimFrameId = null;
const LERP_SPEED = 0.25;
```

### 4.2 WebSocket handlers

Change `drawAudioWaveform(data.w)` → set `waveformTarget`, call `startAudioAnimation()`.
Change `drawSpectrumBars(data.bands, data.freq)` → set `spectrumTarget`/`targetDominantFreq`, call `startAudioAnimation()`.

### 4.3 Animation loop

`audioAnimLoop()` via `requestAnimationFrame`:
- Lerp each value: `current += (target - current) * LERP_SPEED`
- Call `drawAudioWaveform(waveformCurrent)` and `drawSpectrumBars(spectrumCurrent, ...)`
- Stop loop when all values converge (diff < threshold)

### 4.4 Cleanup

Stop animation and reset arrays when leaving audio tab.

---

## 5. Night-Mode Canvas Colors

### 5.1 CSS change (line ~1849)

Change `.audio-canvas-wrap { background: #1A1A1A; }` → `.audio-canvas-wrap { background: var(--bg-card); }` so the container adapts to theme.

### 5.2 Canvas fill

Both draw functions use `isNight` check to pick background, gridline, and label colors (see sections 2.2 and 3.2). The waveform line stays orange (`#FF9800`) in both modes. Spectrum bars keep their orange→red gradient.

---

## 6. LED Bar Mode Toggle

### 6.1 HTML (line ~2153)

Add inline toggle to Frequency Spectrum card title:
```html
<div class="card-title" style="display:flex;align-items:center;justify-content:space-between;">
    Frequency Spectrum
    <div style="display:flex;align-items:center;gap:6px;">
        <span style="font-size:11px;color:var(--text-secondary);">LED</span>
        <label class="switch" style="transform:scale(0.75);">
            <input type="checkbox" id="ledModeToggle" onchange="toggleLedMode()">
            <span class="slider round"></span>
        </label>
    </div>
</div>
```

### 6.2 JS (near audio functions)

```javascript
let ledBarMode = localStorage.getItem('ledBarMode') === 'true';

function toggleLedMode() {
    ledBarMode = document.getElementById('ledModeToggle').checked;
    localStorage.setItem('ledBarMode', ledBarMode.toString());
}
```

Sync checkbox on page load in init section.

### 6.3 LED rendering in `drawSpectrumBars`

When `ledBarMode` is true:
- Split each bar into 4px segments with 1.5px gaps
- Color gradient: green (bottom 60%) → yellow → red (top 40%)
- Unlit segments drawn at 5% opacity
- Reuses `drawRoundedBar()` helper with radius=1

When `ledBarMode` is false: standard smooth bars with rounded tops.

---

## Verification

1. `pio run -e esp32-s3-devkitm-1` — firmware builds
2. `node build_web_assets.js` — gzip assets regenerated
3. Manual (mobile): nav bar at bottom, thumb-reachable, active indicator at top of bar
4. Manual (desktop): sidebar unchanged, no bottom nav visible
5. Manual (audio tab): waveform shows Y-axis (-1 to +1) labels and X-axis time labels with gridlines, orange glow on waveform
6. Manual (audio tab): spectrum shows X-axis freq labels, Y-axis dB labels, peak hold lines, rounded bar tops
7. Manual (LED toggle): switch enables segmented LED bar mode, persists across refresh via localStorage
8. Manual (night mode): toggle dark/light — canvas background, gridlines, labels adapt automatically
9. Manual (animation): bars and waveform animate smoothly between WebSocket updates
