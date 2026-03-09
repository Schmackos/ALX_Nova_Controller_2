# ADC Componentization — Route All ADC Init Through HAL (DEBT-ADC)

## Context

The ADC subsystem has the same hardcoded-bypass problem DAC had before DEBT-3. Two parallel code paths exist:
1. **Legacy path**: `_register_adc_sources()` in `i2s_audio.cpp` hardcodes `"ADC1 (PCM1808)"` / `"ADC2 (PCM1808)"` names and lanes 0/1, calling `audio_pipeline_set_source()` directly — bypassing HAL entirely
2. **HAL path**: `HalPcm1808` exists but its `getInputSource()` returns `nullptr` — so the bridge silently skips ADC source registration

Additionally:
- `i2s_audio_init_channels()` always initialises exactly 2 ADC lanes with `nullptr` config (no HAL pin overrides applied)
- `_rx_handle_adc1` / `_rx_handle_adc2` are hardcoded statics rather than a port-indexed array
- The dual-master I2S clock selection (who outputs MCLK/BCK/WS) is implicit in the init order, not a per-device config field
- ESP32-P4 has **3 I2S controllers** (I2S0, I2S1, I2S2) — future chips may have more or fewer; the code should not assume a fixed count

**Goal:** HAL is the sole entry point for ADC lifecycle. Any I2S ADC (PCM1808, WM8782, TDM, future) gets registered and initialised via HAL config, zero firmware changes. N ADCs supported dynamically. All compliant with the Components + Platform + State Machine approach.

---

## Architecture Alignment (Components + Platform + State Machine)

Every hardware subsystem must follow this pattern — ADC is the next to be migrated:

```
Component (HalDevice subclass)          Platform (HAL framework)
─────────────────────────────           ──────────────────────────
HalPcm1808::probe()        ─────────►  HalDeviceManager tracks state
HalPcm1808::init()         ─────────►  State: UNKNOWN→CONFIGURING→AVAILABLE
HalPcm1808::getInputSource() ────────►  Bridge: audio_pipeline_set_source(lane, src)
HalPcm1808::deinit()       ─────────►  Bridge: audio_pipeline_remove_source(lane)
HalPcm1808::healthCheck()  ─────────►  State: AVAILABLE⇄UNAVAILABLE→ERROR
```

No external code calls `i2s_configure_adc1()` or similar. The bridge owns pipeline registration.

---

## Phase 1 — HalPcm1808 Implements `getInputSource()`

**Goal:** Bridge can register PCM1808 sources dynamically. No hardcoded names or lanes.

### Changes

**`src/hal/hal_pcm1808.h`**
- Add `#include "../audio_input_source.h"`
- Add private members:
  ```cpp
  AudioInputSource _inputSrc = {};
  bool _inputSrcReady = false;
  ```
- Add public override:
  ```cpp
  const AudioInputSource* getInputSource() const override;
  ```

**`src/hal/hal_pcm1808.cpp`**
- Add `#ifndef NATIVE_TEST` guard around `#include "../i2s_audio.h"`
- In `init()`, populate `_inputSrc` after reading HalDeviceConfig:
  ```cpp
  memset(&_inputSrc, 0, sizeof(_inputSrc));
  _inputSrc.name          = _descriptor.name;   // from HAL config, not hardcoded
  _inputSrc.isHardwareAdc = true;
  _inputSrc.gainLinear    = 1.0f;
  _inputSrc.vuL           = -90.0f;
  _inputSrc.vuR           = -90.0f;

  uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 0;
  #ifndef NATIVE_TEST
  _inputSrc.read          = (port == 0) ? i2s_audio_port0_read : i2s_audio_port1_read;
  _inputSrc.isActive      = (port == 0) ? i2s_audio_port0_active : i2s_audio_port1_active;
  _inputSrc.getSampleRate = i2s_audio_get_sample_rate;
  #endif
  _inputSrcReady = true;
  ```
- Implement `getInputSource()`:
  ```cpp
  const AudioInputSource* HalPcm1808::getInputSource() const {
      return _inputSrcReady ? &_inputSrc : nullptr;
  }
  ```

**`src/i2s_audio.h`**
- Add port-indexed public API (with `NATIVE_TEST` inline stubs):
  ```cpp
  uint32_t i2s_audio_read_port(int port, int32_t *dst, uint32_t frames);
  bool     i2s_audio_is_port_active(int port);
  uint32_t i2s_audio_get_sample_rate(void);
  // Pre-baked thunks for AudioInputSource.read / .isActive (no capture):
  uint32_t i2s_audio_port0_read(int32_t* dst, uint32_t frames);
  uint32_t i2s_audio_port1_read(int32_t* dst, uint32_t frames);
  bool     i2s_audio_port0_active(void);
  bool     i2s_audio_port1_active(void);
  ```

**`src/i2s_audio.cpp`**
- Add port-indexed public wrappers delegating to existing `i2s_audio_read_adc1/2`:
  ```cpp
  uint32_t i2s_audio_port0_read(int32_t* d, uint32_t f) { return i2s_audio_read_adc1(d, f); }
  uint32_t i2s_audio_port1_read(int32_t* d, uint32_t f) { return i2s_audio_read_adc2(d, f); }
  bool i2s_audio_port0_active(void) { return true; }
  bool i2s_audio_port1_active(void) { return i2s_audio_adc2_ok(); }
  uint32_t i2s_audio_get_sample_rate(void) {
      return AppState::getInstance().audio.sampleRate;
  }
  ```

**`test/test_hal_pcm1808/test_hal_pcm1808.cpp`**
- Add 6 tests:
  1. `getInputSource()` returns nullptr before `init()`
  2. Returns non-null after `init()`
  3. `isHardwareAdc == true`
  4. `name` is not null
  5. `gainLinear == 1.0f`
  6. VU initial values are -90 dBFS

### Verification
`pio test -e native -f test_hal_pcm1808` — count rises from 14 to 20.

---

## Phase 2 — Remove `_register_adc_sources()`, Make Activation HAL-Driven

**Goal:** ADC pipeline sources registered exclusively via bridge. Hardcoded names and lanes gone.

### Boot Sequence After Phase 1+2
```
main.cpp: hal_load_auto_devices()
  → HalPcm1808 instances created, init() called
  → hal_pipeline_on_device_available(slot) called
  → bridge: getInputSource() now returns non-null ✓
  → bridge: audio_pipeline_set_source(adcLane, src)  ← source registered here

main.cpp: i2s_audio_init()
  → _register_adc_sources() REMOVED
  → i2s_audio_init_channels() deferred to audio_pipeline_task on Core 1

main.cpp: hal_pipeline_sync()
  → bridge rescans, PCM1808 already mapped → idempotent re-registration (safe)
```

### Changes

**`src/i2s_audio.cpp`**
- Delete: `static void _register_adc_sources()` function body
- Delete: 6 private statics used only by it: `_adc1_src_read`, `_adc1_src_isActive`, `_adc1_src_getSampleRate`, `_adc2_src_read`, `_adc2_src_isActive`, `_adc2_src_getSampleRate`
- Remove: the call `_register_adc_sources();` from `i2s_audio_init()`
- Keep: `_adc2InitOk`, `i2s_audio_adc2_ok()`, all I2S handle management

**`src/audio_input_source.h`**
- Mark `AUDIO_SRC_LANE_ADC1` / `AUDIO_SRC_LANE_ADC2` defines as DEPRECATED (no callers remain)

### Verification
`pio test -e native` — all 1579+ tests pass. `_register_adc_sources` was `#ifndef NATIVE_TEST` — removal has zero native test impact.

---

## Phase 3 — Add `isClockMaster` to HalDeviceConfig + Wire to `i2s_audio_init_channels()`

**Goal:** The dual-master I2S clock selection (who outputs MCLK/BCK/WS) is an explicit per-device config field. `i2s_audio_init_channels()` queries HAL devices instead of hardcoding 2 lanes.

### Changes

**`src/hal/hal_types.h`** — Add field to `HalDeviceConfig`:
```cpp
// I2S clock topology — only meaningful for HAL_BUS_I2S devices
// true  = this device outputs MCLK/BCK/WS clocks (I2S master with clock output)
// false = this device receives clocks only (I2S master data-only, BCK/WS/MCLK = UNUSED)
bool isI2sClockMaster = false;
```

**`src/i2s_audio.cpp`** — Rewrite `i2s_audio_init_channels()`:
- Query `HalDeviceManager` for all devices with `HAL_CAP_ADC_PATH` and `HAL_BUS_I2S`
- Pass 1: non-clock-masters first; Pass 2: clock master last
- Pass HAL config to `i2s_audio_configure_adc(port, cfg)` — no more `nullptr`
- Falls back to hardcoded 2-lane init inside `#else` guard (no HAL/safe mode)

```cpp
void i2s_audio_init_channels() {
#if !defined(NATIVE_TEST) && defined(DAC_ENABLED)
    HalDeviceManager& mgr = HalDeviceManager::instance();
    bool portOk[AUDIO_PIPELINE_MAX_INPUTS] = {};

    // Pass 1: non-clock-masters (data-only, no MCLK/BCK/WS output)
    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDevice* dev = mgr.getDevice(i);
        if (!dev) continue;
        auto& desc = dev->getDescriptor();
        if (!(desc.capabilities & HAL_CAP_ADC_PATH)) continue;
        if (desc.bus.type != HAL_BUS_I2S) continue;
        HalDeviceConfig* cfg = mgr.getConfig(i);
        if (cfg && cfg->isI2sClockMaster) continue;
        uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 1;
        portOk[port] = i2s_audio_configure_adc(port, cfg);
    }

    // Pass 2: clock master (outputs MCLK/BCK/WS — must be last)
    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDevice* dev = mgr.getDevice(i);
        if (!dev) continue;
        auto& desc = dev->getDescriptor();
        if (!(desc.capabilities & HAL_CAP_ADC_PATH)) continue;
        if (desc.bus.type != HAL_BUS_I2S) continue;
        HalDeviceConfig* cfg = mgr.getConfig(i);
        if (!cfg || !cfg->isI2sClockMaster) continue;
        uint8_t port = (cfg->i2sPort != 255) ? cfg->i2sPort : 0;
        portOk[port] = i2s_audio_configure_adc(port, cfg);
    }

    _adc2InitOk = portOk[1];
    _numAdcsDetected = 0;
    for (int p = 0; p < AUDIO_PIPELINE_MAX_INPUTS; p++) {
        if (portOk[p]) _numAdcsDetected++;
    }
    gpio_pulldown_en((gpio_num_t)I2S_DOUT2_PIN);
#else
    i2s_audio_configure_adc(0, nullptr);
    gpio_pulldown_en((gpio_num_t)I2S_DOUT2_PIN);
    _numAdcsDetected = 1;
#endif
}
```

**`src/hal/hal_device_db.cpp`** — Update PCM1808 builtin entries:
- Port 0 entry: `isI2sClockMaster = true` (ADC1 outputs MCLK/BCK/WS)
- Port 1 entry: `isI2sClockMaster = false` (ADC2 data-only)

**`src/hal/hal_eeprom_v3.cpp`** — persist `isI2sClockMaster` in JSON config.

### Verification
- Build firmware: `pio run`
- Boot: PCM1808 port 1 initialised before port 0 (dual-master constraint preserved)
- New ADC device with `i2sPort=0, isI2sClockMaster=true` → correct I2S setup without firmware changes

---

## Phase 4 — Full N-ADC Handle Array Refactor

**Goal:** Replace `_rx_handle_adc1` / `_rx_handle_adc2` with `_rx_handle[AUDIO_PIPELINE_MAX_INPUTS]`. ESP32-P4 has 3 I2S controllers; future chips may have more.

### Changes

**`src/i2s_audio.cpp`**
- Replace two statics with array:
  ```cpp
  static i2s_chan_handle_t _rx_handle[AUDIO_PIPELINE_MAX_INPUTS] = {};
  ```
- Update `i2s_audio_configure_adc(int port, ...)` to use `_rx_handle[port]` with `port >= AUDIO_PIPELINE_MAX_INPUTS` guard
- Update `i2s_audio_read_port()` and `i2s_audio_is_port_active()` to use `_rx_handle[port]`
- Update all 11 reference sites (`_rx_handle_adc1` → `_rx_handle[0]`, `_rx_handle_adc2` → `_rx_handle[1]`)

### Test additions
- `test/test_i2s_audio/test_audio_rms.cpp` — add port-dispatch tests (native stubs return 0 frames)

### Verification
`pio test -e native` — all tests pass. `pio run` — I2S0/I2S1 still initialise correctly.

---

## Critical Files

| File | Changes |
|------|---------|
| `src/hal/hal_pcm1808.h` | Add `AudioInputSource _inputSrc`, `getInputSource()` override |
| `src/hal/hal_pcm1808.cpp` | Implement `getInputSource()`, populate `_inputSrc` in `init()` |
| `src/i2s_audio.h` | Add port-dispatch API + pre-baked thunks with NATIVE_TEST stubs |
| `src/i2s_audio.cpp` | Remove `_register_adc_sources()` + 6 statics; rewrite `i2s_audio_init_channels()`; add port-dispatch; refactor `_rx_handle[]` array |
| `src/hal/hal_types.h` | Add `isI2sClockMaster` field to `HalDeviceConfig` |
| `src/hal/hal_device_db.cpp` | Set `isI2sClockMaster` on builtin PCM1808 entries |
| `src/hal/hal_eeprom_v3.cpp` | Persist `isI2sClockMaster` in JSON config |
| `src/audio_input_source.h` | Mark `AUDIO_SRC_LANE_ADC1` / `AUDIO_SRC_LANE_ADC2` deprecated |
| `test/test_hal_pcm1808/test_hal_pcm1808.cpp` | Add 6 `getInputSource()` tests |

## Reuse (existing patterns)
- `HalSigGen::getInputSource()` in `src/hal/hal_siggen.cpp` — direct template for Phase 1
- `hal_pipeline_on_device_available()` in `src/hal/hal_pipeline_bridge.cpp` — no bridge changes needed
- `HalDeviceManager::getDevice(uint8_t slot)` — used in Phase 3 iteration
- `i2s_audio_configure_adc(int lane, const HalDeviceConfig* cfg)` — keep signature

---

## Future Work — Component + Platform + State Machine Alignment

| Component | HAL Class | Current Gap | Alignment Work | Priority |
|-----------|-----------|-------------|----------------|----------|
| **ADC** | `HalPcm1808` | `getInputSource()=nullptr`, `_register_adc_sources()` bypasses HAL | THIS PLAN | Now |
| **DAC** | `HalPcm5102a`, `HalEs8311`, `HalDacAdapter` | Completed in DEBT-3 | Done | Complete |
| **Display** | `HalDisplay` (bookkeeping only) | `LGFX tft` static in `gui_manager.cpp`; `lv_display_create(160,128)` hardcodes size | Add `HalDisplayInterface`; `gui_manager_init()` queries HAL; backlight via HAL | High |
| **Encoder / Button** | `HalEncoder`, `HalButton` (bookkeeping) | `gui_input_init()` attaches ISRs to compile-time pins; duplicate defines | Add `HalInputInterface`; `gui_input_init()` queries HAL for pins dynamically | Medium |
| **Buzzer** | `HalBuzzer` — wiring unknown | `buzzer_handler.cpp` vs `hal_buzzer.h` — audit needed | Audit; route through device state if bypassing HAL | Medium |
| **Signal Generator** | `HalSignalGen` (pin bookkeeping) | `init()` does NOT call `siggen_init()`; `SIGGEN_PWM_PIN` read directly | `HalSignalGen.init()` calls `siggen_init(pin)` | Small |
| **USB Audio** | `HalUsbAudio` (fully wired) | `cfg.pid = 0x4004` magic; strings hardcoded | Move to named constants in `config.h` | Minimal |
| **NS4150B Amp** | `HalNs4150b` (fully wired) | Amp enables before DAC ready | Trigger amp enable from DAC AVAILABLE state | Minimal |
| **External DSP** | `HalDspBridge` | Interface ready; no external DSP chip yet | No work needed until external DSP chip added | Future |
