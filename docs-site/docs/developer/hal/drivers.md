---
title: Built-in Drivers
sidebar_position: 4
description: Reference for all built-in HAL drivers — PCM5102A, ES8311, PCM1808, NS4150B, MCP4725, ESS SABRE ADC expansion family, ESS SABRE DAC expansion family, Cirrus Logic DAC expansion family, amplifier relay, and more.
---

This page documents every driver registered by `hal_register_builtins()` in `src/hal/hal_builtin_devices.cpp`. Each entry covers the compatible string, device class, type, capabilities, bus requirements, and any runtime configuration options that affect behaviour.

:::note Deprecated wrappers removed
The 19 device-specific I2S wrapper functions that existed before the port-generic API (`i2s_configure_adc1()`, `i2s_configure_dac_tx()`, etc.) have been removed. All drivers now call the port-generic API (`i2s_port_enable_tx()`, `i2s_port_enable_rx()`, `i2s_port_write()`, `i2s_port_read()`) with the port index from `HalDeviceConfig.i2sPort`. If you are porting a driver that still references the old functions, replace them with their port-generic equivalents.
:::

## Quick Reference

| Compatible String | Class | Type | Bus | I2C Addr | Capabilities |
|---|---|---|---|---|---|
| `ti,pcm5102a` | `HalPcm5102a` | DAC | I2S | — | `DAC_PATH`, `MUTE`, `HW_VOLUME` |
| `everest-semi,es8311` | `HalEs8311` | CODEC | I2C + I2S | 0x18 | `DAC_PATH`, `ADC_PATH`, `CODEC`, `HW_VOLUME`, `FILTERS`, `MUTE`, `PGA_CONTROL`, `HPF_CONTROL` |
| `ti,pcm1808` | `HalPcm1808` | ADC | I2S | — | `ADC_PATH` |
| `ns,ns4150b-amp` | `HalNs4150b` | AMP | GPIO | — | — |
| `espressif,esp32p4-temp` | `HalTempSensor` | SENSOR | Internal | — | — |
| `alx,signal-gen` | `HalSigGen` | ADC | Internal | — | `ADC_PATH` |
| `alx,usb-audio` | `HalUsbAudio` | ADC | USB OTG | — | `ADC_PATH` |
| `microchip,mcp4725` | `HalMcp4725` | DAC | I2C | 0x60 | — |
| `ess,es9822pro` | `HalEs9822pro` | ADC | I2C Bus 2 | 0x40 | `ADC_PATH`, `HW_VOLUME`, `PGA_CONTROL`, `HPF_CONTROL` |
| `ess,es9843pro` | `HalEs9843pro` | ADC | I2C Bus 2 | 0x40 | `ADC_PATH`, `HW_VOLUME`, `PGA_CONTROL`, `HPF_CONTROL` |
| `ess,es9826` | `HalEs9826` | ADC | I2C Bus 2 | 0x40 | `ADC_PATH`, `HW_VOLUME`, `PGA_CONTROL`, `HPF_CONTROL` |
| `ess,es9823pro` | `HalEs9823pro` | ADC | I2C Bus 2 | 0x40 | `ADC_PATH`, `HW_VOLUME`, `PGA_CONTROL`, `HPF_CONTROL` |
| `ess,es9821` | `HalEs9821` | ADC | I2C Bus 2 | 0x40 | `ADC_PATH`, `HW_VOLUME`, `HPF_CONTROL` |
| `ess,es9820` | `HalEs9820` | ADC | I2C Bus 2 | 0x40 | `ADC_PATH`, `HW_VOLUME`, `PGA_CONTROL`, `HPF_CONTROL` |
| `ess,es9842pro` | `HalEs9842pro` | ADC | I2C Bus 2 | 0x40 | `ADC_PATH`, `HW_VOLUME`, `PGA_CONTROL`, `HPF_CONTROL` |
| `ess,es9840` | `HalEs9840` | ADC | I2C Bus 2 | 0x40 | `ADC_PATH`, `HW_VOLUME`, `PGA_CONTROL`, `HPF_CONTROL` |
| `ess,es9841` | `HalEs9841` | ADC | I2C Bus 2 | 0x40 | `ADC_PATH`, `HW_VOLUME`, `PGA_CONTROL`, `HPF_CONTROL` |
| `ess,es9038q2m` | `HalEs9038q2m` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE` |
| `ess,es9039q2m` | `HalEs9039q2m` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE` |
| `ess,es9069q` | `HalEs9069Q` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE`, `MQA` |
| `ess,es9033q` | `HalEs9033Q` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE`, `LINE_DRIVER` |
| `ess,es9020-dac` | `HalEs9020Dac` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE`, `APLL` |
| `ess,es9038pro` | `HalEs9038pro` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE` |
| `ess,es9028pro` | `HalEs9028pro` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE` |
| `ess,es9039pro` | `HalEs9039pro` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE` |
| `ess,es9039mpro` | `HalEs9039pro` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE` |
| `ess,es9027pro` | `HalEs9027pro` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE` |
| `ess,es9081` | `HalEs9081` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE` |
| `ess,es9082` | `HalEs9082` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE` |
| `ess,es9017` | `HalEs9017` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE` |
| `cirrus,cs43198` | `HalCs43198` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE`, `DSD` |
| `cirrus,cs43131` | `HalCs43131` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE`, `DSD`, `HP_AMP` |
| `cirrus,cs4398` | `HalCs4398` | DAC | I2C Bus 2 | 0x4C | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE`, `DSD` |
| `cirrus,cs4399` | `HalCs4399` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE` |
| `cirrus,cs43130` | `HalCs43130` | DAC | I2C Bus 2 | 0x48 | `DAC_PATH`, `HW_VOLUME`, `FILTERS`, `MUTE`, `DSD`, `HP_AMP` |
| `generic,relay-amp` | `HalRelay` | AMP | GPIO | — | — |
| `alx,dsp-pipeline` | `HalDspBridge` | DSP | Internal | — | — |
| `generic,piezo-buzzer` | `HalBuzzer` | GPIO | GPIO | — | — |
| `generic,tact-switch` | `HalButton` | INPUT | GPIO | — | — |
| `generic,status-led` | `HalLed` | GPIO | GPIO | — | — |
| `alps,ec11` | `HalEncoder` | INPUT | GPIO | — | — |

---

## Audio Output Drivers

### PCM5102A — `ti,pcm5102a`

**Class:** `HalPcm5102a`
**Type:** `HAL_DEV_DAC`
**Bus:** I2S (I2S_NUM_0, TX only)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

The PCM5102A is a passive stereo I2S DAC with no I2C control interface. All communication is via the I2S data stream. `probe()` always returns `true` because there is nothing to interrogate on the bus. Volume is controlled via the XSMT (soft mute) pin if `paControlPin` is configured in `HalDeviceConfig`.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_MUTE | HAL_CAP_HW_VOLUME`

**Interfaces implemented:** `HalAudioDevice`, `HalAudioDacInterface`

**Relevant config fields:**

| Field | Default | Notes |
|---|---|---|
| `i2sPort` | 0 | I2S peripheral number |
| `pinBck` | `config.h` default | Bit clock GPIO |
| `pinLrc` | `config.h` default | Word select GPIO |
| `pinData` | 24 | I2S TX data GPIO |
| `sampleRate` | 48000 | Preferred sample rate |
| `bitDepth` | 32 | I2S frame width |
| `paControlPin` | -1 | XSMT/mute GPIO, -1 = not connected |
| `isI2sClockMaster` | `false` | Set `true` to output BCK/WS clocks |

```cpp
// Reading PCM5102A from the bridge — use isReady() accessor, not raw _ready
HalDevice* dev = HalDeviceManager::instance().findByCompatible("ti,pcm5102a");
if (dev && dev->isReady()) {
    auto* dac = static_cast<HalAudioDacInterface*>(static_cast<HalPcm5102a*>(dev));
    dac->dacSetVolume(75);
}
```

PCM5102A discovery no longer uses a hardcoded slot lookup. The HAL device manager now locates the device by capability scan (`HAL_CAP_DAC_PATH`) rather than by a fixed slot index, so any `HAL_CAP_DAC_PATH` device at any slot can be returned.

**healthCheck():** Always returns `true` — no I2C register to probe. The audio pipeline's VU meter provides runtime signal health.

---

### ES8311 — `everest-semi,es8311`

**Class:** `HalEs8311`
**Type:** `HAL_DEV_CODEC`
**Bus:** I2C (Bus 1, ONBOARD) + I2S2 (TX)
**I2C Address:** 0x18 (fixed on the Waveshare board)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

The ES8311 is the onboard codec on the ESP32-P4 board. It provides a high-quality stereo DAC output via I2S2 and a mono ADC input with programmable gain and high-pass filter. The codec is controlled over I2C Bus 1 (GPIO 7/8), which is dedicated to this device and always safe to access regardless of WiFi state.

A legacy compatible alias `evergrande,es8311` is registered as a second registry entry pointing to the same factory function, for backward compatibility with EEPROM-programmed modules that used the old vendor string.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_ADC_PATH | HAL_CAP_CODEC | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL`

**Interfaces implemented:** `HalAudioDevice`, `HalAudioCodecInterface` (which extends both `HalAudioDacInterface` and `HalAudioAdcInterface`)

**Relevant config fields:**

| Field | Default | Notes |
|---|---|---|
| `i2cBusIndex` | 1 | ONBOARD bus; do not change |
| `i2cAddr` | 0x18 | Fixed; I2C address pins are hardwired |
| `i2sPort` | 2 | I2S2 TX |
| `sampleRate` | 48000 | |
| `bitDepth` | 16 | |
| `mclkMultiple` | 256 | MCLK = sampleRate × mclkMultiple |
| `i2sFormat` | 0 | 0=Philips, 1=MSB left-justified, 2=RSB |
| `paControlPin` | 53 | NS4150B power amp enable (shared with NS4150B driver) |
| `pgaGain` | 0 | ADC PGA gain in dB (0–23) |
| `hpfEnabled` | `false` | ADC high-pass filter |
| `volume` | 0 | Initial DAC output volume 0–100 |

```cpp
// Enable ADC high-pass filter
HalDevice* dev = HalDeviceManager::instance().findByCompatible("everest-semi,es8311");
if (dev) {
    auto* codec = static_cast<HalAudioCodecInterface*>(static_cast<HalEs8311*>(dev));
    codec->adcSetHpfEnabled(true);
    codec->adcSetGain(12);  // 12 dB PGA
}
```

**probe():** Sends an I2C address byte to 0x18 on Bus 1 and checks for ACK. Also reads the chip ID register (0x00) and verifies it returns the expected device ID.

**healthCheck():** Reads register 0x00 over I2C. A NACK indicates the codec has lost power or its I2C bus has been corrupted.

---

## Audio Input Drivers

### PCM1808 — `ti,pcm1808`

**Class:** `HalPcm1808`
**Type:** `HAL_DEV_ADC`
**Bus:** I2S (RX, no I2C control)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

The PCM1808 is a stereo audio ADC. Two instances are registered as builtin devices, giving the system eight input channels (two 4-channel FIFO reads per ADC per DMA callback). Like the PCM5102A it has no I2C interface; format selection uses the FMT0/FMT1 GPIO pins.

Both instances share BCK, LRCK, and MCLK clock lines from I2S_NUM_0. I2S_NUM_1 (the second PCM1808) is configured as a master with no clock output — only the DATA input pin is wired. Both use identical divider chains so their clocks are frequency-locked.

**Capabilities:** `HAL_CAP_ADC_PATH`

**Interfaces implemented:** `HalAudioDevice`, `HalAudioAdcInterface`

**Implements `getInputSource()`:** Yes — returns an `AudioInputSource` descriptor that `hal_pipeline_bridge` copies into the audio pipeline's input lane registration.

**Relevant config fields:**

| Field | Default | Notes |
|---|---|---|
| `i2sPort` | 0 / 1 | First PCM1808 uses port 0, second uses port 1 |
| `pinBck` | 20 | Shared between both instances |
| `pinLrc` | 21 | Shared between both instances |
| `pinMclk` | 22 | Shared between both instances |
| `pinData` | 23 / 25 | ADC1 DOUT on GPIO23, ADC2 DOUT on GPIO25 |
| `sampleRate` | 48000 | |
| `isI2sClockMaster` | `false` (ADC2) | ADC1 outputs clocks; ADC2 receives them |

**probe():** Always returns `true` — there is no I2C register to check.

**healthCheck():** Always returns `true`. The pipeline's noise gate and VU metrics detect loss-of-signal conditions without requiring a bus transaction.

:::note Dual-master I2S
Both PCM1808 instances run as I2S master but only the first one outputs clocks. This is a confirmed-working workaround for an IDF5 I2S slave driver bug where `bclk_div` is always calculated as 4 (below the hardware minimum of 8).
:::

---

### Signal Generator — `alx,signal-gen`

**Class:** `HalSigGen`
**Type:** `HAL_DEV_ADC` (pipeline input lane)
**Bus:** Internal (software source)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

The signal generator is a software audio source that injects synthesised waveforms (sine, square, white noise, swept sine) directly into the audio pipeline. It is typed as `HAL_DEV_ADC` so the pipeline bridge assigns it an input lane exactly like a hardware ADC.

`probe()` and `healthCheck()` both return `true` unconditionally — the signal generator is always available as long as the firmware is running.

**Capabilities:** `HAL_CAP_ADC_PATH`

**Implements `getInputSource()`:** Yes — the returned `AudioInputSource` has `isHardwareAdc = false`, which tells the pipeline to skip the noise gate for this lane.

**GPIO output mode:** In addition to the software pipeline injection, the signal generator can drive a PWM output on GPIO 47 for speaker-level stimulus without requiring a DAC. This mode is independent of the HAL registration.

---

### USB Audio — `alx,usb-audio`

**Class:** `HalUsbAudio`
**Type:** `HAL_DEV_ADC` (pipeline input lane)
**Bus:** USB OTG (TinyUSB UAC2)
**Guard:** `#ifdef USB_AUDIO_ENABLED`
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

The USB Audio device presents the controller as a UAC2 USB speaker to the host computer. Received audio is fed into the pipeline via a SPSC lock-free ring buffer (1024 frames, PSRAM-backed). The USB Product ID defaults to 0x4004 and can be overridden in `HalDeviceConfig.usbPid`.

`probe()` returns `true` unconditionally; the USB connection state is tracked internally and reported via the `audioChannelMap` WebSocket broadcast.

**Capabilities:** `HAL_CAP_ADC_PATH`

**Implements `getInputSource()`:** Yes — `isHardwareAdc = false` (no noise gate).

**Relevant config fields:**

| Field | Default | Notes |
|---|---|---|
| `usbPid` | 0x4004 | USB Product ID shown to the host |
| `enabled` | `true` | Setting to `false` prevents TinyUSB initialisation |

:::note Build flag required
USB Audio requires `-D USB_AUDIO_ENABLED` in `platformio.ini` and `build_unflags = -DARDUINO_USB_MODE -DARDUINO_USB_CDC_ON_BOOT`. It also requires `build_flags = -D USB_AUDIO_ENABLED` to be present; the driver class and factory function are compiled out by `#ifdef USB_AUDIO_ENABLED` guards.
:::

---

## Expansion ADC Drivers (ESS SABRE Family)

Nine ESS Technology SABRE ADC expansion drivers are registered in `hal_builtin_devices.cpp`. They connect to the carrier board via the mezzanine connector on I2C Bus 2 (GPIO 28/29) at I2C address 0x40 (default, ADDR1=LOW, ADDR2=LOW). Only one expansion ADC module is physically installed at a time — the HAL discovery layer reads the chip ID register (0xE1) via the AT24C02 EEPROM's compatible string to select the correct driver automatically.

All nine drivers share a common base class `HalEssSabreAdcBase` (in `src/hal/hal_ess_sabre_adc_base.h`) that provides:

- `_writeReg(reg, val)` / `_readReg(reg)` / `_writeReg16(regLsb, val)` — I2C register helpers
- `_selectWire()` — Wire instance selection based on `_i2cBusIndex`
- `_applyConfigOverrides()` — reads `HalDeviceConfig` into member fields at the start of `init()`
- `_validateSampleRate(hz, supported[], count)` — validates against a device-specific rate table

Shared constants across the family are defined in `src/drivers/ess_sabre_common.h`. Per-device register maps live in `src/drivers/es98xx_regs.h` files.

All ESS SABRE ADC devices share the same 8 digital filter presets (ordinals 0–7):

| Ordinal | Filter Shape |
|---|---|
| 0 | Minimum Phase |
| 1 | Linear Apodizing Fast |
| 2 | Linear Fast |
| 3 | Linear Fast Low Ripple |
| 4 | Linear Slow |
| 5 | Minimum Fast |
| 6 | Minimum Slow |
| 7 | Minimum Slow Low Dispersion |

---

### Pattern A: 2-Channel I2S Devices

These devices output stereo audio via standard I2S on the mezzanine DIN pin (pin 10 of the connector). Each registers one `AudioInputSource` via `getInputSource()`. The pipeline bridge assigns a single input lane when the device transitions to AVAILABLE.

---

#### ES9822PRO — `ess,es9822pro`

**Class:** `HalEs9822pro`
**Type:** `HAL_DEV_ADC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave RX
**I2C Address:** 0x40
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel 32-bit SABRE ADC with I2C register control. Features per-channel 16-bit digital volume, programmable gain amplifier (0–18 dB in 6 dB steps), high-pass filter, and 8 digital filter presets.

**Capabilities:** `HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `pgaGain` | 0 | PGA gain in dB: 0, 6, 12, or 18 |
| `hpfEnabled` | `true` | High-pass filter enable |
| `volume` | 100 | Master volume 0–100 (maps to 16-bit register) |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7), `setChannelVolume(uint8_t ch, uint16_t vol16)` (per-channel 16-bit volume).

**probe():** Reads chip ID register 0xE1 and verifies it matches the ES9822PRO chip ID.

**healthCheck():** I2C ACK check. A NACK after init indicates the module has been disconnected.

---

#### ES9826 — `ess,es9826`

**Class:** `HalEs9826`
**Type:** `HAL_DEV_ADC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave RX
**I2C Address:** 0x40
**Chip ID:** 0x8A (register 0xE1)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel SABRE ADC with a wider PGA gain range than the ES9822PRO (0–30 dB in 3 dB steps). Gain is nibble-packed in a single register (CH2 bits\[7:4\], CH1 bits\[3:0\]). Per-channel 16-bit volume. 8 filter presets.

Note: `adcSetHpfEnabled()` stores the flag for UI state but does not write a dedicated hardware register — the ES9826 does not expose a separate HPF control register.

**Capabilities:** `HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `pgaGain` | 0 | PGA gain in dB: 0–30 in 3 dB steps |
| `volume` | 100 | Master volume 0–100 (maps to 16-bit register) |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7), `setChannelVolume(uint8_t ch, uint16_t vol16)`.

---

#### ES9823PRO / ES9823MPRO — `ess,es9823pro` / `ess,es9823mpro`

**Class:** `HalEs9823pro`
**Type:** `HAL_DEV_ADC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave RX
**I2C Address:** 0x40
**Chip IDs:** 0x8D (ES9823PRO), 0x8C (ES9823MPRO)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

Highest-specification 2-channel SABRE ADC, handling both the ES9823PRO and ES9823MPRO package variants in a single driver. The chip ID register (0xE1) is read during `init()` to identify which variant is present — `_isMonolithic = true` is set for the ES9823MPRO (chip ID 0x8C). Both variants share the same register interface.

Features per-channel 16-bit digital volume, PGA gain 0–42 dB in 6 dB steps (3-bit register, values 0–7), and 8 digital filter presets. `adcSetHpfEnabled()` stores the state flag but does not write a separate HPF register on this device.

Both compatible strings (`ess,es9823pro` and `ess,es9823mpro`) are registered in `hal_register_builtins()` pointing to the same `factory_es9823pro` factory function.

**Capabilities:** `HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `pgaGain` | 0 | PGA gain in dB: 0, 6, 12, 18, 24, 30, 36, or 42 |
| `volume` | 100 | Master volume 0–100 (maps to 16-bit register) |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7), `setChannelVolume(uint8_t ch, uint16_t vol16)`.

---

#### ES9821 — `ess,es9821`

**Class:** `HalEs9821`
**Type:** `HAL_DEV_ADC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave RX
**I2C Address:** 0x40
**Chip ID:** 0x88 (register 0xE1)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel SABRE ADC with no hardware PGA. `adcSetGain(0)` is accepted; any non-zero gain value returns `false`. Per-channel 16-bit volume and 8 digital filter presets are supported. `adcSetHpfEnabled()` stores the state flag but is a no-op on hardware (no dedicated HPF register on this device).

**Capabilities:** `HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_HPF_CONTROL`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 (maps to 16-bit register) |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7), `setChannelVolume(uint8_t ch, uint16_t vol16)`.

---

#### ES9820 — `ess,es9820`

**Class:** `HalEs9820`
**Type:** `HAL_DEV_ADC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave RX
**I2C Address:** 0x40
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

Entry-tier 2-channel SABRE ADC with per-channel 16-bit digital volume, 2-bit DATA_GAIN register (0–18 dB in 6 dB steps: 0, 6, 12, 18 dB), HPF enable, and 8 per-channel filter presets. The filter preset is applied simultaneously to both channels.

**Capabilities:** `HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `pgaGain` | 0 | PGA gain in dB: 0, 6, 12, or 18 |
| `hpfEnabled` | `true` | High-pass filter enable |
| `volume` | 100 | Master volume 0–100 (maps to 16-bit register) |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7, applied to both channels), `setChannelVolume(uint8_t ch, uint16_t vol16)`.

---

### Pattern B: 4-Channel TDM Devices

These devices output 4 audio channels time-multiplexed on a single I2S DATA line in TDM mode. Each frame consists of 4 consecutive 32-bit slots: \[SLOT0=CH1\]\[SLOT1=CH2\]\[SLOT2=CH3\]\[SLOT3=CH4\].

Each TDM driver registers **two** `AudioInputSource` entries with the pipeline bridge:

- Source index 0: CH1/CH2 (stereo pair from SLOT0+SLOT1)
- Source index 1: CH3/CH4 (stereo pair from SLOT2+SLOT3)

The bridge discovers both sources via `getInputSourceCount()` (returns 2 when initialized) and `getInputSourceAt(idx)`. Consecutive input lanes are allocated for the two stereo pairs.

Frame splitting is handled by `HalTdmDeinterleaver` embedded in each driver — see the [TDM Deinterleaver](#tdm-deinterleaver) subsection below.

---

#### ES9843PRO — `ess,es9843pro`

**Class:** `HalEs9843pro`
**Type:** `HAL_DEV_ADC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave RX (TDM)
**I2C Address:** 0x40
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

4-channel SABRE ADC with per-channel 8-bit volume, PGA gain 0–42 dB in 6 dB steps (3-bit, gain register 0–7), HPF per-channel, and a single global digital filter preset (0–7). TDM output. The ASP2 DSP block is kept disabled in this driver.

**Capabilities:** `HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `pgaGain` | 0 | PGA gain in dB: 0, 6, 12, 18, 24, 30, 36, or 42 |
| `hpfEnabled` | `true` | High-pass filter enable |
| `volume` | 100 | Master volume 0–100 (maps to 8-bit per-channel register) |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7 global), `setChannelVolume(uint8_t ch, uint8_t vol8)`.

**Registered sources:** "ES9843PRO CH1/2" and "ES9843PRO CH3/4".

---

#### ES9842PRO — `ess,es9842pro`

**Class:** `HalEs9842pro`
**Type:** `HAL_DEV_ADC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave RX (TDM)
**I2C Address:** 0x40
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

4-channel SABRE ADC with per-channel 16-bit volume, 2-bit gain per channel (0–18 dB in 6 dB steps), per-channel HPF (DC blocking), per-channel filter preset (0–7), and TDM output.

**Capabilities:** `HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `pgaGain` | 0 | PGA gain in dB: 0, 6, 12, or 18 |
| `hpfEnabled` | `true` | High-pass filter enable |
| `volume` | 100 | Master volume 0–100 (maps to 16-bit per-channel register) |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7, applied to all 4 channels), `setChannelVolume16(uint8_t ch, uint16_t vol)`.

**Registered sources:** "ES9842PRO CH1/2" and "ES9842PRO CH3/4".

---

#### ES9841 — `ess,es9841`

**Class:** `HalEs9841`
**Type:** `HAL_DEV_ADC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave RX (TDM)
**I2C Address:** 0x40
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

4-channel SABRE ADC with per-channel 8-bit volume, 3-bit gain per channel packed two-per-register (0–42 dB in 6 dB steps, values 0–7), per-channel HPF (DC blocking at same register offsets as ES9842PRO), and a single global filter preset (0–7). TDM output.

**Volume encoding difference vs ES9842PRO:** ES9842PRO uses 16-bit volume (0x7FFF = 0 dB, 0x0000 = mute). ES9841 uses 8-bit volume (0xFF = 0 dB, 0x00 = mute). `setVolume(percent)` maps 100% to 0xFF and 0% to 0x00 linearly.

**Capabilities:** `HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `pgaGain` | 0 | PGA gain in dB: 0, 6, 12, 18, 24, 30, 36, or 42 |
| `hpfEnabled` | `true` | High-pass filter enable |
| `volume` | 100 | Master volume 0–100 (maps to 8-bit per-channel register, 0xFF=0dB) |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7 global), `setChannelVolume(uint8_t ch, uint8_t vol8)`.

**Registered sources:** "ES9841 CH1/2" and "ES9841 CH3/4".

---

#### ES9840 — `ess,es9840`

**Class:** `HalEs9840`
**Type:** `HAL_DEV_ADC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave RX (TDM)
**I2C Address:** 0x40
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

Entry-tier 4-channel SABRE ADC. Architecturally identical to the ES9842PRO — same register map, same driver pattern — with a lower DNR specification (116 dB vs 122 dB). Per-channel 16-bit volume, 2-bit gain per channel (0–18 dB in 6 dB steps), per-channel HPF, per-channel filter preset (0–7), TDM output.

**Capabilities:** `HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `pgaGain` | 0 | PGA gain in dB: 0, 6, 12, or 18 |
| `hpfEnabled` | `true` | High-pass filter enable |
| `volume` | 100 | Master volume 0–100 (maps to 16-bit per-channel register) |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7, applied to all 4 channels), `setChannelVolume16(uint8_t ch, uint16_t vol)`.

**Registered sources:** "ES9840 CH1/2" and "ES9840 CH3/4".

---

### TDM Deinterleaver

**Class:** `HalTdmDeinterleaver`
**Header:** `src/hal/hal_tdm_deinterleaver.h`

All four TDM expansion devices (ES9843PRO, ES9842PRO, ES9841, ES9840) embed a `HalTdmDeinterleaver` instance. It splits 4-slot TDM frames into two stereo pairs for the audio pipeline.

**How it works:**

1. Pair A's read callback is called first by the pipeline (lower lane index). It reads the full 4-slot TDM DMA buffer from I2S, deinterleaves CH1/CH2 into one ping-pong buffer side and CH3/CH4 into the other, then atomically swaps the write index.
2. Pair B's read callback is called second. It reads from the side that pair A just finished writing — no DMA transaction needed.
3. The ping-pong swap is a single `uint8_t` write, which is atomic on RISC-V. No mutex is required.

**Buffer allocation:** Each ping-pong side stores `TDM_MAX_FRAMES_PER_BUF` (128) stereo int32\_t pairs per channel pair. Total: 4 × 128 × 2 × 4 = 4096 bytes, allocated from PSRAM when available.

**Multi-instance support:** Up to 2 concurrent `HalTdmDeinterleaver` instances are supported, keyed by I2S port index. This allows two 4-channel TDM devices on separate I2S ports to operate simultaneously (though only one expansion ADC is physically installed at a time on the current hardware).

**API used by drivers:**

```cpp
// In driver's init():
if (!_tdm.init(i2sPort)) { return hal_init_fail(DIAG_ERR_ALLOC, "TDM buf alloc"); }
_tdm.buildSources("ES9843PRO CH1/2", "ES9843PRO CH3/4", &_srcA, &_srcB);

// In driver:
int getInputSourceCount() const override { return _initialized ? 2 : 0; }
const AudioInputSource* getInputSourceAt(int idx) const override;
```

---

## Expansion DAC Drivers (ESS SABRE Family)

Twelve ESS Technology SABRE DAC expansion drivers are registered in `hal_builtin_devices.cpp`. They connect to the carrier board via the mezzanine connector on I2C Bus 2 (GPIO 28/29). DAC devices use I2C addresses in the 0x48–0x4E range (distinct from the ADC 0x40–0x46 range), which permits a DAC and an ADC mezzanine to coexist on the same bus when the hardware supports dual module population.

All twelve drivers share the common base class `HalEssSabreDacBase` (in `src/hal/hal_ess_sabre_dac_base.h`) that provides:

- `_writeReg(reg, val)` / `_readReg(reg)` / `_writeReg16(regLsb, val)` — I2C register helpers
- `_selectWire()` — Wire instance selection based on `_i2cBusIndex`
- `_applyConfigOverrides()` — reads `HalDeviceConfig` into member fields at the start of `init()`
- `_enableI2sTx()` / `_disableI2sTx()` — expansion I2S TX lifecycle via `i2s_audio_enable_expansion_tx()` / `i2s_audio_disable_expansion_tx()`
- `dacSetSampleRate()`, `dacSetBitDepth()`, `dacGetSampleRate()` delegating through `configure()`
- `dacGetVolume()`, `dacIsMuted()`, hardware-mute ramp state (`_muteRampState`) for click-free transitions

Volume encoding for all ESS SABRE DAC devices uses an 8-bit attenuation register: 0x00 = 0 dB (full output), 0xFF = full mute, with 0.5 dB per step (128 effective steps). `setVolume(percent)` maps percent 0–100 linearly to this range.

All ESS SABRE DAC devices share the same 8 digital filter presets, though the ordinal-to-register mapping varies by device. Filter preset names for the DAC family:

| Ordinal | Filter Shape |
|---|---|
| 0 | Fast Roll-Off Linear Phase |
| 1 | Slow Roll-Off Linear Phase |
| 2 | Minimum Phase Fast Roll-Off |
| 3 | Minimum Phase Slow Roll-Off |
| 4 | Apodizing Fast Roll-Off Linear Phase |
| 5 | Corrected Minimum Phase Fast Roll-Off |
| 6 | Brick Wall |
| 7 | HB2 |

Shared family constants (I2C addresses, filter ordinals, volume constants) are defined in `src/drivers/ess_sabre_common.h`. Per-device register maps live in `src/drivers/es9XXX_regs.h` files.

---

### Pattern C: 2-Channel I2S DAC Devices

These devices receive stereo audio via standard I2S on the mezzanine DOUT pin (pin 11 of the connector). The ESP32-P4 drives BCK, WS, and MCLK as I2S master; the DAC operates as slave. Each Pattern C device registers a single `AudioOutputSink` via `buildSink()`. The pipeline bridge assigns one output slot when the device transitions to AVAILABLE.

---

#### ES9038Q2M — `ess,es9038q2m`

**Class:** `HalEs9038q2m`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX
**I2C Address:** 0x48
**Chip ID:** 0x90 (register 0xE1)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel 32-bit SABRE DAC with Hyperstream II architecture (128 dB DNR). PCM input up to 768 kHz, DSD512. I2C-controlled volume (8-bit, 0.5 dB/step), 8 digital filter presets, clock gear register for 384 kHz and 768 kHz operation (divides MCLK by 2x or 4x). Full duplex I2S slave to ESP32-P4.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 (maps to 8-bit attenuation register) |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7).

**probe():** Reads chip ID register 0xE1 and verifies it matches 0x90.

**healthCheck():** I2C ACK check. A NACK after init indicates the module has been disconnected.

---

#### ES9039Q2M — `ess,es9039q2m`

**Class:** `HalEs9039q2m`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel 32-bit SABRE DAC with Hyperstream IV architecture (130 dB DNR) — the newest modulator generation available in a 2-channel package. PCM input up to 768 kHz, DSD1024. Same I2S and I2C interface as ES9038Q2M; filter presets 6 and 7 are Hyperstream IV hybrid modes not present on earlier devices.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7).

---

#### ES9069Q — `ess,es9069q`

**Class:** `HalEs9069Q`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel 32-bit SABRE DAC with integrated MQA hardware renderer. DSD1024 capable. The MQA renderer is controlled via a dedicated I2C register (0x17). Setting `setMqaEnabled(true)` activates the silicon MQA unfold; `isMqaActive()` polls the MQA decode status register to report whether a valid MQA stream is currently being rendered.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_MQA`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7), `setMqaEnabled(bool)`, `isMqaActive() const`.

---

#### ES9033Q — `ess,es9033q`

**Class:** `HalEs9033Q`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel 32-bit SABRE DAC with integrated 2 Vrms ground-centered line-level output drivers. The on-chip output stage eliminates the external op-amp output buffers typically required in discrete designs, reducing BOM cost and PCB area on compact mezzanine modules. Supports PCM up to 768 kHz. `setLineDriverEnabled(false)` disables the integrated output stage for designs that prefer an external output buffer.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_LINE_DRIVER`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7), `setLineDriverEnabled(bool)`.

---

#### ES9020 — `ess,es9020-dac`

**Class:** `HalEs9020Dac`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel 32-bit SABRE DAC (Hyperstream IV, 122 dB DNR) with an integrated asynchronous PLL (APLL) for clock recovery directly from the I2S BCK signal. When APLL is enabled, the device switches its internal clock source to BCK recovery mode (`REG_CLK_SOURCE = 0x00`), providing jitter-immune operation without requiring a precision external MCLK oscillator. `isApllLocked()` polls the APLL lock status register — the hardware locks within approximately 1 ms of enable. Flexible TDM support (2/4/8/16 slots) is available via `REG_INPUT_CONFIG` but is not exposed in the standard driver interface.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_APLL`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7), `setApllEnabled(bool)`, `isApllLocked() const`.

---

### Pattern D: 8-Channel TDM DAC Devices

These devices receive all 8 channels time-multiplexed on the DOUT line in 8-slot TDM mode. Each frame consists of 8 consecutive 32-bit slots: \[SLOT0=CH1\]\[SLOT1=CH2\]\[SLOT2=CH3\]\[SLOT3=CH4\]\[SLOT4=CH5\]\[SLOT5=CH6\]\[SLOT6=CH7\]\[SLOT7=CH8\].

Each Pattern D driver registers **four** `AudioOutputSink` entries with the pipeline bridge:

- Sink index 0: CH1/CH2 (SLOT0+SLOT1)
- Sink index 1: CH3/CH4 (SLOT2+SLOT3)
- Sink index 2: CH5/CH6 (SLOT4+SLOT5)
- Sink index 3: CH7/CH8 (SLOT6+SLOT7)

The bridge discovers all four sinks via `getSinkCount()` (returns 4 when initialized) and `buildSinkAt(idx, sinkSlot, out)`. Consecutive output slots are allocated for the four stereo pairs, so an 8-channel TDM DAC occupies four output slots in the routing matrix.

Frame assembly is handled by `HalTdmInterleaver` embedded in each driver — see the [TDM Interleaver](#tdm-interleaver) subsection below.

---

#### ES9038PRO — `ess,es9038pro`

**Class:** `HalEs9038pro`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX (TDM)
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

8-channel 32-bit SABRE DAC with Hyperstream II architecture (132 dB DNR). PCM up to 768 kHz, DSD512. Per-channel 8-bit volume (0.5 dB/step), global mute, 8 digital filter presets. Flagship of the HyperStream II generation, widely used in high-end DAC hardware.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 (applied to all 8 channels) |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–7 global).

**Registered sinks:** "ES9038PRO CH1/2", "ES9038PRO CH3/4", "ES9038PRO CH5/6", "ES9038PRO CH7/8".

**probe():** Reads chip ID register 0xE1 and verifies the expected ES9038PRO chip ID.

**healthCheck():** I2C ACK check.

---

#### ES9028PRO — `ess,es9028pro`

**Class:** `HalEs9028pro`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX (TDM)
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

8-channel 32-bit SABRE DAC with Hyperstream II architecture (124 dB DNR, lower tier than ES9038PRO). PCM up to 768 kHz, DSD512. Register-compatible with ES9038PRO — same init sequence and TDM channel map. Primary differentiation is the modulator tier DNR specification.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Registered sinks:** "ES9028PRO CH1/2", "ES9028PRO CH3/4", "ES9028PRO CH5/6", "ES9028PRO CH7/8".

---

#### ES9039PRO / ES9039MPRO — `ess,es9039pro` / `ess,es9039mpro`

**Class:** `HalEs9039pro`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX (TDM)
**I2C Address:** 0x48
**Chip IDs:** 0x39 (ES9039PRO), 0x3A (ES9039MPRO)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

8-channel 32-bit SABRE DAC with Hyperstream IV architecture (132 dB DNR) — the highest-specification 8-channel device. PCM up to 768 kHz, DSD1024. Handles both the ES9039PRO and ES9039MPRO package variants in a single driver. The chip ID register (0xE1) is read during `init()` to identify which variant is present; the device descriptor name is updated to reflect "ES9039MPRO" when the MPRO chip ID (0x3A) is detected, and the `_isMpro` flag is set internally.

Both compatible strings (`ess,es9039pro` and `ess,es9039mpro`) are registered pointing to the same factory function.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Registered sinks:** "ES9039PRO CH1/2" (or "ES9039MPRO CH1/2" if MPRO detected), "ES9039PRO CH3/4", "ES9039PRO CH5/6", "ES9039PRO CH7/8".

---

#### ES9027PRO — `ess,es9027pro`

**Class:** `HalEs9027pro`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX (TDM)
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

8-channel 32-bit SABRE DAC with Hyperstream IV architecture (124 dB DNR). PCM up to 768 kHz, DSD1024. The ES9027PRO relates to the ES9039PRO as the ES9028PRO relates to the ES9038PRO — same modulator generation, lower DNR tier. Register map is shared with ES9039PRO, making the two drivers structurally identical except for the chip ID check.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Registered sinks:** "ES9027PRO CH1/2", "ES9027PRO CH3/4", "ES9027PRO CH5/6", "ES9027PRO CH7/8".

---

#### ES9081 — `ess,es9081`

**Class:** `HalEs9081`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX (TDM)
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

8-channel 32-bit SABRE DAC with Hyperstream IV architecture (120 dB DNR), 40-pin QFN package. Cost-optimised entry point for 8-channel Hyperstream IV designs. Pin-compatible with ES9080Q for drop-in upgrade paths on mezzanine layouts originally designed for the older device. Target use: home theatre receivers and AVR applications.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Registered sinks:** "ES9081 CH1/2", "ES9081 CH3/4", "ES9081 CH5/6", "ES9081 CH7/8".

---

#### ES9082 — `ess,es9082`

**Class:** `HalEs9082`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX (TDM)
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

8-channel 32-bit SABRE DAC with Hyperstream IV architecture (120 dB DNR), 48-pin QFN package. Larger package than ES9081 — the additional pins expose supplementary GPIOs and an optional ASP2 user-programmable DSP interface. The ASP2 block is not exposed in the current driver implementation; only the core TDM audio path and volume control are used.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Registered sinks:** "ES9082 CH1/2", "ES9082 CH3/4", "ES9082 CH5/6", "ES9082 CH7/8".

---

#### ES9017 — `ess,es9017`

**Class:** `HalEs9017`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX (TDM)
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

8-channel 32-bit SABRE DAC with Hyperstream IV architecture (120 dB DNR). Pin-compatible with ES9027PRO — intended as a direct drop-in replacement at lower cost. The register map is identical to ES9027PRO including the TDM channel map registers (0x40–0x47), so the same mezzanine PCB can be populated with either device.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–7 |

**Registered sinks:** "ES9017 CH1/2", "ES9017 CH3/4", "ES9017 CH5/6", "ES9017 CH7/8".

---

### TDM Interleaver

**Class:** `HalTdmInterleaver`
**Header:** `src/hal/hal_tdm_interleaver.h`

All seven 8-channel TDM DAC devices (ES9038PRO, ES9028PRO, ES9039PRO, ES9027PRO, ES9081, ES9082, ES9017) embed a `HalTdmInterleaver` instance. It combines four stereo pipeline output sinks into a single 8-slot TDM frame for delivery to the DAC.

**How it works:**

1. The audio pipeline task (Core 1) calls sink write callbacks in ascending slot order. The bridge registers pair 0 at the lowest slot index, pair 3 at the highest — so pairs are called in order 0, 1, 2, 3 within the same pipeline tick.
2. Pairs 0, 1, and 2 copy their stereo frame data into per-pair ping-pong buffers but do not flush.
3. Pair 3 copies its data, then interleaves all four pair buffers into the TDM output buffer (8 slots per frame) and calls `i2s_audio_write_expansion_tdm_tx()` to push to I2S DMA. The ping-pong write index is then swapped so the next tick uses the idle side.
4. The ping-pong swap is a single `uint8_t` write, which is atomic on RISC-V. No mutex is required.

**Buffer allocation:** TDM output buffer is `TDM_INTERLEAVER_FRAMES` (256) × 8 slots × 4 bytes = 8 192 bytes. Each per-pair ping-pong side is 256 × 2 × 4 = 2 048 bytes. All buffers are allocated from PSRAM when available via `psram_alloc()`.

**Multi-instance support:** Up to 2 concurrent `HalTdmInterleaver` instances are supported (keyed by I2S port index), allowing two 8-channel TDM DAC devices on separate I2S ports to operate simultaneously.

**API used by drivers:**

```cpp
// In driver's init():
if (!_tdm.init(i2sPort)) { return hal_init_fail(DIAG_ERR_ALLOC, "TDM buf alloc"); }
_tdm.buildSinks("ES9038PRO CH1/2", "ES9038PRO CH3/4",
                "ES9038PRO CH5/6", "ES9038PRO CH7/8",
                &_sinks[0], &_sinks[1], &_sinks[2], &_sinks[3],
                _slot);

// In driver:
int  getSinkCount() const override { return _sinksBuilt ? 4 : 0; }
bool buildSinkAt(int idx, uint8_t sinkSlot, AudioOutputSink* out) override;
```

---

## Expansion DAC Drivers (Cirrus Logic Family)

Five Cirrus Logic DAC expansion drivers are registered in `hal_builtin_devices.cpp`. They connect to the carrier board via the mezzanine connector on I2C Bus 2 (GPIO 28/29). CS43198, CS43131, CS4399, and CS43130 use I2C address 0x48 — the same base address range as ESS SABRE DACs. They coexist with ESS devices via different EEPROM IDs and compatible string matching rather than by address separation. CS4398 uses its hardware-fixed address 0x4C, which is distinct from both the ESS SABRE DAC range and the ADC 0x40 range.

All five drivers share the common base class `HalCirrusDacBase` (in `src/hal/hal_cirrus_dac_base.h`) that provides:

- Dual I2C addressing mode: 8-bit direct registers for legacy devices (CS4398) and 16-bit paged register access for modern devices (CS43198, CS43131, CS4399, CS43130)
- `_writeReg(reg, val)` / `_readReg(reg)` — I2C register helpers with automatic address width selection
- `_selectWire()` — Wire instance selection based on `_i2cBusIndex`
- `_applyConfigOverrides()` — reads `HalDeviceConfig` into member fields at the start of `init()`
- `_enableI2sTx()` / `_disableI2sTx()` — expansion I2S TX lifecycle via port-generic API
- `dacSetSampleRate()`, `dacSetBitDepth()`, `dacGetSampleRate()` delegating through `configure()`
- `dacGetVolume()`, `dacIsMuted()`, hardware-mute ramp state (`_muteRampState`) for click-free transitions

Shared family constants are defined in `src/drivers/cirrus_dac_common.h`. Per-device register maps live in `src/drivers/cs*_regs.h` files.

---

### Pattern C: 2-Channel I2S DAC Devices (Cirrus Logic)

These devices receive stereo audio via standard I2S on the mezzanine DOUT pin (pin 11 of the connector). The ESP32-P4 drives BCK, WS, and MCLK as I2S master; the DAC operates as slave. Each device registers a single `AudioOutputSink` via `buildSink()`. The pipeline bridge assigns one output slot when the device transitions to AVAILABLE.

---

#### CS43198 — `cirrus,cs43198`

**Class:** `HalCs43198`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel 32-bit SABRE-class DAC (130 dBA DNR) with PCM input up to 384 kHz and DSD256 support. Uses 16-bit paged I2C register access. I2C-controlled volume, 7 digital filter presets, and a low-power mode for battery-powered designs. Full duplex I2S slave to ESP32-P4.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_DSD`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–6 |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–6).

**probe():** Reads chip ID via paged register access and verifies the Cirrus Logic device ID.

**healthCheck():** I2C ACK check. A NACK after init indicates the module has been disconnected.

---

#### CS43131 — `cirrus,cs43131`

**Class:** `HalCs43131`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel 32-bit DAC (127 dB DNR) with an integrated high-performance headphone amplifier and DSD256 support. PCM input up to 384 kHz. The on-chip headphone amplifier eliminates the need for an external amplifier stage in portable and desktop headphone designs. Uses 16-bit paged I2C register access. 7 digital filter presets.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_DSD | HAL_CAP_HP_AMP`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–6 |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–6), `setHeadphoneAmpEnabled(bool)`, `isHeadphoneAmpEnabled() const`.

---

#### CS4398 — `cirrus,cs4398`

**Class:** `HalCs4398`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX
**I2C Address:** 0x4C
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel 24-bit DAC (120 dB DNR) with DSD64 support. PCM input up to 192 kHz. Uses legacy 8-bit direct I2C register access (no paged addressing). 3 digital filter presets. A proven, well-established converter widely used in CD player and standalone DAC designs.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_DSD`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–2 |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–2).

---

#### CS4399 — `cirrus,cs4399`

**Class:** `HalCs4399`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel 32-bit DAC (130 dBA DNR) with PCM input up to 384 kHz. Uses 16-bit paged I2C register access. 5 digital filter presets including a NOS (non-oversampling) mode that bypasses the internal digital interpolation filter for a time-domain-optimized impulse response preferred by some audiophile listeners.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–4 (4 = NOS mode) |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–4), `setNosMode(bool enable)`, `isNosMode() const`.

---

#### CS43130 — `cirrus,cs43130`

**Class:** `HalCs43130`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C Bus 2 (GPIO 28/29) + I2S slave TX
**I2C Address:** 0x48
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

2-channel 32-bit DAC (130 dB DNR) with an integrated headphone amplifier and DSD128 support. PCM input up to 384 kHz. Combines the headphone amplifier of the CS43131 with a NOS (non-oversampling) filter mode option. Uses 16-bit paged I2C register access. 5 digital filter presets including NOS mode.

**Capabilities:** `HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_DSD | HAL_CAP_HP_AMP`

**Config fields:**

| Field | Default | Notes |
|---|---|---|
| `volume` | 100 | Master volume 0–100 |
| `filterMode` | 0 | Digital filter preset 0–4 (4 = NOS mode) |

**Extension methods:** `setFilterPreset(uint8_t preset)` (0–4), `setHeadphoneAmpEnabled(bool)`, `isHeadphoneAmpEnabled() const`, `setNosMode(bool enable)`, `isNosMode() const`.

---

## Amplifier Drivers

### NS4150B — `ns,ns4150b-amp`

**Class:** `HalNs4150b`
**Type:** `HAL_DEV_AMP`
**Bus:** GPIO
**GPIO:** 53 (shared with ES8311 PA pin)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

The NS4150B is a mono class-D power amplifier. The HAL driver controls it via a single GPIO (the PA enable pin). The driver is automatically enabled and disabled by `hal_pipeline_bridge` based on DAC availability: when at least one DAC-path device has an active sink slot the amplifier is enabled; when all DAC sinks are removed it is disabled.

```cpp
HalNs4150b* amp = static_cast<HalNs4150b*>(
    HalDeviceManager::instance().findByCompatible("ns,ns4150b-amp"));
if (amp) {
    amp->setEnable(true);   // drives GPIO 53 HIGH
    bool on = amp->isEnabled();
}
```

**probe():** Checks that the GPIO pin number is valid (>= 0).

**healthCheck():** Returns `true` unconditionally — GPIO state is always readable.

:::caution Shared GPIO with ES8311
GPIO 53 is the PA enable pin for both the NS4150B amplifier and the ES8311 codec's `codecSetPaEnabled()` method. Both drivers reference this pin. If you reconfigure one, check whether the other is also affected.
:::

---

### Amplifier Relay — `generic,relay-amp`

**Class:** `HalRelay`
**Type:** `HAL_DEV_AMP`
**Bus:** GPIO
**Init Priority:** `HAL_PRIORITY_IO` (900)

The relay driver controls a GPIO-connected amplifier relay. Smart sensing routes through this driver via `findByCompatible("generic,relay-amp")` when available, falling back to direct GPIO control of the amplifier pin if no relay device is registered in the HAL.

**Capabilities:** None (GPIO control only)

**Key API:**

- `setEnabled(bool)` — Drive the relay GPIO high (on) or low (off)

**Relevant config fields:**

| Field | Default | Notes |
|---|---|---|
| `gpioA` | 27 | Relay control GPIO pin |

**probe():** Returns `true` if the configured GPIO pin number is valid.

**healthCheck():** Returns `true` unconditionally — GPIO state is always readable.

---

## Sensor Drivers

### ESP32-P4 Temperature Sensor — `espressif,esp32p4-temp`

**Class:** `HalTempSensor`
**Type:** `HAL_DEV_SENSOR`
**Bus:** Internal (IDF5 `driver/temperature_sensor.h`)
**Guard:** `#if CONFIG_IDF_TARGET_ESP32P4`
**Init Priority:** `HAL_PRIORITY_LATE` (100)

Reads the ESP32-P4 on-chip temperature sensor. Range is -10 to +80°C. The measurement is updated every 30 seconds via `healthCheck()` and published to MQTT and the web UI health dashboard.

```cpp
HalTempSensor* ts = static_cast<HalTempSensor*>(
    HalDeviceManager::instance().findByCompatible("espressif,esp32p4-temp"));
if (ts && ts->_ready) {
    float degC = ts->getTemperature();
}
```

The driver factory for this device is `nullptr` in the registry — it is registered by `hal_register_builtins()` using a direct `new HalTempSensor()` call rather than going through the registry factory, because the temperature sensor is an internal peripheral and cannot be discovered dynamically.

---

## Voltage Output

### MCP4725 — `microchip,mcp4725`

**Class:** `HalMcp4725`
**Type:** `HAL_DEV_DAC`
**Bus:** I2C (Bus 2, EXPANSION)
**I2C Address:** 0x60 (ADDR pin low) or 0x61 (ADDR pin high)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

The MCP4725 is a 12-bit single-channel I2C voltage output DAC. It is **not** an audio streaming device — it outputs a DC voltage proportional to a 12-bit code (0–4095), useful for controlling bias voltages, variable gain amplifiers, or automated test equipment.

```cpp
HalMcp4725* dac = static_cast<HalMcp4725*>(
    HalDeviceManager::instance().findByCompatible("microchip,mcp4725"));
if (dac && dac->_ready) {
    dac->setVolume(50);          // 50% of VCC
    dac->setVoltageCode(2048);   // mid-scale
}
```

**Relevant config fields:**

| Field | Default | Notes |
|---|---|---|
| `i2cAddr` | 0x60 | Override to 0x61 if ADDR pin is tied high |
| `i2cBusIndex` | 2 | Use expansion bus by default |

**probe():** I2C ACK check at the configured address.

**healthCheck():** I2C ACK check. A NACK after initial success indicates the module has been disconnected.

---

## DSP and Pipeline Drivers

### DSP Pipeline Bridge — `alx,dsp-pipeline`

**Class:** `HalDspBridge`
**Type:** `HAL_DEV_DSP`
**Bus:** Internal
**Init Priority:** `HAL_PRIORITY_DATA` (600)

The DSP bridge is a pseudo-device that connects the HAL device model to the `dsp_pipeline` module. It does not represent any physical hardware — its purpose is to expose DSP state (input/output levels, bypass flag) through the standard HAL interface so that the health dashboard and web UI can query it without importing `dsp_pipeline.h` directly.

**Interfaces implemented:** `HalAudioDspInterface`

```cpp
HalDspBridge* dsp = static_cast<HalDspBridge*>(
    HalDeviceManager::instance().findByCompatible("alx,dsp-pipeline"));
if (dsp) {
    float vu = dsp->dspGetInputLevel(0);   // VU of ADC lane 0
    dsp->dspSetBypassed(true);             // bypass all DSP processing
}
```

**probe():** Returns `true` unconditionally.

**healthCheck():** Returns `true` unconditionally — the DSP pipeline's internal state determines whether processing succeeds.

---

## Input / Control Drivers

### Piezo Buzzer — `generic,piezo-buzzer`

**Class:** `HalBuzzer`
**Type:** `HAL_DEV_GPIO`
**Bus:** GPIO (LEDC PWM)
**GPIO:** `BUZZER_PIN` (GPIO 45 by default, overridable via `gpioA`)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

Registers the piezo buzzer in the HAL device model so that its GPIO pin is claimed through the pin table and the buzzer's state appears in device listings. The actual tone generation and pattern sequencer live in `src/buzzer_handler.cpp` — `HalBuzzer` is a thin wrapper that initialises the LEDC channel and claims the pin.

**Relevant config fields:**

| Field | Default | Notes |
|---|---|---|
| `gpioA` | -1 | Buzzer pin override (-1 = use `BUZZER_PIN` from `config.h`) |

---

### Reset Button — `generic,tact-switch`

**Class:** `HalButton`
**Type:** `HAL_DEV_INPUT`
**Bus:** GPIO
**GPIO:** `RESET_BUTTON_PIN` (GPIO 46 by default, overridable via `gpioA`)
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

Registers the tactile reset button. Short press, long press, very-long press, and multi-click detection are handled by `src/button_handler.cpp`; `HalButton` claims the GPIO and provides the HAL device entry.

**Relevant config fields:**

| Field | Default | Notes |
|---|---|---|
| `gpioA` | -1 | Button pin override (-1 = use `RESET_BUTTON_PIN` from `config.h`) |

---

### Rotary Encoder — `alps,ec11`

**Class:** `HalEncoder`
**Type:** `HAL_DEV_INPUT`
**Bus:** GPIO (ISR-driven gray code)
**GPIOs:** `ENCODER_A_PIN` (32), `ENCODER_B_PIN` (33), `ENCODER_SW_PIN` (36) by default
**Init Priority:** `HAL_PRIORITY_HARDWARE` (800)

Registers the EC11-compatible rotary encoder used for GUI navigation. The ISR-driven gray code state machine and debounce logic live in `src/gui/gui_input.cpp`; `HalEncoder` claims the three GPIO pins and exposes them via `getPinA()`, `getPinB()`, `getPinSw()`.

**Relevant config fields:**

| Field | Default | Notes |
|---|---|---|
| `gpioA` | -1 | Channel A pin override |
| `gpioB` | -1 | Channel B pin override |
| `gpioC` | -1 | Switch pin override |

---

### Status LED — `generic,status-led`

**Class:** `HalLed`
**Type:** `HAL_DEV_GPIO`
**Bus:** GPIO
**Init Priority:** `HAL_PRIORITY_IO` (900)

Registers the onboard status LED in the HAL device model. The driver claims the LED GPIO pin and provides a `setOn(bool)` API for controlling the LED state. Has a wired factory function in `hal_builtin_devices.cpp`.

**Key API:**

- `setOn(bool state)` — Set the LED on or off

**Relevant config fields:**

| Field | Default | Notes |
|---|---|---|
| `gpioA` | 1 | LED GPIO pin |

**probe():** Returns `true` if the configured GPIO pin number is valid.

**healthCheck():** Returns `true` unconditionally.

---

## Capability Flags Reference

| Flag | Bit | Meaning |
|---|---|---|
| `HAL_CAP_HW_VOLUME` | 0 | Device supports hardware volume control |
| `HAL_CAP_FILTERS` | 1 | Device has on-chip EQ or filter capability |
| `HAL_CAP_MUTE` | 2 | Device has a hardware mute path |
| `HAL_CAP_ADC_PATH` | 3 | Device produces audio input (pipeline bridge assigns an input lane) |
| `HAL_CAP_DAC_PATH` | 4 | Device consumes audio output (pipeline bridge assigns a sink slot) |
| `HAL_CAP_PGA_CONTROL` | 5 | ADC gain is software-controllable |
| `HAL_CAP_HPF_CONTROL` | 6 | ADC high-pass filter is software-controllable |
| `HAL_CAP_CODEC` | 7 | Device has both ADC and DAC paths (combined codec) |
| `HAL_CAP_MQA` | 8 | MQA decoder support |
| `HAL_CAP_LINE_DRIVER` | 9 | Line driver outputs |
| `HAL_CAP_APLL` | 10 | Asynchronous PLL |
| `HAL_CAP_DSD` | 11 | DSD native playback |

:::note
The `capabilities` field is `uint16_t` (widened from `uint8_t` to accommodate bits 8-11).
:::

The pipeline bridge uses `HAL_CAP_DAC_PATH` and `HAL_CAP_ADC_PATH` exclusively to decide whether to assign a sink slot or input lane. Device type (`HAL_DEV_DAC`, `HAL_DEV_ADC`, etc.) is used only for UI display and type-based fallback when capabilities are zero.
