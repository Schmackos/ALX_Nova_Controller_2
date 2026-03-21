#ifndef ES9826_REGS_H
#define ES9826_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR1=LOW, ADDR2=LOW → 0x40 (default)
// ADDR1=HIGH, ADDR2=LOW → 0x42
// ADDR1=LOW,  ADDR2=HIGH → 0x44
// ADDR1=HIGH, ADDR2=HIGH → 0x46
#define ES9826_I2C_ADDR           0x40

// ===== ADC Device ID =====
// Chip ID register 0xE1 always reads 0x8A on ES9826.
#define ES9826_CHIP_ID            0x8A

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
#ifndef ES9826_I2C_SDA_PIN
#define ES9826_I2C_SDA_PIN        28
#endif
#ifndef ES9826_I2C_SCL_PIN
#define ES9826_I2C_SCL_PIN        29
#endif

// ===== Register Addresses =====

// ----- System / Reset -----
#define ES9826_REG_SYS_CONFIG     0x00  // bit[7]=SOFT_RESET (W, write 0x80 to reset)

// ----- Volume Registers (16-bit, LSB+MSB) -----
// 0dB = 0x7FFF, mute = 0x0000
#define ES9826_REG_CH1_VOL_LSB    0x2D  // CH1 volume 16-bit LSB
#define ES9826_REG_CH1_VOL_MSB    0x2E  // CH1 volume 16-bit MSB (write latches both)
#define ES9826_REG_CH2_VOL_LSB    0x2F  // CH2 volume 16-bit LSB
#define ES9826_REG_CH2_VOL_MSB    0x30  // CH2 volume 16-bit MSB (write latches both)

// ----- PGA Gain Register -----
// reg 0x44: bits[7:4] = CH2_PGA_GAIN_SET, bits[3:0] = CH1_PGA_GAIN_SET
// Nibble encoding: 0=0dB, 1=3dB, 2=6dB, 3=9dB, 4=12dB, 5=15dB,
//                  6=18dB, 7=21dB, 8=24dB, 9=27dB, 10=30dB
// Maximum nibble value: 10 (30 dB). Values > 10 are clamped to 10.
#define ES9826_REG_PGA_GAIN       0x44

// ----- Filter Register -----
// reg 0x3B: bits[4:2] = FILTER_SHAPE (3-bit field, presets 0-7)
#define ES9826_REG_FILTER         0x3B

// ----- Chip ID Register (read-only) -----
#define ES9826_REG_CHIP_ID        0xE1  // Reads 0x8A on ES9826

// ===== Bit Masks =====

// REG_SYS_CONFIG (0x00)
#define ES9826_SOFT_RESET_BIT     0x80  // Write 1 to reset; self-clearing

// REG_FILTER (0x3B) — FILTER_SHAPE field bits[4:2]
#define ES9826_FILTER_SHAPE_SHIFT 2     // Bit shift for FILTER_SHAPE field
#define ES9826_FILTER_SHAPE_MASK  0x1C  // bits[4:2] mask

// ===== Volume Constants =====
#define ES9826_VOL_MUTE           0x0000  // 16-bit: mute
#define ES9826_VOL_0DB            0x7FFF  // 16-bit: 0 dB (unity gain, default)

// ===== Gain Constants =====
#define ES9826_PGA_MAX_DB         30    // Maximum PGA gain in dB
#define ES9826_PGA_STEP_DB        3     // PGA step size in dB
#define ES9826_PGA_MAX_NIBBLE     10    // Maximum nibble value (30 dB / 3 dB per step)

#endif // ES9826_REGS_H
