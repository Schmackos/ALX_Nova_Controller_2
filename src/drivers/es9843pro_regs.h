#ifndef ES9843PRO_REGS_H
#define ES9843PRO_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR1=LOW, ADDR2=LOW → 0x40 (default)
// ADDR1=HIGH, ADDR2=LOW → 0x42
// ADDR1=LOW,  ADDR2=HIGH → 0x44
// ADDR1=HIGH, ADDR2=HIGH → 0x46
// No synchronous slave interface on ES9843PRO.
#define ES9843PRO_I2C_ADDR        0x40

// ===== ADC Device ID (for EEPROM identification) =====
// Chip ID register 0xE1 always reads 0x8F on ES9843PRO.
#define ES9843PRO_CHIP_ID         0x8F

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9843PRO_I2C_SDA_PIN=N / -D ES9843PRO_I2C_SCL_PIN=N.
#ifndef ES9843PRO_I2C_SDA_PIN
#define ES9843PRO_I2C_SDA_PIN     28
#endif
#ifndef ES9843PRO_I2C_SCL_PIN
#define ES9843PRO_I2C_SCL_PIN     29
#endif

// ===== Register Addresses =====

// ----- System Registers (0x00-0x03) -----
#define ES9843PRO_REG_SYS_CONFIG          0x00  // bit7=SOFT_RESET(W), bit5=EN_MCLK_IN, bits3:0=ENABLE_ADC_CH4..CH1
#define ES9843PRO_REG_FS_CONFIG           0x01  // bit6=AUTO_FS_DETECT, bit5=STEREO_MODE, bit4=MONO_MODE
#define ES9843PRO_REG_ENCODER_ENABLE      0x02  // TDM/I2S/DSD encoder enable bits
#define ES9843PRO_REG_OUTPUT_FORMAT       0x03  // bits6:4=OUTPUT_SEL (0=I2S,1=TDM,2=DSD,3=DoP,4=PDM,5=SPDIF,6=RAW)

// ----- Master Mode Registers (0x06, 0x08) -----
#define ES9843PRO_REG_MASTER_DCLK_DIV    0x06  // MCLK to BCK divider ratio
#define ES9843PRO_REG_MASTER_CONFIG       0x08  // bit1=DSD_MASTER_EN, bit0=MASTER_MODE_EN

// ----- TDM Registers (0x09-0x13) -----
#define ES9843PRO_REG_TDM_CONFIG1         0x09  // bit6=AUTO_CH_DETECT, bits4:0=TDM_CH_NUM
#define ES9843PRO_REG_TDM_CONFIG2         0x0A  // bit6=NOISE_SHAPE_16BIT, bits5:4=TDM_WORD_WIDTH, bits3:2=TDM_BIT_DEPTH
#define ES9843PRO_REG_TDM_ENC_CH1_SLOT   0x0B  // Encoder output slot assignment for CH1
#define ES9843PRO_REG_TDM_ENC_CH2_SLOT   0x0C  // Encoder output slot assignment for CH2
#define ES9843PRO_REG_TDM_ENC_CH3_SLOT   0x0D  // Encoder output slot assignment for CH3
#define ES9843PRO_REG_TDM_ENC_CH4_SLOT   0x0E  // Encoder output slot assignment for CH4
#define ES9843PRO_REG_TDM_DAISY_CHAIN    0x0F  // TDM daisy-chain configuration
#define ES9843PRO_REG_TDM_DEC_CH1_SLOT   0x10  // Decoder input slot assignment for CH1
#define ES9843PRO_REG_TDM_DEC_CH2_SLOT   0x11  // Decoder input slot assignment for CH2
#define ES9843PRO_REG_TDM_DEC_CH3_SLOT   0x12  // Decoder input slot assignment for CH3
#define ES9843PRO_REG_TDM_DEC_CH4_SLOT   0x13  // Decoder input slot assignment for CH4

// ----- GPIO System (0x2F-0x3E, 11 GPIOs) -----
#define ES9843PRO_REG_GPIO0_CFG           0x2F  // GPIO0 direction and function
#define ES9843PRO_REG_GPIO1_CFG           0x30  // GPIO1 direction and function
#define ES9843PRO_REG_GPIO2_CFG           0x31  // GPIO2 direction and function
#define ES9843PRO_REG_GPIO3_CFG           0x32  // GPIO3 direction and function
#define ES9843PRO_REG_GPIO4_CFG           0x33  // GPIO4 direction and function
#define ES9843PRO_REG_GPIO5_CFG           0x34  // GPIO5 direction and function
#define ES9843PRO_REG_GPIO6_CFG           0x35  // GPIO6 direction and function
#define ES9843PRO_REG_GPIO7_CFG           0x36  // GPIO7 direction and function
#define ES9843PRO_REG_GPIO8_CFG           0x37  // GPIO8 direction and function
#define ES9843PRO_REG_GPIO9_CFG           0x38  // GPIO9 direction and function
#define ES9843PRO_REG_GPIO10_CFG          0x39  // GPIO10 direction and function
#define ES9843PRO_REG_GPIO_DATA           0x3A  // GPIO read/write data register
#define ES9843PRO_REG_GPIO_PULLUP         0x3B  // GPIO pull-up enable bits
#define ES9843PRO_REG_GPIO_PULLDOWN       0x3C  // GPIO pull-down enable bits
#define ES9843PRO_REG_GPIO_OD             0x3D  // GPIO open-drain enable bits
#define ES9843PRO_REG_GPIO_INVERT         0x3E  // GPIO output inversion bits

// ----- PWM Registers (0x40-0x48, 3 channels) -----
#define ES9843PRO_REG_PWM0_COUNT          0x40  // PWM channel 0 duty count
#define ES9843PRO_REG_PWM1_COUNT          0x41  // PWM channel 1 duty count
#define ES9843PRO_REG_PWM2_COUNT          0x42  // PWM channel 2 duty count
#define ES9843PRO_REG_PWM0_FREQ           0x43  // PWM channel 0 frequency divider
#define ES9843PRO_REG_PWM1_FREQ           0x44  // PWM channel 1 frequency divider
#define ES9843PRO_REG_PWM2_FREQ           0x45  // PWM channel 2 frequency divider
#define ES9843PRO_REG_PWM_ENABLE          0x46  // bits2:0 = enable PWM2..PWM0
#define ES9843PRO_REG_PWM_INVERT          0x47  // bits2:0 = invert PWM2..PWM0 output polarity
#define ES9843PRO_REG_PWM_CONFIG          0x48  // PWM mode configuration

// ----- Detection Flags (0x3F) -----
#define ES9843PRO_REG_DETECTION_FLAGS     0x3F  // bits3:0 = per-channel signal detected (CH4..CH1)

// ----- Filter / HPF / ADC Digital Config (0x49-0x4C) -----
#define ES9843PRO_REG_ADC_NEG_SEL         0x49  // bits3:0 = CH4..CH1 input negation enable
#define ES9843PRO_REG_FILTER_CONFIG       0x4A  // bits7:5=FILTER_SHAPE(0-7), bit4=BYPASS_FIR2X, bit3=BYPASS_FIR4X, bit2=BYPASS_IIR
#define ES9843PRO_REG_DC_BLOCK            0x4C  // bits15:12=DC_BLOCK_EN_CH4..CH1; lower bits FIR coeff control (16-bit, LSB=0x4C, MSB=0x4D)
#define ES9843PRO_REG_DC_BLOCK_MSB        0x4D  // DC_BLOCK MSB

// ----- Volume Registers (0x51-0x54) — 8-bit per channel -----
#define ES9843PRO_REG_CH1_VOLUME          0x51  // 0x00=0dB, 0xFE=-127dB (0.5dB/step), 0xFF=mute
#define ES9843PRO_REG_CH2_VOLUME          0x52  // same format as CH1_VOLUME
#define ES9843PRO_REG_CH3_VOLUME          0x53  // same format as CH1_VOLUME
#define ES9843PRO_REG_CH4_VOLUME          0x54  // same format as CH1_VOLUME

// ----- Gain Registers (0x55-0x56) — 3-bit packed per channel -----
#define ES9843PRO_REG_GAIN_PAIR1          0x55  // bits5:3=CH2_GAIN, bits2:0=CH1_GAIN (0=0dB..7=+42dB, 6dB steps)
#define ES9843PRO_REG_GAIN_PAIR2          0x56  // bit7=MONO_VOL_MODE, bits5:3=CH4_GAIN, bits2:0=CH3_GAIN

// ----- Phase Inversion (0x57) -----
#define ES9843PRO_REG_PHASE_INVERSION     0x57  // bits3:0 = CH4..CH1 phase invert enable

// ----- Volume Ramp (0x58-0x59) -----
#define ES9843PRO_REG_VOL_RAMP_UP         0x58  // Volume ramp-up rate (0x00=slowest, 0xFF=fastest)
#define ES9843PRO_REG_VOL_RAMP_DOWN       0x59  // Volume ramp-down rate (0x00=slowest, 0xFF=fastest)

// ----- THD Compensation (0x5A-0x61, per channel pair) -----
#define ES9843PRO_REG_THD_C2_PAIR1_LSB   0x5A  // CH1/CH2 2nd harmonic compensation LSB
#define ES9843PRO_REG_THD_C2_PAIR1_MSB   0x5B  // CH1/CH2 2nd harmonic compensation MSB
#define ES9843PRO_REG_THD_C3_PAIR1_LSB   0x5C  // CH1/CH2 3rd harmonic compensation LSB
#define ES9843PRO_REG_THD_C3_PAIR1_MSB   0x5D  // CH1/CH2 3rd harmonic compensation MSB
#define ES9843PRO_REG_THD_C2_PAIR2_LSB   0x5E  // CH3/CH4 2nd harmonic compensation LSB
#define ES9843PRO_REG_THD_C2_PAIR2_MSB   0x5F  // CH3/CH4 2nd harmonic compensation MSB
#define ES9843PRO_REG_THD_C3_PAIR2_LSB   0x60  // CH3/CH4 3rd harmonic compensation LSB
#define ES9843PRO_REG_THD_C3_PAIR2_MSB   0x61  // CH3/CH4 3rd harmonic compensation MSB

// ----- Peak Detection (0x62-0x67) -----
#define ES9843PRO_REG_PEAK_DETECT_EN      0x62  // bits3:0 = CH4..CH1 peak detector enable
#define ES9843PRO_REG_PEAK_DETECT_CFG     0x63  // bit7=LOCK_PEAK, bits6:2=DECAY_RATE
#define ES9843PRO_REG_PEAK_THRESH_CH1     0x64  // 8-bit peak threshold for CH1
#define ES9843PRO_REG_PEAK_THRESH_CH2     0x65  // 8-bit peak threshold for CH2
#define ES9843PRO_REG_PEAK_THRESH_CH3     0x66  // 8-bit peak threshold for CH3
#define ES9843PRO_REG_PEAK_THRESH_CH4     0x67  // 8-bit peak threshold for CH4

// ----- ASP2 (platform uses own DSP — leave disabled) -----
#define ES9843PRO_REG_ASP_CONTROL         0x6B  // bit0=ASP_CORE_EN (keep 0 — platform has external DSP)
#define ES9843PRO_REG_ASP_BYPASS          0x6C  // bits3:0 = bypass per channel (set all to 0x0F)

// ----- Readback Registers (read-only, 0xE0-0xF3) -----
#define ES9843PRO_REG_CODEC_VALIDITY      0xE0  // TDM_ENC_VALID, TDM_DEC_VALID, and other validity flags
#define ES9843PRO_REG_CHIP_ID             0xE1  // Chip ID — reads 0x8F on ES9843PRO
#define ES9843PRO_REG_AUTO_FS_READBACK    0xE6  // Detected sample rate (AUTO_FS_DETECT result)
#define ES9843PRO_REG_CLK_VALIDITY        0xE7  // Clock validity flags
#define ES9843PRO_REG_OVERLOAD_FLAGS      0xEA  // bits3:0 = per-channel overload detected (CH4..CH1)
#define ES9843PRO_REG_PEAK_FLAGS          0xEB  // bits3:0 = per-channel peak detected (CH4..CH1)
#define ES9843PRO_REG_PEAK_LEVEL_CH1_LSB  0xEC  // CH1 peak level 16-bit LSB
#define ES9843PRO_REG_PEAK_LEVEL_CH1_MSB  0xED  // CH1 peak level 16-bit MSB
#define ES9843PRO_REG_PEAK_LEVEL_CH2_LSB  0xEE  // CH2 peak level 16-bit LSB
#define ES9843PRO_REG_PEAK_LEVEL_CH2_MSB  0xEF  // CH2 peak level 16-bit MSB
#define ES9843PRO_REG_PEAK_LEVEL_CH3_LSB  0xF0  // CH3 peak level 16-bit LSB
#define ES9843PRO_REG_PEAK_LEVEL_CH3_MSB  0xF1  // CH3 peak level 16-bit MSB
#define ES9843PRO_REG_PEAK_LEVEL_CH4_LSB  0xF2  // CH4 peak level 16-bit LSB
#define ES9843PRO_REG_PEAK_LEVEL_CH4_MSB  0xF3  // CH4 peak level 16-bit MSB

// ===== Bit Masks =====

// REG_SYS_CONFIG (0x00)
#define ES9843PRO_SOFT_RESET_BIT          0x80  // Write 1 to reset; self-clearing
#define ES9843PRO_EN_MCLK_IN              0x20  // bit5: enable MCLK input
#define ES9843PRO_ENABLE_CH1              0x01  // bit0: enable ADC channel 1
#define ES9843PRO_ENABLE_CH2              0x02  // bit1: enable ADC channel 2
#define ES9843PRO_ENABLE_CH3              0x04  // bit2: enable ADC channel 3
#define ES9843PRO_ENABLE_CH4              0x08  // bit3: enable ADC channel 4

// REG_FS_CONFIG (0x01)
#define ES9843PRO_AUTO_FS_DETECT          0x40  // bit6: auto sample-rate detection
#define ES9843PRO_STEREO_MODE             0x20  // bit5: stereo channel pairing mode
#define ES9843PRO_MONO_MODE               0x10  // bit4: mono (sum both inputs) mode

// REG_OUTPUT_FORMAT (0x03) — OUTPUT_SEL bits6:4
#define ES9843PRO_OUTPUT_I2S              0x00  // OUTPUT_SEL = 0b000 → I2S
#define ES9843PRO_OUTPUT_TDM              0x10  // OUTPUT_SEL = 0b001 → TDM
#define ES9843PRO_OUTPUT_DSD              0x20  // OUTPUT_SEL = 0b010 → DSD
#define ES9843PRO_OUTPUT_DOP              0x30  // OUTPUT_SEL = 0b011 → DoP
#define ES9843PRO_OUTPUT_PDM              0x40  // OUTPUT_SEL = 0b100 → PDM
#define ES9843PRO_OUTPUT_SPDIF            0x50  // OUTPUT_SEL = 0b101 → S/PDIF
#define ES9843PRO_OUTPUT_RAW              0x60  // OUTPUT_SEL = 0b110 → RAW

// REG_MASTER_CONFIG (0x08)
#define ES9843PRO_MASTER_MODE_BIT         0x01  // bit0: MASTER_MODE_EN
#define ES9843PRO_DSD_MASTER_BIT          0x02  // bit1: DSD_MASTER_EN

// REG_FILTER_CONFIG (0x4A) — FILTER_SHAPE field (bits7:5)
// Filter shape preset names (0-7):
//   0 = Minimum phase
//   1 = Linear apodizing fast
//   2 = Linear fast
//   3 = Linear fast low ripple
//   4 = Linear slow
//   5 = Minimum fast
//   6 = Minimum slow
//   7 = Minimum slow low dispersion
#define ES9843PRO_FILTER_MIN_PHASE        0x00  // bits7:5 = 0b000
#define ES9843PRO_FILTER_LIN_APO_FAST     0x20  // bits7:5 = 0b001
#define ES9843PRO_FILTER_LIN_FAST         0x40  // bits7:5 = 0b010
#define ES9843PRO_FILTER_LIN_FAST_LR      0x60  // bits7:5 = 0b011
#define ES9843PRO_FILTER_LIN_SLOW         0x80  // bits7:5 = 0b100
#define ES9843PRO_FILTER_MIN_FAST         0xA0  // bits7:5 = 0b101
#define ES9843PRO_FILTER_MIN_SLOW         0xC0  // bits7:5 = 0b110
#define ES9843PRO_FILTER_MIN_SLOW_LOW_DISP 0xE0 // bits7:5 = 0b111
#define ES9843PRO_BYPASS_FIR2X            0x10  // bit4: bypass 2x FIR decimation filter
#define ES9843PRO_BYPASS_FIR4X            0x08  // bit3: bypass 4x FIR decimation filter
#define ES9843PRO_BYPASS_IIR              0x04  // bit2: bypass IIR filter stage

// REG_GAIN_PAIR1 / REG_GAIN_PAIR2 — per-channel 3-bit gain field (6 dB steps)
#define ES9843PRO_GAIN_0DB                0x00  // 0 dB
#define ES9843PRO_GAIN_6DB                0x01  // +6 dB
#define ES9843PRO_GAIN_12DB               0x02  // +12 dB
#define ES9843PRO_GAIN_18DB               0x03  // +18 dB
#define ES9843PRO_GAIN_24DB               0x04  // +24 dB
#define ES9843PRO_GAIN_30DB               0x05  // +30 dB
#define ES9843PRO_GAIN_36DB               0x06  // +36 dB
#define ES9843PRO_GAIN_42DB               0x07  // +42 dB
#define ES9843PRO_MONO_VOL_MODE           0x80  // REG_GAIN_PAIR2 bit7: single volume control for all channels

// REG_PHASE_INVERSION (0x57)
#define ES9843PRO_PHASE_INV_CH1           0x01  // bit0: invert CH1 phase
#define ES9843PRO_PHASE_INV_CH2           0x02  // bit1: invert CH2 phase
#define ES9843PRO_PHASE_INV_CH3           0x04  // bit2: invert CH3 phase
#define ES9843PRO_PHASE_INV_CH4           0x08  // bit3: invert CH4 phase

// ===== Optimal Init Values =====

// Channel enable presets for REG_SYS_CONFIG bits3:0
#define ES9843PRO_ENABLE_2CH              0x03  // CH1 + CH2 only (stereo pair)
#define ES9843PRO_ENABLE_4CH              0x0F  // All 4 channels enabled

// ASP bypass — set all channel bypass bits; platform uses external DSP pipeline
#define ES9843PRO_ASP_BYPASS_ALL          0x0F  // Write to REG_ASP_BYPASS (0x6C)

// Soft reset command — write to REG_SYS_CONFIG (0x00); chip self-clears the bit
#define ES9843PRO_SOFT_RESET_CMD          0xA0  // SOFT_RESET(bit7)=1 + EN_MCLK_IN(bit5)=1

// ===== Volume Constants =====
#define ES9843PRO_VOL_0DB                 0x00  // 8-bit: 0 dB (unity gain)
#define ES9843PRO_VOL_MUTE                0xFF  // 8-bit: mute

#endif // ES9843PRO_REGS_H
