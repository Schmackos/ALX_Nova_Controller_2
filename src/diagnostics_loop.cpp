#include "diagnostics_loop.h"
#include "app_state.h"
#include "app_events.h"
#include "config.h"
#include "debug_serial.h"
#include "diag_journal.h"
#include "diag_event.h"
#include "health_check.h"
#include "task_monitor.h"
#include "audio_pipeline.h"
#ifdef DAC_ENABLED
#include "hal/hal_device_manager.h"
#include "hal/hal_audio_health_bridge.h"
#include "hal/hal_audio_device.h"
#endif
#include <Arduino.h>

void diagnostics_loop_tick() {
  // Periodic journal flush (WARN+ entries to LittleFS, every 60s)
  {
    static unsigned long lastJournalFlush = 0;
    if (millis() - lastJournalFlush >= 60000) {
      diag_journal_flush();
      lastJournalFlush = millis();
    }
  }

  // Periodic health check re-run (every 60s)
  {
    static unsigned long lastHealthCheck = 0;
    if (millis() - lastHealthCheck >= 60000) {
      lastHealthCheck = millis();
      health_check_run_full(nullptr);  // Uses internal static report
    }
  }

#ifdef DAC_ENABLED
  // HAL periodic health check (every 30s, aligned with heap check)
  static unsigned long lastHalHealthCheck = 0;
  if (millis() - lastHalHealthCheck >= 30000) {
    lastHalHealthCheck = millis();
    HalDeviceManager::instance().healthCheckAll();
  }

  // Audio → HAL health bridge (every 5s, aligned with audio_periodic_dump)
  {
    static unsigned long lastAudioHealthBridge = 0;
    if (millis() - lastAudioHealthBridge >= 5000) {
      lastAudioHealthBridge = millis();
      hal_audio_health_check();
    }
  }

  // Format negotiation check (every 5s) — detects source/sink sample rate mismatches
  {
    static unsigned long lastFormatCheck = 0;
    if (millis() - lastFormatCheck >= 5000) {
      lastFormatCheck = millis();
      bool mismatch = audio_pipeline_check_format();

      // ASRC lane configuration — when a rate mismatch is detected, arm the SRC
      // engine for each affected lane. When resolved, deactivate all lanes.
      static uint32_t prevLaneSampleRates[AUDIO_PIPELINE_MAX_INPUTS] = {};
      bool ratesChanged = false;
      for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        if (prevLaneSampleRates[lane] != appState.audio.laneSampleRates[lane]) {
          ratesChanged = true;
          prevLaneSampleRates[lane] = appState.audio.laneSampleRates[lane];
        }
      }
      if (ratesChanged || mismatch != appState.audio.rateMismatch) {
        uint32_t sinkRate = appState.audio.sampleRate;  // Pipeline sink rate (48kHz nominal)
        for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
          uint32_t srcRate = appState.audio.laneSampleRates[lane];
          if (srcRate == 0 || srcRate == sinkRate || appState.audio.laneDsd[lane]) {
            // No ASRC needed: unknown rate, rate matches, or DSD lane
            audio_pipeline_set_lane_src(lane, sinkRate, sinkRate);  // passthrough
          } else {
            audio_pipeline_set_lane_src(lane, srcRate, sinkRate);
          }
        }
      }
    }
  }

  // DSD DAC mode switching — when a lane transitions to/from DoP DSD, switch all
  // DSD-capable Cirrus Logic DAC sinks into/out of DSD mode.
  // EVT_FORMAT_CHANGE is signalled by the pipeline when laneDsd[] changes.
  {
    static bool prevLaneDsd[AUDIO_PIPELINE_MAX_INPUTS] = {};
    bool anyChange = false;
    for (uint8_t lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
      if (prevLaneDsd[lane] != appState.audio.laneDsd[lane]) {
        anyChange = true;
        prevLaneDsd[lane] = appState.audio.laneDsd[lane];
      }
    }
    if (anyChange) {
      // Determine whether any lane is now in DSD mode
      bool anyDsd = false;
      for (uint8_t lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        if (appState.audio.laneDsd[lane]) { anyDsd = true; break; }
      }
      // Iterate all pipeline sinks; find DSD-capable Cirrus DACs and switch mode
      int sinkCount = audio_pipeline_get_sink_count();
      for (int s = 0; s < sinkCount; s++) {
        const AudioOutputSink* sink = audio_pipeline_get_sink(s);
        if (!sink || !sink->supportsDsd || sink->halSlot == 0xFF) continue;
        HalDevice* dev = HalDeviceManager::instance().getDevice(sink->halSlot);
        if (!dev) continue;
        if (!(dev->getDescriptor().capabilities & HAL_CAP_DSD)) continue;
        // Safe upcast: DSD-capable devices are always HalAudioDevice subclasses
        if (dev->getType() == HAL_DEV_DAC || dev->getType() == HAL_DEV_CODEC) {
          static_cast<HalAudioDevice*>(dev)->setDsdMode(anyDsd);
        }
      }
    }
  }

  // Rule 8: Sustained Clipping — clipRate >1% for >5 consecutive checks (25s)
  {
    static uint8_t clipHighCount[AUDIO_PIPELINE_MAX_INPUTS] = {};
    static bool clipEmitted[AUDIO_PIPELINE_MAX_INPUTS] = {};
    static unsigned long lastClipCheck = 0;
    if (millis() - lastClipCheck >= 5000) {
      lastClipCheck = millis();
      for (uint8_t lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        if (appState.audio.adc[lane].clipRate > 0.01f) {
          clipHighCount[lane]++;
          if (clipHighCount[lane] > 5 && !clipEmitted[lane]) {
            clipEmitted[lane] = true;
            char msg[24];
            snprintf(msg, sizeof(msg), "ADC%u clip %.1f%%", lane, appState.audio.adc[lane].clipRate * 100.0f);
            diag_emit(DIAG_AUDIO_CLIPPING, DIAG_SEV_WARN,
                      0xFF, "Audio", msg);
          }
        } else {
          clipHighCount[lane] = 0;
          clipEmitted[lane] = false;
        }
      }
    }
  }
#endif

  // Rule 7: Loop Time Spike — loopTimeMaxUs > 50ms
  {
    static bool loopSpikeEmitted = false;
    const TaskMonitorData& tmd = task_monitor_get_data();
    if (tmd.loopTimeMaxUs > 50000) {
      if (!loopSpikeEmitted) {
        loopSpikeEmitted = true;
        char msg[24];
        snprintf(msg, sizeof(msg), "loop %lums", (unsigned long)(tmd.loopTimeMaxUs / 1000));
        diag_emit(DIAG_SYS_LOOP_TIME_SPIKE, DIAG_SEV_WARN,
                  0xFF, "System", msg);
      }
    } else {
      loopSpikeEmitted = false;
    }
  }

  // Heap health monitor — graduated pressure detection (every 30s)
  static unsigned long lastHeapCheck = 0;
  if (millis() - lastHeapCheck >= 30000) {
    lastHeapCheck = millis();
    uint32_t maxBlock = ESP.getMaxAllocHeap();
    bool wasCritical = appState.debug.heapCritical;
    bool wasWarning  = appState.debug.heapWarning;

    appState.debug.heapCritical = (maxBlock < HEAP_CRITICAL_THRESHOLD);
    appState.debug.heapWarning  = !appState.debug.heapCritical && (maxBlock < HEAP_WARNING_THRESHOLD);

    if (appState.debug.heapCritical != wasCritical) {
      if (appState.debug.heapCritical) {
        LOG_W("[Main] HEAP CRITICAL: maxBlock=%lu freeHeap=%lu freePsram=%lu",
              (unsigned long)maxBlock, (unsigned long)ESP.getFreeHeap(),
              (unsigned long)ESP.getFreePsram());
        char msg[24];
        snprintf(msg, sizeof(msg), "maxBlock=%lu", (unsigned long)maxBlock);
        diag_emit(DIAG_SYS_HEAP_CRITICAL, DIAG_SEV_WARN,
                  0xFF, "System", msg);
      } else {
        LOG_I("[Main] Heap recovered: maxBlock=%lu freeHeap=%lu freePsram=%lu",
              (unsigned long)maxBlock, (unsigned long)ESP.getFreeHeap(),
              (unsigned long)ESP.getFreePsram());
        diag_emit(DIAG_SYS_HEAP_RECOVERED, DIAG_SEV_INFO,
                  0xFF, "System", "heap recovered");
      }
      appState.markHeapDirty();
    }
    if (appState.debug.heapWarning != wasWarning) {
      if (appState.debug.heapWarning) {
        LOG_W("[Main] HEAP WARNING: maxBlock=%lu freeHeap=%lu freePsram=%lu",
              (unsigned long)maxBlock, (unsigned long)ESP.getFreeHeap(),
              (unsigned long)ESP.getFreePsram());
        char msg[24];
        snprintf(msg, sizeof(msg), "maxBlock=%lu", (unsigned long)maxBlock);
        diag_emit(DIAG_SYS_HEAP_WARNING, DIAG_SEV_WARN,
                  0xFF, "System", msg);
      } else {
        LOG_I("[Main] Heap warning cleared: maxBlock=%lu", (unsigned long)maxBlock);
        diag_emit(DIAG_SYS_HEAP_WARNING_CLEARED, DIAG_SEV_INFO,
                  0xFF, "System", "warning cleared");
      }
      appState.markHeapDirty();
    }
  }

  // PSRAM pressure monitoring (every 30s, same cadence as heap check)
  static unsigned long lastPsramCheck = 0;
  if (millis() - lastPsramCheck >= 30000) {
    lastPsramCheck = millis();
#ifndef NATIVE_TEST
    {
      uint32_t psramFree = ESP.getFreePsram();
      bool wasPsramCrit = appState.debug.psramCritical;
      bool wasPsramWarn = appState.debug.psramWarning;

      appState.debug.psramCritical = (psramFree < PSRAM_CRITICAL_THRESHOLD);
      appState.debug.psramWarning  = !appState.debug.psramCritical &&
                                      (psramFree < PSRAM_WARNING_THRESHOLD);

      if (appState.debug.psramCritical && !wasPsramCrit) {
        LOG_W("[System] PSRAM critical: %lu bytes free", (unsigned long)psramFree);
        diag_emit(DIAG_SYS_PSRAM_WARNING, DIAG_SEV_ERROR,
                  0xFF, "System", "PSRAM critical");
        app_events_signal(EVT_HEAP_PRESSURE);
      } else if (!appState.debug.psramCritical && wasPsramCrit) {
        LOG_I("[System] PSRAM recovered from critical");
        diag_emit(DIAG_SYS_PSRAM_WARNING_CLEARED, DIAG_SEV_INFO,
                  0xFF, "System", "PSRAM recovered");
      }

      if (appState.debug.psramWarning && !wasPsramWarn) {
        LOG_W("[System] PSRAM warning: %lu bytes free", (unsigned long)psramFree);
        diag_emit(DIAG_SYS_PSRAM_WARNING, DIAG_SEV_WARN,
                  0xFF, "System", "PSRAM warning");
        app_events_signal(EVT_HEAP_PRESSURE);
      } else if (!appState.debug.psramWarning && wasPsramWarn && !appState.debug.psramCritical) {
        LOG_I("[System] PSRAM warning cleared");
        diag_emit(DIAG_SYS_PSRAM_WARNING_CLEARED, DIAG_SEV_INFO,
                  0xFF, "System", "PSRAM warning cleared");
      }
    }
#endif
  }
}
