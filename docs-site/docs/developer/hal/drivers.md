---
title: Built-in Drivers
sidebar_position: 4
description: Reference for all built-in HAL drivers — PCM5102A, ES8311, PCM1808, NS4150B, MCP4725, and more.
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
| `alx,dsp-pipeline` | `HalDspBridge` | DSP | Internal | — | — |
| `generic,piezo-buzzer` | `HalBuzzer` | GPIO | GPIO | — | — |
| `generic,tact-switch` | `HalButton` | INPUT | GPIO | — | — |
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

The pipeline bridge uses `HAL_CAP_DAC_PATH` and `HAL_CAP_ADC_PATH` exclusively to decide whether to assign a sink slot or input lane. Device type (`HAL_DEV_DAC`, `HAL_DEV_ADC`, etc.) is used only for UI display and type-based fallback when capabilities are zero.
