#ifndef ES9841_REGS_H
#define ES9841_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR1=LOW, ADDR2=LOW → 0x40 (default)
// ADDR1=HIGH, ADDR2=LOW → 0x42
// ADDR1=LOW,  ADDR2=HIGH → 0x44
// ADDR1=HIGH, ADDR2=HIGH → 0x46
#define ES9841_I2C_ADDR           0x40

// ===== ADC Device ID (for EEPROM identification) =====
// Chip ID register 0xE1 always reads 0x91 on ES9841.
#define ES9841_CHIP_ID            0x91

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9841_I2C_SDA_PIN=N / -D ES9841_I2C_SCL_PIN=N.
#ifndef ES9841_I2C_SDA_PIN
#define ES9841_I2C_SDA_PIN        28
#endif
#ifndef ES9841_I2C_SCL_PIN
#define ES9841_I2C_SCL_PIN        29
#endif

// ===== Register Addresses =====

// ----- System / Output Select (0x00) -----
// bit[7]=SOFT_RESET, bits[6:5]=OUTPUT_SEL (0b10=TDM)
#define ES9841_REG_SYS_CONFIG             0x00

// ----- Filter (0x4A) -----
#define ES9841_REG_FILTER_CONFIG          0x4A  // bits[7:5]=FILTER_SHAPE (3-bit, 8 presets 0-7)

// ----- Volume Registers (0x51-0x54) — 8-bit per channel -----
// 0xFF=0dB (unity gain), 0x00=mute. Linear attenuation: 0xFF is full scale.
#define ES9841_REG_CH1_VOLUME             0x51  // CH1 8-bit volume (0xFF=0dB, 0x00=mute)
#define ES9841_REG_CH2_VOLUME             0x52  // CH2 8-bit volume
#define ES9841_REG_CH3_VOLUME             0x53  // CH3 8-bit volume
#define ES9841_REG_CH4_VOLUME             0x54  // CH4 8-bit volume

// ----- Gain Registers (0x55-0x56) — 3-bit packed per channel -----
#define ES9841_REG_GAIN_PAIR1             0x55  // bits[6:4]=CH2_DIGITAL_GAIN, bits[2:0]=CH1_DIGITAL_GAIN
#define ES9841_REG_GAIN_PAIR2             0x56  // bits[6:4]=CH4_DIGITAL_GAIN, bits[2:0]=CH3_DIGITAL_GAIN
// Gain values: 0=0dB, 1=6dB, 2=12dB, ..., 7=42dB (6 dB per step)

// ----- HPF / DC Blocking — same offsets as ES9842PRO -----
// Per-channel DC blocking at same register offsets as ES9842PRO/ES9843PRO
#define ES9841_REG_CH1_DC_BLOCKING        0x65  // bit[2]=CH1_DC_BLOCKING (HPF enable)
#define ES9841_REG_CH2_DC_BLOCKING        0x76  // bit[2]=CH2_DC_BLOCKING
#define ES9841_REG_CH3_DC_BLOCKING        0x87  // bit[2]=CH3_DC_BLOCKING
#define ES9841_REG_CH4_DC_BLOCKING        0x98  // bit[2]=CH4_DC_BLOCKING

// ----- Readback (0xE1) -----
#define ES9841_REG_CHIP_ID                0xE1  // Chip ID — reads 0x91 on ES9841

// ===== Bit Masks =====

// REG_SYS_CONFIG (0x00)
#define ES9841_SOFT_RESET_BIT             0x80  // Write 1 to reset; self-clearing
// OUTPUT_SEL bits[6:5]: 0b10 → TDM mode
#define ES9841_OUTPUT_TDM                 0x40  // bits[6:5] = 0b10

// DC_BLOCKING registers — shared bit position across all channels
#define ES9841_HPF_ENABLE_BIT             0x04  // bit[2] = DC_BLOCKING enable

// GAIN registers — 3-bit per channel, 6 dB steps (same encoding as ES9843PRO)
#define ES9841_GAIN_0DB                   0x00  // 0 dB
#define ES9841_GAIN_6DB                   0x01  // +6 dB
#define ES9841_GAIN_12DB                  0x02  // +12 dB
#define ES9841_GAIN_18DB                  0x03  // +18 dB
#define ES9841_GAIN_24DB                  0x04  // +24 dB
#define ES9841_GAIN_30DB                  0x05  // +30 dB
#define ES9841_GAIN_36DB                  0x06  // +36 dB
#define ES9841_GAIN_42DB                  0x07  // +42 dB

// FILTER_CONFIG (0x4A) bits[7:5] — FILTER_SHAPE (3-bit, 8 presets)
#define ES9841_FILTER_MASK                0xE0  // bits[7:5] mask
#define ES9841_FILTER_SHIFT               5     // shift to apply preset ordinal

// ===== Volume Constants (8-bit) =====
// ES9841 uses single 8-bit volume registers (0xFF=0dB, 0x00=mute)
#define ES9841_VOL_0DB                    0xFF  // 8-bit: 0 dB (unity gain)
#define ES9841_VOL_MUTE                   0x00  // 8-bit: mute

// ===== Soft Reset =====
#define ES9841_SOFT_RESET_CMD             0x80  // bit[7]=SOFT_RESET

#endif // ES9841_REGS_H
