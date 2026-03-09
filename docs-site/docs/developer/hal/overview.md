---
title: HAL Overview
sidebar_position: 1
description: Overview of the ALX Nova Hardware Abstraction Layer framework.
---

The Hardware Abstraction Layer (HAL) is the central device management framework for the ALX Nova Controller. It owns the complete lifecycle of every hardware peripheral on the board — from first discovery through runtime health monitoring — and exposes a uniform interface to the rest of the firmware regardless of what physical bus or protocol a device uses.

## Design Philosophy

The HAL is deliberately modelled after the ESPHome component/platform pattern. Each device is identified by a `compatible` string in the Linux Device Tree convention (`vendor,model`), has a factory function registered in a central driver registry, and implements a fixed set of lifecycle methods. This means:

- Adding a new hardware variant requires touching exactly two files: the driver class and `hal_builtin_devices.cpp`.
- All devices go through the same probe → init → health check cycle with no special-casing in the main loop.
- The audio pipeline, web UI, and MQTT layer all interact with devices through the same manager API — they never talk to a driver directly.

The build flag `-D DAC_ENABLED` gates the entire HAL subsystem. All source files in `src/hal/` are excluded from non-HAL builds.

## Component Relationships

```mermaid
graph TD
    subgraph "Discovery Layer"
        DISC[hal_discovery]
        EEPROM[EEPROM v3 probe]
        I2CSCAN[I2C bus scan]
        DISC --> EEPROM
        DISC --> I2CSCAN
    end

    subgraph "Core Framework"
        MGR[HalDeviceManager\nsingleton]
        REG[HalDriverRegistry\ncompatible → factory]
        DB[HalDeviceDB\nin-memory presets]
        CFG[/hal_config.json\nper-device config]
        MGR --> CFG
        REG --> MGR
        DB --> REG
    end

    subgraph "Device Layer"
        DRV[HalDevice\nbase class]
        PCM5102A[HalPcm5102a]
        ES8311[HalEs8311]
        PCM1808[HalPcm1808]
        OTHERS[HalNs4150b\nHalMcp4725\nHalSigGen\nHalUsbAudio\n...]
        DRV --> PCM5102A
        DRV --> ES8311
        DRV --> PCM1808
        DRV --> OTHERS
    end

    subgraph "Integration"
        BRIDGE[hal_pipeline_bridge\nsink/source slots]
        PIPELINE[audio_pipeline]
        API[hal_api.cpp\nREST endpoints]
        BRIDGE --> PIPELINE
        MGR --> BRIDGE
        MGR --> API
    end

    DISC --> MGR
    MGR --> DRV
```

## Key Data Structures

### HalDevice — the base class

Every driver inherits from `HalDevice` (`src/hal/hal_device.h`). The two most important members are declared `volatile` because the audio pipeline task on Core 1 reads them without taking any lock:

```cpp
volatile bool           _ready;   // true only when AVAILABLE — read by pipeline hot path
volatile HalDeviceState _state;   // full state enum for UI and bridge logic
```

The pipeline reads `_ready` directly with no virtual dispatch. A state change that sets `_ready = false` is immediately visible to the audio task on the next DMA callback cycle without any locking overhead.

### HalDeviceDescriptor

Populated by each driver's constructor and never modified after registration:

```cpp
struct HalDeviceDescriptor {
    char          compatible[32];   // "ti,pcm5102a"
    char          name[33];         // "PCM5102A DAC"
    char          manufacturer[33]; // "Texas Instruments"
    HalDeviceType type;             // HAL_DEV_DAC, HAL_DEV_ADC, etc.
    uint16_t      legacyId;         // backward compat with DAC_ID_* constants
    HalBusRef     bus;              // bus type, index, pins, frequency
    uint8_t       i2cAddr;          // primary I2C address (0 = not I2C)
    uint8_t       channelCount;     // audio channels (1–8)
    uint32_t      sampleRatesMask;  // HAL_RATE_* bit flags
    uint8_t       capabilities;     // HAL_CAP_* flags
    uint8_t       instanceId;       // auto-assigned per compatible string
    uint8_t       maxInstances;     // maximum concurrent instances
};
```

### HalDeviceConfig

Runtime configuration that is persisted to `/hal_config.json`. Drivers read this during `init()` to honour any overrides the user set through the web UI:

```cpp
struct HalDeviceConfig {
    bool    valid;           // has been explicitly set
    uint8_t i2cAddr;         // address override (0 = use descriptor default)
    uint8_t i2cBusIndex;     // 0 = EXT, 1 = ONBOARD, 2 = EXPANSION
    int8_t  pinSda;          // -1 = use board default from config.h
    int8_t  pinBck;          // I2S bit clock GPIO (-1 = default)
    uint8_t volume;          // initial volume 0–100
    bool    mute;            // initial mute state
    bool    enabled;         // user enable/disable
    char    userLabel[33];   // custom display name
    // ... many more fields: I2S port, bit depth, PGA gain, HPF, GPIO overrides
};
```

## The Device Manager Singleton

`HalDeviceManager` (`src/hal/hal_device_manager.h`) owns all registered devices. It is a Meyers singleton — thread-safe construction on C++11 — accessed via `HalDeviceManager::instance()`.

### Slot Model

The manager holds a flat array of `HAL_MAX_DEVICES` (24) device pointers. A device's slot index is assigned at registration and never changes. The audio pipeline bridge uses slot indices as stable keys in its mapping tables.

```cpp
HalDeviceManager& mgr = HalDeviceManager::instance();

// Register a newly created device
int slot = mgr.registerDevice(dev, HAL_DISC_BUILTIN);

// Lookup
HalDevice* d = mgr.getDevice(slot);
HalDevice* d = mgr.findByCompatible("ti,pcm5102a");
HalDevice* d = mgr.findByCompatible("ti,pcm1808", /*instanceId=*/1); // second PCM1808
uint8_t n    = mgr.countByCompatible("ti,pcm1808"); // 2 if dual ADC fitted

// Iteration
mgr.forEach([](HalDevice* dev, void* ctx) {
    // called for every non-null slot
}, nullptr);
```

`instanceId` is auto-assigned at registration time by calling `countByCompatible()` before inserting the new device. The first PCM1808 gets `instanceId=0`, the second gets `instanceId=1`.

### Pin Claim System

Before a driver configures a GPIO it must claim it through the manager. Claiming fails if another driver has already taken the pin, preventing silent GPIO conflicts:

```cpp
// In driver init():
if (!HalDeviceManager::instance().claimPin(myGpio, HAL_BUS_GPIO, 0, _slot)) {
    return hal_init_fail(DIAG_HAL_INIT_FAILED, "GPIO already claimed");
}
// In driver deinit():
HalDeviceManager::instance().releasePin(myGpio);
```

The pin table tracks up to `HAL_MAX_PINS` (56) simultaneous claims, covering all ESP32-P4 GPIOs (0–54). GPIO numbers outside this range are rejected with a warning log.

### Priority-Sorted Initialisation

`initAll()` sorts all registered devices by `_initPriority` descending before calling `init()` on each one. This guarantees bus controllers are ready before their clients:

| Priority | Constant | Intended Use |
|---|---|---|
| 1000 | `HAL_PRIORITY_BUS` | I2C, I2S, SPI bus controllers |
| 900 | `HAL_PRIORITY_IO` | GPIO expanders, pin allocators |
| 800 | `HAL_PRIORITY_HARDWARE` | Audio codec / DAC / ADC chips |
| 600 | `HAL_PRIORITY_DATA` | Pipeline, metering consumers |
| 100 | `HAL_PRIORITY_LATE` | Diagnostics, logging |

### State Change Callback

The manager fires a single registered callback whenever a device's `_state` changes:

```cpp
typedef void (*HalStateChangeCb)(uint8_t slot, HalDeviceState oldState, HalDeviceState newState);

mgr.setStateChangeCallback(hal_pipeline_state_change);
```

`hal_pipeline_bridge` registers its `hal_pipeline_state_change` function here during `hal_pipeline_sync()` at boot. This is the only connection between the device manager and the audio pipeline — no direct coupling.

## Driver Registry

`HalDriverRegistry` (`src/hal/hal_driver_registry.h`) maps compatible strings to factory functions. It holds up to `HAL_MAX_DRIVERS` (16) entries.

```cpp
struct HalDriverEntry {
    char             compatible[32];
    HalDeviceType    type;
    uint16_t         legacyId;       // matches DAC_ID_* for EEPROM backward compat
    HalDeviceFactory factory;        // HalDevice* (*fn)()
};
```

All builtin drivers are registered in `hal_register_builtins()` (`src/hal/hal_builtin_devices.cpp`), which is called once from `setup()` before discovery runs.

```cpp
// Example factory registration (from hal_builtin_devices.cpp)
HalDriverEntry e;
memset(&e, 0, sizeof(e));
strncpy(e.compatible, "ti,pcm5102a", 31);
e.type    = HAL_DEV_DAC;
e.legacyId = 0x0001;
e.factory = []() -> HalDevice* { return new HalPcm5102a(); };
hal_registry_register(e);
```

## Device Database

`HalDeviceDB` (`src/hal/hal_device_db.h`) is an in-memory database of known device descriptors with preset configurations. It is separate from the driver registry: the registry knows how to *create* a device; the DB knows what its default *configuration* looks like.

The DB is populated from hardcoded entries for all builtin devices plus any EEPROM-discovered devices. The REST endpoint `GET /api/hal/db/presets` exposes the full list so the web UI can offer a preset picker when a user manually configures an expansion device.

## EEPROM v3 Discovery

Expansion modules can carry an AT24C02 EEPROM programmed with device identity information. The HAL reads this during discovery using the v3 format:

| Offset | Size | Contents |
|---|---|---|
| 0x00–0x5D | 94 bytes | v1/v2 legacy fields (device ID, name, I2C address) |
| 0x5E | 32 bytes | Compatible string — null-terminated, e.g. `"ti,pcm5102a"` |
| 0x7E | 2 bytes | CRC-16/CCITT over bytes 0x00–0x7D |
| 0x80–0xFF | 128 bytes | Reserved for driver-specific data |

`hal_eeprom_parse_v3()` validates the CRC and extracts the compatible string. If validation fails the firmware falls back to matching by the legacy numeric `deviceId` from v1/v2 fields.

## I2C Bus Architecture

The ESP32-P4 board has three I2C buses with distinct safety characteristics:

| Index | Constant | SDA | SCL | Safety | Devices |
|---|---|---|---|---|---|
| 0 | `HAL_I2C_BUS_EXT` | GPIO 48 | GPIO 54 | Skip when WiFi active | External expansion |
| 1 | `HAL_I2C_BUS_ONBOARD` | GPIO 7 | GPIO 8 | Always safe | ES8311 codec |
| 2 | `HAL_I2C_BUS_EXP` | GPIO 28 | GPIO 29 | Always safe | Expansion modules |

:::danger SDIO Conflict on Bus 0
GPIO 48 and GPIO 54 are shared with the ESP32-C6 WiFi co-processor SDIO interface. Any I2C transaction on Bus 0 while WiFi is active causes `sdmmc_send_cmd` errors and triggers an MCU reset. The discovery routine automatically skips Bus 0 when `appState.ethernet.activeInterface == NET_WIFI`. Never scan Bus 0 without this guard.
:::

Discovery scans Bus 1 (ONBOARD) only through the ES8311 driver's existing Wire instance. Buses 0 and 2 are initialised, scanned, then released in `hal_i2c_scan_bus()` to avoid holding GPIO pins unnecessarily.

## Config Persistence

Per-device configuration is persisted to `/hal_config.json` on LittleFS. The structure is a JSON array keyed by slot index. Changes made through the web UI are applied immediately and written to flash; they survive power cycles and OTA updates.

The REST endpoint `PUT /api/hal/devices` accepts a partial config object — only the fields present in the request body are updated. The driver's `init()` method is then re-run with the new config active.

## REST API Surface

All HAL management is exposed under `/api/hal/`:

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/api/hal/devices` | List all registered devices with state, config, and retry counters |
| `POST` | `/api/hal/scan` | Trigger a full rescan (returns 409 if scan already in progress) |
| `PUT` | `/api/hal/devices` | Update per-device config (triggers reinit) |
| `DELETE` | `/api/hal/devices` | Remove a device (sets state to REMOVED) |
| `POST` | `/api/hal/devices/reinit` | Force reinitialise a specific device slot |
| `GET` | `/api/hal/db/presets` | Return the full device DB preset list |

## Pipeline Bridge

`hal_pipeline_bridge` (`src/hal/hal_pipeline_bridge.h`) is the glue between device lifecycle events and the audio pipeline's slot-indexed sink/source API. It is covered in detail in the [Device Lifecycle](./device-lifecycle.md) page. The short version:

- When a device reaches `AVAILABLE`, the bridge assigns it a pipeline sink slot (for DAC/CODEC devices) or an ADC lane (for ADC devices) using first-fit capability-based ordinal counting.
- When a device becomes `UNAVAILABLE` (transient), the bridge sets `_ready = false` and leaves the slot/lane registered — the pipeline skips it automatically.
- When a device reaches `ERROR`, `REMOVED`, or `MANUAL`, the bridge explicitly calls `audio_pipeline_remove_sink()` or `audio_pipeline_remove_source()` to free the slot.
