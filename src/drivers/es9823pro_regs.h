#ifndef ES9823PRO_REGS_H
#define ES9823PRO_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR1=LOW, ADDR2=LOW → 0x40 (default)
// ADDR1=HIGH, ADDR2=LOW → 0x42
// ADDR1=LOW,  ADDR2=HIGH → 0x44
// ADDR1=HIGH, ADDR2=HIGH → 0x46
#define ES9823PRO_I2C_ADDR        0x40

// ===== ADC Device ID (for EEPROM / chip identification) =====
// Chip ID register 0xE1 reads 0x8D on ES9823PRO, 0x8C on ES9823MPRO.
// Both variants share the same register map and driver.
#define ES9823PRO_CHIP_ID         0x8D  // ES9823PRO
#define ES9823MPRO_CHIP_ID        0x8C  // ES9823MPRO ("monolithic" variant)

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9823PRO_I2C_SDA_PIN=N / -D ES9823PRO_I2C_SCL_PIN=N.
#ifndef ES9823PRO_I2C_SDA_PIN
#define ES9823PRO_I2C_SDA_PIN     28
#endif
#ifndef ES9823PRO_I2C_SCL_PIN
#define ES9823PRO_I2C_SCL_PIN     29
#endif

// ===== Register Addresses =====

// ----- System Registers -----
#define ES9823PRO_REG_SYS_CONFIG          0x00  // bit7=SOFT_RESET(W), bits6:5=OUTPUT_SEL(0=I2S,1=SPDIF,2=TDM,3=DSD)
#define ES9823PRO_REG_ADC_CLOCK_CONFIG1   0x01  // bits7:4=enable CH data input clocks, bits3:0=enable decimation clocks

// ----- I2S/TDM Master Config -----
#define ES9823PRO_REG_I2S_TDM_MASTER_CFG  0x08  // bit0=MASTER_MODE_ENABLE, bit1=WS_INVERT, bit2=BCK_INVERT

// ----- ADC Filter Config -----
// bits[7:5] = FILTER_SHAPE (3-bit, 8 presets 0-7)
#define ES9823PRO_REG_FILTER_SHAPE        0x4A

// ----- Volume Registers (CH1: 0x51/0x52, CH2: 0x53/0x54) -----
// 16-bit signed: LSB first, MSB write latches both. 0dB=0x7FFF, mute=0x0000.
#define ES9823PRO_REG_CH1_VOLUME_LSB      0x51
#define ES9823PRO_REG_CH1_VOLUME_MSB      0x52
#define ES9823PRO_REG_CH2_VOLUME_LSB      0x53
#define ES9823PRO_REG_CH2_VOLUME_MSB      0x54

// ----- Digital Gain Register -----
// reg 0x55: bits[6:4]=CH2_DIGITAL_GAIN, bits[2:0]=CH1_DIGITAL_GAIN
// Values: 0=0dB, 1=6dB, 2=12dB, 3=18dB, 4=24dB, 5=30dB, 6=36dB, 7=42dB
#define ES9823PRO_REG_DIGITAL_GAIN        0x55

// ----- Volume Ramp Registers (optional) -----
#define ES9823PRO_REG_VOL_RAMP_RATE_UP    0x58  // 0x00=instant ramp
#define ES9823PRO_REG_VOL_RAMP_RATE_DOWN  0x59  // 0x00=instant ramp

// ----- Readback Registers (read-only) -----
#define ES9823PRO_REG_CHIP_ID             0xE1  // 0x8D=ES9823PRO, 0x8C=ES9823MPRO

// ===== Bit Masks =====

// REG_SYS_CONFIG (0x00) — OUTPUT_SEL bits6:5
#define ES9823PRO_SOFT_RESET_BIT          0x80  // Write 1 to reset; self-clearing
#define ES9823PRO_OUTPUT_I2S              0x00  // OUTPUT_SEL = 0b00 → I2S/Philips

// REG_DIGITAL_GAIN (0x55) — CH field positions
#define ES9823PRO_CH1_GAIN_SHIFT          0     // bits[2:0] = CH1_DIGITAL_GAIN
#define ES9823PRO_CH2_GAIN_SHIFT          4     // bits[6:4] = CH2_DIGITAL_GAIN
#define ES9823PRO_CH_GAIN_MASK            0x07  // 3-bit gain field

// REG_FILTER_SHAPE (0x4A) — FILTER_SHAPE bits[7:5]
#define ES9823PRO_FILTER_SHIFT            5     // bits[7:5] = FILTER_SHAPE
#define ES9823PRO_FILTER_MASK             0x07  // 3-bit field

// ===== Gain Constants =====
// Valid gain steps (register value = gainDb / 6, max 7)
#define ES9823PRO_GAIN_0DB                0x00  // 0 dB
#define ES9823PRO_GAIN_6DB                0x01  // +6 dB
#define ES9823PRO_GAIN_12DB               0x02  // +12 dB
#define ES9823PRO_GAIN_18DB               0x03  // +18 dB
#define ES9823PRO_GAIN_24DB               0x04  // +24 dB
#define ES9823PRO_GAIN_30DB               0x05  // +30 dB
#define ES9823PRO_GAIN_36DB               0x06  // +36 dB
#define ES9823PRO_GAIN_42DB               0x07  // +42 dB
#define ES9823PRO_GAIN_MAX_DB             42    // Maximum gain in dB

// ===== Volume Constants =====
#define ES9823PRO_VOL_MUTE                0x0000  // 16-bit: mute
#define ES9823PRO_VOL_0DB                 0x7FFF  // 16-bit: 0 dB (unity gain)

// ===== Clock Config =====
// bits7:4 = 0011 (CH1+CH2 data enabled), bits3:0 = 0011 (CH1+CH2 decimation enabled)
#define ES9823PRO_CLOCK_ENABLE_2CH        0x33

#endif // ES9823PRO_REGS_H
