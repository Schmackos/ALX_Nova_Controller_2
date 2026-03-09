#ifndef STATE_SIGNAL_GEN_STATE_H
#define STATE_SIGNAL_GEN_STATE_H

#include "config.h"

// Signal generator parameters extracted from AppState
struct SignalGenState {
  bool enabled = false;           // Always boots false
  int waveform = 0;               // 0=sine, 1=square, 2=noise, 3=sweep
  float frequency = 1000.0f;      // 1.0 - 22000.0 Hz
  float amplitude = -6.0f;        // -96.0 to 0.0 dBFS
  int channel = 2;                // 0=Ch1, 1=Ch2, 2=Both
  int outputMode = 0;             // 0=software, 1=PWM
  float sweepSpeed = 1000.0f;     // Hz per second
};

#endif // STATE_SIGNAL_GEN_STATE_H
