#ifndef ES9822PRO_REGS_H
#define ES9822PRO_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR1=LOW, ADDR2=LOW → 0x40 (default, synchronous master mode)
// ADDR1=HIGH, ADDR2=LOW → 0x42
// ADDR1=LOW,  ADDR2=HIGH → 0x44
// ADDR1=HIGH, ADDR2=HIGH → 0x46
// Synchronous slave variants (no MCLK required): 0x48 / 0x4A / 0x4C / 0x4E
#define ES9822PRO_I2C_ADDR        0x40

// ===== ADC Device ID (for EEPROM identification) =====
// Chip ID register 0xE1 always reads 0x81 on ES9822PRO.
#define ES9822PRO_CHIP_ID         0x81

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9822PRO_I2C_SDA_PIN=N / -D ES9822PRO_I2C_SCL_PIN=N.
#ifndef ES9822PRO_I2C_SDA_PIN
#define ES9822PRO_I2C_SDA_PIN     28
#endif
#ifndef ES9822PRO_I2C_SCL_PIN
#define ES9822PRO_I2C_SCL_PIN     29
#endif

// ===== Register Addresses =====

// ----- System Registers (0x00-0x22) -----
#define ES9822PRO_REG_SYS_CONFIG          0x00  // bit7=SOFT_RESET(W), bits6:5=OUTPUT_SEL(0=I2S,1=SPDIF,2=TDM,3=DSD), bit3=ENABLE_64FS_MODE, bit1=MONO_MODE
#define ES9822PRO_REG_ADC_CLOCK_CONFIG1   0x01  // bits7:4=enable CH data input clocks, bits3:0=enable decimation clocks
#define ES9822PRO_REG_ADC_CLOCK_CONFIG2   0x02  // bit5=SELECT_ADC_HALF, bits4:0=SELECT_ADC_NUM (CLK_ADC divider, default 3)
#define ES9822PRO_REG_I2S_TDM_MASTER_CFG  0x08  // bit7=MASTER_BCK_DIV1, bits5:4=MASTER_FRAME_LENGTH(0=32bit,2=16bit), bit3=WS_PULSE_MODE, bit2=BCK_INVERT, bit1=WS_INVERT, bit0=MASTER_MODE_ENABLE
#define ES9822PRO_REG_I2S_TDM_MASTER_CLK  0x09  // bit7=SELECT_I2S_TDM_HALF, bits6:0=SELECT_I2S_TDM_NUM (default 3)

// Interrupt Register
#define ES9822PRO_REG_INTERRUPT           0x1B  // bit5=CLEAR_CH2_PEAK(W), bit4=CLEAR_CH1_PEAK(W), bit1=MASK_CH2_PEAK, bit0=MASK_CH1_PEAK

// ----- TDM Registers (0x0A-0x0D) -----
#define ES9822PRO_REG_TDM_CONFIG1         0x0A  // bits7:3=TDM_BIT_DELAY, bit2=TDM_VALID_EDGE(default 1), bit1=TDM_LJ, bit0=ENABLE_TDM_CLK(default 1)
#define ES9822PRO_REG_TDM_CONFIG2         0x0B  // bit7=TDM_GPIO456, bit6=TDM_CASCADE, bit5=TDM_LENGTH(0=32bit,1=16bit), bits4:0=TDM_CH_NUM(default 1, value=N+1 channels)
#define ES9822PRO_REG_TDM_SLOT_CH1        0x0C  // bits6:5=TDM_LINE_SEL_CH1, bits4:0=TDM_SLOT_SEL_CH1(default 0)
#define ES9822PRO_REG_TDM_SLOT_CH2        0x0D  // bits6:5=TDM_LINE_SEL_CH2, bits4:0=TDM_SLOT_SEL_CH2(default 1)

// ----- Analog ADC Config (0x3F-0x47) — "program to optimal" values -----
#define ES9822PRO_REG_ADC_CH1A_CFG1       0x3F  // bits7:6=INT2_SEL(=0b10), bits5:4=INT1_SEL(=0b11), bit3=EN_FB, bit1=EN_INT, bit0=EN_COMP
#define ES9822PRO_REG_ADC_CH1A_CFG2       0x40  // bits7:5=COMP_SEL(=0b001), bits4:3=SUM_SEL(=0b11), bit2=DITHER_EXT, bit1=DITHER, bit0=USE_STATE
#define ES9822PRO_REG_ADC_CH2A_CFG1       0x41  // same bit layout as CH1A_CFG1
#define ES9822PRO_REG_ADC_CH2A_CFG2       0x42  // same bit layout as CH1A_CFG2
#define ES9822PRO_REG_ADC_COMMON_MODE     0x47  // all 8 bits set to 1 for optimal operation

// ----- CH1 Digital Registers (0x65-0x75) -----
#define ES9822PRO_REG_CH1_DATAPATH        0x65  // bit7=BYPASS_FIR2X, bit6=BYPASS_FIR4X, bit2=ENABLE_DC_BLOCKING(HPF), bit0=CH1A_NEG_SEL
#define ES9822PRO_REG_CH1_THD_COMP_CFG   0x66  // bits7:2=CORRECTION_ADDR, bit1=CORRECTION_WE, bit0=ENABLE_THD_COMP
#define ES9822PRO_REG_CH1_THD_COMP_LSB   0x67  // THD correction 16-bit value LSB
#define ES9822PRO_REG_CH1_THD_COMP_MSB   0x68  // THD correction 16-bit value MSB
#define ES9822PRO_REG_CH1_PEAK_DETECT_CFG 0x69  // bit7=LOCK_PEAK, bits6:2=PEAK_DECAY_RATE(default 10), bit0=ENABLE_PEAK_DETECT
#define ES9822PRO_REG_CH1_PEAK_THRESHOLD  0x6A  // 8-bit threshold: 0x01=-48dB, 0xFF=0dB, default 0xFF
#define ES9822PRO_REG_CH1_DC_OFFSET_LSB  0x6B  // DC offset 16-bit signed LSB
#define ES9822PRO_REG_CH1_DC_OFFSET_MSB  0x6C  // DC offset 16-bit signed MSB
#define ES9822PRO_REG_CH1_VOLUME_LSB     0x6D  // Volume 16-bit signed LSB (0x0000=mute, 0x7FFF=0dB, default 0x7FFF)
#define ES9822PRO_REG_CH1_VOLUME_MSB     0x6E  // Volume 16-bit signed MSB
#define ES9822PRO_REG_CH1_VOLUME_RATE    0x6F  // Ramp rate: 0x00=instant, 0xFF=fastest ramp
#define ES9822PRO_REG_CH1_GAIN           0x70  // bits1:0=DATA_GAIN (0=0dB, 1=+6dB, 2=+12dB, 3=+18dB)
#define ES9822PRO_REG_CH1_FILTER         0x71  // bits4:2=FILTER_SHAPE(0-7), bit1=PROG_COEFF_WE, bit0=PROG_COEFF_EN

// ----- CH2 Digital Registers (0x76-0x82) — mirrors CH1 at offset +0x11 -----
#define ES9822PRO_REG_CH2_DATAPATH        0x76  // same bit layout as CH1_DATAPATH (0x65)
#define ES9822PRO_REG_CH2_VOLUME_LSB     0x7E  // Volume 16-bit signed LSB (same format as CH1)
#define ES9822PRO_REG_CH2_VOLUME_MSB     0x7F  // Volume 16-bit signed MSB
#define ES9822PRO_REG_CH2_VOLUME_RATE    0x80  // Ramp rate (same format as CH1)
#define ES9822PRO_REG_CH2_GAIN           0x81  // bits1:0=DATA_GAIN (same format as CH1)
#define ES9822PRO_REG_CH2_FILTER         0x82  // bits4:2=FILTER_SHAPE(0-7), bit1=PROG_COEFF_WE, bit0=PROG_COEFF_EN

// ----- Sync Slave Registers (0xC0-0xC2) — accessible WITHOUT MCLK at addr 0x48-0x4E -----
#define ES9822PRO_REG_SYNC_SOFT_RESET    0xC0  // bit7=AO_SOFT_RESET
#define ES9822PRO_REG_SYNC_CLK_SELECT    0xC1  // bits2:1=SEL_SYSCLK_IN(0=XTAL,1=MCLK,2=ACLK), bit0=EN_ANA_CLKIN
#define ES9822PRO_REG_SYNC_CLK_DIVIDE    0xC2  // bits1:0=SEL_CLK_DIV(0=full,1=1/2,2=1/4,3=1/8)

// ----- Readback Registers (read-only, 0xE0-0xF3) -----
#define ES9822PRO_REG_READ_SYSTEM0       0xE0  // bit3=MODE, bit2=ADDR2, bit1=ADDR1
#define ES9822PRO_REG_CHIP_ID            0xE1  // Chip ID — reads 0x81 on ES9822PRO
#define ES9822PRO_REG_PEAK_FLAG          0xE5  // bit1=PEAK_FLAG_CH2, bit0=PEAK_FLAG_CH1
#define ES9822PRO_REG_READ_SYSTEM5       0xE7  // bit7=ASP2_INIT_DONE, bit6=ASP1_INIT_DONE, bit4=TDM_VALID

// ===== Bit Masks =====

// REG_SYS_CONFIG (0x00) — OUTPUT_SEL bits6:5
#define ES9822PRO_SOFT_RESET_BIT          0x80  // Write 1 to reset; self-clearing
#define ES9822PRO_OUTPUT_I2S              0x00  // OUTPUT_SEL = 0b00 → I2S/LJ
#define ES9822PRO_OUTPUT_SPDIF            0x20  // OUTPUT_SEL = 0b01 → S/PDIF
#define ES9822PRO_OUTPUT_TDM              0x40  // OUTPUT_SEL = 0b10 → TDM
#define ES9822PRO_OUTPUT_DSD              0x60  // OUTPUT_SEL = 0b11 → DSD
#define ES9822PRO_64FS_ENABLE             0x08  // ENABLE_64FS_MODE
#define ES9822PRO_MONO_MODE               0x02  // MONO_MODE

// REG_I2S_TDM_MASTER_CFG (0x08)
#define ES9822PRO_MASTER_MODE_BIT         0x01  // MASTER_MODE_ENABLE
#define ES9822PRO_WS_INVERT               0x02  // WS_INVERT
#define ES9822PRO_BCK_INVERT              0x04  // BCK_INVERT
#define ES9822PRO_WS_PULSE_MODE           0x08  // WS_PULSE_MODE
#define ES9822PRO_FRAME_LEN_32            0x00  // MASTER_FRAME_LENGTH = 32-bit (bits5:4 = 0b00)
#define ES9822PRO_FRAME_LEN_16            0x20  // MASTER_FRAME_LENGTH = 16-bit (bits5:4 = 0b10)

// REG_CH1_DATAPATH / REG_CH2_DATAPATH
#define ES9822PRO_HPF_ENABLE_BIT          0x04  // ENABLE_DC_BLOCKING (bit2, same position in both CH1/CH2)
#define ES9822PRO_BYPASS_FIR4X            0x40  // BYPASS_FIR4X (bit6)
#define ES9822PRO_BYPASS_FIR2X            0x80  // BYPASS_FIR2X (bit7)

// REG_CH1_GAIN / REG_CH2_GAIN (bits1:0)
#define ES9822PRO_GAIN_0DB                0x00  // 0 dB
#define ES9822PRO_GAIN_6DB                0x01  // +6 dB
#define ES9822PRO_GAIN_12DB               0x02  // +12 dB
#define ES9822PRO_GAIN_18DB               0x03  // +18 dB

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
#define ES9822PRO_FILTER_MIN_PHASE        0x00  // bits4:2 = 0b000 (shift already applied)
#define ES9822PRO_FILTER_LIN_APO_FAST     0x04  // bits4:2 = 0b001
#define ES9822PRO_FILTER_LIN_FAST         0x08  // bits4:2 = 0b010
#define ES9822PRO_FILTER_LIN_FAST_LR      0x0C  // bits4:2 = 0b011
#define ES9822PRO_FILTER_LIN_SLOW         0x10  // bits4:2 = 0b100
#define ES9822PRO_FILTER_MIN_FAST         0x14  // bits4:2 = 0b101
#define ES9822PRO_FILTER_MIN_SLOW         0x18  // bits4:2 = 0b110
#define ES9822PRO_FILTER_MIN_SLOW_LOW_DISP 0x1C // bits4:2 = 0b111

// REG_SYNC_SOFT_RESET (0xC0)
#define ES9822PRO_AO_SOFT_RESET           0x80  // AO_SOFT_RESET (synchronous slave path)

// ===== Optimal Init Values =====
// Values recommended by ESS datasheet v0.5.2 for "optimal" analog ADC performance.

// 0x3F / 0x41 — CH1A_CFG1 / CH2A_CFG1
// INT2_SEL=0b10, INT1_SEL=0b11, EN_FB=1, EN_INT=1, EN_COMP=0 → 0b10_11_1_0_1_0 = 0xBA
// Verified encoding: bits7:6=10, bits5:4=11, bit3=1, bit2=0, bit1=1, bit0=0
#define ES9822PRO_OPTIMAL_CH1A_CFG1       0xBA

// 0x40 / 0x42 — CH1A_CFG2 / CH2A_CFG2
// COMP_SEL=0b001, SUM_SEL=0b11, DITHER_EXT=0, DITHER=1, USE_STATE=0 → 0b001_11_0_1_0 = 0x3A
#define ES9822PRO_OPTIMAL_CH1A_CFG2       0x3A

// 0x47 — ADC_COMMON_MODE: all bits set for optimal common-mode rejection
#define ES9822PRO_OPTIMAL_COMMON_MODE     0xFF

// 0x01 — ADC_CLOCK_CONFIG1: enable CH1A+CH2A data input clocks and decimation clocks
// bits7:4 = 0011 (CH1A+CH2A data enabled), bits3:0 = 0011 (CH1A+CH2A decimation enabled)
#define ES9822PRO_CLOCK_ENABLE_2CH        0x33

// ===== Volume Constants =====
#define ES9822PRO_VOL_MUTE                0x0000  // 16-bit: mute
#define ES9822PRO_VOL_0DB                 0x7FFF  // 16-bit: 0 dB (default, unity gain)

#endif // ES9822PRO_REGS_H
