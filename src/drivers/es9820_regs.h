#ifndef ES9820_REGS_H
#define ES9820_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR1=LOW, ADDR2=LOW → 0x40 (default)
// ADDR1=HIGH, ADDR2=LOW → 0x42
// ADDR1=LOW,  ADDR2=HIGH → 0x44
// ADDR1=HIGH, ADDR2=HIGH → 0x46
#define ES9820_I2C_ADDR           0x40

// ===== ADC Device ID (for EEPROM / chip identification) =====
// Chip ID register 0xE1 always reads 0x84 on ES9820.
#define ES9820_CHIP_ID            0x84

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9820_I2C_SDA_PIN=N / -D ES9820_I2C_SCL_PIN=N.
#ifndef ES9820_I2C_SDA_PIN
#define ES9820_I2C_SDA_PIN        28
#endif
#ifndef ES9820_I2C_SCL_PIN
#define ES9820_I2C_SCL_PIN        29
#endif

// ===== Register Addresses =====

// ----- System Registers -----
#define ES9820_REG_SYS_CONFIG             0x00  // bit7=SOFT_RESET(W), bits6:5=OUTPUT_SEL(0=I2S)

// ----- I2S/TDM Master Config -----
#define ES9820_REG_I2S_TDM_MASTER_CFG     0x08  // bit0=MASTER_MODE_ENABLE, bit1=WS_INVERT

// ----- CH1 Digital Registers (0x65-0x71) — same layout as ES9822PRO -----
#define ES9820_REG_CH1_DATAPATH           0x65  // bit2=ENABLE_DC_BLOCKING (HPF)
#define ES9820_REG_CH1_VOLUME_LSB         0x6D  // Volume 16-bit signed LSB (0x0000=mute, 0x7FFF=0dB)
#define ES9820_REG_CH1_VOLUME_MSB         0x6E  // Volume 16-bit signed MSB
#define ES9820_REG_CH1_VOLUME_RATE        0x6F  // Ramp rate: 0x00=instant
// reg 0x70: bits[1:0] = CH1_DATA_GAIN (0=0dB, 1=+6dB, 2=+12dB, 3=+18dB)
#define ES9820_REG_CH1_GAIN               0x70
// reg 0x71: bits[4:2] = ADC1_FILTER_SHAPE (0-7)
#define ES9820_REG_CH1_FILTER             0x71

// ----- CH2 Digital Registers (0x76-0x82) — mirrors CH1 at offset +0x11 -----
#define ES9820_REG_CH2_DATAPATH           0x76  // bit2=ENABLE_DC_BLOCKING (HPF)
#define ES9820_REG_CH2_VOLUME_LSB         0x7E  // Volume 16-bit signed LSB (same format as CH1)
#define ES9820_REG_CH2_VOLUME_MSB         0x7F  // Volume 16-bit signed MSB
#define ES9820_REG_CH2_VOLUME_RATE        0x80  // Ramp rate (same format as CH1)
// reg 0x81: bits[1:0] = CH2_DATA_GAIN (0=0dB, 1=+6dB, 2=+12dB, 3=+18dB)
#define ES9820_REG_CH2_GAIN               0x81
// reg 0x82: bits[4:2] = ADC2_FILTER_SHAPE (0-7)
#define ES9820_REG_CH2_FILTER             0x82

// ----- Readback Registers (read-only) -----
#define ES9820_REG_CHIP_ID                0xE1  // Chip ID — reads 0x84 on ES9820

// ===== Bit Masks =====

// REG_SYS_CONFIG (0x00)
#define ES9820_SOFT_RESET_BIT             0x80  // Write 1 to reset; self-clearing
#define ES9820_OUTPUT_I2S                 0x00  // OUTPUT_SEL = 0b00 → I2S/Philips

// REG_CH1_DATAPATH / REG_CH2_DATAPATH
#define ES9820_HPF_ENABLE_BIT             0x04  // ENABLE_DC_BLOCKING (bit2, same on both channels)

// REG_CH1_GAIN / REG_CH2_GAIN (bits1:0)
#define ES9820_GAIN_0DB                   0x00  // 0 dB
#define ES9820_GAIN_6DB                   0x01  // +6 dB
#define ES9820_GAIN_12DB                  0x02  // +12 dB
#define ES9820_GAIN_18DB                  0x03  // +18 dB
#define ES9820_GAIN_MAX_DB                18    // Maximum gain in dB

// REG_CH1_FILTER / REG_CH2_FILTER — FILTER_SHAPE field (bits4:2)
// Filter shape preset names (0-7):
//   0 = Minimum phase
//   1 = Linear apodizing fast
//   2 = Linear fast
//   3 = Linear fast low ripple
//   4 = Linear slow
//   5 = Minimum fast
//   6 = Minimum slow
//   7 = Minimum slow low dispersion
#define ES9820_FILTER_SHIFT               2     // bits[4:2] = FILTER_SHAPE
#define ES9820_FILTER_MASK                0x07  // 3-bit field

// ===== Volume Constants =====
#define ES9820_VOL_MUTE                   0x0000  // 16-bit: mute
#define ES9820_VOL_0DB                    0x7FFF  // 16-bit: 0 dB (unity gain)

// ===== Clock Config =====
// bits7:4 = 0011 (CH1+CH2 data enabled), bits3:0 = 0011 (CH1+CH2 decimation enabled)
#define ES9820_CLOCK_ENABLE_2CH           0x33

#endif // ES9820_REGS_H
