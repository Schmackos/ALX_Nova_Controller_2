# Legacy & Dead Code Inventory

Status as of 2026-03-07. Items marked ✅ have been addressed.

## Deleted Files (committed)

| File(s) | Status | Commit |
|---------|--------|--------|
| `src/io_registry.cpp`, `src/io_registry.h` | ✅ Deleted | 7e7f4fc |
| `src/io_registry_api.cpp`, `src/io_registry_api.h` | ✅ Deleted | 7e7f4fc |
| `web_src/js/10-input-audio.js` | ✅ Deleted | f8d7449 |
| `web_src/js/11-input-overview.js` | ✅ Deleted | f8d7449 |
| `web_src/js/12-output-dac.js` | ✅ Deleted | f8d7449 |
| `test/test_io_registry/test_io_registry.cpp` | ✅ Deleted | 7e7f4fc |

## Dead Code Removed

| Item | Location | Status |
|------|----------|--------|
| `halSyncAudioTabVisibility()` no-op stub | `web_src/js/15-hal-devices.js` | ✅ Removed |
| `src/Testfile.h` stale artifact | `src/` | ✅ Deleted |

## Legacy Code — Active but Scheduled for Unification

| File(s) | Purpose | Migration Target | Status |
|---------|---------|-----------------|--------|
| `src/hal/hal_pipeline_bridge.cpp` | Metadata-only pipeline sync | Functional bridge (Phase 3.4) | 🔄 In progress |
| `src/hal/hal_dac_adapter.h/.cpp` | Wraps DacDriver → HalAudioDevice | Remove after bridge owns sinks | ⏳ Pending |
| `src/dac_registry.h/.cpp` | Parallel device registry | Unify into HalDriverRegistry | ⏳ Pending |
| `src/dac_eeprom.h/.cpp` | Legacy EEPROM detection | Migrate to hal_discovery | ⏳ Pending |

## Duplicate Registries

The project has two parallel device registries:
1. **HalDriverRegistry** (`src/hal/hal_driver_registry.h/.cpp`) — HAL framework's factory (compatible string → constructor)
2. **DacRegistry** (`src/dac_registry.h/.cpp`) — Legacy DAC-specific registry

Plan: Unify all device registration under HalDriverRegistry. DacRegistry to be removed after all DAC drivers are HAL-native.

## Legacy DAC Init Path

`dac_hal.cpp` currently:
1. Creates `DacDriver` via `DacRegistry`
2. Wraps it in `HalDacAdapter` for HAL integration
3. Registers sinks directly via `audio_pipeline_register_sink()`

Target: Bridge registers sinks via state change callback. `dac_hal.cpp` reduced to I2S driver management only.
