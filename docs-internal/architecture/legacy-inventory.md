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

## Completed Removals (DEBT-6)

DEBT-6 (Registry Unification & Bridge Sink Ownership) is now complete. The following legacy components have been removed or refactored:

| Component | Role | Status |
|-----------|------|--------|
| `DacRegistry` | Legacy DAC-specific device registry | ✅ Deleted |
| `HalDacAdapter` | Wrapper converting DacDriver → HalAudioDevice | ✅ Deleted |
| `dac_output_init()` / `dac_secondary_init()` | Direct sink registration | ✅ Refactored (bridge now owns) |
| Dual-registry architecture | HalDriverRegistry + DacRegistry parallel systems | ✅ Unified under HAL |

## Current Architecture (post DEBT-6)

`dac_hal.cpp` is now **bus-utility only**:
- I2S TX management (BCK/DOUT clock configuration)
- Volume curve computation (dB → linear scaling)
- Periodic device logging (loops all active sinks via `audio_pipeline_get_sink()`)

**Sink lifecycle is owned solely by HAL-Pipeline Bridge:**
- Bridge registers/removes sinks on `HalStateChangeCb` state transitions
- No legacy `DacRegistry` or `HalDacAdapter` wrappers
- `audio_pipeline_register_sink()` called only from `hal_pipeline_bridge.cpp`

## Remaining Known Gaps

| Component | Purpose | Deferred Until | Notes |
|-----------|---------|----------------|-------|
| `src/dac_eeprom.h/.cpp` | Legacy EEPROM v2 I2C detection | Future discovery phase | Low priority; HAL discovery v3 handles modern EEPROM |
| TLS cert verification | Secure OTA downloads | Post-v1.13 | Pre-authorized GitHub releases used as workaround |
