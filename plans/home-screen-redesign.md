# Home Screen Redesign: 6-Cell Status Grid Dashboard

## Context
The current Home screen is a 9-item scrollable text menu (Back + 8 `MENU_INFO` rows) built via the reusable `scr_menu_create()`. On the 160x128 TFT this requires scrolling and provides no visual status indicators — just plain text labels and values. The goal is to replace it with a glanceable 6-cell grid dashboard using colored status dots, an audio level bar, and a title bar showing firmware version + update indicator.

## Final Layout (3 rows x 2 columns)
```
┌─────────────────────────────────┐
│  ALX Nova v1.5.3           [↓]  │  title bar (↓ = DOWNLOAD icon, orange when update)
├───────────────┬─────────────────┤
│  ⏻  Amp       │  🔊  Signal     │
│     ON  ●     │  -22 dBFS  ●   │
├───────────────┼─────────────────┤
│  📶  WiFi     │  ☁  MQTT       │
│  Connected ●  │  Connected ●   │
├───────────────┼─────────────────┤
│  ⚙  Mode      │  🔊  Level     │
│  Smart Auto   │  [████░░░░]    │
├───────────────┴─────────────────┤
│            < Back               │
└─────────────────────────────────┘
```

## Pixel Budget (160w x 128h)
| Section | Height |
|---------|--------|
| Title bar | 18px (montserrat_12 + padding) |
| 3 grid rows | 3 x 30px = 90px |
| Row gaps | 2 x 1px = 2px |
| Back button | 18px (16px btn + 2px margin) |
| **Total** | **128px** |

- Cell width: ~77px each (2 columns, 2px gap, 2px side margins)
- Cell internals: header 12px (montserrat_10) + value 14px (montserrat_12) + 4px pad = 30px

## Dot Color Semantics
| Cell | Green (0x4CAF50) | Orange (0xFF9800) | Red (0xF44336) | Gray (0x666666) |
|------|---|---|---|---|
| Amp | ON | — | OFF | — |
| Signal | Detected (>= threshold) | — | — | Below threshold |
| WiFi | Connected | AP Mode | Disconnected | — |
| MQTT | Connected | — | Disconnected | Disabled |
| Update icon | — | Update available | — | Up to date |

## Mode Cell Alternation
When `currentMode == SMART_AUTO` and `timerRemaining > 0`, alternate every 3 seconds:
- Phase 0 (`millis()/3000 % 2 == 0`): show mode name ("Smart Auto")
- Phase 1 (`millis()/3000 % 2 == 1`): show timer countdown ("04:32")
- When timer is not active or mode is not Smart Auto: always show mode name

## Files to Modify

### 1. `src/gui/gui_icons.h` — Add 3 icon aliases
```cpp
#define ICON_DOWNLOAD  LV_SYMBOL_DOWNLOAD
#define ICON_AUDIO     LV_SYMBOL_AUDIO
#define ICON_LEVEL     LV_SYMBOL_VOLUME_MID
```

### 2. `src/gui/screens/scr_home.cpp` — Complete rewrite
Replace the menu-based implementation with a custom LVGL layout.

**Remove**: `#include "scr_menu.h"`, `format_uptime()`, `MenuConfig home_config`

**Add**:
- Layout constants: `TITLE_BAR_H=18`, `ROW_H=30`, `ROW_GAP=1`, `COL_GAP=2`, `CELL_W=77`, `DOT_SIZE=6`
- Static widget handles: `lbl_amp_value`, `dot_amp`, `lbl_sig_value`, `dot_sig`, `lbl_wifi_value`, `dot_wifi`, `lbl_mqtt_value`, `dot_mqtt`, `lbl_mode_value`, `bar_level`, `lbl_update_icon`
- `create_row(parent)` — helper that creates a flex-row container
- `create_cell(parent, icon, title, value_out, dot_out)` — helper that creates a cell card with:
  - Header row: orange icon (montserrat_10) + gray title label (montserrat_10)
  - Value row: white value text (montserrat_12) + optional 6px colored dot
  - Card styling: `COLOR_BG_CARD` bg, 4px radius, 1px `COLOR_BORDER_DARK` border
- `scr_home_create()`:
  - Title bar: "ALX Nova v{VERSION}" (montserrat_12, orange) + DOWNLOAD icon (dim/orange)
  - 3 rows of 2 cells each (Amp+Signal, WiFi+MQTT, Mode+Level)
  - Level cell: custom — uses `lv_bar` (range -96..0, 8px height) instead of value+dot
  - Back button (60x16, bottom-center) following `scr_debug.cpp` pattern (lines 314-328)
  - Invisible back button object added to encoder group
- `scr_home_refresh()`:
  - Update icon: orange when `updateAvailable`, dim otherwise
  - Amp: text "ON"/"OFF", dot green/red
  - Signal: text "%+.0f dBFS", dot green (detected) / gray (not)
  - WiFi: text "Connected"/"AP Mode"/"Disconnected", dot green/orange/red
  - MQTT: text "Connected"/"Disconnected"/"Disabled", dot green/red/gray
  - Mode: alternate mode name / timer using `millis()/3000 % 2`
  - Level bar: `lv_bar_set_value(bar, (int)audioVuCombined, LV_ANIM_ON)`, indicator green/gray based on detection

**Key data sources from AppState**: `amplifierState`, `audioLevel_dBFS`, `audioThreshold_dBFS`, `audioVuCombined`, `currentMode`, `timerRemaining`, `isAPMode`, `mqttEnabled`, `mqttConnected`, `updateAvailable`

### 3. `src/gui/screens/scr_home.h` — No changes
Public API (`scr_home_create()`, `scr_home_refresh()`) stays identical.

### 4. `test/test_gui_home/test_gui_home.cpp` — Update tests

**Remove** (4 tests): `test_home_uptime_format_*` (uptime no longer shown)

**Remove** `format_uptime()` function

**Update** `format_signal()`: remove "Detected"/em-dash text, just `"%+.0f dBFS"` (dot conveys detection now)

**Update** signal tests to match new format:
- `test_home_signal_detected`: expect `"-18 dBFS"` (was `"-18 dBFS Detected"`)
- `test_home_signal_not_detected`: expect `"-55 dBFS"` (was `"-55 dBFS \xE2\x80\x94"`)

**Add** `format_mode_display(mode, timer_remaining, mock_millis, buf, len)` — mode alternation logic

**Add** dot color helpers:
```cpp
enum DotColor { DOT_GREEN, DOT_RED, DOT_ORANGE, DOT_GRAY };
static DotColor get_amp_dot(bool state);
static DotColor get_signal_dot(float level, float threshold);
static DotColor get_wifi_dot(bool connected, bool apMode);
static DotColor get_mqtt_dot(bool enabled, bool connected);
```

**Add tests** (~8 new):
- `test_home_mode_shows_name_when_no_timer` — timerRemaining==0, always shows mode name
- `test_home_mode_shows_name_when_not_smart_auto` — ALWAYS_ON + timer>0, always shows mode name
- `test_home_mode_alternates_shows_mode_phase` — millis phase 0, shows "Smart Auto"
- `test_home_mode_alternates_shows_timer_phase` — millis phase 1, shows "04:32"
- `test_home_amp_dot_colors` — ON=green, OFF=red
- `test_home_signal_dot_colors` — detected=green, below=gray
- `test_home_wifi_dot_colors` — connected=green, AP=orange, disconnected=red
- `test_home_mqtt_dot_colors` — connected=green, disconnected=red, disabled=gray

**Final test count**: 15 - 4 + 8 = **19 tests**

## Verification
1. `pio test -e native -f test_gui_home` — all 19 tests pass
2. `pio run -e esp32-s3-devkitm-1` — firmware builds successfully
3. Visual: flash to device or Wokwi — 6 cells visible, dots colored correctly, level bar moves, mode alternates with timer, download icon reflects update state, encoder click returns to desktop
