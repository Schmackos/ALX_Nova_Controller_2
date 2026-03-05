#ifdef DAC_ENABLED

#include "hal_builtin_devices.h"
#include "hal_driver_registry.h"
#include "hal_types.h"
#include <string.h>

// ===== Compatible strings for builtin devices =====
// Uses Linux DT "vendor,model" convention
#define COMPAT_PCM5102A   "ti,pcm5102a"
#define COMPAT_ES8311     "evergrande,es8311"
#define COMPAT_PCM1808    "ti,pcm1808"
#define COMPAT_NS4150B    "ns,ns4150b-amp"
#define COMPAT_TEMP_SENSOR "espressif,esp32p4-temp"
#define COMPAT_ST7735S     "sitronix,st7735s"
#define COMPAT_EC11        "alps,ec11"
#define COMPAT_BUZZER      "generic,piezo-buzzer"
#define COMPAT_LED         "generic,status-led"
#define COMPAT_RELAY       "generic,relay-amp"
#define COMPAT_BUTTON      "generic,tact-switch"
#define COMPAT_SIGGEN      "generic,signal-gen"

void hal_register_builtins() {
    hal_registry_init();

    // PCM5102A — primary DAC, I2S-only (no I2C control)
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_PCM5102A, 31);
        e.type = HAL_DEV_DAC;
        e.legacyId = 0x0001;  // DAC_ID_PCM5102A
        e.factory = nullptr;  // Created by dac_output_init(), not factory
        hal_registry_register(e);
    }

    // ES8311 — onboard codec (I2C + I2S2, P4 only)
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_ES8311, 31);
        e.type = HAL_DEV_CODEC;
        e.legacyId = 0x0004;  // DAC_ID_ES8311
        e.factory = nullptr;  // Created by dac_secondary_init()
        hal_registry_register(e);
    }

    // PCM1808 ADC — I2S-only (no I2C control)
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_PCM1808, 31);
        e.type = HAL_DEV_ADC;
        e.legacyId = 0;  // No legacy DAC_ID for ADC
        e.factory = nullptr;
        hal_registry_register(e);
    }

    // NS4150B — onboard mono amplifier (GPIO PA control)
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_NS4150B, 31);
        e.type = HAL_DEV_AMP;
        e.legacyId = 0;
        e.factory = nullptr;
        hal_registry_register(e);
    }

    // ESP32-P4 internal temperature sensor
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_TEMP_SENSOR, 31);
        e.type = HAL_DEV_SENSOR;
        e.legacyId = 0;
        e.factory = nullptr;
        hal_registry_register(e);
    }

    // ST7735S TFT Display
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_ST7735S, 31);
        e.type = HAL_DEV_DISPLAY;
        e.factory = nullptr;
        hal_registry_register(e);
    }

    // Rotary Encoder
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_EC11, 31);
        e.type = HAL_DEV_INPUT;
        e.factory = nullptr;
        hal_registry_register(e);
    }

    // Piezo Buzzer
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_BUZZER, 31);
        e.type = HAL_DEV_GPIO;
        e.factory = nullptr;
        hal_registry_register(e);
    }

    // Status LED
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_LED, 31);
        e.type = HAL_DEV_GPIO;
        e.factory = nullptr;
        hal_registry_register(e);
    }

    // Amplifier Relay
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_RELAY, 31);
        e.type = HAL_DEV_AMP;
        e.factory = nullptr;
        hal_registry_register(e);
    }

    // Reset Button
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_BUTTON, 31);
        e.type = HAL_DEV_INPUT;
        e.factory = nullptr;
        hal_registry_register(e);
    }

    // Signal Generator
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_SIGGEN, 31);
        e.type = HAL_DEV_GPIO;
        e.factory = nullptr;
        hal_registry_register(e);
    }
}

#endif // DAC_ENABLED
