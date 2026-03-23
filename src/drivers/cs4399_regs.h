#ifndef CS4399_REGS_H
#define CS4399_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR=LOW → 0x48 (default DAC base address)
// ADDR=HIGH → 0x49 (alternate address via ADDR pin)
#define CS4399_I2C_ADDR         0x48

// ===== Device ID =====
// Register 0x0001 (DEVID/REVID) contains the chip revision.
// The device ID byte reads 0x97 on CS4399 silicon.
#define CS4399_CHIP_ID          0x97

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D CS4399_I2C_SDA_PIN=N / -D CS4399_I2C_SCL_PIN=N.
#ifndef CS4399_I2C_SDA_PIN
#define CS4399_I2C_SDA_PIN      28
#endif
#ifndef CS4399_I2C_SCL_PIN
#define CS4399_I2C_SCL_PIN      29
#endif

// ===== 16-bit Paged Register Addresses =====
// The CS4399 uses a 2-byte register address (page:offset) transmitted
// MSB first over I2C.  All register accesses use _writeRegPaged() /
// _readRegPaged() rather than the 8-bit helpers used by ESS SABRE devices.

// ----- Device Identification -----
#define CS4399_REG_DEVID_REVID        0x0001  // [7:0] Device revision — reads CS4399_CHIP_ID

// ----- Power Control -----
// bit0 = PDN (1=power down, 0=power up); bit1 = RSVD; bits[7:2] = RSVD
#define CS4399_REG_POWER_CTL          0x0006  // Power-down control

// ----- Functional Mode -----
// bits[1:0] = MODE (0b00=HiFi, 0b01=DSP, 0b10=Reserved, 0b11=I2S slave default)
#define CS4399_REG_FUNC_MODE          0x0007  // Functional mode selection

// ----- Interface Control -----
// bits[2:0] = AUDIO_FORMAT (0=I2S, 1=LJ, 2=RJ, 3=TDM)
// bits[5:4] = WORD_LENGTH  (0=32-bit, 1=24-bit, 2=20-bit, 3=16-bit)
// bit7      = MASTER_MODE  (1=master, 0=slave; normally slave on expansion)
#define CS4399_REG_IFACE_CTL          0x0009  // Interface format and word length

// ----- PCM Signal Path (filter + mute) -----
// bits[2:0] = FILTER_SEL (0-4 digital filter presets; preset 4 = NOS mode)
// bit6      = MUTE_A     (1=mute left channel)
// bit7      = MUTE_B     (1=mute right channel)
#define CS4399_REG_PCM_PATH           0x0020  // PCM path: filter selection and per-channel mute

// ----- Volume Registers -----
// 0x00 = 0 dB (full volume), 0xFF = maximum attenuation (mute), 0.5 dB/step
#define CS4399_REG_VOL_A              0x001E  // Left channel attenuation (0x00=0dB, 0xFF=mute)
#define CS4399_REG_VOL_B              0x001F  // Right channel attenuation (0x00=0dB, 0xFF=mute)

// ----- Master Volume -----
// Controls both channels together; individual VOL_A/VOL_B add to this offset.
// 0x00 = 0 dB, 0xFF = maximum attenuation.
#define CS4399_REG_MASTER_VOL         0x001A  // Master volume control (both channels)

// ----- Clocking Control -----
// bits[2:0] = MCLK_FREQ selection
// bit3      = MCLK_DIS (1=disable MCLK input, use internal PLL)
// bits[6:4] = SPEED_MODE (0=normal ≤48kHz, 1=double ≤96kHz, 2=quad ≤192kHz, 3=oct ≤384kHz)
#define CS4399_REG_CLOCK_CTL          0x000C  // Clock and speed mode configuration

// ----- NOS Filter Control (CS4399-specific) -----
// bit0 = NOS_ENABLE (1=enable 512-element NOS filter, 0=standard oversampling)
// NOS (Non-OverSampling) mode bypasses the digital interpolation filter and routes
// the 512 single-bit element array directly to the analog output stage.
// Only meaningful when filter preset 4 (CS4399_FILTER_NOS_PRESET) is active.
#define CS4399_REG_NOS_CTL            0x001C  // NOS filter control
#define CS4399_NOS_ENABLE             0x01    // bit0: enable 512-element NOS filter
#define CS4399_NOS_DISABLE            0x00    // NOS disabled (standard oversampling)

// ===== Bit Masks =====

// CS4399_REG_POWER_CTL (0x0006)
#define CS4399_PDN_BIT                0x01    // bit0: 1=power down, 0=normal operation

// CS4399_REG_IFACE_CTL (0x0009) — AUDIO_FORMAT bits[2:0]
#define CS4399_FMT_I2S                0x00    // Standard I2S (Philips) format
#define CS4399_FMT_LJ                 0x01    // Left-justified format
#define CS4399_FMT_RJ                 0x02    // Right-justified format
#define CS4399_FMT_TDM                0x03    // TDM / DSP mode

// CS4399_REG_IFACE_CTL (0x0009) — WORD_LENGTH bits[5:4]
#define CS4399_WL_32BIT               0x00    // 32-bit word length
#define CS4399_WL_24BIT               0x10    // 24-bit word length
#define CS4399_WL_20BIT               0x20    // 20-bit word length
#define CS4399_WL_16BIT               0x30    // 16-bit word length

// CS4399_REG_PCM_PATH (0x0020) — mute bits
#define CS4399_MUTE_A_BIT             0x40    // bit6: mute left channel
#define CS4399_MUTE_B_BIT             0x80    // bit7: mute right channel
#define CS4399_MUTE_BOTH              (CS4399_MUTE_A_BIT | CS4399_MUTE_B_BIT)

// CS4399_REG_PCM_PATH (0x0020) — FILTER_SEL bits[2:0]
#define CS4399_FILTER_MASK            0x07    // Mask for filter preset field

// CS4399_REG_CLOCK_CTL (0x000C) — SPEED_MODE bits[6:4]
#define CS4399_SPEED_NORMAL           0x00    // Normal speed:  ≤48kHz
#define CS4399_SPEED_DOUBLE           0x10    // Double speed:  ≤96kHz
#define CS4399_SPEED_QUAD             0x20    // Quad speed:    ≤192kHz
#define CS4399_SPEED_OCTAL            0x30    // Octal speed:   ≤384kHz

// ===== Filter Preset Constants =====
// CS4399 provides 5 digital filter presets (indices 0-4).
// Preset 4 selects NOS mode — use setNosMode(true) to activate the NOS engine.
#define CS4399_FILTER_COUNT           5

#define CS4399_FILTER_FAST_LINEAR     0x00    // Fast roll-off, linear phase
#define CS4399_FILTER_SLOW_LINEAR     0x01    // Slow roll-off, linear phase
#define CS4399_FILTER_FAST_MINPHASE   0x02    // Fast roll-off, minimum phase
#define CS4399_FILTER_SLOW_MINPHASE   0x03    // Slow roll-off, minimum phase
#define CS4399_FILTER_NOS_PRESET      0x04    // NOS (Non-OverSampling) — 512 single-bit elements

// ===== Volume Constants =====
#define CS4399_VOL_0DB                0x00    // 0 dB (full volume, no attenuation)
#define CS4399_VOL_MUTE               0xFF    // Maximum attenuation (effective mute)

// ===== Init Config Values =====
// Default functional mode: I2S slave, 32-bit word length, normal speed
#define CS4399_IFACE_DEFAULT          (CS4399_FMT_I2S | CS4399_WL_32BIT)

#endif // CS4399_REGS_H
