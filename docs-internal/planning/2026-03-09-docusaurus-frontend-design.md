# Docusaurus Frontend Design — ALX Nova Controller 2

**Date**: 2026-03-09
**Status**: Validated via brainstorming session

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Navigation | Top navbar tabs (User Guide / Developer Docs) | Clear audience separation, Docusaurus native pattern |
| Landing page | Hero with feature cards + two CTA buttons | Professional identity, welcoming to both audiences |
| Colour scheme | Orange-on-dark from `design_tokens.h` | Matches firmware web UI and TFT, brand consistency |
| Design token flow | Shared JSON intermediary (Option C) | Single source of truth across TFT, web UI, and docs |
| Search | Local search (`@easyops-cn/docusaurus-search-local`) | No external dependency, sufficient for ~25 pages |
| Developer overview | System architecture Mermaid diagram | Technical big-picture view for developers |

## Site Structure

### Landing Page (`src/pages/index.js`)
- Hero: "ALX Nova" title, tagline "Intelligent Amplifier Control for ESP32-P4"
- Two CTA buttons: "Get Started" → user/getting-started, "Developer Guide" → developer/overview
- 4 feature cards: Smart Sensing, HAL Device Framework, DSP Engine, Web + MQTT Control
- Each card: SVG icon, title, one-line description

### User Guide Sidebar (9 pages)
1. Introduction — what ALX Nova is, key capabilities
2. Getting Started — unbox, power on, AP connect, WiFi setup, web UI
3. Web Interface — tab-by-tab walkthrough
4. Smart Sensing — modes, threshold, auto-off timer
5. WiFi Configuration — STA + AP, multi-network
6. MQTT & Home Assistant — broker setup, auto-discovery, topics
7. OTA Updates — auto-check, manual, binary upload
8. Button Controls — short/long/very-long press, multi-click
9. Troubleshooting — categorised FAQ

### Developer Docs Sidebar (17 pages)
1. Overview — project scope, tech stack, repo structure
2. Architecture — system Mermaid diagram, AppState, events, FreeRTOS tasks
3. Build Setup — PlatformIO, flags, environments
4. API Reference (5 sub-pages): REST Main, HAL, DSP, Pipeline, DAC
5. WebSocket Protocol — commands, broadcasts, binary frames, auth flow
6. HAL Framework (4 sub-pages): Overview, Device Lifecycle, Driver Guide, Drivers
7. Audio Pipeline — 8-lane input, 16x16 matrix, 8-slot sink
8. DSP System — biquad IIR, FIR, double-buffered swap, output DSP
9. Testing — Unity + Playwright + CI gates
10. Contributing — commit conventions, PR workflow

## Design Token Pipeline

```
src/design_tokens.h
        |
        v
tools/extract_tokens.js
        |
        +---> web_src/css/00-tokens.css    (web UI)
        +---> docs-site/src/css/tokens.css (Docusaurus)
        |
        (LVGL reads design_tokens.h directly via #include)
```

## Colour Palette (from design_tokens.h)

| Token | Hex | Usage |
|-------|-----|-------|
| DT_ACCENT | #FF9800 | Primary accent (links, buttons, highlights) |
| DT_ACCENT_LIGHT | #FFB74D | Hover states |
| DT_ACCENT_DARK | #E68900 | Active/pressed states |
| DT_DARK_BG | #121212 | Page background (dark mode) |
| DT_DARK_CARD | #1E1E1E | Card/surface background |
| DT_DARK_BORDER | #333333 | Borders and dividers |
| DT_SUCCESS | #4CAF50 | Success status |
| DT_WARNING | #FFC107 | Warning status |
| DT_ERROR | #F44336 | Error status |
| DT_INFO | #2196F3 | Info status |

## Cross-linking Strategy
- User pages link to relevant developer API sections
- Developer pages link back to user context where helpful
- Admonitions used consistently: tip, warning, info, danger
