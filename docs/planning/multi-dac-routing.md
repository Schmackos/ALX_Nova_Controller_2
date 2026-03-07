# Multi-DAC Dynamic Audio Routing System

## Context

The ALX Nova Controller is evolving from a fixed single-DAC amplifier controller into a **flexible audio DSP development platform**. Users/developers connect custom DAC and ADC daughter boards and use this project as the backbone for testing and deploying audio systems. The system must dynamically adapt to whatever I/O hardware is connected — 1 DAC + 2 ADC, 3 DACs + USB input, etc.

The immediate trigger: the ES8311 onboard DAC (P4) outputs silence because its settings are never persisted — `es8311Enabled` defaults `false` and has no load/save path. Fixing this is Phase 0, then we build the full multi-DAC routing architecture on top.

**Target**: ESP32-P4 only (3 I2S peripherals). Fixed peripheral map: I2S0 = primary DAC+ADC1, I2S1 = ADC2, I2S2 = secondary output.

---

## Execution Model

**Opus** (this model) coordinates: phase sequencing, context management, code review, test validation, commits.

**Sonnet specialized agents** execute: code implementation (firmware C/C++, web JS/CSS/HTML), test writing, codebase exploration. Each phase launches parallel Sonnet agents where tasks are independent (e.g., firmware changes + web UI + test writing can run simultaneously within a phase).

After each phase:
1. Sonnet agents implement code changes
2. Opus reviews changes for correctness and consistency
3. **Dead code & duplication scan**: identify and remove/consolidate dead code paths, duplicate logic, and orphaned functions introduced or exposed by the refactor
4. `pio test -e native` validates all tests pass (932+ baseline)
5. `pio run` confirms ESP32-P4 firmware builds cleanly
6. `node tools/build_web_assets.js` regenerates web assets
7. Commit phase to Dev branch
8. Hardware validation (user tests on P4 board)

---

## Phase 0: Fix ES8311 Output Bug

**Root cause confirmed**: `appState.es8311Enabled` defaults `false`. `dac_secondary_init()` (line 281 of `dac_hal.cpp`) checks this and returns early. There is **zero persistence** for `es8311Enabled`, `es8311Volume`, or `es8311Mute` — not in `dac_load_settings()`, not in `settings_manager.cpp`.

### Changes

**`src/dac_hal.cpp`** — Add ES8311 settings to the existing `/dac_config.json` persistence:

- In `dac_load_settings()`: add reads for `doc["es8311Enabled"]`, `doc["es8311Volume"]`, `doc["es8311Mute"]` → AppState members
- In `dac_save_settings()`: add writes for same three fields
- This way a single file handles all DAC state. No new files needed.

**`src/dac_hal.cpp`** — Fix I2C bus collision on P4:
- `dac_eeprom_scan()` uses `Wire` on external I2C pins (48/54)
- `DacEs8311::init()` uses `Wire` on onboard I2C pins (7/8)
- After ES8311 `Wire.begin(7,8)`, any subsequent EEPROM op on `Wire` would hit the wrong bus
- Fix: use `Wire1` for the external EEPROM bus on P4. Add `#define DAC_EEPROM_WIRE Wire1` for P4, `#define DAC_EEPROM_WIRE Wire` for S3 in `dac_eeprom.h`

**`src/drivers/dac_es8311.cpp`** — Add logging to `init()` to confirm chip ID verification and register write success.

### Files to modify
- `src/dac_hal.cpp` — load/save ES8311 settings in existing functions
- `src/dac_eeprom.h` / `src/dac_eeprom.cpp` — Wire instance abstraction for P4
- `src/drivers/dac_es8311.cpp` — Enhanced init logging

### Verification
- Boot P4 → serial should show `[DAC] ES8311 secondary output initialized`
- Play audio on ADC1 → hear output from both PCM5102A and ES8311/NS4150B speaker
- Toggle `es8311Enabled` via web UI → reboot → setting persists

---

## Phase 1: I/O Registry & Dynamic Discovery

### New unified EEPROM format v2

Extend `DacEepromData` in `src/dac_eeprom.h` with:
- `uint8_t deviceType` — 0=DAC, 1=ADC, 2=Codec (backward-compat: v1 defaults to DAC)
- `uint8_t i2sPort` — which I2S peripheral (0/1/2)
- Remain backward-compatible with existing v1 "ALXD" EEPROMs

### New module: `src/io_registry.h/.cpp`

Runtime registry of all audio I/O endpoints:

```
IoOutputEntry: active, index(0-3), deviceId, name, deviceType, discoveryMethod(EEPROM/manual/builtin),
               i2sPort, channelCount(1-2), firstOutputChannel(in 8x8 matrix), DacDriver*, ready
IoInputEntry:  active, index(0-3), name, discoveryMethod, i2sPort, channelCount, firstInputChannel
```

- `io_registry_init()` → I2C scan → EEPROM probe → register discovered devices
- Built-in ES8311 (P4) registered as output slot 1 (`IO_DISC_BUILTIN`)
- Built-in PCM1808 ADCs registered as input slots 0-1
- Manual add/remove via web UI for boards without EEPROM
- Persisted to `/io_registry.json` via LittleFS
- Output channel assignment: slot 0 → matrix out 0,1; slot 1 → out 2,3; slot 2 → out 4,5; slot 3 → out 6,7

### Web UI: `web_src/js/14-io-registry.js`
- Displays discovered I/O devices with status
- Manual add/remove for unconfigured boards
- REST: `GET /api/io/registry`, `POST /api/io/output`, `DELETE /api/io/output/{idx}`, `POST /api/io/scan`

### AppState additions
- `markIoRegistryDirty()` + dirty flag
- Add `EVT_IO_REGISTRY` to `app_events.h` (bit 11, within the 13 reserved bits)

### Files
- **New**: `src/io_registry.h`, `src/io_registry.cpp`, `web_src/js/14-io-registry.js`
- **Modify**: `src/dac_eeprom.h/.cpp` (v2 format), `src/app_state.h` (dirty flag), `src/app_events.h` (new event bit), `src/main.cpp` (call `io_registry_init()`), `web_src/index.html` (I/O config panel)

---

## Phase 2: Pipeline Output Expansion

### Expand output from 2 mono channels to 8

**`src/audio_pipeline.cpp`**:

1. Replace `_outL`/`_outR` with `_outCh[8]` — 8 mono float output buffers (PSRAM)
2. Expand `pipeline_mix_matrix()` inner loop from `for (o=0; o<2; o++)` to `for (o=0; o<8; o++)` using `_outCh[o]`
3. Update all `_outL`/`_outR` references to `_outCh[0]`/`_outCh[1]`

### New: `src/audio_output_sink.h` (mirrors `AudioInputSource`)

```c
typedef struct AudioOutputSink {
    const char *name;
    uint8_t firstChannel;    // First mono output channel (0,2,4,6)
    uint8_t channelCount;    // 1=mono, 2=stereo
    void (*write)(const int32_t *buf, int stereoFrames);
    bool (*isReady)(void);
    float gainLinear;
    bool muted;
    float vuL, vuR;
} AudioOutputSink;
```

### Rewrite `pipeline_write_output()`

Instead of hardcoded `dac_output_write()` + `dac_secondary_write()`, iterate registered sinks:
- For each ready sink: gather `_outCh[sink.firstChannel]` and `_outCh[sink.firstChannel+1]` → `to_int32_lj()` → `sink.write()`
- Primary DAC registered as sink 0 (channels 0-1) by `dac_output_init()`
- ES8311 registered as sink 1 (channels 2-3) by `dac_secondary_init()`

### Matrix persistence

Add `audio_pipeline_save_matrix()` / `audio_pipeline_load_matrix()` → `/pipeline_matrix.json`. Currently the 8x8 matrix has no persistence (re-initialized at boot).

### REST API: `src/pipeline_api.h/.cpp`
- `GET /api/pipeline/matrix` → `{matrix:8x8, inputNames:[], outputNames:[]}`
- `POST /api/pipeline/matrix/cell` → `{out, in, gainDb}`
- `GET /api/pipeline/sinks` → registered sinks + VU data

### Files
- **New**: `src/audio_output_sink.h`, `src/pipeline_api.h`, `src/pipeline_api.cpp`
- **Modify**: `src/audio_pipeline.h/.cpp` (output expansion, sink dispatch), `src/dac_hal.cpp` (register sinks after init), `src/main.cpp` (register API endpoints), `src/config.h` (`AUDIO_OUT_MAX_SINKS 4`)

---

## Phase 3: Per-Output DSP

### New lightweight float-native output DSP: `src/output_dsp.h/.cpp`

Separate from the existing pre-matrix DSP engine (`dsp_pipeline.h`, `DSP_MAX_CHANNELS=4`). Rationale: the existing engine works in int32 with a float bridge, is stereo-pair oriented, and has complex double-buffered state. Output DSP operates on **mono float channels** directly, post-matrix.

```
OUTPUT_DSP_MAX_CHANNELS = 8   (one per mono output in 8x8 matrix)
OUTPUT_DSP_MAX_STAGES   = 12  (crossover + EQ + limiter + gain per channel)
```

- Reuses existing `DspStageType` enum and biquad coefficient functions from `dsp_biquad_gen.h`
- `output_dsp_process(ch, float* buf, frames)` — in-place mono processing
- Double-buffered config with atomic swap (same pattern as existing DSP)
- Crossover convenience: `output_dsp_setup_crossover(subCh, mainCh, freq, type, order)`
- Persisted per-channel: `/output_dsp_ch{N}.json`

### Pipeline integration

New stage in `audio_pipeline_task_fn` between matrix and write:

```
pipeline_read_inputs()
pipeline_to_float()
pipeline_run_dsp()           ← existing per-input DSP (pre-matrix)
pipeline_mix_matrix()        ← 8x8 routing
pipeline_run_output_dsp()    ← NEW: per-output mono DSP
pipeline_write_output()      ← multi-sink dispatch
pipeline_update_metering()
```

### REST API
- `GET /api/output/dsp/{ch}` — channel config
- `PUT /api/output/dsp/{ch}` — set config
- `POST /api/output/dsp/{ch}/stage` — add stage
- `DELETE /api/output/dsp/{ch}/stage/{idx}` — remove stage
- `POST /api/output/dsp/{ch}/crossover` — convenience: `{pairCh, freq, type, order}`

### Files
- **New**: `src/output_dsp.h`, `src/output_dsp.cpp`
- **Modify**: `src/audio_pipeline.cpp` (add `pipeline_run_output_dsp()` call), `src/config.h` (constants), `src/app_state.h` (dirty flags)

---

## Phase 4: Web UI — Routing Matrix + Block Diagram

### Replace current 4x4 DSP routing UI with 8x8 pipeline matrix

**`web_src/js/18-dsp-routing.js`** — full rewrite:
- Dynamic NxM grid: rows = active output channels (from io_registry), columns = 8 input channels
- Mono channel labels: "ADC1 L", "ADC1 R", ..., "DAC0 L", "ES8311 R"
- Color-coded cells: active routes highlighted, click to edit gain (dB) or set Off
- Loads from `GET /api/pipeline/matrix`, updates via `POST /api/pipeline/matrix/cell`

### Per-output DSP panels: `web_src/js/19-output-dsp.js`
- Tab/accordion per active output mono channel
- Reuses PEQ + chain stage UI patterns from `16-dsp-peq.js` / `17-dsp-chain.js`
- Points at `/api/output/dsp/{ch}` endpoints

### Signal flow block diagram: `web_src/js/20-signal-flow.js`
- Read-only HTML/CSS block diagram showing: Inputs → Pre-DSP → Matrix → Output DSP → Sinks
- Updates from `/api/io/registry` and `/api/pipeline/sinks` data
- Clickable blocks navigate to relevant config tabs

### Files
- **Modify**: `web_src/js/18-dsp-routing.js` (rewrite), `web_src/index.html` (new tabs/panels)
- **New**: `web_src/js/19-output-dsp.js`, `web_src/js/20-signal-flow.js`
- **Run**: `node tools/find_dups.js` + `node tools/build_web_assets.js` after all JS changes

---

## Phase 5: Integration & Polish

### MQTT / HA Discovery
- Per-output-sink `number` entity for volume, `switch` for mute, `sensor` for VU
- Topics: `{base}/output/{sink_name}/volume/set`, `.../mute/set`, `.../vu`
- In `src/mqtt_handler.cpp`, extend `publishHADiscovery()` with io_registry loop

### Preset system expansion
- Extend existing DSP preset save/load to include: 8x8 matrix state, per-output DSP configs, I/O registry assignment
- REST: `GET/POST /api/pipeline/presets/{name}`

### Per-output VU metering
- Compute RMS of each `_outCh[]` in `pipeline_update_metering()`
- Store in `AudioOutputSink.vuL/vuR`
- Broadcast via WS: `{type:"sinkVu", sinks:[{name, vuL, vuR}]}`

### Dead Code Removal & Consolidation

Scan and clean at each phase. Known targets:

**Phase 2 — Remove when pipeline output expansion replaces hardcoded paths:**
- `dac_secondary_write()` / `dac_secondary_is_ready()` hardcoded calls in `pipeline_write_output()` → replaced by sink dispatch loop
- `_outL` / `_outR` static pointers → replaced by `_outCh[8]` array
- `_swapHoldL` / `_swapHoldR` single-pair hold buffers → expand or generalize to per-sink holds

**Phase 3 — Remove dead DSP routing code superseded by 8x8 pipeline matrix + output DSP:**
- `DspRoutingMatrix` struct in `dsp_crossover.h` — never wired into live audio path, superseded by `_matrixGain[8][8]`
- `dsp_routing_init()` / `dsp_routing_apply()` / `dsp_routing_set_preset()` in `dsp_crossover.cpp` — dead functions
- `_routingMatrix` static in `dsp_api.cpp` + `dsp_get_routing_matrix()` — orphaned state
- `GET/PUT /api/dsp/routing` endpoints in `dsp_api.cpp` — replaced by `/api/pipeline/matrix`
- `/dsp_routing.json` persistence in `loadRoutingMatrix()` / `saveRoutingMatrix()` — orphaned
- `dsp_routing` presets (identity, mono_sum, swap_lr, sub_sum) in `dsp_crossover.cpp` — moved to pipeline matrix presets

**Phase 4 — Web UI dead code:**
- Old `dspLoadRouting()` / `dspSaveRouting()` functions in `18-dsp-routing.js` → full rewrite replaces these
- `DSP_CH_NAMES` constant if superseded by dynamic names from io_registry

**General — scan for across all phases:**
- Unused `#include` directives after refactoring
- Orphaned AppState dirty flags/members if their consumers are removed
- Duplicate volume conversion functions (e.g., `dac_volume_to_linear` used by both primary and secondary → consolidate if now shared via sink interface)

---

## Phase Dependencies

```
Phase 0 (ES8311 fix) → Phase 1 (I/O Registry) → Phase 2 (Pipeline Expansion) → Phase 3 (Output DSP)
                                                                                      ↓
                                                  Phase 4 (Web UI) ← depends on Phase 2 + 3
                                                                                      ↓
                                                  Phase 5 (Integration) ← depends on all above
```

Phase 4 web UI can partially overlap with Phase 3 (matrix UI from Phase 2 data, output DSP panels added when Phase 3 lands).

---

## New Files Summary

| File | Phase | Purpose |
|---|---|---|
| `src/io_registry.h/.cpp` | 1 | Dynamic I/O device registry |
| `src/audio_output_sink.h` | 2 | Output sink vtable (mirrors AudioInputSource) |
| `src/pipeline_api.h/.cpp` | 2 | REST API for matrix + sinks |
| `src/output_dsp.h/.cpp` | 3 | Per-output mono float DSP chains |
| `web_src/js/14-io-registry.js` | 1 | I/O device management UI |
| `web_src/js/19-output-dsp.js` | 4 | Per-output DSP config panels |
| `web_src/js/20-signal-flow.js` | 4 | Signal flow block diagram |

## Key Existing Code to Reuse

| What | Where | Reuse in |
|---|---|---|
| `DacDriver` abstract class | `src/dac_hal.h` | Output sink drivers |
| `AudioInputSource` vtable struct | `src/audio_input_source.h` | Pattern for `AudioOutputSink` |
| `dac_eeprom_scan()` + ALXD format | `src/dac_eeprom.h/.cpp` | Extend to v2 with deviceType |
| `dac_registry` factory pattern | `src/dac_registry.cpp` | Sink driver lookup |
| `_matrixGain[8][8]` + SIMD mix | `src/audio_pipeline.cpp:300` | Already the right size, expand output loop |
| `dsp_biquad_gen.h` coefficients | `src/dsp_biquad_gen.h` | Output DSP biquad stages |
| `dsp_crossover` filter insertion | `src/dsp_crossover.cpp` | Output DSP crossover convenience API |
| `dac_save/load_settings` pattern | `src/dac_hal.cpp:392-467` | ES8311 persistence, output DSP persistence |
| `dsp_swap_config()` double-buffer | `src/dsp_pipeline.cpp` | Output DSP atomic config swap |

## Test Plan

Tests follow the project convention: Unity framework, native platform (`pio test -e native`), Arrange-Act-Assert pattern, mocks in `test/test_mocks/`. Each test module in its own directory. Current baseline: 932 tests.

### Phase 0 Tests — `test/test_dac_settings/`

**Unit tests (native):**
1. `test_es8311_settings_default` — Verify `es8311Enabled=false`, `es8311Volume=80`, `es8311Mute=false` as defaults
2. `test_es8311_settings_load` — Mock `/dac_config.json` with `{"es8311Enabled":true,"es8311Volume":60,"es8311Mute":true}` → verify AppState populated correctly
3. `test_es8311_settings_save` — Set AppState ES8311 fields → call `dac_save_settings()` → verify JSON output contains all three fields
4. `test_es8311_settings_roundtrip` — Save → clear → load → verify identical state
5. `test_es8311_settings_missing_fields` — Load JSON with only primary DAC fields → ES8311 fields retain defaults (backward compat)
6. `test_dac_secondary_init_disabled` — `es8311Enabled=false` → `dac_secondary_init()` returns without creating driver
7. `test_dac_secondary_is_ready_all_conditions` — Verify all 4 conditions required: `_secondaryI2sTxEnabled && _secondaryDriver && driver->isReady() && es8311Enabled`

**Hardware validation (P4 board):**
- Serial: `[DAC] Settings loaded: ... es8311Enabled=1`
- Serial: `[DAC] ES8311 secondary output initialized, vol=80%`
- Audio test: ADC1 input → audible on ES8311 speaker
- Persistence: toggle es8311Enabled via API, reboot, verify setting retained

### Phase 1 Tests — `test/test_io_registry/`

**Unit tests (native):**
1. `test_registry_init_empty` — No EEPROM, no manual config → registry has only builtins (2 inputs, 1-2 outputs depending on P4)
2. `test_registry_eeprom_v1_compat` — v1 EEPROM (no deviceType) → parsed as DAC (deviceType=0)
3. `test_registry_eeprom_v2_dac` — v2 EEPROM with deviceType=0 → registered as output
4. `test_registry_eeprom_v2_adc` — v2 EEPROM with deviceType=1 → registered as input
5. `test_registry_eeprom_v2_codec` — v2 EEPROM with deviceType=2 → registered as both input and output
6. `test_registry_channel_assignment` — slot 0→ch 0,1; slot 1→ch 2,3; slot 2→ch 4,5; slot 3→ch 6,7
7. `test_registry_manual_add_output` — Add manual output → appears in registry, persisted to JSON
8. `test_registry_manual_remove_output` — Remove output → slot freed, channels unassigned
9. `test_registry_max_outputs` — Adding 5th output fails gracefully (max 4)
10. `test_registry_persistence_roundtrip` — Save → clear → load → identical state
11. `test_registry_duplicate_slot` — Adding device to occupied slot replaces previous

**Hardware validation:**
- Boot with DAC EEPROM board → serial: `[IO] Discovered DAC: PCM5102A on I2S0`
- Boot without EEPROM → serial shows only builtins
- Web UI `/api/io/registry` returns correct JSON
- Manual add via POST → re-scan shows new entry

### Phase 2 Tests — `test/test_pipeline_output/`

**Unit tests (native):**
1. `test_output_buffers_allocated` — All 8 `_outCh[]` non-null after init
2. `test_matrix_8x8_identity` — Identity matrix: `_outCh[i]` == `_laneL[i/2]` or `_laneR[i/2]`
3. `test_matrix_8x8_gain` — Set gain 0.5 on cell [2][0] → output ch 2 = 0.5 × input ch 0
4. `test_matrix_8x8_summing` — Two inputs routed to same output → correct additive mix
5. `test_matrix_8x8_zero_skip` — Zero-gain cells produce no computation (verify output stays zero)
6. `test_matrix_mono_to_mono` — Single mono input → single mono output (subwoofer use case)
7. `test_sink_registration` — Register sink → `audio_pipeline_get_sink(0)` returns it
8. `test_sink_dispatch_stereo` — Sink at ch 0,1 receives interleaved `_outCh[0]` + `_outCh[1]`
9. `test_sink_dispatch_independent` — Sink 0 (ch 0-1) and sink 1 (ch 2-3) receive different audio when matrix routes differently
10. `test_sink_ready_check` — Unready sink skipped, no write called
11. `test_sink_mono` — Mono sink (channelCount=1) duplicates single channel into both L/R of interleaved output
12. `test_matrix_persistence_save` — `audio_pipeline_save_matrix()` writes valid JSON with 8x8 array
13. `test_matrix_persistence_load` — Load matrix JSON → all 64 cells restored correctly
14. `test_matrix_api_set_cell` — REST `POST /api/pipeline/matrix/cell` with `{out:2, in:0, gainDb:-6.0}` → correct linear gain in matrix

**Hardware validation:**
- API call: set `_matrixGain[2][0]=1.0, _matrixGain[3][1]=1.0` → ADC1 L/R routed to ES8311 (channels 2-3)
- API call: set `_matrixGain[0][0]=0, _matrixGain[1][1]=0` → primary DAC silent while ES8311 plays
- Confirm independent volume: adjust primary DAC volume → ES8311 unaffected
- VU metering: `/api/pipeline/sinks` shows per-sink VU levels

### Phase 3 Tests — `test/test_output_dsp/`

**Unit tests (native):**
1. `test_output_dsp_init` — All 8 channels initialized with 0 stages, bypass=true
2. `test_output_dsp_bypass` — Bypass=true → `output_dsp_process()` is a no-op (buffer unchanged)
3. `test_output_dsp_gain_stage` — Add gain stage (+6dB) → output amplitude doubles
4. `test_output_dsp_mute_stage` — Add mute stage → output all zeros
5. `test_output_dsp_biquad_lpf` — Add LPF at 1kHz → 100Hz sine passes, 5kHz sine attenuated by >20dB
6. `test_output_dsp_biquad_hpf` — Add HPF at 1kHz → 5kHz sine passes, 100Hz sine attenuated
7. `test_output_dsp_crossover_setup` — `output_dsp_setup_crossover(sub=2, main=0, 80Hz, LR4)` → ch 0 has HPF, ch 2 has LPF
8. `test_output_dsp_stage_add_remove` — Add 3 stages, remove middle → remaining 2 correct
9. `test_output_dsp_max_stages` — Adding 13th stage fails (max 12)
10. `test_output_dsp_stage_rollback` — If biquad coefficient generation fails, stage not added (pool unchanged)
11. `test_output_dsp_config_swap` — Double-buffer swap: modify inactive config → swap → active reflects changes
12. `test_output_dsp_persistence_roundtrip` — Save ch 0 config → clear → load → identical stages
13. `test_output_dsp_mono_processing` — Process 256-frame mono buffer in-place → no stereo artifacts
14. `test_output_dsp_multi_stage_chain` — LPF → gain → limiter chain → correct cumulative effect

**Hardware validation:**
- Apply LPF (80Hz, LR4) on output ch 2 (ES8311 L) → ES8311 outputs bass only
- Apply HPF (80Hz, LR4) on output ch 0 (DAC0 L) → primary DAC outputs mid/high only
- Sweep test tone → verify crossover frequency and slope
- Toggle output DSP bypass → hear difference

### Phase 4 Tests — Web UI

**Automated checks (build-time):**
1. `node tools/find_dups.js` → exit 0 (no duplicate declarations across new + existing JS files)
2. `node tools/check_missing_fns.js` → no undefined functions called from `02-ws-router.js` or HTML
3. `node tools/build_web_assets.js` → successful build, gzip output generated

**Manual UI validation:**
1. Matrix grid: correct number of rows (dynamic from io_registry) × 8 input columns
2. Matrix grid: click cell → enter gain in dB → cell updates → audio routing changes
3. Matrix grid: color coding — active routes visually distinct from inactive
4. Output DSP panels: one panel per active output channel
5. Output DSP: add/remove stages → changes reflected in audio
6. Signal flow diagram: shows all discovered I/O with connecting lines
7. Signal flow diagram: updates when io_registry changes (device added/removed)
8. Responsive: matrix and diagram usable on mobile viewport

### Phase 5 Tests — Integration

**Unit tests:**
1. `test_mqtt_ha_discovery_outputs` — HA discovery JSON contains per-sink entities (number, switch, sensor)
2. `test_preset_save_includes_matrix` — Preset JSON contains `matrix` key with 8x8 array
3. `test_preset_save_includes_output_dsp` — Preset JSON contains `outputDsp` key with per-channel configs
4. `test_preset_load_restores_all` — Load preset → matrix + output DSP + io_registry all restored

**Hardware validation:**
- HA: per-output volume slider appears in Home Assistant
- HA: toggle per-output mute → audio stops/starts on correct output
- Preset: save "2-way crossover" preset → load "flat" preset → load "2-way" again → crossover restored
- VU: web UI shows per-output VU meters updating in real-time

### Test Execution Commands

```bash
# Run all native tests (should pass at every phase gate)
pio test -e native

# Run specific phase test module
pio test -e native -f test_dac_settings
pio test -e native -f test_io_registry
pio test -e native -f test_pipeline_output
pio test -e native -f test_output_dsp

# Build firmware for P4
pio run -e esp32-p4

# Rebuild web assets
node tools/build_web_assets.js

# Check for JS issues
node tools/find_dups.js
node tools/check_missing_fns.js
```

### Phase Gate Criteria

Each phase must pass before the next begins:
1. All new tests pass (`pio test -e native`)
2. All existing 932+ tests still pass (no regressions)
3. Firmware builds cleanly (`pio run -e esp32-p4`)
4. Web assets build cleanly (`node tools/build_web_assets.js`)
5. Hardware validation items confirmed by user on P4 board
6. Phase committed to Dev branch
