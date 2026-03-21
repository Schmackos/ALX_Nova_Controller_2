#ifndef ES9821_REGS_H
#define ES9821_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR1=LOW, ADDR2=LOW → 0x40 (default)
// ADDR1=HIGH, ADDR2=LOW → 0x42
// ADDR1=LOW,  ADDR2=HIGH → 0x44
// ADDR1=HIGH, ADDR2=HIGH → 0x46
#define ES9821_I2C_ADDR           0x40

// ===== ADC Device ID =====
// Chip ID register 0xE1 always reads 0x88 on ES9821.
#define ES9821_CHIP_ID            0x88

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
#ifndef ES9821_I2C_SDA_PIN
#define ES9821_I2C_SDA_PIN        28
#endif
#ifndef ES9821_I2C_SCL_PIN
#define ES9821_I2C_SCL_PIN        29
#endif

// ===== Register Addresses =====

// ----- System / Reset -----
#define ES9821_REG_SYS_CONFIG     0x00  // bit[7]=SOFT_RESET (W, write 0x80 to reset)

// ----- Volume Registers (16-bit, LSB+MSB) -----
// 0dB = 0x7FFF, mute = 0x0000
#define ES9821_REG_CH1_VOL_LSB    0x32  // CH1 volume 16-bit LSB
#define ES9821_REG_CH1_VOL_MSB    0x33  // CH1 volume 16-bit MSB (write latches both)
#define ES9821_REG_CH2_VOL_LSB    0x34  // CH2 volume 16-bit LSB
#define ES9821_REG_CH2_VOL_MSB    0x35  // CH2 volume 16-bit MSB (write latches both)

// ----- Filter Register -----
// reg 0x40: bits[4:2] = ADC_FILTER_SHAPE (3-bit field, presets 0-7)
#define ES9821_REG_FILTER         0x40

// ----- Chip ID Register (read-only) -----
#define ES9821_REG_CHIP_ID        0xE1  // Reads 0x88 on ES9821

// ===== Bit Masks =====

// REG_SYS_CONFIG (0x00)
#define ES9821_SOFT_RESET_BIT     0x80  // Write 1 to reset; self-clearing

// REG_FILTER (0x40) — ADC_FILTER_SHAPE field bits[4:2]
#define ES9821_FILTER_SHAPE_SHIFT 2     // Bit shift for ADC_FILTER_SHAPE field
#define ES9821_FILTER_SHAPE_MASK  0x1C  // bits[4:2] mask

// ===== Volume Constants =====
#define ES9821_VOL_MUTE           0x0000  // 16-bit: mute
#define ES9821_VOL_0DB            0x7FFF  // 16-bit: 0 dB (unity gain, default)

// NOTE: ES9821 has no PGA (no hardware gain register).
// adcSetGain(0) is accepted as a no-op; any non-zero value returns false.

#endif // ES9821_REGS_H
