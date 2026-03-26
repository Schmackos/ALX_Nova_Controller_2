#ifndef STATE_AUDIO_STATE_H
#define STATE_AUDIO_STATE_H

#include "config.h"
#include "state/enums.h"

// Per-ADC audio analysis state
struct AdcState {
  float rms1 = 0.0f, rms2 = 0.0f, rmsCombined = 0.0f;
  float vu1 = 0.0f, vu2 = 0.0f, vuCombined = 0.0f;
  float peak1 = 0.0f, peak2 = 0.0f, peakCombined = 0.0f;
  float vrms1 = 0.0f, vrms2 = 0.0f, vrmsCombined = 0.0f;
  float dBFS = -96.0f;
  // Diagnostics
  uint8_t healthStatus = 0;      // AudioHealthStatus enum value
  uint32_t i2sErrors = 0;
  uint32_t allZeroBuffers = 0;
  uint32_t consecutiveZeros = 0;
  float noiseFloorDbfs = -96.0f;
  float dcOffset = 0.0f;
  unsigned long lastNonZeroMs = 0;
  uint32_t totalBuffers = 0;
  uint32_t clippedSamples = 0;
  float clipRate = 0.0f;           // EMA clip rate (0.0-1.0)
  uint32_t i2sRecoveries = 0;     // I2S driver restart count (timeout recovery)
};

// I2S runtime metrics (written by audio task, read by diagnostics)
struct I2sRuntimeMetrics {
  uint32_t audioTaskStackFree = 0;           // bytes remaining (high watermark x 4)
  float buffersPerSec[AUDIO_PIPELINE_MAX_INPUTS] = {};  // actual buf/s per ADC
  float avgReadLatencyUs[AUDIO_PIPELINE_MAX_INPUTS] = {};// avg i2s_read() time in us
};

// Audio and smart sensing state
struct AudioState {
  // Smart sensing
  SensingMode currentMode = ALWAYS_ON;
  unsigned long timerDuration = DEFAULT_TIMER_DURATION;
  unsigned long timerRemaining = 0;
  unsigned long lastSignalDetection = 0;
  unsigned long lastTimerUpdate = 0;
  float threshold_dBFS = DEFAULT_AUDIO_THRESHOLD;
  bool amplifierState = false;
  float level_dBFS = -96.0f;
  bool previousSignalState = false;
  unsigned long lastSmartSensingHeartbeat = 0;

  // Per-ADC state
  AdcState adc[AUDIO_PIPELINE_MAX_INPUTS];
  int numAdcsDetected = 1;
  int activeInputCount = 0;   // HAL-driven: number of audio input lanes currently mapped
  int activeOutputCount = 0;  // HAL-driven: number of audio output sinks currently mapped

  // DMA buffer allocation tracking
  bool dmaAllocFailed = false;        // True if any DMA buffer alloc failed
  uint16_t dmaAllocFailMask = 0;      // Bits 0-7: rawBuf lanes, bits 8-15: sinkBuf slots

  // I2S metrics
  I2sRuntimeMetrics i2sMetrics;

  // Audio processing
  float dominantFreq = 0.0f;
  float spectrumBands[16] = {};
  uint32_t sampleRate = DEFAULT_AUDIO_SAMPLE_RATE;
  float adcVref = DEFAULT_ADC_VREF;
  bool adcEnabled[AUDIO_PIPELINE_MAX_INPUTS] = {true, true};
  volatile bool paused = false;  // Cross-core: written Core 0, read Core 1
#ifndef UNIT_TEST
  SemaphoreHandle_t taskPausedAck = nullptr;
#endif

  // Input channel names (user-configurable)
  char inputNames[AUDIO_PIPELINE_MAX_INPUTS * 2][32] = {};

  // Audio update rate
  uint16_t updateRate = DEFAULT_AUDIO_UPDATE_RATE;

  // Audio graph toggles
  bool vuMeterEnabled = true;
  bool waveformEnabled = true;
  bool spectrumEnabled = true;

  // FFT window type
  FftWindowType fftWindowType = FFT_WINDOW_HANN;

  // ADC signal quality metrics
  float snrDb[AUDIO_PIPELINE_MAX_INPUTS] = {};
  float sfdrDb[AUDIO_PIPELINE_MAX_INPUTS] = {};

  // Format negotiation state (Phase 1+2 hardening)
  bool     rateMismatch = false;                          // True when input/output sample rates differ
  uint32_t laneSampleRates[AUDIO_PIPELINE_MAX_INPUTS] = {}; // Per-lane sample rate (Hz, 0=unknown)
  bool     laneDsd[AUDIO_PIPELINE_MAX_INPUTS] = {};      // Per-lane DoP DSD detection flag
  bool     laneSrcActive[AUDIO_PIPELINE_MAX_INPUTS] = {}; // True when ASRC is active for this lane
};

#endif // STATE_AUDIO_STATE_H
