# Architectural Insights: Modular AppState + DEBT-3

**Date:** 2026-03-09
**Context:** Review of DEBT-3 plan against new modular AppState architecture

---

## How AppState Modularization Changes DEBT-3

### Before (Monolithic AppState)
```
src/app_state.h (1000+ lines)
├── wifi fields
├── mqtt fields
├── dac fields
├── dsp fields
├── audio fields
├── ... everything mixed
```

**Problem:** Adding new state = modifying huge file, risk of field collision

### After (Modular Composition)
```
src/app_state.h (200 lines)
├── #include "state/dac_state.h" → DacState dac;
├── #include "state/dsp_state.h" → DspState dsp;
├── #include "state/audio_state.h" → AudioState audio;
├── ... 10 modular structs
```

**Benefit:** New modules can add state without touching app_state.h

---

## What This Means for DEBT-3

### The Plan Assumed It Would Need To:
1. Extract `_pendingDacToggle` / `_pendingEs8311Toggle` from AppState
2. Introduce generic `PendingDeviceToggle` struct
3. Rewrite toggle logic to look up HAL devices by compatible string

### What Actually Exists:
1. ✓ Flags already extracted into `state/dac_state.h` (composed into AppState)
2. ✓ Flags already validated via `requestDacToggle()` / `requestEs8311Toggle()`
3. ✓ Toggle logic is simple and correct (main loop handles both paths)

### Result:
**Phase 3 doesn't need the generic struct.** The modular design has already solved the composition problem. DEBT-3 can focus purely on routing initialization through HAL, not on refactoring AppState.

---

## Key Architectural Patterns from New Design

### Pattern 1: Composition Over Inheritance
```cpp
// Bad (monolithic)
class AppState : public WiFiState, public DacState, public DspState { };

// Good (composition)
class AppState {
    WifiState wifi;
    DacState dac;
    DspState dsp;
};
```

**Implication for DEBT-3:** DAC state is a clean namespace (`appState.dac.pendingDacToggle`), not mingled with other fields.

---

### Pattern 2: Validated Setters in Leaf Structs
```cpp
// In state/dac_state.h
struct DacState {
  volatile int8_t pendingDacToggle = 0;

  void requestDacToggle(int8_t action) {
    if (action >= -1 && action <= 1) pendingDacToggle = action;
  }
};
```

**Implication for DEBT-3:**
- No need for generic wrapper struct (validation is already there)
- AppState acts as a registry of leaf structs (not a monolithic blob)
- Toggle logic is one concern (AppState composition); DAC activation is another (dac_hal.cpp)

---

### Pattern 3: Dirty Flags in Leaf Structs
```cpp
// In state/dac_state.h (inferred from app_state.h)
// Has dac.enabled, dac.ready, dac.volume, etc.

// In app_state.h
bool isDacDirty() const { return _dacDirty; }
void markDacDirty() { _dacDirty = true; app_events_signal(EVT_DAC); }
void clearDacDirty() { _dacDirty = false; }
```

**Implication for DEBT-3:**
- WS handlers set flags (in `appState.dac.pendingDacToggle`)
- Main loop broadcasts changes (via `isDacDirty()`)
- Clear separation: UI→flags, broadcast→dirty flags
- **No change needed** — pattern already supports N DACs

---

## How Modular State Reduces DEBT-3 Risk

### Risk 1: State Collision (ELIMINATED)
**Old:** Adding a new DAC flag = modify app_state.h, risk conflicts with other subsystems
**New:** Add to `state/dac_state.h`, compile in isolation, test in unit tests only

### Risk 2: AppState Bloat (SOLVED)
**Old:** Every device, every subsystem adds fields to AppState
**New:** Each subsystem gets its own struct, composed into AppState

### Risk 3: Toggle Logic Scattered (REDUCED)
**Old:** DAC toggle scattered: `_pendingDacToggle` (AppState), `dac_output_init()` (dac_hal.cpp), WebSocket handler calls
**New:** Centralized in `DacState`: flags + validated setters + main loop handler

### Risk 4: Test Isolation (IMPROVED)
**Old:** Testing toggle logic requires mocking entire AppState
**New:** Test `DacState` struct in isolation: `test_deferred_toggle` ✓ exists

---

## Implications for Future Refactoring

### If Phase 3 Goes Generic (Plan v1):
```cpp
// ANTI-PATTERN
struct PendingDeviceToggle {
    uint8_t halSlot = 0xFF;
    int8_t action = 0;
};
// Main loop: HalDevice* dev = mgr.findByCompatible(appState.pendingDeviceToggle.halSlot);
```

**Problems:**
- Requires device lookup (extra complexity)
- Breaks the leaf-struct pattern (mixing UI state with HAL details)
- Makes testing harder (need mock HAL manager)
- Doesn't scale to >1 pending toggle (what if user clicks "toggle DAC" twice? Lost request)

### If Phase 3 Goes Simplified (Recommended):
```cpp
// PATTERN-ALIGNED
struct DacState {
    volatile int8_t pendingDacToggle = 0;
    volatile int8_t pendingEs8311Toggle = 0;
    void requestDacToggle(int8_t action) { ... }
    void requestEs8311Toggle(int8_t action) { ... }
};

// Main loop: if (appState.dac.pendingDacToggle > 0) dac_activate_primary();
```

**Benefits:**
- Simple (no lookups)
- Testable (no HAL mocking needed)
- Extensible (add more `pendingXxxToggle` fields if needed)
- Follows established pattern (leaf-struct composition)

---

## Generalization: When to Introduce Generic Structs

### Generic Struct Anti-Pattern
```cpp
// BAD: Generic toggle that could apply to any device
struct PendingDeviceToggle {
    uint8_t halSlot;
    int8_t action;
};
```

**Why it's anti-pattern:**
- Trades **simplicity for false generality**
- Requires device lookup (extra indirection)
- Harder to test (need mock device manager)
- Doesn't actually support multiple pending toggles (only one field)

### When Generics Are Good
```cpp
// GOOD: Generic list of pending operations
struct PendingOperation {
    uint16_t opId;
    void (*callback)(void);
};
// Keep a queue: vector<PendingOperation> pendingOps;
```

**Why it's good:**
- True multiplicity (N pending ops, not just 1)
- Callback abstraction (any op, any handler)
- Useful across multiple domains
- Scalable

---

## Lessons for Future Modules

### When Designing New AppState Substrates:

1. **Composition First** — Always use `struct ModuleName { }` in leaf file, then compose into AppState
2. **Validators at the Leaf** — Put validated setters in the leaf struct, not scattered in handlers
3. **Dirty Flags at AppState Level** — Dirty flags belong in AppState (broadcast coordination), state belongs in leaf
4. **Tests for Leaf Structs** — Unit test the leaf struct independently, then test integration with AppState
5. **Don't Generalize Prematurely** — If only 2 devices need toggles, use 2 separate flags, not a generic array

### Bad Pattern to Avoid:
```cpp
// DON'T DO THIS
class AppState {
    volatile int8_t pendingToggle[24];  // Generic array for "any device"
    uint8_t pendingToggleSlot = 0xFF;   // Which slot?
    int8_t pendingToggleAction = 0;     // What action?
    // Now main loop needs: if (pendingToggleSlot < 24) { dev = mgr.getDevice(slot); ... }
};
```

### Good Pattern:
```cpp
// DO THIS INSTEAD
struct DacState {
    volatile int8_t pendingDacToggle = 0;
    volatile int8_t pendingEs8311Toggle = 0;
    void requestDacToggle(int8_t action) { if (action >= -1 && action <= 1) pendingDacToggle = action; }
    void requestEs8311Toggle(int8_t action) { ... }
};

class AppState {
    DacState dac;
    // Main loop: if (dac.pendingDacToggle > 0) dac_activate_primary();
};
```

---

## Architectural Decision Record: DEBT-3 Phase 3

**Question:** Should we generalize the toggle mechanism to support N DACs?

**Option A (Generic Array):**
```cpp
struct PendingDeviceToggle {
    uint8_t halSlot = 0xFF;
    int8_t action = 0;
};
```
- **Pro:** Sounds flexible
- **Con:** Requires device lookup, harder to test, no real multiplicity

**Option B (Per-DAC Flags):**
```cpp
volatile int8_t pendingDacToggle = 0;
volatile int8_t pendingEs8311Toggle = 0;
```
- **Pro:** Simple, testable, extensible (add more flags as needed)
- **Con:** Doesn't scale to 10+ DACs automatically

**Option C (Queue of Operations):**
```cpp
struct PendingOp { uint8_t halSlot; int8_t action; };
vector<PendingOp> pendingOps;
```
- **Pro:** True multiplicity, scales well
- **Con:** Overkill for 2-3 devices, requires queue management

**Decision:** **Option B (Per-DAC Flags)**
- Current firmware supports 2 DACs (primary + ES8311)
- If >3 DACs needed in future, we can upgrade to Option C
- YAGNI: Don't over-engineer for hypothetical 10-DAC setup
- Consistency with `appState.dac.enabled`, `appState.dac.es8311Enabled`

---

## Conclusion

The modular AppState architecture has **inadvertently solved part of DEBT-3's problem** by providing:
1. Isolated state namespaces (`appState.dac.*`)
2. Leaf-struct composition (no monolithic class)
3. Tested validated setters (`requestDacToggle()`)

This means DEBT-3 can focus on its core goal: **route DAC initialization through HAL** (Phases 1-2), without distraction from AppState refactoring (Plan Phase 3).

The simplified Phase 3 approach (keep separate flags, add wrappers) aligns with the established architectural patterns and reduces risk compared to the original generic-struct proposal.

