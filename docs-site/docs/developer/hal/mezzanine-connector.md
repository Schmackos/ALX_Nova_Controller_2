---
sidebar_position: 5
title: Mezzanine Connector Standard
description: Expansion connector pinout and design guide for ALX Nova Controller 2 mezzanine modules
---

The ALX Nova Controller 2 supports standardized mezzanine expansion modules that sit on top of the carrier board via a defined mechanical and electrical interface. Each module connects through a single keyed connector that carries I2C control, I2S audio clocks and data, and regulated power. At boot the HAL's tier-2 discovery mechanism probes each populated slot for an EEPROM carrying the module's compatible string, allowing plug-and-play device registration with no manual configuration required.

Community-built modules can apply for "Works with ALX" certification. Certified commercial modules are permitted to display the dedicated "Certified" badge and may be sold through the ALX store. See the project website for certification requirements.

## Connector Pinout

The connector is a 14-pin keyed header. Pins 1–9 carry shared bus signals present on every slot; pins 10–14 are per-slot and routed to unique GPIOs on the carrier board.

### Shared bus signals

These signals are common to all expansion slots. Modules must not drive them as outputs except where the direction is explicitly listed as bidir.

| Pin | Signal | Direction | Notes |
|-----|--------|-----------|-------|
| 1 | 5V | Power | Source from carrier onboard LDO; fused per-slot at 500 mA |
| 2 | 3.3V | Power | Digital I/O reference; isolate analog supply with ferrite bead on the mezzanine |
| 3 | GND | Power | Digital ground — first of two ground pins |
| 4 | GND | Power | Additional ground return; wire both pins to reduce impedance |
| 5 | I2C\_SDA | Bidir | I2C Bus 2, GPIO 28 on carrier; 4.7 kΩ pull-up on carrier side |
| 6 | I2C\_SCL | Bidir | I2C Bus 2, GPIO 29 on carrier; 4.7 kΩ pull-up on carrier side |
| 7 | I2S\_BCK | Bidir | Shared bit clock; driven by whichever device is the I2S master |
| 8 | I2S\_WS | Bidir | Shared word select / LRCLK; driven by the I2S master |
| 9 | I2S\_MCLK | Bidir | Master clock; footprint on the mezzanine for either a local oscillator or a jumper to this carrier pin |

### Per-slot signals

Each connector position on the carrier board routes the following signals to distinct GPIOs. The GPIO numbers for each physical slot are documented in the carrier board hardware reference.

| Pin | Signal | Direction | Notes |
|-----|--------|-----------|-------|
| 10 | DIN | Input | I2S audio data from mezzanine ADC to ESP32 RX |
| 11 | DOUT | Output | I2S audio data from ESP32 TX to mezzanine DAC |
| 12 | CHIP\_EN | Output | Device enable, active high; de-asserted during HAL deinit |
| 13 | INT\_N | Input | Optional open-drain interrupt or status output from the module |
| 14 | RESERVED | — | Reserved for future use; leave unconnected on mezzanine |

:::info INT\_N is optional
The HAL does not poll or wait on INT\_N during normal operation. It is intended for future event-driven health monitoring. Modules that do not implement an interrupt may leave pin 13 unconnected on both ends.
:::

## Architecture Decisions

### Shared clock bus

All expansion modules share a single set of BCK, WS, and MCLK clock lines. This means all active expansion devices must operate at the same sample rate at any given time — there is no clock domain crossing or asynchronous sample rate conversion between slots. The audio pipeline enforces a single active sample rate across all registered sources and sinks.

Which device acts as I2S master is configured per-device through `HalDeviceConfig.isI2sClockMaster`. In the typical configuration the carrier's ESP32-P4 drives BCK, WS, and MCLK on I2S2, and all mezzanine devices operate as I2S slaves consuming those clocks. A mezzanine with a high-precision fixed oscillator may instead act as master — in that case the ESP32 I2S2 peripheral must be configured in slave receive mode and `isI2sClockMaster` set to `false` for that device's HAL entry.

### I2S peripheral assignment

The ESP32-P4 provides three I2S peripherals:

| Peripheral | Assignment |
|------------|------------|
| I2S0 | Onboard ADC1 (PCM1808, I2S master RX — BCK, WS, MCLK output) |
| I2S1 | Onboard ADC2 (PCM1808, master RX — data only; clocks from I2S0) |
| I2S2 | Expansion slot (full-duplex with ES8311 TX; also available for mezzanine ADC RX) |

Mezzanine ADC modules are assigned to I2S2 RX. Mezzanine DAC modules are assigned to I2S2 TX. Full-duplex operation (simultaneous ADC and DAC on I2S2) follows the same paired channel-handle pattern as the onboard peripherals.

### TDM mode

I2S2 supports TDM (Time Division Multiplexed) framing for multi-channel mezzanines. A TDM-capable module can deliver up to 16 audio channels over the single DIN line by interleaving slots within each LRCLK frame. Configure the TDM slot mask through `HalDeviceConfig` and ensure the mezzanine's TDM frame width matches the carrier configuration. Standard stereo modules use I2S mode with no TDM framing.

## EEPROM Auto-Discovery

Mezzanine modules that carry an AT24C02 EEPROM (or compatible 2 kbit device) are recognized automatically at boot without any manual configuration. The discovery flow is:

1. HAL tier-1 scans I2C Bus 2 (GPIO 28/29) for responding addresses.
2. For each slot offset address in the range 0x50–0x57, tier-2 probes for the EEPROM and reads the v3 layout.
3. The compatible string extracted from the EEPROM (offset 0x5E, null-terminated, 32 bytes) is looked up in the HAL driver registry.
4. If a matching factory is found, the device is registered, probed, and initialized in priority order.
5. If no match is found the module is logged as UNKNOWN and appears in the web UI device list for manual identification.

| Slot | EEPROM Address |
|------|----------------|
| 0 | 0x50 |
| 1 | 0x51 |
| 2 | 0x52 |
| 3–7 | 0x53–0x57 |

The EEPROM content follows the HAL v3 format:

| Offset | Size | Contents |
|--------|------|----------|
| 0x00–0x5D | 94 bytes | v1/v2 legacy fields (device ID, name, I2C address) |
| 0x5E | 32 bytes | Compatible string, null-terminated, e.g. `"ess,es9822pro"` |
| 0x7E | 2 bytes | CRC-16/CCITT over bytes 0x00–0x7D |
| 0x80–0xFF | 128 bytes | Reserved for driver-specific data |

Modules without an EEPROM can still be used by manually adding the device through the REST API (`POST /api/hal/devices`) or the web UI. The HAL will not attempt auto-discovery for slots where no EEPROM ACKs.

:::tip Programming the EEPROM
A Python flashing script and a set of pre-built binary images for supported modules are available in `tools/eeprom/`. Write the image with any I2C-capable programmer (e.g. CH341A or a Raspberry Pi) before populating the mezzanine on the carrier. The CRC must be valid or the HAL falls back to legacy numeric device ID matching.
:::

## Signal Flow Diagram

```mermaid
graph LR
    subgraph Mezzanine
        ADC[ESS SABRE ADC]
        EEPROM[AT24C02\n0x50+slot]
        OSC[MCLK Oscillator\noptional]
    end

    subgraph ESP32-P4
        I2S2[I2S2 RX]
        I2C2[I2C Bus 2\nGPIO 28/29]
        HAL[HAL Driver]
        Pipeline[Audio Pipeline]
    end

    ADC -->|I2S DIN pin 10| I2S2
    EEPROM -->|Compatible string| I2C2
    I2C2 -->|Register control| ADC
    OSC -->|MCLK optional| ADC
    I2S2 --> HAL
    HAL -->|AudioInputSource| Pipeline
```

The carrier drives BCK and WS to the mezzanine in the default clock-master configuration. When the mezzanine provides its own MCLK oscillator, pin 9 is left floating on the carrier side and the module drives the ADC directly. The HAL driver sets `_descriptor.bus` accordingly and the HAL discovery layer resolves the I2S pin map from `HalDeviceConfig` at init time.

## Supported Expansion Devices

The following expansion ADC modules have registered HAL drivers and EEPROM images:

| Device | Compatible | Channels | Volume Control | PGA Gain Range | I2C Address |
|--------|-----------|----------|----------------|----------------|-------------|
| ES9822PRO | `ess,es9822pro` | 2 | 16-bit digital | 0–18 dB | 0x40 |
| ES9843PRO | `ess,es9843pro` | 4 | 8-bit digital | 0–42 dB | 0x40 |

Both devices are registered in `hal_builtin_devices.cpp` and have preset entries in the device database (`GET /api/hal/db/presets`). Multi-channel modules (ES9843PRO) report `channelCount = 4` in their descriptor and are assigned four consecutive lanes in the audio pipeline input matrix when the pipeline bridge calls `audio_pipeline_set_source()`.

:::info Adding your own module
If you are developing a new mezzanine module, follow the [Driver Guide](./driver-guide.md) to create the HAL driver class, register the factory, and write the unit tests. Use `"ess,es9822pro"` as a reference implementation for I2C ADC devices with PGA control.
:::

## Design Guidelines for Mezzanine Makers

The following guidelines apply to all modules seeking "Works with ALX" certification.

### MCLK supply

Provide a footprint for both a local fixed-frequency crystal oscillator and a zero-ohm jumper to the carrier's MCLK line (pin 9). Use a solder-bridge or 0402 zero-ohm resistor to select between the two at assembly time. This allows the same PCB to operate in both clock-master (standalone oscillator) and clock-slave (carrier MCLK) configurations.

Common MCLK frequencies for ESS SABRE ADCs:

| Sample Rate | Required MCLK |
|-------------|--------------|
| 44.1 kHz | 22.5792 MHz |
| 48 kHz | 24.576 MHz |
| 96 kHz | 24.576 MHz |
| 192 kHz | 24.576 MHz |

### Power supply

ESS SABRE ADCs require a clean analog supply (AVCC) separate from the digital 3.3V rail. Derive AVCC from the 5V pin (pin 1) through a low-noise LDO set to 4.5V. Do not power AVCC from the 3.3V pin — the 3.3V rail on the carrier may carry switching noise from the ESP32 core regulators.

Isolate the module's 3.3V analog circuits from the digital 3.3V supply using a ferrite bead (100 Ω at 100 MHz, rated at least 500 mA) in series with a pi filter. Keep AGND and DGND joined at a single point on the mezzanine (star topology), with the star point as close as possible to the ADC's ground pad.

### Decoupling

Place decoupling capacitors as close as possible to each analog supply pin on the ADC package:

| Position | Capacitors |
|----------|-----------|
| Per AVCC pin | 10 µF (MLCC X5R) + 100 nF (C0G/NP0) + 10 nF (C0G/NP0) |
| Per DVCC pin | 10 µF (MLCC X5R) + 100 nF (C0G/NP0) |
| MCLK oscillator VCC | 10 µF + 100 nF + 10 nF |

Use C0G/NP0 dielectric for the small-value capacitors on analog supplies. X5R is acceptable for bulk capacitance. Avoid Y5V or Z5U dielectrics on any supply pin.

### PCB layout

- Route I2S signal traces (BCK, WS, DIN) as matched-length pairs. Maximum skew between BCK and DIN should be less than 200 ps (approximately 30 mm length difference on standard FR4).
- Keep I2S traces away from the MCLK oscillator and its associated power traces.
- Place the EEPROM (if fitted) on the I2C Bus 2 side of the connector with short traces to SDA and SCL. The carrier provides 4.7 kΩ pull-ups; do not add additional pull-ups on the mezzanine.
- Expose the module's compatible string silk-screen label on the top layer for easy identification.

### Mechanical

The connector on the carrier is a 2.54 mm pitch, 14-pin, single-row, right-angle or vertical male header. The mezzanine mates with a matching female socket. Provide at minimum two M3 mounting hole positions aligned with the carrier board standoff pattern to prevent mechanical stress on the connector pins.

:::warning Keying
Pin 1 is the 5V power supply. Reversing the connector will apply 5V to the I2C and I2S signal lines, permanently damaging the ESP32-P4 GPIO inputs and the ADC. The connector must be mechanically keyed (polarized housing or pin-1 marker silk screen) to prevent incorrect insertion. All officially certified modules use a keyed Molex PicoBlade or equivalent connector housing.
:::
