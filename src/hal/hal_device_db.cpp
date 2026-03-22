#ifdef DAC_ENABLED

#include "hal_device_db.h"
#include "hal_device_manager.h"
#include "hal_driver_registry.h"
#include "hal_pipeline_bridge.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../diag_journal.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#else
#define LOG_I(tag, ...) ((void)0)
#define LOG_W(tag, ...) ((void)0)
#define LOG_E(tag, ...) ((void)0)
#endif

// ===== In-memory database =====
static HalDeviceDescriptor _db[HAL_DB_MAX_ENTRIES];
static int _dbCount = 0;

// ===== Builtin entries (always available, no LittleFS needed) =====
static void hal_db_add_builtins() {
    // PCM5102A
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ti,pcm5102a");
        hal_safe_strcpy(d.name, sizeof(d.name), "PCM5102A");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Texas Instruments");
        d.type = HAL_DEV_DAC;
        d.legacyId = 0x0001;
        d.channelCount = 2;
        d.bus.type = HAL_BUS_I2S;
        d.bus.index = 0;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // ES8311 (canonical compatible string: everest-semi,es8311)
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "everest-semi,es8311");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES8311");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Everest Semiconductor");
        d.type = HAL_DEV_CODEC;
        d.legacyId = 0x0004;
        d.channelCount = 2;
        d.i2cAddr = 0x18;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_ONBOARD;
        d.sampleRatesMask = HAL_RATE_8K | HAL_RATE_16K | HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = HAL_CAP_CODEC | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE |
                         HAL_CAP_ADC_PATH | HAL_CAP_DAC_PATH;
        hal_db_add(&d);
    }
    // ES8311 legacy alias (evergrande) — kept for backward compatibility
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "evergrande,es8311");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES8311");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Everest Semiconductor");
        d.type = HAL_DEV_CODEC;
        d.legacyId = 0x0004;
        d.channelCount = 2;
        d.i2cAddr = 0x18;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_ONBOARD;
        d.sampleRatesMask = HAL_RATE_8K | HAL_RATE_16K | HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = HAL_CAP_CODEC | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE |
                         HAL_CAP_ADC_PATH | HAL_CAP_DAC_PATH;
        hal_db_add(&d);
    }
    // PCM1808
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ti,pcm1808");
        hal_safe_strcpy(d.name, sizeof(d.name), "PCM1808");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Texas Instruments");
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.bus.type = HAL_BUS_I2S;
        d.bus.index = 0;
        d.sampleRatesMask = HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = HAL_CAP_ADC_PATH;
        hal_db_add(&d);
    }
    // ES9822PRO — expansion ADC, I2C control
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ess,es9822pro");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES9822PRO");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ESS Technology");
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.i2cAddr = 0x40;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = 2;  // Expansion bus
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        d.capabilities = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL;
        hal_db_add(&d);
    }
    // ES9843PRO — expansion 4-channel ADC, I2C control
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ess,es9843pro");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES9843PRO");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ESS Technology");
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 4;
        d.i2cAddr = 0x40;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = 2;  // Expansion bus
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        d.capabilities = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL;
        hal_db_add(&d);
    }
    // ES9826 — expansion 2-channel ADC, I2C control, PGA 0-30dB, no HPF register
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ess,es9826");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES9826");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ESS Technology");
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.i2cAddr = 0x40;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        d.capabilities = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL;
        hal_db_add(&d);
    }
    // ES9821 — expansion 2-channel ADC, I2C control, no PGA, no HPF register
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ess,es9821");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES9821");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ESS Technology");
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.i2cAddr = 0x40;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        d.capabilities = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME;
        hal_db_add(&d);
    }
    // ES9823PRO — expansion 2-channel ADC, I2C control, PGA 0-42dB, highest spec 2ch
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ess,es9823pro");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES9823PRO");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ESS Technology");
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.i2cAddr = 0x40;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        d.capabilities = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL;
        hal_db_add(&d);
    }
    // ES9823MPRO — monolithic package variant of ES9823PRO (same registers, different package)
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ess,es9823mpro");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES9823MPRO");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ESS Technology");
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.i2cAddr = 0x40;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        d.capabilities = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL;
        hal_db_add(&d);
    }
    // ES9820 — expansion 2-channel ADC, I2C control, PGA 0-18dB, HPF, entry-tier 2ch
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ess,es9820");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES9820");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ESS Technology");
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.i2cAddr = 0x40;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        d.capabilities = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL;
        hal_db_add(&d);
    }
    // ES9842PRO — expansion 4-channel TDM ADC, I2C control, PGA 0-18dB, HPF
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ess,es9842pro");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES9842PRO");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ESS Technology");
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 4;
        d.i2cAddr = 0x40;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        d.capabilities = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL;
        hal_db_add(&d);
    }
    // ES9840 — expansion 4-channel TDM ADC, I2C control, PGA 0-18dB, entry-tier 4ch
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ess,es9840");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES9840");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ESS Technology");
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 4;
        d.i2cAddr = 0x40;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        d.capabilities = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL;
        hal_db_add(&d);
    }
    // ES9841 — expansion 4-channel TDM ADC, I2C control, PGA 0-42dB, 8-bit volume
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ess,es9841");
        hal_safe_strcpy(d.name, sizeof(d.name), "ES9841");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ESS Technology");
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 4;
        d.i2cAddr = 0x40;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        d.capabilities = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL;
        hal_db_add(&d);
    }
    // NS4150B
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "ns,ns4150b-amp");
        hal_safe_strcpy(d.name, sizeof(d.name), "NS4150B Amp");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Nsiway");
        d.type = HAL_DEV_AMP;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_GPIO;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Chip Temperature Sensor
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "espressif,esp32p4-temp");
        hal_safe_strcpy(d.name, sizeof(d.name), "Chip Temperature");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Espressif");
        d.type = HAL_DEV_SENSOR;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_INTERNAL;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // ST7735S TFT Display
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "sitronix,st7735s");
        hal_safe_strcpy(d.name, sizeof(d.name), "ST7735S TFT");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Sitronix");
        d.type = HAL_DEV_DISPLAY;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_SPI;
        d.bus.pinA = 2;   // MOSI
        d.bus.pinB = 3;   // SCLK
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Rotary Encoder
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "alps,ec11");
        hal_safe_strcpy(d.name, sizeof(d.name), "Rotary Encoder");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Alps");
        d.type = HAL_DEV_INPUT;
        d.channelCount = 3;
        d.bus.type = HAL_BUS_GPIO;
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Piezo Buzzer
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "generic,piezo-buzzer");
        hal_safe_strcpy(d.name, sizeof(d.name), "Piezo Buzzer");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Generic");
        d.type = HAL_DEV_GPIO;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_GPIO;
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Status LED
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "generic,status-led");
        hal_safe_strcpy(d.name, sizeof(d.name), "Status LED");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Generic");
        d.type = HAL_DEV_GPIO;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_GPIO;
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Amplifier Relay
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "generic,relay-amp");
        hal_safe_strcpy(d.name, sizeof(d.name), "Amplifier Relay");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Generic");
        d.type = HAL_DEV_AMP;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_GPIO;
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Reset Button
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "generic,tact-switch");
        hal_safe_strcpy(d.name, sizeof(d.name), "Reset Button");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Generic");
        d.type = HAL_DEV_INPUT;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_GPIO;
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Signal Generator
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "alx,signal-gen");
        hal_safe_strcpy(d.name, sizeof(d.name), "Signal Generator");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ALX");
        d.type = HAL_DEV_ADC;
        d.channelCount = 2;
        d.bus.type = HAL_BUS_INTERNAL;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = HAL_CAP_ADC_PATH;
        hal_db_add(&d);
    }
    // USB Audio
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "alx,usb-audio");
        hal_safe_strcpy(d.name, sizeof(d.name), "USB Audio");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "ALX");
        d.type = HAL_DEV_ADC;
        d.channelCount = 2;
        d.bus.type = HAL_BUS_INTERNAL;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = HAL_CAP_ADC_PATH;
        hal_db_add(&d);
    }
    // MCP4725 — 12-bit I2C voltage output DAC (add-on module)
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), "microchip,mcp4725");
        hal_safe_strcpy(d.name, sizeof(d.name), "MCP4725");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), "Microchip Technology");
        d.type = HAL_DEV_DAC;
        d.channelCount = 1;
        d.i2cAddr = 0x60;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;  // GPIO 28/29 expansion bus
        d.sampleRatesMask = 0;
        d.capabilities = HAL_CAP_HW_VOLUME;
        hal_db_add(&d);
    }
    // ES9038Q2M — expansion 2-channel SABRE DAC, I2C control + I2S data, up to 768kHz
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9038q2m", 31);
        strncpy(d.name, "ES9038Q2M", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        hal_db_add(&d);
    }
    // ES9039Q2M — expansion 2-channel SABRE DAC, I2C control + I2S data, up to 768kHz
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9039q2m", 31);
        strncpy(d.name, "ES9039Q2M", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        hal_db_add(&d);
    }
    // ES9069Q — expansion 2-channel SABRE DAC with MQA hardware renderer, up to 768kHz
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9069q", 31);
        strncpy(d.name, "ES9069Q", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_MQA;
        hal_db_add(&d);
    }
    // ES9033Q — expansion 2-channel SABRE DAC with integrated line driver, up to 768kHz
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9033q", 31);
        strncpy(d.name, "ES9033Q", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_LINE_DRIVER;
        hal_db_add(&d);
    }
    // ES9020 — expansion 2-channel SABRE DAC with analog PLL, up to 192kHz
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9020-dac", 31);
        strncpy(d.name, "ES9020", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_APLL;
        hal_db_add(&d);
    }
    // ES9038PRO — expansion 8-channel SABRE DAC, HyperStream II, 132dB DNR, up to 768kHz
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9038pro", 31);
        strncpy(d.name, "ES9038PRO", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.channelCount = 8;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        hal_db_add(&d);
    }
    // ES9028PRO — expansion 8-channel SABRE DAC, HyperStream II, 124dB DNR, up to 768kHz
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9028pro", 31);
        strncpy(d.name, "ES9028PRO", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.channelCount = 8;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        hal_db_add(&d);
    }
    // ES9039PRO — expansion 8-channel SABRE DAC, HyperStream IV, 132dB DNR, up to 768kHz
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9039pro", 31);
        strncpy(d.name, "ES9039PRO", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.channelCount = 8;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        hal_db_add(&d);
    }
    // ES9039MPRO — industrial/automotive variant of ES9039PRO (auto-detected at init)
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9039mpro", 31);
        strncpy(d.name, "ES9039MPRO", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.channelCount = 8;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        hal_db_add(&d);
    }
    // ES9027PRO — expansion 8-channel SABRE DAC, HyperStream IV, 124dB DNR, up to 768kHz
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9027pro", 31);
        strncpy(d.name, "ES9027PRO", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.channelCount = 8;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        hal_db_add(&d);
    }
    // ES9081 — expansion 8-channel SABRE DAC, HyperStream IV, 120dB DNR, 40-pin QFN
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9081", 31);
        strncpy(d.name, "ES9081", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.channelCount = 8;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        hal_db_add(&d);
    }
    // ES9082 — expansion 8-channel SABRE DAC, HyperStream IV, 120dB DNR, 48-pin QFN, ASP2
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9082", 31);
        strncpy(d.name, "ES9082", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.channelCount = 8;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        hal_db_add(&d);
    }
    // ES9017 — expansion 8-channel SABRE DAC, HyperStream IV, 120dB DNR, ES9027PRO drop-in
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ess,es9017", 31);
        strncpy(d.name, "ES9017", 32);
        strncpy(d.manufacturer, "ESS Technology", 32);
        d.type = HAL_DEV_DAC;
        d.channelCount = 8;
        d.i2cAddr = 0x48;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K;
        d.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        hal_db_add(&d);
    }
}

void hal_db_init() {
    _dbCount = 0;
    memset(_db, 0, sizeof(_db));
    hal_db_add_builtins();

#ifndef NATIVE_TEST
    // Load additional entries from LittleFS
    if (!LittleFS.exists(HAL_DB_FILE_PATH)) return;

    File f = LittleFS.open(HAL_DB_FILE_PATH, "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        LOG_W("[HAL:DB]", "Failed to parse %s: %s", HAL_DB_FILE_PATH, err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        const char* compat = obj["compatible"] | "";
        if (hal_db_lookup(compat, nullptr)) continue;  // Skip if already a builtin

        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        hal_safe_strcpy(d.compatible, sizeof(d.compatible), compat);
        hal_safe_strcpy(d.name, sizeof(d.name), obj["name"] | "");
        hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), obj["manufacturer"] | "");
        d.type = static_cast<HalDeviceType>(obj["type"] | 0);
        d.legacyId = obj["legacyId"] | 0;
        d.i2cAddr = obj["i2cAddr"] | 0;
        d.channelCount = obj["channels"] | 2;
        d.sampleRatesMask = obj["ratesMask"] | 0;
        d.capabilities = obj["capabilities"] | 0;
        hal_db_add(&d);
    }
    LOG_I("[HAL:DB]", "Loaded %d entries (including builtins)", _dbCount);
#endif
}

bool hal_db_lookup(const char* compatible, HalDeviceDescriptor* out) {
    if (!compatible) return false;
    for (int i = 0; i < _dbCount; i++) {
        if (strcmp(_db[i].compatible, compatible) == 0) {
            if (out) *out = _db[i];
            return true;
        }
    }
    return false;
}

bool hal_db_add(const HalDeviceDescriptor* desc) {
    if (!desc || desc->compatible[0] == '\0') return false;

    // Update existing entry
    for (int i = 0; i < _dbCount; i++) {
        if (strcmp(_db[i].compatible, desc->compatible) == 0) {
            _db[i] = *desc;
            return true;
        }
    }

    if (_dbCount >= HAL_DB_MAX_ENTRIES) {
        LOG_W("[HAL DB] Device DB full (%d/%d): %s", _dbCount, HAL_DB_MAX_ENTRIES, desc->compatible);
#ifndef NATIVE_TEST
        diag_emit(DIAG_HAL_DB_FULL, DIAG_SEV_ERROR, 0, desc->compatible, "DB full");
#endif
        return false;
    }
    _db[_dbCount] = *desc;
    _dbCount++;
    return true;
}

bool hal_db_remove(const char* compatible) {
    if (!compatible) return false;
    for (int i = 0; i < _dbCount; i++) {
        if (strcmp(_db[i].compatible, compatible) == 0) {
            // Shift remaining entries
            for (int j = i; j < _dbCount - 1; j++) {
                _db[j] = _db[j + 1];
            }
            _dbCount--;
            return true;
        }
    }
    return false;
}

bool hal_db_save() {
#ifndef NATIVE_TEST
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < _dbCount; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["compatible"] = _db[i].compatible;
        obj["name"] = _db[i].name;
        obj["manufacturer"] = _db[i].manufacturer;
        obj["type"] = _db[i].type;
        obj["legacyId"] = _db[i].legacyId;
        obj["i2cAddr"] = _db[i].i2cAddr;
        obj["channels"] = _db[i].channelCount;
        obj["ratesMask"] = _db[i].sampleRatesMask;
        obj["capabilities"] = _db[i].capabilities;
    }

    File f = LittleFS.open(HAL_DB_FILE_PATH, "w");
    if (!f) {
        LOG_E("[HAL:DB]", "Failed to open %s for writing", HAL_DB_FILE_PATH);
        return false;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[HAL:DB]", "Saved %d entries to %s", _dbCount, HAL_DB_FILE_PATH);
    return true;
#else
    return true;
#endif
}

int hal_db_count() { return _dbCount; }

int hal_db_max() { return HAL_DB_MAX_ENTRIES; }

const HalDeviceDescriptor* hal_db_get(int index) {
    if (index < 0 || index >= _dbCount) return nullptr;
    return &_db[index];
}

void hal_load_device_configs() {
#ifndef NATIVE_TEST
    // Recovery: complete an interrupted atomic write (tmp exists, final does not)
    if (LittleFS.exists(HAL_CONFIG_TMP_PATH) && !LittleFS.exists(HAL_CONFIG_FILE_PATH)) {
        LittleFS.rename(HAL_CONFIG_TMP_PATH, HAL_CONFIG_FILE_PATH);
        LOG_I("[HAL:DB]", "Recovered config from interrupted write");
    }
    // Clean up stale tmp if both exist (previous rename succeeded before crash check ran)
    if (LittleFS.exists(HAL_CONFIG_TMP_PATH)) {
        LittleFS.remove(HAL_CONFIG_TMP_PATH);
    }

    if (!LittleFS.exists(HAL_CONFIG_FILE_PATH)) return;

    File f = LittleFS.open(HAL_CONFIG_FILE_PATH, "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        LOG_W("[HAL:DB]", "Failed to parse %s: %s", HAL_CONFIG_FILE_PATH, err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    HalDeviceManager& mgr = HalDeviceManager::instance();
    for (JsonObject obj : arr) {
        uint8_t slot = obj["slot"] | 255;
        if (slot >= HAL_MAX_DEVICES) continue;

        HalDeviceConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.valid = true;
        cfg.i2cAddr = obj["i2cAddr"] | 0;
        cfg.i2cBusIndex = obj["i2cBus"] | 0;
        cfg.i2cSpeedHz = obj["i2cSpeed"] | 0;
        cfg.pinSda = obj["pinSda"] | -1;
        cfg.pinScl = obj["pinScl"] | -1;
        cfg.pinMclk = obj["pinMclk"] | -1;
        cfg.pinData = obj["pinData"] | -1;
        cfg.i2sPort = obj["i2sPort"] | 255;
        cfg.gpioA = obj["gpioA"] | (int)-1;
        cfg.gpioB = obj["gpioB"] | (int)-1;
        cfg.gpioC = obj["gpioC"] | (int)-1;
        cfg.gpioD = obj["gpioD"] | (int)-1;
        cfg.usbPid = obj["usbPid"] | (int)0;
        cfg.filterMode = obj["filterMode"] | 0;
        cfg.sampleRate = obj["sampleRate"] | 0;
        cfg.bitDepth = obj["bitDepth"] | 0;
        cfg.volume = obj["volume"] | 100;
        cfg.mute = obj["mute"] | false;
        cfg.enabled = obj["enabled"] | true;
        const char* label = obj["label"] | "";
        hal_safe_strcpy(cfg.userLabel, sizeof(cfg.userLabel), label);

        mgr.setConfig(slot, cfg);
    }
    LOG_I("[HAL:DB]", "Device configs loaded from %s", HAL_CONFIG_FILE_PATH);
#endif
}

bool hal_save_device_config(uint8_t slot) {
#ifndef NATIVE_TEST
    // Save ALL configs (simpler than surgical single-slot update)
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    HalDeviceManager& mgr = HalDeviceManager::instance();

    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDeviceConfig* cfg = mgr.getConfig(i);
        if (!cfg || !cfg->valid) continue;

        JsonObject obj = arr.add<JsonObject>();
        obj["slot"] = i;
        obj["i2cAddr"] = cfg->i2cAddr;
        obj["i2cBus"] = cfg->i2cBusIndex;
        obj["i2cSpeed"] = cfg->i2cSpeedHz;
        obj["pinSda"] = cfg->pinSda;
        obj["pinScl"] = cfg->pinScl;
        obj["pinMclk"] = cfg->pinMclk;
        obj["pinData"] = cfg->pinData;
        obj["i2sPort"] = cfg->i2sPort;
        obj["sampleRate"] = cfg->sampleRate;
        obj["bitDepth"] = cfg->bitDepth;
        obj["volume"] = cfg->volume;
        obj["mute"] = cfg->mute;
        obj["enabled"] = cfg->enabled;
        if (cfg->userLabel[0]) obj["label"] = cfg->userLabel;
        if (cfg->gpioA >= 0) obj["gpioA"] = cfg->gpioA;
        if (cfg->gpioB >= 0) obj["gpioB"] = cfg->gpioB;
        if (cfg->gpioC >= 0) obj["gpioC"] = cfg->gpioC;
        if (cfg->gpioD >= 0) obj["gpioD"] = cfg->gpioD;
        if (cfg->usbPid != 0) obj["usbPid"] = cfg->usbPid;
        if (cfg->filterMode != 0) obj["filterMode"] = cfg->filterMode;
    }

    File f = LittleFS.open(HAL_CONFIG_TMP_PATH, "w");
    if (!f) {
        LOG_E("[HAL:DB]", "Failed to open tmp config for writing");
        return false;
    }
    serializeJson(doc, f);
    f.close();
    LittleFS.rename(HAL_CONFIG_TMP_PATH, HAL_CONFIG_FILE_PATH);
    LOG_I("[HAL:DB]", "Saved device configs to %s", HAL_CONFIG_FILE_PATH);
    return true;
#else
    (void)slot;
    return true;
#endif
}

void hal_db_reset() {
    _dbCount = 0;
    memset(_db, 0, sizeof(_db));
}

// ===== Auto-provisioning =====

void hal_provision_defaults() {
#ifndef NATIVE_TEST
    if (LittleFS.exists("/hal_auto_devices.json")) return;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    // PCM5102A DAC — I2S TX (DOUT=24, shared BCK/LRC/MCLK with ADCs)
    {
        JsonObject o = arr.add<JsonObject>();
        o["compatible"] = "ti,pcm5102a";
        o["label"]      = "PCM5102A DAC";
        o["i2sPort"]    = 0;
        o["sampleRate"] = 48000;
        o["bitDepth"]   = 32;
        o["pinData"]    = 24;    // I2S_TX_DATA_PIN
        o["pinBck"]     = 20;    // I2S_BCK_PIN (shared)
        o["pinLrc"]     = 21;    // I2S_LRC_PIN (shared)
        o["pinMclk"]    = 22;    // I2S_MCLK_PIN (shared)
        o["pinFmt"]     = -1;
        o["probeOnly"]  = true;  // HAL state holder; audio pipeline sink via dac_output_init()
    }

    // PCM1808 ADC1 — I2S RX channel 0 (DIN=23), clock master
    {
        JsonObject o = arr.add<JsonObject>();
        o["compatible"]       = "ti,pcm1808";
        o["label"]            = "PCM1808 ADC1";
        o["i2sPort"]          = 0;
        o["sampleRate"]       = 48000;
        o["bitDepth"]         = 32;
        o["pinData"]          = 23;    // I2S_DOUT_PIN
        o["pinBck"]           = 20;
        o["pinLrc"]           = 21;
        o["pinMclk"]          = 22;
        o["pinFmt"]           = -1;
        o["isI2sClockMaster"] = true;  // ADC1 outputs MCLK/BCK/WS
        o["probeOnly"]        = false;
    }

    // PCM1808 ADC2 — I2S RX channel 1 (DIN=25, shared clocks, data-only)
    {
        JsonObject o = arr.add<JsonObject>();
        o["compatible"]       = "ti,pcm1808";
        o["label"]            = "PCM1808 ADC2";
        o["i2sPort"]          = 1;
        o["sampleRate"]       = 48000;
        o["bitDepth"]         = 32;
        o["pinData"]          = 25;    // I2S_DOUT2_PIN
        o["pinBck"]           = 20;
        o["pinLrc"]           = 21;
        o["pinMclk"]          = 22;
        o["pinFmt"]           = -1;
        o["isI2sClockMaster"] = false; // ADC2 receives clocks only (data-only)
        o["probeOnly"]        = false;
    }

    File f = LittleFS.open("/hal_auto_devices.json", "w");
    if (!f) {
        LOG_E("[HAL:DB] Cannot create /hal_auto_devices.json");
        return;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[HAL:DB] Default auto-devices written to /hal_auto_devices.json");
#endif
}

void hal_load_auto_devices() {
#ifndef NATIVE_TEST
    if (!LittleFS.exists("/hal_auto_devices.json")) return;

    File f = LittleFS.open("/hal_auto_devices.json", "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        LOG_E("[HAL:DB] Parse error in /hal_auto_devices.json: %s", err.c_str());
        return;
    }

    HalDeviceManager& mgr = HalDeviceManager::instance();

    for (JsonObject obj : doc.as<JsonArray>()) {
        const char* compatible = obj["compatible"] | "";
        if (!compatible[0]) continue;

        // Skip if already registered (e.g. by dac_hal.cpp or a previous call)
        if (mgr.findByCompatible(compatible) != nullptr) {
            LOG_I("[HAL:DB] '%s' already registered — skip", compatible);
            continue;
        }

        // Look up driver factory
        const HalDriverEntry* entry = hal_registry_find(compatible);
        if (!entry || !entry->factory) {
            LOG_W("[HAL:DB]No factory for '%s' — skip", compatible);
            continue;
        }

        HalDevice* dev = entry->factory();
        int slot = mgr.registerDevice(dev, HAL_DISC_MANUAL);
        if (slot < 0) {
            delete dev;
            LOG_W("[HAL:DB]No free slot for '%s' — skip", compatible);
            continue;
        }

        // Apply config from JSON (pin overrides, sample rate, label)
        HalDeviceConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.valid        = true;
        cfg.enabled      = obj["enabled"]    | true;
        cfg.i2sPort      = obj["i2sPort"]    | (uint8_t)255;
        cfg.sampleRate   = obj["sampleRate"] | (uint32_t)0;
        cfg.bitDepth     = obj["bitDepth"]   | (uint8_t)0;
        cfg.pinData      = obj["pinData"]    | (int8_t)-1;
        cfg.pinBck       = obj["pinBck"]     | (int8_t)-1;
        cfg.pinLrc       = obj["pinLrc"]     | (int8_t)-1;
        cfg.pinMclk      = obj["pinMclk"]    | (int8_t)-1;
        cfg.pinFmt       = obj["pinFmt"]     | (int8_t)-1;
        cfg.pinSda       = -1;
        cfg.pinScl       = -1;
        cfg.paControlPin = -1;
        cfg.volume       = 100;
        const char* label = obj["label"] | "";
        hal_safe_strcpy(cfg.userLabel, sizeof(cfg.userLabel), label);
        mgr.setConfig(slot, cfg);

        // probeOnly=true  — device is a config/state holder; full init done elsewhere
        //   (e.g. PCM5102A: audio pipeline sink registered by dac_output_init())
        // probeOnly=false — probe + init now
        //   (e.g. PCM1808: I2S RX managed by i2s_audio.cpp; HAL just tracks state)
        bool probeOnly = obj["probeOnly"] | false;
        dev->probe();
        if (!probeOnly) {
            dev->init();
            hal_pipeline_on_device_available(slot);
        }

        LOG_I("[HAL:DB]Auto-device '%s' (%s) slot %d%s",
              compatible, label[0] ? label : "?", slot,
              probeOnly ? " [probe-only]" : " [ready]");
    }
#endif
}

#endif // DAC_ENABLED
