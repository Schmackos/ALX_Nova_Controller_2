#ifndef STATE_DEBUG_STATE_H
#define STATE_DEBUG_STATE_H

#include "config.h"

// Debug mode toggles and hardware stats config
struct DebugState {
  bool debugMode = true;           // Master debug gate
  int serialLevel = 2;             // 0=Off, 1=Errors, 2=Info, 3=Debug
  bool hwStats = true;             // HW stats WS broadcast + web tab
  bool i2sMetrics = true;          // I2S runtime metrics in audio task
  bool taskMonitor = false;        // Task monitor update & serial print (opt-in)
  unsigned long hardwareStatsInterval = HARDWARE_STATS_INTERVAL;
  bool heapCritical = false;       // True when largest free block < HEAP_CRITICAL_THRESHOLD
  bool heapWarning = false;        // True when largest free block < HEAP_WARNING_THRESHOLD but >= HEAP_CRITICAL_THRESHOLD
  bool psramWarning  = false;      // True when free PSRAM < PSRAM_WARNING_THRESHOLD
  bool psramCritical = false;      // True when free PSRAM < PSRAM_CRITICAL_THRESHOLD
};

#endif // STATE_DEBUG_STATE_H
