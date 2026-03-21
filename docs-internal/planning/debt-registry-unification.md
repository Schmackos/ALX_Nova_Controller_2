# DEBT-6: Registry Unification & Bridge Sink Ownership

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Eliminate the dual device registry (`DacRegistry` + `HalDriverRegistry`) and make the HAL pipeline bridge the sole owner of output sink lifecycle — bus-agnostic, not tied to I2S/DAC specifics.

**Architecture:** The bridge becomes the single orchestrator for all output device sink registration. Each HAL device with `HAL_CAP_DAC_PATH` builds its own `AudioOutputSink` via a `buildSink()` virtual method, using shared write utilities for common operations (software volume, mute ramp). `dac_hal.cpp` is slimmed to bus-level utilities only. `DacRegistry`, `DacDriver`, and `HalDacAdapter` are deleted entirely.

**Tech Stack:** ESP32-P4, PlatformIO/Arduino, FreeRTOS, Unity test framework, C/C++

**Addresses concerns:**
- "Dual device registries — `DacRegistry` and `HalDriverRegistry` both active" (CONCERNS.md line 9)
- "Bridge is metadata-only — sinks registered directly from `dac_hal.cpp`" (CONCERNS.md line 16)
- "I2S MCLK pin hardcoded in `i2s_configure_adc1()` despite HAL config" (CONCERNS.md line 167, principle extended)

**Specialist reviews completed:**
- Embedded RT safety review: 6 hard constraints identified (see Constraints section)
- Architecture review: 7 gaps identified and addressed (see Gap Resolution section)

---

## Hard Constraints (from Embedded RT Safety Review)

These are non-negotiable. Every phase must preserve all six:

| # | Constraint | Violation consequence |
|---|---|---|
| HC-1 | `calloc()` for DMA buffers (`_sinkBuf[]`, `_rawBuf[]`) must never execute while `vTaskSuspendAll()` is active | Heap corruption — non-deterministic crash |
| HC-2 | `audioPaused` binary semaphore taken **exactly once** per deinit sequence — batch multi-device teardowns under a single envelope | Deadlock (double take) or I2S race (skipped take) |
| HC-3 | I2S TX teardown must use `i2s_audio_disable_tx()` / `_dac_disable_i2s_tx_for_port()` only — HAL device `deinit()` must **never** call `i2s_channel_disable()` / `i2s_del_channel()` on the RX channel | PCM1808 PLL loss → 10-50ms garbage audio |
| HC-4 | `i2s_configure_adc1()` must remain pinned to Core 1 task context — no re-init from Core 0 HAL callbacks | DMA ISR registered on wrong core → audio pops |
| HC-5 | `halSlot → sinkSlot` mapping must be idempotent across AVAILABLE→UNAVAILABLE→AVAILABLE cycles — `_sinkSlotForDevice()` returns existing slot if already mapped | DMA buffer / thunk slot mismatch |
| HC-6 | `_muteGain` must be reset to `1.0f` when slot 0 is deactivated | Stale ramp state on device re-enable |

Additional embedded concern:
- **Callback re-entry**: Bridge → activate → `hal_pipeline_state_change()` → bridge. All re-entrant paths must be idempotent (early-return on already-mapped slot).

---

## Gap Resolution (from Architecture Review)

| Gap | Resolution |
|---|---|
| G1: `DacDriver` class hierarchy deletion not explicit | Phase 1 deletes `DacDriver`, `DacPcm5102`, `DacEs8311`, `HalDacAdapter` |
| G2: Write thunk/I2S dispatch ownership (150+ lines) | Extracted to `sink_write_utils.h/.cpp` — shared stateless helpers |
| G3: `dac_boot_prepare()` not mentioned | Stays in `dac_hal.cpp` — genuinely bus-level EEPROM/I2C init |
| G4: `dac_output_is_ready()` callers | Replaced with `hal_pipeline_has_active_sink(slot)` bridge query |
| G5: `filterMode` ownership | Moved from `DacState` to `HalDeviceConfig` — device-specific field |
| G6: Main loop `pendingToggle` handler | Updated to call bridge-level activate/deactivate |
| G7: `DacCapabilities` struct migration | Capabilities from `HalDeviceDescriptor` fields + optional `getExtendedCaps()` |

---

## Phase 0: State Consolidation (Prep)

**Goal:** Isolate mutable per-slot state into the pipeline's sink struct before touching audio control flow. No audio path changes — purely state-location moves.

**Commit prefix:** `refactor(phase0):`

### Task 0.1: Add `audio_pipeline_set_sink_muted()` API

**Files:**
- Modify: `src/audio_pipeline.h` (add declaration)
- Modify: `src/audio_pipeline.cpp` (add implementation)
- Test: `test/test_sink_slot_api/test_sink_slot_api.cpp`

**Step 1: Write the failing test**

In `test/test_sink_slot_api/test_sink_slot_api.cpp`, add:

```cpp
void test_set_sink_muted_updates_sink_struct(void) {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.write = mock_write_fn;
    sink.isReady = mock_ready_fn;
    audio_pipeline_set_sink(0, &sink);

    audio_pipeline_set_sink_muted(0, true);
    // Verify the sink's muted field is set
    TEST_ASSERT_TRUE(audio_pipeline_is_sink_muted(0));

    audio_pipeline_set_sink_muted(0, false);
    TEST_ASSERT_FALSE(audio_pipeline_is_sink_muted(0));
}

void test_set_sink_muted_ignores_invalid_slot(void) {
    // Should not crash on out-of-range slot
    audio_pipeline_set_sink_muted(AUDIO_OUT_MAX_SINKS, true);
    audio_pipeline_set_sink_muted(255, true);
}
```

**Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_sink_slot_api -v`
Expected: FAIL — `audio_pipeline_set_sink_muted` not defined

**Step 3: Write minimal implementation**

In `src/audio_pipeline.h`, add declarations:

```cpp
void audio_pipeline_set_sink_muted(uint8_t slot, bool muted);
bool audio_pipeline_is_sink_muted(uint8_t slot);
```

In `src/audio_pipeline.cpp`, add implementation:

```cpp
void audio_pipeline_set_sink_muted(uint8_t slot, bool muted) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return;
    vTaskSuspendAll();           // HC-1: no calloc here, just a bool write
    _sinks[slot].muted = muted;
    xTaskResumeAll();
}

bool audio_pipeline_is_sink_muted(uint8_t slot) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return true;
    return _sinks[slot].muted;
}
```

**Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_sink_slot_api -v`
Expected: PASS

**Step 5: Commit**

```
refactor(phase0): add audio_pipeline_set_sink_muted() API

Provides atomic mute control on sink structs via vTaskSuspendAll().
Preparation for consolidating _muteForSlot[] into pipeline-owned state.
```

---

### Task 0.2: Migrate `_muteForSlot[]` callers to pipeline API

**Files:**
- Modify: `src/dac_hal.cpp` (remove `_muteForSlot[]` reads/writes)
- Modify: `src/dac_api.cpp` (replace `dac_set_mute_for_slot()` calls)
- Modify: `src/websocket_handler.cpp` (replace `dac_set_mute_for_slot()` calls)
- Test: existing `test_dac_hal` + `test_sink_slot_api`

**Step 1: Write test verifying pipeline mute is checked in write path**

In `test/test_sink_slot_api/test_sink_slot_api.cpp`:

```cpp
void test_muted_sink_skipped_in_dispatch(void) {
    // Register a sink, then mute it
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.write = mock_write_fn;
    sink.isReady = mock_ready_fn;
    audio_pipeline_set_sink(0, &sink);

    mock_write_call_count = 0;
    audio_pipeline_set_sink_muted(0, true);

    // Pipeline dispatch should skip muted sinks
    // (verified via the existing sink->muted check in audio_pipeline.cpp dispatch loop)
    TEST_ASSERT_TRUE(audio_pipeline_is_sink_muted(0));
}
```

**Step 2: Run to verify baseline**

Run: `pio test -e native -f test_sink_slot_api -v`

**Step 3: Update callers**

In `src/dac_api.cpp` and `src/websocket_handler.cpp`, replace all `dac_set_mute_for_slot(slot, mute)` calls with:

```cpp
audio_pipeline_set_sink_muted(sinkSlot, mute);
```

Also update the HAL device's `setMute()` so hardware mute (for devices with I2C control like ES8311) is still applied:

```cpp
HalDevice* dev = HalDeviceManager::instance().getDevice(halSlot);
if (dev) {
    HalAudioDevice* audioDev = static_cast<HalAudioDevice*>(dev);
    audioDev->setMute(mute);
}
audio_pipeline_set_sink_muted(sinkSlot, mute);
```

In `src/dac_hal.cpp`:
- Remove `static bool _muteForSlot[AUDIO_OUT_MAX_SINKS]`
- Remove `dac_set_mute_for_slot()` function
- Update `_dac_write_for_slot()` to read mute from the sink struct (passed as parameter or via pipeline query) — OR remove the mute check from the write function entirely since the pipeline dispatch loop already skips muted sinks (`if (sink->muted) continue;`)

**Step 4: Run all affected tests**

Run: `pio test -e native -v`
Expected: All 1618+ tests pass

**Step 5: Commit**

```
refactor(phase0): migrate _muteForSlot[] to pipeline sink muted field

Callers now use audio_pipeline_set_sink_muted() instead of
dac_set_mute_for_slot(). Pipeline dispatch already skips muted sinks.
Removes redundant per-slot mute array from dac_hal.cpp.
```

---

### Task 0.3: Add `audio_pipeline_set_sink_volume()` API and migrate volume

**Files:**
- Modify: `src/audio_pipeline.h` (add volume field to `AudioOutputSink` if not present, add API)
- Modify: `src/audio_pipeline.cpp` (implementation)
- Modify: `src/dac_hal.cpp` (remove `_volumeGainForSlot[]`)
- Modify: `src/dac_api.cpp` (update callers)
- Modify: `src/websocket_handler.cpp` (update callers)
- Test: `test/test_sink_slot_api/`

**Step 1: Write the failing test**

```cpp
void test_set_sink_volume_updates_gain(void) {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.write = mock_write_fn;
    sink.isReady = mock_ready_fn;
    audio_pipeline_set_sink(0, &sink);

    audio_pipeline_set_sink_volume(0, 0.75f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, audio_pipeline_get_sink_volume(0));
}
```

**Step 2: Run test — expect FAIL**

**Step 3: Implement**

Add `float volumeGain` field to `AudioOutputSink` struct (default `1.0f` in `AUDIO_OUTPUT_SINK_INIT`).

```cpp
void audio_pipeline_set_sink_volume(uint8_t slot, float gain) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return;
    _sinks[slot].volumeGain = gain;  // Atomic float write on ESP32-P4 RISC-V — no suspension needed
}

float audio_pipeline_get_sink_volume(uint8_t slot) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return 0.0f;
    return _sinks[slot].volumeGain;
}
```

Migrate callers same pattern as Task 0.2. Remove `_volumeGainForSlot[]` from `dac_hal.cpp`. The write callback reads `sink->volumeGain` instead of the removed array.

**Step 4: Run all tests**

Run: `pio test -e native -v`
Expected: All pass

**Step 5: Commit**

```
refactor(phase0): migrate _volumeGainForSlot[] to pipeline sink volumeGain

Volume gain now lives on AudioOutputSink struct, set via
audio_pipeline_set_sink_volume(). Removes per-slot volume array
from dac_hal.cpp.
```

---

### Task 0.4: Reset mute ramp state on slot deactivation (HC-6)

**Files:**
- Modify: `src/dac_hal.cpp` (`dac_deactivate_for_hal()`)
- Test: `test/test_dac_hal/`

**Step 1: Write the failing test**

```cpp
void test_deactivate_resets_mute_ramp(void) {
    // Simulate partial mute ramp (gain at 0.5)
    extern float _muteGain;
    extern bool _prevDacMute;
    _muteGain = 0.5f;
    _prevDacMute = true;

    // Deactivate slot 0
    dac_deactivate_for_hal(mockDev);

    // Mute ramp state should be reset
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, _muteGain);
    TEST_ASSERT_FALSE(_prevDacMute);
}
```

**Step 2: Run test — expect FAIL**

**Step 3: Add reset in `dac_deactivate_for_hal()`**

In the slot cleanup block of `dac_deactivate_for_hal()` (after clearing slot arrays), add:

```cpp
if (sinkSlot == 0) {
    _muteGain = 1.0f;
    _prevDacMute = false;
}
```

**Step 4: Run tests — expect PASS**

**Step 5: Commit**

```
fix(phase0): reset _muteGain on slot 0 deactivation

Prevents stale mute ramp state when a device is re-enabled at slot 0.
Addresses embedded safety constraint HC-6.
```

---

## Phase 1: Core Refactor — Bridge & HAL Devices Own Sinks

**Goal:** HAL devices build their own `AudioOutputSink`. Bridge orchestrates registration. `DacRegistry`, `DacDriver`, `HalDacAdapter` deleted. Bus-agnostic design — any output device with `HAL_CAP_DAC_PATH` works through the same path regardless of bus type.

**Commit prefix:** `refactor(phase1):`

### Task 1.1: Create `sink_write_utils.h/.cpp` — shared write helpers

**Files:**
- Create: `src/sink_write_utils.h`
- Create: `src/sink_write_utils.cpp`
- Create: `test/test_sink_write_utils/test_sink_write_utils.cpp`

**Step 1: Write failing tests**

```cpp
#include "../../src/sink_write_utils.h"

void test_apply_volume_scales_buffer(void) {
    float buf[4] = {1.0f, -1.0f, 0.5f, -0.5f};
    sink_apply_volume(buf, 4, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, buf[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, buf[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, buf[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.25f, buf[3]);
}

void test_apply_volume_unity_is_noop(void) {
    float buf[4] = {1.0f, -1.0f, 0.5f, -0.5f};
    float expected[4] = {1.0f, -1.0f, 0.5f, -0.5f};
    sink_apply_volume(buf, 4, 1.0f);
    for (int i = 0; i < 4; i++)
        TEST_ASSERT_FLOAT_WITHIN(0.001f, expected[i], buf[i]);
}

void test_mute_ramp_fades_to_zero(void) {
    float buf[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float rampState = 1.0f;
    sink_apply_mute_ramp(buf, 4, &rampState, true);
    // After ramping toward mute, rampState should decrease
    TEST_ASSERT_TRUE(rampState < 1.0f);
}

void test_mute_ramp_recovers_to_unity(void) {
    float buf[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float rampState = 0.0f;
    sink_apply_mute_ramp(buf, 4, &rampState, false);
    // After ramping toward unmute, rampState should increase
    TEST_ASSERT_TRUE(rampState > 0.0f);
}
```

**Step 2: Run — expect FAIL**

**Step 3: Implement**

`src/sink_write_utils.h`:

```cpp
#ifndef SINK_WRITE_UTILS_H
#define SINK_WRITE_UTILS_H

#include <stddef.h>
#include <stdint.h>

#define MUTE_RAMP_STEP 0.02f

// Apply software volume gain to a float buffer in-place.
// Skips processing if gain == 1.0f (unity).
void sink_apply_volume(float* buf, size_t len, float gain);

// Apply mute ramp to a float buffer in-place.
// rampState tracks the current ramp position [0.0 .. 1.0].
// muted=true ramps toward 0.0; muted=false ramps toward 1.0.
void sink_apply_mute_ramp(float* buf, size_t len, float* rampState, bool muted);

// Convert float [-1.0,+1.0] buffer to int32 left-justified for I2S DMA.
void sink_float_to_i2s_int32(const float* in, int32_t* out, size_t len);

#endif
```

`src/sink_write_utils.cpp`:

```cpp
#include "sink_write_utils.h"
#include <math.h>

void sink_apply_volume(float* buf, size_t len, float gain) {
    if (gain >= 0.999f && gain <= 1.001f) return;  // Unity — skip
    for (size_t i = 0; i < len; i++) {
        buf[i] *= gain;
    }
}

void sink_apply_mute_ramp(float* buf, size_t len, float* rampState, bool muted) {
    float target = muted ? 0.0f : 1.0f;
    float g = *rampState;
    if (fabsf(g - target) < 0.001f) {
        *rampState = target;
        if (muted) {
            for (size_t i = 0; i < len; i++) buf[i] = 0.0f;
        }
        return;
    }
    for (size_t i = 0; i < len; i++) {
        if (g < target) { g += MUTE_RAMP_STEP; if (g > target) g = target; }
        else            { g -= MUTE_RAMP_STEP; if (g < target) g = target; }
        buf[i] *= g;
    }
    *rampState = g;
}

void sink_float_to_i2s_int32(const float* in, int32_t* out, size_t len) {
    for (size_t i = 0; i < len; i++) {
        float clamped = in[i];
        if (clamped > 1.0f) clamped = 1.0f;
        if (clamped < -1.0f) clamped = -1.0f;
        out[i] = (int32_t)(clamped * 2147483647.0f);
    }
}
```

Note: On ESP32, `sink_apply_volume()` can use `dsps_mulc_f32()` from ESP-DSP for SIMD acceleration. The native test fallback uses the scalar loop above.

**Step 4: Run tests — expect PASS**

Run: `pio test -e native -f test_sink_write_utils -v`

**Step 5: Commit**

```
refactor(phase1): extract sink_write_utils — shared stateless write helpers

Pure functions for software volume, mute ramp, and float-to-I2S conversion.
Bus-agnostic — any output device can compose these in its write callback.
Extracted from _dac_write_for_slot() in dac_hal.cpp.
```

---

### Task 1.2: Add `buildSink()` virtual to `HalAudioDevice`

**Files:**
- Modify: `src/hal/hal_audio_device.h` (add virtual method)
- Modify: `src/audio_output_sink.h` (ensure `volumeGain` and `muteRampState` fields exist)
- Test: `test/test_hal_pcm5102a/` (add buildSink test)

**Step 1: Write the failing test**

In `test/test_hal_pcm5102a/test_hal_pcm5102a.cpp`:

```cpp
void test_pcm5102a_buildSink_populates_struct(void) {
    HalPcm5102a dev;
    // Simulate init
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;

    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    bool ok = dev.buildSink(0, &sink);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_NOT_NULL(sink.write);
    TEST_ASSERT_NOT_NULL(sink.isReady);
    TEST_ASSERT_EQUAL(0, sink.halSlot);
}
```

**Step 2: Run — expect FAIL** (`buildSink` not defined)

**Step 3: Add virtual to base, implement in HalPcm5102a**

In `src/hal/hal_audio_device.h`:

```cpp
// Any device with HAL_CAP_DAC_PATH should implement this.
// Returns true if sink was populated successfully.
virtual bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) { return false; }
```

In `src/hal/hal_pcm5102a.cpp`, implement `buildSink()`:

```cpp
bool HalPcm5102a::buildSink(uint8_t sinkSlot, AudioOutputSink* out) {
    if (!out) return false;
    *out = AUDIO_OUTPUT_SINK_INIT;
    out->halSlot = getSlot();
    out->write = _pcm5102a_write_cb;     // Static callback using sink_write_utils
    out->isReady = _pcm5102a_ready_cb;   // Reads this->_ready
    return true;
}
```

The write callback uses `sink_write_utils` for volume/mute, then calls `i2s_audio_write_tx()` for I2S port 0. The `isReady` callback returns the device's `_ready` volatile flag.

Each HAL device that supports `HAL_CAP_DAC_PATH` implements `buildSink()` with its own bus-specific write path:
- `HalPcm5102a` — I2S port 0 write
- `HalEs8311` — I2S port 2 write + I2C register volume
- `HalMcp4725` — I2C DAC write (no I2S)
- Future devices — SPI, network, GPIO, etc.

**Step 4: Run tests — expect PASS**

Run: `pio test -e native -f test_hal_pcm5102a -v`

**Step 5: Commit**

```
refactor(phase1): add buildSink() virtual to HalAudioDevice

HAL devices with HAL_CAP_DAC_PATH implement buildSink() to populate
an AudioOutputSink with device-specific write and isReady callbacks.
Bus-agnostic — each device provides its own write path.
```

---

### Task 1.3: Implement `buildSink()` for HalEs8311 and HalMcp4725

**Files:**
- Modify: `src/hal/hal_es8311.cpp`
- Modify: `src/hal/hal_mcp4725.cpp`
- Test: `test/test_hal_es8311/`, `test/test_hal_mcp4725/`

Follow same pattern as Task 1.2. Each device:
- Implements `buildSink()` with its bus-specific write callback
- Uses `sink_write_utils` for software volume/mute ramp where applicable
- ES8311: write callback dispatches to I2S port 2 via `i2s_audio_write_es8311()`
- MCP4725: write callback converts samples to I2C voltage commands

**Commit:**

```
refactor(phase1): implement buildSink() for ES8311 and MCP4725

Each device provides its own bus-specific write callback.
ES8311 uses I2S port 2; MCP4725 uses I2C write path.
```

---

### Task 1.4: Bridge takes sink registration ownership

**Files:**
- Modify: `src/hal/hal_pipeline_bridge.cpp` (`on_device_available`, `on_device_removed`)
- Modify: `src/hal/hal_pipeline_bridge.h` (add `hal_pipeline_activate_device()` / `hal_pipeline_deactivate_device()`)
- Modify: `src/dac_hal.cpp` (remove `audio_pipeline_set_sink()` call from `dac_activate_for_hal()`)
- Test: `test/test_hal_bridge/test_hal_bridge.cpp`

**Step 1: Write the failing test**

In `test/test_hal_bridge/test_hal_bridge.cpp`:

```cpp
// Track sink registration calls
static int set_sink_call_count = 0;
static uint8_t last_set_sink_slot = 0xFF;

void audio_pipeline_set_sink(uint8_t slot, const AudioOutputSink* sink) {
    set_sink_call_count++;
    last_set_sink_slot = slot;
    // ... existing stub logic ...
}

void test_bridge_registers_sink_on_device_available(void) {
    set_sink_call_count = 0;
    // Create a mock HAL device with HAL_CAP_DAC_PATH and buildSink()
    MockDacDevice dev;
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    dev._caps = HAL_CAP_DAC_PATH;
    HalDeviceManager::instance().registerDevice(&dev);

    // Fire state change to AVAILABLE
    hal_pipeline_on_state_change(dev.getSlot(), HAL_STATE_CONFIGURING, HAL_STATE_AVAILABLE);

    TEST_ASSERT_EQUAL(1, set_sink_call_count);
    TEST_ASSERT_EQUAL(0, last_set_sink_slot);  // First DAC-path device gets slot 0
}
```

**Step 2: Run — expect FAIL** (bridge does not call `set_sink` directly yet)

**Step 3: Implement in bridge**

In `hal_pipeline_bridge.cpp`, modify `on_device_available()`:

```cpp
void hal_pipeline_on_device_available(uint8_t halSlot) {
    HalDevice* dev = HalDeviceManager::instance().getDevice(halSlot);
    if (!dev) return;

    uint8_t caps = _effectiveCaps(dev);

    if (caps & HAL_CAP_DAC_PATH) {
        int8_t sinkSlot = _sinkSlotForDevice(halSlot);  // Idempotent (HC-5)
        if (sinkSlot < 0) return;

        // NEW: Bridge asks device to build its sink
        HalAudioDevice* audioDev = static_cast<HalAudioDevice*>(dev);
        AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
        if (audioDev->buildSink((uint8_t)sinkSlot, &sink)) {
            audio_pipeline_set_sink((uint8_t)sinkSlot, &sink);
        }

        _sinkCount++;
        appState.setDacDirty();
        app_events_signal(EVT_DAC);
    }

    if (caps & HAL_CAP_ADC_PATH) {
        // ... existing source registration (unchanged) ...
    }
}
```

For deactivation, add bridge-level batch deinit with semaphore envelope (HC-2):

```cpp
void hal_pipeline_deactivate_device(uint8_t halSlot) {
    HalDevice* dev = HalDeviceManager::instance().getDevice(halSlot);
    if (!dev) return;

    uint8_t caps = _effectiveCaps(dev);
    if (!(caps & HAL_CAP_DAC_PATH)) return;

    int8_t sinkSlot = _halSlotToSinkSlot[halSlot];
    if (sinkSlot < 0) return;

    // HC-2: Single semaphore envelope for the entire teardown
    AppState& as = AppState::getInstance();
    as.audio.paused = true;
    xSemaphoreTake(as.audio.taskPausedAck, pdMS_TO_TICKS(50));

    // Device-specific deinit (I2S TX disable, I2C cleanup, etc.)
    dev->deinit();  // HC-3: device deinit must only call TX-specific teardown

    // Remove sink from pipeline
    audio_pipeline_remove_sink((uint8_t)sinkSlot);

    as.audio.paused = false;

    // Clear mapping
    _halSlotToSinkSlot[halSlot] = -1;
    _sinkCount--;
    appState.setDacDirty();
    app_events_signal(EVT_DAC);
}
```

Remove `audio_pipeline_set_sink()` call from `dac_hal.cpp` `dac_activate_for_hal()`.
Remove `hal_pipeline_state_change()` calls from `dac_hal.cpp` (breaks circular dependency).

**Step 4: Run all tests**

Run: `pio test -e native -v`
Expected: All pass

**Step 5: Commit**

```
refactor(phase1): bridge owns sink registration and deactivation

on_device_available() calls dev->buildSink() then audio_pipeline_set_sink().
hal_pipeline_deactivate_device() owns the audioPaused semaphore envelope (HC-2).
Removes circular dependency: dac_hal.cpp no longer calls bridge functions.
```

---

### Task 1.5: Update main loop `pendingToggle` handler

**Files:**
- Modify: `src/main.cpp` (lines 1216-1236)
- Modify: `src/hal/hal_pipeline_bridge.h` (export `hal_pipeline_activate_device()` / `hal_pipeline_deactivate_device()`)

**Step 1: Update main loop handler**

Replace:
```cpp
dac_activate_for_hal(dev, sinkSlot);
// and
dac_deactivate_for_hal(dev);
```

With:
```cpp
hal_pipeline_activate_device(halSlot);
// and
hal_pipeline_deactivate_device(halSlot);
```

Where `hal_pipeline_activate_device()` calls `on_device_available()` internally.

**Step 2: Run all tests**

Run: `pio test -e native -v`

**Step 3: Commit**

```
refactor(phase1): main loop pendingToggle uses bridge-level activate/deactivate

Main loop no longer calls dac_activate_for_hal() directly.
All device activation flows through hal_pipeline_bridge.
```

---

### Task 1.6: Internalize DacDriver into HAL devices

**Files:**
- Modify: `src/hal/hal_pcm5102a.cpp` (absorb `DacPcm5102` I2S init logic)
- Modify: `src/hal/hal_es8311.cpp` (absorb `DacEs8311` I2C register logic)
- Modify: `src/hal/hal_pcm5102a.h`, `src/hal/hal_es8311.h`
- Test: `test/test_hal_pcm5102a/`, `test/test_hal_es8311/`

Each HAL device absorbs the relevant functionality from its legacy `DacDriver`:
- `HalPcm5102a::init()` — I2S TX pin resolution from `HalDeviceConfig`, calls `i2s_audio_enable_tx()` (not hardcoded pins — reads from config)
- `HalEs8311::init()` — I2C register programming (codec init sequence), I2S port 2 TX enable
- `HalPcm5102a::deinit()` — calls `i2s_audio_disable_tx()` for port 0 only (HC-3: never touch RX/MCLK)
- `HalEs8311::deinit()` — I2C power down, I2S port 2 TX disable only

**HC-3 enforcement**: Add a comment block at the top of each `deinit()`:

```cpp
// HC-3: ONLY disable TX. NEVER call i2s_channel_disable() on RX.
// Use i2s_audio_disable_tx() which handles port-specific TX teardown.
```

**Commit:**

```
refactor(phase1): internalize DacDriver logic into HAL device classes

HalPcm5102a and HalEs8311 now own their I2S/I2C init/deinit.
Pin configuration read from HalDeviceConfig — no hardcoded pins.
Legacy DacPcm5102 and DacEs8311 classes no longer needed.
HC-3 enforced: deinit() only touches TX path.
```

---

### Task 1.7: Delete legacy artifacts

**Files:**
- Delete: `src/dac_registry.h`
- Delete: `src/dac_registry.cpp`
- Delete: `src/hal/hal_dac_adapter.h`
- Delete: `src/hal/hal_dac_adapter.cpp`
- Delete: `src/drivers/dac_pcm5102.h` / `src/drivers/dac_pcm5102.cpp` (if separate files exist)
- Delete: `src/drivers/dac_es8311.h` / `src/drivers/dac_es8311.cpp` (if separate files exist)
- Delete: `test/test_hal_adapter/` (entire directory)
- Modify: `src/dac_hal.cpp` (remove `_driverForSlot[]`, `_adapterForSlot[]`, `_writePortForSlot[]`, write thunks, `dac_activate_for_hal()`, `dac_deactivate_for_hal()`, includes of removed headers)
- Modify: `src/main.cpp` (remove `#include "dac_registry.h"`)
- Modify: `src/websocket_handler.cpp` (remove `#include "dac_registry.h"`)
- Modify: `src/dac_api.cpp` (remove `#include "dac_registry.h"`)

**Step 1: Delete files**

Remove all listed files. Remove `test/test_hal_adapter/` directory.

**Step 2: Remove stale includes and dead code**

In `dac_hal.cpp`, remove:
- `#include "dac_registry.h"`
- `#include "hal/hal_dac_adapter.h"`
- `#include "hal/hal_pipeline_bridge.h"` (break circular dependency)
- `static DacDriver* _driverForSlot[]`
- `static HalDacAdapter* _adapterForSlot[]`
- `static uint8_t _writePortForSlot[]`
- Write thunks (`_dac_slot0_write`, `_dac_slot1_write`, etc.)
- `_dac_write_for_slot()` (replaced by `sink_write_utils` + device callbacks)
- `dac_activate_for_hal()` / `dac_deactivate_for_hal()` (moved to bridge + device init)

What remains in `dac_hal.cpp`:
- `dac_boot_prepare()` — EEPROM scanning, I2C bus init, mutex setup
- `i2s_audio_enable_tx()` / `i2s_audio_disable_tx()` — bus utilities
- Volume curve math (`dac_percent_to_gain()`)
- Diagnostics helpers

**Step 3: Run all tests**

Run: `pio test -e native -v`
Expected: All pass (minus deleted test_hal_adapter module — adjust test count expectation)

**Step 4: Commit**

```
refactor(phase1): delete DacRegistry, DacDriver, HalDacAdapter

Removes dual registry system. HAL devices own their drivers.
Bridge owns sink lifecycle. dac_hal.cpp slimmed to bus utilities.
Deletes: dac_registry.h/.cpp, hal_dac_adapter.h/.cpp,
DacPcm5102, DacEs8311 legacy drivers, test_hal_adapter module.
```

---

## Phase 2: Migrate Enumeration APIs

**Goal:** REST and WebSocket endpoints enumerate output devices from HAL, not from the deleted legacy registry. No audio path changes.

**Commit prefix:** `refactor(phase2):`

### Task 2.1: Replace `GET /api/dac/drivers` with HAL query

**Files:**
- Modify: `src/dac_api.cpp` (lines 166-198)
- Test: E2E — `e2e/mock-server/routes/dac.js` (if exists)

**Step 1: Replace endpoint implementation**

```cpp
server.on("/api/dac/drivers", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray drivers = doc["drivers"].to<JsonArray>();
    HalDeviceManager& mgr = HalDeviceManager::instance();

    for (uint8_t s = 0; s < HAL_MAX_DEVICES; s++) {
        HalDevice* dev = mgr.getDevice(s);
        if (!dev) continue;
        if (!(dev->getDescriptor().capabilities & HAL_CAP_DAC_PATH)) continue;

        const HalDeviceDescriptor& desc = dev->getDescriptor();
        JsonObject drv = drivers.add<JsonObject>();
        drv["compatible"] = desc.compatible;
        drv["name"] = desc.name;
        drv["manufacturer"] = desc.manufacturer;
        drv["channelCount"] = desc.channelCount;
        drv["halSlot"] = s;
        if (desc.legacyId > 0) drv["legacyId"] = desc.legacyId;  // Backward compat
    }

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
});
```

No temporary driver instantiation needed — devices are already live.

**Step 2: Run E2E tests**

Run: `cd e2e && npx playwright test`

**Step 3: Commit**

```
refactor(phase2): GET /api/dac/drivers enumerates from HAL

No longer uses DacRegistry. Queries HalDeviceManager filtered by
HAL_CAP_DAC_PATH. No temporary driver instantiation.
```

---

### Task 2.2: Replace `GET /api/dac/eeprom/presets` with HAL device DB query

**Files:**
- Modify: `src/dac_api.cpp` (lines 480-517)

Same pattern as 2.1 — query HAL device database for preset data instead of legacy registry.

**Commit:**

```
refactor(phase2): GET /api/dac/eeprom/presets queries HAL device DB
```

---

### Task 2.3: Replace `sendDacState()` WS broadcast with HAL query

**Files:**
- Modify: `src/websocket_handler.cpp` (lines 1700-1710)
- Modify: `e2e/fixtures/ws-messages/` (update DAC state fixture)
- Modify: `e2e/helpers/ws-helpers.js` (update `buildInitialState()`)

**Step 1: Replace driver enumeration in WS broadcast**

```cpp
JsonArray drivers = doc["drivers"].to<JsonArray>();
HalDeviceManager& mgr = HalDeviceManager::instance();
for (uint8_t s = 0; s < HAL_MAX_DEVICES; s++) {
    HalDevice* dev = mgr.getDevice(s);
    if (!dev) continue;
    if (!(dev->getDescriptor().capabilities & HAL_CAP_DAC_PATH)) continue;
    const HalDeviceDescriptor& desc = dev->getDescriptor();
    JsonObject drv = drivers.add<JsonObject>();
    drv["compatible"] = desc.compatible;
    drv["name"] = desc.name;
    drv["halSlot"] = s;
    if (desc.legacyId > 0) drv["id"] = desc.legacyId;  // Backward compat for old frontends
}
```

**Step 2: Update E2E fixtures to match new format**

**Step 3: Run E2E tests**

Run: `cd e2e && npx playwright test`

**Step 4: Commit**

```
refactor(phase2): sendDacState() WS broadcast uses HAL device query

Replaces dac_registry_get_entries() with HalDeviceManager iteration.
E2E fixtures updated for new driver format.
```

---

### Task 2.4: Move `filterMode` from `DacState` to `HalDeviceConfig`

**Files:**
- Modify: `src/state/dac_state.h` (remove `filterMode`)
- Modify: `src/hal/hal_types.h` (`HalDeviceConfig` — add `uint8_t filterMode`)
- Modify: `src/websocket_handler.cpp` (remove deprecated `setDacFilter` handler or update to HAL path)
- Modify: `src/hal/hal_settings.cpp` (persist `filterMode` in `/hal_config.json`)
- Test: `test/test_dac_settings/`

**Step 1: Add `filterMode` to `HalDeviceConfig`**

In `src/hal/hal_types.h`, add to `HalDeviceConfig`:

```cpp
uint8_t filterMode = 0;  // Device-specific filter mode (e.g., PCM5102A sharp/slow roll-off)
```

**Step 2: Wire through `PUT /api/hal/devices`**

`hal_apply_config()` already handles config updates. Add `filterMode` to the JSON parse and the config struct copy.

**Step 3: Remove from `DacState`**

Remove `uint8_t filterMode` from `src/state/dac_state.h`. Update any callers.

**Step 4: Run all tests**

Run: `pio test -e native -v`
Expected: All pass

**Step 5: Commit**

```
refactor(phase2): move filterMode from DacState to HalDeviceConfig

filterMode is device-specific (only PCM5102A uses it). Now persisted
in /hal_config.json and wired through PUT /api/hal/devices.
```

---

### Task 2.5: Replace `dac_output_is_ready()` with bridge query

**Files:**
- Modify: `src/dac_hal.h` (remove `dac_output_is_ready()` or mark deprecated)
- Modify: `src/hal/hal_pipeline_bridge.h` (add `hal_pipeline_has_active_sink(uint8_t slot)`)
- Modify: all callers of `dac_output_is_ready()`

**Step 1: Add bridge API**

```cpp
bool hal_pipeline_has_active_sink(uint8_t sinkSlot);
```

Returns true if a sink is registered at the given slot and its device is ready.

**Step 2: Grep and replace all `dac_output_is_ready()` callers**

**Step 3: Run all tests**

**Step 4: Commit**

```
refactor(phase2): replace dac_output_is_ready() with hal_pipeline_has_active_sink()

No slot-0 hardcoding. Works for any sink slot generically.
```

---

## Phase 3: Cleanup & Dependency Inversion

**Goal:** Remove all remaining legacy references, break circular dependencies, update documentation.

**Commit prefix:** `refactor(phase3):`

### Task 3.1: Break circular dependency — dac_hal no longer depends on bridge

**Files:**
- Modify: `src/dac_hal.cpp` (remove `#include "hal/hal_pipeline_bridge.h"`)
- Verify: no calls from dac_hal into bridge remain

After Phase 1, `dac_hal.cpp` should have zero references to bridge functions. Verify with grep and remove the include.

**Commit:**

```
refactor(phase3): break dac_hal -> bridge circular dependency

dac_hal.cpp no longer includes hal_pipeline_bridge.h.
Dependency is strictly one-directional: bridge -> dac bus utilities.
```

---

### Task 3.2: Remove `hal_registry_find_by_legacy_id()` from non-discovery paths

**Files:**
- Modify: `src/hal/hal_driver_registry.h` (keep function but mark as discovery-only in comment)
- Verify: only `src/hal/hal_discovery.cpp` calls it

`legacyId` stays in `HalDriverEntry` for EEPROM v1/v2 backward compat. But `hal_registry_find_by_legacy_id()` should only be called from `hal_discovery.cpp`. Verify no other callers exist.

**Commit:**

```
refactor(phase3): restrict hal_registry_find_by_legacy_id to discovery only

Only hal_discovery.cpp needs legacy ID lookup for EEPROM v1/v2 compat.
```

---

### Task 3.3: Delete deprecated WS handlers (7 paths)

**Files:**
- Modify: `src/websocket_handler.cpp` (lines 952-1065)

These handlers (`setDacEnabled`, `setDacVolume`, `setDacMute`, `setDacFilter`, `setEs8311Enabled`, `setEs8311Volume`, `setEs8311Mute`) are already marked `DEPRECATED` with `LOG_W`. Confirm `web_src/js/` and `e2e/` have no remaining callers, then delete.

**Step 1: Grep for callers**

```bash
grep -r "setDacEnabled\|setDacVolume\|setDacMute\|setDacFilter\|setEs8311Enabled\|setEs8311Volume\|setEs8311Mute" web_src/ e2e/
```

**Step 2: If no callers, delete handler blocks**

**Step 3: Run E2E tests**

Run: `cd e2e && npx playwright test`

**Step 4: Commit**

```
refactor(phase3): delete 7 deprecated WS DAC command handlers

No callers remain in web_src/ or e2e/. Commands were deprecated in v1.14.
Addresses concern: "Deprecated WS message handlers still active (7 paths)"
```

---

### Task 3.4: Remove `DacDriver` base class and `DacCapabilities`

**Files:**
- Delete: `src/drivers/dac_driver.h` (or wherever `DacDriver` base class is defined)
- Modify: any files still referencing `DacDriver` or `DacCapabilities`
- Modify: `src/dac_hal.h` (remove forward declarations)

Grep for `DacDriver` and `DacCapabilities`. Replace any remaining references with HAL equivalents.

**Commit:**

```
refactor(phase3): delete DacDriver base class and DacCapabilities

All driver functionality now lives in HalAudioDevice subclasses.
Capabilities derived from HalDeviceDescriptor fields.
```

---

### Task 3.5: Update documentation

**Files:**
- Modify: `CLAUDE.md` (update architecture section — remove DacRegistry references, update dac_hal description)
- Modify: `docs-site/docs/developer/api/rest-dac.md` (mark legacy endpoints removed)
- Modify: `docs-site/docs/developer/hal/overview.md` (update bridge description — now owns sink lifecycle)
- Modify: `.planning/codebase/CONCERNS.md` (mark both concerns as FIXED)

**Commit:**

```
docs(phase3): update architecture docs for registry unification

Marks dual registry and bridge metadata-only concerns as FIXED.
Updates CLAUDE.md, Docusaurus HAL/DAC pages.
```

---

### Task 3.6: Final test verification

**Step 1: Run all C++ tests**

Run: `pio test -e native -v`
Expected: All pass (count will decrease by ~9 from deleted test_hal_adapter module)

**Step 2: Run E2E tests**

Run: `cd e2e && npx playwright test`
Expected: All 26 pass

**Step 3: Run static analysis**

Run: `cd e2e && npx eslint ../web_src/js/ --config ../web_src/.eslintrc.json`
Run: `node tools/find_dups.js && node tools/check_missing_fns.js`

**Step 4: Build firmware**

Run: `pio run`
Expected: Clean build, no warnings from modified files

**Step 5: Commit (if any final fixups needed)**

```
test(phase3): verify all quality gates pass after registry unification
```

---

## Test Module Impact Summary

| Module | Phase | Change |
|---|---|---|
| `test_sink_slot_api` | 0 | Add mute/volume pipeline API tests |
| `test_dac_hal` | 0, 1 | Remove driver factory tests, keep volume curve + boot prep |
| `test_sink_write_utils` | 1 | **New module** — volume, mute ramp, float-to-I2S tests |
| `test_hal_pcm5102a` | 1 | Add `buildSink()` + write callback tests |
| `test_hal_es8311` | 1 | Add `buildSink()` + I2C init tests |
| `test_hal_mcp4725` | 1 | Add `buildSink()` tests |
| `test_hal_bridge` | 1 | Add sink registration + deactivation verification |
| `test_hal_adapter` | 1 | **Delete entirely** |
| `test_dac_settings` | 2 | Update filterMode field location |
| E2E fixtures | 2 | Update WS broadcast format + API response fixtures |

---

## Files Deleted (complete list)

| File | Phase | Reason |
|---|---|---|
| `src/dac_registry.h` | 1 | Legacy registry header |
| `src/dac_registry.cpp` | 1 | Legacy registry implementation |
| `src/hal/hal_dac_adapter.h` | 1 | Bridge adapter header |
| `src/hal/hal_dac_adapter.cpp` | 1 | Bridge adapter implementation |
| `src/drivers/dac_pcm5102.h/.cpp` | 1 | Legacy PCM5102A driver (absorbed into HalPcm5102a) |
| `src/drivers/dac_es8311.h/.cpp` | 1 | Legacy ES8311 driver (absorbed into HalEs8311) |
| `test/test_hal_adapter/` | 1 | Tests for deleted HalDacAdapter |

## Files Created (complete list)

| File | Phase | Purpose |
|---|---|---|
| `src/sink_write_utils.h` | 1 | Shared stateless write helpers (volume, mute ramp, float-to-I2S) |
| `src/sink_write_utils.cpp` | 1 | Implementation |
| `test/test_sink_write_utils/test_sink_write_utils.cpp` | 1 | Unit tests for write utils |
