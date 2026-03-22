---
title: Built-in Drivers
sidebar_position: 4
description: Reference for all built-in HAL drivers — PCM5102A, ES8311, PCM1808, NS4150B, MCP4725, ESS SABRE ADC expansion family, ESS SABRE DAC expansion family, amplifier relay, and more.
---

This page documents every driver registered by `hal_register_builtins()` in `src/hal/hal_builtin_devices.cpp`. Each entry covers the compatible string, device class, type, capabilities, bus requirements, and any runtime configuration options that affect behaviour.

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
// Reading PCM5102A from the bridge
HalDevice* dev = HalDeviceManager::instance().findByCompatible("ti,pcm5102a");
if (dev && dev->_ready) {
    auto* dac = static_cast<HalAudioDacInterface*>(static_cast<HalPcm5102a*>(dev));
    dac->dacSetVolume(75);
}
```

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
