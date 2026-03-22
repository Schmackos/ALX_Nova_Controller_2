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
#include "hal_led.h"
#include "hal_relay.h"
#include "hal_es9822pro.h"
#include "hal_es9843pro.h"
#include "hal_es9826.h"
#include "hal_es9821.h"
#include "hal_es9823pro.h"
#include "hal_es9820.h"
#include "hal_es9842pro.h"
#include "hal_es9840.h"
#include "hal_es9841.h"
#include "../config.h"
#include "../debug_serial.h"
#include "../drivers/es8311_regs.h"
#ifdef USB_AUDIO_ENABLED
#include "hal_usb_audio.h"
#endif
#include <string.h>

// ===== Registration macro — reduces each 7-line block to a single call =====
// Declares a local HalDriverEntry on the stack, fills it, and registers it.
// Logs a warning if registration fails (registry full).
#define HAL_REGISTER(compat, devType, legacy, factoryFn) do { \
    HalDriverEntry _e; memset(&_e, 0, sizeof(_e)); \
    strncpy(_e.compatible, compat, 31); \
    _e.type = devType; _e.legacyId = legacy; _e.factory = factoryFn; \
    if (!hal_registry_register(_e)) { LOG_W("[HAL] Register failed: %s", _e.compatible); } \
} while(0)

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
static HalDevice* factory_led()      { return new HalLed(LED_PIN); }
static HalDevice* factory_relay()    { return new HalRelay(AMPLIFIER_PIN); }
static HalDevice* factory_es9822pro() { return new HalEs9822pro(); }
static HalDevice* factory_es9843pro() { return new HalEs9843pro(); }
static HalDevice* factory_es9826()    { return new HalEs9826(); }
static HalDevice* factory_es9821()    { return new HalEs9821(); }
static HalDevice* factory_es9823pro() { return new HalEs9823pro(); }
static HalDevice* factory_es9820()    { return new HalEs9820(); }
static HalDevice* factory_es9842pro() { return new HalEs9842pro(); }
static HalDevice* factory_es9840()    { return new HalEs9840(); }
static HalDevice* factory_es9841()    { return new HalEs9841(); }
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
#define COMPAT_ES9822PRO   "ess,es9822pro"
#define COMPAT_ES9843PRO   "ess,es9843pro"
#define COMPAT_ES9826      "ess,es9826"
#define COMPAT_ES9821      "ess,es9821"
#define COMPAT_ES9823PRO   "ess,es9823pro"
#define COMPAT_ES9823MPRO  "ess,es9823mpro"
#define COMPAT_ES9820      "ess,es9820"
#define COMPAT_ES9842PRO   "ess,es9842pro"
#define COMPAT_ES9840      "ess,es9840"
#define COMPAT_ES9841      "ess,es9841"

void hal_register_builtins() {
    hal_registry_init();

    // Core audio devices
    HAL_REGISTER(COMPAT_PCM5102A,  HAL_DEV_DAC,     0x0001, factory_pcm5102a);  // primary DAC, I2S-only
    HAL_REGISTER(COMPAT_ES8311,    HAL_DEV_CODEC,   0x0004, factory_es8311);    // onboard codec (I2C + I2S)
    HAL_REGISTER(COMPAT_ES8311_ALT,HAL_DEV_CODEC,   0x0004, factory_es8311);    // legacy alias (evergrande)
    HAL_REGISTER(COMPAT_PCM1808,   HAL_DEV_ADC,     0,      factory_pcm1808);   // I2S-only ADC

    // ESS SABRE expansion ADCs (2-channel I2S, Pattern A)
    HAL_REGISTER(COMPAT_ES9822PRO, HAL_DEV_ADC,     0,      factory_es9822pro); // PGA 0-18dB, HPF
    HAL_REGISTER(COMPAT_ES9826,    HAL_DEV_ADC,     0,      factory_es9826);    // PGA 0-30dB
    HAL_REGISTER(COMPAT_ES9823PRO, HAL_DEV_ADC,     0,      factory_es9823pro); // PGA 0-42dB, highest 2ch
    HAL_REGISTER(COMPAT_ES9823MPRO,HAL_DEV_ADC,     0,      factory_es9823pro); // monolithic variant
    HAL_REGISTER(COMPAT_ES9821,    HAL_DEV_ADC,     0,      factory_es9821);    // no PGA
    HAL_REGISTER(COMPAT_ES9820,    HAL_DEV_ADC,     0,      factory_es9820);    // PGA 0-18dB, entry

    // ESS SABRE expansion ADCs (4-channel TDM, Pattern B)
    HAL_REGISTER(COMPAT_ES9843PRO, HAL_DEV_ADC,     0,      factory_es9843pro); // PGA 0-42dB
    HAL_REGISTER(COMPAT_ES9842PRO, HAL_DEV_ADC,     0,      factory_es9842pro); // PGA 0-18dB
    HAL_REGISTER(COMPAT_ES9841,    HAL_DEV_ADC,     0,      factory_es9841);    // PGA 0-42dB, 8-bit vol
    HAL_REGISTER(COMPAT_ES9840,    HAL_DEV_ADC,     0,      factory_es9840);    // entry-tier 4ch

    // Software and internal devices
    HAL_REGISTER("alx,dsp-pipeline", HAL_DEV_DSP,   0,      factory_dsp);       // DSP pipeline bridge
    HAL_REGISTER("alx,signal-gen",   HAL_DEV_ADC,   0,      factory_siggen);    // software audio source
    HAL_REGISTER(COMPAT_MCP4725,  HAL_DEV_DAC,      0,      factory_mcp4725);   // 12-bit I2C DAC

    // GPIO-controlled peripherals
    HAL_REGISTER(COMPAT_NS4150B,  HAL_DEV_AMP,      0,      factory_ns4150b);   // mono amp (GPIO PA)
    HAL_REGISTER(COMPAT_RELAY,    HAL_DEV_AMP,      0,      factory_relay);     // amplifier relay
    HAL_REGISTER(COMPAT_BUZZER,   HAL_DEV_GPIO,     0,      factory_buzzer);    // piezo buzzer
    HAL_REGISTER(COMPAT_LED,      HAL_DEV_GPIO,     0,      factory_led);       // status LED
    HAL_REGISTER(COMPAT_BUTTON,   HAL_DEV_INPUT,    0,      factory_button);    // reset button
    HAL_REGISTER(COMPAT_EC11,     HAL_DEV_INPUT,    0,      factory_encoder);   // rotary encoder

    // Sensor / display (no factory — metadata-only entries)
    HAL_REGISTER(COMPAT_TEMP_SENSOR, HAL_DEV_SENSOR,  0,    nullptr);           // ESP32-P4 internal temp
    HAL_REGISTER(COMPAT_ST7735S,     HAL_DEV_DISPLAY, 0,    nullptr);           // TFT display

    // USB Audio — software audio source (guarded: requires USB_AUDIO_ENABLED build flag)
#ifdef USB_AUDIO_ENABLED
    HAL_REGISTER("alx,usb-audio", HAL_DEV_ADC,     0,      factory_usb_audio);
#endif
}

#endif // DAC_ENABLED
