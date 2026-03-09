# DEBT-3 Plan Review: Route All DAC Init Through HAL
**Conducted:** 2026-03-09
**Plan:** `sequential-skipping-dolphin.md`
**New Architecture:** Modular AppState with `state/dac_state.h`
**Reviewer Role:** Senior Code Reviewer

---

## Executive Summary

**Recommendation: PROCEED WITH MODIFICATIONS to Phase 3**

The DEBT-3 plan is **fundamentally sound** and well-scoped. However, the **new modular AppState architecture has invalidated one critical assumption** in Phase 3, creating an opportunity for simplification.

**Status by Phase:**
- **Phase 1 (Extract boot ops):** GREEN — Zero architectural conflicts
- **Phase 2 (Bridge-triggered activation):** GREEN — Perfect alignment with new architecture
- **Phase 3 (Unified toggle):** YELLOW — Plan's generic `PendingDeviceToggle` is **over-engineered** given the new `state/dac_state.h` structure already has validated setters
- **Phase 4 (Cleanup):** GREEN — Straightforward once phases 1-2 complete

---

## 1. Plan Assumption Validation

### Assumption: AppState is Monolithic
**Reality:** ✓ PARTIALLY CORRECT (but architected differently than expected)

The plan assumes AppState would be a single large class requiring decomposition. Instead:
- AppState **IS now modular via composition**, not via inheritance or separate files
- `src/state/dac_state.h` contains `struct DacState` with all DAC-related fields
- AppState includes it as `DacState dac;` (line 15 in app_state.h shows `#include "state/dac_state.h"`)
- Already completed: **modular state design is the new baseline**

**Impact:** No changes needed to Phase 1 or Phase 2. Phase 3 assumption needs revision.

### Current Toggle Implementation

**In `state/dac_state.h` (lines 48-59):**
```cpp
// Deferred toggle flags — main loop executes actual init/deinit
volatile int8_t pendingEs8311Toggle = 0;  // 0=none, 1=init, -1=deinit
volatile int8_t pendingDacToggle = 0;     // 0=none, 1=init, -1=deinit

// Validated setters — direct dev->deinit() is unsafe (audio task race),
// so all toggle requests go through deferred flags consumed by main loop
void requestDacToggle(int8_t action) {
  if (action >= -1 && action <= 1) pendingDacToggle = action;
}
void requestEs8311Toggle(int8_t action) {
  if (action >= -1 && action <= 1) pendingEs8311Toggle = action;
}
```

**Status:** Already exists and is **working correctly**. The plan's Phase 3 calls for unifying these, but they are already:
1. Validated (input guard: `-1, 0, 1` only)
2. Deferred (set by WebSocket/HTTP handlers, consumed by main loop)
3. Per-device (separate flags for primary/secondary)

---

## 2. Phase 3 Feasibility Assessment: SIMPLIFICATION NEEDED

### Plan Proposes: Generic `PendingDeviceToggle` Struct

The plan (Phase 3, lines 197-202) suggests:
```cpp
struct PendingDeviceToggle {
    uint8_t halSlot = 0xFF;  // 0xFF = none
    int8_t action = 0;       // 1=enable, -1=disable
};
volatile PendingDeviceToggle _pendingDeviceToggle = {};
```

**Problem with This Approach:**
1. **Over-indexes on HAL slot** — The main loop toggle handler (lines 1207-1229 in main.cpp) works perfectly with **two separate flags** because:
   - Primary DAC (`pendingDacToggle`) always uses hardcoded `dac_output_init()` / `dac_output_deinit()`
   - ES8311 (`pendingEs8311Toggle`) always uses hardcoded `dac_secondary_init()` / `dac_secondary_deinit()`
   - **No device lookup is needed** — the driver is already selected via `dac_load_settings()`

2. **Doesn't match actual implementation** — Phase 1 calls for `dac_activate_for_hal(HalDevice* dev, uint8_t sinkSlot)`, which requires a device pointer. But in the toggle path:
   - The toggle flag is set by WebSocket or HAL API handlers
   - The main loop **does not know which HAL device** the toggle applies to
   - Dereferencing `HalDevice* dev = mgr.findByCompatible(...)` in the main loop adds complexity

3. **Breaks the clean separation** — Toggle flags belong in AppState because they coordinate UI→Main Loop. HAL device details should stay in HAL layer.

### Recommended Simplification: Keep Separate Flags, Adapt Phase 1

**Instead of Phase 3 overhaul, do this:**

1. Keep `pendingDacToggle` and `pendingEs8311Toggle` as-is
2. In Phase 1, create wrapper functions:
```cpp
// NEW (in dac_hal.cpp)
void dac_activate_primary() {
    // Same as dac_output_init() but internally calls dac_activate_for_hal()
    dac_activate_for_hal(/* primary DAC device */, 0);
}

void dac_deactivate_primary() {
    // Same as dac_output_deinit() but internally calls dac_deactivate_for_hal()
    dac_deactivate_for_hal(/* primary DAC device */);
}

// For secondary
void dac_activate_secondary() { dac_activate_for_hal(/* ES8311 device */, 1); }
void dac_deactivate_secondary() { dac_deactivate_for_hal(/* ES8311 device */); }
```

3. Main loop toggle handler stays simple (lines 1207-1229):
```cpp
if (appState.dac.pendingDacToggle > 0) {
    dac_activate_primary();  // NEW wrapper
} else if (appState.dac.pendingDacToggle < 0) {
    dac_deactivate_primary();
}
```

**Advantages:**
- Maintains existing test coverage (`test_deferred_toggle`)
- No change to AppState structure
- Phase 1 refactoring stays focused on extracting boot ops
- Toggle path remains lightweight (no device lookups)

---

## 3. Risk Assessment: Modular Architecture Impact

### Risk Category: LOW

The new modular state design **REDUCES risk** for Phases 1-2:

#### Advantage 1: Scope Isolation
**Before:** `dac_hal.cpp` would have had to know about `appState._pendingDacToggle` scattered throughout AppState
**After:** Localized reference to `appState.dac.pendingDacToggle` — single struct import

#### Advantage 2: Composition Safety
The `DacState` struct is **immutable in layout** (no risk of field shifts during refactoring):
```cpp
struct DacState {
  // ... 10 existing fields ...
  volatile int8_t pendingEs8311Toggle = 0;
  volatile int8_t pendingDacToggle = 0;
  // Validators built-in
};
```

If Phase 1 adds new DAC state fields (e.g., `_adapterForSlot[]`), they go in **`dac_hal.cpp` statics**, not AppState.

#### Advantage 3: Test Infrastructure Ready
`test/test_deferred_toggle/test_deferred_toggle.cpp` already has 6 passing tests:
- ✓ `test_requestDacToggle_enable_sets_flag_to_1`
- ✓ `test_requestDacToggle_disable_sets_flag_to_minus1`
- ✓ `test_requestDacToggle_clear_sets_flag_to_0`
- ✓ `test_requestDacToggle_rejects_invalid_values`
- ✓ `test_requestEs8311Toggle_enable_sets_flag_to_1`
- ✓ `test_requestEs8311Toggle_disable_sets_flag_to_minus1`

These tests will **pass unchanged** through all phases because the flag interface is stable.

### Risk Category: MEDIUM

#### Risk A: DAC Adapter Slot Mapping

**Phase 1 introduces** `static HalDacAdapter* _adapterForSlot[AUDIO_OUT_MAX_SINKS]` (line 64 of plan)

**Current state:** Only 2 hardcoded adapters exist (`_halPrimaryAdapter`, `_halSecondaryAdapter`)

**New risk:** If 3+ DAC devices are hotplugged, slot assignments may collide or fragment

**Mitigation (already in place):**
- `HAL_MAX_DEVICES=24` (plenty of space)
- `_sinkSlotForDevice()` in bridge does ordinal counting — deterministic
- `audio_output_sink.h` has slot constants (being removed in Phase 4)
- Test: `test/test_hal_bridge/test_hal_bridge.cpp` already verifies slot assignment

**Verdict:** ACCEPTABLE. No special action needed.

#### Risk B: I2S TX Delegation During Toggle

**Phase 2 calls bridge's `on_device_available()` → triggers `dac_activate_for_hal()`**
**Current line 1210 in main.cpp:** `dac_output_init()` calls `dac_enable_i2s_tx()` (line 152 in dac_hal.cpp)

**New risk:** If bridge runs during HAL init (in a HAL callback), calling `dac_enable_i2s_tx()` from callback context may block WiFi SDIO

**Mitigation:**
- Bridge callbacks are **called from task context only** (during `hal_pipeline_sync()` in main.cpp, or HAL discovery task)
- NOT called from ISR, interrupt handler, or WiFi callback
- Safe to call blocking I2C/I2S ops

**Verdict:** ACCEPTABLE. Existing SDIO guards are sufficient (WiFi scan skips Bus 0).

---

## 4. Critical Blockers: NONE IDENTIFIED

### Phase 1 Blockers: ✓ CLEAR
- `dac_boot_prepare()` extraction is pure refactoring — no new APIs needed
- `_adapterForSlot[]` addition is internal to `dac_hal.cpp` — no public impact
- `dac_activate_for_hal()` signature matches bridge callback expectations

### Phase 2 Blockers: ✓ CLEAR
- Bridge `on_device_available()` already has the hook point (lines 139-148)
- No circular dependency (bridge ← dac_hal, not vice versa)
- `#ifndef NATIVE_TEST` guards already present for conditional compilation

### Phase 3 Blockers: ARCHITECTURAL (see section 2)
- **Not a blocker, but requires simplification**
- Generic toggle struct is unnecessary given two separate flags already work
- Recommend keeping flags, adding wrappers instead

### Phase 4 Blockers: ✓ CLEAR
- Test updates are straightforward once dead code is deleted
- New test file `test/test_dac_lifecycle/test_dac_lifecycle.cpp` is greenfield (no conflicts)

---

## 5. Top 3 Risks Specific to New Architecture

### Risk 1: Cross-Module State Consistency (MEDIUM)
**Scenario:** `appState.dac.enabled` is set by HAL layer, but `_halPrimaryAdapter != nullptr` is the source-of-truth in dac_hal.cpp

**Current:** Lines 115-116 in dac_periodic_log() check `as.dac.enabled` to decide whether to log

**Phase 1 introduces:** `_adapterForSlot[slot]` as the real readiness indicator

**Mitigation:**
- After Phase 1, introduce a helper: `dac_is_slot_active(uint8_t slot)` that checks the adapter array
- Use that helper instead of `appState.dac.enabled` for internal decisions
- Keep `appState.dac.enabled` for WS/MQTT broadcasts only (user-facing state)

**Action:** Add validation step after Phase 1 to ensure `appState.dac.enabled == (_adapterForSlot[0] != nullptr)`

### Risk 2: HAL Device Hotplug During Audio Playback (MEDIUM)
**Scenario:** User toggles DAC on/off while audio is playing. Audio task reads `_adapterForSlot[slot]` while main loop writes it.

**Current safeguard:** Volatile pointer (atomic read on ARM)

**Phase 2 changes:** Bridge calls `dac_activate_for_hal()` which modifies `_adapterForSlot[slot]`

**Mitigation:**
- Audio task holds `vTaskSuspendAll()` during `dac_output_write()` callback (already in place per CLAUDE.md)
- Main loop toggle handler also suspends tasks before calling activate/deactivate
- **Verify:** Both paths use `vTaskSuspendAll()` consistently

**Action:** Add comment in Phase 1 code documenting the locking model

### Risk 3: Sink Slot Assignment Collision (LOW)
**Scenario:** `_sinkSlotForDevice()` ordinal counting skips slots based on device type. If 8 DAC devices register, slots 0-7 are consumed. A 9th DAC can't register.

**Current:** Only 2 DAC devices exist (PCM5102A, ES8311). `AUDIO_OUT_MAX_SINKS=8` ✓

**Phase 1/2 changes:** Support N DAC devices (user-configurable expansion)

**Mitigation:**
- `HAL_MAX_DEVICES=24` prevents unbounded device growth
- Document: "Max 8 simultaneous DAC outputs due to sink slot array"
- Add validation in bridge: reject device registration if all 8 slots occupied

**Action:** Add defensive check in Phase 2: `if (sinkSlot >= AUDIO_OUT_MAX_SINKS) return error;`

---

## 6. Recommendation: Proceed with Conditional Changes

### Recommendation Summary
✅ **Proceed with Phases 1, 2, and 4 as-is**
🔄 **Modify Phase 3: Keep separate flags, add wrappers**
📋 **Add 3 validation tests before merging**

### Exact Changes to Plan

**Phase 3 Replacement (Simplified):**

**In `src/dac_hal.h`, add:**
```cpp
// Wrappers for deferred toggle path (simpler than generic PendingDeviceToggle)
void dac_activate_primary();   // Calls dac_activate_for_hal() for primary DAC
void dac_deactivate_primary(); // Calls dac_deactivate_for_hal() for primary DAC
void dac_activate_secondary();   // Calls dac_activate_for_hal() for ES8311
void dac_deactivate_secondary(); // Calls dac_deactivate_for_hal() for ES8311
```

**In `src/main.cpp` (line 1208-1229), replace with:**
```cpp
if (appState.dac.pendingDacToggle > 0) {
    dac_activate_primary();
} else if (appState.dac.pendingDacToggle < 0) {
    dac_deactivate_primary();
}
appState.dac.pendingDacToggle = 0;

if (appState.dac.pendingEs8311Toggle > 0) {
    dac_activate_secondary();
} else if (appState.dac.pendingEs8311Toggle < 0) {
    dac_deactivate_secondary();
}
appState.dac.pendingEs8311Toggle = 0;
```

**NO changes to `src/app_state.h` or `state/dac_state.h`** — keep existing toggle flags.

---

## 7. Implementation Checklist

- [ ] **Phase 1:** Extract boot ops, introduce `_adapterForSlot[]`, add test file `test_dac_lifecycle`
  - [ ] Run `pio test -e native` — all 1561 tests pass
  - [ ] Code review: Verify `_adapterForSlot` accesses are atomic (volatile or guarded by vTaskSuspendAll)

- [ ] **Phase 2:** Bridge-triggered activation
  - [ ] Add `#ifndef NATIVE_TEST` guard in `hal_pipeline_on_device_available()` for `dac_activate_for_hal()` call
  - [ ] Remove line 313 from main.cpp: `dac_secondary_init();`
  - [ ] Manual test: Boot with ES8311 enabled → verify audio outputs on both DACs
  - [ ] Hardware test: Safe mode fallback still produces audio

- [ ] **Phase 3 (Simplified):** Add wrapper functions
  - [ ] Implement `dac_activate_primary()`, `dac_deactivate_primary()`, etc. in `dac_hal.cpp`
  - [ ] Update main loop toggle handler (lines 1208-1229)
  - [ ] **No AppState changes** — flags stay as-is
  - [ ] Existing tests pass unchanged

- [ ] **Phase 4:** Remove legacy API
  - [ ] Delete `dac_output_init()`, `dac_secondary_init()`, and 2 old adapter statics
  - [ ] Update test files and fixtures
  - [ ] Run `pio test -e native -v` — 1561+ tests green
  - [ ] Run `cd e2e && npx playwright test` — 26 tests green

- [ ] **Validation Tests (NEW):**
  - [ ] Test: `appState.dac.enabled` matches `_adapterForSlot[0] != nullptr` after toggle
  - [ ] Test: Audio task survives DAC toggle (no deadlock with vTaskSuspendAll)
  - [ ] Test: Sink slot assignment deterministic across multiple device register/unregister cycles

---

## 8. Code Quality Notes

### Strengths of Current Architecture
1. **Deferred toggle pattern is proven** — 6 passing tests, no race conditions observed
2. **Modular state isolates DAC concerns** — AppState composition avoids monolithic bloat
3. **HAL adapter layer bridges legacy code** — `HalDacAdapter` is lightweight and correct
4. **Bridge slot assignment is deterministic** — `_sinkSlotForDevice()` uses ordinal counting

### Areas for Improvement (Post-Phase 4)
1. **Document locking model** — Add comment block in `dac_hal.cpp` explaining vTaskSuspendAll atomicity
2. **Consolidate I2S delegate guard** — Lines 753-777 in current dac_hal.cpp (mentioned in plan) should have a helper function
3. **Add state consistency check** — Periodic (5s) validation: `assert(_adapterForSlot[0] == nullptr || appState.dac.enabled)`

---

## Final Verdict

**Status: GREEN — CLEAR TO PROCEED**

The DEBT-3 plan is well-designed and addresses a real architectural debt (dual code paths for DAC init). The new modular AppState architecture has **eliminated one major refactoring burden** (AppState decomposition) and provides a clean foundation for Phases 1-2.

Phase 3 should be simplified per this review to avoid over-engineering. The existing toggle flag design is already correct for the current use case, and wrapper functions are a lighter-touch solution than introducing a generic `PendingDeviceToggle` struct.

**Estimated effort:** 2-3 days (1 day per phase + 1 day integration testing)
**Test impact:** Add 3 new tests, ~20 LoC each
**Breaking changes:** None (phases 1-3 are backwards-compatible; cleanup happens in phase 4)

