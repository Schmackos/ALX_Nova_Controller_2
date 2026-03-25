#ifndef NATIVE_TEST

#include "health_check.h"
#include "config.h"
#include "debug_serial.h"
#include "app_state.h"
#include "diag_journal.h"
#include "diag_error_codes.h"
#include "task_monitor.h"
#include "i2s_audio.h"

#include "hal/hal_types.h"  // hal_safe_strcpy, HAL_BUS_I2C, HAL_STATE_* constants
#ifdef DAC_ENABLED
#include "hal/hal_device_manager.h"
#endif

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

// ===== Module-private state =====

// Static report storage (2.1KB BSS, not stack)
static HealthCheckReport _lastReport;
static bool              _hasReport        = false;
static uint32_t          _lastRunMs        = 0;
static volatile bool     _deferredPending  = false;
static TimerHandle_t     _deferredTimer    = nullptr;

// Serial command accumulator
static char     _serialBuf[32];
static uint8_t  _serialBufLen = 0;

static const char* const SERIAL_CMD = "HEALTH_CHECK";
static const uint32_t STALENESS_MS  = 5000;

// ===== Forward declarations =====
static void _timer_callback(TimerHandle_t xTimer);
static void _run_all(HealthCheckReport* report, bool deferredPhase);
static void _check_system(HealthCheckReport* report);
static void _check_i2c_buses(HealthCheckReport* report);
static void _check_hal_devices(HealthCheckReport* report);
static void _check_i2s_ports(HealthCheckReport* report);
static void _check_network(HealthCheckReport* report);
static void _check_mqtt(HealthCheckReport* report);
static void _check_tasks(HealthCheckReport* report);
static void _check_storage(HealthCheckReport* report);
static void _check_audio(HealthCheckReport* report);
static void _check_clock(HealthCheckReport* report);
static void _update_appstate(const HealthCheckReport* report);

// ===== Item helpers =====

static void _add_item(HealthCheckReport* r, const char* name,
                      HealthCheckResult result, const char* detail) {
    if (r->count >= HEALTH_CHECK_MAX_ITEMS) return;
    HealthCheckItem& item = r->items[r->count++];
    hal_safe_strcpy(item.name,   sizeof(item.name),   name);
    hal_safe_strcpy(item.detail, sizeof(item.detail), detail);
    item.result = result;
    switch (result) {
        case HC_PASS: r->passCount++; break;
        case HC_WARN: r->warnCount++; break;
        case HC_FAIL: r->failCount++; break;
        case HC_SKIP: r->skipCount++; break;
    }
}

static void _add_item_fmt(HealthCheckReport* r, const char* name,
                          HealthCheckResult result, const char* fmt, ...) {
    char buf[40];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    _add_item(r, name, result, buf);
}

// ===== Init =====

static void _timer_callback(TimerHandle_t /*xTimer*/) {
    _deferredPending = true;
}

void health_check_init() {
    memset(&_lastReport, 0, sizeof(_lastReport));
    memset(_serialBuf, 0, sizeof(_serialBuf));
    _hasReport       = false;
    _lastRunMs       = 0;
    _deferredPending = false;
    _serialBufLen    = 0;

    _deferredTimer = xTimerCreate(
        "hc_deferred",
        pdMS_TO_TICKS(HEALTH_CHECK_DEFERRED_DELAY),
        pdFALSE,           // one-shot
        nullptr,
        _timer_callback
    );
    if (_deferredTimer) {
        xTimerStart(_deferredTimer, 0);
    }
    LOG_I("[Health] Health check module initialised (deferred in %us)", HEALTH_CHECK_DEFERRED_DELAY / 1000);
}

// ===== Poll functions (called from main loop) =====

void health_check_poll_deferred() {
    if (!_deferredPending) return;
    _deferredPending = false;
    LOG_I("[Health] Deferred check triggered");
    health_check_run_full(&_lastReport);
}

void health_check_poll_serial() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            _serialBuf[_serialBufLen] = '\0';
            if (strcmp(_serialBuf, SERIAL_CMD) == 0) {
                LOG_I("[Health] Serial trigger received — running full check");
                health_check_run_full(&_lastReport);
            }
            _serialBufLen = 0;
        } else {
            if (_serialBufLen < (uint8_t)(sizeof(_serialBuf) - 1)) {
                _serialBuf[_serialBufLen++] = c;
            }
        }
    }
}

// ===== Public run functions =====

void health_check_run_full(HealthCheckReport* report) {
    uint32_t now = millis();
    if (_hasReport && (now - _lastRunMs) < STALENESS_MS) {
        if (report && report != &_lastReport) {
            memcpy(report, &_lastReport, sizeof(HealthCheckReport));
        }
        return;
    }
    _run_all(&_lastReport, true);
    _lastRunMs = millis();
    _hasReport = true;
    if (report && report != &_lastReport) {
        memcpy(report, &_lastReport, sizeof(HealthCheckReport));
    }
    _update_appstate(&_lastReport);
}

void health_check_run_immediate(HealthCheckReport* report) {
    HealthCheckReport tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.timestamp     = millis();
    tmp.deferredPhase = false;

    _check_system(&tmp);
    _check_storage(&tmp);

    tmp.durationMs = millis() - tmp.timestamp;

    LOG_I("[Health] Immediate summary: %u/%u PASS, %u FAIL (%ums)",
          tmp.passCount, tmp.count, tmp.failCount, tmp.durationMs);

    if (report) memcpy(report, &tmp, sizeof(HealthCheckReport));
}

const HealthCheckReport* health_check_get_last() {
    return _hasReport ? &_lastReport : nullptr;
}

#ifdef UNIT_TEST
void health_check_reset_for_test() {
    memset(&_lastReport, 0, sizeof(_lastReport));
    _hasReport       = false;
    _lastRunMs       = 0;
    _deferredPending = false;
    _serialBufLen    = 0;
    if (_deferredTimer) {
        xTimerStop(_deferredTimer, 0);
        xTimerDelete(_deferredTimer, 0);
        _deferredTimer = nullptr;
    }
}
#endif

// ===== Internal runner =====

static void _run_all(HealthCheckReport* report, bool deferredPhase) {
    memset(report, 0, sizeof(HealthCheckReport));
    report->timestamp     = millis();
    report->deferredPhase = deferredPhase;

    // Immediate checks — always run
    _check_system(report);
    _check_storage(report);

    if (deferredPhase) {
        _check_i2c_buses(report);
        _check_hal_devices(report);
        _check_i2s_ports(report);
        _check_network(report);
        _check_mqtt(report);
        _check_tasks(report);
        _check_audio(report);
        _check_clock(report);
    }

    report->durationMs = millis() - report->timestamp;

    LOG_I("[Health] Summary: %u/%u PASS, %u WARN, %u FAIL, %u SKIP (%ums)",
          report->passCount, report->count,
          report->warnCount, report->failCount,
          report->skipCount, report->durationMs);
}

// ===== AppState sync =====

static void _update_appstate(const HealthCheckReport* report) {
    appState.healthCheck.lastPassCount       = report->passCount;
    appState.healthCheck.lastFailCount       = report->failCount;
    appState.healthCheck.lastSkipCount       = report->skipCount;
    appState.healthCheck.lastCheckDurationMs = report->durationMs;
    appState.healthCheck.lastCheckTimestamp  = report->timestamp;
    appState.healthCheck.deferredComplete    = report->deferredPhase;
    appState.markHealthCheckDirty();
}

// ===== Check implementations =====

// 1. System — heap, PSRAM, DMA (immediate)
static void _check_system(HealthCheckReport* report) {
    // --- Heap ---
    size_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if (maxBlock >= HEAP_WARNING_THRESHOLD) {
        _add_item_fmt(report, "heap_free", HC_PASS, "%uKB largest free", (unsigned)(maxBlock / 1024));
        LOG_I("[Health] heap_free: PASS (%uKB)", (unsigned)(maxBlock / 1024));
    } else if (maxBlock >= HEAP_CRITICAL_THRESHOLD) {
        _add_item_fmt(report, "heap_free", HC_WARN, "%uKB < 50KB threshold", (unsigned)(maxBlock / 1024));
        LOG_W("[Health] heap_free: WARN (%uKB below warning threshold)", (unsigned)(maxBlock / 1024));
        diag_emit(DIAG_HEALTH_HEAP_LOW, DIAG_SEV_WARN, 0, "health", "Heap below warning threshold");
    } else {
        _add_item_fmt(report, "heap_free", HC_FAIL, "%uKB < 40KB critical", (unsigned)(maxBlock / 1024));
        LOG_W("[Health] heap_free: FAIL (%uKB below critical threshold)", (unsigned)(maxBlock / 1024));
        diag_emit(DIAG_HEALTH_HEAP_LOW, DIAG_SEV_ERROR, 0, "health", "Heap below critical threshold");
    }

    // --- PSRAM ---
    size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (freePsram == 0) {
        // PSRAM not present or not detected
        _add_item(report, "psram", HC_WARN, "PSRAM not detected");
        LOG_W("[Health] psram: WARN (PSRAM not detected)");
        diag_emit(DIAG_HEALTH_PSRAM_MISSING, DIAG_SEV_WARN, 0, "health", "PSRAM not detected");
    } else if (freePsram >= PSRAM_WARNING_THRESHOLD) {
        _add_item_fmt(report, "psram", HC_PASS, "%uKB free", (unsigned)(freePsram / 1024));
        LOG_I("[Health] psram: PASS (%uKB free)", (unsigned)(freePsram / 1024));
    } else if (freePsram >= PSRAM_CRITICAL_THRESHOLD) {
        _add_item_fmt(report, "psram", HC_WARN, "%uKB < 1MB threshold", (unsigned)(freePsram / 1024));
        LOG_W("[Health] psram: WARN (%uKB below warning threshold)", (unsigned)(freePsram / 1024));
        diag_emit(DIAG_HEALTH_PSRAM_LOW, DIAG_SEV_WARN, 0, "health", "PSRAM below warning threshold");
    } else {
        _add_item_fmt(report, "psram", HC_FAIL, "%uKB < 512KB critical", (unsigned)(freePsram / 1024));
        LOG_W("[Health] psram: FAIL (%uKB below critical threshold)", (unsigned)(freePsram / 1024));
        diag_emit(DIAG_HEALTH_PSRAM_LOW, DIAG_SEV_ERROR, 0, "health", "PSRAM below critical threshold");
    }

    // --- DMA allocation ---
    if (appState.audio.dmaAllocFailed) {
        _add_item_fmt(report, "dma_alloc", HC_FAIL, "mask=0x%04X", appState.audio.dmaAllocFailMask);
        LOG_W("[Health] dma_alloc: FAIL (mask=0x%04X)", appState.audio.dmaAllocFailMask);
        diag_emit(DIAG_HEALTH_DMA_FAIL, DIAG_SEV_ERROR, 0, "health", "DMA buffer alloc failed");
    } else {
        _add_item(report, "dma_alloc", HC_PASS, "all DMA buffers OK");
        LOG_I("[Health] dma_alloc: PASS");
    }
}

// 2. I2C Buses — inferred from HAL device states (deferred)
static void _check_i2c_buses(HealthCheckReport* report) {
#ifdef DAC_ENABLED
    struct BusCounts { uint8_t avail[3]; uint8_t error[3]; uint8_t total[3]; } bc;
    memset(&bc, 0, sizeof(bc));
    HalDeviceManager::instance().forEach([](HalDevice* dev, void* ctx) {
        BusCounts* bc = static_cast<BusCounts*>(ctx);
        if (dev->getDescriptor().bus.type != HAL_BUS_I2C) return;
        const HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(dev->getSlot());
        uint8_t bus = (cfg && cfg->valid) ? cfg->i2cBusIndex : dev->getDescriptor().bus.index;
        if (bus > 2) return;
        bc->total[bus]++;
        if (dev->_state == HAL_STATE_AVAILABLE) bc->avail[bus]++;
        else if (dev->_state == HAL_STATE_ERROR)  bc->error[bus]++;
    }, &bc);

    static const char* busNames[3] = {"i2c_bus0_ext", "i2c_bus1_onboard", "i2c_bus2_exp"};
    for (uint8_t b = 0; b < 3; b++) {
        if (bc.total[b] == 0) {
            _add_item_fmt(report, busNames[b], HC_SKIP, "no I2C devices on bus %u", b);
            LOG_I("[Health] %s: SKIP (no devices)", busNames[b]);
            continue;
        }
        if (bc.avail[b] > 0) {
            _add_item_fmt(report, busNames[b], HC_PASS,
                          "%u/%u devices AVAILABLE", bc.avail[b], bc.total[b]);
            LOG_I("[Health] %s: PASS (%u/%u AVAILABLE)", busNames[b], bc.avail[b], bc.total[b]);
        } else if (bc.error[b] == bc.total[b]) {
            _add_item_fmt(report, busNames[b], HC_FAIL,
                          "all %u devices ERROR", bc.total[b]);
            LOG_W("[Health] %s: FAIL (all %u devices ERROR)", busNames[b], bc.total[b]);
            diag_emit(DIAG_HEALTH_I2C_BUS_FAIL, DIAG_SEV_ERROR, b, busNames[b], "All I2C devices on bus in ERROR state");
        } else {
            _add_item_fmt(report, busNames[b], HC_WARN,
                          "%u devices ERROR (of %u)", bc.error[b], bc.total[b]);
            LOG_W("[Health] %s: WARN (%u/%u devices ERROR)", busNames[b], bc.error[b], bc.total[b]);
        }
    }
#else
    _add_item(report, "i2c_buses", HC_SKIP, "DAC_ENABLED not set");
#endif
}

// 3. HAL Devices (deferred)
static void _check_hal_devices(HealthCheckReport* report) {
#ifdef DAC_ENABLED
    struct HalCheckCtx {
        HealthCheckReport* report;
        uint8_t total;
        uint8_t available;
        uint8_t unavailable;
        uint8_t error;
        uint8_t failCount;
    } ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.report = report;

    HalDeviceManager::instance().forEach([](HalDevice* dev, void* vctx) {
        HalCheckCtx* ctx = static_cast<HalCheckCtx*>(vctx);
        ctx->total++;
        switch (dev->_state) {
            case HAL_STATE_AVAILABLE:
                ctx->available++;
                break;
            case HAL_STATE_UNAVAILABLE:
                ctx->unavailable++;
                if (ctx->failCount < 8) {
                    ctx->failCount++;
                    char itemName[24];
                    snprintf(itemName, sizeof(itemName), "hal_%u", dev->getSlot());
                    char detail[40];
                    const char* lastErr = dev->getLastError();
                    if (lastErr && lastErr[0]) {
                        snprintf(detail, sizeof(detail), "UNAVAIL: %.28s", lastErr);
                    } else {
                        snprintf(detail, sizeof(detail), "%s UNAVAILABLE", dev->getDescriptor().name);
                    }
                    // Truncate to fit HealthCheckItem sizes
                    detail[39] = '\0';
                    _add_item(ctx->report, itemName, HC_WARN, detail);
                    LOG_W("[Health] hal_%u: WARN (%s UNAVAILABLE)", dev->getSlot(), dev->getDescriptor().name);
                }
                break;
            case HAL_STATE_ERROR:
                ctx->error++;
                if (ctx->failCount < 8) {
                    ctx->failCount++;
                    char itemName[24];
                    snprintf(itemName, sizeof(itemName), "hal_%u", dev->getSlot());
                    char detail[40];
                    const char* lastErr = dev->getLastError();
                    if (lastErr && lastErr[0]) {
                        snprintf(detail, sizeof(detail), "ERROR: %.32s", lastErr);
                    } else {
                        snprintf(detail, sizeof(detail), "%s ERROR", dev->getDescriptor().name);
                    }
                    detail[39] = '\0';
                    _add_item(ctx->report, itemName, HC_FAIL, detail);
                    LOG_W("[Health] hal_%u: FAIL (%s in ERROR)", dev->getSlot(), dev->getDescriptor().name);
                    diag_emit(DIAG_HEALTH_HAL_DEVICE, DIAG_SEV_ERROR,
                              dev->getSlot(), dev->getDescriptor().name, "HAL device in ERROR state");
                }
                break;
            case HAL_STATE_CONFIGURING:
            case HAL_STATE_UNKNOWN:
                // Silently skip initialising devices
                break;
            default:
                break;
        }
    }, &ctx);

    // Summarise overflow
    uint8_t unhealthy = ctx.unavailable + ctx.error;
    if (unhealthy > 8) {
        char detail[40];
        snprintf(detail, sizeof(detail), "%u more not shown", (unsigned int)(unhealthy - 8));
        _add_item(report, "hal_overflow", HC_FAIL, detail);
    }

    if (ctx.error == 0 && ctx.unavailable == 0) {
        _add_item_fmt(report, "hal_summary", HC_PASS,
                      "%u/%u devices AVAILABLE", ctx.available, ctx.total);
        LOG_I("[Health] hal_summary: PASS (%u/%u AVAILABLE)", ctx.available, ctx.total);
    } else {
        LOG_W("[Health] hal_summary: %u AVAIL, %u UNAVAIL, %u ERROR of %u total",
              ctx.available, ctx.unavailable, ctx.error, ctx.total);
    }
#else
    _add_item(report, "hal_devices", HC_SKIP, "DAC_ENABLED not set");
#endif
}

// 4. I2S Ports (deferred)
static void _check_i2s_ports(HealthCheckReport* report) {
    bool anyActive = false;
    for (uint8_t p = 0; p < I2S_PORT_COUNT; p++) {
        I2sPortInfo info = i2s_port_get_info(p);
        if (!info.txActive && !info.rxActive) {
            // Port unused — SKIP
            char name[24];
            snprintf(name, sizeof(name), "i2s_port%u", p);
            _add_item(report, name, HC_SKIP, "port not configured");
            continue;
        }
        anyActive = true;
        char name[24];
        snprintf(name, sizeof(name), "i2s_port%u", p);
        char detail[40];
        snprintf(detail, sizeof(detail), "%s/%s mode=%u",
                 info.txActive ? "TX" : "--",
                 info.rxActive ? "RX" : "--",
                 info.txActive ? info.txMode : info.rxMode);
        _add_item(report, name, HC_PASS, detail);
        LOG_I("[Health] %s: PASS (%s)", name, detail);
    }
    if (!anyActive) {
        _add_item(report, "i2s_ports", HC_WARN, "no I2S ports active");
        LOG_W("[Health] i2s_ports: WARN (no ports active)");
    }
}

// 5. Network (deferred)
static void _check_network(HealthCheckReport* report) {
    if (appState.wifi.isAPMode) {
        _add_item(report, "network", HC_WARN, "AP mode (no upstream)");
        LOG_W("[Health] network: WARN (AP mode)");
        return;
    }
    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        char detail[40];
        snprintf(detail, sizeof(detail), "WiFi %d.%d.%d.%d",
                 ip[0], ip[1], ip[2], ip[3]);
        _add_item(report, "network", HC_PASS, detail);
        LOG_I("[Health] network: PASS (%s)", detail);
    } else {
        char detail[40];
        snprintf(detail, sizeof(detail), "disconnected (status=%d)", (int)status);
        _add_item(report, "network", HC_FAIL, detail);
        LOG_W("[Health] network: FAIL (%s)", detail);
        diag_emit(DIAG_HEALTH_NETWORK, DIAG_SEV_ERROR, 0, "health", "WiFi not connected");
    }
}

// 6. MQTT (deferred)
static void _check_mqtt(HealthCheckReport* report) {
    const MqttState& ms = appState.mqtt;
    bool configured = (ms.broker.length() > 0);
    if (!configured) {
        _add_item(report, "mqtt", HC_SKIP, "not configured");
        LOG_I("[Health] mqtt: SKIP (not configured)");
        return;
    }
    if (ms.connected) {
        char detail[40];
        snprintf(detail, sizeof(detail), "connected to %.20s", ms.broker.c_str());
        _add_item(report, "mqtt", HC_PASS, detail);
        LOG_I("[Health] mqtt: PASS (connected)");
    } else {
        char detail[40];
        snprintf(detail, sizeof(detail), "disconnected %.20s", ms.broker.c_str());
        _add_item(report, "mqtt", HC_WARN, detail);
        LOG_W("[Health] mqtt: WARN (disconnected, broker configured)");
        diag_emit(DIAG_HEALTH_MQTT, DIAG_SEV_WARN, 0, "health", "MQTT configured but not connected");
    }
}

// 7. FreeRTOS Tasks (deferred)
static void _check_tasks(HealthCheckReport* report) {
    const TaskMonitorData& tmd = task_monitor_get_data();
    if (tmd.taskCount == 0) {
        _add_item(report, "tasks", HC_SKIP, "task monitor not updated");
        LOG_I("[Health] tasks: SKIP (no snapshot)");
        return;
    }

    // Required task names
    static const char* REQUIRED_TASKS[] = {
        "loopTask",
        "audio_cap",
        "mqtt_task",
#ifdef GUI_ENABLED
        "gui_task",
#endif
        nullptr
    };

    bool anyFail = false;
    for (int ri = 0; REQUIRED_TASKS[ri] != nullptr; ri++) {
        const char* reqName = REQUIRED_TASKS[ri];
        bool found = false;
        for (uint8_t ti = 0; ti < tmd.taskCount; ti++) {
            if (strncmp(tmd.tasks[ti].name, reqName, 15) == 0) {
                found = true;
                // Check stack watermark
                if (tmd.tasks[ti].stackFreeBytes < 512) {
                    char detail[40];
                    snprintf(detail, sizeof(detail), "%s stack=%uB low",
                             reqName, (unsigned)tmd.tasks[ti].stackFreeBytes);
                    _add_item(report, "task_stack", HC_WARN, detail);
                    LOG_W("[Health] task_stack: WARN (%s stack=%uB)", reqName, (unsigned)tmd.tasks[ti].stackFreeBytes);
                    diag_emit(DIAG_HEALTH_TASK, DIAG_SEV_WARN, 0, reqName, "Task stack watermark < 512B");
                }
                break;
            }
        }
        if (!found) {
            char detail[40];
            snprintf(detail, sizeof(detail), "%s not found", reqName);
            _add_item(report, "task_missing", HC_FAIL, detail);
            LOG_W("[Health] task_missing: FAIL (%s not found)", reqName);
            diag_emit(DIAG_HEALTH_TASK, DIAG_SEV_ERROR, 0, reqName, "Required task not found");
            anyFail = true;
        }
    }

    if (!anyFail) {
        _add_item_fmt(report, "tasks", HC_PASS, "%u tasks running", tmd.taskCount);
        LOG_I("[Health] tasks: PASS (%u tasks)", tmd.taskCount);
    }
}

// 8. Storage (immediate)
static void _check_storage(HealthCheckReport* report) {
    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    if (total == 0) {
        _add_item(report, "storage", HC_FAIL, "LittleFS not mounted");
        LOG_W("[Health] storage: FAIL (LittleFS not mounted)");
        diag_emit(DIAG_HEALTH_STORAGE, DIAG_SEV_ERROR, 0, "health", "LittleFS not mounted");
        return;
    }

    bool hasConfig = LittleFS.exists("/config.json");
    char detail[40];
    snprintf(detail, sizeof(detail), "%uKB/%uKB used%s",
             (unsigned)(used / 1024), (unsigned)(total / 1024),
             hasConfig ? "" : " no config.json");

    if (!hasConfig) {
        _add_item(report, "storage", HC_WARN, detail);
        LOG_W("[Health] storage: WARN (no /config.json)");
    } else {
        _add_item(report, "storage", HC_PASS, detail);
        LOG_I("[Health] storage: PASS (%s)", detail);
    }
}

// 9. Audio Pipeline (deferred)
static void _check_audio(HealthCheckReport* report) {
    // paused flag — should be false during normal operation
    if (appState.audio.paused) {
        _add_item(report, "audio_pipeline", HC_WARN, "pipeline paused");
        LOG_W("[Health] audio_pipeline: WARN (paused flag set)");
        return;
    }

    // DMA already reported in _check_system; just reference it here
    if (appState.audio.dmaAllocFailed) {
        _add_item(report, "audio_pipeline", HC_FAIL, "DMA alloc failed");
        LOG_W("[Health] audio_pipeline: FAIL (DMA alloc failed)");
        diag_emit(DIAG_HEALTH_AUDIO, DIAG_SEV_ERROR, 0, "health", "DMA allocation failed");
        return;
    }

    // ADC health status — check first active ADC
    bool anyFault = false;
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        const AdcState& adc = appState.audio.adc[i];
        if (adc.totalBuffers == 0) continue;  // ADC not producing data — skip
        if (adc.healthStatus > 0) {           // AudioHealthStatus != AUDIO_OK
            char detail[40];
            snprintf(detail, sizeof(detail), "ADC%d health=%u", i, adc.healthStatus);
            _add_item(report, "audio_adc", HC_WARN, detail);
            LOG_W("[Health] audio_adc: WARN (ADC%d healthStatus=%u)", i, adc.healthStatus);
            anyFault = true;
        }
    }

    if (!anyFault) {
        _add_item_fmt(report, "audio_pipeline", HC_PASS,
                      "%d in, %d out active",
                      appState.audio.activeInputCount,
                      appState.audio.activeOutputCount);
        LOG_I("[Health] audio_pipeline: PASS (%d in, %d out)",
              appState.audio.activeInputCount,
              appState.audio.activeOutputCount);
    }
}

// 9. Clock quality — DPLL/APLL lock status for ESS SABRE DAC/ADC devices (deferred)
static void _check_clock(HealthCheckReport* report) {
#ifdef DAC_ENABLED
    struct ClockCheckCtx {
        HealthCheckReport* report;
        uint8_t total;
        uint8_t locked;
        uint8_t unlocked;
    } ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.report = report;

    HalDeviceManager::instance().forEach([](HalDevice* dev, void* vctx) {
        ClockCheckCtx* ctx = static_cast<ClockCheckCtx*>(vctx);
        uint16_t caps = dev->getDescriptor().capabilities;
        if (!(caps & (HAL_CAP_DPLL | HAL_CAP_APLL))) return;
        if (dev->_state != HAL_STATE_AVAILABLE) return;

        ctx->total++;
        ClockStatus cs = dev->getClockStatus();
        if (!cs.available) return;

        if (cs.locked) {
            ctx->locked++;
        } else {
            ctx->unlocked++;
            char itemName[24];
            snprintf(itemName, sizeof(itemName), "clock_%u", dev->getSlot());
            char detail[40];
            snprintf(detail, sizeof(detail), "%s: %s", dev->getDescriptor().name, cs.description);
            detail[39] = '\0';
            _add_item(ctx->report, itemName, HC_WARN, detail);
            LOG_W("[Health] %s: WARN (%s)", itemName, detail);
        }
    }, &ctx);

    if (ctx.total == 0) {
        _add_item(report, "clock_lock", HC_SKIP, "no DPLL/APLL devices");
        LOG_I("[Health] clock_lock: SKIP (no DPLL/APLL devices)");
    } else if (ctx.unlocked == 0) {
        _add_item_fmt(report, "clock_lock", HC_PASS, "%u/%u locked", ctx.locked, ctx.total);
        LOG_I("[Health] clock_lock: PASS (%u/%u devices locked)", ctx.locked, ctx.total);
    } else {
        _add_item_fmt(report, "clock_lock", HC_WARN, "%u/%u unlocked", ctx.unlocked, ctx.total);
        LOG_W("[Health] clock_lock: WARN (%u/%u devices unlocked)", ctx.unlocked, ctx.total);
    }
#else
    _add_item(report, "clock_lock", HC_SKIP, "DAC_ENABLED not set");
#endif
}

#endif // NATIVE_TEST
