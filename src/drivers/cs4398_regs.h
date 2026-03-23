#ifndef CS4398_REGS_H
#define CS4398_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// ADDR pin LOW → 0x4C (CS4398 default — differs from CS43198/CS43131 which use 0x48)
// ADDR pin HIGH → 0x4D (alternate address via ADDR pin)
#define CS4398_I2C_ADDR         0x4C

// ===== Device ID =====
// Register 0x01 (Chip ID / Revision) upper nibble is the chip ID.
// Reads 0x72 on CS4398 silicon (upper nibble 0x7 = CS4398 family).
#define CS4398_CHIP_ID          0x72

// ===== I2C Bus 2 (Expansion) Default Pins =====
// GPIO28=SDA, GPIO29=SCL — always safe (no SDIO conflict).
// Override at build time via -D CS4398_I2C_SDA_PIN=N / -D CS4398_I2C_SCL_PIN=N.
#ifndef CS4398_I2C_SDA_PIN
#define CS4398_I2C_SDA_PIN      28
#endif
#ifndef CS4398_I2C_SCL_PIN
#define CS4398_I2C_SCL_PIN      29
#endif

// ===== 8-bit Register Addresses =====
// The CS4398 uses simple single-byte register addresses transmitted over I2C.
// All register accesses use _writeReg8() / _readReg8() — NOT the 16-bit
// paged helpers used by CS43198/CS43131.

// ----- Chip ID / Revision -----
// [7:4] = Chip ID (0x7 for CS4398 family)
// [3:0] = Silicon revision
#define CS4398_REG_CHIP_ID          0x01   // Chip ID and silicon revision

// ----- Mode Control -----
// bit7 = CP_EN  (1=control port enabled, 0=stand-alone / hardware mode)
// bit6 = CHSL   (0=I2S input, 1=DSD input)
// bits[5:4] = FM (00=single-speed ≤50kHz, 01=double-speed ≤100kHz, 10=quad-speed ≤200kHz)
// bit3 = DEM    (0=no de-emphasis, 1=32kHz/44.1kHz/48kHz auto de-emphasis)
// bit2 = DIF2   (interface format high bit; combined with reg 0x03 bit7)
// bit1 = RSVD
// bit0 = RSVD
#define CS4398_REG_MODE_CTL         0x02   // Mode control (DSD/PCM, speed mode)

// ----- Volume / Mixing Control -----
// bit7 = DIF1   (interface format low bit; 00=left-justified, 01=I2S, 10/11=right-justified)
// bit6 = ATCA   (0=independent A/B volume, 1=A volume register controls both channels)
// bit5 = ATCB   (0=independent, 1=B controls A; only valid when ATCA=0)
// bit4 = INVA   (1=invert polarity channel A)
// bit3 = INVB   (1=invert polarity channel B)
// bit2 = MUTE_SP (1=soft-mute with ramp; 0=hard mute)
// bit1 = RSVD
// bit0 = RSVD
#define CS4398_REG_VOL_MIX_CTL     0x03   // Volume/mixing control

// ----- Mute Control -----
// bit1 = MUTEB  (1=mute channel B / right)
// bit0 = MUTEA  (1=mute channel A / left)
// bits[7:2] = RSVD
#define CS4398_REG_MUTE_CTL        0x04   // Mute control

// ----- Channel A Volume -----
// Attenuation register: 0x00=0dB (full volume), 0xFF=mute (−127.5dB), 0.5dB/step.
// Inverted attenuation: higher register value = more attenuation.
#define CS4398_REG_VOL_A           0x05   // Channel A (left) volume

// ----- Channel B Volume -----
// Same encoding as REG_VOL_A.
#define CS4398_REG_VOL_B           0x06   // Channel B (right) volume

// ----- Ramp / Filter Control -----
// bits[3:2] = FILT_SEL[1:0]  — 2-bit digital filter preset selection (shift=2)
//             0b00 = Fast roll-off linear phase
//             0b01 = Slow roll-off linear phase
//             0b10 = Minimum phase
// bit1 = RMPDN  (1=ramp-down on mute)
// bit0 = RMPUP  (1=ramp-up on unmute)
// bits[7:4] = RSVD
#define CS4398_REG_RAMP_FILT       0x07   // Ramp and digital filter control

// ----- Misc Control -----
// bit7 = PDN    (1=power down entire device)
// bit6 = RSVD
// bit5 = FREEZE (1=freeze volume/filter settings until cleared)
// bit4 = CPEN   (1=use control port volume; 0=use hardware pins)
// bits[3:0] = RSVD
#define CS4398_REG_MISC_CTL        0x08   // Miscellaneous control

// ----- Misc Control 2 -----
// bit7 = RSVD
// bit6 = DSD_AUTO  (1=auto-detect DSD vs PCM on input)
// bit5 = DSD64_ONLY (1=DSD64 mode only; 0=auto DSD rate)
// bits[4:0] = RSVD
#define CS4398_REG_MISC_CTL2       0x09   // Miscellaneous control 2

// ===== Bit Masks =====

// CS4398_REG_MUTE_CTL (0x04) — per-channel mute bits
#define CS4398_MUTE_A_BIT          0x01   // bit0: mute channel A (left)
#define CS4398_MUTE_B_BIT          0x02   // bit1: mute channel B (right)
#define CS4398_MUTE_BOTH           (CS4398_MUTE_A_BIT | CS4398_MUTE_B_BIT)

// CS4398_REG_RAMP_FILT (0x07) — FILT_SEL field
#define CS4398_FILTER_MASK         0x0C   // bits[3:2]: 2-bit filter select field
#define CS4398_FILTER_SHIFT        2      // Shift amount to place preset value in bits[3:2]

// CS4398_REG_MISC_CTL (0x08) — power-down bit
#define CS4398_PDN_BIT             0x80   // bit7: 1=power down, 0=normal operation

// CS4398_REG_MODE_CTL (0x02) — FM speed mode bits[5:4]
#define CS4398_FM_MASK             0x30   // bits[5:4]: speed mode field
#define CS4398_FM_SINGLE           0x00   // Single speed:  ≤50kHz
#define CS4398_FM_DOUBLE           0x10   // Double speed:  ≤100kHz
#define CS4398_FM_QUAD             0x20   // Quad speed:    ≤200kHz

// CS4398_REG_MODE_CTL (0x02) — DSD bit
#define CS4398_DSD_BIT             0x40   // bit6: 1=DSD input, 0=PCM input

// CS4398_REG_MODE_CTL (0x02) — control port enable
#define CS4398_CP_EN_BIT           0x80   // bit7: 1=control port enabled

// CS4398_REG_VOL_MIX_CTL (0x03) — DIF1 interface format
// Combined with MODE_CTL bit2 (DIF2): {DIF2,DIF1} = 01 → I2S (Philips) format
#define CS4398_DIF1_I2S            0x80   // bit7: I2S format selector (DIF1=1, DIF2=0)

// ===== Filter Preset Constants =====
// CS4398 provides 3 digital filter presets (2-bit field, indices 0-2).
#define CS4398_FILTER_COUNT        3

#define CS4398_FILTER_FAST_LINEAR  0x00   // Fast roll-off, linear phase
#define CS4398_FILTER_SLOW_LINEAR  0x01   // Slow roll-off, linear phase
#define CS4398_FILTER_MIN_PHASE    0x02   // Minimum phase

// ===== Volume Constants =====
#define CS4398_VOL_0DB             0x00   // 0 dB (full volume, no attenuation)
#define CS4398_VOL_MUTE            0xFF   // Maximum attenuation (effective mute)

// ===== Init Config Values =====
// Control port enabled, PCM mode, single-speed (default 48kHz).
// DIF2=0; DIF1=1 (in REG_VOL_MIX_CTL) → {DIF2,DIF1}=01 = I2S slave format.
#define CS4398_MODE_CTL_DEFAULT    (CS4398_CP_EN_BIT | CS4398_FM_SINGLE)
#define CS4398_VOL_MIX_DEFAULT     (CS4398_DIF1_I2S)

#endif // CS4398_REGS_H
