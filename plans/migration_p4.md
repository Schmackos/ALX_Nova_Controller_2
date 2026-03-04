# ESP32-S3 → ESP32-P4 Migration Plan (Waveshare ESP32-P4-WiFi6-DEV-Kit)

## Context

The ALX Nova Controller runs on an ESP32-S3 (512KB SRAM, 8MB PSRAM, 240MHz Xtensa). Internal SRAM pressure is the primary pain point — MbedTLS needs ~33KB contiguous for OTA TLS, WiFi RX buffers need ~40KB headroom, and the DSP/audio pipeline competes for DMA-capable internal memory. Despite aggressive heap recovery (DMA 16→6, GUI 16→10, OTA 12→8, WS 15→4KB), we're at the edge.

The ESP32-P4 solves this structurally: **768KB internal SRAM** (+50%), **400MHz dual-core RISC-V** (+67% clock), **32MB PSRAM** (4x), **3× I2S** (no more dual-master hack), and **USB 2.0 High-Speed** (40x faster). The Waveshare board also adds Ethernet, an ES8311 audio codec, and WiFi 6 via ESP32-C6 co-processor.

### User Requirements
1. Keep ST7735S 128x160 SPI TFT (MIPI DSI upgrade deferred)
2. Dual-stack networking: Ethernet + WiFi with automatic failover
3. Add ES8311 as extra DAC output alongside PCM5102 (headphone/speaker via NS4150B amp). No mic input
4. LP core always-on audio sensing
5. USB Audio HS multi-channel (24-bit/96kHz+)
6. More processing power → higher TFT FPS, more headroom

### Recent Architecture Changes (Post-Plan, Pre-Migration)

The codebase has been significantly refactored since the original plan. These changes affect migration strategy:

| Change | Files | Migration Impact |
|---|---|---|
| **Event-driven main loop** | `src/app_events.h/.cpp` | FreeRTOS event groups are standard — **fully portable** to P4. `app_events_wait(5)` replaces `delay(5)`. 11 event bits (EVT_OTA through EVT_ADC_ENABLED). Native tests stub all 3 functions as no-ops via `#define` |
| **Dedicated MQTT task** | `src/mqtt_task.h/.cpp` | Runs on Core 0 (4KB stack, priority 1, 50ms poll). On P4: WiFi runs on ESP32-C6 via SDIO — Core 0 no longer hosts WiFi stack. **Reassess core affinity**: MQTT task may move to Core 1 or stay on Core 0 with GUI |
| **Dead dirty flags removed** | `src/app_state.h` | 18→11 flags. Removed: `_fsmStateDirty`, `_amplifierDirty`, `_sensingModeDirty`, `_timerDirty`, `_audioDirty`, `_dspMetricsDirty`, `_dspPresetDirty`, `clearAllDirtyFlags()`, `hasAnyDirtyFlag()`. Cleaner state machine — less code to migrate |
| **Deferred save pattern** | settings_manager, smart_sensing, signal_gen, dac_hal | 2s debounce via `millis()` — **fully portable**. 4 `checkDeferredXxxSave()` calls in main loop |
| **Audio pipeline hardening** | `src/audio_pipeline.cpp`, `platformio.ini` | 12 DMA buffers (was unspecified), WiFi power-save disabled, I2S DMA ISR pinned to Core 1, GUI moved to Core 0. On P4: ISR pinning still valid (3 I2S peripherals). WiFi PS irrelevant (separate C6 chip) |
| **Emergency Limiter removed** | `src/audio_pipeline.cpp` | Global brick-wall limiter + audio quality diagnostics fully deleted. Per-channel DSP limiter remains. **Fewer DSP stages to port** |
| **hwStatsJustSent gate** | `src/main.cpp` | Prevents WiFi TX burst stacking. On P4 with Ethernet primary: less critical but still good practice |
| **SmartSensing 1s polling** | `src/main.cpp` | Not event-driven — uses `millis()` polling. **Fully portable** |

---

## What We Gain

| Resource | ESP32-S3 | ESP32-P4 | Δ |
|---|---|---|---|
| Internal SRAM | 512 KB | 768 KB + 8 KB TCM | +50% — TLS 33KB is trivial |
| CPU | 240 MHz Xtensa ×2 | 400 MHz RISC-V ×2 | +67% clock |
| LP Core | None | 40 MHz RISC-V (32KB) | Always-on sensing |
| PSRAM | 8 MB OPI | 32 MB OPI | 4× |
| I2S | 2 peripherals | 3 HP + 1 LP | No dual-master hack |
| USB | Full-Speed 12Mbps | **High-Speed 480Mbps** | 40× bandwidth |
| Ethernet | None | 100M onboard | Wired fallback |
| Audio codec | External only | **ES8311 onboard** (DAC+amp) | Extra output |
| GPIO | 45 | 55 | +10 pins |

---

## Key Risks

| Risk | Severity | Mitigation |
|---|---|---|
| WiFi via SDIO to ESP32-C6 has active init bugs ([#11404](https://github.com/espressif/arduino-esp32/issues/11404), [esp-hosted #66](https://github.com/espressif/esp-hosted-mcu/issues/66)) | **HIGH** | Dual-stack: Ethernet primary, WiFi backup. Test SDIO early in Phase 0 |
| Arduino framework beta on P4 | MEDIUM | pioarduino fork used (same as now). ESP-IDF underneath is stable (5.5.x) |
| ESP-DSP pre-built P4 lib may not be in pioarduino SDK | MEDIUM | Fallback: `lib/esp_dsp_lite/` ANSI C (already in repo). P4 400MHz compensates for no SIMD |
| TinyUSB HS HAL differs from S3 | HIGH | Prototype USB in isolation first (Phase 5) |
| LovyanGFX MIPI DSI broken on P4 | LOW | Keeping SPI TFT — non-MIPI path works |
| LP core requires IDF component, not Arduino API | MEDIUM | Defer to Phase 7. HP-side sensing works as fallback |

---

## Physical Wiring: S3 → P4 Pin Remapping

### Waveshare ESP32-P4-WiFi6-DEV-Kit — 40-Pin Header (Verified from Board Image)

```
              ┌──────────────────────────┐
              │   ESP32-P4-WIFI6-DEV-KIT │
              │         40-PIN HEADER     │
              ├─────────────┬────────────┤
  Pin 1  ───→ │  3V3  (PWR) │  5V  (PWR) │ ←── Pin 2
  Pin 3  ───→ │  SDA/GPIO7  │  5V  (PWR) │ ←── Pin 4
  Pin 5  ───→ │  SCL/GPIO8  │  GND       │ ←── Pin 6
  Pin 7  ───→ │  GPIO23     │  TXD/GPIO37│ ←── Pin 8
  Pin 9  ───→ │  GND        │  RXD/GPIO38│ ←── Pin 10
  Pin 11 ───→ │  GPIO21     │  GPIO22    │ ←── Pin 12
  Pin 13 ───→ │  GPIO20     │  GND       │ ←── Pin 14
  Pin 15 ───→ │  GPIO6      │  GPIO5     │ ←── Pin 16
  Pin 17 ───→ │  3V3  (PWR) │  GPIO4     │ ←── Pin 18
  Pin 19 ───→ │  GPIO3      │  GND       │ ←── Pin 20
  Pin 21 ───→ │  GPIO2      │  GPIO1     │ ←── Pin 22
  Pin 23 ───→ │  GPIO0      │  GPIO36    │ ←── Pin 24
  Pin 25 ───→ │  GND        │  GPIO32    │ ←── Pin 26
  Pin 27 ───→ │  GPIO24     │  GPIO25    │ ←── Pin 28
  Pin 29 ───→ │  GPIO33     │  GND       │ ←── Pin 30
  Pin 31 ───→ │  GPIO26     │  GPIO54    │ ←── Pin 32
  Pin 33 ───→ │  GPIO48     │  GND       │ ←── Pin 34
  Pin 35 ───→ │  GPIO53     │  GPIO46    │ ←── Pin 36
  Pin 37 ───→ │  GPIO47     │  GPIO27    │ ←── Pin 38
  Pin 39 ───→ │  GND        │  GPIO45    │ ←── Pin 40
              └─────────────┴────────────┘
```

**Header summary**: 28 GPIOs + 3× 3V3 + 2× 5V + 8× GND = 40 pins

**GPIOs on header**: 0, 1, 2, 3, 4, 5, 6, 7, 8, 20, 21, 22, 23, 24, 25, 26, 27, 32, 33, 36, 37, 38, 45, 46, 47, 48, 53, 54

**Avoid**: GPIO0 (strapping pin), GPIO37/38 (UART debug — need serial monitor)

**Reserved for onboard peripherals** (on header but DO NOT use for external wiring):
- GPIO 7/8 — I2C bus dedicated to onboard ES8311 codec. External DAC I2C uses GPIO 48/54 instead
- GPIO 53 — Onboard ES8311 PA_Ctrl (NS4150B amp). Controlled by firmware in Phase 6 — no external wire needed

### Onboard Peripherals — NOT on Header (internal to PCB)

| Peripheral | GPIOs (internal) | Notes |
|---|---|---|
| **ES8311 I2S** | 9 (DSDIN), 10 (LRCK), 11 (ASDOUT), 12 (SCLK), 13 (MCLK) | Not on header. Firmware-only (Phase 6) |
| **ES8311 I2C** | 7 (SDA), 8 (SCL) | ON header — shared bus |
| **ES8311 Amp** | 53 (PA_Ctrl → NS4150B) | ON header — controlled by firmware |
| **Ethernet RMII** | 28-31, 34-35, 49-52 | Not on header. Plug RJ45 cable |
| **SDIO → ESP32-C6** | 14-19 | Not on header. WiFi/BLE via firmware |
| **SD Card (SDMMC)** | 39-44 | Not on header. Insert microSD |
| **UART0** | 37 (TXD), 38 (RXD) | ON header — serial debug |
| **USB OTG HS** | Dedicated analog pads | Not GPIO-numbered |

### Pin Mapping: S3 → P4 (with Header Pin Position)

| Function | S3 GPIO | P4 GPIO | Header Pin | Side | Notes |
|---|---|---|---|---|---|
| **I2S0 — PCM1808 #1 + PCM5102 (full-duplex)** | | | | | |
| BCK (bit clock) | 16 | **20** | Pin 13 | Left | S3 GPIO16 is SDIO on P4 |
| WS / LRC | 18 | **21** | Pin 11 | Left | S3 GPIO18 is SDIO CLK on P4 |
| MCLK (12.288 MHz) | 3 | **22** | Pin 12 | Right | Master clock for PCM1808 PLL |
| DOUT — PCM1808 #1 data (RX) | 17 | **23** | Pin 7 | Left | ADC1 I2S data in |
| DOUT — PCM5102 DAC data (TX) | 40 | **24** | Pin 27 | Left | DAC I2S data out (full-duplex on I2S0) |
| **I2S1 — PCM1808 #2 (RX only)** | | | | | |
| DOUT2 — PCM1808 #2 data | 9 | **25** | Pin 28 | Right | Shares BCK/WS/MCLK from I2S0 |
| **DAC I2C (separate bus)** | | | | | |
| SDA | 41 | **48** | Pin 33 | Left | Dedicated DAC I2C (GPIO7/8 reserved for ES8311) |
| SCL | 42 | **54** | Pin 32 | Right | Dedicated DAC I2C (GPIO7/8 reserved for ES8311) |
| **ST7735S TFT (SPI2)** | | | | | |
| MOSI | 11 | **2** | Pin 21 | Left | |
| SCLK | 12 | **3** | Pin 19 | Left | |
| CS | 10 | **4** | Pin 18 | Right | |
| DC | 13 | **5** | Pin 16 | Right | |
| RST | 14 | **6** | Pin 15 | Left | |
| Backlight | 21 | **26** | Pin 31 | Left | PWM (LEDC) |
| **Rotary Encoder (EC11)** | | | | | |
| A | 5 | **32** | Pin 26 | Right | ISR-driven Gray code |
| B | 6 | **33** | Pin 29 | Left | ISR-driven Gray code |
| SW (push button) | 7 | **36** | Pin 24 | Right | |
| **Control & Indicators** | | | | | |
| LED | 2 | **1** | Pin 22 | Right | Stays LOW permanently |
| Amplifier relay | 4 | **27** | Pin 38 | Right | Active HIGH relay driver |
| Buzzer (PWM) | 8 | **45** | Pin 40 | Right | LEDC PWM |
| Reset button | 15 | **46** | Pin 36 | Right | Factory reset, pull-up, active LOW |
| Signal gen PWM | 38 | **47** | Pin 37 | Left | MCPWM output |
| **Spare** | | — | — | — | All 26 GPIOs allocated. No spares |

### ES8311 — Onboard (No User Wiring Needed)

The ES8311 codec is connected onboard via I2S and I2C. It will use **I2S2 TX only** (DAC output) in the firmware. Mic/ADC input is **not used**.

| Signal | GPIO | Header Pin | Direction | Used? |
|---|---|---|---|---|
| I2S MCLK | 13 | — (internal) | P4 → ES8311 | Yes |
| I2S SCLK (BCK) | 12 | — (internal) | P4 → ES8311 | Yes |
| I2S LRCK (WS) | 10 | — (internal) | P4 → ES8311 | Yes |
| I2S ASDOUT (ADC data) | 11 | — (internal) | ES8311 → P4 | **No** — mic input not used |
| I2S DSDIN (DAC data) | 9 | — (internal) | P4 → ES8311 (speaker out) | Yes |
| PA_Ctrl (amp enable) | 53 | Pin 35 (shared) | P4 → NS4150B | Yes |
| I2C SDA | 7 | Pin 3 (shared) | Bidirectional | Yes |
| I2C SCL | 8 | Pin 5 (shared) | Bidirectional | Yes |

### Ethernet — Onboard (No User Wiring Needed)

Plug in RJ45 cable. Ethernet PHY is wired internally (not on header).

### WiFi — Onboard (No User Wiring Needed)

ESP32-C6 co-processor communicates via SDIO (GPIOs 14-19, not on header).

### Physical Wiring Guide — Step by Step

Each wire that was connected to the ESP32-S3 must be moved to the new P4 header pin. Reference the header diagram above for physical pin positions.

#### Step 1: Power (do first)
```
P4 Pin 1  (3V3, top-left)    ←── Red wire ──→  Breadboard 3V3 rail
P4 Pin 6  (GND, top-right)   ←── Black wire ──→  Breadboard GND rail
```
Connect TFT VCC and Encoder VCC to 3V3 rail. Connect TFT GND and Encoder GND to GND rail.

#### Step 2: Audio I2S Bus (8 wires total)

**Shared clock lines** (3 wires, bus topology — all 3 audio devices share these):
```
PCM1808 #1 BCK  ──┐
PCM1808 #2 BCK  ──┼──→ P4 Pin 13 (GPIO20, left)
PCM5102    BCK  ──┘

PCM1808 #1 WS   ──┐
PCM1808 #2 WS   ──┼──→ P4 Pin 11 (GPIO21, left)
PCM5102    LRC   ──┘

PCM1808 #1 MCLK ──┐
PCM1808 #2 MCLK ──┼──→ P4 Pin 12 (GPIO22, right)
                       12.288 MHz — keep wire SHORT (< 10cm)
```

**Data lines** (3 wires, point-to-point):
```
PCM1808 #1 DOUT  ──→ P4 Pin 7  (GPIO23, left)     ADC1 data in
PCM1808 #2 DOUT  ──→ P4 Pin 28 (GPIO25, right)    ADC2 data in
PCM5102    DIN   ──→ P4 Pin 27 (GPIO24, left)     DAC data out
```

**I2C for DAC** (2 wires, separate bus — GPIO 7/8 reserved for onboard ES8311):
```
DAC SDA  ──→ P4 Pin 33 (GPIO48, left)    Dedicated DAC I2C bus
DAC SCL  ──→ P4 Pin 32 (GPIO54, right)   Dedicated DAC I2C bus
```
Add external 4.7kΩ pull-up resistors to 3V3 on both lines (no onboard pull-ups on these GPIOs).

#### Step 3: TFT Display (6 signal wires + power)
```
ST7735S MOSI  ──→ P4 Pin 21 (GPIO2, left)
ST7735S SCLK  ──→ P4 Pin 19 (GPIO3, left)
ST7735S CS    ──→ P4 Pin 18 (GPIO4, right)
ST7735S DC    ──→ P4 Pin 16 (GPIO5, right)
ST7735S RST   ──→ P4 Pin 15 (GPIO6, left)
ST7735S BL    ──→ P4 Pin 31 (GPIO26, left)    Backlight PWM
ST7735S VCC   ──→ 3V3 rail
ST7735S GND   ──→ GND rail
```

#### Step 4: Rotary Encoder (3 signal wires + power)
```
EC11 A    ──→ P4 Pin 26 (GPIO32, right)
EC11 B    ──→ P4 Pin 29 (GPIO33, left)
EC11 SW   ──→ P4 Pin 24 (GPIO36, right)
EC11 VCC  ──→ 3V3 rail
EC11 GND  ──→ GND rail
```

#### Step 5: Control & Indicators (5 wires)
```
Status LED     ──→ P4 Pin 22 (GPIO1, right)     Stays LOW permanently
Amp Relay      ──→ P4 Pin 38 (GPIO27, right)    Active HIGH
Buzzer         ──→ P4 Pin 40 (GPIO45, right)    LEDC PWM
Reset Button   ──→ P4 Pin 36 (GPIO46, right)    Pull-up, active LOW
Signal Gen PWM ──→ P4 Pin 37 (GPIO47, left)     MCPWM output
```

#### Step 6: Pre-Power Verification Checklist
- [ ] Count wires: 22 signal + 3V3 + GND = 24 total
- [ ] No wires touching adjacent header pins
- [ ] MCLK wire (Pin 12, GPIO22) is short (< 10cm) — 12.288 MHz signal integrity
- [ ] DAC I2C wires (Pin 33/32, GPIO48/54) — add external 4.7kΩ pull-up resistors to 3V3 (no onboard pull-ups on these GPIOs)
- [ ] USB-C cable connected to **UART port** (not USB OTG port) for serial monitor
- [ ] No wires on Pin 23 (GPIO0 — strapping pin) or Pin 8/10 (UART TX/RX)
- [ ] 3V3 power (not 5V!) to TFT and encoder

### I2C Bus Layout

The P4 board has **two separate I2C buses**:

| Bus | SDA | SCL | Devices | Pull-ups |
|---|---|---|---|---|
| **Bus 0 (onboard)** | GPIO 7 (Pin 3) | GPIO 8 (Pin 5) | ES8311 codec only | Onboard |
| **Bus 1 (external)** | GPIO 48 (Pin 33) | GPIO 54 (Pin 32) | DAC EEPROM / I2C DACs | **Add 4.7kΩ to 3V3** |

GPIO 7/8 are reserved for the onboard ES8311 — do NOT connect external I2C devices to them. The firmware uses a separate `Wire1` instance on GPIO 48/54 for external DAC I2C (ES9038, ES9842, or EEPROM). PCM5102A has no I2C (passive DAC — these 2 wires are unused if PCM5102A is your only DAC).

---

## Phase 0: Environment Bootstrap — P4 builds at all

**Goal**: Minimal `setup()/loop()` compiles and boots on P4. Serial works.

**Files**:
- `platformio.ini` — add `[env:esp32-p4]` section (keep S3 env for reference)
- `src/idf_component.yml` — declare `esp_hosted` + `esp_wifi_remote` dependencies

**Changes**:
```ini
[env:esp32-p4]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip
board = esp32-p4_r3            ; 400MHz production silicon
framework = arduino
board_build.filesystem = littlefs
board_build.arduino.memory_type = opi_opi  ; P4 32MB OPI PSRAM
extra_scripts = pre:tools/patch_websockets.py
```

- Temporarily disable `USB_AUDIO_ENABLED`, `DAC_ENABLED`, `GUI_ENABLED` to get a clean compile
- Swap 17 ESP-DSP `-I` paths: `esp32s3` → `esp32p4` (verify paths exist in SDK)
- Swap 2 TinyUSB `-I` paths: `esp32s3` → `esp32p4`
- Verify pioarduino has `esp32-p4_r3` board definition

**Portable modules (no changes needed)**:
- `app_events.h/.cpp` — FreeRTOS event groups are standard RTOS API, fully portable
- `mqtt_task.h/.cpp` — Pure FreeRTOS task, portable (core affinity revisited in Phase 4)
- All deferred save logic — `millis()` based, portable
- `app_state.h` — 11 dirty flags + event signaling, all portable

- **First milestone**: Serial prints "Hello from P4" at 400MHz with PSRAM detected

**Risk**: LOW-MEDIUM. Validate board JSON exists before writing full config.

---

## Phase 1: GPIO & Display — Boots with TFT + encoder

**Goal**: TFT renders, encoder input works, serial logging, buzzer beeps. All pinout displays updated.

**Files**:
- `src/config.h` — all pin `#define`s remapped for P4/Waveshare pinout
- `platformio.ini` — pin build flags updated
- `src/gui/lgfx_config.h` — SPI bus + GPIO numbers for P4
- `src/gui/screens/scr_debug.cpp` — update `all_pins[]` array for P4 GPIOs
- `web_src/index.html` — update HTML pin table (lines 1925-1961) for P4 GPIOs
- `test/test_pinout/test_pinout.cpp` — update mirror of `all_pins[]` and expected values

**Changes**:
- Consult Waveshare ESP32-P4-WiFi6-DEV-Kit schematic for GPIO mapping
- ST7735S stays on SPI2_HOST (exists on P4), just different pin numbers
- MCPWM `SIGGEN_MCPWM_RESOLUTION = 160000000` — P4 also has PLL_F160M, likely unchanged
- `IRAM_ATTR` ISRs → maps to 8KB TCM on P4 RISC-V (encoder ISRs are small, fits fine)
- `ESP.getChipModel()` auto-returns "ESP32-P4" — no code change
- Update cosmetic strings in `src/strings.h` ("ESP32-S3" → "ALX Nova" or auto-detect)

**Debug Pinout Table Updates** (3 locations — all must be consistent):

1. **GUI Debug Screen** (`src/gui/screens/scr_debug.cpp` lines 47-72):
   - `all_pins[]` array uses `#define` constants from `config.h` — **auto-updates** when config.h pin defines change
   - Add new entries for P4-only peripherals (ES8311 DAC output, Ethernet, Signal Gen PWM)
   - Wrap S3-only entries (if any) with `#if CONFIG_IDF_TARGET_ESP32S3`

2. **Web Interface** (`web_src/index.html` lines 1925-1961):
   - HTML table has **hardcoded GPIO numbers** — must be manually updated
   - Add new rows: ES8311 DAC (I2S2 TX, GPIO 9), PA_Ctrl (GPIO 53), Ethernet (info-only)
   - Add new category badge: `pin-cat-network` for Ethernet/WiFi
   - Change all GPIO numbers from S3 values to P4 values
   - **Alternative**: Make table dynamic via WebSocket (firmware sends pin config at connect)
   - After editing: run `node tools/build_web_assets.js` to regenerate `src/web_pages.cpp`

3. **Unit Test** (`test/test_pinout/test_pinout.cpp` lines 81-107):
   - Has its own `all_pins[]` copy — must be manually updated to match `scr_debug.cpp`
   - **Bug fix**: DOUT2 GPIO currently hardcoded as 19 (wrong) — should be 9 on S3, 25 on P4
   - Update all expected GPIO values in test assertions
   - Add entries for new P4 peripherals (ES8311 DAC, Signal Gen PWM)
   - Guard S3 vs P4 expected values: `#if CONFIG_IDF_TARGET_ESP32P4` or use config.h defines

**Depends on**: Phase 0

---

## Phase 2: I2S Audio — PCM1808 + PCM5102 on P4

**Goal**: Audio capture from both PCM1808 ADCs and playback to PCM5102 DAC work correctly.

**Files**:
- `src/i2s_audio.cpp` — `i2s_configure_adc1()`, `i2s_configure_adc2()`, I2S instance assignment
- `src/config.h` — I2S pin defines

**Changes**:
- IDF5 `driver/i2s_std.h` API is the same on P4 — functions are portable
- P4 has 3 HP I2S → plan instance allocation:
  - **I2S0**: PCM1808 #1 full-duplex (RX ADC1 + TX PCM5102) — same as now
  - **I2S1**: PCM1808 #2 (RX only, master no-clock-output) — same as now
  - **I2S2**: Reserved for ES8311 (Phase 6)
  - **LP I2S**: Reserved for LP core sensing (Phase 7)
- **Test slave mode first**: The S3 slave DMA bug may not exist on P4. If slave works, ADC2 becomes a true slave (simpler). If not, reuse dual-master workaround
- MCLK: `I2S_CLK_SRC_DEFAULT` works on P4 (PLL, not APLL — same as S3). Verify 12.288MHz output for PCM1808
- Full-duplex init order (TX first, RX second, same MCLK pin) is IDF5-portable

**Audio hardening portability**:
- 12 DMA buffers (`I2S_DMA_BUF_COUNT=12`, `I2S_DMA_BUF_LEN=256`) — IDF5 standard, same on P4
- I2S DMA ISR pinned to Core 1 via `i2s_channel_register_event_callback()` — valid on P4, same API
- WiFi power-save disabled (`WIFI_PS_NONE`) — irrelevant on P4 (WiFi runs on separate C6 chip via SDIO). Remove or guard with `CONFIG_IDF_TARGET_ESP32S3`
- Emergency Limiter was removed — one fewer DSP stage in pipeline. Per-channel DSP limiter in `dsp_pipeline.cpp` unchanged

**Depends on**: Phases 0, 1
**Risk**: MEDIUM — PCM1808 PLL lock and MCLK continuity are hardware-critical. Prototype in isolation first.

---

## Phase 3: ESP-DSP Library — S3 Xtensa → P4 RISC-V

**Goal**: DSP pipeline compiles and runs (biquad IIR, FIR, FFT, vector math).

**Files**:
- `platformio.ini` — 17 `-I` include paths, `lib_ignore`

**Changes**:
- Espressif ships P4-optimized ESP-DSP with RISC-V assembly (`_arp4` extensions)
- Check if pre-built `.a` exists at `{packages}/framework-arduinoespressif32/tools/sdk/esp32p4/...`
- If yes: drop-in replacement (same API: `dsps_biquad_f32`, `dsps_fir_f32`, `dsps_fft4r_fc32`, etc.)
- If no: temporarily un-ignore `lib/esp_dsp_lite/` ANSI C fallback for the P4 env. Performance is lower but P4's 400MHz compensates
- The `dsps_fft4r` (Radix-4) may only have `dsps_fft2r` (Radix-2) on P4 — check and adapt `i2s_audio.cpp` FFT calls if needed

**Depends on**: Phase 0
**Risk**: LOW if pre-built lib exists. MEDIUM if ANSI C fallback needed.

---

## Phase 4: Dual-Stack Networking — Ethernet + WiFi with Failover

**Goal**: Device connects via Ethernet when cable present, falls back to WiFi 6 via C6. MQTT, OTA, WebSocket, HTTP all work over either interface transparently.

**Files**:
- New: `src/eth_manager.h` / `src/eth_manager.cpp` — Ethernet HAL (P4 internal MAC + RTL8201F PHY)
- `src/wifi_manager.h` / `src/wifi_manager.cpp` — add failover gate
- `src/app_state.h` — new network state fields
- `src/main.cpp` — init sequence
- `src/idf_component.yml` — `esp_hosted`, `esp_wifi_remote`

**Changes**:

**WiFi (unchanged API, new transport)**:
- `WiFi.h` / `esp_wifi.h` calls in `wifi_manager.cpp` work transparently via `esp_wifi_remote` RPC over SDIO to the ESP32-C6
- No source code changes to WiFi logic — component declaration handles it
- `WiFi.setPins()` may need SDIO GPIO config for Waveshare board

**Ethernet (new module)**:
```cpp
// src/eth_manager.h
void eth_manager_init();           // MAC + RTL8201F PHY, RMII pins from schematic
bool eth_manager_is_connected();   // Link up + IP acquired
String eth_manager_get_ip();
```
Uses IDF `esp_eth.h`: `esp_eth_mac_new_esp32()` + `esp_eth_phy_new_rtl8201()`

**Failover logic** (main loop or network task):
- Priority: Ethernet > WiFi
- Ethernet up → suppress WiFi reconnect, use Ethernet as default route
- Ethernet down → trigger WiFi reconnect cycle
- TCP stack routes via `esp-netif` default route — MQTT/HTTP/WS don't need changes

**AppState additions**:
```cpp
enum NetworkInterface { NET_NONE, NET_ETHERNET, NET_WIFI };
NetworkInterface activeInterface = NET_NONE;
bool ethernetConnected = false;
```

**Add event bits**: `EVT_ETHERNET` (bit 11) for Ethernet link state changes. Wire to `markEthernetDirty()` in AppState. `app_events.h` has 13 spare bits (11-23).

**MQTT task core affinity reassessment**:
- On S3: MQTT task on Core 0 because WiFi stack runs on Core 0 (shared memory bus)
- On P4: WiFi runs on separate ESP32-C6 chip via SDIO — no WiFi contention on either core
- **Recommendation**: Keep MQTT on Core 0 with GUI (both low-priority, non-realtime). Core 1 reserved for audio pipeline + I2S DMA ISR
- `mqtt_task.cpp` checks `WiFi.status()` — on P4 this proxies through `esp_wifi_remote` transparently. No code change needed
- Add Ethernet connectivity check: `if (!eth_manager_is_connected() && WiFi.status() != WL_CONNECTED)` guard

**Depends on**: Phases 0, 1
**Risk**: MEDIUM-HIGH — SDIO WiFi init has active community bugs. Ethernet is straightforward (on-chip MAC). Test WiFi SDIO init in Phase 0 hello-world to de-risk early.

---

## Phase 5: USB Audio High-Speed

**Goal**: UAC2 speaker device at 480Mbps HS, 24-bit, up to 96kHz stereo.

**Files**:
- `src/usb_audio.cpp` — rewrite ESP32-specific HAL section for P4
- `src/usb_audio.h` — update rate/format constants
- `src/config.h` — `USB_AUDIO_*` constants for HS
- `platformio.ini` — TinyUSB include paths (already swapped in Phase 0)

**Changes**:

**HAL rewrite**:
- S3 uses `esp32-hal-tinyusb.h` / `tinyusb_init()` / `tinyusb_get_free_out_endpoint()` — none exist on P4
- P4 uses `esp_tinyusb.h` / `tinyusb_driver_install()` — different init path
- Custom class driver via `usbd_app_driver_get_cb()` weak function is TinyUSB-level (portable)
- USB HS endpoint max = 1024 bytes/microframe (3072 bytes/ms) — supports 96kHz 24-bit stereo easily
  - 96kHz × 2ch × 3 bytes = 576 bytes/ms (well within 1024)
  - 192kHz = 1152 bytes/ms (needs Phase 8 rate negotiation)

**Phase 5 scope**: 44.1/48/96kHz, 24-bit, stereo. Ring buffer expanded to 4096 frames.
**Deferred to Phase 8**: 192kHz (needs pipeline rate negotiation), multi-channel >2.

**Portable code** (no changes needed):
- Ring buffer (`usb_rb_*` functions) — pure C
- Format conversion (`usb_pcm24_to_int32`) — pure C
- Pipeline integration via `AudioInputSource` — architecture-agnostic

**Depends on**: Phases 0, 1
**Risk**: HIGH — P4 TinyUSB HS integration is recent. Prototype in isolation with minimal UAC2 loopback first.

---

## Phase 6: ES8311 Codec — Secondary DAC Output

**Goal**: ES8311 registered as secondary DAC output alongside PCM5102. Headphone/speaker output via onboard NS4150B amplifier. **No mic/ADC input** — ES8311 ASDOUT pin unused.

**Files**:
- New: `src/drivers/es8311.h` / `src/drivers/es8311.cpp` — I2C register driver (DAC-only init)
- New: `src/drivers/dac_es8311.h` / `src/drivers/dac_es8311.cpp` — DacDriver subclass
- `src/i2s_audio.h` / `src/i2s_audio.cpp` — add `i2s_configure_es8311()` on I2S2 (TX only)
- `src/dac_hal.h` / `src/dac_hal.cpp` — secondary output path
- `src/audio_pipeline.cpp` — dual output write
- `src/app_state.h` — ES8311 enable flag
- `src/config.h` — `AUDIO_PIPELINE_MAX_OUTPUTS` updated

**No dimension expansion needed**:
```cpp
// These stay UNCHANGED:
#define AUDIO_PIPELINE_MAX_INPUTS   4   // ADC1, ADC2, Siggen, USB (no ES8311 input)
#define AUDIO_PIPELINE_MATRIX_SIZE  8   // 8×8 matrix unchanged
// Only output count changes:
#define AUDIO_PIPELINE_MAX_OUTPUTS  4   // PCM5102 L/R + ES8311-DAC L/R
```

**Matrix channel mapping** (unchanged inputs, added output):
```
Inputs (same as before):
  ch 0,1  = ADC1 L/R
  ch 2,3  = ADC2 L/R
  ch 4,5  = Siggen L/R
  ch 6,7  = USB L/R

Outputs:
  out 0,1 = PCM5102 L/R     (I2S0 TX — existing)
  out 2,3 = ES8311 DAC L/R  (I2S2 TX — new)
```

**ES8311 I2S setup**:
- I2S2 **TX only**: P4→ES8311 DSDIN (GPIO 9) for DAC output to headphone/speaker
- ES8311 ADC path not initialized (ASDOUT GPIO 11 unused)
- ES8311 is I2S slave, P4 I2S2 is master
- I2C config for ES8311 DAC registers (volume, sample rate, output mode) — Espressif provides reference driver in `esp-adf`
- PA_Ctrl (GPIO 53) → NS4150B amp enable

**Dual output path** in `dac_output_write()`:
- Primary: PCM5102 via I2S0 TX (matrix output ch 0,1)
- Secondary: ES8311 via I2S2 TX (matrix output ch 2,3)
- Independent volume per output device
- ES8311 has hardware volume control (I2C register), unlike PCM5102 (software volume only)

**DSP channels**: `DSP_MAX_CHANNELS = 4` unchanged — ES8311 is output only, no DSP processing needed on its path.

**Depends on**: Phases 0, 1, 2
**Risk**: LOW-MEDIUM. Output-only is much simpler than full-duplex. ES8311 DAC init is well-documented. Main risk: I2S2 TX timing with I2S0 TX (both running same sample rate, separate DMA).

---

## Phase 7: LP Core Always-On Sensing (Deferred)

**Goal**: Move signal detection to P4's LP RISC-V core. HP cores can sleep while LP monitors audio.

**Files**:
- New: `src/lp_sensing/` — LP core firmware (separate RISC-V compilation)
- `src/smart_sensing.cpp` — read LP core shared memory instead of I2S analysis
- `src/app_state.h` — LP core shared memory struct

**Approach**:
- LP core runs simple RMS threshold on LP I2S (16kHz mono, low-power)
- Writes `signal_detected` flag to shared memory
- HP `detectSignal()` just reads the flag
- Requires `esp_lp_core` IDF component + LP RISC-V toolchain

**Simpler fallback**: HP audio task writes RMS to volatile struct (already happens via `_analysis`). LP core only used for deep-sleep wake-on-signal via GPIO threshold comparator.

**Depends on**: All prior phases stable
**Risk**: HIGH — LP core in Arduino framework is undocumented. **Recommend deferring until after v2.0 is stable.**

---

## Phase 8: Deferred Features

| Feature | Why deferred |
|---|---|
| 192kHz USB Audio | Needs pipeline rate negotiation — all buffer sizes, DSP coefficients, DMA timing change |
| Multi-channel USB (>2ch) | UAC2 spatial cluster descriptors + lane-to-channel mapping design needed |
| MIPI DSI display | LovyanGFX P4 MIPI path broken. Use ESP32_Display_Panel when ready |
| H.264 / JPEG / Camera | No current use case |
| PPA-accelerated LVGL | Useful but complex. Investigate after display upgrade |

---

## Testing Strategy

### Current State (Pre-Migration Baseline)
- **891 tests** across **36 test modules** + 3 hardware test envs (native `pio test -e native`)
- Test directories: `test_api`, `test_audio_diagnostics`, `test_audio_pipeline`, `test_auth`, `test_button`, `test_buzzer`, `test_crash_log`, `test_dac_eeprom`, `test_dac_hal`, `test_debug_mode`, `test_dim_timeout`, `test_dsp`, `test_dsp_presets`, `test_dsp_rew`, `test_dsp_swap`, `test_esp_dsp`, `test_fft`, `test_gui_home`, `test_gui_input`, `test_gui_navigation`, `test_i2s_audio`, `test_mqtt`, `test_ota`, `test_ota_task`, `test_peq`, `test_pinout`, `test_settings`, `test_signal_generator`, `test_smart_sensing`, `test_task_monitor`, `test_usb_audio`, `test_utils`, `test_vrms`, `test_websocket`, `test_websocket_messages`, `test_wifi`
- Existing mocks: Arduino, WiFi, MQTT (PubSubClient), NVS (Preferences), I2S, LVGL stubs
- `app_events.h` already stubs for native tests (`#define` no-ops under `UNIT_TEST`)

### Native Test Build Flag Gaps (Fix in Phase 6)
The `[env:native]` build flags are missing explicit pipeline dimension defines. Currently they rely on `#ifndef` defaults (4, 4, 8). Since ES8311 is output-only (no new input lane), dimensions stay at 4/4/8. However, `AUDIO_PIPELINE_MAX_OUTPUTS` should be explicitly set for clarity.

**Action**: Add to `platformio.ini [env:native]` build_flags:
```ini
-D AUDIO_PIPELINE_MAX_INPUTS=4
-D AUDIO_PIPELINE_MAX_OUTPUTS=4
-D AUDIO_PIPELINE_MATRIX_SIZE=8
```

### New Mocks Needed Per Phase

| Phase | Mock / Stub | Pattern |
|---|---|---|
| **Phase 4** | `test/test_mocks/ETH.h` — Ethernet HAL mock | Follow `test_mocks/WiFi.h` pattern: `eth_init()`, `eth_connected()`, `eth_get_ip()` |
| **Phase 4** | `test/test_mocks/esp_eth.h` — IDF Ethernet stubs | Return `ESP_OK` for `esp_eth_mac_new_esp32()`, `esp_eth_phy_new_rtl8201()` |
| **Phase 6** | `test/test_mocks/es8311.h` — ES8311 I2C register mock | Mock I2C writes for DAC volume/rate config |
| **Phase 6** | `test_es8311/` — New test module | Validate ES8311 DacDriver subclass, I2C DAC register sequences |
| **Phase 6** | `test_dac_hal/` — Expand existing | Dual output dispatch (PCM5102 + ES8311) |

### Per-Phase Test Actions

**Phase 0**: No test changes. `pio test -e native` must pass unchanged (891 tests). This validates that all portable modules compile identically

**Phase 1**: Update `test_pinout/` — P4 GPIO numbers differ from S3. Add `#if CONFIG_IDF_TARGET_ESP32P4` guards in pin validation tests, or parameterize pin tables

**Phase 2**: Existing `test_i2s_audio/` and `test_audio_pipeline/` cover I2S logic. No changes needed — they test pure computation functions (RMS, VU, quantize, downsample), not hardware

**Phase 3**: `lib/esp_dsp_lite/` ANSI C fallback already used by native tests. DSP test modules (`test_dsp`, `test_esp_dsp`, `test_dsp_rew`, `test_dsp_swap`, `test_dsp_presets`, `test_peq`) validate coefficient generation and pipeline logic. If P4 uses Radix-2 FFT instead of Radix-4, update `test_fft` expected values

**Phase 4**: Add `test_eth_manager/` module + ETH mock. Test: failover logic (Ethernet→WiFi→Ethernet), MQTT connectivity across interface switches, `EVT_ETHERNET` event signaling. Update `test_mqtt/` to cover `mqtt_task.cpp` Ethernet awareness

**Phase 5**: Existing `test_usb_audio/` covers ring buffer, format conversion, pipeline integration. No changes for HS — buffer sizes and format logic are rate-independent

**Phase 6**: Moderate test expansion (output-only, no pipeline dimension changes):
- New `test_es8311/` — I2C DAC register driver, DacDriver subclass, volume/rate config
- Expand `test_dac_hal/` — secondary output registration, dual-output dispatch (PCM5102 + ES8311)
- Add explicit pipeline dimension build flags to `[env:native]` for clarity

### Hardware Validation Envs
For each phase, create standalone `[env:p4_xxx_test]` in platformio.ini (like existing `idf5_pcm1808_test`):
- `p4_hello_test` — Phase 0: Serial, PSRAM, chip info
- `p4_gpio_test` — Phase 1: TFT, encoder, buzzer, LED
- `p4_i2s_test` — Phase 2: PCM1808 capture, PCM5102 playback
- `p4_eth_test` — Phase 4: Ethernet link, DHCP, ping
- `p4_usb_test` — Phase 5: UAC2 enumeration, audio stream
- `p4_es8311_test` — Phase 6: Codec init, I2C regs, I2S capture/playback

### CI/CD
- `.github/workflows/tests.yml` currently says "106 tests" — stale. Update to `891` (or remove hardcoded count)
- CI runs `pio test -e native` — no hardware needed, unaffected by P4 migration
- Add `pio run -e esp32-p4` build step to CI after Phase 0 (compile check only, no flash)

---

## S3-Specific Blockers Summary (6 BLOCKER, 4 HIGH, 5 MEDIUM, 3 LOW)

| # | Item | Severity | Phase | Notes |
|---|---|---|---|---|
| 1 | `platformio.ini` board/memory_type/include paths | BLOCKER | 0 | |
| 2 | ESP-DSP pre-built Xtensa `.a` → P4 RISC-V | BLOCKER | 3 | |
| 3 | USB Audio: entire S3 TinyUSB HAL layer | BLOCKER | 5 | |
| 4 | USB build flags (`ARDUINO_USB_MODE`) | BLOCKER | 0 | |
| 5 | All GPIO pin assignments (23 pins) | BLOCKER | 1 | |
| 6 | FFT: `dsps_fft4r` S3 SIMD → P4 variant | BLOCKER | 3 | |
| 7 | I2S dual-master clock hack (S3-specific) | HIGH | 2 | May be fixable — test P4 slave mode first |
| 8 | PSRAM `qio_opi` memory type | HIGH | 0 | |
| 9 | WiFi needs `esp_wifi_remote`+`esp_hosted` | HIGH | 4 | |
| 10 | `esp32-hal-tinyusb.h` S3 HAL functions | HIGH | 5 | |
| 11 | SPI TFT pin numbers in `lgfx_config.h` | MEDIUM | 1 | |
| 12 | MCPWM 160MHz resolution constant | MEDIUM | 1 | |
| 13 | `BOARD_HAS_PSRAM` verification | MEDIUM | 0 | |
| 14 | I2C DAC pins: GPIO 41/42 → GPIO 48/54 (separate bus from ES8311) | MEDIUM | 1 | `dac_hal.h` defaults + `Wire1` instance |
| 15 | WiFi PS disable (`WIFI_PS_NONE`) — irrelevant on P4 | MEDIUM | 2 | Guard with `#if CONFIG_IDF_TARGET_ESP32S3` |
| 16 | `"ESP32-S3"` hardcoded strings | LOW | 1 | |
| 17 | MQTT task core affinity (Core 0 WiFi assumption) | LOW | 4 | Keep Core 0 — works, just different reason |
| 18 | Native test build flags missing explicit pipeline dimensions | LOW | 6 | Add explicit `-D` defines to `[env:native]` for clarity (values unchanged: 4/4/8) |

**Resolved by recent refactoring** (no longer blockers):
- Emergency Limiter — fully removed, no DSP code to port
- 7 dead dirty flags — removed, fewer AppState paths to validate
- Main loop MQTT calls — moved to `mqtt_task.cpp`, main loop is cleaner

---

## Verification (End-to-End)

After each phase, verify both **native tests** and **hardware**:

| Phase | Native Tests | Hardware Verification |
|---|---|---|
| **0** | `pio test -e native` passes (891 tests, unchanged) | `pio run -e esp32-p4` compiles. Flash → serial prints chip info, PSRAM size |
| **1** | `test_pinout` updated for P4 GPIOs (fix DOUT2 bug: GPIO 19→9). All 3 pinout displays consistent | TFT renders boot animation. Encoder navigates menus. Buzzer beeps. Debug screen shows P4 GPIO numbers |
| **2** | `test_i2s_audio`, `test_audio_pipeline` pass | `[Audio] ADC1 health: OK` and `ADC2 health: OK` in serial. Waveform visible on TFT |
| **3** | `test_dsp`, `test_esp_dsp`, `test_fft` pass | DSP EQ applies audibly |
| **4** | New `test_eth_manager` passes. `test_mqtt` expanded | MQTT connects via Ethernet. Pull cable → WiFi reconnects. Plug cable → switches back. Event-driven wake (`app_events_wait`) responds to `EVT_ETHERNET` |
| **5** | `test_usb_audio` passes (existing) | Host PC sees UAC2 device at 48kHz/24-bit HS. Audio streams to pipeline lane 3 |
| **6** | New `test_es8311` passes. `test_dac_hal` expanded (dual output) | PCM5102 + ES8311 headphone/speaker output simultaneous. NS4150B amp enabled via PA_Ctrl |

**Critical invariants**:
- Keep S3 `[env:esp32-s3-devkitm-1]` working throughout. Both envs compile from the same source via `#if CONFIG_IDF_TARGET_ESP32S3 / CONFIG_IDF_TARGET_ESP32P4` guards where needed
- `pio test -e native` must pass at every phase (regression gate)
- MQTT task must connect over both Ethernet and WiFi interfaces without code changes (only `esp-netif` route changes)
- Event-driven main loop (`app_events_wait`) must work identically on both targets

---

## Module Portability Summary

Quick reference for which modules need changes vs. port cleanly:

| Module | Portable? | Notes |
|---|---|---|
| `app_events.h/.cpp` | **Yes** | FreeRTOS standard API. Native test stubs via `#define` |
| `app_state.h` | **Yes** | 11 dirty flags + event signaling. Pure C++ |
| `mqtt_task.h/.cpp` | **Yes** | FreeRTOS task. Add Ethernet check in Phase 4 |
| `settings_manager` | **Yes** | NVS/Preferences + deferred save (millis-based) |
| `smart_sensing` | **Yes** | millis-based polling + deferred save |
| `signal_generator` | **Yes** | MCPWM uses PLL_F160M (same on P4) |
| `dsp_pipeline` | **Yes** | ESP-DSP API identical. Only lib binary changes |
| `dsp_biquad_gen` | **Yes** | Pure math (RBJ cookbook) |
| `audio_pipeline` | **Mostly** | Pin defines change. DMA/I2S API identical. Add ES8311 output path in Phase 6 (no input lane changes) |
| `i2s_audio` | **Mostly** | Pin remapping + test P4 slave mode. API identical. Add I2S2 TX for ES8311 |
| `dac_hal` | **Mostly** | Pin remapping. Add ES8311 as secondary DAC output in Phase 6 |
| `wifi_manager` | **Mostly** | `esp_wifi_remote` transparent. Add failover gate in Phase 4 |
| `usb_audio` | **No** | Full HAL rewrite for HS (Phase 5) |
| `gui/lgfx_config.h` | **No** | SPI pin numbers change (Phase 1) |
| `config.h` | **No** | All 23 pin defines change (Phase 1) |

---

> **Save location**: Copy this plan to `plans/migration_p4.md` in the project root.
