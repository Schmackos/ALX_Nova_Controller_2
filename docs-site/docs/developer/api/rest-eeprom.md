---
title: EEPROM API
sidebar_label: EEPROM
---

# REST API -- EEPROM

The EEPROM API provides access to board-identity EEPROM programming and diagnostics. The ALX Nova uses small I2C EEPROMs (AT24C02 or compatible, address range 0x50--0x57) to store device descriptors in the ALXD v3 binary format. These descriptors are read during HAL discovery to automatically identify and bind the correct driver to expansion hardware. All endpoints require authentication and the `DAC_ENABLED` build flag.

## Endpoint summary

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/hal/eeprom` | Yes | Read EEPROM state, parsed fields, and raw hex dump |
| POST | `/api/hal/eeprom` | Yes | Program EEPROM with device descriptor |
| POST | `/api/hal/eeprom/erase` | Yes | Erase EEPROM (fill with 0xFF) |
| POST | `/api/hal/eeprom/scan` | Yes | Re-scan I2C bus and EEPROM |
| GET | `/api/hal/eeprom/presets` | Yes | List HAL device DB entries for EEPROM pre-fill |

---

## EEPROM v3 data format

The EEPROM stores a binary descriptor with an `ALXD` magic header, CRC-16 integrity check, and a compatible string for driver matching.

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `deviceId` | uint16 | Numeric driver ID matching the registry |
| `hwRevision` | uint8 | PCB hardware revision |
| `deviceName` | string (32) | Human-readable device name |
| `manufacturer` | string (32) | Manufacturer name |
| `maxChannels` | uint8 | Output channel count |
| `dacI2cAddress` | uint8 | I2C address of the device chip on the board |
| `flags` | uint8 | Capability bitmask |
| `sampleRates` | uint32[] | Supported sample rates |

### Flags bitmask

| Bit | Constant | Description |
|-----|----------|-------------|
| 0x01 | `DAC_FLAG_INDEPENDENT_CLOCK` | Requires independent MCLK |
| 0x02 | `DAC_FLAG_HW_VOLUME` | Has hardware volume control |
| 0x04 | `DAC_FLAG_FILTERS` | Has hardware filter modes |

---

## GET /api/hal/eeprom

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

## POST /api/hal/eeprom

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
| `address` | integer | No | Target I2C address (0x50--0x57); defaults to 0x50 if omitted or out of range |
| `deviceId` | integer | Yes | Driver registry ID |
| `hwRevision` | integer | Yes | Hardware revision byte |
| `deviceName` | string | Yes | Device name (max 32 chars) |
| `manufacturer` | string | Yes | Manufacturer name (max 32 chars) |
| `maxChannels` | integer | Yes | Number of output channels |
| `dacI2cAddress` | integer | Yes | Device chip I2C address (0 if not applicable) |
| `flags` | object | Yes | Capability flags object |
| `sampleRates` | integer[] | Yes | Supported sample rates array |

**Response**

```json
\{ "success": true \}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | EEPROM programmed and verified |
| 400 | No body or invalid JSON |
| 401 | Authentication required |
| 500 | Serialization failed, or write/verify failed |

:::warning
Programming an EEPROM that is connected to the same I2C bus as an active device may disrupt ongoing I2C communication. Disable the device before programming the EEPROM on production hardware.
:::

---

## POST /api/hal/eeprom/erase

Erases the EEPROM at the target address (fills all bytes with 0xFF). The diagnostic state is cleared after a successful erase.

**Request (optional)**

```json
\{ "address": 80 \}
```

If no body is provided (or `address` is absent), the address stored in `appState.dac.eepromDiag.eepromAddr` is used. If that is also 0 or out of range, the erase targets address 0x50.

**Response**

```json
\{ "success": true \}
```

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | EEPROM erased |
| 401 | Authentication required |
| 500 | Erase operation failed |

---

## POST /api/hal/eeprom/scan

Rescans the I2C bus and probes for an EEPROM. Updates `appState.dac.eepromDiag` and broadcasts the updated state. The response returns the current diagnostic state.

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

## GET /api/hal/eeprom/presets

Returns pre-filled EEPROM data derived from all registered HAL device database entries. Use this to populate the EEPROM programming form in the UI without manual entry.

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
