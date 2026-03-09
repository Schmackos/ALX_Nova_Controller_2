# Phase 3 Code Changes: Simplified Toggle Approach

**Status:** Recommended alternative to generic `PendingDeviceToggle` struct
**Reason:** Current flags are already validated and correct; wrappers avoid over-engineering
**Files Changed:** `src/dac_hal.h`, `src/dac_hal.cpp`, `src/main.cpp`

---

## Overview

Instead of introducing a generic toggle struct that requires HAL device lookups, we:
1. Keep existing `appState.dac.pendingDacToggle` and `appState.dac.pendingEs8311Toggle` flags
2. Add simple wrapper functions in `dac_hal.cpp`
3. Update main loop to call wrappers instead of hardcoded init/deinit functions

**Result:** Simpler, more testable, follows established architectural patterns

---

## File 1: src/dac_hal.h (additions)

**Location:** Add after existing declarations (around line 120, before secondary DAC section)

```cpp
// ===== Phase 3: Wrapper Functions for Main Loop Toggle Handler =====
// These wrappers call the new dac_activate_for_hal() / dac_deactivate_for_hal()
// functions introduced in Phase 1, using stored HAL device pointers.
// They simplify the main loop toggle handler by eliminating device lookups.

void dac_activate_primary();     // Primary DAC init (PCM5102A, I2S0)
void dac_deactivate_primary();   // Primary DAC deinit

void dac_activate_secondary();   // Secondary DAC init (ES8311, I2S2)
void dac_deactivate_secondary(); // Secondary DAC deinit
```

---

## File 2: src/dac_hal.cpp (implementation)

**Location:** Add new section after `dac_output_deinit()` (around line 950)

```cpp
// ===== Phase 3: Wrapper Functions for Toggle Handler =====
// Main loop calls these when deferred toggles are pending.
// Each wrapper holds a reference to the corresponding HAL device.
// Avoids device lookups in main loop context.

// Forward reference to the HalDevice pointers stored during HAL init
// (These are set when HalDacAdapter registers the devices)
// NOTE: In Phase 1, we'll expose these via a getter or store them here
// For now, assume HalDevice* pointers are available in dac_hal.cpp

static HalDevice* _halPrimaryDacDevice = nullptr;  // Points to HalPcm5102a
static HalDevice* _halEs8311Device = nullptr;      // Points to HalEs8311

void dac_register_hal_devices(HalDevice* primary, HalDevice* secondary) {
    _halPrimaryDacDevice = primary;
    _halEs8311Device = secondary;
    LOG_D("[DAC] Registered HAL device pointers for toggle handler");
}

void dac_activate_primary() {
#ifndef NATIVE_TEST
    if (!_halPrimaryDacDevice) {
        LOG_W("[DAC] Primary DAC device not registered, cannot activate");
        return;
    }
    int8_t sinkSlot = hal_pipeline_get_sink_slot(_halPrimaryDacDevice->getSlot());
    if (sinkSlot < 0) {
        // Not yet assigned; bridge will do it during next sync
        sinkSlot = 0;  // Default to slot 0 for primary
    }
    LOG_I("[DAC] Activating primary DAC (Phase 3 wrapper)");
    dac_activate_for_hal(_halPrimaryDacDevice, (uint8_t)sinkSlot);
#else
    // Native test stub
    LOG_I("[DAC] Primary DAC activation (stub)");
#endif
}

void dac_deactivate_primary() {
#ifndef NATIVE_TEST
    if (!_halPrimaryDacDevice) {
        LOG_W("[DAC] Primary DAC device not registered, cannot deactivate");
        return;
    }
    LOG_I("[DAC] Deactivating primary DAC (Phase 3 wrapper)");
    dac_deactivate_for_hal(_halPrimaryDacDevice);
#else
    // Native test stub
    LOG_I("[DAC] Primary DAC deactivation (stub)");
#endif
}

void dac_activate_secondary() {
#ifndef NATIVE_TEST
    if (!_halEs8311Device) {
        LOG_W("[DAC] ES8311 device not registered, cannot activate");
        return;
    }
    int8_t sinkSlot = hal_pipeline_get_sink_slot(_halEs8311Device->getSlot());
    if (sinkSlot < 0) {
        sinkSlot = 1;  // Default to slot 1 for secondary
    }
    LOG_I("[DAC] Activating ES8311 secondary DAC (Phase 3 wrapper)");
    dac_activate_for_hal(_halEs8311Device, (uint8_t)sinkSlot);
#else
    // Native test stub
    LOG_I("[DAC] ES8311 DAC activation (stub)");
#endif
}

void dac_deactivate_secondary() {
#ifndef NATIVE_TEST
    if (!_halEs8311Device) {
        LOG_W("[DAC] ES8311 device not registered, cannot deactivate");
        return;
    }
    LOG_I("[DAC] Deactivating ES8311 secondary DAC (Phase 3 wrapper)");
    dac_deactivate_for_hal(_halEs8311Device);
#else
    // Native test stub
    LOG_I("[DAC] ES8311 DAC deactivation (stub)");
#endif
}
```

---

## File 3: src/main.cpp (toggle handler update)

**Location:** Replace lines 1207-1229 (current toggle handler)

**Before:**
```cpp
#ifdef DAC_ENABLED
  // Process deferred DAC enable/disable (set by WebSocket handler —
  // I2C EEPROM scan + I2S driver init is too heavy for WS context, blocks SDIO → WiFi crash)
  if (appState.dac.pendingDacToggle != 0) {
    if (appState.dac.pendingDacToggle > 0) {
      LOG_I("[DAC] Deferred primary DAC init (main loop)");
      dac_output_init();
    } else {
      LOG_I("[DAC] Deferred primary DAC deinit (main loop)");
      dac_output_deinit();
    }
    appState.dac.pendingDacToggle = 0;
    appState.markDacDirty();
  }
  // Process deferred ES8311 enable/disable
  if (appState.dac.pendingEs8311Toggle != 0) {
    if (appState.dac.pendingEs8311Toggle > 0) {
      LOG_I("[DAC] Deferred ES8311 init (main loop)");
      dac_secondary_init();
    } else {
      LOG_I("[DAC] Deferred ES8311 deinit (main loop)");
      dac_secondary_deinit();
    }
    appState.dac.pendingEs8311Toggle = 0;
    appState.markDacDirty();
  }
```

**After:**
```cpp
#ifdef DAC_ENABLED
  // Process deferred DAC enable/disable (set by WebSocket handler —
  // I2C EEPROM scan + I2S driver init is too heavy for WS context, blocks SDIO → WiFi crash)
  // Phase 3: Use wrapper functions that call new dac_activate_for_hal() API
  if (appState.dac.pendingDacToggle != 0) {
    if (appState.dac.pendingDacToggle > 0) {
      dac_activate_primary();
    } else {
      dac_deactivate_primary();
    }
    appState.dac.pendingDacToggle = 0;
    appState.markDacDirty();
  }
  // Process deferred ES8311 enable/disable
  if (appState.dac.pendingEs8311Toggle != 0) {
    if (appState.dac.pendingEs8311Toggle > 0) {
      dac_activate_secondary();
    } else {
      dac_deactivate_secondary();
    }
    appState.dac.pendingEs8311Toggle = 0;
    appState.markDacDirty();
  }
```

---

## Additional Detail: Registering HAL Device Pointers

**Location:** Somewhere in main.cpp after HAL devices are initialized (around line 310-320)

You'll need to call `dac_register_hal_devices()` once, after the HAL builtin devices are registered:

```cpp
// In main.cpp, after hal_builtin_devices_register() or similar
#ifdef DAC_ENABLED
extern void dac_register_hal_devices(HalDevice* primary, HalDevice* secondary);

// ... later in setup() or main initialization ...

// Get the registered HAL devices by searching for PCM5102A and ES8311
HalDevice* pcm5102a = HalDeviceManager::instance().findByCompatible("alx,pcm5102a");
HalDevice* es8311 = HalDeviceManager::instance().findByCompatible("alx,es8311");
dac_register_hal_devices(pcm5102a, es8311);
#endif
```

**Alternative Approach:** If you prefer not to add explicit device registration, the wrapper functions can do a lazy lookup:

```cpp
void dac_activate_primary() {
#ifndef NATIVE_TEST
    if (!_halPrimaryDacDevice) {
        // Lazy initialization on first use
        _halPrimaryDacDevice = HalDeviceManager::instance().findByCompatible("alx,pcm5102a");
    }
    if (!_halPrimaryDacDevice) {
        LOG_W("[DAC] Primary DAC device not found in HAL registry");
        return;
    }
    // ... rest of function
#endif
}
```

Lazy lookup is simpler but slightly slower. Choose based on your preference.

---

## Why This Approach

### vs. Plan's Generic Struct
```cpp
// Plan said:
struct PendingDeviceToggle {
    uint8_t halSlot = 0xFF;
    int8_t action = 0;
};
// Main loop: HalDevice* dev = mgr.findByCompatible(appState.pendingDeviceToggle.halSlot);
```

**Problems:** Requires device lookup every toggle, mixes UI state with HAL details, harder to test

### vs. Direct Function Calls
```cpp
// Could just do:
if (appState.dac.pendingDacToggle > 0) {
    dac_activate_for_hal(primaryDevice, 0);
}
// But main loop doesn't know primaryDevice
```

**Problem:** Requires passing device pointers into main loop (coupling)

### Our Approach: Wrappers + Device Storage
```cpp
// dac_hal.cpp stores device pointers
static HalDevice* _halPrimaryDacDevice = nullptr;

void dac_activate_primary() {
    dac_activate_for_hal(_halPrimaryDacDevice, 0);
}

// Main loop just calls wrapper
if (appState.dac.pendingDacToggle > 0) {
    dac_activate_primary();
}
```

**Benefits:**
1. Simple (no lookups in main loop)
2. Testable (device storage can be mocked)
3. Follows separation of concerns (device details stay in dac_hal.cpp)
4. Extensible (add more devices = add more wrappers, not a complex generic)

---

## Testing Recommendations

### Unit Tests
Add tests in `test/test_deferred_toggle/test_deferred_toggle.cpp`:

```cpp
void test_dac_activate_primary_calls_dac_activate_for_hal(void) {
    // Verify wrapper function calls the HAL activation
    // Mock dac_activate_for_hal, call dac_activate_primary(), assert mock was called
    // This test ensures wrapper integration works
}

void test_dac_activate_primary_safe_without_device(void) {
    // If _halPrimaryDacDevice is nullptr, wrapper should log warning and return gracefully
    // Don't crash, don't call dac_activate_for_hal()
}
```

### Integration Tests
After Phase 1 completes, update `test/test_hal_bridge/test_hal_bridge.cpp`:

```cpp
void test_toggle_handler_activates_primary_dac(void) {
    // Set appState.dac.pendingDacToggle = 1
    // Call the main loop toggle handler (extracted to a testable function)
    // Verify dac_activate_primary() was called and device is now active
}
```

### Hardware Test
1. Boot with `appState.dac.enabled = true` and `appState.dac.es8311Enabled = true`
2. Send WebSocket command to toggle DAC: `{ "cmd": "setDacEnable", "enabled": false }`
3. Verify audio stops
4. Send toggle command again to enable
5. Verify audio resumes

---

## Migration Path

This approach allows **Phase 1 and Phase 2 to be completed independently** of the toggle handler changes:

1. **Phase 1 (Day 1):** Refactor init/deinit into `dac_activate_for_hal()` / `dac_deactivate_for_hal()`
   - Main loop still calls `dac_output_init()` / `dac_secondary_init()`
   - Everything works as before

2. **Phase 2 (Day 2):** Bridge integration
   - HAL devices are registered and available
   - Bridge calls `dac_activate_for_hal()` on device_available
   - Still compatible with Phase 1 main loop

3. **Phase 3 (Day 2.5):** Add wrappers
   - Add `dac_activate_primary()` etc.
   - Call `dac_register_hal_devices()` to populate pointers
   - Update main loop to use wrappers
   - Old `dac_output_init()` function still exists (not deleted yet)

4. **Phase 4 (Day 3):** Cleanup
   - Delete old `dac_output_init()`, `dac_secondary_init()`
   - Wrappers are now the official API
   - Tests updated

---

## Summary of Changes

| File | Changes | Lines |
|------|---------|-------|
| src/dac_hal.h | Add 4 wrapper declarations | +6 |
| src/dac_hal.cpp | Add statics + 4 wrapper implementations + device register function | +80 |
| src/main.cpp | Update toggle handler to use wrappers | +8 (simplified) |
| Total | — | ~94 LoC added |

**No AppState changes** — keeps state/dac_state.h stable and tested

