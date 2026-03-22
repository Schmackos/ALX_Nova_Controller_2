#ifndef ES9038PRO_REGS_H
#define ES9038PRO_REGS_H

#include <stdint.h>

// ES9038PRO — 8-channel HyperStream II flagship DAC register definitions
// Source: ESS Technology ES9038PRO datasheet (placeholder chip ID — verify on hardware)
// Architecture: HyperStream II, 8-channel, 132 dB DNR
// Compatible string: "ess,es9038pro"

// ===== I2C Address =====
// ES9038PRO DAC default address: 0x48 (ADDR pins LOW).
// All variants sit in the 0x48-0x4E range (separate from ADC 0x40-0x46).
#define ES9038PRO_I2C_ADDR        0x48

// ===== Chip Identification =====
// Register 0xE1 — placeholder value, verify on hardware.
#define ES9038PRO_CHIP_ID         0x03

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9038PRO_I2C_SDA_PIN=N / -D ES9038PRO_I2C_SCL_PIN=N.
#ifndef ES9038PRO_I2C_SDA_PIN
#define ES9038PRO_I2C_SDA_PIN    28
#endif
#ifndef ES9038PRO_I2C_SCL_PIN
#define ES9038PRO_I2C_SCL_PIN    29
#endif

// ===== Register Addresses =====

// ----- System Configuration (0x00) -----
// bit0 = SOFT_RESET (W, self-clearing)
// bit1 = I2C_RESET
// bits[3:2] = CHANNEL_MODE (00=stereo/2ch, 01=8-channel, 10=mono_L, 11=mono_R)
#define ES9038PRO_REG_SYS_CONFIG         0x00

// ----- Input Configuration (0x01) -----
// bits[3:2] = SERIAL_MODE (00=I2S, 01=Left-Justified, 10=Right-Justified, 11=TDM/DSP)
// bits[5:4] = BIT_DEPTH   (00=16-bit, 01=24-bit, 10=32-bit, 11=32-bit)
// bit6      = AUTO_DETECT_I2S
#define ES9038PRO_REG_INPUT_CFG          0x01

// ----- Mixing / Channel Mode (0x02) -----
// bits[1:0] = CHANNEL_MODE mirror (same as bits[3:2] in SYS_CONFIG on some revisions)
// bit4      = CH_SWAP (swap left/right pairs)
#define ES9038PRO_REG_CHANNEL_CFG        0x02

// ----- Automute Time (0x04) -----
// 8-bit: time constant for automatic mute detection; 0x00 = disabled
#define ES9038PRO_REG_AUTOMUTE_TIME      0x04

// ----- Automute Level (0x05) -----
// 8-bit: input level threshold below which automute triggers
#define ES9038PRO_REG_AUTOMUTE_LEVEL     0x05

// ----- De-emphasis / DoP (0x06) -----
// bit0 = DE_EMPHASIS_EN (1=enable de-emphasis filter)
// bit1 = DOP_EN (1=enable DSD-over-PCM detection)
#define ES9038PRO_REG_DEEMP_DOP          0x06

// ----- Digital Filter + Mute (0x07) -----
// bits[2:0] = FILTER_SHAPE (0-7 filter presets)
// bit5      = MUTE (1=mute all channels)
#define ES9038PRO_REG_FILTER_MUTE        0x07

// ----- GPIO Configuration (0x08) -----
// Multi-function GPIO control (exact bit layout device-specific)
#define ES9038PRO_REG_GPIO_CFG           0x08

// ----- Master Mode / Sync (0x0A) -----
// bit7 = MASTER_MODE_EN (1=ES9038PRO drives BCK/WS, 0=slave to ESP32)
// bit4 = BCK_INVERT
// bit5 = WS_INVERT
#define ES9038PRO_REG_MASTER_MODE        0x0A

// ----- DPLL Bandwidth (0x0C) -----
// bits[3:0] = DPLL_BW — bandwidth 0-15 (higher = more jitter rejection, less selective)
#define ES9038PRO_REG_DPLL_CFG           0x0C

// ----- THD Compensation Enable (0x0D) -----
// bit0 = THD_COMP_EN (1=enable hardware THD compensation engine)
#define ES9038PRO_REG_THD_COMP_CFG       0x0D

// ----- Soft Start Configuration (0x0E) -----
// Controls analog output soft-start ramp timing
#define ES9038PRO_REG_SOFT_START         0x0E

// ----- Per-Channel Volume (0x0F–0x16, 8 registers) -----
// 8-bit attenuation per channel: 0x00 = 0 dB, 0xFF = -127.5 dB (full mute)
// Step size: 0.5 dB per LSB. Note: these registers overlap with MASTER_TRIM (0x11-0x14);
// use one scheme or the other — do not mix per-channel volume and master trim simultaneously.
#define ES9038PRO_REG_VOL_CH1            0x0F  // Channel 1 (Left Front)
#define ES9038PRO_REG_VOL_CH2            0x10  // Channel 2 (Right Front)
#define ES9038PRO_REG_VOL_CH3            0x11  // Channel 3 (Left Rear / Center)
#define ES9038PRO_REG_VOL_CH4            0x12  // Channel 4 (Right Rear / LFE)
#define ES9038PRO_REG_VOL_CH5            0x13  // Channel 5
#define ES9038PRO_REG_VOL_CH6            0x14  // Channel 6
#define ES9038PRO_REG_VOL_CH7            0x15  // Channel 7
#define ES9038PRO_REG_VOL_CH8            0x16  // Channel 8

// ----- Master Trim (0x11–0x14, 32-bit) -----
// Global volume trim applied uniformly to all 8 channels.
// Occupies the same address range as VOL_CH3–VOL_CH6 — use per-channel OR master trim, not both.
// Write LSB first; trim value is applied after per-channel attenuation.
#define ES9038PRO_REG_MASTER_TRIM_0      0x11  // Byte 0 (LSB)
#define ES9038PRO_REG_MASTER_TRIM_1      0x12  // Byte 1
#define ES9038PRO_REG_MASTER_TRIM_2      0x13  // Byte 2
#define ES9038PRO_REG_MASTER_TRIM_3      0x14  // Byte 3 (MSB)

// ----- Input Select (0x15) -----
// Selects active input source when multiple inputs are available
#define ES9038PRO_REG_INPUT_SELECT       0x15

// ----- General Config 0 (0x1B) -----
// bit0 = ASRC_DISABLE (1=bypass internal ASRC; use only when source sample rate is stable)
#define ES9038PRO_REG_GENERAL_CFG0       0x1B

// ----- NCO / Master Mode Clock (0x22–0x25, 32-bit) -----
// Numerically-controlled oscillator for master mode output clock generation.
// Write all 4 bytes; setting takes effect on the next sample-rate transition.
#define ES9038PRO_REG_NCO_0              0x22  // Byte 0 (LSB)
#define ES9038PRO_REG_NCO_1              0x23  // Byte 1
#define ES9038PRO_REG_NCO_2              0x24  // Byte 2
#define ES9038PRO_REG_NCO_3              0x25  // Byte 3 (MSB)

// ----- General Config 1 (0x27) -----
// bit0 = AMP_SUPPLY_SEL (0=3.3V, 1=5V analog supply routing)
#define ES9038PRO_REG_GENERAL_CFG1       0x27

// ----- THD Compensation Coefficients (0x16–0x19) -----
// ES9038PRO extended registers for second- and third-order harmonic correction.
// C2 (second harmonic) and C3 (third harmonic) are 16-bit signed values.
#define ES9038PRO_REG_THD_COMP_C2_LSB    0x16  // C2 coefficient LSB
#define ES9038PRO_REG_THD_COMP_C2_MSB    0x17  // C2 coefficient MSB
#define ES9038PRO_REG_THD_COMP_C3_LSB    0x18  // C3 coefficient LSB
#define ES9038PRO_REG_THD_COMP_C3_MSB    0x19  // C3 coefficient MSB

// ----- Auto Calibration (0x2D) -----
// Triggers internal calibration cycle for offset and gain; self-clearing on completion
#define ES9038PRO_REG_AUTO_CAL           0x2D

// ----- Chip ID (read-only, 0x40) -----
// Returns ES9038PRO_CHIP_ID; used for runtime identity verification
#define ES9038PRO_REG_CHIP_ID_LOCAL      0x40

// ----- Readback (read-only, 0xE1) -----
// ESS family-wide chip ID register address (shared across all ESS SABRE devices)
#define ES9038PRO_REG_CHIP_ID            0xE1

// ===== Bit Masks =====

// REG_SYS_CONFIG (0x00)
#define ES9038PRO_SOFT_RESET_BIT         0x01  // bit0: write 1 to reset; self-clearing
#define ES9038PRO_CHANNEL_MODE_STEREO    0x00  // bits[3:2] = 00 (stereo / 2-channel)
#define ES9038PRO_CHANNEL_MODE_8CH       0x04  // bits[3:2] = 01 (8-channel TDM)
#define ES9038PRO_CHANNEL_MODE_MONO_L    0x08  // bits[3:2] = 10 (mono, left channel)
#define ES9038PRO_CHANNEL_MODE_MONO_R    0x0C  // bits[3:2] = 11 (mono, right channel)

// REG_INPUT_CFG (0x01)
#define ES9038PRO_INPUT_I2S              0x00  // SERIAL_MODE bits[3:2] = 00 (I2S/Philips)
#define ES9038PRO_INPUT_LJ               0x04  // SERIAL_MODE bits[3:2] = 01 (Left-Justified)
#define ES9038PRO_INPUT_RJ               0x08  // SERIAL_MODE bits[3:2] = 10 (Right-Justified)
#define ES9038PRO_INPUT_TDM              0x0C  // SERIAL_MODE bits[3:2] = 11 (TDM/DSP)
#define ES9038PRO_INPUT_16BIT            0x00  // BIT_DEPTH bits[5:4] = 00 (16-bit)
#define ES9038PRO_INPUT_24BIT            0x10  // BIT_DEPTH bits[5:4] = 01 (24-bit)
#define ES9038PRO_INPUT_32BIT            0x20  // BIT_DEPTH bits[5:4] = 10 (32-bit)

// REG_FILTER_MUTE (0x07)
#define ES9038PRO_FILTER_MASK            0x07  // bits[2:0] = FILTER_SHAPE (3 bits)
#define ES9038PRO_MUTE_BIT               0x20  // bit5: mute all channels

// REG_MASTER_MODE (0x0A)
#define ES9038PRO_MASTER_MODE_EN         0x80  // bit7: enable master mode (ES9038PRO drives clocks)
#define ES9038PRO_SLAVE_MODE             0x00  // I2S slave (receive clocks from ESP32)
#define ES9038PRO_BCK_INVERT_BIT         0x10  // bit4: invert bit clock polarity
#define ES9038PRO_WS_INVERT_BIT          0x20  // bit5: invert word select polarity

// REG_DPLL_CFG (0x0C)
#define ES9038PRO_DPLL_MASK              0x0F  // bits[3:0]: 16 bandwidth levels
#define ES9038PRO_DPLL_BW_LOW            0x02  // Low bandwidth (good for low-jitter sources)
#define ES9038PRO_DPLL_BW_MED            0x05  // Medium bandwidth (general purpose)
#define ES9038PRO_DPLL_BW_HIGH           0x0A  // High bandwidth (noisy/variable sources)

// REG_THD_COMP_CFG (0x0D)
#define ES9038PRO_THD_COMP_EN_BIT        0x01  // bit0: enable THD compensation engine

// REG_GENERAL_CFG0 (0x1B)
#define ES9038PRO_ASRC_DISABLE_BIT       0x01  // bit0: disable ASRC (use with stable source clock)

// ===== Volume Constants =====
// Volume registers: 0x00 = 0 dB attenuation, 0xFF = -127.5 dB (full mute)
// Step size: 0.5 dB per LSB. 256 steps total.
#define ES9038PRO_VOL_0DB                0x00  // Full volume (0 dB attenuation)
#define ES9038PRO_VOL_MUTE               0xFF  // Full mute (-127.5 dB attenuation)
#define ES9038PRO_VOL_MINUS6DB           0x0C  // -6 dB (12 steps)
#define ES9038PRO_VOL_MINUS20DB          0x28  // -20 dB (40 steps)

// ===== Filter Preset Names (bits[2:0] in REG_FILTER_MUTE) =====
// ES9038PRO HyperStream II filter shapes:
// 0 = Fast Roll-Off Linear Phase
// 1 = Slow Roll-Off Linear Phase
// 2 = Minimum Phase Fast Roll-Off
// 3 = Minimum Phase Slow Roll-Off
// 4 = Apodizing Fast Roll-Off Linear Phase
// 5 = Corrected Minimum Phase Fast Roll-Off
// 6 = Brick Wall
// 7 = HB2
#define ES9038PRO_FILTER_FAST_LINEAR     0x00  // bits[2:0] = 0b000
#define ES9038PRO_FILTER_SLOW_LINEAR     0x01  // bits[2:0] = 0b001
#define ES9038PRO_FILTER_MIN_FAST        0x02  // bits[2:0] = 0b010
#define ES9038PRO_FILTER_MIN_SLOW        0x03  // bits[2:0] = 0b011
#define ES9038PRO_FILTER_APOD_FAST       0x04  // bits[2:0] = 0b100
#define ES9038PRO_FILTER_CORR_MIN_FAST   0x05  // bits[2:0] = 0b101
#define ES9038PRO_FILTER_BRICK_WALL      0x06  // bits[2:0] = 0b110
#define ES9038PRO_FILTER_HB2             0x07  // bits[2:0] = 0b111

// ===== Default Init Values =====
// I2S 32-bit Philips/I2S format, slave mode (ESP32 is master), DPLL BW = 5
#define ES9038PRO_INIT_INPUT_CFG         (ES9038PRO_INPUT_I2S | ES9038PRO_INPUT_32BIT)
#define ES9038PRO_INIT_DPLL_BW           0x05

#endif // ES9038PRO_REGS_H
