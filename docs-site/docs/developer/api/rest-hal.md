---
title: REST API — HAL
sidebar_position: 2
description: HAL device management REST API endpoints.
---

The HAL API manages the hardware abstraction layer device lifecycle. It covers device enumeration, runtime discovery, per-device configuration, manual registration of expansion hardware, and a custom device schema system. All endpoints require the `DAC_ENABLED` build flag and are registered at boot by `registerHalApiEndpoints()`.

## Endpoint summary

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/hal/devices` | No | List all registered devices |
| POST | `/api/hal/devices` | No | Manually register a device by compatible string |
| PUT | `/api/hal/devices` | No | Update per-device runtime configuration |
| DELETE | `/api/hal/devices` | No | Remove a device from its slot |
| POST | `/api/hal/devices/reinit` | No | Deinit and re-initialize a device |
| GET | `/api/hal/devices/custom` | No | List custom device schemas stored in LittleFS |
| POST | `/api/hal/devices/custom` | No | Upload a custom JSON device schema |
| DELETE | `/api/hal/devices/custom` | No | Remove a custom device schema by name |
| POST | `/api/hal/devices/custom/create` | No | Create a new custom device via structured form data |
| POST | `/api/hal/scan` | No | Trigger a full device rescan (I2C + EEPROM) |
| GET | `/api/hal/scan/unmatched` | No | List I2C addresses found but not matched to any driver |
| GET | `/api/hal/db` | No | List all entries in the device database |
| GET | `/api/hal/db/presets` | No | List device presets (compatible string + name + type) |
| GET | `/api/hal/settings` | No | Get HAL auto-discovery toggle state |
| PUT | `/api/hal/settings` | No | Set HAL auto-discovery toggle state |

---

## Device object schema

Every device returned by `GET /api/hal/devices` has the following fields:

| Field | Type | Description |
|-------|------|-------------|
| `slot` | integer | Slot index (0 – `HAL_MAX_DEVICES - 1`, currently 31) |
| `compatible` | string | ESPHome-style compatible string (e.g. `"alx,es8311"`) |
| `name` | string | Human-readable device name |
| `manufacturer` | string | Manufacturer string |
| `type` | integer | Device type enum (see below) |
| `state` | integer | Lifecycle state enum (see below) |
| `discovery` | integer | How the device was found (0=unknown, 1=I2C scan, 2=EEPROM, 3=manual) |
| `ready` | boolean | Volatile ready flag — safe to read from any core |
| `i2cAddr` | integer | I2C address (0 if not applicable) |
| `channels` | integer | Number of audio channels |
| `capabilities` | integer | Bitmask of `HAL_CAP_*` flags |
| `legacyId` | integer | Legacy numeric device ID for EEPROM v3 compatibility |
| `busType` | integer | Bus type (0=none, 1=I2C, 2=I2S, 3=GPIO) |
| `busIndex` | integer | Bus index (0=EXT, 1=ONBOARD, 2=EXPANSION) |
| `pinA` | integer | Primary pin (SDA for I2C, BCLK for I2S) |
| `pinB` | integer | Secondary pin (SCL for I2C, WS for I2S) |
| `busFreq` | integer | Bus frequency in Hz |
| `sampleRatesMask` | integer | Bitmask of supported sample rates |

When a device has a saved config (`cfgEnabled` is present), additional fields are included:

| Field | Type | Description |
|-------|------|-------------|
| `userLabel` | string | User-assigned label (up to 32 chars) |
| `cfgEnabled` | boolean | Whether the device is enabled in config |
| `cfgI2sPort` | integer | I2S port index (255 = not set) |
| `cfgVolume` | integer | Hardware volume (0–100) |
| `cfgMute` | boolean | Mute state |
| `cfgPinSda` | integer | Config override: I2C SDA pin (-1 = use default) |
| `cfgPinScl` | integer | Config override: I2C SCL pin (-1 = use default) |
| `cfgPinData` | integer | Config override: I2S data pin |
| `cfgPinMclk` | integer | Config override: MCLK pin |
| `cfgMclkMultiple` | integer | MCLK multiplier |
| `cfgI2sFormat` | integer | I2S format enum |
| `cfgPgaGain` | integer | PGA gain setting |
| `cfgHpfEnabled` | boolean | Hardware HPF enabled |
| `cfgPaControlPin` | integer | PA enable pin (-1 = not set) |
| `cfgPinBck` | integer | BCK pin override |
| `cfgPinLrc` | integer | LRC/WS pin override |
| `cfgPinFmt` | integer | Format pin override |
| `cfgGpioA`–`cfgGpioD` | integer | General-purpose GPIO overrides |
| `cfgUsbPid` | integer | USB product ID (USB Audio devices) |

### Device type enum (`type`)

| Value | Constant | Description |
|-------|----------|-------------|
| 0 | `HAL_DEV_UNKNOWN` | Unknown / uninitialized |
| 1 | `HAL_DEV_DAC` | Digital-to-analog converter |
| 2 | `HAL_DEV_ADC` | Analog-to-digital converter |
| 3 | `HAL_DEV_CODEC` | Combined codec (ADC + DAC) |
| 4 | `HAL_DEV_AMP` | Power amplifier |
| 5 | `HAL_DEV_SENSOR` | Sensor (temperature, etc.) |

### Device state enum (`state`)

| Value | Constant | Description |
|-------|----------|-------------|
| 0 | `HAL_STATE_UNKNOWN` | Not yet probed |
| 1 | `HAL_STATE_DETECTED` | Detected on bus but not initialized |
| 2 | `HAL_STATE_CONFIGURING` | Initialization in progress |
| 3 | `HAL_STATE_AVAILABLE` | Initialized and ready |
| 4 | `HAL_STATE_UNAVAILABLE` | Temporarily unavailable (auto-recovery active) |
| 5 | `HAL_STATE_ERROR` | Initialization failed |
| 6 | `HAL_STATE_REMOVED` | Explicitly removed |
| 7 | `HAL_STATE_MANUAL` | Manually disabled by user |

### Capability bitmask (`capabilities`)

| Bit | Constant | Description |
|-----|----------|-------------|
| 0x01 | `HAL_CAP_DAC_PATH` | Routes audio to DAC output path |
| 0x02 | `HAL_CAP_ADC_PATH` | Provides an audio input lane |
| 0x04 | `HAL_CAP_HW_VOLUME` | Supports hardware volume control |
| 0x08 | `HAL_CAP_I2C_CTRL` | Controllable over I2C |
| 0x10 | `HAL_CAP_I2S_AUDIO` | Carries audio over I2S |

---

## GET /api/hal/devices

Returns a JSON array of all currently registered HAL devices (up to 24 slots).

**Response**

```json
[
  {
    "slot": 0,
    "compatible": "alx,pcm5102a",
    "name": "PCM5102A",
    "manufacturer": "Texas Instruments",
    "type": 1,
    "state": 3,
    "discovery": 3,
    "ready": true,
    "i2cAddr": 0,
    "channels": 2,
    "capabilities": 17,
    "legacyId": 1,
    "busType": 2,
    "busIndex": 0,
    "pinA": 20,
    "pinB": 21,
    "busFreq": 3072000,
    "sampleRatesMask": 15,
    "userLabel": "Main DAC",
    "cfgEnabled": true,
    "cfgI2sPort": 0,
    "cfgVolume": 80,
    "cfgMute": false
  },
  {
    "slot": 1,
    "compatible": "alx,es8311",
    "name": "ES8311",
    "manufacturer": "Everest Semiconductor",
    "type": 3,
    "state": 3,
    "ready": true,
    "i2cAddr": 24,
    "channels": 2,
    "capabilities": 29
  }
]
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success |

---

## POST /api/hal/devices

Manually register a device from the built-in driver registry. Use this to add expansion hardware that was not discovered automatically.

**Request**

```json
{
  "compatible": "alx,pcm5102a"
}
```

**Response (201 Created — success)**

```json
{
  "status": "ok",
  "slot": 5,
  "name": "PCM5102A",
  "state": 3
}
```

**Response (201 Created — init failure)**

When the slot is allocated but `init()` fails (the device is in ERROR state), the response includes an `error` field with the reason stored in `HalDevice._lastError`:

```json
{
  "status": "ok",
  "slot": 5,
  "name": "PCM5102A",
  "state": 5,
  "error": "I2C write to reg 0x00 NAKed"
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 201 | Device registered and init attempted (check `state` for result) |
| 400 | Missing or empty `compatible` field |
| 404 | Unknown compatible string — not in device database |
| 409 | Max instances for this compatible reached (8), or no free slots |
| 422 | No driver factory registered for this device |
| 500 | Driver factory returned null, or no free slots internally |

:::note
A 201 response does not guarantee hardware is functioning. Check the returned `state` field: `3` (AVAILABLE) indicates success; `5` (ERROR) means initialization failed but the slot was still allocated. When `state` is `5`, the `error` field contains the reason text from the driver's `init()` call.
:::

---

## PUT /api/hal/devices

Update runtime configuration for a device. All fields are optional — only fields present in the request body are applied. Changes are persisted to `/hal_config.json` and `hal_apply_config()` is called immediately.

**Request**

```json
{
  "slot": 0,
  "label": "Main DAC",
  "volume": 85,
  "mute": false,
  "enabled": true,
  "i2cAddr": 24,
  "i2cBus": 1,
  "i2cSpeed": 400000,
  "pinSda": 7,
  "pinScl": 8,
  "pinMclk": 22,
  "pinData": 24,
  "i2sPort": 0,
  "sampleRate": 48000,
  "bitDepth": 32,
  "cfgMclkMultiple": 256,
  "cfgI2sFormat": 0,
  "cfgPgaGain": 0,
  "cfgHpfEnabled": false,
  "cfgPaControlPin": 53,
  "pinBck": 20,
  "pinLrc": 21
}
```

**Response**

```json
{ "status": "ok" }
```

**Response (422 — invalid field)**

```json
{
  "status": "error",
  "field": "i2sPort",
  "error": "i2sPort must be 0-2 or 255"
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Configuration saved and applied |
| 400 | Invalid JSON, or `slot` out of range |
| 404 | No device in the specified slot |
| 422 | Config field failed validation (see `field` and `error` in response body) |

Config fields validated by `hal_validate_config()`:

| Field | Valid values |
|-------|-------------|
| `i2sPort` | 0, 1, 2 (physical ports), or 255 (auto / not set) |
| `i2cBus` | 0 (EXT), 1 (ONBOARD), 2 (EXPANSION) |
| GPIO pins (`pinSda`, `pinScl`, `pinBck`, `pinLrc`, `pinData`, `pinMclk`, `cfgPaControlPin`, `cfgGpioA`–`cfgGpioD`) | -1 (not set) or 0–54 |

:::warning
For DAC-path devices, changing `enabled` to `false` uses the `HalCoordState` deferred toggle queue (`appState.halCoord.requestDeviceToggle(halSlot, -1)`). `hal_apply_config()` handles this automatically.
:::

---

## DELETE /api/hal/devices

Remove a device from a slot. Calls `deinit()` on the device, clears the slot configuration, and triggers a WebSocket broadcast.

For DAC-path devices, this endpoint enqueues deferred teardown via `appState.halCoord.requestDeviceToggle(halSlot, -1)`. Device-type-agnostic — works for `HAL_DEV_DAC`, `HAL_DEV_CODEC`, etc.

**Request**

```json
{ "slot": 5 }
```

**Response**

```json
{ "status": "ok" }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Device removed |
| 400 | Invalid JSON, or `slot` out of range |
| 404 | No device in the specified slot |

---

## POST /api/hal/devices/reinit

Deinitialize and re-initialize a device in place. Useful after configuration changes that require a full hardware reset. The device briefly enters `HAL_STATE_CONFIGURING` while the operation runs.

**Request**

```json
{ "slot": 1 }
```

**Response**

```json
{
  "status": "ok",
  "state": 3
}
```

On failure, the response includes the `error` field containing the reason text from `HalDevice._lastError`:

```json
{
  "status": "error",
  "state": 5,
  "error": "I2C write to reg 0x00 NAKed"
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Reinit attempted (check `state` for result) |
| 400 | Invalid JSON, or `slot` out of range |
| 404 | No device in the specified slot |

---

## POST /api/hal/scan

Triggers a full 3-tier device rescan: I2C bus scan, EEPROM probe, and manual config application. A 409 is returned immediately if a scan is already in progress (concurrency guard via `appState._halScanInProgress`).

:::note
The scan runs synchronously on the HTTP request — the response is not sent until scanning is complete. Typical scan time is 100–500 ms depending on how many I2C addresses respond.
:::

**Request body**: none required.

**Response**

```json
{
  "status": "ok",
  "devicesFound": 3,
  "partialScan": false
}
```

When Bus 0 is skipped due to an active WiFi connection, the response includes additional fields:

```json
{
  "status": "ok",
  "devicesFound": 3,
  "partialScan": true,
  "skippedBuses": "Bus 0 (WiFi SDIO conflict)"
}
```

`partialScan` is `true` when Bus 0 (GPIO 48/54) was skipped because WiFi is active. Bus 0 shares SDIO lines with the ESP32-C6 WiFi co-processor — scanning while WiFi is connected causes `sdmmc_send_cmd` errors and MCU resets. A full scan covering all three buses is only possible when WiFi is fully disconnected. `DIAG_HAL_I2C_BUS_CONFLICT` (0x1101) is emitted in the diagnostic journal whenever this skip occurs.

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Scan complete |
| 409 | Scan already in progress |

:::warning
I2C Bus 0 (GPIO 48/54) shares the SDIO lines with the ESP32-C6 WiFi co-processor. The scanner automatically skips Bus 0 when WiFi is active to prevent `sdmmc_send_cmd` errors and MCU resets.
:::

---

## GET /api/hal/db

Returns the full device database — all known device descriptors regardless of whether they are currently instantiated.

**Response**

```json
[
  {
    "compatible": "alx,pcm5102a",
    "name": "PCM5102A",
    "manufacturer": "Texas Instruments",
    "type": 1,
    "i2cAddr": 0,
    "channels": 2
  },
  {
    "compatible": "alx,es8311",
    "name": "ES8311",
    "manufacturer": "Everest Semiconductor",
    "type": 3,
    "i2cAddr": 24,
    "channels": 2
  }
]
```

---

## GET /api/hal/db/presets

Returns a condensed list of device database entries suitable for populating a UI picker. Only `compatible`, `name`, and `type` are included.

**Response**

```json
[
  { "compatible": "alx,pcm5102a", "name": "PCM5102A", "type": 1 },
  { "compatible": "alx,es8311",   "name": "ES8311",   "type": 3 },
  { "compatible": "alx,pcm1808",  "name": "PCM1808",  "type": 2 }
]
```

---

## GET /api/hal/settings

Returns the current HAL settings.

**Response**

```json
{ "halAutoDiscovery": true }
```

---

## PUT /api/hal/settings

Updates HAL settings. Currently controls only the auto-discovery toggle.

**Request**

```json
{ "halAutoDiscovery": false }
```

**Response**

```json
{ "status": "ok" }
```

---

## GET /api/hal/devices/custom

Lists custom device schema files stored in LittleFS under `/hal/custom/`.

**Response**

```json
{
  "schemas": [
    { "name": "my_dac_alx_custom.json", "size": 412 }
  ]
}
```

---

## POST /api/hal/devices/custom

Uploads a custom JSON device schema. The schema must include a `compatible` field. The file is stored at `/hal/custom/<compatible>.json` (commas in the compatible string are replaced with underscores for filesystem safety). `hal_load_custom_devices()` is called immediately after saving.

**Request**

```json
{
  "compatible": "vendor,my-dac",
  "name": "My Custom DAC",
  "manufacturer": "Acme Corp",
  "type": 1,
  "i2cAddr": 30,
  "channels": 2
}
```

**Response**

```json
{ "ok": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Schema saved and loaded |
| 400 | Invalid JSON or missing `compatible` field |
| 500 | LittleFS write failed |

---

## DELETE /api/hal/devices/custom

Removes a custom schema file. The schema is identified by the `name` query parameter (without the `.json` extension — use the raw compatible string with underscores as returned by `GET /api/hal/devices/custom`).

**Query parameter**: `?name=vendor_my-dac`

**Response**

```json
{ "ok": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Schema removed |
| 400 | Missing `name` query parameter |
| 404 | Schema file not found |

---

## POST /api/hal/devices/custom/create

Creates a new custom device from a structured JSON body. This endpoint is the counterpart to the web UI custom device modal — it validates the schema, writes it to LittleFS, and immediately registers and initialises the device.

**Request**

```json
{
  "compatible": "vendor,my-dac-i2c",
  "name": "My Custom DAC",
  "manufacturer": "Acme Corp",
  "type": 1,
  "channels": 2,
  "i2cAddr": 72,
  "tier": 2,
  "i2sBus": 2,
  "i2sPort": 2,
  "sampleRatesMask": 14,
  "initSequence": [
    { "reg": 0, "val": 0 },
    { "reg": 1, "val": 25 },
    { "reg": 8, "val": 192 }
  ]
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `compatible` | string | Yes | Unique `vendor,model` identifier. Commas are replaced with underscores in the filename. |
| `name` | string | Yes | Human-readable display name (max 32 chars) |
| `manufacturer` | string | Yes | Manufacturer name (max 32 chars) |
| `type` | integer | Yes | `1` = DAC, `2` = ADC |
| `channels` | integer | Yes | `2` for stereo, `4` for 4-channel TDM |
| `i2cAddr` | integer | Tier 2 | 7-bit I2C address. Required when `tier` is 2. |
| `tier` | integer | Yes | `1` = I2S passthrough only, `2` = I2C init sequence |
| `i2sBus` | integer | Tier 2 | I2C bus index: `0` = external, `1` = onboard, `2` = expansion |
| `i2sPort` | integer | Yes | I2S port index (0, 1, or 2) |
| `sampleRatesMask` | integer | Yes | Bitfield: bit 0=8K, 1=16K, 2=44.1K, 3=48K, 4=96K, 5=192K |
| `initSequence` | array | Tier 2 | Ordered list of `\{"reg": N, "val": N\}` register writes |

**Response (201 Created — success)**

```json
{
  "status": "ok",
  "slot": 7,
  "state": 3
}
```

**Response (201 Created — init failure)**

```json
{
  "status": "ok",
  "slot": 7,
  "state": 5,
  "error": "I2C NAK at address 0x48"
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 201 | Device schema saved and init attempted (check `state` for hardware result) |
| 400 | Invalid JSON, missing required fields, or `compatible` already in use |
| 409 | No free device slots |
| 500 | LittleFS write failed |

:::note Init result
A 201 response indicates the schema was saved. The `state` field tells you whether hardware init succeeded (`3` = AVAILABLE) or failed (`5` = ERROR). When `state` is `5`, `error` contains the failure reason from the driver's `probe()` or `init()` call.
:::

---

## GET /api/hal/scan/unmatched

Returns I2C addresses that were found during the most recent bus scan but could not be matched to any registered driver or custom schema. This endpoint is used by the web UI's custom device creation flow to help users identify what is connected.

**Request body**: none required.

**Response**

```json
{
  "status": "ok",
  "count": 2,
  "addresses": [
    {
      "bus": 2,
      "addr": 72,
      "addrHex": "0x48",
      "description": "No driver matched"
    },
    {
      "bus": 2,
      "addr": 80,
      "addrHex": "0x50",
      "description": "No driver matched"
    }
  ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `count` | integer | Number of unmatched addresses |
| `addresses[].bus` | integer | I2C bus index (0, 1, or 2) |
| `addresses[].addr` | integer | 7-bit I2C address |
| `addresses[].addrHex` | string | Address in hexadecimal (for display) |
| `addresses[].description` | string | Human-readable reason why no driver matched |

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success (count may be 0 if all addresses were matched) |
| 409 | A scan is currently in progress — retry after the scan completes |

:::tip Using this endpoint
Call `POST /api/hal/scan` first to ensure the address list is fresh, then call this endpoint to get unmatched addresses. The web UI custom device modal does this automatically when you click **Scan for devices**.
:::

---

## GET /api/i2s/ports

Returns the status of all 3 I2S ports, or a single port when the `?id=N` query parameter is provided. Each port entry includes its current mode (STD/TDM/off), direction (TX/RX), pin assignments, clock master status, sample rate, and configurable audio parameters (`format`, `bitDepth`, `mclkMultiple`).

| Field | Type | Values | Description |
|-------|------|--------|-------------|
| `format` | string | `"philips"`, `"msb"`, `"pcm_short"` | I2S data format (default: philips) |
| `bitDepth` | integer | 16, 24, 32 | Sample bit width (default: 32) |
| `mclkMultiple` | integer | 128, 192, 256, 384, 512, 768, 1152 | MCLK-to-sample-rate ratio (default: 256) |

**Query parameters**

| Parameter | Type | Description |
|-----------|------|-------------|
| `id` | integer (optional) | I2S port index (0–2). When omitted, all 3 ports are returned. |

**Response (all ports)**

```json
\{
  "ports": [
    \{
      "id": 0,
      "mode": "STD",
      "direction": "RX",
      "bck": 20,
      "ws": 21,
      "data": 23,
      "mclk": 22,
      "clockMaster": true,
      "sampleRate": 48000,
      "format": "philips",
      "bitDepth": 32,
      "mclkMultiple": 256
    \},
    \{
      "id": 1,
      "mode": "STD",
      "direction": "RX",
      "bck": -1,
      "ws": -1,
      "data": 25,
      "mclk": -1,
      "clockMaster": false,
      "sampleRate": 48000,
      "format": "philips",
      "bitDepth": 32,
      "mclkMultiple": 256
    \},
    \{
      "id": 2,
      "mode": "off",
      "direction": "none",
      "bck": -1,
      "ws": -1,
      "data": -1,
      "mclk": -1,
      "clockMaster": false,
      "sampleRate": 0,
      "format": "philips",
      "bitDepth": 32,
      "mclkMultiple": 256
    \}
  ]
\}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success |
| 400 | Invalid `id` value (out of range 0–2) |

---

## GET /api/psram/status

Returns PSRAM health and per-subsystem allocation tracking. Requires authentication.

**Response**

```json
{
  "total": 33554432,
  "free": 31457280,
  "usagePercent": 6.25,
  "fallbackCount": 0,
  "failedCount": 0,
  "allocPsram": 1048576,
  "allocSram": 32768,
  "warning": false,
  "critical": false,
  "budget": [
    {"label": "dsp_delay", "bytes": 524288, "psram": true},
    {"label": "audio_dma", "bytes": 32768, "psram": false}
  ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `total` | integer | Total PSRAM size in bytes |
| `free` | integer | Free PSRAM in bytes |
| `usagePercent` | float | PSRAM usage percentage (0–100) |
| `fallbackCount` | integer | Lifetime count of PSRAM-to-SRAM fallback allocations |
| `failedCount` | integer | Lifetime count of total allocation failures |
| `allocPsram` | integer | Total bytes currently tracked in PSRAM across all budget entries |
| `allocSram` | integer | Total bytes currently tracked in SRAM across all budget entries |
| `warning` | boolean | True when free PSRAM is below 1 MB (`PSRAM_WARNING_THRESHOLD`) |
| `critical` | boolean | True when free PSRAM is below 512 KB (`PSRAM_CRITICAL_THRESHOLD`) |
| `budget` | array | Per-subsystem allocation entries — each has `label` (string), `bytes` (integer), and `psram` (boolean) |

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success |
| 401 | Authentication required |

:::note
`psramWarning` and `psramCritical` thresholds are defined in `src/config.h` as `PSRAM_WARNING_THRESHOLD` (1,048,576 bytes) and `PSRAM_CRITICAL_THRESHOLD` (524,288 bytes). When `critical` is true, DSP delay-line and convolution allocations are refused to preserve remaining PSRAM for essential subsystems.
:::
