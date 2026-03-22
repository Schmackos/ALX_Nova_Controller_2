#ifndef ES9082_REGS_H
#define ES9082_REGS_H

#include <stdint.h>

// ES9082 — 8-channel HyperStream IV DAC register definitions
// Source: ESS Technology ES9082 datasheet (placeholder chip ID — verify on hardware)
// Architecture: HyperStream IV, 8-channel, 120 dB DNR
// Compatible string: "ess,es9082"
// Package: 48-pin QFN (larger than ES9081; exposes additional GPIO and optional ASP2 interface)
// Supports ASP2 algorithm customization via ESS SIS tool — user-programmable DSP post-processing.
// ASP2 registers are device-private and not exposed here; refer to ESS SIS tool documentation.

// ===== I2C Address =====
// ES9082 default address: 0x48 (ADDR pins LOW).
// All variants sit in the 0x48-0x4E range (separate from ADC 0x40-0x46).
#define ES9082_I2C_ADDR        0x48

// ===== Chip Identification =====
// Register 0xE1 — placeholder value, verify on hardware.
#define ES9082_CHIP_ID         0x82

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9082_I2C_SDA_PIN=N / -D ES9082_I2C_SCL_PIN=N.
#ifndef ES9082_I2C_SDA_PIN
#define ES9082_I2C_SDA_PIN    28
#endif
#ifndef ES9082_I2C_SCL_PIN
#define ES9082_I2C_SCL_PIN    29
#endif

// ===== Register Addresses =====

// ----- System Configuration (0x00) -----
// bit0 = SOFT_RESET (W, self-clearing)
// bit1 = I2C_RESET
// bits[3:2] = CHANNEL_MODE (00=stereo/2ch, 01=8-channel, 10=mono_L, 11=mono_R)
// bit7      = ASP2_RESET (reset the ASP2 DSP core; self-clearing)
#define ES9082_REG_SYS_CONFIG         0x00

// ----- Input Configuration (0x01) -----
// bits[3:2] = SERIAL_MODE (00=I2S, 01=Left-Justified, 10=Right-Justified, 11=TDM/DSP)
// bits[5:4] = BIT_DEPTH   (00=16-bit, 01=24-bit, 10=32-bit, 11=32-bit)
// bit6      = AUTO_DETECT_I2S
#define ES9082_REG_INPUT_CFG          0x01

// ----- Mixing / Channel Mode (0x02) -----
// bits[1:0] = CHANNEL_MODE mirror
// bit4      = CH_SWAP (swap left/right pairs)
#define ES9082_REG_CHANNEL_CFG        0x02

// ----- Automute Time (0x04) -----
// 8-bit: time constant for automatic mute detection; 0x00 = disabled
#define ES9082_REG_AUTOMUTE_TIME      0x04

// ----- Automute Level (0x05) -----
// 8-bit: input level threshold below which automute triggers
#define ES9082_REG_AUTOMUTE_LEVEL     0x05

// ----- De-emphasis / DoP (0x06) -----
// bit0 = DE_EMPHASIS_EN (1=enable de-emphasis filter)
// bit1 = DOP_EN (1=enable DSD-over-PCM detection)
// bit3 = ASP2_BYPASS (1=bypass ASP2 DSP post-processing, 0=ASP2 active if loaded)
#define ES9082_REG_DEEMP_DOP          0x06

// ----- Digital Filter + Mute (0x07) -----
// bits[2:0] = FILTER_SHAPE (0-7 filter presets; HyperStream IV adds hybrid modes at 6-7)
// bit5      = MUTE (1=mute all channels)
#define ES9082_REG_FILTER_MUTE        0x07

// ----- GPIO Configuration (0x08) -----
// ES9082 exposes more GPIO pins than ES9081 (48-QFN); exact bit layout device-specific.
// bits[1:0] = GPIO1_CFG, bits[3:2] = GPIO2_CFG, bits[5:4] = GPIO3_CFG, bits[7:6] = GPIO4_CFG
// CFG values: 00=off, 01=input, 10=output-low, 11=output-high
#define ES9082_REG_GPIO_CFG           0x08

// ----- GPIO Control (0x09) -----
// bit0 = GPIO1_STATE, bit1 = GPIO2_STATE, bit2 = GPIO3_STATE, bit3 = GPIO4_STATE
#define ES9082_REG_GPIO_CTRL          0x09

// ----- Master Mode / Sync (0x0A) -----
// bit7 = MASTER_MODE_EN (1=ES9082 drives BCK/WS, 0=slave to ESP32)
// bit4 = BCK_INVERT
// bit5 = WS_INVERT
#define ES9082_REG_MASTER_MODE        0x0A

// ----- DPLL Bandwidth (0x0C) -----
// bits[3:0] = DPLL_BW — bandwidth 0-15
#define ES9082_REG_DPLL_CFG           0x0C

// ----- THD Compensation Enable (0x0D) -----
// bit0 = THD_COMP_EN (1=enable hardware THD compensation engine)
#define ES9082_REG_THD_COMP_CFG       0x0D

// ----- Soft Start Configuration (0x0E) -----
// Controls analog output soft-start ramp timing
#define ES9082_REG_SOFT_START         0x0E

// ----- Per-Channel Volume (0x0F–0x16, 8 registers) -----
// 8-bit attenuation per channel: 0x00 = 0 dB, 0xFF = -127.5 dB (full mute)
// Step size: 0.5 dB per LSB.
// Note: 0x11-0x14 overlaps with MASTER_TRIM — use one scheme only.
#define ES9082_REG_VOL_CH1            0x0F  // Channel 1 (Left Front)
#define ES9082_REG_VOL_CH2            0x10  // Channel 2 (Right Front)
#define ES9082_REG_VOL_CH3            0x11  // Channel 3 (Left Rear / Center)
#define ES9082_REG_VOL_CH4            0x12  // Channel 4 (Right Rear / LFE)
#define ES9082_REG_VOL_CH5            0x13  // Channel 5
#define ES9082_REG_VOL_CH6            0x14  // Channel 6
#define ES9082_REG_VOL_CH7            0x15  // Channel 7
#define ES9082_REG_VOL_CH8            0x16  // Channel 8

// ----- Master Trim (0x11–0x14, 32-bit) -----
// Global volume trim applied uniformly to all 8 channels.
// Occupies the same address range as VOL_CH3–VOL_CH6 — use per-channel OR master trim, not both.
#define ES9082_REG_MASTER_TRIM_0      0x11  // Byte 0 (LSB)
#define ES9082_REG_MASTER_TRIM_1      0x12  // Byte 1
#define ES9082_REG_MASTER_TRIM_2      0x13  // Byte 2
#define ES9082_REG_MASTER_TRIM_3      0x14  // Byte 3 (MSB)

// ----- Input Select (0x15) -----
#define ES9082_REG_INPUT_SELECT       0x15

// ----- General Config 0 (0x1B) -----
// bit0 = ASRC_DISABLE (1=bypass internal ASRC)
// bit2 = ASP2_EN (1=enable ASP2 DSP core; requires valid SIS firmware loaded)
#define ES9082_REG_GENERAL_CFG0       0x1B

// ----- NCO / Master Mode Clock (0x22–0x25, 32-bit) -----
#define ES9082_REG_NCO_0              0x22  // Byte 0 (LSB)
#define ES9082_REG_NCO_1              0x23  // Byte 1
#define ES9082_REG_NCO_2              0x24  // Byte 2
#define ES9082_REG_NCO_3              0x25  // Byte 3 (MSB)

// ----- General Config 1 (0x27) -----
// bit0 = AMP_SUPPLY_SEL (0=3.3V, 1=5V)
#define ES9082_REG_GENERAL_CFG1       0x27

// ----- Auto Calibration (0x2D) -----
#define ES9082_REG_AUTO_CAL           0x2D

// ----- ASP2 Status (read-only, 0x30) -----
// bit0 = ASP2_INIT_DONE (1=ASP2 firmware loaded and running)
// bit1 = ASP2_ERROR    (1=ASP2 reported an error condition)
// Monitor after enabling ASP2_EN in GENERAL_CFG0 to confirm successful init.
#define ES9082_REG_ASP2_STATUS        0x30

// ----- Chip ID (read-only, 0x40) -----
#define ES9082_REG_CHIP_ID_LOCAL      0x40

// ----- Readback (read-only, 0xE1) -----
#define ES9082_REG_CHIP_ID            0xE1

// ===== Bit Masks =====

// REG_SYS_CONFIG (0x00)
#define ES9082_SOFT_RESET_BIT         0x01  // bit0: write 1 to reset; self-clearing
#define ES9082_CHANNEL_MODE_STEREO    0x00  // bits[3:2] = 00 (stereo / 2-channel)
#define ES9082_CHANNEL_MODE_8CH       0x04  // bits[3:2] = 01 (8-channel TDM)
#define ES9082_CHANNEL_MODE_MONO_L    0x08  // bits[3:2] = 10 (mono, left channel)
#define ES9082_CHANNEL_MODE_MONO_R    0x0C  // bits[3:2] = 11 (mono, right channel)
#define ES9082_ASP2_RESET_BIT         0x80  // bit7: reset ASP2 DSP core; self-clearing

// REG_INPUT_CFG (0x01)
#define ES9082_INPUT_I2S              0x00  // SERIAL_MODE bits[3:2] = 00 (I2S/Philips)
#define ES9082_INPUT_LJ               0x04  // SERIAL_MODE bits[3:2] = 01 (Left-Justified)
#define ES9082_INPUT_RJ               0x08  // SERIAL_MODE bits[3:2] = 10 (Right-Justified)
#define ES9082_INPUT_TDM              0x0C  // SERIAL_MODE bits[3:2] = 11 (TDM/DSP)
#define ES9082_INPUT_16BIT            0x00  // BIT_DEPTH bits[5:4] = 00 (16-bit)
#define ES9082_INPUT_24BIT            0x10  // BIT_DEPTH bits[5:4] = 01 (24-bit)
#define ES9082_INPUT_32BIT            0x20  // BIT_DEPTH bits[5:4] = 10 (32-bit)

// REG_DEEMP_DOP (0x06)
#define ES9082_DEEMP_EN_BIT           0x01  // bit0: enable de-emphasis
#define ES9082_DOP_EN_BIT             0x02  // bit1: enable DSD-over-PCM
#define ES9082_ASP2_BYPASS_BIT        0x08  // bit3: bypass ASP2 DSP post-processing

// REG_FILTER_MUTE (0x07)
#define ES9082_FILTER_MASK            0x07  // bits[2:0] = FILTER_SHAPE (3 bits)
#define ES9082_MUTE_BIT               0x20  // bit5: mute all channels

// REG_MASTER_MODE (0x0A)
#define ES9082_MASTER_MODE_EN         0x80  // bit7: enable master mode
#define ES9082_SLAVE_MODE             0x00  // I2S slave (receive clocks from ESP32)
#define ES9082_BCK_INVERT_BIT         0x10  // bit4: invert bit clock polarity
#define ES9082_WS_INVERT_BIT          0x20  // bit5: invert word select polarity

// REG_DPLL_CFG (0x0C)
#define ES9082_DPLL_MASK              0x0F  // bits[3:0]: 16 bandwidth levels
#define ES9082_DPLL_BW_LOW            0x02  // Low bandwidth (low-jitter sources)
#define ES9082_DPLL_BW_MED            0x05  // Medium bandwidth (general purpose)
#define ES9082_DPLL_BW_HIGH           0x0A  // High bandwidth (noisy sources)

// REG_THD_COMP_CFG (0x0D)
#define ES9082_THD_COMP_EN_BIT        0x01  // bit0: enable THD compensation engine

// REG_GENERAL_CFG0 (0x1B)
#define ES9082_ASRC_DISABLE_BIT       0x01  // bit0: disable ASRC
#define ES9082_ASP2_EN_BIT            0x04  // bit2: enable ASP2 DSP core

// REG_ASP2_STATUS (0x30)
#define ES9082_ASP2_INIT_DONE_BIT     0x01  // bit0: ASP2 initialized and running
#define ES9082_ASP2_ERROR_BIT         0x02  // bit1: ASP2 error condition

// ===== Volume Constants =====
// Volume registers: 0x00 = 0 dB attenuation, 0xFF = -127.5 dB (full mute)
// Step size: 0.5 dB per LSB.
#define ES9082_VOL_0DB                0x00  // Full volume (0 dB attenuation)
#define ES9082_VOL_MUTE               0xFF  // Full mute (-127.5 dB attenuation)
#define ES9082_VOL_MINUS6DB           0x0C  // -6 dB (12 steps)
#define ES9082_VOL_MINUS20DB          0x28  // -20 dB (40 steps)

// ===== Filter Preset Names (bits[2:0] in REG_FILTER_MUTE) =====
// ES9082 HyperStream IV filter shapes:
// 0 = Fast Roll-Off Linear Phase
// 1 = Slow Roll-Off Linear Phase
// 2 = Minimum Phase Fast Roll-Off
// 3 = Minimum Phase Slow Roll-Off
// 4 = Apodizing Fast Roll-Off Linear Phase
// 5 = Corrected Minimum Phase Fast Roll-Off
// 6 = Hybrid Minimum Phase (HyperStream IV)
// 7 = Hybrid Linear Phase (HyperStream IV)
#define ES9082_FILTER_FAST_LINEAR     0x00  // bits[2:0] = 0b000
#define ES9082_FILTER_SLOW_LINEAR     0x01  // bits[2:0] = 0b001
#define ES9082_FILTER_MIN_FAST        0x02  // bits[2:0] = 0b010
#define ES9082_FILTER_MIN_SLOW        0x03  // bits[2:0] = 0b011
#define ES9082_FILTER_APOD_FAST       0x04  // bits[2:0] = 0b100
#define ES9082_FILTER_CORR_MIN_FAST   0x05  // bits[2:0] = 0b101
#define ES9082_FILTER_HYBRID_MIN      0x06  // bits[2:0] = 0b110 (HyperStream IV)
#define ES9082_FILTER_HYBRID_LINEAR   0x07  // bits[2:0] = 0b111 (HyperStream IV)

// ===== Default Init Values =====
// I2S 32-bit Philips/I2S format, slave mode (ESP32 is master), DPLL BW = 5, ASP2 bypassed
#define ES9082_INIT_INPUT_CFG         (ES9082_INPUT_I2S | ES9082_INPUT_32BIT)
#define ES9082_INIT_DPLL_BW           0x05

#endif // ES9082_REGS_H
