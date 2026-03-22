#ifndef ES9033Q_REGS_H
#define ES9033Q_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR1=LOW, ADDR2=LOW → 0x48 (default DAC base address)
// ADDR1=HIGH, ADDR2=LOW → 0x4A
// ADDR1=LOW,  ADDR2=HIGH → 0x4C
// ADDR1=HIGH, ADDR2=HIGH → 0x4E
#define ES9033Q_I2C_ADDR        0x48

// ===== Device ID (for EEPROM identification) =====
// Chip ID register 0xE1 always reads 0x88 on ES9033Q.
#define ES9033Q_CHIP_ID         0x88

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9033Q_I2C_SDA_PIN=N / -D ES9033Q_I2C_SCL_PIN=N.
#ifndef ES9033Q_I2C_SDA_PIN
#define ES9033Q_I2C_SDA_PIN     28
#endif
#ifndef ES9033Q_I2C_SCL_PIN
#define ES9033Q_I2C_SCL_PIN     29
#endif

// ===== Register Addresses =====

// ----- System / Control Registers -----
#define ES9033Q_REG_SYSTEM_SETTINGS   0x00  // bits7:6=INPUT_SEL(0=I2S,1=DSD,2=TDM), bit1=SOFT_START
#define ES9033Q_REG_INPUT_CONFIG      0x01  // bits7:6=I2S_LEN(0=32,1=24,2=20,3=16), bits5:4=I2S_MODE(0=Philips)
#define ES9033Q_REG_AUTOMUTE_TIME     0x04  // Automute trigger time (0=disable)
#define ES9033Q_REG_AUTOMUTE_LEVEL    0x05  // Automute trigger level
#define ES9033Q_REG_DSD_CONFIG        0x06  // bits1:0=DSD_RATE (0=DSD64, 1=DSD128, 2=DSD256, 3=DSD512)
#define ES9033Q_REG_FILTER_SHAPE      0x07  // bits2:0=FILTER_SHAPE (0-7 filter presets)
#define ES9033Q_REG_GENERAL_CONFIG    0x08  // bit7=DUAL_MONO_MODE, bit6=STEREO_MODE_INV
#define ES9033Q_REG_GPIO_CONFIG       0x09  // GPIO function select
#define ES9033Q_REG_MASTER_MODE       0x0A  // bit0=MASTER_MODE_ENABLE
#define ES9033Q_REG_DPLL_BANDWIDTH    0x0C  // bits3:0=DPLL_BW (0=lowest, 15=highest)
#define ES9033Q_REG_SOFT_START_CONFIG 0x0E  // bits3:0=SOFT_START_RATE

// ----- Volume Registers (0.5 dB per step attenuation) -----
// 0x00 = 0 dB (full volume), 0xFF = full attenuation (mute)
#define ES9033Q_REG_VOLUME_L          0x0F  // Left channel attenuation (0=0dB, 0xFF=mute)
#define ES9033Q_REG_VOLUME_R          0x10  // Right channel attenuation (0=0dB, 0xFF=mute)

// ----- THD Compensation Register -----
#define ES9033Q_REG_THD_COMP_BYPASS   0x0D  // bit0=BYPASS_THD_COMP

// ----- Integrated Line Driver Control Register (ES9033Q unique) -----
// The ES9033Q integrates a 2 Vrms ground-centered line output stage on-chip,
// eliminating the need for external op-amp output buffers.
//
// bit0   = LINE_DRIVER_ENABLE  (1 = enable integrated line drivers, 0 = disable/power down)
// bits[3:1] = OUTPUT_IMPEDANCE (output impedance select: 0=lowest/75Ω, 7=highest/600Ω)
// bit4   = COMMON_MODE_SEL    (0 = ground-centered, 1 = mid-supply referenced)
// bit5   = CURRENT_LIMIT_EN   (1 = enable over-current protection on output drivers)
#define ES9033Q_REG_LINE_DRIVER       0x14  // Integrated line driver control register

// ----- Readback Registers (read-only, 0xE0+) -----
#define ES9033Q_REG_CHIP_ID           0xE1  // Chip ID — reads 0x88 on ES9033Q
#define ES9033Q_REG_DPLL_LOCK         0xE2  // bit0=DPLL_LOCK, bit1=AUTOMUTE_ACTIVE
#define ES9033Q_REG_INPUT_DETECT      0xE3  // bits3:0=detected input format

// ===== Bit Masks =====

// REG_SYSTEM_SETTINGS (0x00)
#define ES9033Q_INPUT_I2S             0x00  // I2S input (bits7:6 = 0b00)
#define ES9033Q_INPUT_DSD             0x40  // DSD native
#define ES9033Q_INPUT_TDM             0x80  // TDM mode
#define ES9033Q_SOFT_START_BIT        0x02  // Enable soft start ramp

// REG_INPUT_CONFIG (0x01) — I2S_LEN bits7:6
#define ES9033Q_I2S_LEN_32            0x00  // 32-bit I2S
#define ES9033Q_I2S_LEN_24            0x40  // 24-bit I2S
#define ES9033Q_I2S_LEN_20            0x80  // 20-bit I2S
#define ES9033Q_I2S_LEN_16            0xC0  // 16-bit I2S

// REG_FILTER_SHAPE (0x07) — bits2:0
#define ES9033Q_FILTER_MIN_PHASE      0x00  // Minimum Phase
#define ES9033Q_FILTER_LIN_APO_FAST   0x01  // Linear Apodizing Fast
#define ES9033Q_FILTER_LIN_FAST       0x02  // Linear Fast
#define ES9033Q_FILTER_LIN_FAST_LR    0x03  // Linear Fast Low Ripple
#define ES9033Q_FILTER_LIN_SLOW       0x04  // Linear Slow
#define ES9033Q_FILTER_MIN_FAST       0x05  // Minimum Fast
#define ES9033Q_FILTER_MIN_SLOW       0x06  // Minimum Slow
#define ES9033Q_FILTER_MIN_SLOW_LD    0x07  // Minimum Slow Low Dispersion

// REG_LINE_DRIVER (0x14) — Integrated line driver control
#define ES9033Q_LINE_DRIVER_ENABLE    0x01  // bit0: enable integrated line drivers
#define ES9033Q_LINE_DRIVER_IMP_MASK  0x0E  // bits[3:1]: output impedance select
#define ES9033Q_LINE_DRIVER_IMP_SHIFT 1     // Right-shift to extract impedance nibble
#define ES9033Q_LINE_DRIVER_CMSEL     0x10  // bit4: common mode reference select
#define ES9033Q_LINE_DRIVER_ILIMIT    0x20  // bit5: current limit enable

// Output impedance preset values (after shifting bits[3:1])
#define ES9033Q_LINE_IMP_75_OHM       0x00  // Lowest impedance (~75 Ω) — default
#define ES9033Q_LINE_IMP_150_OHM      0x01  // ~150 Ω
#define ES9033Q_LINE_IMP_300_OHM      0x02  // ~300 Ω
#define ES9033Q_LINE_IMP_600_OHM      0x07  // Highest impedance (~600 Ω)

// REG_DPLL_LOCK (0xE2)
#define ES9033Q_DPLL_LOCKED_BIT       0x01  // DPLL locked to incoming clock
#define ES9033Q_AUTOMUTE_ACTIVE_BIT   0x02  // Automute is active

// ===== Volume Constants =====
#define ES9033Q_VOL_0DB               0x00  // 0 dB attenuation (full volume)
#define ES9033Q_VOL_MUTE              0xFF  // Full attenuation (mute)

// ===== Optimal Init Values =====
#define ES9033Q_DPLL_BW_DEFAULT       0x04  // Balanced DPLL bandwidth setting
// Line driver init: enable with lowest impedance (75 Ω), ground-centered, current limit enabled
#define ES9033Q_LINE_DRIVER_INIT      (ES9033Q_LINE_DRIVER_ENABLE | ES9033Q_LINE_DRIVER_ILIMIT)

#endif // ES9033Q_REGS_H
