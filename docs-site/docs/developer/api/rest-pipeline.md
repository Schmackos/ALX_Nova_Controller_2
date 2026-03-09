---
title: REST API — Pipeline
sidebar_position: 4
description: Audio pipeline routing matrix REST API endpoints.
---

The Pipeline API controls the 16×16 gain routing matrix that sits between the input DSP stage and the output sink dispatch. Each cell in the matrix stores a linear gain value (float32, range 0.0–1.0). The API also exposes registered output sinks and their real-time VU state, and provides output DSP configuration for per-sink post-processing. All endpoints require the `DAC_ENABLED` build flag.

## Endpoint summary

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/pipeline/matrix` | No | Get full routing matrix and channel names |
| POST | `/api/pipeline/matrix/cell` | No | Set a single matrix cell by dB value |
| GET | `/api/pipeline/sinks` | No | List registered output sinks with VU state |
| GET | `/api/output/dsp` | No | Get output DSP config for a channel |
| POST | `/api/output/dsp/stage` | No | Add an output DSP stage |
| DELETE | `/api/output/dsp/stage` | No | Remove an output DSP stage |
| PUT | `/api/output/dsp` | No | Update output DSP channel settings |
| POST | `/api/output/dsp/crossover` | No | Configure an output crossover |

---

## Matrix format

The routing matrix is a 2D array of `AUDIO_PIPELINE_MATRIX_SIZE × AUDIO_PIPELINE_MATRIX_SIZE` linear gain values. The current matrix size is 16×16.

- **Rows** represent output channels (sinks), indexed 0–15.
- **Columns** represent input channels (sources), indexed 0–15.
- A value of `1.0` is unity gain (0 dB). A value of `0.0` is silence (−∞ dB).
- The typical passthrough configuration has `1.0` on the diagonal.

### Input channel assignments (default layout)

The tables below show the default channel layout for a standard two-ADC build. **The actual layout is dynamic** — HAL assigns input lanes and output slots at runtime based on which devices are discovered and available. Always consult the `audioChannelMap` WebSocket broadcast for the live channel map; do not hard-code these indices.

| Index | Default name |
|-------|------|
| 0 | ADC1 L |
| 1 | ADC1 R |
| 2 | ADC2 L |
| 3 | ADC2 R |
| 4 | SigGen L |
| 5 | SigGen R |
| 6 | USB L |
| 7 | USB R |
| 8–15 | Populated dynamically by HAL at runtime |

### Output channel assignments (default layout)

| Index | Default name |
|-------|------|
| 0 | Out 0 (L) |
| 1 | Out 1 (R) |
| 2 | Out 2 |
| 3 | Out 3 |
| 4–7 | Out 4–7 |
| 8–15 | Populated dynamically by HAL at runtime |

:::note Live channel map via WebSocket
The `audioChannelMap` broadcast is the authoritative source for currently active input lanes and output sinks, including HAL device metadata (`compatible`, `manufacturer`, `capabilities`, `ready`). Use `GET /api/pipeline/matrix` for the stored gain values and `audioChannelMap` for what those indices actually map to.
:::

### Backward compatibility: 8×8 to 16×16 migration

When loading a matrix persisted by firmware older than v1.12.0 (which used an 8×8 matrix), the values are placed in the top-left 8×8 corner of the 16×16 matrix and the remaining cells are zero-filled. The API always reads and writes the full 16×16 matrix.

---

## GET /api/pipeline/matrix

Returns the complete routing matrix, channel names, and bypass state.

**Response**

```json
{
  "matrix": [
    [1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
  ],
  "inputs": [
    "ADC1 L", "ADC1 R", "ADC2 L", "ADC2 R",
    "SigGen L", "SigGen R", "USB L", "USB R"
  ],
  "outputs": [
    "Out 0 (L)", "Out 1 (R)", "Out 2", "Out 3",
    "Out 4", "Out 5", "Out 6", "Out 7"
  ],
  "bypass": false
}
```

The `matrix` array is abbreviated to 4 rows above for readability. The full response contains 16 rows each with 16 values.

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success |

---

## POST /api/pipeline/matrix/cell

Sets the gain for a single routing matrix cell. The gain is specified in dB and is converted to a linear value internally. Changes are persisted to LittleFS after a 2-second debounce. The response echoes back both the dB input and the resulting linear gain stored.

**Request**

```json
{
  "out": 0,
  "in": 0,
  "gainDb": 0.0
}
```

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `out` | integer | 0–15 | Output channel index |
| `in` | integer | 0–15 | Input channel index |
| `gainDb` | float | −96.0 to +6.0 | Gain in dB (−96 dB ≈ silence) |

**Response**

```json
{
  "status": "ok",
  "out": 0,
  "in": 0,
  "gainDb": 0.0,
  "gainLinear": 1.0
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Cell updated |
| 400 | Missing body, parse error, or channel index out of range |

:::note
Matrix saves are deferred 2 seconds after the last cell change. Rapid sequential cell updates will coalesce into a single LittleFS write. Call `pipeline_api_check_deferred_save()` from the main loop to flush the pending save.
:::

---

## GET /api/pipeline/sinks

Returns all registered output sinks with their current VU metering state and readiness.

**Response**

```json
[
  {
    "name": "PCM5102A",
    "firstChannel": 0,
    "channelCount": 2,
    "gainLinear": 1.0,
    "muted": false,
    "ready": true,
    "vuL": -18.5,
    "vuR": -19.2
  },
  {
    "name": "ES8311",
    "firstChannel": 2,
    "channelCount": 2,
    "gainLinear": 0.794,
    "muted": false,
    "ready": true,
    "vuL": -24.1,
    "vuR": -24.8
  }
]
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Sink name from HAL device |
| `firstChannel` | integer | First output channel index this sink occupies |
| `channelCount` | integer | Number of channels handled by this sink |
| `gainLinear` | float | Output gain (linear, 0.0–1.0+) |
| `muted` | boolean | Whether the sink is muted |
| `ready` | boolean | Whether the sink `isReady()` callback returns true |
| `vuL` | float | Left channel VU in dBFS |
| `vuR` | float | Right channel VU in dBFS |

---

## Output DSP endpoints

The output DSP is a per-sink post-matrix processing stage supporting the same stage types as the input DSP. Changes use the same inactive-buffer/swap pattern. The `ch` parameter is the output channel index (0–`OUTPUT_DSP_MAX_CHANNELS`).

### Stage types supported by output DSP

`LPF`, `HPF`, `BPF`, `PEQ`, `NOTCH`, `LOW_SHELF`, `HIGH_SHELF`, `LIMITER`, `GAIN`, `POLARITY`, `MUTE`, `COMPRESSOR`

---

## GET /api/output/dsp

Returns the output DSP configuration for a single channel.

**Query parameter**: `?ch=0`

**Response**

```json
{
  "channel": 0,
  "bypass": false,
  "globalBypass": false,
  "sampleRate": 48000,
  "stages": [
    {
      "index": 0,
      "enabled": true,
      "type": "LIMITER",
      "label": "Output limiter",
      "thresholdDb": -0.3,
      "attackMs": 0.5,
      "releaseMs": 200.0,
      "ratio": 20.0,
      "gainReduction": 0.0
    }
  ]
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success |
| 400 | Invalid or missing `ch` parameter |

---

## POST /api/output/dsp/stage

Adds a stage to an output DSP channel.

**Request**

```json
{
  "ch": 0,
  "type": "LIMITER",
  "position": -1
}
```

| Field | Type | Description |
|-------|------|-------------|
| `ch` | integer | Output channel index |
| `type` | string | Stage type string (see stage type table above) |
| `position` | integer | Insert position; −1 = append at end |

**Response**

```json
{
  "status": "ok",
  "ch": 0,
  "index": 1,
  "type": "LIMITER"
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Stage added |
| 400 | No body, parse error, invalid channel, or add failed |

---

## DELETE /api/output/dsp/stage

Removes a stage from an output DSP channel.

**Request**

```json
{ "ch": 0, "index": 1 }
```

**Response**

```json
{ "status": "ok" }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Stage removed |
| 400 | No body, parse error, invalid channel, or remove failed |

---

## PUT /api/output/dsp

Updates output DSP channel configuration. Supports changing the bypass flag, the global bypass flag, and toggling individual stage enable state.

**Request**

```json
{
  "ch": 0,
  "bypass": false,
  "globalBypass": false,
  "stageIndex": 0,
  "stageEnabled": true
}
```

All fields except `ch` are optional.

**Response**

```json
{ "status": "ok" }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Configuration applied |
| 400 | No body, parse error, or invalid channel |

---

## POST /api/output/dsp/crossover

Configures a 2-channel crossover on the output DSP. An LPF is inserted on `subCh` (subwoofer) and an HPF is inserted on `mainCh` (satellite), both at the specified frequency. The `order` parameter controls the filter slope (2 = LR2, 4 = LR4, 8 = LR8).

**Request**

```json
{
  "subCh": 0,
  "mainCh": 1,
  "freqHz": 80.0,
  "order": 4
}
```

**Response**

```json
{
  "status": "ok",
  "stagesAdded": 4
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Crossover configured; `stagesAdded` is the total stages inserted across both channels |
| 400 | No body, parse error, or crossover setup failed |
