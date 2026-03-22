#pragma once

// ESS SABRE family -- shared constants for ADC and DAC devices
// ADCs: ES9820, ES9821, ES9822PRO, ES9823PRO, ES9826, ES9840, ES9841, ES9842PRO, ES9843PRO
// DACs: ES9020, ES9033Q, ES9038Q2M, ES9039Q2M, ES9069Q

// ===== I2C (shared across ADC + DAC families) =====
#define ESS_SABRE_REG_CHIP_ID       0xE1   // Chip ID register (same address on all ESS SABRE devices)
#define ESS_SABRE_I2C_BUS2_SDA      28     // I2C Bus 2 (Expansion) SDA pin
#define ESS_SABRE_I2C_BUS2_SCL      29     // I2C Bus 2 (Expansion) SCL pin
#define ESS_SABRE_I2C_FREQ_HZ       400000 // Standard fast-mode I2C frequency

// ADC I2C addresses (0x40 range)
#define ESS_SABRE_I2C_ADDR_BASE     0x40   // Default ADC I2C address (ADDR1=LOW, ADDR2=LOW)
#define ESS_SABRE_I2C_ADDR_1        0x42   // ADDR1=HIGH, ADDR2=LOW
#define ESS_SABRE_I2C_ADDR_2        0x44   // ADDR1=LOW,  ADDR2=HIGH
#define ESS_SABRE_I2C_ADDR_3        0x46   // ADDR1=HIGH, ADDR2=HIGH

// DAC I2C addresses (0x48 range — separate from ADCs for dual mezzanine)
#define ESS_SABRE_DAC_I2C_ADDR_BASE 0x48   // Default DAC I2C address
#define ESS_SABRE_DAC_I2C_ADDR_1    0x4A   // ADDR1=HIGH, ADDR2=LOW
#define ESS_SABRE_DAC_I2C_ADDR_2    0x4C   // ADDR1=LOW,  ADDR2=HIGH
#define ESS_SABRE_DAC_I2C_ADDR_3    0x4E   // ADDR1=HIGH, ADDR2=HIGH

// ===== Filter preset ordinals (0-7) =====
// Same vocabulary across ADC and DAC families. Bit positions in registers
// differ per device; use per-device regs.h for actual register writes.
#define ESS_SABRE_FILTER_MIN_PHASE          0  // Minimum Phase
#define ESS_SABRE_FILTER_LINEAR_APOD_FAST   1  // Linear Apodizing Fast
#define ESS_SABRE_FILTER_LINEAR_FAST        2  // Linear Fast
#define ESS_SABRE_FILTER_LINEAR_FAST_LR     3  // Linear Fast Low Ripple
#define ESS_SABRE_FILTER_LINEAR_SLOW        4  // Linear Slow
#define ESS_SABRE_FILTER_MIN_FAST           5  // Minimum Fast
#define ESS_SABRE_FILTER_MIN_SLOW           6  // Minimum Slow
#define ESS_SABRE_FILTER_MIN_SLOW_LD        7  // Minimum Slow Low Dispersion
#define ESS_SABRE_FILTER_COUNT              8  // Total number of filter presets

// ===== Timing =====
#define ESS_SABRE_RESET_DELAY_MS    5  // Soft reset delay (ms) -- required on all ESS SABRE devices

// ===== DAC volume constants =====
#define ESS_SABRE_DAC_VOL_STEPS     128  // 128-step attenuation (0.5dB per step)
#define ESS_SABRE_DAC_VOL_0DB       0x00 // 0dB attenuation (full volume)
#define ESS_SABRE_DAC_VOL_MUTE      0xFF // Full attenuation (mute)
