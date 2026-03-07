# ALX Nova Controller 2 -- Disconnect Analysis

Identified architectural disconnects between HAL framework, audio pipeline, and legacy DAC layer.

---

## Disconnect Table

| ID | Issue | Severity | Location | Description |
|---|---|---|---|---|
| **D1** | Bridge is metadata-only | **CRITICAL** | `src/hal/hal_pipeline_bridge.cpp` | `hal_pipeline_on_device_available()` sets `_pipelineOutputSlots[slot] = true` but never calls `audio_pipeline_register_sink()`. Actual sink registration happens in `dac_hal.cpp` via direct `audio_pipeline_register_sink()` calls, bypassing the bridge entirely. The bridge tracks counts that nothing reads. |
| **D2** | ADC enable ignores HAL state | **CRITICAL** | `src/audio_pipeline.cpp:161` | `pipeline_sync_flags()` reads `appState.adcEnabled[0]` and `appState.pipelineInputBypass[0]` to decide whether to call `i2s_audio_read_adc1()`. HAL device state (`HAL_STATE_AVAILABLE` / `HAL_STATE_MANUAL`) for PCM1808 devices is never consulted. Disabling a PCM1808 via HAL API has zero effect on audio capture. |
| **D3** | `clear_sinks()` kills all sinks | **HIGH** | `src/dac_hal.cpp:919` | `dac_output_deinit()` calls `audio_pipeline_clear_sinks()` which sets `_sinkCount = 0`, removing ALL registered sinks -- including the secondary ES8311 sink. There is no per-sink removal function. Deiniting the primary DAC silently kills secondary audio output. |
| **D4** | `markChannelMapDirty()` never called by producers | **HIGH** | `src/app_state.h:514` | The dirty flag and `EVT_CHANNEL_MAP` event exist, and the main loop dispatches `sendAudioChannelMap()` when set. However, no code path ever calls `markChannelMapDirty()`. Channel map updates only happen as a side-effect of `isHalDeviceDirty()` dispatch in `main.cpp:1111`. The dedicated event bit is dead. |
| **D5** | `EVT_ANY` was 16-bit (FIXED) | **FIXED** | `src/app_events.h:37` | Previously defined as `0xFFFFUL` (16 bits), which masked only bits 0-15. FreeRTOS event groups have 24 usable bits (0-23). Any future events on bits 16-23 would have been invisible to `app_events_wait()`. Now fixed to `0x00FFFFFFUL`. |
| **D6** | `audioPaused` race window | **MEDIUM** | `src/dac_hal.cpp:913,426` | Both `dac_output_deinit()` and `dac_secondary_deinit()` set `audioPaused = true`, wait 40ms, then proceed. If both are called in sequence (e.g., full shutdown), the second call sets `audioPaused = false` at line 453 before the first caller expects the audio task to be stopped. No reference counting or mutex protects the flag. |
| **D7** | Signal Generator not modeled as audio source | **MEDIUM** | `src/hal/hal_signal_gen.cpp` | `HalSignalGen` is registered as `HAL_DEV_GPIO` type. The signal generator's audio path (lane 2 in audio pipeline) is entirely separate from HAL. Enabling/disabling siggen via HAL has no effect on audio injection. The HAL device is cosmetic. |
| **D8** | String-based sink-to-HAL matching | **MEDIUM** | `src/websocket_handler.cpp:1745` | `sendAudioChannelMap()` matches sinks to HAL devices using `strstr(sk->name, desc.name)`. This is fragile -- e.g., a sink named "PCM5102A Primary" matches a HAL device named "PCM5102A" only because one is a substring of the other. No slot ID or stable identifier links them. |
| **D9** | Per-output DSP processes all 8 channels | **LOW** | `src/audio_pipeline.cpp:384` | `pipeline_run_output_dsp()` iterates `ch = 0..7` and calls `output_dsp_process()` for every channel. With only 2 active sinks (primary stereo L/R), channels 2-7 are processed but their output is never consumed by any sink. Wasted CPU on Core 1. |
| **D10** | Duplicate driver registries | **LOW** | `src/dac_registry.h` vs `src/hal/hal_driver_registry.h` | `DacRegistry` maps `uint16_t deviceId -> DacFactoryFn` (legacy). `HalDriverRegistry` maps `compatible string -> HalDeviceFactory` (HAL). Both exist, both are populated at boot, and `HalDacAdapter` bridges between them. Target: unify into `HalDriverRegistry` only. |

---

## Severity Definitions

| Level | Meaning |
|---|---|
| **CRITICAL** | Architectural path is completely non-functional; feature depends on legacy workaround |
| **HIGH** | Incorrect behavior under specific conditions; data loss or silent failure possible |
| **MEDIUM** | Fragile or wasteful; works today but breaks under reasonable future changes |
| **LOW** | Technical debt; no runtime impact but blocks architectural cleanup |
| **FIXED** | Previously identified, now resolved |
