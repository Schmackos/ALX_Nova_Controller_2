#pragma once

// Cirrus Logic DAC family -- shared constants for all Cirrus Logic DAC expansion drivers
// Devices: CS43198, CS43131, CS4398, CS4399, CS43130

// ===== I2C =====
#define CIRRUS_DAC_I2C_ADDR_BASE     0x48   // Default DAC I2C address
#define CIRRUS_DAC_I2C_BUS2_SDA      28     // I2C Bus 2 (Expansion) SDA pin
#define CIRRUS_DAC_I2C_BUS2_SCL      29     // I2C Bus 2 (Expansion) SCL pin
#define CIRRUS_DAC_I2C_FREQ_HZ       400000 // Fast-mode I2C frequency

// ===== Timing =====
#define CIRRUS_DAC_RESET_DELAY_MS    10     // Power-on / soft reset delay (ms)

// ===== Volume =====
#define CIRRUS_DAC_VOL_0DB           0x00   // 0 dB attenuation (full volume)
#define CIRRUS_DAC_VOL_MUTE          0xFF   // Full attenuation (mute)
