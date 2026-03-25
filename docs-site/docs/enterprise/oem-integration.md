---
title: OEM Integration
sidebar_position: 2
description: HAL driver tier system, carrier board mezzanine connector, BOM guidance, and firmware customisation points for OEMs.
---

# OEM Integration

This page covers the technical integration points for OEMs building carrier boards or mezzanine modules on the ALX Nova platform.

---

## HAL Driver Tier System

The HAL uses a three-tier model that balances ease of integration with runtime control capability. Choose the tier that matches your device's complexity and the control features your product needs.

### Tier 1 — I2S Passthrough (No I2C Init)

Devices that self-configure from pin strapping or that run with default register settings. The HAL registers an audio sink or source for the device, but does not send any I2C commands at init time or at runtime.

**When to use Tier 1:** Fixed-configuration ADC/DAC modules, modules where I2C is not required, or prototyping scenarios where the init sequence is not yet known.

**How to implement:** Add a Tier 1 custom device entry via `POST /api/hal/devices/custom` with `"tier": 1`. No driver code is required.

### Tier 2 — Static I2C Init Sequence

Devices that require a fixed sequence of I2C register writes at init time but do not need runtime volume or filter commands from the firmware. The HAL executes the register write list at `init()` and then leaves the device running.

**When to use Tier 2:** DACs and ADCs that require power-on register programming but expose volume or filter control through hardware controls (potentiometers, DIP switches) rather than firmware.

**How to implement:** Add a Tier 2 custom device entry via `POST /api/hal/devices/custom` with `"tier": 2` and a `registers` array. The custom device creator in the web UI generates the correct JSON schema.

### Tier 3 — Full HAL Driver

Full C++ driver class implementing the complete `HalDevice` interface: `init()`, `deinit()`, `setVolume()`, `setMute()`, `setFilterMode()`, and capability reporting via `getCapabilities()`. Runtime web UI and MQTT controls work automatically once the driver implements the interface.

**When to use Tier 3:** All production devices that need web UI volume/mute/filter control, devices with complex init sequences (conditional writes, read-modify-write, firmware loading), and any device you intend to contribute to the public driver registry.

**How to implement:** Follow the [Driver Guide](../developer/hal/driver-guide) and reference the [Drivers reference](../developer/hal/drivers). Use `HAL_REGISTER()` macro to register the driver factory. Drivers must pass the HAL unit test suite before merging.

Cross-reference: [HAL Overview](../developer/hal/overview), [Device Lifecycle](../developer/hal/device-lifecycle).

---

## Carrier Board Mezzanine Connector

The ALX Nova expansion connector is a 16-pin keyed header (Revision 2+). Full electrical and mechanical specification is in [Mezzanine Connector Standard](../developer/hal/mezzanine-connector).

Key points for carrier board designers:

- **Pins 1–9**: Shared bus signals (5V, 3.3V, GND×2, I2C SDA/SCL, I2S BCK/WS/MCLK)
- **Pins 10–14**: Per-slot I2S data lines and device control (DIN, DOUT, CHIP_EN, INT_N, RESERVED)
- **Pins 15–16**: Analog power rails (+15V and −15V), added in Revision 2 for op-amp I/V stage mezzanines

The EEPROM auto-discovery mechanism probes I2C Bus 2 (GPIO 28/29) addresses 0x50–0x57 at boot. Each slot maps to one address offset. Program the AT24C02 EEPROM using `tools/eeprom/` scripts before populating the mezzanine.

:::note Revision 1 vs Revision 2 connectors
Revision 1 carrier boards use a 14-pin connector — pins 15 and 16 are absent. The ±15V rails must be sourced externally on Revision 1 designs. All new designs should target the 16-pin Revision 2 pinout. See [Power Supply Design](../developer/hal/power-supply) for rail specifications.
:::

---

## BOM Silicon Guidance

The following silicon has pre-tested HAL drivers and EEPROM images. Selecting from this list eliminates driver development time.

### ADC Recommendations

| Part | Compatible String | Channels | Notes |
|---|---|---|---|
| ES9822PRO | `ess,es9822pro` | 2 (I2S) | 2-channel reference, Pattern A |
| ES9843PRO | `ess,es9843pro` | 4 (TDM) | 4-channel reference, Pattern B |
| ES9826 | `ess,es9826` | 2 (I2S) | 30 dB PGA range |
| ES9823PRO | `ess,es9823pro` | 2 (I2S) | 42 dB PGA range |
| ES9841 | `ess,es9841` | 4 (TDM) | 42 dB PGA range |

### DAC Recommendations

| Part | Compatible String | Channels | Notes |
|---|---|---|---|
| ES9038Q2M | `ess,es9038q2m` | 2 (I2S) | Hyperstream II, 128 dB DNR, DSD512, Pattern C |
| ES9038PRO | `ess,es9038pro` | 8 (TDM) | Hyperstream II, 132 dB DNR, Pattern D |
| CS43198 | `cirrus,cs43198` | 2 (I2S) | MasterHIFI, 130 dBA DNR, 7 filters, DSD256 |
| CS43131 | `cirrus,cs43131` | 2 (I2S) | MasterHIFI, integrated headphone amp |
| ES9039Q2M | `ess,es9039q2m` | 2 (I2S) | Hyperstream IV, 130 dB DNR, DSD1024 |

For the full device matrix with I2C addresses and capability flags see [Mezzanine Connector — Supported Expansion Devices](../developer/hal/mezzanine-connector#supported-expansion-devices).

---

## Build Flag Customisation

The firmware is controlled by a set of compile-time flags in `platformio.ini` under `build_flags`. OEM forks typically adjust the following:

| Flag | Default | Purpose |
|---|---|---|
| `-D GUI_ENABLED` | Defined | Enables LVGL TFT display (ST7735S 128×160). Remove for headless products. |
| `-D DAC_ENABLED` | Defined | Enables DAC-specific HAL drivers and web UI tab. |
| `-D USB_AUDIO_ENABLED` | Defined | Enables USB Audio Class device (USB HS). Remove if USB port is not exposed. |
| `-D DSP_ENABLED` | Defined | Enables 4-channel biquad/FIR/limiter/gain/delay/compressor DSP pipeline. |
| `-D FIRMWARE_VERSION=\"1.0.0\"` | From `config.h` | Override firmware version string for OEM release tracking. |
| `-D OTA_RELEASE_URL=\"https://...\"` | Public repo | Override the OTA update check URL for private release hosting. |

:::tip Headless production units
For products without a display, remove `-D GUI_ENABLED` to eliminate the LVGL task and its ~120 KB code footprint. The TFT-related hardware pins are then unused and can be reassigned.
:::

---

## Firmware Versioning for OEM Forks

- Fork from a tagged release (e.g. `v1.20.0`), not from `main`. The `main` branch carries in-progress features that may not be stable.
- Update `FIRMWARE_VERSION` in `src/config.h` for every production build. The version string is reported by `GET /api/system/info`, exposed in MQTT birth messages, and used by the OTA update checker.
- For private OTA hosting, set `OTA_RELEASE_URL` in `platformio.ini` to your internal release endpoint. The endpoint must return a JSON response with `latest_version` and `download_url` fields matching the standard ALX release format.
- Pin the ESP-IDF / Arduino-ESP32 versions in `platformio.ini` to avoid unexpected regressions during a production run. Increment only on a planned maintenance window with full regression testing.
