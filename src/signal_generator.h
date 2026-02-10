#ifndef SIGNAL_GENERATOR_H
#define SIGNAL_GENERATOR_H

#include <stdint.h>

// ===== Enums =====
enum SignalWaveform  { WAVE_SINE = 0, WAVE_SQUARE, WAVE_NOISE, WAVE_SWEEP, WAVE_COUNT };
enum SignalOutputMode { SIGOUT_SOFTWARE = 0, SIGOUT_PWM };
enum SignalChannel   { SIGCHAN_LEFT = 0, SIGCHAN_RIGHT, SIGCHAN_BOTH };
enum SignalTargetAdc { SIGTARGET_ADC1 = 0, SIGTARGET_ADC2, SIGTARGET_BOTH };

// ===== Public API =====
void siggen_init();
bool siggen_is_active();
bool siggen_is_software_mode();
void siggen_apply_params();

// Fill an interleaved stereo I2S buffer (int32_t L,R,L,R...)
// stereo_frames = number of L+R frame pairs
void siggen_fill_buffer(int32_t *buf, int stereo_frames, uint32_t sample_rate);

// ===== Pure testable functions =====
float siggen_sine_sample(float phase);
float siggen_square_sample(float phase);
float siggen_noise_sample(uint32_t *seed);
float siggen_dbfs_to_linear(float dbfs);

#endif // SIGNAL_GENERATOR_H
