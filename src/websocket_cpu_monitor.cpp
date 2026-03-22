// websocket_cpu_monitor.cpp — CPU utilization tracking via FreeRTOS idle hooks
// Extracted from websocket_handler.cpp for modularity.
// Functions declared in websocket_handler.h.

#include "websocket_handler.h"
#include "esp_freertos_hooks.h"
#include "esp_timer.h"

// ===== CPU Utilization Tracking =====
// Uses FreeRTOS idle hooks with microsecond wall-clock timing.
// Each hook accumulates actual wall-clock microseconds spent in idle,
// not iteration counts (which are affected by WiFi interrupt overhead).
static volatile int64_t idleTimeUs0 = 0;   // Accumulated idle microseconds
static volatile int64_t idleTimeUs1 = 0;
static int64_t lastIdleTimeUs0 = 0;        // Previous snapshot
static int64_t lastIdleTimeUs1 = 0;
static int64_t lastCpuMeasureTimeUs = 0;   // Wall clock at last measurement
static float cpuUsageCore0 = -1.0f;
static float cpuUsageCore1 = -1.0f;
static bool cpuHooksInstalled = false;
static int cpuWarmupCycles = 0;            // Skip first 2 measurements for stability

// Track entry time per-core (local to each core, no cross-core cache contention)
static volatile int64_t idleEntryUs0 = 0;
static volatile int64_t idleEntryUs1 = 0;

// Idle hook: measure wall-clock time between calls using esp_timer_get_time().
// Each call = one iteration of the idle task loop. We accumulate the delta
// between consecutive calls, which represents time spent in idle (not in ISRs/tasks).
static bool idleHookCore0() {
  int64_t now = esp_timer_get_time();
  if (idleEntryUs0 > 0) {
    int64_t delta = now - idleEntryUs0;
    // Only count short deltas (<1ms) — longer gaps mean we were preempted
    if (delta < 1000) idleTimeUs0 += delta;
  }
  idleEntryUs0 = now;
  return false;
}

static bool idleHookCore1() {
  int64_t now = esp_timer_get_time();
  if (idleEntryUs1 > 0) {
    int64_t delta = now - idleEntryUs1;
    if (delta < 1000) idleTimeUs1 += delta;
  }
  idleEntryUs1 = now;
  return false;
}

// ===== CPU Utilization Functions =====

void initCpuUsageMonitoring() {
  if (!cpuHooksInstalled) {
    esp_register_freertos_idle_hook_for_cpu(idleHookCore0, 0);
    esp_register_freertos_idle_hook_for_cpu(idleHookCore1, 1);
    cpuHooksInstalled = true;
    cpuWarmupCycles = 0;
    idleTimeUs0 = 0;
    idleTimeUs1 = 0;
    idleEntryUs0 = 0;
    idleEntryUs1 = 0;
    lastIdleTimeUs0 = 0;
    lastIdleTimeUs1 = 0;
    lastCpuMeasureTimeUs = esp_timer_get_time();
    cpuUsageCore0 = -1.0f;
    cpuUsageCore1 = -1.0f;
  }
}

void deinitCpuUsageMonitoring() {
  if (cpuHooksInstalled) {
    esp_deregister_freertos_idle_hook_for_cpu(idleHookCore0, 0);
    esp_deregister_freertos_idle_hook_for_cpu(idleHookCore1, 1);
    cpuHooksInstalled = false;
    cpuUsageCore0 = -1.0f;
    cpuUsageCore1 = -1.0f;
  }
}

void updateCpuUsage() {
  if (!cpuHooksInstalled) {
    initCpuUsageMonitoring();
    return;
  }

  int64_t nowUs = esp_timer_get_time();
  int64_t elapsedUs = nowUs - lastCpuMeasureTimeUs;

  // Only update every 2 seconds for stable readings
  if (elapsedUs < 2000000) return;

  // Snapshot idle accumulations (volatile reads)
  int64_t curIdle0 = idleTimeUs0;
  int64_t curIdle1 = idleTimeUs1;

  // Delta idle microseconds since last measurement
  int64_t deltaIdle0 = curIdle0 - lastIdleTimeUs0;
  int64_t deltaIdle1 = curIdle1 - lastIdleTimeUs1;

  lastIdleTimeUs0 = curIdle0;
  lastIdleTimeUs1 = curIdle1;
  lastCpuMeasureTimeUs = nowUs;

  // Skip the first 2 cycles — hooks need time to accumulate stable data
  if (cpuWarmupCycles < 2) {
    cpuWarmupCycles++;
    cpuUsageCore0 = -1.0f;
    cpuUsageCore1 = -1.0f;
    return;
  }

  // CPU = 100% - (idle_time / total_time * 100%)
  // idle_time is actual microseconds the idle task ran (not counting ISR time)
  // total_time is wall-clock elapsed microseconds
  if (elapsedUs > 0) {
    float idlePct0 = (float)deltaIdle0 / (float)elapsedUs * 100.0f;
    float idlePct1 = (float)deltaIdle1 / (float)elapsedUs * 100.0f;
    cpuUsageCore0 = 100.0f - idlePct0;
    cpuUsageCore1 = 100.0f - idlePct1;

    // Clamp
    if (cpuUsageCore0 < 0) cpuUsageCore0 = 0;
    if (cpuUsageCore0 > 100) cpuUsageCore0 = 100;
    if (cpuUsageCore1 < 0) cpuUsageCore1 = 0;
    if (cpuUsageCore1 > 100) cpuUsageCore1 = 100;
  }
}

float getCpuUsageCore0() {
  return cpuUsageCore0;
}

float getCpuUsageCore1() {
  return cpuUsageCore1;
}
