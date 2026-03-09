#ifndef STATE_DSP_STATE_H
#define STATE_DSP_STATE_H

#include "config.h"

// DSP settings state (guarded by DSP_ENABLED at the AppState level)
// Named DspSettingsState to avoid collision with DspState in dsp_pipeline.h
struct DspSettingsState {
  bool enabled = false;       // Master DSP enable
  bool bypass = false;        // Master bypass (pass-through)

  // DSP Presets (up to 32 named slots)
  int8_t presetIndex = -1;           // -1 = custom/no preset, 0-31 = active preset
  char presetNames[DSP_PRESET_MAX_SLOTS][21] = {};  // 20 char max + null

  // DSP config swap diagnostics
  uint32_t swapFailures = 0;
  uint32_t swapSuccesses = 0;
  unsigned long lastSwapFailure = 0;
};

#endif // STATE_DSP_STATE_H
