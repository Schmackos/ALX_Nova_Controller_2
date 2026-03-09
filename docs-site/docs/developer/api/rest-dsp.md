---
title: REST API — DSP
sidebar_position: 3
description: DSP pipeline configuration REST API endpoints.
---

The DSP API configures the per-input biquad IIR/FIR processing chain. Each input channel gets an independent stage list. All mutation endpoints write to an inactive double-buffer and then call `dsp_swap_config()` to make the new config live with no glitches. Saves to LittleFS (`/dsp_global.json`, `/dsp_ch<N>.json`, `/dsp_fir<N>.bin`) are debounced 5 seconds after the last write. All endpoints require authentication.

## Endpoint summary

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/dsp` | Yes | Get full DSP configuration for all channels |
| PUT | `/api/dsp` | Yes | Replace full DSP configuration |
| POST | `/api/dsp/bypass` | Yes | Set or toggle global bypass |
| GET | `/api/dsp/metrics` | Yes | Get DSP CPU load and limiter gain reduction |
| GET | `/api/dsp/channel` | Yes | Get single channel configuration |
| POST | `/api/dsp/channel/bypass` | Yes | Set or toggle per-channel bypass |
| POST | `/api/dsp/stage` | Yes | Add a stage to a channel |
| PUT | `/api/dsp/stage` | Yes | Update stage parameters |
| DELETE | `/api/dsp/stage` | Yes | Remove a stage |
| POST | `/api/dsp/stage/reorder` | Yes | Reorder stages on a channel |
| POST | `/api/dsp/stage/enable` | Yes | Enable or toggle a stage |
| POST | `/api/dsp/crossover` | Yes | Apply a crossover filter to a channel |
| POST | `/api/dsp/bassmanagement` | Yes | Configure bass management routing |
| POST | `/api/dsp/bafflestep` | Yes | Apply baffle step correction |
| POST | `/api/dsp/import/apo` | Yes | Import Equalizer APO filter text |
| POST | `/api/dsp/import/minidsp` | Yes | Import miniDSP biquad coefficients |
| POST | `/api/dsp/import/fir` | Yes | Import FIR filter coefficients (text format) |
| GET | `/api/dsp/export/apo` | Yes | Export channel config as Equalizer APO text |
| GET | `/api/dsp/export/minidsp` | Yes | Export channel config in miniDSP format |
| GET | `/api/dsp/export/json` | Yes | Export full config as JSON |
| GET | `/api/dsp/peq/presets` | Yes | List saved PEQ preset names |
| POST | `/api/dsp/peq/presets` | Yes | Save a PEQ preset |
| GET | `/api/thd` | Yes | Get THD+N measurement result |
| POST | `/api/thd` | Yes | Start a THD+N measurement |
| DELETE | `/api/thd` | Yes | Stop a THD+N measurement |

---

## Double-buffer swap mechanism

All write endpoints use the following pattern:

1. `dsp_copy_active_to_inactive()` — copy live state to the inactive buffer
2. Modify the inactive buffer
3. `dsp_swap_config()` — atomically swap buffers (returns `false` if the audio task is busy)

If `dsp_swap_config()` returns `false`, the endpoint returns HTTP 503. Clients should retry after a short delay (typically under 10 ms). The active config is never partially modified.

:::note
Saves to LittleFS are debounced — the file is not written until 5 seconds after the last API call that requested a save. Do not power-cycle the device immediately after making DSP changes.
:::

---

## Stage type reference

The `type` field in all stage objects is a string:

### Biquad filter types

| Type string | Description |
|-------------|-------------|
| `LPF` | Low-pass filter (2nd order) |
| `HPF` | High-pass filter (2nd order) |
| `BPF` | Band-pass filter (constant skirt gain) |
| `BPF_0DB` | Band-pass filter (0 dB peak) |
| `PEQ` | Parametric EQ (peaking bell) |
| `NOTCH` | Notch filter |
| `LOW_SHELF` | Low-shelf filter |
| `HIGH_SHELF` | High-shelf filter |
| `ALLPASS` | All-pass (phase shift) |
| `ALLPASS_360` | All-pass 360° variant |
| `ALLPASS_180` | All-pass 180° variant |
| `LINKWITZ` | Linkwitz transform (resonance correction) |
| `LPF_1ST` | 1st-order low-pass |
| `HPF_1ST` | 1st-order high-pass |
| `CUSTOM` | Raw biquad coefficients `[b0, b1, b2, a1, a2]` |

### Non-biquad types

| Type string | Parameters |
|-------------|------------|
| `GAIN` | `gainDb` |
| `DELAY` | `delaySamples` (max: `DSP_MAX_DELAY_SAMPLES`) |
| `POLARITY` | `inverted` (boolean) |
| `MUTE` | `muted` (boolean) |
| `LIMITER` | `thresholdDb`, `attackMs`, `releaseMs`, `ratio` |
| `COMPRESSOR` | `thresholdDb`, `attackMs`, `releaseMs`, `ratio`, `kneeDb`, `makeupGainDb` |
| `NOISE_GATE` | `thresholdDb`, `attackMs`, `holdMs`, `releaseMs`, `ratio`, `rangeDb` |
| `FIR` | Loaded via `/api/dsp/import/fir` — not directly settable through stage add |
| `TONE_CTRL` | `bassGain`, `midGain`, `trebleGain` |
| `SPEAKER_PROT` | `powerRatingW`, `impedanceOhms`, `thermalTauMs`, `excursionLimitMm`, `driverDiameterMm`, `maxTempC` |
| `STEREO_WIDTH` | `width`, `centerGainDb` |
| `LOUDNESS` | `referenceLevelDb`, `currentLevelDb`, `amount` |
| `BASS_ENHANCE` | `frequency`, `harmonicGainDb`, `mix`, `order` |
| `MULTIBAND_COMP` | `numBands` |

---

## GET /api/dsp

Returns the complete DSP state for all channels, including the active config exported as JSON and the `dspEnabled` flag.

**Response**

```json
{
  "dspEnabled": true,
  "globalBypass": false,
  "sampleRate": 48000,
  "channels": [
    {
      "channel": 0,
      "bypass": false,
      "stageCount": 2,
      "stages": [
        {
          "index": 0,
          "type": "HPF",
          "enabled": true,
          "label": "Subsonic",
          "frequency": 20.0,
          "gain": 0.0,
          "Q": 0.707
        },
        {
          "index": 1,
          "type": "PEQ",
          "enabled": true,
          "label": "Room mode",
          "frequency": 63.0,
          "gain": -4.5,
          "Q": 2.0
        }
      ]
    }
  ]
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success |
| 503 | Out of memory allocating export buffer |

---

## PUT /api/dsp

Replaces the complete DSP configuration. The body format matches the output of `GET /api/dsp`. All channel configs are loaded into the inactive buffer, coefficients are recomputed, and the config is swapped live.

**Request**: same structure as the response to `GET /api/dsp`.

**Response**

```json
{ "success": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Configuration applied |
| 400 | No body or invalid JSON |
| 503 | DSP busy — swap failed; retry |

---

## POST /api/dsp/bypass

Sets or toggles the global DSP bypass flag. If a body is provided, the `bypass` and `enabled` fields are applied. If no body is provided, the global bypass is toggled.

**Request (optional)**

```json
{
  "bypass": true,
  "enabled": false
}
```

**Response**

```json
{ "success": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | State applied |
| 503 | DSP busy — swap failed; retry |

---

## GET /api/dsp/metrics

Returns DSP processing load and per-channel limiter gain reduction.

**Response**

```json
{
  "processTimeUs": 1240,
  "maxProcessTimeUs": 1880,
  "cpuLoad": 12.5,
  "limiterGr": [0.0, 0.0, -2.3, 0.0]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `processTimeUs` | integer | Last frame processing time in microseconds |
| `maxProcessTimeUs` | integer | Peak processing time since boot |
| `cpuLoad` | float | Estimated DSP CPU load percentage |
| `limiterGr` | float array | Per-channel limiter gain reduction in dB (0 = no limiting) |

---

## GET /api/dsp/channel

Returns the configuration for a single channel. Use the `ch` query parameter to select the channel.

**Query parameter**: `?ch=0`

**Response**

```json
{
  "channel": 0,
  "bypass": false,
  "stageCount": 1,
  "stages": [
    {
      "index": 0,
      "type": "PEQ",
      "enabled": true,
      "label": "",
      "frequency": 1000.0,
      "gain": 3.0,
      "Q": 1.0
    }
  ]
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success |
| 400 | Missing or invalid `ch` parameter |
| 503 | Out of memory |

---

## POST /api/dsp/channel/bypass

Sets or toggles the bypass flag for a specific channel.

**Query parameter**: `?ch=0`

**Request (optional)**

```json
{ "bypass": true }
```

**Response**

```json
{ "success": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Bypass set |
| 400 | Invalid channel |
| 503 | DSP busy; retry |

---

## POST /api/dsp/stage

Adds a new processing stage to a channel. The stage is inserted at `position` (default: appended at end). Biquad coefficients are computed immediately from the provided `params`.

**Query parameter**: `?ch=0`

**Request**

```json
{
  "type": "PEQ",
  "position": -1,
  "enabled": true,
  "label": "Bass boost",
  "params": {
    "frequency": 80.0,
    "gain": 4.0,
    "Q": 0.7
  }
}
```

For a LIMITER:

```json
{
  "type": "LIMITER",
  "params": {
    "thresholdDb": -6.0,
    "attackMs": 1.0,
    "releaseMs": 100.0,
    "ratio": 10.0
  }
}
```

For a GAIN:

```json
{
  "type": "GAIN",
  "params": { "gainDb": -3.0 }
}
```

For a DELAY:

```json
{
  "type": "DELAY",
  "params": { "delaySamples": 48 }
}
```

**Response**

```json
{ "success": true, "index": 2 }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Stage added; `index` is the assigned stage position |
| 400 | Invalid channel, invalid JSON, or max stages reached |
| 503 | DSP busy; retry |

:::note
When a channel has stereo-link enabled, the partner channel is automatically mirrored after each stage add, update, or remove.
:::

---

## PUT /api/dsp/stage

Updates parameters on an existing stage. Biquad coefficients are recomputed automatically. For `CUSTOM` biquad type, provide raw coefficients in the `coeffs` array.

**Query parameters**: `?ch=0&stage=1`

**Request**

```json
{
  "enabled": true,
  "label": "Tweeter HPF",
  "params": {
    "frequency": 3000.0,
    "gain": 0.0,
    "Q": 0.707
  }
}
```

For `CUSTOM` type:

```json
{
  "params": {
    "coeffs": [1.0, -1.98, 0.98, -1.98, 0.98]
  }
}
```

**Response**

```json
{ "success": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Stage updated |
| 400 | Invalid channel, invalid stage index, or invalid JSON |
| 503 | DSP busy; retry |

---

## DELETE /api/dsp/stage

Removes a stage from a channel. Later stages shift down by one index.

**Query parameters**: `?ch=0&stage=1`

**Response**

```json
{ "success": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Stage removed |
| 400 | Invalid channel or stage index |
| 503 | DSP busy; retry |

---

## POST /api/dsp/stage/reorder

Reorders the stages on a channel by providing a new index permutation. All existing stage indices must appear exactly once in the `order` array.

**Query parameter**: `?ch=0`

**Request**

```json
{ "order": [2, 0, 1] }
```

**Response**

```json
{ "success": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Stages reordered |
| 400 | Invalid channel, missing `order` array, or invalid permutation |
| 503 | DSP busy; retry |

---

## POST /api/dsp/stage/enable

Enables or toggles a specific stage. If no body is provided, the stage is toggled.

**Query parameters**: `?ch=0&stage=1`

**Request (optional)**

```json
{ "enabled": false }
```

**Response**

```json
{ "success": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | State applied |
| 400 | Invalid channel or stage index |
| 503 | DSP busy; retry |

---

## POST /api/dsp/crossover

Applies a crossover filter to a channel. All existing crossover-tagged stages on the channel are cleared first. Supported types: `lr2`, `lr4`, `lr8` (Linkwitz-Riley 2nd/4th/8th order), `bw2`–`bw8` (Butterworth), `bessel4` (Bessel 4th order).

**Query parameter**: `?ch=0`

**Request**

```json
{
  "freq": 2500.0,
  "role": 1,
  "type": "lr4"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `freq` | float | Crossover frequency in Hz |
| `role` | integer | 0 = LPF (subwoofer/bass), 1 = HPF (tweeter/midrange) |
| `type` | string | Filter topology: `lr2`, `lr4`, `lr8`, `bw2`–`bw8`, `bessel4` |

**Response**

```json
{ "success": true, "firstStage": 0 }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Crossover applied |
| 400 | Invalid channel, missing body, unknown type, or setup failed |
| 503 | DSP busy; retry |

---

## POST /api/dsp/bassmanagement

Configures bass management: an LPF is inserted on the subwoofer channel and an HPF is inserted on each main channel, all at the same crossover frequency.

**Request**

```json
{
  "subChannel": 0,
  "freq": 80.0,
  "mainChannels": [2, 3]
}
```

**Response**

```json
{ "success": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Bass management configured |
| 400 | Invalid body, missing `mainChannels`, or setup failed |
| 503 | DSP busy; retry |

---

## POST /api/dsp/bafflestep

Computes and inserts a high-shelf filter to correct for baffle step diffraction. The correction frequency and gain are derived from the enclosure width using the standard formula.

**Query parameter**: `?ch=0`

**Request**

```json
{ "baffleWidthMm": 250.0 }
```

**Response**

```json
{
  "success": true,
  "frequency": 548.0,
  "gainDb": 6.0,
  "index": 3
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Stage inserted |
| 400 | Invalid channel, missing body, or no room for stage |
| 503 | DSP busy; retry |

---

## POST /api/dsp/import/apo

Imports filters from Equalizer APO text format into a channel. All valid `Filter` lines are parsed and appended as biquad stages.

**Query parameter**: `?ch=0`

**Request body**: plain text Equalizer APO filter definitions

```
Filter 1: ON PK Fc 100 Hz Gain -3.0 dB Q 1.00
Filter 2: ON HPF Fc 20 Hz
```

**Response**

```json
{ "success": true, "added": 2 }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Filters imported; `added` is the count of stages created |
| 400 | Invalid channel, no body, or parse error |
| 503 | DSP busy; retry |

---

## POST /api/dsp/import/minidsp

Imports biquad coefficients from miniDSP export format. Each line should contain five space-separated floating point values: `b0 b1 b2 a1 a2`.

**Query parameter**: `?ch=0`

**Request body**: plain text miniDSP biquad coefficient lines

```
1.0023 -1.9876 0.9876 -1.9876 0.9876
1.0000 -2.0000 1.0000 -1.9956 0.9956
```

**Response**

```json
{ "success": true, "added": 2 }
```

---

## POST /api/dsp/import/fir

Imports FIR filter coefficients from plain text (one coefficient per line). Allocates a FIR pool slot, writes taps to both double-buffer states, and adds a `FIR` stage.

**Query parameter**: `?ch=0`

**Request body**: plain text FIR coefficients, one per line

```
-0.00012
0.00034
0.01023
...
```

**Response**

```json
{ "success": true, "taps": 512 }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | FIR stage added; `taps` is the coefficient count loaded |
| 400 | Invalid channel, no body, no valid taps, max stages reached, or no FIR slots |
| 500 | FIR pool allocation error |
| 503 | DSP busy; retry |

:::warning
FIR stages use delay lines allocated from PSRAM when available. Loading a very long FIR kernel on a system with low free PSRAM may trigger the 40 KB heap reserve guard and fail. Check `ESP.getMaxAllocHeap()` if you encounter 503 errors after FIR imports.
:::

---

## GET /api/dsp/export/apo

Exports the active channel configuration in Equalizer APO text format.

**Query parameter**: `?ch=0`

**Response**: `text/plain` APO filter text.

---

## GET /api/dsp/export/minidsp

Exports the active channel configuration in miniDSP biquad coefficient format.

**Query parameter**: `?ch=0`

**Response**: `text/plain` coefficient lines.

---

## GET /api/dsp/export/json

Exports the complete active DSP configuration as JSON (same format as `GET /api/dsp`).

**Response**: `application/json` — full DSP config object.

---

## PEQ preset endpoints

### GET /api/dsp/peq/presets

Lists the names of all saved PEQ presets stored as `/peq_<name>.json` files in LittleFS.

**Response**

```json
{ "presets": ["Flat", "RoomEQ", "BassBoost"] }
```

### POST /api/dsp/peq/presets

Saves the current channel configuration as a named PEQ preset.

**Request**

```json
{ "name": "RoomEQ", "ch": 0 }
```

**Response**

```json
{ "success": true }
```

---

## THD+N measurement

### GET /api/thd

Returns the current THD+N measurement state.

**Response**

```json
{
  "measuring": false,
  "testFreq": 1000.0,
  "valid": true,
  "thdPlusNPercent": 0.042,
  "thdPlusNDb": -67.5,
  "fundamentalDbfs": -12.0,
  "framesProcessed": 4096,
  "framesTarget": 4096,
  "harmonicLevels": [-72.1, -78.4, -85.2, -89.0, -91.5]
}
```

### POST /api/thd

Starts a new THD+N measurement at the specified frequency.

**Request**

```json
{ "freq": 1000.0, "averages": 8 }
```

**Response**

```json
{ "success": true }
```

### DELETE /api/thd

Stops an in-progress measurement immediately.

**Response**

```json
{ "success": true }
```
