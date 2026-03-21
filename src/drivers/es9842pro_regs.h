#ifndef ES9842PRO_REGS_H
#define ES9842PRO_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR1=LOW, ADDR2=LOW → 0x40 (default)
// ADDR1=HIGH, ADDR2=LOW → 0x42
// ADDR1=LOW,  ADDR2=HIGH → 0x44
// ADDR1=HIGH, ADDR2=HIGH → 0x46
#define ES9842PRO_I2C_ADDR        0x40

// ===== ADC Device ID (for EEPROM identification) =====
// Chip ID register 0xE1 always reads 0x83 on ES9842PRO.
#define ES9842PRO_CHIP_ID         0x83

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9842PRO_I2C_SDA_PIN=N / -D ES9842PRO_I2C_SCL_PIN=N.
#ifndef ES9842PRO_I2C_SDA_PIN
#define ES9842PRO_I2C_SDA_PIN     28
#endif
#ifndef ES9842PRO_I2C_SCL_PIN
#define ES9842PRO_I2C_SCL_PIN     29
#endif

// ===== Register Addresses =====

// ----- System / Output Select (0x00) -----
// bit[7]=SOFT_RESET, bits[6:5]=OUTPUT_SEL (0b10=TDM)
#define ES9842PRO_REG_SYS_CONFIG          0x00

// ----- Readback (0xE1) -----
#define ES9842PRO_REG_CHIP_ID             0xE1  // Chip ID — reads 0x83 on ES9842PRO

// ----- CH1 Digital Registers (0x65-0x71) -----
#define ES9842PRO_REG_CH1_DC_BLOCKING     0x65  // bit[2]=CH1_DC_BLOCKING (HPF enable)
#define ES9842PRO_REG_CH1_VOLUME_LSB      0x6D  // CH1 volume 16-bit LSB (0x0000=mute, 0x7FFF=0dB)
#define ES9842PRO_REG_CH1_VOLUME_MSB      0x6E  // CH1 volume 16-bit MSB
#define ES9842PRO_REG_CH1_GAIN            0x70  // bits[1:0]=DATA_GAIN (0=0dB,1=+6dB,2=+12dB,3=+18dB)
#define ES9842PRO_REG_CH1_FILTER          0x71  // bits[4:2]=FILTER_SHAPE (0-7)

// ----- CH2 Digital Registers (0x76-0x82) — mirrors CH1 at offset +0x11 -----
#define ES9842PRO_REG_CH2_DC_BLOCKING     0x76  // bit[2]=CH2_DC_BLOCKING (HPF enable)
#define ES9842PRO_REG_CH2_VOLUME_LSB      0x7E  // CH2 volume 16-bit LSB
#define ES9842PRO_REG_CH2_VOLUME_MSB      0x7F  // CH2 volume 16-bit MSB
#define ES9842PRO_REG_CH2_GAIN            0x81  // bits[1:0]=DATA_GAIN
#define ES9842PRO_REG_CH2_FILTER          0x82  // bits[4:2]=FILTER_SHAPE (0-7)

// ----- CH3 Digital Registers (0x87-0x93) — offset +0x11 from CH2 -----
#define ES9842PRO_REG_CH3_DC_BLOCKING     0x87  // bit[2]=CH3_DC_BLOCKING (HPF enable)
#define ES9842PRO_REG_CH3_VOLUME_LSB      0x8F  // CH3 volume 16-bit LSB
#define ES9842PRO_REG_CH3_VOLUME_MSB      0x90  // CH3 volume 16-bit MSB
#define ES9842PRO_REG_CH3_GAIN            0x92  // bits[1:0]=DATA_GAIN
#define ES9842PRO_REG_CH3_FILTER          0x93  // bits[4:2]=FILTER_SHAPE (0-7)

// ----- CH4 Digital Registers (0x98-0xA4) — offset +0x11 from CH3 -----
#define ES9842PRO_REG_CH4_DC_BLOCKING     0x98  // bit[2]=CH4_DC_BLOCKING (HPF enable)
#define ES9842PRO_REG_CH4_VOLUME_LSB      0xA0  // CH4 volume 16-bit LSB
#define ES9842PRO_REG_CH4_VOLUME_MSB      0xA1  // CH4 volume 16-bit MSB
#define ES9842PRO_REG_CH4_GAIN            0xA3  // bits[1:0]=DATA_GAIN
#define ES9842PRO_REG_CH4_FILTER          0xA4  // bits[4:2]=FILTER_SHAPE (0-7)

// ===== Bit Masks =====

// REG_SYS_CONFIG (0x00)
#define ES9842PRO_SOFT_RESET_BIT          0x80  // Write 1 to reset; self-clearing
// OUTPUT_SEL bits[6:5]: 0b10 → TDM mode
#define ES9842PRO_OUTPUT_TDM              0x40  // bits[6:5] = 0b10

// DC_BLOCKING registers — shared bit position across all channels
#define ES9842PRO_HPF_ENABLE_BIT          0x04  // bit[2] = DC_BLOCKING enable

// GAIN registers bits[1:0]
#define ES9842PRO_GAIN_0DB                0x00  // 0 dB
#define ES9842PRO_GAIN_6DB                0x01  // +6 dB
#define ES9842PRO_GAIN_12DB               0x02  // +12 dB
#define ES9842PRO_GAIN_18DB               0x03  // +18 dB
#define ES9842PRO_GAIN_MASK               0x03  // 2-bit field mask

// FILTER registers bits[4:2] — FILTER_SHAPE (3-bit, 8 presets)
#define ES9842PRO_FILTER_MASK             0x1C  // bits[4:2] mask
#define ES9842PRO_FILTER_SHIFT            2     // shift to apply preset ordinal

// ===== Volume Constants =====
#define ES9842PRO_VOL_MUTE                0x0000  // 16-bit: mute
#define ES9842PRO_VOL_0DB                 0x7FFF  // 16-bit: 0 dB (unity gain)

// ===== Soft Reset / TDM Init =====
// Write SOFT_RESET_BIT to REG_SYS_CONFIG, then re-write OUTPUT_TDM
#define ES9842PRO_SOFT_RESET_CMD          0x80  // bit[7]=SOFT_RESET

#endif // ES9842PRO_REGS_H
