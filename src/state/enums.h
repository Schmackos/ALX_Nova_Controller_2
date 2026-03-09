#ifndef STATE_ENUMS_H
#define STATE_ENUMS_H

#include <stdint.h>

// ===== FFT Window Types =====
enum FftWindowType : uint8_t {
  FFT_WINDOW_HANN = 0,
  FFT_WINDOW_BLACKMAN,
  FFT_WINDOW_BLACKMAN_HARRIS,
  FFT_WINDOW_BLACKMAN_NUTTALL,
  FFT_WINDOW_NUTTALL,
  FFT_WINDOW_FLAT_TOP,
  FFT_WINDOW_COUNT
};

// ===== FSM Application States =====
enum AppFSMState {
  STATE_IDLE,
  STATE_SIGNAL_DETECTED,
  STATE_AUTO_OFF_TIMER,
  STATE_WEB_CONFIG,
  STATE_OTA_UPDATE,
  STATE_ERROR
};

// ===== Network Interface =====
enum NetIfType { NET_NONE, NET_ETHERNET, NET_WIFI };

#endif // STATE_ENUMS_H
