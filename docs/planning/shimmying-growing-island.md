# TFT GUI — Add Missing Web App Features

## Context
The TFT GUI is missing several features available in the Web App. After auditing both interfaces, the user confirmed 8 features to add across 7 implementation phases. Each phase is self-contained and can be tested independently.

## Feature Gap Summary (Confirmed)
1. **Check for Updates + OTA with Progress Bar** — Settings menu + full-screen progress view
2. **Timezone Settings** — Timezone offset cycle + DST toggle + live time display
3. **Change Password** — Multi-step keyboard flow in Settings
4. **Saved WiFi Networks** — View + Remove + Edit in WiFi submenu
5. **Static IP Configuration** — Per-network IP/Mask/Gateway/DNS in WiFi
6. **Debug Log Viewer** — Ring buffer + scrollable log in Debug screen
7. **Audio VU Meter + Level** — New Audio screen with VU bars + sample rate

## Pre-requisite: Increase MENU_MAX_ITEMS
Settings menu currently has 16 items (at the limit). Adding OTA (2), timezone (3), password (1) = 22 items total.
- **File**: `src/gui/screens/scr_menu.h` — change `MENU_MAX_ITEMS` from 16 to 24

---

## Phase 1: OTA Update with Progress Bar

### Files to modify
- `src/gui/gui_navigation.h` — Add `SCR_OTA_PROGRESS` to `ScreenId` enum
- `src/gui/gui_manager.cpp` — Register screen, add refresh dispatch, check `otaStartRequested` flag in gui_task
- `src/gui/screens/scr_settings.cpp` — Add "Check Update" and "Update Now" items after Firmware info (item 13)
- `src/app_state.h` — Add `bool otaCheckRequested = false` and `bool otaStartRequested = false` flags
- `src/main.cpp` — In main loop, check and consume `otaCheckRequested`/`otaStartRequested` flags

### New files
- `src/gui/screens/scr_ota_progress.h` — declares `scr_ota_progress_create()` + `scr_ota_progress_refresh()`
- `src/gui/screens/scr_ota_progress.cpp` — Full-screen OTA progress view

### Implementation
**Settings menu additions** (after item 13 "Firmware"):
- Item 14: "Check Update" (MENU_ACTION) — Sets `appState.otaCheckRequested = true`, shows "Checking..." in value text
- Item 15: "Update Now" (MENU_ACTION) — Only actionable when `updateAvailable`. Confirmation via VE_TOGGLE. On confirm: push `SCR_OTA_PROGRESS`, set `appState.otaStartRequested = true`
- Shift Reboot to item 16, Factory Reset to item 17

**OTA Progress screen** (custom, not menu-based):
- LVGL widgets: title label, status label, `lv_bar` progress bar (140x12px), bytes label, countdown label
- Refresh reads: `otaProgress` (0-100), `otaStatusMessage`, `otaProgressBytes`, `otaTotalBytes`, `otaStatus`
- On `otaStatus == "complete"`: show "Rebooting in 3..." countdown, then ESP.restart()
- On error: show error message, enable back navigation
- Back button disabled while `otaInProgress` is true

**Cross-core trigger**: GUI (Core 1) sets `otaStartRequested`, main loop (Core 0) checks it and calls `performOTAUpdate(cachedFirmwareUrl)`. Same pattern for `otaCheckRequested` → `checkForFirmwareUpdate()`.

### Existing APIs to reuse
- `checkForFirmwareUpdate()` — `src/ota_updater.h:7`
- `performOTAUpdate(String url)` — `src/ota_updater.h:8`
- `appState.otaInProgress/otaProgress/otaStatus/otaStatusMessage/otaProgressBytes/otaTotalBytes` — `src/app_state.h:80-85`
- `appState.updateAvailable`, `appState.cachedFirmwareUrl` — `src/app_state.h:87,92`

---

## Phase 2: Timezone Settings

### Files to modify
- `src/gui/screens/scr_settings.cpp` — Add 3 items: Timezone, DST, Time

### Implementation
Add after Dark Mode (item 7), shifting subsequent items:
- Item 8: "Timezone" (MENU_ACTION) — VE_CYCLE with 25 options (UTC-12 to UTC+12). Confirm sets `appState.timezoneOffset` (in seconds), calls `saveSettings()` + `syncTimeWithNTP()`
- Item 9: "DST" (MENU_ACTION) — VE_TOGGLE. Confirm sets `appState.dstOffset = val ? 3600 : 0`, calls `saveSettings()` + `syncTimeWithNTP()`
- Item 10: "Time" (MENU_INFO) — Read-only, refreshed every second via `getLocalTime()` → `HH:MM:SS` format

### Existing APIs to reuse
- `appState.timezoneOffset` (int, seconds) — `src/app_state.h:89`
- `appState.dstOffset` (int, seconds) — `src/app_state.h:90`
- `syncTimeWithNTP()` — `src/ota_updater.h:16`

---

## Phase 3: Change Password

### Files to modify
- `src/gui/screens/scr_settings.cpp` — Add "Password" item + multi-step keyboard flow

### Implementation
Add after SSL Validation (now item ~15 after timezone items shift things):
- Item: "Password" (MENU_ACTION) — Value shows "Default" or "Custom"
- 3-step keyboard flow using static state machine:
  1. "Current Password" (password_mode=true) → validate against `getWebPassword()`
  2. "New Password" (password_mode=true) → store temporarily, validate length >= 8
  3. "Confirm Password" (password_mode=true) → compare with step 2, call `setWebPassword()`
- Each step opens `scr_keyboard_open()` with callback that advances to next step

### Existing APIs to reuse
- `getWebPassword()` — `src/auth_handler.h:34`
- `setWebPassword(String)` — `src/auth_handler.h:35`
- `isDefaultPassword()` — `src/auth_handler.h:36`
- `scr_keyboard_open()` — `src/gui/screens/scr_keyboard.h`

---

## Phase 4: Saved WiFi Networks Management

### Files to modify
- `src/gui/screens/scr_wifi.cpp` — Add "Saved Networks" submenu to Network Config menu

### Implementation
**Saved Networks list** — Dynamic screen via `gui_nav_register(SCR_INFO, ...)`:
- Back item + up to 5 saved network items (MAX_WIFI_NETWORKS = 5)
- Each item shows SSID + "DHCP"/"Static" indicator
- Click opens per-network action menu

**Per-network action menu** — Another dynamic `SCR_INFO` screen:
- "Connect" — calls `connectToWiFi(config)`
- "Edit Password" — opens keyboard, saves via `saveWiFiNetwork(config)`
- "Remove" — VE_TOGGLE confirmation, then `removeWiFiNetwork(index)` + pop back

### Existing APIs to reuse
- `getWiFiNetworkCount()` — `src/wifi_manager.h:76`
- `readNetworkFromPrefs(int, WiFiNetworkConfig&)` — `src/wifi_manager.h:77`
- `removeWiFiNetwork(int)` — `src/wifi_manager.h:75`
- `saveWiFiNetwork(WiFiNetworkConfig&)` — `src/wifi_manager.h:70`
- `connectToWiFi(WiFiNetworkConfig&)` — `src/wifi_manager.h:50`

---

## Phase 5: Static IP Configuration

### Files to modify
- `src/gui/screens/scr_wifi.cpp` — Extend per-network action menu from Phase 4

### Implementation
Add to per-network action menu (Phase 4):
- "Static IP" (MENU_ACTION) — VE_TOGGLE, toggles `config.useStaticIP`
- If static IP enabled, show additional items:
  - "IP Address" — keyboard input, validates IP format
  - "Subnet Mask" — keyboard input
  - "Gateway" — keyboard input
  - "DNS 1" — keyboard input
  - "DNS 2" — keyboard input (optional)
- Menu is dynamically built: when `useStaticIP=false` → only toggle shown; when `true` → all 6 fields shown
- On any change: `saveWiFiNetwork(config)`

### Existing fields
- `WiFiNetworkConfig.useStaticIP`, `.staticIP`, `.subnet`, `.gateway`, `.dns1`, `.dns2` — `src/wifi_manager.h:15-23`

---

## Phase 6: Debug Log Viewer

### Files to modify
- `src/debug_serial.h` — Add ring buffer storage + `getLogLines()` public method
- `src/debug_serial.cpp` — Implement ring buffer fill in `broadcastLine()` + `getLogLines()`
- `src/gui/screens/scr_debug.cpp` — Add "Console" section with scrollable log label

### Implementation
**Ring buffer** (in DebugSerial class):
- `char _logRing[20][64]` — 20 lines, 64 chars each (1280 bytes)
- `int _logHead`, `_logCount` — circular buffer pointers
- `SemaphoreHandle_t _logMutex` — FreeRTOS mutex (both cores write logs, GUI reads)
- `addToRing()` called from `broadcastLine()` — truncates line to 63 chars, stores in ring
- `getLogLines(char* buf, int bufSize)` — public method, copies ring contents oldest-first

**Debug screen addition**:
- Add `lbl_log` label after the "System" section
- In `scr_debug_refresh()`: call `DebugOut.getLogLines()`, set label text
- Font: smallest available (LV_FONT_MONTSERRAT_10 or similar) to fit more lines

### Memory impact
~1280 bytes for ring buffer + mutex overhead. Acceptable.

---

## Phase 7: Audio VU Meter Screen

### New files
- `src/gui/screens/scr_audio.h` — declares `scr_audio_create()` + `scr_audio_refresh()`
- `src/gui/screens/scr_audio.cpp` — Custom screen with VU bars

### Files to modify
- `src/gui/gui_navigation.h` — Add `SCR_AUDIO_MENU` to ScreenId enum
- `src/gui/gui_manager.cpp` — Register screen, add refresh dispatch
- `src/gui/screens/scr_desktop.cpp` — Add Audio card to carousel (8 cards total)

### Implementation
**Layout (160x128):**
```
  Audio                    [title]
  L ████████░░░░░  -12 dB  [VU bar L + dBFS]
  R ████████░░░░░  -14 dB  [VU bar R + dBFS]
  Signal: Detected         [detection status]
  Freq: 440 Hz             [dominant frequency]
  Rate: 44100 Hz     [>]   [sample rate, editable]
  [< Back]
```

**VU bars**: `lv_bar` widgets, range 0-100, map `audioVuLeft/Right` (0.0-1.0) to 0-100. Color: orange accent (theme primary).

**Sample rate**: MENU_ACTION-style clickable item opening VE_CYCLE with options: 16000, 44100, 48000 Hz.

**Desktop card**: Add at index 2 (after Control), shift existing cards. Shows: "VU: -12dB / Signal / 440 Hz".

**Refresh**: Called every 1s, reads `audioVuLeft`, `audioVuRight`, `audioLevel_dBFS`, `audioDominantFreq`, `audioSampleRate` from AppState.

### Existing APIs to reuse
- `appState.audioVuLeft/Right` — `src/app_state.h:115-116`
- `appState.audioPeakLeft/Right` — `src/app_state.h:118-119`
- `appState.audioDominantFreq` — `src/app_state.h:121`
- `appState.audioSampleRate` — `src/app_state.h:123`
- `appState.audioSpectrumBands[16]` — `src/app_state.h:122`

---

## Implementation Order
1. Pre-req: Bump `MENU_MAX_ITEMS` to 24
2. Phase 1: OTA Progress (most requested, self-contained)
3. Phase 2: Timezone (small, validates settings menu expansion)
4. Phase 3: Change Password (small, tests keyboard flow)
5. Phase 4: Saved WiFi Networks (medium, extends WiFi submenu)
6. Phase 5: Static IP (extends Phase 4)
7. Phase 6: Debug Log Viewer (independent, medium)
8. Phase 7: Audio VU Meter (largest, new screen + desktop card)

## Verification
After each phase:
- `pio run -e esp32-s3-devkitm-1` — build succeeds
- `pio test -e native` — all existing tests pass
- Manual test on device or Wokwi: navigate to new items, verify functionality
- For OTA: test check update + progress bar flow
- For Audio: verify VU bars update with audio input
- Final: verify desktop carousel shows all 8 cards correctly
