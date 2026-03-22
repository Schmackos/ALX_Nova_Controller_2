---
title: Audio Matrix Routing
sidebar_position: 9
description: How the user configures the 32x32 audio routing matrix to connect input sources to output sinks.
---

# Audio Matrix Routing

The ALX Nova Controller 2 audio pipeline is structured as an 8-lane input stage → per-input DSP → 32×32 routing matrix → per-output DSP → 16-slot sink dispatch. The routing matrix is the central switching fabric: it determines which audio inputs are mixed into which outputs, with per-crosspoint gain control. Users configure it through the web UI's Audio tab, which renders an interactive matrix grid populated from a live REST snapshot.

The internal matrix is `AUDIO_PIPELINE_MATRIX_SIZE × AUDIO_PIPELINE_MATRIX_SIZE` (currently 16×16, covering 8 stereo-channel input pairs and 8 stereo-channel output pairs). Each cell stores a `float` gain value: `0.0` (silence / −∞ dB) through `1.0` (unity / 0 dB). The dB interface uses the conversion `gain_db <= -96.0 → 0.0`; values above −96 dB use `powf(10.0f, gain_db / 20.0f)`. Changes are a direct array write (no suspension needed for individual cells) and are therefore glitch-free at the sample level. `vTaskSuspendAll()` is used only for structural operations such as registering or removing sink and source slots.

The grid is populated dynamically: registered sources determine active rows; registered sinks determine active columns. A cell that crosses an unregistered lane/slot is harmlessly inactive. Configuration is saved to `/pipeline_matrix.json` with a 2-second debounce after the last cell change, using `audio_pipeline_save_matrix()` called from `pipeline_api_check_deferred_save()` in the main loop.

## Preconditions

- Audio pipeline initialized (`audio_pipeline_init()` completed at boot before WiFi connects).
- At least one input source registered (onboard PCM1808 ADCs are registered at boot; expansion ADC mezzanines add more lanes via HAL discovery).
- At least one output sink registered (onboard PCM5102A is registered at boot; expansion DAC mezzanines add more slots).
- Web UI authenticated and connected via WebSocket (port 81).

## Sequence Diagram

```mermaid
sequenceDiagram
    actor User
    participant WebUI as Web UI
    participant REST as REST API
    participant Pipeline as Audio Pipeline
    participant FS as LittleFS
    participant WS as WebSocket

    User->>WebUI: Navigate to Audio tab

    WebUI->>REST: GET /api/pipeline/matrix
    REST->>Pipeline: audio_pipeline_get_matrix_gain(o, i)<br/>for all o, i in AUDIO_PIPELINE_MATRIX_SIZE
    REST-->>WebUI: JSON {matrix: [[...]], inputs: [...], outputs: [...], bypass: false}
    Note over WebUI: matrix is a 2D float array.<br/>inputs/outputs are channel-name arrays.

    WebUI->>REST: GET /api/pipeline/sinks
    REST->>Pipeline: audio_pipeline_get_sink(s) for each registered slot
    REST-->>WebUI: JSON [{name, firstChannel, channelCount, gainLinear, muted, ready, vuL, vuR}, ...]

    WebUI->>WebUI: Render interactive matrix grid<br/>(rows = input channels, columns = output channels)<br/>Active rows/columns from audioChannelMap broadcast

    rect rgb(30, 40, 60)
        Note over User,WebUI: User edits a crosspoint
        User->>WebUI: Click matrix cell (e.g., ADC1 L → Out 2)
        WebUI->>WebUI: Show gain slider / dB input for that crosspoint
        User->>WebUI: Adjust gain (e.g., 0 dB = unity, -6 dB, or -inf = mute)
        User->>WebUI: Confirm / slider auto-saves
    end

    WebUI->>REST: POST /api/pipeline/matrix/cell<br/>{out: 2, in: 0, gainDb: 0.0}
    Note over REST: out and in are channel indices (0 to AUDIO_PIPELINE_MATRIX_SIZE-1).<br/>gainDb: -96.0 or lower = silence; 0.0 = unity gain.

    REST->>REST: Validate: out_ch and in_ch both in [0, AUDIO_PIPELINE_MATRIX_SIZE)
    alt Validation fails
        REST-->>WebUI: 400 {error: "channel out of range"}
    else Validation passes
        REST->>Pipeline: audio_pipeline_set_matrix_gain_db(out_ch, in_ch, gainDb)
        Note over Pipeline: Converts dB to linear: gain_db <= -96 → 0.0,<br/>else powf(10.0f, gain_db / 20.0f).<br/>Direct array write to _matrixGain[out][in] — no suspension needed.
        REST->>REST: _matrixSavePending = millis() + 2000
        REST-->>WebUI: 200 {status:"ok", out, in, gainDb, gainLinear}
    end

    Note over REST,FS: 2 seconds after last cell change (main loop drain)
    REST->>REST: pipeline_api_check_deferred_save()<br/>millis() >= _matrixSavePending
    REST->>Pipeline: audio_pipeline_save_matrix()
    Pipeline->>FS: Write /pipeline_matrix.json
    FS-->>Pipeline: ok
    Pipeline->>Pipeline: AppState::getInstance().markChannelMapDirty()
    Note over Pipeline,WS: EVT_CHANNEL_MAP signalled — main loop wakes immediately

    WS-->>WebUI: audioChannelMap broadcast<br/>{type:"audioChannelMap", inputs:[...], outputs:[...]}
    WebUI->>WebUI: Update matrix grid gain indicators<br/>to reflect confirmed server state
```

## Step-by-Step Walkthrough

### 1. Navigate to the Audio tab

The user opens the web UI (port 80) and selects the Audio tab. The JavaScript module `web_src/js/05-audio-tab.js` initialises the matrix grid. On tab activation it fetches the current matrix state from two REST endpoints.

### 2. Fetch the current matrix — GET /api/pipeline/matrix

`GET /api/pipeline/matrix` is handled in `src/pipeline_api.cpp` (`registerPipelineApiEndpoints()`). The handler iterates all `AUDIO_PIPELINE_MATRIX_SIZE × AUDIO_PIPELINE_MATRIX_SIZE` combinations and calls `audio_pipeline_get_matrix_gain(o, i)` for each cell. The response is a JSON object with three arrays:

- `matrix` — a 2D array of linear float gains (size `N×N`, row-major: `matrix[out][in]`).
- `inputs` — channel name strings (`"ADC1 L"`, `"ADC1 R"`, `"ADC2 L"`, `"ADC2 R"`, `"SigGen L"`, `"SigGen R"`, `"USB L"`, `"USB R"`).
- `outputs` — output channel name strings (`"Out 0 (L)"` through `"Out 7"`).
- `bypass` — boolean indicating whether the matrix is in bypass mode (`audio_pipeline_is_matrix_bypass()`).

### 3. Fetch sink metadata — GET /api/pipeline/sinks

`GET /api/pipeline/sinks` returns the registered output sinks. Each sink entry includes `name`, `firstChannel`, `channelCount`, `gainLinear`, `muted`, `ready` (result of calling `sink->isReady()`), `vuL`, and `vuR`. This lets the web UI highlight columns that correspond to a live, ready output device.

### 4. Render the matrix grid

The web UI renders a grid where rows represent input channels and columns represent output sinks. The active row count and input-lane names come from the most recent `audioChannelMap` WebSocket broadcast (`src/websocket_broadcast.cpp`), which is delivered on connect and re-sent whenever source or sink registration changes. Cells where both lane and slot are active display the current gain value; inactive lane/slot intersections are visually dimmed.

### 5. User selects a crosspoint

Clicking a cell opens a gain control (slider and numeric input). The dB scale is used for display; −∞ dB (shown as "−inf" or "Mute") maps to `gainDb = -96.0`, which the firmware converts to `0.0` linear. Unity gain is `0 dB` / `1.0` linear.

### 6. Send the cell update — POST /api/pipeline/matrix/cell

On confirmation the web UI sends:

```json
POST /api/pipeline/matrix/cell
{"out": 2, "in": 0, "gainDb": 0.0}
```

`out` and `in` are zero-based channel indices within the internal matrix. The firmware parses the body using `ArduinoJson`, extracts `out` (default `−1`), `in` (default `−1`), and `gainDb` (default `−96.0`).

### 7. Index validation

Before touching the matrix the handler checks:

```cpp
if (out_ch < 0 || out_ch >= AUDIO_PIPELINE_MATRIX_SIZE ||
    in_ch  < 0 || in_ch  >= AUDIO_PIPELINE_MATRIX_SIZE) {
    server_send(400, "application/json", "{\"error\":\"channel out of range\"}");
    return;
}
```

Any out-of-range index returns HTTP 400 immediately.

### 8. Apply the gain — audio_pipeline_set_matrix_gain_db()

`audio_pipeline_set_matrix_gain_db(out_ch, in_ch, gainDb)` in `src/audio_pipeline.cpp` converts to linear:

```cpp
float linear = (gain_db <= -96.0f) ? 0.0f : powf(10.0f, gain_db / 20.0f);
audio_pipeline_set_matrix_gain(out_ch, in_ch, linear);
```

`audio_pipeline_set_matrix_gain()` performs a direct write to `_matrixGain[out_ch][in_ch]`. Because this is a single `float` array store, the audio pipeline task on Core 1 picks it up on the very next DMA callback without any suspension or handshake. Individual cell updates are atomic at the processor word level on the ESP32-P4.

### 9. Schedule the deferred save

After updating the gain, the REST handler sets:

```cpp
_matrixSavePending = millis() + MATRIX_SAVE_DELAY_MS;  // +2000 ms
```

If the user adjusts multiple cells in quick succession, each POST resets this deadline. The 2-second debounce means LittleFS is only written once per editing session, not once per keypress.

### 10. REST response

The handler responds with HTTP 200 and a confirmation payload:

```json
{"status": "ok", "out": 2, "in": 0, "gainDb": 0.0, "gainLinear": 1.0}
```

`gainLinear` is the value read back from `audio_pipeline_get_matrix_gain()` after the write, confirming the stored value.

### 11. Deferred save — audio_pipeline_save_matrix()

`pipeline_api_check_deferred_save()` is called from the main loop on every `app_events_wait(5)` wakeup. When `millis() >= _matrixSavePending` and a save is pending, it calls `audio_pipeline_save_matrix()`.

`audio_pipeline_save_matrix()` serialises the full `_matrixGain` array to `/pipeline_matrix.json` on LittleFS. After a successful write it calls `AppState::getInstance().markChannelMapDirty()`, which sets the `_channelMapDirty` flag and signals `EVT_CHANNEL_MAP`. The main loop wakes immediately on the event and schedules a `sendAudioChannelMap()` WebSocket broadcast.

Note: the matrix file is written directly (not via tmp+rename) because a corrupt matrix on boot is recoverable — the pipeline starts with all-zero gains if the file cannot be parsed, and the user can re-apply their configuration.

### 12. WebSocket broadcast — audioChannelMap

`sendAudioChannelMap()` in `src/websocket_broadcast.cpp` serialises the current input/output lane assignments and sends a JSON frame of type `"audioChannelMap"` to all authenticated WebSocket clients. The web UI's `02-ws-router.js` routes this to the Audio tab handler, which refreshes the matrix grid to show the confirmed post-save state.

## Postconditions

- The crosspoint gain is applied immediately in the live audio pipeline — no restart required.
- Audio from the selected input channel flows to the selected output channel at the configured level from the very next DMA callback (~5 ms at 48 kHz with default DMA buffer sizing).
- Configuration is persisted to `/pipeline_matrix.json` after the 2-second debounce expires.
- All connected WebSocket clients receive an updated `audioChannelMap` broadcast after the save.

## Error Scenarios

| Trigger | Behaviour | Recovery |
|---------|-----------|----------|
| `out` or `in` index out of range | REST returns HTTP 400 `\{"error":"channel out of range"\}` | Verify the channel count; check that `out` and `in` are in `[0, AUDIO_PIPELINE_MATRIX_SIZE)` |
| Request body missing or unparseable | REST returns HTTP 400 `\{"error":"no body"\}` or `\{"error":"parse error"\}` | Ensure the POST body is valid JSON with `Content-Type: application/json` |
| No input sources registered | Matrix has no active rows; `GET /api/pipeline/matrix` still succeeds (zero-gain full matrix) | Insert an ADC mezzanine card and run `POST /api/hal/scan`, or ensure the signal generator is enabled |
| No output sinks registered | Matrix has no active columns; audio is computed but discarded | Insert a DAC mezzanine card or verify the onboard PCM5102A is in `AVAILABLE` state |
| Pipeline not initialized | `audio_pipeline_set_matrix_gain_db()` is a no-op (array bounds guard exits early) | Wait for the boot sequence to complete; check diagnostic journal for `DIAG_AUDIO_DMA_ALLOC_FAIL` |
| LittleFS save failure | `audio_pipeline_save_matrix()` logs `LOG_W`; `_matrixSavePending` is cleared | Matrix remains active in RAM for the current session but is not persisted; re-apply on next boot |
| Rapid repeated cell edits | Each POST resets `_matrixSavePending`; only one LittleFS write occurs per editing session | Expected behaviour — no action needed |

## Key Source Locations

| Concern | File |
|---------|------|
| REST endpoint registration | `src/pipeline_api.cpp` — `registerPipelineApiEndpoints()` |
| Deferred save check | `src/pipeline_api.cpp` — `pipeline_api_check_deferred_save()` |
| Matrix gain read/write | `src/audio_pipeline.cpp` — `audio_pipeline_set_matrix_gain_db()`, `audio_pipeline_get_matrix_gain()` |
| Matrix persistence | `src/audio_pipeline.cpp` — `audio_pipeline_save_matrix()` |
| Channel map dirty flag | `src/app_state.h` — `markChannelMapDirty()`, `EVT_CHANNEL_MAP` |
| WebSocket broadcast | `src/websocket_broadcast.cpp` — `sendAudioChannelMap()` |
| Web UI matrix grid | `web_src/js/05-audio-tab.js` |

## Related

- [PEQ / DSP Configuration](dsp-peq-config) — per-channel DSP applied before the matrix (input DSP) and after it (output DSP)
- [Mezzanine ADC Card Insertion](mezzanine-adc-insert) — how new input sources are added to the matrix
- [Mezzanine DAC Card Insertion](mezzanine-dac-insert) — how new output sinks are added to the matrix
- [Device Enable/Disable Toggle](device-toggle) — disabling a device removes its lane or sink slot from the matrix
- [Audio Pipeline](../audio-pipeline) — full pipeline architecture including DSP stages and sink dispatch
- [REST API (Pipeline)](../api/rest-pipeline) — complete matrix and output-DSP endpoint reference
- [WebSocket Protocol](../websocket) — `audioChannelMap` message format and field definitions
