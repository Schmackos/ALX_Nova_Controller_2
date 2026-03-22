#ifndef ES9069Q_REGS_H
#define ES9069Q_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR1=LOW, ADDR2=LOW → 0x48 (default DAC base address)
// ADDR1=HIGH, ADDR2=LOW → 0x4A
// ADDR1=LOW,  ADDR2=HIGH → 0x4C
// ADDR1=HIGH, ADDR2=HIGH → 0x4E
#define ES9069Q_I2C_ADDR        0x48

// ===== Device ID (for EEPROM identification) =====
// Chip ID register 0xE1 always reads 0x94 on ES9069Q.
#define ES9069Q_CHIP_ID         0x94

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D ES9069Q_I2C_SDA_PIN=N / -D ES9069Q_I2C_SCL_PIN=N.
#ifndef ES9069Q_I2C_SDA_PIN
#define ES9069Q_I2C_SDA_PIN     28
#endif
#ifndef ES9069Q_I2C_SCL_PIN
#define ES9069Q_I2C_SCL_PIN     29
#endif

// ===== Register Addresses =====

// ----- System / Control Registers -----
#define ES9069Q_REG_SYSTEM_SETTINGS   0x00  // bits7:6=INPUT_SEL(0=I2S,1=DSD,2=TDM), bit5=BYPASS_MODE, bit1=SOFT_START
#define ES9069Q_REG_INPUT_CONFIG      0x01  // bits7:6=I2S_LEN(0=32,1=24,2=20,3=16), bits5:4=I2S_MODE(0=Philips,1=LJ,2=RJ)
#define ES9069Q_REG_AUTOMUTE_TIME     0x04  // Automute trigger time (0=disable, 1-255 in 0.25s steps)
#define ES9069Q_REG_AUTOMUTE_LEVEL    0x05  // Automute trigger level (0x00 = -108 dBFS, 0xFF = 0 dBFS)
#define ES9069Q_REG_DSD_CONFIG        0x06  // bits3:0=DSD_RATE (0=DSD64, 1=DSD128, 2=DSD256, 3=DSD512, 4=DSD1024)
#define ES9069Q_REG_FILTER_SHAPE      0x07  // bits2:0=FILTER_SHAPE (0-7 filter presets)
#define ES9069Q_REG_GENERAL_CONFIG    0x08  // bit7=DUAL_MONO_MODE, bit6=STEREO_MODE_INV, bit2=BYPASS_DPLL
#define ES9069Q_REG_GPIO_CONFIG       0x09  // bits7:4=GPIO3_CFG, bits3:0=GPIO2_CFG
#define ES9069Q_REG_MASTER_MODE       0x0A  // bit0=MASTER_MODE_ENABLE, bit1=BCK_INVERT, bit2=WS_INVERT
#define ES9069Q_REG_CHANNEL_MAP       0x0B  // bit0=SWAP_LR (swap left/right channels)
#define ES9069Q_REG_DPLL_BANDWIDTH    0x0C  // bits3:0=DPLL_BW (0=lowest, 15=highest), bit7=DPLL_BYPASS
#define ES9069Q_REG_THD_COMP_BYPASS   0x0D  // bit0=BYPASS_THD_COMP
#define ES9069Q_REG_SOFT_START_CONFIG 0x0E  // bits3:0=SOFT_START_RATE

// ----- Volume Registers -----
#define ES9069Q_REG_VOLUME_L          0x0F  // Left channel attenuation: 0x00=0dB, 0xFF=mute (0.5dB/step)
#define ES9069Q_REG_VOLUME_R          0x10  // Right channel attenuation: 0x00=0dB, 0xFF=mute (0.5dB/step)

// ----- MQA Control Register (ES9069Q unique — integrated hardware MQA renderer) -----
// bit0  = MQA_ENABLE       (1 = enable MQA hardware renderer, 0 = disabled)
// bits[3:1] = MQA_STATUS   (read-only: 0b000=no MQA, 0b001=MQA core, 0b010=MQA studio)
// bit4  = MQA_PASSTHROUGH  (1 = pass decoded PCM directly, no additional rendering)
#define ES9069Q_REG_MQA_CONTROL       0x17  // MQA enable + status register

// ----- GPIO/IRQ Registers -----
#define ES9069Q_REG_GPIO_STATUS       0x18  // GPIO readback register (read-only)
#define ES9069Q_REG_IRQ_MASK          0x19  // IRQ mask: bit0=LOCK_IRQ, bit1=AUTOMUTE_IRQ, bit2=MQA_IRQ

// ----- Readback Registers (read-only, 0xE0+) -----
#define ES9069Q_REG_CHIP_ID           0xE1  // Chip ID — reads 0x94 on ES9069Q
#define ES9069Q_REG_DPLL_LOCK         0xE2  // bit0=DPLL_LOCK (1=locked), bit1=AUTOMUTE_ACTIVE
#define ES9069Q_REG_INPUT_DETECT      0xE3  // bits3:0=detected input format
#define ES9069Q_REG_MQA_STATUS_RB     0xE4  // bits2:0=MQA_DECODE_STATUS (mirrors REG_MQA_CONTROL[3:1])

// ===== Bit Masks =====

// REG_SYSTEM_SETTINGS (0x00) — INPUT_SEL bits7:6
#define ES9069Q_INPUT_I2S             0x00  // I2S / left-justified / right-justified
#define ES9069Q_INPUT_DSD             0x40  // DSD native
#define ES9069Q_INPUT_TDM             0x80  // TDM / DSP mode
#define ES9069Q_SOFT_START_BIT        0x02  // Enable soft start on power-up

// REG_INPUT_CONFIG (0x01) — I2S_LEN bits7:6
#define ES9069Q_I2S_LEN_32            0x00  // 32-bit I2S
#define ES9069Q_I2S_LEN_24            0x40  // 24-bit I2S
#define ES9069Q_I2S_LEN_20            0x80  // 20-bit I2S
#define ES9069Q_I2S_LEN_16            0xC0  // 16-bit I2S

// REG_FILTER_SHAPE (0x07) — bits2:0
#define ES9069Q_FILTER_MIN_PHASE      0x00  // Minimum Phase
#define ES9069Q_FILTER_LIN_APO_FAST   0x01  // Linear Apodizing Fast
#define ES9069Q_FILTER_LIN_FAST       0x02  // Linear Fast
#define ES9069Q_FILTER_LIN_FAST_LR    0x03  // Linear Fast Low Ripple
#define ES9069Q_FILTER_LIN_SLOW       0x04  // Linear Slow
#define ES9069Q_FILTER_MIN_FAST       0x05  // Minimum Fast
#define ES9069Q_FILTER_MIN_SLOW       0x06  // Minimum Slow
#define ES9069Q_FILTER_MIN_SLOW_LD    0x07  // Minimum Slow Low Dispersion

// REG_MQA_CONTROL (0x17)
#define ES9069Q_MQA_ENABLE_BIT        0x01  // bit0: enable MQA hardware renderer
#define ES9069Q_MQA_STATUS_MASK       0x0E  // bits[3:1]: MQA decode status
#define ES9069Q_MQA_STATUS_SHIFT      1     // Right-shift to extract status nibble
#define ES9069Q_MQA_PASSTHROUGH_BIT   0x10  // bit4: MQA passthrough mode

// MQA decode status values (after masking + shifting bits[3:1])
#define ES9069Q_MQA_STATUS_NONE       0x00  // No MQA detected
#define ES9069Q_MQA_STATUS_CORE       0x01  // MQA Core (studio quality)
#define ES9069Q_MQA_STATUS_STUDIO     0x02  // MQA Studio (highest quality)

// REG_DPLL_LOCK (0xE2)
#define ES9069Q_DPLL_LOCKED_BIT       0x01  // DPLL locked to incoming clock
#define ES9069Q_AUTOMUTE_ACTIVE_BIT   0x02  // Automute is active

// ===== DSD Rate Constants =====
#define ES9069Q_DSD_RATE_64           0x00  // DSD64   (2.8 MHz)
#define ES9069Q_DSD_RATE_128          0x01  // DSD128  (5.6 MHz)
#define ES9069Q_DSD_RATE_256          0x02  // DSD256  (11.2 MHz)
#define ES9069Q_DSD_RATE_512          0x03  // DSD512  (22.6 MHz)
#define ES9069Q_DSD_RATE_1024         0x04  // DSD1024 (45.2 MHz)

// ===== Volume Constants =====
#define ES9069Q_VOL_0DB               0x00  // 0 dB attenuation (full volume)
#define ES9069Q_VOL_MUTE              0xFF  // Full attenuation (mute)

// ===== Optimal Init Values =====
// DPLL bandwidth: balanced setting (mode 4) suitable for most sources
#define ES9069Q_DPLL_BW_DEFAULT       0x04

#endif // ES9069Q_REGS_H
