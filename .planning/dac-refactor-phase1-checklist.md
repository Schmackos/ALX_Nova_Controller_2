# DAC Refactor Phase 1 — Test and Verification Checklist

## What this refactoring changes

`dac_output_init()` is split into two functions:

- `dac_boot_prepare()` — the one-time-only work: mutex init, settings load, EEPROM scan, volume gain calculation. Safe to call before HAL discovery completes.
- `dac_activate_for_hal(HalDevice* halDev)` — the per-activation work: driver selection, driver init, I2S TX enable, HAL adapter registration, pipeline sink registration. Called by the HAL bridge on state transition to AVAILABLE.

A thin `dac_output_init()` wrapper that calls both in sequence is retained so existing callsites in `audio_pipeline.cpp` (line 599) and `main.cpp` (lines 1210, 1222) continue compiling and behaving identically.

The `_halPrimaryAdapter` file-static is replaced by `_adapterForSlot[HAL_MAX_DEVICES]`, indexed by HAL slot number. `_halSecondaryAdapter` stays unchanged until Phase 2.

---

## Pre-checklist: understand the impact surface

Before executing any check, confirm which files touch the affected API:

| File | Role | Expected change |
|---|---|---|
| `src/dac_hal.h` | Public API declaration | New declarations added; `dac_output_init()` kept |
| `src/dac_hal.cpp` | Implementation | Split into `dac_boot_prepare` + `dac_activate_for_hal`; wrapper added |
| `src/audio_pipeline.cpp` line 599 | Calls `dac_output_init()` inside `audio_pipeline_init()` | No change — wrapper hides the split |
| `src/main.cpp` lines 1210, 1222 | Re-init on web-UI DAC toggle | No change — wrapper hides the split |
| `src/hal/hal_pipeline_bridge.cpp` | Calls `hal_pipeline_state_change()` | No change in Phase 1 |
| `test/test_dac_hal/test_dac_hal.cpp` | Inline reimplementations — no src/ includes | No change expected |
| `test/test_dac_settings/test_dac_settings.cpp` | Tests AppState JSON round-trip only | No change expected |
| `test/test_hal_bridge/test_hal_bridge.cpp` | Tests bridge mapping tables | No change expected |

---

## Section 1 — Code review gates (manual, before running any tool)

These are things to read in the diff before touching a single command.

### 1.1 Static storage: `_adapterForSlot[]` replaces `_halPrimaryAdapter`

- [ ] `_adapterForSlot` is declared as `static HalDacAdapter* _adapterForSlot[HAL_MAX_DEVICES]` at file scope in `dac_hal.cpp`.
- [ ] Every element is initialized to `nullptr` (either via `= {}` or an explicit loop in `dac_boot_prepare()`).
- [ ] The original `static HalDacAdapter* _halPrimaryAdapter = nullptr` line is removed.
- [ ] `_halSecondaryAdapter` is **not** replaced in Phase 1.

### 1.2 `dac_boot_prepare()` contains exactly the one-time-only block

Verify these operations moved into `dac_boot_prepare()` and are absent from `dac_activate_for_hal()`:

- [ ] `dac_eeprom_init_mutex()` call.
- [ ] `static bool _settingsLoaded` guard and `dac_load_settings()`.
- [ ] `_volumeGain = dac_volume_to_linear(as.dac.volume)` and its `LOG_I`.
- [ ] `static bool _eepromScanned` guard, `dac_i2c_scan()`, and `dac_eeprom_scan()` block (wrapped in `#ifndef NATIVE_TEST`).

### 1.3 `dac_activate_for_hal()` contains exactly the per-activation block

Verify these operations are in `dac_activate_for_hal()` and are absent from `dac_boot_prepare()`:

- [ ] The delegation guard (find PCM5102A in HAL, check `HAL_STATE_AVAILABLE`, handle probe-only placeholder removal).
- [ ] `if (!as.dac.enabled) return;` early exit.
- [ ] `dac_select_driver()` call and fallback logic.
- [ ] Pin config resolution via `hal_get_config_for_type(HAL_DEV_DAC)` (inside `#ifndef NATIVE_TEST`).
- [ ] `_driver->init(pins)`, `dac_enable_i2s_tx()`, `_driver->configure()` sequence with error returns preserved.
- [ ] `as.dac.detected = true; as.dac.ready = true;` and counter resets.
- [ ] HAL adapter registration block: either `new HalDacAdapter(...)` + `registerDevice()` (first boot) or state update + `hal_pipeline_state_change()` (re-enable). This now indexes via `_adapterForSlot[halSlot]` instead of `_halPrimaryAdapter`.
- [ ] Pipeline sink registration: `audio_pipeline_set_sink(AUDIO_SINK_SLOT_PRIMARY, &primarySink)`.

### 1.4 `dac_output_init()` wrapper is correct

- [ ] The wrapper body is exactly two calls in order: `dac_boot_prepare()` then `dac_activate_for_hal(nullptr)` (or with a sentinel to signal the legacy callsite).
- [ ] No logic is added to the wrapper beyond those two calls.
- [ ] The function signature in `dac_hal.h` is unchanged: `void dac_output_init()`.

### 1.5 `dac_output_deinit()` is unchanged

- [ ] The function still references `_halPrimaryAdapter` or the equivalent lookup into `_adapterForSlot`. The deinit logic for the primary adapter must remain functionally identical.
- [ ] Audio pause/resume semaphore protocol (`as.audio.paused = true`, `xSemaphoreTake(audioTaskPausedAck, pdMS_TO_TICKS(50))`, `as.audio.paused = false`) is preserved verbatim.
- [ ] `dac_disable_i2s_tx()` call remains after `_driver->deinit()`.

### 1.6 `dac_activate_for_hal()` error handling paths

Each early-return path in the original `dac_output_init()` must have an exact counterpart in `dac_activate_for_hal()`:

- [ ] Driver select failure + PCM5102A fallback failure sets `as.dac.enabled = false` and returns.
- [ ] `_driver->init(pins)` failure sets `as.dac.ready = false` and returns.
- [ ] `dac_enable_i2s_tx()` failure calls `_driver->deinit()`, sets `as.dac.ready = false`, returns.
- [ ] `_driver->configure()` failure sets `as.dac.ready = false` and returns (`LOG_W`, not `LOG_E`).

### 1.7 Guard macro coverage

- [ ] `dac_boot_prepare()` is wrapped in `#ifdef DAC_ENABLED` (same as `dac_output_init()` was).
- [ ] `dac_activate_for_hal()` is wrapped in `#ifdef DAC_ENABLED`.
- [ ] Both new declarations appear in `dac_hal.h` inside `#ifdef DAC_ENABLED`.
- [ ] No NATIVE_TEST-guarded hardware calls (I2C scan, I2S enable) are in `dac_boot_prepare()` at the top level — they must stay inside their existing `#ifndef NATIVE_TEST` guards.

---

## Section 2 — Native compilation check

```bash
# From the repo root. This compiles only the native test environment
# (no hardware, uses MinGW gcc). Catches include errors, undefined symbols,
# and duplicate definitions. Zero tolerance for new warnings.
pio run -e native 2>&1 | tee logs/phase1-native-compile.log
```

Pass criterion: exit code 0, no lines containing `error:` or new `warning:` patterns that were not present before the change. Compare against baseline `pio run -e native` output on the pre-change branch if needed.

### What to look for in the log

- `undefined reference to dac_boot_prepare` — means the declaration is in the .h but the .cpp was not compiled into the test (unlikely, but check if `test_build_src = no` causes it in any test that inline-includes `dac_hal.cpp`).
- `multiple definition of _adapterForSlot` — means the array was accidentally put in a header rather than the .cpp.
- `implicit conversion` or `narrowing` warnings on the new `halSlot` index arithmetic.

---

## Section 3 — Firmware compilation check

```bash
# Full ESP32-P4 firmware build. Tests that esp-idf headers, PSRAM caps,
# and Arduino-framework-specific includes all still resolve.
pio run 2>&1 | tee logs/phase1-firmware-compile.log
```

Pass criterion: exit code 0, RAM/Flash usage within previous bounds (check `.pio/build/esp32-p4/firmware.elf` size, or read the table printed at the end of `pio run`). If RAM changes by more than 1 KB, investigate — the `_adapterForSlot[]` array adds `HAL_MAX_DEVICES * sizeof(pointer)` = 24 × 4 = 96 bytes on a 32-bit build, which is expected and acceptable.

---

## Section 4 — Unit tests: focused modules

Run each affected module individually first to get fast, isolated failure messages.

### 4.1 `test_dac_hal`

```bash
pio test -e native -f test_dac_hal -v 2>&1 | tee logs/phase1-test_dac_hal.log
```

Expected: 20 tests, all PASS. This module has no src/ includes — it uses inline reimplementations. The test runner should be unaffected by the refactor. If any test fails, the failure is in the inline simulation logic or a Unity setup issue, not the production code.

Tests this module covers:
- Registry lookup by ID and name (5 tests)
- PCM5102A driver capabilities, init, configure, volume, mute, ready-state (9 tests)
- Volume curve: zero, full, monotonicity, midpoint, clamp (5 tests)
- Software volume: unity, half gain, zero gain, null buffer, zero samples (5 tests — actually 5 in the runner)

If this module fails, the refactoring touched the volume curve or software volume helpers — check that `dac_volume_to_linear()` and `dac_apply_software_volume()` were not accidentally moved or renamed.

### 4.2 `test_dac_settings`

```bash
pio test -e native -f test_dac_settings -v 2>&1 | tee logs/phase1-test_dac_settings.log
```

Expected: 6 tests, all PASS. This module tests AppState JSON persistence logic, not `dac_output_init()`. The only failure mode from the refactor would be if `AppState::dac` struct members were accidentally renamed while moving initialization code.

Tests: default values, missing ES8311 fields preserve defaults, JSON save contains ES8311 keys, out-of-range volume rejected, JSON round-trip, ES8311 dirty flag lifecycle.

### 4.3 `test_hal_bridge`

```bash
pio test -e native -f test_hal_bridge -v 2>&1 | tee logs/phase1-test_hal_bridge.log
```

Expected: 50 tests, all PASS (8 groups covering available/removed/unavailable/transient/boot-sync/counts/ordinal-slots/forward-lookup). This module inline-includes `hal_pipeline_bridge.cpp`, which has no compile-time dependency on `dac_output_init()`. If this module fails, the refactoring accidentally changed a symbol name or struct layout that the bridge depends on.

Pay specific attention to:
- Group 7 (dynamic ordinal assignment): the `_adapterForSlot[]` change must not alter how the bridge assigns sink slots.
- Group 8 (forward-lookup API): `hal_pipeline_get_sink_slot()` must still return correct values.

---

## Section 5 — Full unit test suite

```bash
pio test -e native -v 2>&1 | tee logs/phase1-full-test-suite.log
```

Pass criterion: exactly 1561 tests PASS, 0 FAIL, 0 ERROR (current baseline from MEMORY.md). Any regression against this number is a blocker.

If a module outside `test_dac_hal`, `test_dac_settings`, `test_hal_bridge` fails, the cause is almost certainly a symbol conflict introduced by the refactor. Run that module in isolation to get the specific error.

```bash
# Isolate any failing module (replace test_module_name):
pio test -e native -f test_module_name -v 2>&1 | head -60
```

---

## Section 6 — Static analysis

### 6.1 Duplicate JS declarations

Not applicable to this C++ refactor, but run as part of the standard quality gate:

```bash
node tools/find_dups.js
```

Pass criterion: exit code 0, no output.

### 6.2 Missing function references

```bash
node tools/check_missing_fns.js
```

Pass criterion: exit code 0, no output.

### 6.3 cppcheck

```bash
cppcheck --enable=warning,performance --suppress=badBitmaskCheck \
  -i src/gui/ src/ 2>&1 | tee logs/phase1-cppcheck.log
```

Pass criterion: no new error or warning lines compared to the pre-change baseline. Known suppressions are documented in the CLAUDE.md and CI workflow.

Specific things to check for from this refactor:
- `nullPointer` on `_adapterForSlot[slot]` if the bounds check was omitted.
- `arrayIndexOutOfBounds` if `HAL_MAX_DEVICES` is not used as the array bound.
- `uninitMemberVar` if `_adapterForSlot` elements are not zeroed.

---

## Section 7 — Behavioral invariant verification (logic checks, no hardware)

These are not automated tests but are required code-reading checks to confirm the refactoring preserves semantics.

### 7.1 `_settingsLoaded` static is in `dac_boot_prepare()`, not the wrapper

The static guard ensures settings are loaded only once across all calls. Confirm:
- The `static bool _settingsLoaded = false;` guard lives inside `dac_boot_prepare()`.
- `dac_output_init()` (the wrapper) does NOT contain its own `_settingsLoaded` guard.
- Calling `dac_output_init()` twice still loads settings only once.

### 7.2 `_eepromScanned` static is in `dac_boot_prepare()`, not the wrapper

Same pattern as `_settingsLoaded`. The I2C scan must run once even if `dac_activate_for_hal()` is called multiple times (on re-enable). Verify the guard is in `dac_boot_prepare()`.

### 7.3 Re-enable path preserves slot identity

When the web UI toggles DAC off then on, `dac_output_deinit()` is called then `dac_output_init()`. The re-enable path must:

1. Find `_adapterForSlot[slot]` non-null (set from the first activation).
2. Update `_ready = true` and transition state to `HAL_STATE_AVAILABLE`.
3. Call `hal_pipeline_state_change()` so the bridge re-wires the pipeline sink.
4. NOT call `new HalDacAdapter(...)` or `registerDevice()` again.

Confirm this logic is correct in `dac_activate_for_hal()` by tracing the `if (!_adapterForSlot[slot]) { ... } else { ... }` branches.

### 7.4 Audio pause/resume semaphore is in `dac_output_deinit()` only

Confirm `dac_activate_for_hal()` does NOT set `as.audio.paused`. The pause is only needed during deinit (I2S driver uninstall). Init does not touch a running I2S channel.

### 7.5 `dac_secondary_init()` and `dac_secondary_deinit()` are unchanged

These functions reference `_halSecondaryAdapter` which is not part of the Phase 1 refactor. Confirm:
- `_halSecondaryAdapter` is still a `static HalDacAdapter*` file-static initialized to `nullptr`.
- Neither function calls any of the new Phase 1 functions.
- Both functions compile without warnings.

---

## Section 8 — Exit criteria summary

All of the following must be true before Phase 2 begins:

| Check | Command | Pass condition |
|---|---|---|
| Native compile | `pio run -e native` | Exit 0, no new warnings |
| Firmware compile | `pio run` | Exit 0, RAM delta within 200 bytes |
| `test_dac_hal` | `pio test -e native -f test_dac_hal -v` | 20/20 PASS |
| `test_dac_settings` | `pio test -e native -f test_dac_settings -v` | 6/6 PASS |
| `test_hal_bridge` | `pio test -e native -f test_hal_bridge -v` | 50/50 PASS |
| Full suite | `pio test -e native -v` | 1561/1561 PASS |
| find_dups | `node tools/find_dups.js` | Exit 0 |
| check_missing_fns | `node tools/check_missing_fns.js` | Exit 0 |
| cppcheck | `cppcheck ...` | No new errors or warnings |
| Code review 1.1-1.7 | Manual diff read | All boxes checked |
| Invariants 7.1-7.5 | Manual diff read | All boxes checked |

---

## Section 9 — Recommended execution order

Run in this sequence to get the fastest feedback on failures:

1. **Manual code review (Section 1)** — catches design mistakes before spending compile time.
2. **`pio run -e native`** (Section 2) — fastest compile, catches symbol errors in 30 seconds.
3. **`pio test -e native -f test_dac_hal -v`** (Section 4.1) — 5 seconds, confirms the pure-logic helpers were not damaged.
4. **`pio test -e native -f test_dac_settings -v`** (Section 4.2) — 5 seconds, confirms AppState struct is intact.
5. **`pio test -e native -f test_hal_bridge -v`** (Section 4.3) — 10 seconds, confirms bridge-to-pipeline wiring is intact.
6. **`pio test -e native -v`** (Section 5) — full suite, ~3 minutes, confirms no cross-module regressions.
7. **Static analysis (Section 6)** — run after all tests pass to catch pointer and bounds issues.
8. **`pio run`** (Section 3) — ESP32-P4 firmware build, run last as it takes the most time.
9. **Behavioral invariants (Section 7)** — final diff review against the checklist before marking Phase 1 complete.

---

## Section 10 — Common failure patterns and remediation

| Symptom | Likely cause | Fix |
|---|---|---|
| `undefined reference to dac_boot_prepare` in test build | Test inlines `dac_hal.cpp` and the function is not guarded correctly | Ensure `dac_boot_prepare` is inside `#ifdef DAC_ENABLED` and the test mock defines `DAC_ENABLED` |
| `multiple definition of _adapterForSlot` | Array definition placed in a header that is included multiple times | Move definition to `.cpp` file only; `.h` gets only the `extern` declaration if needed (preferred: keep it static and unexported) |
| `test_hal_bridge` Group 7 fails (wrong sink slot numbers) | `_adapterForSlot[]` indexing changed how the bridge ordinal counter works | The bridge does not read `_adapterForSlot` — if slot numbers changed, the bridge's own `_halSlotToSinkSlot[]` logic was accidentally modified |
| Full suite passes 1560/1561 with one `test_dac_settings` failure | `AppState::dac` struct member renamed during refactor | Revert any member renames; the struct is part of the public AppState API |
| cppcheck `nullPointer` on `_adapterForSlot[slot]` | Slot bounds not checked before array access in `dac_activate_for_hal()` | Add `if (slot >= HAL_MAX_DEVICES) return;` guard at the top of `dac_activate_for_hal()` |
| Firmware build size increased by more than 1 KB | `dac_boot_prepare()` duplicated the EEPROM scan code instead of moving it | Verify the scan block appears in exactly one of the two new functions |
