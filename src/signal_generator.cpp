#include "signal_generator.h"
#include "config.h"
#include <cmath>

#ifndef NATIVE_TEST
#include "app_state.h"
#include "debug_serial.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <driver/mcpwm_prelude.h>
#else
#include <cstdlib>
#endif

// ===== Sine lookup table (256 entries, one full cycle) =====
static const int16_t SINE_LUT[256] = {
        0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
     6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
    18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
    32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285,
    32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683,
    27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868,
    18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278,  9512,  8739,  7962,  7179,
     6393,  5602,  4808,  4011,  3212,  2410,  1608,   804,
        0,  -804, -1608, -2410, -3212, -4011, -4808, -5602,
    -6393, -7179, -7962, -8739, -9512,-10278,-11039,-11793,
   -12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,
   -18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,
   -23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,
   -27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,
   -30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,
   -32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,
   -32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,
   -32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,
   -30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,
   -27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,
   -23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,
   -18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,
   -12539,-11793,-11039,-10278, -9512, -8739, -7962, -7179,
    -6393, -5602, -4808, -4011, -3212, -2410, -1608,  -804,
};

#define LUT_SIZE 256

// ===== Internal state =====
static volatile bool _siggenActive = false;

// Parameters snapshot (copied from AppState under spinlock)
struct SigGenParams {
    int waveform;
    float frequency;
    float amplitude_linear;
    int channel;
    int outputMode;
    float sweepSpeed;
    float sweepMin;
    float sweepMax;
};

static SigGenParams _params = {};
static float _phase = 0.0f;           // Phase accumulator [0, 1)
static float _sweepFreq = 0.0f;       // Current sweep frequency
static uint32_t _noiseSeed = 12345;   // PRNG seed

#ifndef NATIVE_TEST
static portMUX_TYPE _siggenSpinlock = portMUX_INITIALIZER_UNLOCKED;

// MCPWM peripheral handles for PWM output mode
static mcpwm_timer_handle_t _mcpwm_timer = NULL;
static mcpwm_oper_handle_t  _mcpwm_oper  = NULL;
static mcpwm_cmpr_handle_t  _mcpwm_cmpr  = NULL;
static mcpwm_gen_handle_t   _mcpwm_gen   = NULL;
#endif

// ===== Pure testable functions =====

float siggen_sine_sample(float phase) {
    // phase is [0, 1), map to LUT index with linear interpolation
    float idx_f = phase * LUT_SIZE;
    int idx = (int)idx_f;
    float frac = idx_f - idx;
    idx &= (LUT_SIZE - 1);
    int next = (idx + 1) & (LUT_SIZE - 1);
    float s0 = SINE_LUT[idx] / 32767.0f;
    float s1 = SINE_LUT[next] / 32767.0f;
    return s0 + frac * (s1 - s0);
}

float siggen_square_sample(float phase) {
    return (phase < 0.5f) ? 1.0f : -1.0f;
}

float siggen_noise_sample(uint32_t *seed) {
    // xorshift32 PRNG — deterministic, fast, no platform dependency
    uint32_t s = *seed;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    *seed = s;
    // Map to [-1.0, 1.0]
    return (float)(int32_t)s / 2147483648.0f;
}

float siggen_dbfs_to_linear(float dbfs) {
    if (dbfs <= -96.0f) return 0.0f;
    if (dbfs >= 0.0f) return 1.0f;
    return powf(10.0f, dbfs / 20.0f);
}

// ===== Buffer fill (called from I2S audio task or test code) =====

void siggen_fill_buffer(int32_t *buf, int stereo_frames, uint32_t sample_rate) {
    if (stereo_frames <= 0 || sample_rate == 0) return;

    SigGenParams p;
#ifndef NATIVE_TEST
    portENTER_CRITICAL(&_siggenSpinlock);
#endif
    p = _params;
#ifndef NATIVE_TEST
    portEXIT_CRITICAL(&_siggenSpinlock);
#endif

    float phase_inc = p.frequency / (float)sample_rate;
    float amp = p.amplitude_linear;

    // For sweep mode, track current frequency
    float sweep_freq = _sweepFreq;
    float sweep_inc = p.sweepSpeed / (float)sample_rate; // Hz per sample

    for (int f = 0; f < stereo_frames; f++) {
        float sample = 0.0f;

        switch (p.waveform) {
            case WAVE_SINE:
                sample = siggen_sine_sample(_phase);
                break;
            case WAVE_SQUARE:
                sample = siggen_square_sample(_phase);
                break;
            case WAVE_NOISE:
                sample = siggen_noise_sample(&_noiseSeed);
                break;
            case WAVE_SWEEP:
                sample = siggen_sine_sample(_phase);
                // Update sweep frequency (clamp to Nyquist to prevent phase_inc > 1.0)
                sweep_freq += sweep_inc;
                if (sweep_freq > p.sweepMax || sweep_freq > (float)sample_rate * 0.5f) {
                    sweep_freq = p.sweepMin;
                }
                phase_inc = sweep_freq / (float)sample_rate;
                break;
            default:
                break;
        }

        sample *= amp;

        // Convert to 24-bit left-justified I2S format (<<8)
        int32_t raw = (int32_t)(sample * 8388607.0f) << 8;

        // Write to interleaved stereo buffer based on channel selection
        int li = f * 2;
        int ri = f * 2 + 1;
        switch (p.channel) {
            case SIGCHAN_CH1:
                buf[li] = raw;
                buf[ri] = 0;
                break;
            case SIGCHAN_CH2:
                buf[li] = 0;
                buf[ri] = raw;
                break;
            case SIGCHAN_BOTH:
            default:
                buf[li] = raw;
                buf[ri] = raw;
                break;
        }

        // Advance phase — floorf handles phase_inc > 1.0 (high freq / sweep)
        _phase += phase_inc;
        _phase -= floorf(_phase);
    }

    _sweepFreq = sweep_freq;
}

// ===== Hardware-dependent code (ESP32 only) =====
#ifndef NATIVE_TEST

void siggen_init() {
    // Setup MCPWM peripheral for signal generator PWM output
    // Timer: 160 MHz clock, up-count, default period = 1 kHz
    mcpwm_timer_config_t timer_cfg = {};
    timer_cfg.group_id      = SIGGEN_MCPWM_GROUP;
    timer_cfg.clk_src       = MCPWM_TIMER_CLK_SRC_DEFAULT;
    timer_cfg.resolution_hz = SIGGEN_MCPWM_RESOLUTION;
    timer_cfg.count_mode    = MCPWM_TIMER_COUNT_MODE_UP;
    timer_cfg.period_ticks  = SIGGEN_MCPWM_RESOLUTION / 1000; // 1 kHz default
    mcpwm_new_timer(&timer_cfg, &_mcpwm_timer);

    mcpwm_operator_config_t oper_cfg = {};
    oper_cfg.group_id = SIGGEN_MCPWM_GROUP;
    mcpwm_new_operator(&oper_cfg, &_mcpwm_oper);
    mcpwm_operator_connect_timer(_mcpwm_oper, _mcpwm_timer);

    mcpwm_comparator_config_t cmpr_cfg = {};
    mcpwm_new_comparator(_mcpwm_oper, &cmpr_cfg, &_mcpwm_cmpr);

    mcpwm_generator_config_t gen_cfg = {};
    gen_cfg.gen_gpio_num = SIGGEN_PWM_PIN;
    mcpwm_new_generator(_mcpwm_oper, &gen_cfg, &_mcpwm_gen);

    // GPIO HIGH at timer zero, LOW at comparator match — standard PWM
    mcpwm_generator_set_action_on_timer_event(_mcpwm_gen,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                     MCPWM_TIMER_EVENT_EMPTY,
                                     MCPWM_GEN_ACTION_HIGH));
    mcpwm_generator_set_action_on_compare_event(_mcpwm_gen,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                       _mcpwm_cmpr,
                                       MCPWM_GEN_ACTION_LOW));

    // Start with zero duty (output stays low)
    mcpwm_comparator_set_compare_value(_mcpwm_cmpr, 0);

    mcpwm_timer_enable(_mcpwm_timer);
    mcpwm_timer_start_stop(_mcpwm_timer, MCPWM_TIMER_START_NO_STOP);

    LOG_I("[SigGen] Initialized MCPWM on GPIO %d", SIGGEN_PWM_PIN);
}

bool siggen_is_active() {
    return _siggenActive;
}

bool siggen_is_software_mode() {
    return _params.outputMode == SIGOUT_SOFTWARE;
}

void siggen_apply_params() {
    AppState &st = AppState::getInstance();

    SigGenParams p;
    p.waveform = st.sigGen.waveform;
    p.frequency = st.sigGen.frequency;
    p.amplitude_linear = siggen_dbfs_to_linear(st.sigGen.amplitude);
    p.channel = st.sigGen.channel;
    p.outputMode = st.sigGen.outputMode;
    p.sweepSpeed = st.sigGen.sweepSpeed;
    p.sweepMin = 20.0f;
    p.sweepMax = st.sigGen.frequency; // Sweep up to set frequency

    bool wasActive = _siggenActive;
    bool shouldBeActive = st.sigGen.enabled;

    portENTER_CRITICAL(&_siggenSpinlock);
    _params = p;
    portEXIT_CRITICAL(&_siggenSpinlock);

    _siggenActive = shouldBeActive;

    // Handle PWM mode
    if (shouldBeActive && p.outputMode == SIGOUT_PWM) {
        // Set PWM frequency and duty cycle via MCPWM
        uint32_t period = (uint32_t)((float)SIGGEN_MCPWM_RESOLUTION / p.frequency);
        if (period > 65535) period = 65535;   // clamp to 16-bit MCPWM timer max
        if (period < 2)     period = 2;       // minimum for meaningful PWM
        mcpwm_timer_set_period(_mcpwm_timer, period);
        uint32_t duty_ticks = (uint32_t)(period * 0.5f * p.amplitude_linear);
        mcpwm_comparator_set_compare_value(_mcpwm_cmpr, duty_ticks);
        LOG_I("[SigGen] PWM: %.0f Hz, duty_ticks=%lu", p.frequency, duty_ticks);
    } else if (!shouldBeActive && wasActive) {
        // Stop PWM output — set duty to zero
        mcpwm_comparator_set_compare_value(_mcpwm_cmpr, 0);
        LOG_I("[SigGen] Stopped");
    }

    // Reset phase and sweep on enable
    if (shouldBeActive && !wasActive) {
        _phase = 0.0f;
        _sweepFreq = p.sweepMin;
        _noiseSeed = (uint32_t)millis();
        LOG_I("[SigGen] Started: waveform=%d, freq=%.0f Hz, amp=%.1f dBFS, mode=%s",
              p.waveform, p.frequency, st.sigGen.amplitude,
              p.outputMode == SIGOUT_SOFTWARE ? "software" : "PWM");
    }

    if (shouldBeActive && wasActive) {
        LOG_I("[SigGen] Params: waveform=%d, freq=%.0f Hz, amp=%.1f dBFS, ch=%d",
              p.waveform, p.frequency, st.sigGen.amplitude, p.channel);
    }
}

#else
// Native test stubs
void siggen_init() {}
bool siggen_is_active() { return _siggenActive; }
bool siggen_is_software_mode() { return _params.outputMode == SIGOUT_SOFTWARE; }
void siggen_apply_params() {}
#endif // NATIVE_TEST
