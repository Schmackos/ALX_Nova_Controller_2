#ifndef ES9038Q2M_REGS_H
#define ES9038Q2M_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ES9038Q2M DAC default address: 0x48 (ADDR pins LOW)
// All variants sit in the 0x48-0x4E range (separate from ADC 0x40-0x46).
#define ES9038Q2M_I2C_ADDR        0x48

// ===== Chip Identification =====
// Register 0xE1 reads 0x90 on ES9038Q2M (Hyperstream II architecture).
#define ES9038Q2M_CHIP_ID         0x90

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9038Q2M_I2C_SDA_PIN=N / -D ES9038Q2M_I2C_SCL_PIN=N.
#ifndef ES9038Q2M_I2C_SDA_PIN
#define ES9038Q2M_I2C_SDA_PIN    28
#endif
#ifndef ES9038Q2M_I2C_SCL_PIN
#define ES9038Q2M_I2C_SCL_PIN    29
#endif

// ===== Register Addresses =====

// ----- System Configuration (0x00) -----
// bit0 = SOFT_RESET (W, self-clearing); bit6 = CLK_GEAR_EN; bit7 = reserved
#define ES9038Q2M_REG_SYS_CONFIG         0x00

// ----- Input Configuration (0x01) -----
// bits[1:0] = I2S_LENGTH (0=16bit, 1=20bit, 2=24bit, 3=32bit)
// bits[3:2] = I2S_FORMAT (0=I2S/Philips, 1=Left-Justified, 2=Right-Justified)
// bit4      = AUTO_DETECT_I2S
#define ES9038Q2M_REG_INPUT_CFG          0x01

// ----- Automute Configuration (0x04) -----
// bits[2:0] = AUTOMUTE_TIME  (0=disabled, 1-7=levels)
// bit3      = AUTOMUTE_LOOPBACK_EN
#define ES9038Q2M_REG_AUTOMUTE_CFG       0x04

// ----- Digital Filter (0x07) -----
// bit0      = MUTE (1=muted both channels)
// bits[2:0] already include mute at bit0; filter is bits[4:2] per-channel
// bits[2:1] = FILTER_SHAPE_LOW  (channel assignment varies)
// bits[4:3] = FILTER_SHAPE_HIGH
// Full field: bits[4:2] = FILTER_SHAPE (0-7), bit0 = MUTE_BOTH
#define ES9038Q2M_REG_FILTER_MUTE        0x07

// ----- GPIO Configuration (0x08) -----
// bits[1:0] = GPIO1_CFG (0=off, 1=input, 2=output low, 3=output high)
// bits[3:2] = GPIO2_CFG
#define ES9038Q2M_REG_GPIO_CFG1          0x08

// ----- GPIO Control (0x09) -----
// bit0 = GPIO1_STATE (output drive value)
// bit1 = GPIO2_STATE
#define ES9038Q2M_REG_GPIO_CTRL          0x09

// ----- Master Mode / Slave Mode (0x0A) -----
// bit0 = MASTER_MODE_EN (1=master, 0=slave to ESP32 I2S)
// bit4 = BCK_INVERT
// bit5 = WS_INVERT
#define ES9038Q2M_REG_MASTER_MODE        0x0A

// ----- DPLL Config (0x0B) -----
// bits[3:0] = DPLL_BW — bandwidth 0-15 (higher = more stable, more jitter rejection)
#define ES9038Q2M_REG_DPLL_CFG           0x0B

// ----- Clock Gear / Divider (0x0D) -----
// bits[1:0] = CLOCK_GEAR (0=1x, 1=2x, 2=4x, 3=8x) for high sample rates
// Divide MCLK by 2^N to bring into range for 384kHz / 768kHz operation
#define ES9038Q2M_REG_CLOCK_GEAR         0x0D

// ----- Volume Channel 1 (0x0F) -----
// 8-bit attenuation: 0x00 = 0 dB (full output), 0xFF = full mute
// Step size: 0.5 dB per LSB. 128 steps = 0 to -63.5 dB.
#define ES9038Q2M_REG_VOL_CH1            0x0F

// ----- Volume Channel 2 (0x10) -----
// Same encoding as CH1.
#define ES9038Q2M_REG_VOL_CH2            0x10

// ----- Readback Registers (read-only) -----
#define ES9038Q2M_REG_CHIP_ID            0xE1  // Chip ID — reads 0x90 on ES9038Q2M

// ===== Bit Masks =====

// REG_SYS_CONFIG (0x00)
#define ES9038Q2M_SOFT_RESET_BIT         0x01  // Write 1 to reset; self-clearing

// REG_INPUT_CFG (0x01)
#define ES9038Q2M_I2S_LEN_16BIT          0x00  // INPUT_SELECT bits[1:0] = 00 (16-bit)
#define ES9038Q2M_I2S_LEN_24BIT          0x02  // INPUT_SELECT bits[1:0] = 10 (24-bit)
#define ES9038Q2M_I2S_LEN_32BIT          0x03  // INPUT_SELECT bits[1:0] = 11 (32-bit)
#define ES9038Q2M_I2S_FMT_PHILIPS        0x00  // FORMAT bits[3:2] = 00 (Philips/I2S)
#define ES9038Q2M_I2S_FMT_LJ             0x04  // FORMAT bits[3:2] = 01 (Left-Justified)
#define ES9038Q2M_I2S_FMT_RJ             0x08  // FORMAT bits[3:2] = 10 (Right-Justified)

// REG_FILTER_MUTE (0x07)
#define ES9038Q2M_MUTE_BIT               0x01  // bit0: mute both channels
#define ES9038Q2M_FILTER_SHIFT           2      // Filter preset field starts at bit 2
#define ES9038Q2M_FILTER_MASK            0x1C   // bits[4:2] = FILTER_SHAPE (3 bits)

// REG_MASTER_MODE (0x0A)
#define ES9038Q2M_MASTER_MODE_EN         0x01  // Enable I2S master mode
#define ES9038Q2M_SLAVE_MODE             0x00  // I2S slave (receive clocks from ESP32)
#define ES9038Q2M_BCK_INVERT_BIT         0x10  // Invert bit clock polarity
#define ES9038Q2M_WS_INVERT_BIT          0x20  // Invert word select polarity

// REG_CLOCK_GEAR (0x0D)
#define ES9038Q2M_CLK_GEAR_1X            0x00  // No division (default, ≤192kHz)
#define ES9038Q2M_CLK_GEAR_2X            0x01  // Divide MCLK by 2 (384kHz)
#define ES9038Q2M_CLK_GEAR_4X            0x02  // Divide MCLK by 4 (768kHz)

// ===== Volume Constants =====
// Volume register: 0x00 = 0 dB, 0xFF = full mute. Linear attenuation.
// To convert 0-100% to register: reg = (uint8_t)((100 - percent) * 255 / 100)
// but clipped so 100% -> 0x00 and 0% -> 0xFF.
#define ES9038Q2M_VOL_0DB                0x00  // Full volume (0 dB attenuation)
#define ES9038Q2M_VOL_MUTE               0xFF  // Full mute (-127.5 dB attenuation)
#define ES9038Q2M_VOL_50PCT              0x80  // ~50% volume (-64 dB attenuation)

// ===== Filter Preset Names (bits[4:2] in REG_FILTER_MUTE) =====
// 0 = Fast Roll-Off Linear Phase
// 1 = Slow Roll-Off Linear Phase
// 2 = Minimum Phase Fast Roll-Off
// 3 = Minimum Phase Slow Roll-Off
// 4 = Apodizing Fast Roll-Off Linear Phase
// 5 = Corrected Minimum Phase Fast Roll-Off
// 6 = Brick Wall
// 7 = HB2
#define ES9038Q2M_FILTER_FAST_LINEAR      0x00  // bits[4:2] = 0b000
#define ES9038Q2M_FILTER_SLOW_LINEAR      0x04  // bits[4:2] = 0b001
#define ES9038Q2M_FILTER_MIN_FAST         0x08  // bits[4:2] = 0b010
#define ES9038Q2M_FILTER_MIN_SLOW         0x0C  // bits[4:2] = 0b011
#define ES9038Q2M_FILTER_APOD_FAST        0x10  // bits[4:2] = 0b100
#define ES9038Q2M_FILTER_CORR_MIN_FAST    0x14  // bits[4:2] = 0b101
#define ES9038Q2M_FILTER_BRICK_WALL       0x18  // bits[4:2] = 0b110
#define ES9038Q2M_FILTER_HB2              0x1C  // bits[4:2] = 0b111

// ===== Default Init Values =====
// I2S 32-bit Philips/I2S format, slave mode (ESP32 is master), DPLL BW = 3
#define ES9038Q2M_INIT_INPUT_CFG         (ES9038Q2M_I2S_LEN_32BIT | ES9038Q2M_I2S_FMT_PHILIPS)
#define ES9038Q2M_INIT_DPLL_BW           0x03

#endif // ES9038Q2M_REGS_H
