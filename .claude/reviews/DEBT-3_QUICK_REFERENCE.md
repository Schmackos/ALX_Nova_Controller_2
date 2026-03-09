# DEBT-3 Plan Review — Quick Reference
**Status:** GREEN — Clear to proceed
**Recommendation:** Modify Phase 3 per simplified approach

---

## Phase Status Summary

| Phase | Status | Notes |
|-------|--------|-------|
| **Phase 1** | ✅ GREEN | Extract boot ops, introduce `_adapterForSlot[]`. Zero conflicts. |
| **Phase 2** | ✅ GREEN | Bridge-triggered activation. Perfect alignment with new HAL arch. |
| **Phase 3** | 🔄 YELLOW | **Simplify:** Keep separate flags, add wrappers. Don't over-engineer generic toggle. |
| **Phase 4** | ✅ GREEN | Cleanup dead code. Straightforward once 1-2 complete. |

---

## Key Finding: AppState Already Modular

**Plan Assumption:** AppState would be monolithic and need decomposition.

**Reality:** AppState is **already modular via composition**:
- `#include "state/dac_state.h"` (line 15 of app_state.h)
- `DacState dac;` member with validated setters already in place
- `pendingDacToggle`, `pendingEs8311Toggle`, `requestDacToggle()`, `requestEs8311Toggle()` ✓ exist

**Impact:** Phases 1-2 proceed unchanged. Phase 3 needs revision.

---

## Phase 3 Simplification (Instead of Generic Toggle)

### Plan Said (Over-engineered):
```cpp
// NEW generic struct (unnecessary)
struct PendingDeviceToggle {
    uint8_t halSlot = 0xFF;
    int8_t action = 0;
};
```

### We Should Do (Simpler):
Keep existing separate flags, add wrapper functions in `dac_hal.h`:
```cpp
void dac_activate_primary();     // Wraps dac_activate_for_hal() for primary
void dac_deactivate_primary();   // Wraps dac_deactivate_for_hal() for primary
void dac_activate_secondary();   // Wraps dac_activate_for_hal() for ES8311
void dac_deactivate_secondary(); // Wraps dac_deactivate_for_hal() for ES8311
```

### Why Simpler is Better:
1. Reuses existing validated flags (6 passing tests, no changes needed)
2. Main loop toggle handler stays lightweight (no device lookups)
3. Maintains clean separation: UI→AppState flags, HAL implementation in dac_hal.cpp
4. One less AppState field to track

---

## Top 3 Risks + Mitigations

| Risk | Severity | Mitigation |
|------|----------|-----------|
| **State inconsistency** (appState.dac.enabled vs _adapterForSlot[]) | MEDIUM | Add helper `dac_is_slot_active()`, validate consistency at 5s interval |
| **Hotplug during audio** (main loop writes, audio task reads) | MEDIUM | Both paths use `vTaskSuspendAll()` — add comment documenting this |
| **Sink slot collision** (8 DACs max, only room for 8) | LOW | Add validation: reject if `sinkSlot >= AUDIO_OUT_MAX_SINKS` |

---

## Critical Files to Modify

| File | Phase | Change | LOC |
|------|-------|--------|-----|
| `src/dac_hal.cpp` | 1 | Split `dac_output_init()` → boot + activate | ~100 |
| `src/dac_hal.h` | 1 | Add `dac_boot_prepare()`, `dac_activate_for_hal()`, wrappers | ~10 |
| `src/audio_pipeline.cpp:599` | 1 | Replace `dac_output_init()` with `dac_boot_prepare()` | 1 |
| `src/hal/hal_pipeline_bridge.cpp:128` | 2 | Add `dac_activate_for_hal()` call in `on_device_available()` | ~5 |
| `src/main.cpp:313` | 2 | Remove `dac_secondary_init();` | 1 |
| `src/main.cpp:1208-1229` | 3 | Replace toggle handler with new wrappers | ~15 |
| `src/dac_hal.h/cpp` | 4 | Delete `dac_output_init()`, `dac_secondary_init()`, old adapters | ~50 |

---

## Testing Checklist

- [ ] Phase 1 complete → `pio test -e native` (1561 tests pass)
- [ ] Phase 2 complete → Boot with ES8311 enabled, verify audio on both DACs
- [ ] Phase 2 complete → Safe mode fallback produces audio
- [ ] Phase 3 complete → Toggle via WebSocket → audio on/off works
- [ ] Phase 4 complete → `pio test -e native -v` + `cd e2e && npx playwright test` (all green)
- [ ] NEW: Validation test — `appState.dac.enabled == (_adapterForSlot[0] != nullptr)`
- [ ] NEW: Validation test — Audio task survives DAC toggle (no deadlock)
- [ ] NEW: Validation test — Sink slot assignment deterministic across multiple cycles

---

## Estimated Effort

- **Phase 1:** 1 day (refactoring + unit tests)
- **Phase 2:** 1 day (bridge integration + hardware test)
- **Phase 3:** 0.5 day (add wrappers, update main loop)
- **Phase 4:** 0.5 day (delete dead code, update tests)
- **Integration + Validation:** 1 day

**Total:** 4 days (2-3 with full-time focus)

---

## Architecture Wins

After completing DEBT-3:
1. ✅ All DAC init routed through HAL (single source of truth)
2. ✅ Hardcoded "primary/secondary" concept removed (fully dynamic)
3. ✅ N DACs supported (not just 2)
4. ✅ Safe mode fallback preserved
5. ✅ No breaking changes to tests
6. ✅ Bridge owns sink lifecycle (not legacy dac_hal.cpp)

---

## Questions for the Developer

Before starting Phase 1:

1. **Adapter storage:** In Phase 1, should `_adapterForSlot[]` be indexed by pipeline sink slot or HAL slot?
   - Recommendation: **Sink slot** (0-7), since sink writes use `_primary_sink_write()` / `_secondary_sink_write()` lookup

2. **Safe mode activation:** Should `dac_activate_safe_mode()` create a HalDacAdapter or skip it?
   - Recommendation: **Skip it** (HAL framework isn't running in safe mode, so it's redundant)

3. **Wrapper function location:** Should wrappers live in `dac_hal.cpp` or remain inline in main loop?
   - Recommendation: **Separate functions in dac_hal.cpp** (cleaner, easier to test, documents the intent)

---

## Go-No-Go Decision Points

After Phase 1:
- ✓ All 1561 C++ tests pass?
- ✓ No audio regression on hardware?

After Phase 2:
- ✓ ES8311 activates automatically at boot?
- ✓ Safe mode still works?

After Phase 3:
- ✓ Toggle via WebSocket works?
- ✓ All 26 E2E tests pass?

After Phase 4:
- ✓ All tests pass?
- ✓ No broken references to old functions?

---

**Next Step:** Copy this review + full assessment to the development team. Start with Phase 1 implementation.
