#ifndef ES8311_REGS_H
#define ES8311_REGS_H

#include <stdint.h>

// ===== I2C Address =====
// CE pin LOW = 0x18, CE pin HIGH = 0x19 (7-bit addresses)
// Waveshare ESP32-P4-WiFi6-DEV-Kit has CE=LOW
#define ES8311_I2C_ADDR       0x18

// ===== DAC Device ID (for DacDriver registry) =====
#define DAC_ID_ES8311         0x0004

// ===== ES8311 PA Control Pin (NS4150B class-D amp on Waveshare board) =====
#define ES8311_PA_PIN         53

// ===== ES8311 Onboard I2C Pins (Waveshare ESP32-P4 board) =====
// GPIO7=SDA, GPIO8=SCL — dedicated onboard I2C bus, separate from DAC I2C (GPIO48/54).
// Override at build time via -D ES8311_I2C_SDA_PIN=N / -D ES8311_I2C_SCL_PIN=N.
#ifndef ES8311_I2C_SDA_PIN
#define ES8311_I2C_SDA_PIN    7
#endif
#ifndef ES8311_I2C_SCL_PIN
#define ES8311_I2C_SCL_PIN    8
#endif

// ===== Register Addresses =====

// Reset / Control
#define ES8311_REG_RESET          0x00  // bit7=CSM_ON, bit6=MSC(master/slave), bit4=RST_DIG, bit0=RST_DAC_DIG

// Clock Manager (0x01 - 0x08)
#define ES8311_REG_CLK_MANAGER1   0x01  // bit7=MCLK_SEL(0=pin,1=SCLK), bits5:0=clock enables
#define ES8311_REG_CLK_MANAGER2   0x02  // bits7:5=PRE_MULTI, bits4:0=PRE_DIV
#define ES8311_REG_CLK_MANAGER3   0x03  // bit6=FS_MODE, bits5:0=ADC_OSR
#define ES8311_REG_CLK_MANAGER4   0x04  // bits5:0=DAC_OSR
#define ES8311_REG_CLK_MANAGER5   0x05  // bits7:4=ADC_CLK_DIV, bits3:0=DAC_CLK_DIV
#define ES8311_REG_CLK_MANAGER6   0x06  // bit5=SCLK_INV, bits4:0=BCLK_DIV
#define ES8311_REG_CLK_MANAGER7   0x07  // LRCK_DIV_H (high byte of LRCK divider)
#define ES8311_REG_CLK_MANAGER8   0x08  // LRCK_DIV_L (low byte of LRCK divider)

// Serial Data Port
#define ES8311_REG_SDPIN          0x09  // DAC serial data in: bit6=TRI_STATE, bits4:2=DACWL, bits1:0=SDP_FMT
#define ES8311_REG_SDPOUT         0x0A  // ADC serial data out: bit6=TRI_STATE, bits4:2=ADCWL, bits1:0=SDP_FMT

// System Power (0x0B - 0x14)
#define ES8311_REG_SYSTEM1        0x0B  // Power management 1
#define ES8311_REG_SYSTEM2        0x0C  // Power management 2
#define ES8311_REG_SYSTEM3        0x0D  // bits5:4=VMIDSEL, VREF power
#define ES8311_REG_SYSTEM4        0x0E  // bit3=PDN_PGA, bit1=PDN_MOD_ADC, bit0=PDN_MOD_DAC
#define ES8311_REG_SYSTEM5        0x10  // Digital reference 1
#define ES8311_REG_SYSTEM6        0x11  // Digital reference 2
#define ES8311_REG_SYSTEM7        0x12  // bit1=PDN_DAC, bit0=ENREFR
#define ES8311_REG_SYSTEM8        0x13  // bit4=HPSW (headphone switch), analog reference
#define ES8311_REG_SYSTEM9        0x14  // DMIC, LINSEL, PGAGAIN

// ADC Registers
#define ES8311_REG_ADC_RAMP       0x15  // ADC ramp rate
#define ES8311_REG_ADC_MIC_GAIN   0x16  // ADC microphone gain
#define ES8311_REG_ADC_VOLUME     0x17  // ADC digital volume
#define ES8311_REG_ADC_ALC        0x1B  // ADC ALC settings
#define ES8311_REG_ADC_EQ_DC      0x1C  // ADC EQ / DC removal

// DAC Registers
#define ES8311_REG_DAC_CTRL       0x31  // bit6=SOFT_MUTE, bit5=DAC_MUTE
#define ES8311_REG_DAC_VOLUME     0x32  // 0x00=-95.5dB, 0xBF=0dB, 0xFF=+32dB (0.5dB steps)
#define ES8311_REG_DAC_RAMP       0x37  // DAC ramp rate

// GPIO
#define ES8311_REG_GPIO_CFG       0x44  // GPIO configuration / internal reference
#define ES8311_REG_GP_CTRL        0x45  // General purpose control

// Chip ID
#define ES8311_REG_CHIP_ID1       0xFD  // Chip ID byte 1
#define ES8311_REG_CHIP_ID2       0xFE  // Chip ID byte 2
#define ES8311_REG_CHIP_VER       0xFF  // Chip version

// ===== Bit Masks =====

// REG_RESET (0x00)
#define ES8311_CSM_ON             0x80  // Codec state machine ON
#define ES8311_MSC_MASTER         0x40  // Master mode (0 = slave)
#define ES8311_RST_DIG            0x10  // Reset digital
#define ES8311_RST_DAC_DIG        0x01  // Reset DAC digital

// REG_SDPIN / REG_SDPOUT format bits
#define ES8311_SDP_TRISTATE       0x40  // Tri-state output

// REG_DAC_CTRL (0x31)
#define ES8311_DAC_SOFT_MUTE      0x40  // Soft mute (gradual)
#define ES8311_DAC_MUTE           0x20  // Hard mute

// ===== Word Length Constants (for REG_SDPIN/SDPOUT bits 4:2) =====
#define ES8311_WL_24BIT           0x00
#define ES8311_WL_20BIT           0x04
#define ES8311_WL_18BIT           0x08
#define ES8311_WL_16BIT           0x0C
#define ES8311_WL_32BIT           0x10

// ===== Serial Data Format (for REG_SDPIN/SDPOUT bits 1:0) =====
#define ES8311_FMT_I2S            0x00
#define ES8311_FMT_LJ             0x01
#define ES8311_FMT_DSP            0x03

// ===== Volume Constants =====
#define ES8311_VOL_0DB            0xBF  // 0 dB (unity gain)
#define ES8311_VOL_MIN            0x00  // -95.5 dB (near silence)
#define ES8311_VOL_MAX_SAFE       0xBF  // 0 dB max for safety (0xFF = +32dB, avoid)

// ===== Clock Coefficient Table =====
// Used by initClocks() to configure dividers for a given MCLK + sample rate combo
struct Es8311ClockCoeff {
    uint32_t mclk;        // Master clock frequency (Hz)
    uint32_t sampleRate;  // Target sample rate (Hz)
    uint8_t  pre_div;     // Pre-divider (CLK_MANAGER2 bits4:0)
    uint8_t  pre_multi;   // Pre-multiplier (CLK_MANAGER2 bits7:5)
    uint8_t  adc_div;     // ADC clock divider (CLK_MANAGER5 bits7:4)
    uint8_t  dac_div;     // DAC clock divider (CLK_MANAGER5 bits3:0)
    uint8_t  fs_mode;     // FS mode (CLK_MANAGER3 bit6): 0=single, 1=double
    uint8_t  lrck_h;      // LRCK divider high byte (CLK_MANAGER7)
    uint8_t  lrck_l;      // LRCK divider low byte (CLK_MANAGER8)
    uint8_t  bclk_div;    // BCLK divider (CLK_MANAGER6 bits4:0)
    uint8_t  adc_osr;     // ADC oversampling (CLK_MANAGER3 bits5:0)
    uint8_t  dac_osr;     // DAC oversampling (CLK_MANAGER4 bits5:0)
};

// Clock coefficient table for common MCLK / sample rate combinations.
// MCLK = 12.288 MHz (256fs at 48kHz) is the primary target — provided by
// the ESP32-P4 I2S2 MCLK output.
//
// LRCK divider = MCLK / (pre_div * sample_rate) - 1, split into high/low bytes.
// BCLK divider indexes into an internal divider table (see datasheet Table 2).
// OSR values: 0x20=128x (low rate), 0x10=64x (48k), 0x08=32x (96k).
static const Es8311ClockCoeff ES8311_COEFF_TABLE[] = {
    // MCLK=12.288MHz entries
    { 12288000,  8000, 0x06, 0x00, 0x01, 0x01, 0x00, 0x05, 0xFF, 0x0C, 0x20, 0x20 },
    { 12288000, 11025, 0x05, 0x00, 0x01, 0x01, 0x00, 0x04, 0x5F, 0x0A, 0x20, 0x20 },
    { 12288000, 16000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x02, 0xFF, 0x08, 0x20, 0x20 },
    { 12288000, 22050, 0x02, 0x00, 0x01, 0x01, 0x00, 0x01, 0x17, 0x06, 0x10, 0x10 },
    { 12288000, 32000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x7F, 0x06, 0x10, 0x10 },
    { 12288000, 44100, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x17, 0x04, 0x10, 0x10 },
    { 12288000, 48000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10 },
    { 12288000, 96000, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x7F, 0x02, 0x08, 0x08 },

    // MCLK=6.144MHz entries (half-rate MCLK)
    {  6144000,  8000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x02, 0xFF, 0x08, 0x20, 0x20 },
    {  6144000, 16000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x7F, 0x06, 0x20, 0x20 },
    {  6144000, 32000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xBF, 0x04, 0x10, 0x10 },
    {  6144000, 48000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7F, 0x02, 0x10, 0x10 },

    // MCLK=11.2896MHz entries (256fs at 44.1kHz family)
    { 11289600, 11025, 0x04, 0x00, 0x01, 0x01, 0x00, 0x03, 0xFF, 0x0A, 0x20, 0x20 },
    { 11289600, 22050, 0x02, 0x00, 0x01, 0x01, 0x00, 0x01, 0xFF, 0x06, 0x10, 0x10 },
    { 11289600, 44100, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10 },
    { 11289600, 88200, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x7F, 0x02, 0x08, 0x08 },
};
static const uint8_t ES8311_COEFF_COUNT = sizeof(ES8311_COEFF_TABLE) / sizeof(ES8311_COEFF_TABLE[0]);

// Look up clock coefficients for a given MCLK + sample rate pair.
// Returns pointer to matching entry, or nullptr if no match.
static inline const Es8311ClockCoeff* es8311_find_coeff(uint32_t mclk, uint32_t sampleRate) {
    for (uint8_t i = 0; i < ES8311_COEFF_COUNT; i++) {
        if (ES8311_COEFF_TABLE[i].mclk == mclk && ES8311_COEFF_TABLE[i].sampleRate == sampleRate) {
            return &ES8311_COEFF_TABLE[i];
        }
    }
    return nullptr;
}

#endif // ES8311_REGS_H
