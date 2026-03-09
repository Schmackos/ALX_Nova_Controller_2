#ifdef DAC_ENABLED

#include "hal_siggen.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../signal_generator.h"
#include "../app_state.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
// Stubs for native test
inline void siggen_init(int) {}
inline void siggen_deinit() {}
inline bool siggen_is_active() { return false; }
inline bool siggen_is_software_mode() { return false; }
inline void siggen_fill_buffer(int32_t*, int, uint32_t) {}
#endif

// ===== Static callbacks for AudioInputSource =====

static uint32_t _siggen_read(int32_t *dst, uint32_t frames) {
    if (!siggen_is_active() || !siggen_is_software_mode()) {
        memset(dst, 0, frames * 2 * sizeof(int32_t));
        return frames;
    }
#ifndef NATIVE_TEST
    siggen_fill_buffer(dst, (int)frames, AppState::getInstance().audio.sampleRate);
#else
    siggen_fill_buffer(dst, (int)frames, 48000);
#endif
    return frames;
}

static bool _siggen_isActive(void) {
    return siggen_is_active() && siggen_is_software_mode();
}

static uint32_t _siggen_getSampleRate(void) {
#ifndef NATIVE_TEST
    return AppState::getInstance().audio.sampleRate;
#else
    return 48000;
#endif
}

// ===== HalSigGen implementation =====

HalSigGen::HalSigGen() : HalDevice() {
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "alx,signal-gen", 31);
    strncpy(_descriptor.name, "Signal Generator", 32);
    strncpy(_descriptor.manufacturer, "ALX", 32);
    _descriptor.type = HAL_DEV_ADC;
    _descriptor.legacyId = 0;
    _descriptor.channelCount = 2;
    _descriptor.bus.type = HAL_BUS_INTERNAL;
    _descriptor.bus.index = 0;
    _descriptor.sampleRatesMask = HAL_RATE_48K | HAL_RATE_96K;
    _descriptor.capabilities = HAL_CAP_ADC_PATH;
    _initPriority = HAL_PRIORITY_DATA;

    // Initialize AudioInputSource
    memset(&_source, 0, sizeof(_source));
    _source.name = "Signal Gen";
    _source.lane = 0;           // Set by bridge before registration
    _source.read = _siggen_read;
    _source.isActive = _siggen_isActive;
    _source.getSampleRate = _siggen_getSampleRate;
    _source.gainLinear = 1.0f;
    _source.vuL = -90.0f;
    _source.vuR = -90.0f;
    _source._vuSmoothedL = 0.0f;
    _source._vuSmoothedR = 0.0f;
    _source.halSlot = 0xFF;     // Set by bridge
}

bool HalSigGen::probe() {
    // Software device — always available
    return true;
}

HalInitResult HalSigGen::init() {
    int pwmPin = -1;  // -1 = use SIGGEN_PWM_PIN default
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg && cfg->valid && cfg->gpioA >= 0) {
        pwmPin = cfg->gpioA;
        _descriptor.bus.pinA = pwmPin;
    }
    siggen_init(pwmPin);
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HAL:SigGen] Initialized (instance %u, priority %u, pwm=%d)",
          _descriptor.instanceId, _initPriority, pwmPin);
    return hal_init_ok();
}

void HalSigGen::deinit() {
    siggen_deinit();
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL:SigGen] Deinitialized");
}

void HalSigGen::dumpConfig() {
    LOG_I("[HAL:SigGen] %s (compat=%s) bus=INTERNAL cap=ADC_PATH",
          _descriptor.name, _descriptor.compatible);
}

bool HalSigGen::healthCheck() {
    // Software device — always healthy
    return true;
}

const AudioInputSource* HalSigGen::getInputSource() const {
    return &_source;
}

#endif // DAC_ENABLED
