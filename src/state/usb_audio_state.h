#ifndef STATE_USB_AUDIO_STATE_H
#define STATE_USB_AUDIO_STATE_H

#include "config.h"

// All USB Audio state fields
struct UsbAudioState {
  bool enabled = false;        // USB audio enable (persisted, default off -- avoids EMI when unused)
  bool connected = false;      // USB host connected
  bool streaming = false;      // Host is actively sending audio
  uint32_t sampleRate = 48000;
  uint8_t bitDepth = 16;
  uint8_t channels = 2;
  int16_t volume = 0;          // Host volume in 1/256 dB units (-32768 to 0)
  bool mute = false;           // Host mute state
  uint32_t bufferUnderruns = 0;
  uint32_t bufferOverruns = 0;

  // VU metering (written by Core 1 pipeline task via markUsbAudioVuDirty)
  float vuL = -90.0f;
  float vuR = -90.0f;

  // Dynamically negotiated format (set by control_xfer_cb on SET_CUR)
  uint32_t negotiatedRate  = 48000;
  uint8_t  negotiatedDepth = 16;
};

#endif // STATE_USB_AUDIO_STATE_H
