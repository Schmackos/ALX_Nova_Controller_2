#ifndef ES9020_DAC_REGS_H
#define ES9020_DAC_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// Default DAC address range (0x48 base, separate from ADC 0x40 range)
// ADDR1=LOW, ADDR2=LOW → 0x48 (default)
// ADDR1=HIGH, ADDR2=LOW → 0x4A
// ADDR1=LOW,  ADDR2=HIGH → 0x4C
// ADDR1=HIGH, ADDR2=HIGH → 0x4E
#define ES9020_DAC_I2C_ADDR       0x48

// ===== DAC Device ID (chip identification) =====
// Chip ID register 0xE1 always reads 0x86 on ES9020.
#define ES9020_CHIP_ID            0x86

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9020_I2C_SDA_PIN=N / -D ES9020_I2C_SCL_PIN=N.
#ifndef ES9020_I2C_SDA_PIN
#define ES9020_I2C_SDA_PIN        28
#endif
#ifndef ES9020_I2C_SCL_PIN
#define ES9020_I2C_SCL_PIN        29
#endif

// ===== Register Addresses =====

// ----- System / Format Config (0x00-0x0F) -----
#define ES9020_REG_SOFT_RESET         0x00  // bit7=SOFT_RESET (W, self-clearing)

// Input format / TDM config
// bits[5:4] = TDM_SLOTS (0b00=2slots, 0b01=4slots, 0b10=8slots, 0b11=16slots)
// bit3 = TDM_ENABLE, bit1 = INPUT_SEL (0=I2S, 1=DSD)
#define ES9020_REG_INPUT_CONFIG       0x01

// ----- Digital Filter (0x07) -----
// bits[2:0] = FILTER_SHAPE (0-7 — same 8 preset vocabulary as other ESS SABRE devices)
#define ES9020_REG_FILTER             0x07

// ----- APLL Control (0x0C) -----
// bit0 = APLL_ENABLE (1 = use APLL clock recovery, 0 = bypass MCLK)
// bit4 = APLL_LOCK_STATUS (read-only: 1 = APLL is locked)
#define ES9020_REG_APLL_CTRL          0x0C

// ----- Clock Source Select (0x0D) -----
// bits[1:0] = CLK_SRC:
//   0b00 = BCK recovery (derive MCLK from BCK — APLL mode)
//   0b01 = DSD clock input
//   0b10 = External MCLK
#define ES9020_REG_CLK_SOURCE         0x0D

// ----- Volume (0x0F) -----
// 8-bit attenuation register. 0.5 dB per step.
// 0x00 = 0 dB (no attenuation = full volume)
// 0xFF = 63.5 dB attenuation (effectively muted)
// Same encoding as ESS_SABRE_DAC_VOL_0DB / ESS_SABRE_DAC_VOL_MUTE in ess_sabre_common.h
#define ES9020_REG_VOLUME             0x0F

// ----- Readback Registers (read-only, 0xE0-0xFF) -----
#define ES9020_REG_CHIP_ID            0xE1  // Chip ID — reads 0x86 on ES9020

// ===== Bit Masks =====

// REG_SOFT_RESET (0x00)
#define ES9020_SOFT_RESET_BIT         0x80  // Write 1 to reset; self-clearing

// REG_INPUT_CONFIG (0x01) — TDM slot count field bits[5:4]
#define ES9020_TDM_SLOTS_2            0x00  // bits[5:4] = 0b00 → 2 slots (stereo I2S)
#define ES9020_TDM_SLOTS_4            0x10  // bits[5:4] = 0b01 → 4 slots
#define ES9020_TDM_SLOTS_8            0x20  // bits[5:4] = 0b10 → 8 slots
#define ES9020_TDM_SLOTS_16           0x30  // bits[5:4] = 0b11 → 16 slots
#define ES9020_TDM_SLOTS_MASK         0x30  // Mask for bits[5:4]
#define ES9020_TDM_ENABLE_BIT         0x08  // bit3 = TDM_ENABLE

// REG_FILTER (0x07) — FILTER_SHAPE bits[2:0]
#define ES9020_FILTER_SHAPE_MASK      0x07  // bits[2:0]
#define ES9020_FILTER_MIN_PHASE       0x00  // 0 = Minimum phase
#define ES9020_FILTER_LIN_APO_FAST    0x01  // 1 = Linear apodizing fast
#define ES9020_FILTER_LIN_FAST        0x02  // 2 = Linear fast
#define ES9020_FILTER_LIN_FAST_LR     0x03  // 3 = Linear fast low ripple
#define ES9020_FILTER_LIN_SLOW        0x04  // 4 = Linear slow
#define ES9020_FILTER_MIN_FAST        0x05  // 5 = Minimum fast
#define ES9020_FILTER_MIN_SLOW        0x06  // 6 = Minimum slow
#define ES9020_FILTER_MIN_SLOW_LD     0x07  // 7 = Minimum slow low dispersion

// REG_APLL_CTRL (0x0C)
#define ES9020_APLL_ENABLE_BIT        0x01  // bit0 = APLL_ENABLE
#define ES9020_APLL_LOCK_BIT          0x10  // bit4 = APLL_LOCK_STATUS (read-only)

// REG_CLK_SOURCE (0x0D) — bits[1:0]
#define ES9020_CLK_BCK_RECOVERY       0x00  // 0b00 = BCK recovery via APLL
#define ES9020_CLK_DSD                0x01  // 0b01 = DSD clock
#define ES9020_CLK_MCLK               0x02  // 0b10 = External MCLK

// ===== Volume Constants =====
// 0.5 dB/step attenuation — 0x00 = 0 dB, 0xFF = full attenuation
#define ES9020_VOL_0DB                0x00  // No attenuation (full volume)
#define ES9020_VOL_MUTE               0xFF  // Full attenuation (muted)

#endif // ES9020_DAC_REGS_H
