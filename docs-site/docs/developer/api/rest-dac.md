---
title: REST API — DAC
sidebar_position: 5
description: DAC and audio output REST API endpoints.
---

The DAC API controls the primary digital audio output hardware: device state, volume, mute, filter mode, and driver selection. It also provides direct access to the I2C EEPROM used for board identification. All endpoints require authentication and the `DAC_ENABLED` build flag.

## Endpoint summary

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/dac` | Yes | Get full DAC state and driver capabilities |
| POST | `/api/dac` | Yes | Update DAC settings |
| GET | `/api/dac/drivers` | Yes | List all registered DAC drivers |
| GET | `/api/dac/eeprom` | Yes | Read EEPROM state and raw hex dump |
| POST | `/api/dac/eeprom` | Yes | Program EEPROM with device descriptor |
| POST | `/api/dac/eeprom/erase` | Yes | Erase EEPROM at target I2C address |
| POST | `/api/dac/eeprom/scan` | Yes | Rescan I2C bus and EEPROM |
| GET | `/api/dac/eeprom/presets` | Yes | List driver-registry EEPROM presets |

---

## Deferred toggle pattern

Enabling or disabling a DAC involves stopping I2S DMA, uninstalling the I2S driver, and freeing resources — all of which must not happen while the audio pipeline task on Core 1 is reading I2S buffers. To prevent crashes, the `POST /api/dac` endpoint never calls hardware init/deinit directly.

Instead, it sets `appState.dac.requestDacToggle(1)` (to enable) or `requestDacToggle(-1)` (to disable). The main loop observes the pending toggle flag and performs the actual hardware operation between audio frames using a binary semaphore handshake (`audioTaskPausedAck`).

The same pattern applies to the ES8311 codec: `requestEs8311Toggle(1)` / `requestEs8311Toggle(-1)`.

:::warning
The toggle values accepted by `requestDacToggle()` and `requestEs8311Toggle()` are validated: only `−1`, `0`, and `1` are accepted. Direct writes to `_pendingDacToggle` or `_pendingEs8311Toggle` in AppState bypass this validation and are unsafe.
:::

```
Client               Firmware HTTP handler        Main loop
  |                         |                        |
  |-- POST /api/dac -------->|                        |
  |   { "enabled": true }   |                        |
  |                         |-- requestDacToggle(1) ->|
  |<-- 200 { "success" } ---|                        |
  |                         |              (5ms tick) |
  |                         |            audioPaused=true
  |                         |        audioTaskPausedAck taken
  |                         |              i2s_driver_install()
  |                         |        audioTaskPausedAck released
```

---

## GET /api/dac

Returns the complete DAC state including enabled/ready flags, volume, mute, device ID, detected status, TX underrun count, filter mode, and full driver capabilities.

**Response**

```json
{
  "success": true,
  "enabled": true,
  "volume": 80,
  "mute": false,
  "deviceId": 1,
  "modelName": "PCM5102A",
  "outputChannels": 2,
  "detected": true,
  "ready": true,
  "filterMode": 0,
  "txUnderruns": 0,
  "capabilities": {
    "name": "PCM5102A",
    "manufacturer": "Texas Instruments",
    "maxChannels": 2,
    "hasHardwareVolume": false,
    "hasI2cControl": false,
    "needsIndependentClock": false,
    "hasFilterModes": false,
    "numFilterModes": 0,
    "filterModes": [],
    "supportedRates": [44100, 48000, 96000, 192000]
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `enabled` | boolean | Whether the DAC is enabled in config |
| `volume` | integer | Current volume (0–100) |
| `mute` | boolean | Whether hardware mute is active |
| `deviceId` | integer | Active driver device ID |
| `modelName` | string | Active driver model name |
| `outputChannels` | integer | Number of output channels |
| `detected` | boolean | Whether the device was detected during I2C scan |
| `ready` | boolean | Whether the driver is initialized and ready |
| `filterMode` | integer | Active filter mode index |
| `txUnderruns` | integer | I2S TX underrun counter since boot |

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success |
| 401 | Authentication required |

---

## POST /api/dac

Updates one or more DAC settings. All fields are optional; only fields present in the body are applied. Settings are persisted via `dac_save_settings()` when any field changes. Changes that require hardware interaction (volume, mute, filter mode) are applied immediately through the active driver. Enable/disable transitions use the deferred toggle mechanism.

**Request**

```json
{
  "enabled": true,
  "volume": 85,
  "mute": false,
  "filterMode": 1,
  "deviceId": 2
}
```

| Field | Type | Description |
|-------|------|-------------|
| `enabled` | boolean | Enable or disable the DAC (deferred — see note above) |
| `volume` | integer | Output volume 0–100; applied immediately via driver |
| `mute` | boolean | Hardware mute state; applied immediately |
| `filterMode` | integer | Filter mode index; applied immediately via driver |
| `deviceId` | integer | Switch to a different registered driver |

**Response**

```json
{ "success": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Settings applied |
| 400 | No body, invalid JSON, or unknown `deviceId` |
| 401 | Authentication required |

:::note
`volume` accepts 0–100. Values outside this range are silently ignored to prevent accidental overdriving.
:::

---

## GET /api/dac/drivers

Lists all DAC drivers registered in the driver registry. Each entry includes capabilities queried from a temporary driver instance.

**Response**

```json
{
  "success": true,
  "drivers": [
    {
      "id": 1,
      "name": "PCM5102A",
      "manufacturer": "Texas Instruments",
      "maxChannels": 2,
      "hasHardwareVolume": false,
      "hasI2cControl": false,
      "needsIndependentClock": false,
      "hasFilterModes": false
    },
    {
      "id": 2,
      "name": "ES8311",
      "manufacturer": "Everest Semiconductor",
      "maxChannels": 2,
      "hasHardwareVolume": true,
      "hasI2cControl": true,
      "needsIndependentClock": false,
      "hasFilterModes": false
    }
  ]
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success |
| 401 | Authentication required |

---

## EEPROM endpoints

The ALX Nova uses a small I2C EEPROM (AT24C02 or compatible, address range 0x50–0x57) to store a board-identity descriptor. This descriptor is read at boot by `dac_eeprom_scan()` to automatically select the correct DAC driver without user configuration.

### EEPROM v3 data fields

| Field | Type | Description |
|-------|------|-------------|
| `deviceId` | uint16 | Numeric driver ID matching the registry |
| `hwRevision` | uint8 | PCB hardware revision |
| `deviceName` | string (32) | Human-readable device name |
| `manufacturer` | string (32) | Manufacturer name |
| `maxChannels` | uint8 | Output channel count |
| `dacI2cAddress` | uint8 | I2C address of the DAC chip on the board |
| `flags` | uint8 | Capability bitmask |
| `sampleRates` | uint32[] | Supported sample rates |

### EEPROM flags bitmask

| Bit | Constant | Description |
|-----|----------|-------------|
| 0x01 | `DAC_FLAG_INDEPENDENT_CLOCK` | Requires independent MCLK |
| 0x02 | `DAC_FLAG_HW_VOLUME` | Has hardware volume control |
| 0x04 | `DAC_FLAG_FILTERS` | Has hardware filter modes |

---

## GET /api/dac/eeprom

Returns the current EEPROM diagnostic state including scan results, parsed fields, and a raw hex dump of the EEPROM contents.

**Response (EEPROM found)**

```json
{
  "success": true,
  "scanned": true,
  "found": true,
  "eepromAddr": 80,
  "i2cDevicesMask": 3,
  "i2cTotalDevices": 2,
  "readErrors": 0,
  "writeErrors": 0,
  "lastScanMs": 4820,
  "parsed": {
    "deviceId": 1,
    "hwRevision": 1,
    "deviceName": "PCM5102A Stereo DAC",
    "manufacturer": "Texas Instruments",
    "maxChannels": 2,
    "dacI2cAddress": 0,
    "flags": 0,
    "independentClock": false,
    "hwVolume": false,
    "filters": false,
    "sampleRates": [44100, 48000, 96000, 192000]
  },
  "rawHex": "414C581600010002..."
}
```

**Response (no EEPROM)**

```json
{
  "success": true,
  "scanned": true,
  "found": false,
  "eepromAddr": 0,
  "i2cDevicesMask": 0,
  "i2cTotalDevices": 0,
  "readErrors": 0,
  "writeErrors": 0,
  "lastScanMs": 0
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success (check `found` field) |
| 401 | Authentication required |

---

## POST /api/dac/eeprom

Programs the EEPROM with a new device descriptor. The data is serialized to the v3 binary format, written to the target address, and verified by a read-back. The diagnostic state is refreshed with a rescan after a successful write.

**Request**

```json
{
  "address": 80,
  "deviceId": 1,
  "hwRevision": 1,
  "deviceName": "PCM5102A Stereo DAC",
  "manufacturer": "Texas Instruments",
  "maxChannels": 2,
  "dacI2cAddress": 0,
  "flags": {
    "independentClock": false,
    "hwVolume": false,
    "filters": false
  },
  "sampleRates": [44100, 48000, 96000, 192000]
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `address` | integer | No | Target I2C address (0x50–0x57); defaults to 0x50 if omitted or out of range |
| `deviceId` | integer | Yes | Driver registry ID |
| `hwRevision` | integer | Yes | Hardware revision byte |
| `deviceName` | string | Yes | Device name (max 32 chars) |
| `manufacturer` | string | Yes | Manufacturer name (max 32 chars) |
| `maxChannels` | integer | Yes | Number of output channels |
| `dacI2cAddress` | integer | Yes | DAC chip I2C address (0 if not applicable) |
| `flags` | object | Yes | Capability flags object |
| `sampleRates` | integer[] | Yes | Supported sample rates array |

**Response**

```json
{ "success": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | EEPROM programmed and verified |
| 400 | No body or invalid JSON |
| 401 | Authentication required |
| 500 | Serialization failed, or write/verify failed |

:::warning
Programming an EEPROM that is connected to the same I2C bus as an active DAC chip may disrupt ongoing I2C communication. It is recommended to disable the DAC before programming the EEPROM on production hardware.
:::

---

## POST /api/dac/eeprom/erase

Erases the EEPROM at the target address (fills all bytes with 0xFF). The diagnostic state is cleared after a successful erase.

**Request (optional)**

```json
{ "address": 80 }
```

If no body is provided (or `address` is absent), the address stored in `appState.dac.eepromDiag.eepromAddr` is used. If that is also 0 or out of range, the erase targets address 0x50.

**Response**

```json
{ "success": true }
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | EEPROM erased |
| 401 | Authentication required |
| 500 | Erase operation failed |

---

## POST /api/dac/eeprom/scan

Rescans the I2C bus and probes for an EEPROM. Updates `appState.dac.eepromDiag` and broadcasts an EEPROM dirty flag. The response returns the current diagnostic state.

**Request body**: none required.

**Response**

```json
{
  "success": true,
  "scanned": true,
  "found": true,
  "eepromAddr": 80,
  "i2cTotalDevices": 2,
  "i2cDevicesMask": 3,
  "deviceName": "PCM5102A Stereo DAC",
  "manufacturer": "Texas Instruments",
  "deviceId": 1
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Scan complete (check `found`) |
| 401 | Authentication required |

---

## GET /api/dac/eeprom/presets

Returns pre-filled EEPROM data derived from all registered DAC drivers. Use this to populate the EEPROM programming form in the UI — no manual entry required for known devices.

**Response**

```json
{
  "success": true,
  "presets": [
    {
      "deviceId": 1,
      "deviceName": "PCM5102A",
      "manufacturer": "Texas Instruments",
      "maxChannels": 2,
      "dacI2cAddress": 0,
      "flags": 0,
      "sampleRates": [44100, 48000, 96000, 192000]
    },
    {
      "deviceId": 2,
      "deviceName": "ES8311",
      "manufacturer": "Everest Semiconductor",
      "maxChannels": 2,
      "dacI2cAddress": 24,
      "flags": 2,
      "sampleRates": [44100, 48000, 96000]
    }
  ]
}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Success |
| 401 | Authentication required |
