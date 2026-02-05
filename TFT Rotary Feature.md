# TFT Screen + Rotary Encoder Implementation Plan

## Overview
Add a 1.8" TFT display (ST7735S, 128x160 landscape), EC11 rotary encoder, and integrate with the existing K0/reset button (GPIO 15) to provide offline device control via a hierarchical menu system matching the web interface.

---

## Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Graphics library | **LVGL v9.x** + **TFT_eSPI** | User preference; SquareLine Studio v1.4.1+ supports v9 |
| Back navigation | **"< Back" list item** at top of each menu | Encoder-only navigation; K0 stays as factory reset |
| K0 button (GPIO 15) | **Same as existing reset button** | Short press wakes screen; long/very-long press = existing reset/reboot behavior |
| Compilation | **`#ifdef GUI_ENABLED`** conditional | Device works without display connected |
| GUI task | **FreeRTOS task on Core 1** | Keeps GUI rendering off the main loop |
| State sync | **Read AppState + call existing handlers for writes** | No duplication of business logic |

---

## Proposed Pin Mapping (ESP32-S3-DevKitM-1)

```
TFT SPI (Hardware SPI2/FSPI):
  TFT_MOSI  = GPIO 11
  TFT_SCLK  = GPIO 12
  TFT_CS    = GPIO 10
  TFT_DC    = GPIO 13
  TFT_RST   = GPIO 14
  TFT_BL    = GPIO 21  (backlight, PWM for dimming/sleep)

EC11 Rotary Encoder:
  ENCODER_A  = GPIO 5
  ENCODER_B  = GPIO 6
  ENCODER_SW = GPIO 7   (encoder push button)

Existing (unchanged):
  LED_PIN           = GPIO 2
  VOLTAGE_SENSE_PIN = GPIO 1
  AMPLIFIER_PIN     = GPIO 4
  RESET_BUTTON_PIN  = GPIO 15  (K0 - also wakes screen)
```

---

## File Structure

```
src/
├── gui/                          # All GUI code (self-contained)
│   ├── gui_config.h              # GUI-specific constants (pins, timeouts, buffer sizes)
│   ├── gui_manager.h             # Public API: gui_init(), gui_task(), gui_wake()
│   ├── gui_manager.cpp           # LVGL init, TFT_eSPI setup, main GUI task loop
│   ├── gui_input.h               # Encoder + button input driver for LVGL
│   ├── gui_input.cpp             # Interrupt-based encoder, LVGL indev registration
│   ├── gui_theme.h               # LVGL theme matching web design system colors
│   ├── gui_theme.cpp             # Custom LVGL theme (orange accent, light/dark)
│   ├── gui_navigation.h          # Menu state machine, screen transitions
│   ├── gui_navigation.cpp        # Navigation stack, back handling, screen switching
│   ├── gui_icons.h               # Icon C arrays (shared visual language)
│   ├── gui_icons.cpp             # Icon data definitions
│   ├── screens/                  # Individual screen modules
│   │   ├── scr_desktop.h/.cpp    # Layer 0: Horizontal card carousel
│   │   ├── scr_menu.h/.cpp       # Layer 1-3: Reusable vertical list menu
│   │   ├── scr_value_edit.h/.cpp # Value editor (toggle, slider, numeric)
│   │   ├── scr_keyboard.h/.cpp   # Virtual keyboard for text input
│   │   ├── scr_control.h/.cpp    # Control dashboard card content
│   │   ├── scr_wifi.h/.cpp       # WiFi dashboard card + sub-menus data
│   │   ├── scr_mqtt.h/.cpp       # MQTT dashboard card + sub-menus data
│   │   ├── scr_settings.h/.cpp   # Settings dashboard card + sub-menus data
│   │   └── scr_debug.h/.cpp      # Debug dashboard card + sub-menus data
│   └── lv_conf.h                 # LVGL configuration (SquareLine Studio compatible)
├── config.h                      # Modified: add GUI pin defines + GUI_ENABLED flag
├── app_state.h                   # Modified: add screenTimeout, guiDarkMode fields
├── main.cpp                      # Modified: conditional gui_init() in setup()
├── settings_manager.cpp          # Modified: save/load screen timeout setting
└── ... (existing files unchanged)
```

---

## Menu Tree Definition

```
DESKTOP (Layer 0 - Horizontal Carousel)
├── [Control]  ← summary: mode + amplifier status + voltage
├── [WiFi]     ← summary: connected SSID + signal strength
├── [MQTT]     ← summary: connected/disconnected + broker
├── [Settings] ← summary: firmware version + night mode
└── [Debug]    ← summary: heap free + CPU usage

CONTROL (Layer 1 → push encoder on Control card)
├── < Back
├── Sensing Mode     → Value Editor (cycle: Always On / Always Off / Smart Auto)
├── Amplifier        → Value Editor (toggle: ON / OFF)
├── Timer Duration   → Value Editor (1-60 min, rotary adjust)
├── Voltage Threshold→ Value Editor (0.1-3.3V, rotary adjust)
└── LED Blinking     → Value Editor (toggle: ON / OFF)

WIFI (Layer 1)
├── < Back
├── Connection Status → Info screen (SSID, IP, signal, MAC) [read-only]
├── Network Config   → Sub-menu
│   ├── < Back
│   ├── Select Network → WiFi scan list → keyboard for password
│   └── Static IP     → Sub-menu (IP, mask, gateway, DNS editors)
└── Access Point     → Sub-menu
    ├── < Back
    ├── Enable AP      → Value Editor (toggle)
    ├── Auto AP        → Value Editor (toggle)
    ├── AP SSID        → Keyboard editor
    └── AP Password    → Keyboard editor

MQTT (Layer 1)
├── < Back
├── Connection Status → Info screen (broker, port, connected) [read-only]
├── Enable MQTT      → Value Editor (toggle)
├── Broker           → Keyboard editor
├── Port             → Value Editor (numeric)
├── Username         → Keyboard editor
├── Password         → Keyboard editor
├── Base Topic       → Keyboard editor
└── HA Discovery     → Value Editor (toggle)

SETTINGS (Layer 1)
├── < Back
├── Screen Timeout   → Value Editor (cycle: 30s / 1min / 5min / 10min)
├── Night Mode       → Value Editor (toggle)
├── Timezone         → Value Editor (list selection)
├── Auto Update      → Value Editor (toggle)
├── SSL Validation   → Value Editor (toggle)
├── Firmware Info    → Info screen (version, update available) [read-only]
├── Reboot           → Confirmation dialog
└── Factory Reset    → Confirmation dialog

DEBUG (Layer 1)
├── < Back
├── System Info      → Info screen (serial, MAC, uptime, reset reason) [read-only]
├── Memory           → Info screen (heap total/free/used, PSRAM) [read-only, live]
├── CPU              → Info screen (core0 %, core1 %, temperature) [read-only, live]
├── Storage          → Info screen (flash, LittleFS used/total) [read-only]
└── WiFi Details     → Info screen (RSSI, channel, BSSID) [read-only]
```

---

## Implementation Phases

### Phase 1: Foundation — LVGL + TFT Driver + Input + Wokwi (The Scaffold)
**Goal**: Screen lights up, LVGL renders, encoder input works. One static test screen. Wokwi simulation operational.

**Files created:**
- `src/gui/lv_conf.h` — LVGL v9 configuration (128x160, color depth 16-bit, memory pool ~32KB, encoder input enabled, SquareLine Studio compatible settings)
- `src/gui/gui_config.h` — Pin defines, buffer sizes, timing constants
- `src/gui/gui_manager.h/.cpp` — `gui_init()`: TFT_eSPI setup, LVGL display driver, buffer allocation, FreeRTOS task creation. `gui_task()`: main loop calling `lv_timer_handler()`
- `src/gui/gui_input.h/.cpp` — EC11 interrupt-based driver, LVGL encoder indev registration, K0 button wake detection
- `wokwi.toml` — PlatformIO-Wokwi integration config pointing to firmware build
- `diagram.json` — Wokwi circuit: ESP32-S3 + TFT display + KY-040 rotary encoder + push button + LED, all wired to match proposed pin mapping

**Files modified:**
- `platformio.ini` — Add LVGL v9 + TFT_eSPI libs, add `-D GUI_ENABLED` build flag, add `User_Setup.h` config for ST7735S
- `src/config.h` — Add `#ifdef GUI_ENABLED` pin definitions for TFT + encoder + GUI task constants

**Verification:**
- Screen displays LVGL "Hello World" label
- Rotating encoder prints direction to serial
- Pushing encoder prints "press" to serial
- Build without `-D GUI_ENABLED` compiles clean (no GUI code included)
- Wokwi simulation shows same "Hello World" screen with working encoder interaction

---

### Phase 2: Theme + Desktop Carousel
**Goal**: Custom LVGL theme matching web design system. Desktop horizontal scroll with 5 placeholder cards.

**Files created:**
- `src/gui/gui_theme.h/.cpp` — Custom LVGL theme: orange accent (#FF9800), light/dark mode, readable fonts for 128x160 (14px body, 18px headers). CSS variable colors mapped to LVGL style properties.
- `src/gui/gui_navigation.h/.cpp` — Navigation state machine: screen stack, push/pop screens, transition animations
- `src/gui/screens/scr_desktop.h/.cpp` — Horizontal carousel with 5 cards (Control, WiFi, MQTT, Settings, Debug). Each card shows category icon + name. Encoder rotates between cards.
- `src/gui/gui_icons.h/.cpp` — Icon set as C arrays (16x16 or 20x20 pixel icons for each category). Same visual language as web SVG icons, simplified for TFT resolution.

**Verification:**
- 5 cards scroll horizontally with encoder rotation
- Cards show category name + icon
- Orange accent color visible, matches web app feel
- Push encoder on a card logs "entering [category]" to serial

---

### Phase 3: Menu System + Control Screen (Prove the Architecture)
**Goal**: Full navigation from Desktop → Control menu → value editing. Real-time state from AppState.

**Files created:**
- `src/gui/screens/scr_menu.h/.cpp` — Reusable vertical scrollable list. Accepts array of menu items (label + icon + callback). Highlights focused item. "< Back" as first item.
- `src/gui/screens/scr_value_edit.h/.cpp` — Value editor screen: displays `< current_value >`, encoder rotates through options, push confirms. Supports types: toggle (ON/OFF), numeric range (with step), cycle list (enum values).
- `src/gui/screens/scr_control.h/.cpp` — Control dashboard card content (live: sensing mode, amplifier state, voltage reading, timer remaining). Control menu items with callbacks that read/write AppState via existing handler functions.

**Files modified:**
- `src/gui/gui_manager.cpp` — Wire up screen timeout logic (backlight off after configurable delay). Wake on any input.
- `src/app_state.h` — Add `screenTimeout` field (default 60000ms) under `#ifdef GUI_ENABLED`
- `src/settings_manager.cpp` — Save/load `screenTimeout` setting
- `src/main.cpp` — Add conditional `gui_init()` call in `setup()`, add screen timeout to web settings API

**Key architecture proof:**
- User changes "Sensing Mode" on TFT → calls `handleSmartSensingUpdate()` internally → AppState updates → WebSocket broadcasts → web app reflects change immediately
- Web app changes amplifier state → AppState dirty flag → GUI task reads state on next frame → TFT display updates

**Verification:**
- Desktop → push → Control menu appears with items
- Select "Sensing Mode" → value editor shows current mode
- Rotate encoder to change mode → push to confirm → AppState updates
- Web app shows the change in real-time
- Change value in web app → TFT display updates within 1 second
- Screen turns off after timeout, wakes on encoder/K0 input

---

### Phase 4: WiFi Screen + Keyboard
**Goal**: WiFi menus with text input capability for SSID/password entry.

**Files created:**
- `src/gui/screens/scr_wifi.h/.cpp` — WiFi dashboard (connected SSID, IP, signal bar icon), WiFi menu tree (connection status, network config, access point sub-menus)
- `src/gui/screens/scr_keyboard.h/.cpp` — Virtual keyboard for 128x160: character grid navigated by encoder rotation (left/right moves cursor across characters, wrapping rows). Push selects character. Special keys: Backspace, Shift, Done. Optimized layout for small screen (QWERTY reduced, or T9-style).

**Verification:**
- WiFi connection status shows live SSID + signal
- Navigate to AP settings, toggle AP mode via TFT
- Enter AP SSID via virtual keyboard
- Changes reflect in web app and MQTT

---

### Phase 5: MQTT + Settings Screens
**Goal**: Complete MQTT and Settings menu trees.

**Files created:**
- `src/gui/screens/scr_mqtt.h/.cpp` — MQTT dashboard + full menu tree
- `src/gui/screens/scr_settings.h/.cpp` — Settings dashboard + menu tree including screen timeout selector, night mode toggle, reboot/factory reset confirmation dialogs

**Files modified:**
- `src/gui/gui_theme.cpp` — Night mode toggle switches LVGL theme in real-time

**Verification:**
- All MQTT settings editable via TFT
- Screen timeout changeable from both TFT and web (synced)
- Night mode toggle switches both TFT theme and web theme
- Reboot confirmation dialog works (encoder to select Yes/No, push to confirm)

---

### Phase 6: Debug Screen + Polish
**Goal**: Debug info screen with live stats. Final polish and consistency pass.

**Files created:**
- `src/gui/screens/scr_debug.h/.cpp` — Live system info (heap, CPU, storage, WiFi details). Auto-refreshing display.

**Polish tasks:**
- Smooth scroll animations on menu transitions
- Consistent icon set across all screens
- Memory optimization (ensure <80% heap usage with GUI active)
- Screen transition animations (slide left/right for desktop, slide up/down for menus)
- Edge case handling (WiFi disconnect during editing, MQTT broker change, OTA in progress overlay)

**Verification:**
- All 5 dashboard cards show live summary data
- All menu trees navigable and functional
- Memory usage stable over extended use (no leaks)
- OTA update shows progress on TFT screen
- Full round-trip test: change every setting via TFT, verify on web + MQTT

---

### Phase 7: Testing + Documentation
**Goal**: Unit tests for GUI logic, integration verification.

**Files created:**
- `test/test_gui/test_gui_navigation.cpp` — Test navigation state machine (push/pop, back item)
- `test/test_gui/test_gui_input.cpp` — Test encoder direction/press detection with mocks
- `test/test_mocks/TFT_eSPI.h` — Mock TFT_eSPI for native testing
- `test/test_mocks/lvgl.h` — Minimal LVGL mock for navigation logic tests

**Verification:**
- All existing tests still pass
- New GUI tests pass in native environment
- Build with and without `-D GUI_ENABLED` both succeed

---

## SquareLine Studio Compatibility

To ensure LVGL code can be imported into SquareLine Studio later:
1. **lv_conf.h** placed in `src/gui/` with SquareLine-compatible settings
2. **Screen creation** follows LVGL standard pattern: `lv_obj_t* scr = lv_obj_create(NULL)`
3. **No custom low-level drawing** — use only standard LVGL widgets (label, list, button, roller, keyboard, tabview)
4. **Font usage** via `LV_FONT_MONTSERRAT_14`, `LV_FONT_MONTSERRAT_18` (built-in LVGL fonts)
5. **Theme** applied via `lv_theme_default_init()` with custom palette
6. **Style** definitions use `lv_style_t` structs (exportable)
7. **Screen files** are self-contained (one screen per file, standard naming)

---

## Memory Budget (ESP32-S3, 512KB SRAM)

| Component | Estimated RAM | Notes |
|-----------|--------------|-------|
| LVGL core | ~32KB | LV_MEM_SIZE in lv_conf.h |
| Display buffer | ~10KB | 1/10th of 128x160x2 bytes (single buffer) |
| LVGL widgets | ~15KB | Depends on active screen complexity |
| TFT_eSPI | ~2KB | Driver overhead |
| GUI task stack | 8KB | FreeRTOS task stack |
| Icons | ~5KB | C array icon data |
| **Total GUI** | **~72KB** | Well within ESP32-S3 budget |
| Existing app | ~120KB | WiFi, MQTT, Web, etc. |
| **Grand total** | **~192KB** | ~37% of 512KB SRAM |

---

## Integration Points (Existing Code Modifications Summary)

| File | Change | Guarded by |
|------|--------|------------|
| `platformio.ini` | Add LVGL + TFT_eSPI libs, `-D GUI_ENABLED` flag | Build config |
| `src/config.h` | Add GUI pin defines, task size/priority | `#ifdef GUI_ENABLED` |
| `src/app_state.h` | Add `screenTimeout` field | `#ifdef GUI_ENABLED` |
| `src/main.cpp` | Add `#include "gui/gui_manager.h"` + `gui_init()` in setup() | `#ifdef GUI_ENABLED` |
| `src/settings_manager.cpp` | Save/load screenTimeout | `#ifdef GUI_ENABLED` |
| `src/websocket_handler.cpp` | Broadcast screenTimeout in settings payload | `#ifdef GUI_ENABLED` |

No existing functionality is modified. All GUI additions are additive and guarded.

---

## Wokwi Simulation Environment

A Wokwi simulation will be set up alongside Phase 1 to enable rapid GUI development and debugging without physical hardware. The simulation mirrors the exact pin mapping and components.

### Files Created

```
wokwi.toml              # PlatformIO <-> Wokwi integration config
diagram.json             # Circuit definition (ESP32-S3 + TFT + encoder + button)
```

### wokwi.toml

```toml
[wokwi]
version = 1
firmware = ".pio/build/esp32-s3-devkitm-1/firmware.bin"
elf = ".pio/build/esp32-s3-devkitm-1/firmware.elf"
```

### diagram.json — Circuit Components

| Component | Wokwi Part | Wiring |
|-----------|-----------|--------|
| ESP32-S3 | `board-esp32-s3-devkitm-1` | Main MCU |
| 1.8" TFT | `ili9341` (stand-in for ST7735S*) | MOSI->GPIO11, SCLK->GPIO12, CS->GPIO10, DC->GPIO13, RST->GPIO14, BL->GPIO21 |
| Rotary Encoder | `ky-040` (EC11-compatible) | CLK->GPIO5, DT->GPIO6, SW->GPIO7, VCC->3.3V, GND->GND |
| K0 Push Button | `pushbutton` | Pin->GPIO15, GND->GND (matches existing RESET_BUTTON_PIN) |
| LED | `led` | Anode->GPIO2 (existing LED_PIN) |

> *Note: Wokwi may not have an exact ST7735S component. The ILI9341 or ST7789 uses the same SPI protocol and TFT_eSPI driver pattern. The simulation validates LVGL rendering, input handling, and menu navigation. The only difference is resolution (we configure LVGL for 128x160 regardless). If Wokwi adds ST7735 support, swap the component.*

### What the Simulation Enables

- **Phase 1**: Verify LVGL boots, displays content, encoder generates input events
- **Phase 2**: Iterate on carousel layout, theme colors, card design visually in seconds (no flash cycle)
- **Phase 3-6**: Test full menu navigation, value editing, keyboard input — all without touching hardware
- **Debugging**: Serial Monitor + Wokwi logic analyzer to inspect SPI traffic, encoder interrupts, timing
- **CI potential**: Wokwi CLI can run headless simulations for automated screenshot/smoke testing

### PlatformIO Integration

Add to `platformio.ini` under the ESP32 environment:

```ini
; Wokwi simulation (VS Code extension or CLI)
; Run: pio run -e esp32-s3-devkitm-1 && wokwi-cli simulate
```

### Workflow

1. Write/modify GUI code
2. `pio run` to compile
3. Open Wokwi simulation (VS Code extension: `Wokwi: Start Simulation`)
4. Interact with simulated encoder (click CLK/DT pins) and push button
5. See LVGL rendering on simulated TFT in real-time
6. Iterate without flashing physical hardware

### Phase 1 Addition

Phase 1 now includes creating `wokwi.toml` and `diagram.json` as part of the foundation scaffold. The Wokwi simulation is verified working alongside the "Hello World" screen test — both physical hardware and simulation should show identical results.
