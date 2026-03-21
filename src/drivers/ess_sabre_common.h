#pragma once

// ESS SABRE ADC family -- shared constants
// These are common across ES9820, ES9821, ES9822PRO, ES9823PRO, ES9826, ES9840, ES9841, ES9842PRO, ES9843PRO

// I2C
#define ESS_SABRE_REG_CHIP_ID       0xE1   // Chip ID register (same address on all ESS SABRE ADCs)
#define ESS_SABRE_I2C_ADDR_BASE     0x40   // Default I2C address (ADDR1=LOW, ADDR2=LOW)
#define ESS_SABRE_I2C_ADDR_1        0x42   // ADDR1=HIGH, ADDR2=LOW
#define ESS_SABRE_I2C_ADDR_2        0x44   // ADDR1=LOW,  ADDR2=HIGH
#define ESS_SABRE_I2C_ADDR_3        0x46   // ADDR1=HIGH, ADDR2=HIGH
#define ESS_SABRE_I2C_BUS2_SDA      28     // I2C Bus 2 (Expansion) SDA pin
#define ESS_SABRE_I2C_BUS2_SCL      29     // I2C Bus 2 (Expansion) SCL pin
#define ESS_SABRE_I2C_FREQ_HZ       400000 // Standard fast-mode I2C frequency

// Filter preset ordinals (0-7) -- same vocabulary across ESS SABRE ADC family
// Note: bit positions in the register differ per device; use per-device regs.h for actual register writes
#define ESS_SABRE_FILTER_MIN_PHASE          0  // Minimum Phase
#define ESS_SABRE_FILTER_LINEAR_APOD_FAST   1  // Linear Apodizing Fast
#define ESS_SABRE_FILTER_LINEAR_FAST        2  // Linear Fast
#define ESS_SABRE_FILTER_LINEAR_FAST_LR     3  // Linear Fast Low Ripple
#define ESS_SABRE_FILTER_LINEAR_SLOW        4  // Linear Slow
#define ESS_SABRE_FILTER_MIN_FAST           5  // Minimum Fast
#define ESS_SABRE_FILTER_MIN_SLOW           6  // Minimum Slow
#define ESS_SABRE_FILTER_MIN_SLOW_LD        7  // Minimum Slow Low Dispersion
#define ESS_SABRE_FILTER_COUNT              8  // Total number of filter presets

// Soft reset delay (ms) -- required on all ESS SABRE ADC family devices
#define ESS_SABRE_RESET_DELAY_MS    5
