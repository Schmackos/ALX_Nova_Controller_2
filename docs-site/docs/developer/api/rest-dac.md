---
title: REST API — DAC
sidebar_position: 5
description: DAC and audio output REST API endpoints.
---

The DAC API provides access to DAC-related state and EEPROM board identification. As of DEBT-5, **HAL is the sole system managing all DAC devices** — there are no static device patterns. Enable/disable, volume, and mute for individual DAC devices (PCM5102A, ES8311, etc.) are now managed through `PUT /api/hal/devices`. The `/api/dac` endpoints remain available for backward compatibility but query HAL state internally. Runtime DAC model switching (`dac_select_driver()`) has been removed — driver selection happens exclusively through HAL device discovery and configuration. All endpoints require authentication and the `DAC_ENABLED` build flag.

## Endpoint summary

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/dac` | Yes | Get DAC state (queries HAL internally) |
| POST | `/api/dac` | Yes | Update DAC settings (deprecated — use `PUT /api/hal/devices`) |
| GET | `/api/dac/drivers` | Yes | List registered DAC drivers (deprecated — use `GET /api/hal/db/presets`) |
| GET | `/api/dac/eeprom` | Yes | Read EEPROM state and raw hex dump |
| POST | `/api/dac/eeprom` | Yes | Program EEPROM with device descriptor |
| POST | `/api/dac/eeprom/erase` | Yes | Erase EEPROM at target I2C address |
| POST | `/api/dac/eeprom/scan` | Yes | Rescan I2C bus and EEPROM |
| GET | `/api/dac/eeprom/presets` | Yes | List driver-registry EEPROM presets |

---

## Deferred toggle pattern

Enabling or disabling a DAC-path device involves stopping I2S DMA, uninstalling the I2S driver, and freeing resources — all of which must not happen while the audio pipeline task on Core 1 is reading I2S buffers. To prevent crashes, the API endpoint never calls hardware init/deinit directly.

Instead, it enqueues a deferred toggle request via `appState.halCoord.requestDeviceToggle(halSlot, action)`, where `action` is `1` for enable or `-1` for disable. The queue holds up to 8 pending requests; requests for the same device slot are deduplicated (later request overwrites earlier). The main loop drains the queue between audio frames, performing actual hardware operations with a binary semaphore handshake (`audioTaskPausedAck`).

This mechanism works for **any HAL device type** (`HAL_DEV_DAC`, `HAL_DEV_CODEC`, `HAL_DEV_ADC`, etc.), not just DACs. HAL-routed drivers call `dac_activate_for_hal(halSlot)` / `dac_deactivate_for_hal(halSlot)` based on the deferred action.

:::warning
Direct writes to internal AppState toggle fields bypass the queue and are unsafe. Always use `appState.halCoord.requestDeviceToggle(halSlot, action)` for device enable/disable.
:::

```
Client               Firmware HTTP handler        Main loop
  |                         |                        |
  |-- PUT /api/hal/dev ----->|                        |
  | { "slot": 0, "enabled": true }|                  |
  |                         |-- halCoord.request() -->|
  |<-- 200 { "status" } ----|                        |
  |                         |              (5ms tick) |
  |                         |            audioPaused=true
  |                         |        audioTaskPausedAck taken
  |                         |              i2s_driver_install()
  |                         |        audioTaskPausedAck released
```

---

## GET /api/dac

Returns the complete DAC state including enabled/ready flags, volume, mute, device ID, detected status, TX underrun count, filter mode, and full driver capabilities.

**Query parameter**: `?slot=N` — optional HAL slot index. When present, queries the DAC device at that slot rather than the legacy primary slot. Use `GET /api/hal/devices` to discover slot indices. See [DAC State by HAL Slot](./rest-hal.md#dac-state-by-hal-slot) for full error code reference.

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

:::warning Deprecated
`POST /api/dac` is retained for backward compatibility. New integrations should use `PUT /api/hal/devices` to manage DAC device state. Runtime driver switching via `deviceId` has been removed — `dac_select_driver()` no longer exists. Use HAL device configuration and EEPROM programming to select drivers at discovery time.
:::

Updates one or more DAC settings. All fields are optional; only fields present in the body are applied. Settings are persisted via `dac_save_settings()` when any field changes. Changes that require hardware interaction (volume, mute, filter mode) are applied immediately through the HAL-managed driver. Enable/disable transitions use the deferred toggle mechanism.

**Request**

```json
{
  "enabled": true,
  "volume": 85,
  "mute": false,
  "filterMode": 1
}
```

| Field | Type | Description |
|-------|------|-------------|
| `enabled` | boolean | Enable or disable the DAC (deferred — see note above) |
| `volume` | integer | Output volume 0–100; applied immediately via HAL driver |
| `mute` | boolean | Hardware mute state; applied immediately |
| `filterMode` | integer | Filter mode index; applied immediately via HAL driver |

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

:::warning Deprecated
`GET /api/dac/drivers` is retained for backward compatibility. Use `GET /api/hal/db/presets` instead to list all registered device drivers and their capabilities.
:::

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

The ALX Nova uses a small I2C EEPROM (AT24C02 or compatible, address range 0x50–0x57) to store a board-identity descriptor. This descriptor is read during HAL discovery (`hal_discovery`) to automatically identify and bind the correct HAL driver to the hardware device, without user configuration. The EEPROM endpoints remain the correct way to program or erase board identity descriptors.

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
