#ifdef DAC_ENABLED

#include "hal_builtin_devices.h"
#include "hal_driver_registry.h"
#include "hal_types.h"
#include "hal_es8311.h"
#include "hal_pcm5102a.h"
#include "hal_pcm1808.h"
#include "hal_dsp_bridge.h"
#include "hal_mcp4725.h"
#include "hal_siggen.h"
#include "hal_buzzer.h"
#include "hal_button.h"
#include "hal_encoder.h"
#include "hal_ns4150b.h"
#include "../config.h"
#include "../debug_serial.h"
#include "../drivers/es8311_regs.h"
#ifdef USB_AUDIO_ENABLED
#include "hal_usb_audio.h"
#endif
#include <string.h>

// ===== Factory functions for new platform classes =====
static HalDevice* factory_es8311()   { return new HalEs8311(); }
static HalDevice* factory_pcm5102a() { return new HalPcm5102a(); }
static HalDevice* factory_pcm1808()  { return new HalPcm1808(); }
static HalDevice* factory_dsp()      { return new HalDspBridge(); }
static HalDevice* factory_mcp4725()  { return new HalMcp4725(); }
static HalDevice* factory_siggen()   { return new HalSigGen(); }
static HalDevice* factory_buzzer()   { return new HalBuzzer(BUZZER_PIN); }
static HalDevice* factory_button()   { return new HalButton(RESET_BUTTON_PIN); }
static HalDevice* factory_encoder()  { return new HalEncoder(ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_SW_PIN); }
static HalDevice* factory_ns4150b()  { return new HalNs4150b(ES8311_PA_PIN); }
#ifdef USB_AUDIO_ENABLED
static HalDevice* factory_usb_audio() { return new HalUsbAudio(); }
#endif

// ===== Compatible strings for builtin devices =====
// Uses Linux DT "vendor,model" convention
#define COMPAT_PCM5102A   "ti,pcm5102a"
#define COMPAT_ES8311     "everest-semi,es8311"   // Updated to match HalEs8311 descriptor
#define COMPAT_ES8311_ALT "evergrande,es8311"     // Legacy alias kept for DB lookup
#define COMPAT_PCM1808    "ti,pcm1808"
#define COMPAT_NS4150B    "ns,ns4150b-amp"
#define COMPAT_TEMP_SENSOR "espressif,esp32p4-temp"
#define COMPAT_ST7735S     "sitronix,st7735s"
#define COMPAT_EC11        "alps,ec11"
#define COMPAT_BUZZER      "generic,piezo-buzzer"
#define COMPAT_LED         "generic,status-led"
#define COMPAT_RELAY       "generic,relay-amp"
#define COMPAT_BUTTON      "generic,tact-switch"
#define COMPAT_SIGGEN      "alx,signal-gen"
#define COMPAT_MCP4725     "microchip,mcp4725"

void hal_register_builtins() {
    hal_registry_init();

    // PCM5102A — primary DAC, I2S-only (no I2C control)
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_PCM5102A, 31);
        e.type = HAL_DEV_DAC;
        e.legacyId = 0x0001;  // DAC_ID_PCM5102A
        e.factory = factory_pcm5102a;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // ES8311 — onboard codec (I2C + I2S2, P4 only)
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_ES8311, 31);
        e.type = HAL_DEV_CODEC;
        e.legacyId = 0x0004;  // DAC_ID_ES8311
        e.factory = factory_es8311;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // ES8311 legacy compatible alias (evergrande vs everest-semi)
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_ES8311_ALT, 31);
        e.type = HAL_DEV_CODEC;
        e.legacyId = 0x0004;
        e.factory = factory_es8311;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // PCM1808 ADC — I2S-only (no I2C control)
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_PCM1808, 31);
        e.type = HAL_DEV_ADC;
        e.legacyId = 0;  // No legacy DAC_ID for ADC
        e.factory = factory_pcm1808;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // DSP Pipeline bridge
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, "alx,dsp-pipeline", 31);
        e.type = HAL_DEV_DSP;
        e.legacyId = 0;
        e.factory = factory_dsp;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // NS4150B — onboard mono amplifier (GPIO PA control)
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_NS4150B, 31);
        e.type = HAL_DEV_AMP;
        e.legacyId = 0;
        e.factory = factory_ns4150b;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // ESP32-P4 internal temperature sensor
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_TEMP_SENSOR, 31);
        e.type = HAL_DEV_SENSOR;
        e.legacyId = 0;
        e.factory = nullptr;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // ST7735S TFT Display
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_ST7735S, 31);
        e.type = HAL_DEV_DISPLAY;
        e.factory = nullptr;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // Rotary Encoder
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_EC11, 31);
        e.type = HAL_DEV_INPUT;
        e.factory = factory_encoder;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // Piezo Buzzer
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_BUZZER, 31);
        e.type = HAL_DEV_GPIO;
        e.factory = factory_buzzer;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // Status LED
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_LED, 31);
        e.type = HAL_DEV_GPIO;
        e.factory = nullptr;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // Amplifier Relay
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_RELAY, 31);
        e.type = HAL_DEV_AMP;
        e.factory = nullptr;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // Reset Button
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_BUTTON, 31);
        e.type = HAL_DEV_INPUT;
        e.factory = factory_button;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // Signal Generator — software audio source (HAL_DEV_ADC for pipeline input lane)
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, "alx,signal-gen", 31);
        e.type = HAL_DEV_ADC;
        e.legacyId = 0;
        e.factory = factory_siggen;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }

    // USB Audio — software audio source (HAL_DEV_ADC for pipeline input lane)
#ifdef USB_AUDIO_ENABLED
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, "alx,usb-audio", 31);
        e.type = HAL_DEV_ADC;
        e.legacyId = 0;
        e.factory = factory_usb_audio;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }
#endif

    // MCP4725 — 12-bit I2C voltage output DAC (add-on module)
    {
        HalDriverEntry e;
        memset(&e, 0, sizeof(e));
        strncpy(e.compatible, COMPAT_MCP4725, 31);
        e.type = HAL_DEV_DAC;
        e.legacyId = 0;
        e.factory = factory_mcp4725;
        if (!hal_registry_register(e)) { LOG_W("[HAL] Failed to register driver: %s", e.compatible); }
    }
}

#endif // DAC_ENABLED
