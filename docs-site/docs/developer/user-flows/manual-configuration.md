---
title: Manual Device Configuration
sidebar_position: 5
description: How a user manually configures HAL device parameters (I2C address, I2S port, volume, filters) via the web UI.
---

# Manual Device Configuration

After a device is discovered — either through EEPROM auto-discovery or manual registration — the user can modify its configuration through the web UI's HAL Devices tab. The configuration form exposes every field in `HalDeviceConfig`: I2C bus, address, and speed; I2S port assignment, sample rate, bit depth, MCLK multiple, and data format; audio parameters such as volume, mute, filter mode, PGA gain, and HPF; and GPIO pin overrides for devices that use discrete control lines.

Changes are transmitted to the firmware via a `PUT /api/hal/devices` REST call. The firmware merges the updated fields into the in-memory `HalDeviceConfig`, persists the result atomically to `/hal_config.json`, and then applies the new values to the device. Parameters that map directly to I2C register writes (volume, mute, filter, gain, HPF) take effect immediately. Parameters that affect the I2S or I2C bus topology (port number, I2C bus, bus address, pin assignments) require a subsequent device reinit to take effect, and the web UI shows a "Reinit needed" indicator until the user triggers one.

## Preconditions

- Device is registered in the HAL manager (slot assigned, state is not `REMOVED`)
- The web UI has an authenticated session
- The device card is visible on the HAL Devices tab
- For runtime parameter changes: device state is `AVAILABLE` or `UNAVAILABLE`
- For bus/port changes: the target I2S port is not already claimed by another active device

## Sequence Diagram

```mermaid
sequenceDiagram
    actor User
    participant WebUI as Web UI
    participant REST as REST API
    participant HAL as HAL Manager
    participant Dev as HAL Device
    participant FS as LittleFS
    participant WS as WebSocket

    User->>WebUI: Click Edit / Configure on device card
    WebUI->>WebUI: Populate config form with current<br/>HalDeviceConfig values (slot, i2cBus,<br/>i2cAddr, i2sPort, volume, filter, gain…)

    User->>WebUI: Modify one or more parameters,<br/>then click Save

    WebUI->>REST: PUT /api/hal/devices<br/>JSON body: { slot, i2cBus, i2cAddr,<br/>i2sPort, volume, mute, filterMode,<br/>pgaGain, hpfEnabled, … }

    REST->>REST: Parse JSON body,<br/>extract slot number

    alt Slot out of range or device not found
        REST-->>WebUI: 400 Bad Request
    else Valid slot
        REST->>HAL: mgr.getConfig(slot)<br/>retrieve current HalDeviceConfig

        REST->>REST: Merge updated fields into<br/>new HalDeviceConfig struct

        REST->>HAL: mgr.setConfig(slot, newCfg)<br/>update in-memory config

        rect rgb(30, 40, 60)
            Note over REST,FS: Atomic config persistence
            REST->>FS: hal_save_device_config(slot)<br/>write /hal_config.json.tmp
            FS->>FS: rename .tmp → /hal_config.json
            FS-->>REST: save complete
        end

        REST->>HAL: hal_apply_config(slot)

        alt Runtime-changeable parameters (volume, mute, filter, gain, HPF)
            HAL->>Dev: dev->setVolume(newVol)<br/>dev->setMute(muted)<br/>dev->setFilter(filterMode)<br/>dev->setGain(pgaGain)<br/>dev->setHpf(hpfEnabled)
            Dev->>Dev: I2C register write(s) via _writeReg()
            Dev-->>HAL: OK
            HAL-->>REST: applied immediately
        else Reinit-required parameters (I2S port, I2C bus/address, pin assignments)
            HAL->>HAL: Flag device: reinitNeeded = true
            HAL-->>REST: config saved, reinit needed
            Note over HAL,WebUI: User must POST /api/hal/devices/reinit<br/>to activate bus/port changes
        end

        REST-->>WebUI: 200 OK<br/>{ slot, state, reinitNeeded, config }

        REST->>WS: markHalDeviceDirty()
        WS-->>WebUI: broadcast halDevices JSON<br/>(updated config values, reinitNeeded flag)

        WebUI->>WebUI: Refresh device card —<br/>show new config values;<br/>display "Reinit needed" indicator if flagged
    end
```

## Step-by-Step Walkthrough

### 1. Opening the configuration form

The user locates the device card on the **HAL Devices** tab and clicks the **Edit** or **Configure** button (rendered by `web_src/js/15-hal-devices.js`). The web UI reads the current `HalDeviceConfig` values from the last received `halDevices` WebSocket broadcast and pre-populates every field in the form: I2C bus number, I2C address (hex), I2S port index, sample rate, bit depth, MCLK multiple, data format, volume (0–255 or 0.0–1.0 depending on device capability), mute toggle, filter preset, PGA gain, HPF enabled state, and any GPIO pin overrides. Fields that are not applicable to the device type (e.g., PGA gain on a DAC-only device) are hidden or disabled.

### 2. User edits parameters

The user changes one or more fields. Common configuration tasks include:

- **Reassigning the I2S port** — moving the device from port 0 (onboard I2S) to port 1 or 2 (expansion connectors) when multiple mezzanines share the carrier board.
- **Correcting the I2C address** — required when an address jumper on the mezzanine board is repositioned.
- **Adjusting volume or gain** — fine-tuning the analogue signal level.
- **Selecting a filter preset** — choosing between FIR filter profiles (e.g., the 7 presets on ESS SABRE DACs or the 8 presets on ESS SABRE ADCs).
- **Enabling or disabling HPF** — toggling the high-pass filter on ADC drivers.

### 3. PUT /api/hal/devices

Clicking **Save** sends `PUT /api/hal/devices` (registered in `src/hal/hal_api.cpp`). The JSON body includes the slot number and every field the form exposes. Fields not present in the body are left unchanged by the merge logic on the firmware side.

```
PUT /api/hal/devices
Content-Type: application/json

{
  "slot": 2,
  "i2cBus": 2,
  "i2cAddr": 64,
  "i2sPort": 1,
  "volume": 200,
  "mute": false,
  "filterMode": 3,
  "pgaGain": 6,
  "hpfEnabled": true
}
```

### 4. Slot validation and config retrieval

The REST handler in `src/hal/hal_api.cpp` extracts the slot number from the JSON body. If the slot is out of range (outside 0–31) or maps to an unregistered device entry, the handler returns HTTP 400 immediately. Otherwise it calls `mgr.getConfig(slot)` on the `HalDeviceManager` singleton to retrieve the current `HalDeviceConfig` struct as a starting point.

### 5. Config merge

The handler overlays only the fields present in the request body onto the retrieved struct. Fields absent from the body retain their existing values. This merge pattern means the caller does not need to round-trip the entire config — a `PUT` with only `{ "slot": 2, "volume": 180 }` is valid and changes only the volume.

### 6. In-memory update

`mgr.setConfig(slot, newCfg)` writes the merged struct back into the manager's internal device table in `src/hal/hal_device_manager.cpp`. This is the single source of truth for all subsequent reads — including the next `GET /api/hal/devices` response and the next WebSocket `halDevices` broadcast.

### 7. Atomic config persistence

`hal_save_device_config(slot)` in `src/hal/hal_settings.cpp` serializes the full `/hal_config.json` (all registered devices) and writes it atomically:

1. Write to `/hal_config.json.tmp`
2. `rename("/hal_config.json.tmp", "/hal_config.json")`

If power is lost between steps 1 and 2, the `.tmp` file is left on disk. On the next boot, the settings loader detects the `.tmp` file, completes the rename, and recovers the updated config — no configuration data is lost.

### 8. Applying the new config — runtime vs reinit-required changes

`hal_apply_config(slot)` in `src/hal/hal_device_manager.cpp` inspects which fields changed and routes accordingly.

**Runtime-changeable parameters** — applied immediately via I2C register writes without restarting the device:

| Parameter | Mechanism | Notes |
|-----------|-----------|-------|
| Volume | `dev->setVolume()` → `_writeReg16()` | ESS SABRE: 16-bit or 8-bit per-channel register |
| Mute | `dev->setMute()` → `_writeReg()` | Mute bit in control register |
| Filter mode | `dev->setFilter()` → `_writeReg()` | Preset index written directly |
| PGA gain | `dev->setGain()` → `_writeReg()` | Gain steps: 2-bit (ES9820), 3-bit (ES9823PRO), nibble-packed (ES9826) |
| HPF enabled | `dev->setHpf()` → `_writeReg()` | HPF control bit |

All I2C writes use `_writeReg()` / `_writeReg16()` helpers from `HalEssSabreAdcBase` (`src/hal/hal_ess_sabre_adc_base.cpp`) or `HalEssSabreDacBase` (`src/hal/hal_ess_sabre_dac_base.cpp`). A failure at this layer is logged with `LOG_W` and the device remains in its previous state.

**Reinit-required parameters** — cannot be applied to a running device; the I2S peripheral or I2C bus must be reconfigured:

| Parameter | Why reinit is required |
|-----------|------------------------|
| I2S port | The I2S peripheral is installed/uninstalled during `init()`/`deinit()` |
| I2C bus | Wire selection (`_selectWire()`) is fixed at init time |
| I2C address | Stored in `_i2cAddr`; used in every `_writeReg()` call after init |
| Bit depth / data format | I2S DMA channel is configured once per init |
| MCLK multiple | Clock divider chain set during `i2s_port_enable_tx/rx()` |
| GPIO pin overrides | Pins are claimed via `hal_pin_claim()` during `init()` |

When any reinit-required field changes, the firmware sets an internal `reinitNeeded` flag on the device entry and returns it in the REST response and the WebSocket broadcast. The web UI renders a **"Reinit needed"** indicator on the device card. The user completes the change by clicking **Reinit** (which sends `POST /api/hal/devices/reinit`) — see [Device Reinit / Error Recovery](device-reinit) for the full reinit flow.

### 9. REST response

The handler returns HTTP 200 with a JSON body reflecting the updated device state:

```json
{
  "slot": 2,
  "state": "AVAILABLE",
  "reinitNeeded": false,
  "config": {
    "i2cBus": 2,
    "i2cAddr": 64,
    "i2sPort": 1,
    "volume": 200,
    "mute": false,
    "filterMode": 3,
    "pgaGain": 6,
    "hpfEnabled": true
  }
}
```

If `reinitNeeded` is `true`, the `state` field will still show the current state (e.g., `AVAILABLE`) because the device is still running on the old config until reinit occurs.

### 10. WebSocket broadcast

`markHalDeviceDirty()` signals `src/websocket_broadcast.cpp` to send an updated `halDevices` JSON broadcast to all connected clients. The broadcast includes the full device list with the revised `HalDeviceConfig` values and the `reinitNeeded` flag for the affected slot. All open browser tabs update simultaneously.

### 11. Web UI update

`15-hal-devices.js` receives the `halDevices` broadcast and re-renders the device card. The new parameter values appear in the config display. If `reinitNeeded` is set, an amber **"Reinit needed"** badge appears alongside a **Reinit** button that links to the reinit flow.

## Postconditions

- Device config updated in memory (`HalDeviceManager` device table) and on disk (`/hal_config.json`, atomic write)
- Runtime-changeable parameters (volume, mute, filter, gain, HPF) applied immediately via I2C register writes
- Reinit-required parameter changes recorded; device continues operating on previous config until reinit is triggered
- Web UI device card reflects new config values; "Reinit needed" indicator shown when applicable
- All connected WebSocket clients have received an updated `halDevices` broadcast

## Error Scenarios

| Trigger | Behaviour | Recovery |
|---------|-----------|----------|
| Invalid slot number | REST returns 400 Bad Request | Verify the slot number matches an entry in the current device list |
| I2C write failure (volume / filter) | `LOG_W` emitted; device stays in its current state; REST still returns 200 with the saved config | Check I2C bus connectivity and confirm the device is powered; use the Debug Console to inspect `[HAL]` log entries |
| Config save failure (LittleFS write error) | Atomic write leaves `/hal_config.json.tmp` on disk; in-memory config is updated | On next boot the loader detects `.tmp` and completes the rename, recovering the updated config |
| I2S port conflict | Two devices configured for the same I2S port index | Assign one of the devices to a different port, then reinit both |
| Invalid GPIO pin assignment | `hal_pin_claim()` fails — pin already owned by another device; `LOG_W` emitted | Review the pin allocation table via `GET /api/hal/devices` and resolve the conflict before retrying reinit |
| Reinit-required change without triggering reinit | Device continues on old config; `reinitNeeded` flag remains set | Click **Reinit** on the device card or send `POST /api/hal/devices/reinit` with the slot number |

## Related

- [Device Enable/Disable Toggle](device-toggle) — toggling a device on or off after a config change
- [Device Reinit / Error Recovery](device-reinit) — triggering a full probe+init cycle after bus or port changes
- [REST API (HAL)](../api/rest-hal) — full reference for `PUT /api/hal/devices` and `POST /api/hal/devices/reinit`
- [HAL Device Lifecycle](../hal/device-lifecycle) — state machine diagram covering `CONFIGURING`, `AVAILABLE`, `ERROR`, and `REMOVED` transitions

**Source files:**
- `src/hal/hal_api.cpp` — `PUT /api/hal/devices` REST handler and config merge logic
- `src/hal/hal_settings.cpp` — atomic `/hal_config.json` persistence (`hal_save_device_config`)
- `src/hal/hal_device_manager.cpp` — `getConfig()`, `setConfig()`, and `hal_apply_config()` implementations
- `src/hal/hal_ess_sabre_adc_base.cpp` — shared I2C write helpers for ESS SABRE ADC drivers
- `src/hal/hal_ess_sabre_dac_base.cpp` — shared I2C write helpers for ESS SABRE DAC drivers
- `web_src/js/15-hal-devices.js` — config form population, Save handler, and "Reinit needed" indicator rendering
