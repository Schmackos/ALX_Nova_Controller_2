#include "signal_generator.h"
#include "config.h"
#include <cmath>

#ifndef NATIVE_TEST
#include "app_state.h"
#include "debug_serial.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
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
                // Update sweep frequency
                sweep_freq += sweep_inc;
                if (sweep_freq > p.sweepMax) {
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
            case SIGCHAN_LEFT:
                buf[li] = raw;
                buf[ri] = 0;
                break;
            case SIGCHAN_RIGHT:
                buf[li] = 0;
                buf[ri] = raw;
                break;
            case SIGCHAN_BOTH:
            default:
                buf[li] = raw;
                buf[ri] = raw;
                break;
        }

        // Advance phase
        _phase += phase_inc;
        if (_phase >= 1.0f) _phase -= 1.0f;
    }

    _sweepFreq = sweep_freq;
}

// ===== Hardware-dependent code (ESP32 only) =====
#ifndef NATIVE_TEST

void siggen_init() {
    // Setup LEDC PWM for signal generator output
    ledcSetup(SIGGEN_PWM_CHANNEL, 1000, SIGGEN_PWM_RESOLUTION);
    ledcAttachPin(SIGGEN_PWM_PIN, SIGGEN_PWM_CHANNEL);
    ledcWrite(SIGGEN_PWM_CHANNEL, 0);
    LOG_I("[SigGen] Initialized PWM on GPIO %d", SIGGEN_PWM_PIN);
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
    p.waveform = st.sigGenWaveform;
    p.frequency = st.sigGenFrequency;
    p.amplitude_linear = siggen_dbfs_to_linear(st.sigGenAmplitude);
    p.channel = st.sigGenChannel;
    p.outputMode = st.sigGenOutputMode;
    p.sweepSpeed = st.sigGenSweepSpeed;
    p.sweepMin = 20.0f;
    p.sweepMax = st.sigGenFrequency; // Sweep up to set frequency

    bool wasActive = _siggenActive;
    bool shouldBeActive = st.sigGenEnabled;

    portENTER_CRITICAL(&_siggenSpinlock);
    _params = p;
    portEXIT_CRITICAL(&_siggenSpinlock);

    _siggenActive = shouldBeActive;

    // Handle PWM mode
    if (shouldBeActive && p.outputMode == SIGOUT_PWM) {
        // Set PWM frequency to the signal frequency
        ledcSetup(SIGGEN_PWM_CHANNEL, (uint32_t)p.frequency, SIGGEN_PWM_RESOLUTION);
        // Duty cycle represents amplitude (512 = 50% = full amplitude for square)
        uint32_t duty = (uint32_t)(512.0f * p.amplitude_linear);
        ledcWrite(SIGGEN_PWM_CHANNEL, duty);
        LOG_I("[SigGen] PWM: %.0f Hz, duty=%lu", p.frequency, duty);
    } else if (!shouldBeActive && wasActive) {
        // Stop PWM output
        ledcWrite(SIGGEN_PWM_CHANNEL, 0);
        LOG_I("[SigGen] Stopped");
    }

    // Reset phase and sweep on enable
    if (shouldBeActive && !wasActive) {
        _phase = 0.0f;
        _sweepFreq = p.sweepMin;
        _noiseSeed = (uint32_t)millis();
        LOG_I("[SigGen] Started: waveform=%d, freq=%.0f Hz, amp=%.1f dBFS, mode=%s",
              p.waveform, p.frequency, st.sigGenAmplitude,
              p.outputMode == SIGOUT_SOFTWARE ? "software" : "PWM");
    }
}

#else
// Native test stubs
void siggen_init() {}
bool siggen_is_active() { return _siggenActive; }
bool siggen_is_software_mode() { return _params.outputMode == SIGOUT_SOFTWARE; }
void siggen_apply_params() {}

// Test helper: allow tests to set active/params directly
void siggen_test_set_active(bool active) { _siggenActive = active; }
void siggen_test_set_params(int waveform, float freq, float amp_dbfs,
                            int channel, int outputMode, float sweepSpeed) {
    _params.waveform = waveform;
    _params.frequency = freq;
    _params.amplitude_linear = siggen_dbfs_to_linear(amp_dbfs);
    _params.channel = channel;
    _params.outputMode = outputMode;
    _params.sweepSpeed = sweepSpeed;
    _params.sweepMin = 20.0f;
    _params.sweepMax = freq;
    _phase = 0.0f;
    _sweepFreq = 20.0f;
    _noiseSeed = 42; // Deterministic for tests
}
#endif // NATIVE_TEST
