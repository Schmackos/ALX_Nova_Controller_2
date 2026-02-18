#include "task_monitor.h"

#ifndef NATIVE_TEST
#include "config.h"
#include "debug_serial.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/task_snapshot.h>
#include <esp_idf_version.h>
#else
// Stubs for native test — config constants provided by the test file
#ifndef TASK_STACK_SIZE_AUDIO
#define TASK_STACK_SIZE_AUDIO 10240
#endif
#ifndef TASK_STACK_SIZE_OTA
#define TASK_STACK_SIZE_OTA 16384
#endif
#ifdef GUI_ENABLED
#ifndef TASK_STACK_SIZE_GUI
#define TASK_STACK_SIZE_GUI 16384
#endif
#endif
#ifndef LOG_I
#define LOG_I(...)
#endif
#ifndef LOG_D
#define LOG_D(...)
#endif
#endif

#include <string.h>

// ===== Known task stack sizes (name -> allocated bytes) =====
struct KnownTask {
    const char* name;
    uint32_t stackBytes;
};

static const KnownTask knownTasks[] = {
    {"loopTask",  8192},
    {"audio_cap", TASK_STACK_SIZE_AUDIO},
#ifdef GUI_ENABLED
    {"gui_task",  TASK_STACK_SIZE_GUI},
#endif
    {"OTA_DL",    TASK_STACK_SIZE_OTA},
    {"OTA_CHK",   TASK_STACK_SIZE_OTA},
};
static const int KNOWN_TASK_COUNT = sizeof(knownTasks) / sizeof(knownTasks[0]);

// ===== Static data =====
static TaskMonitorData _tmData;
static uint32_t _loopStartUs = 0;
static uint32_t _loopAccumUs = 0;
static uint32_t _loopIterations = 0;

// ===== Pure helpers =====

uint32_t task_monitor_lookup_stack_alloc(const char* name) {
    for (int i = 0; i < KNOWN_TASK_COUNT; i++) {
        if (strcmp(name, knownTasks[i].name) == 0) {
            return knownTasks[i].stackBytes;
        }
    }
    return 0;
}

const char* task_monitor_state_name(uint8_t state) {
    switch (state) {
        case 0: return "R";   // Running
        case 1: return "r";   // Ready
        case 2: return "B";   // Blocked
        case 3: return "S";   // Suspended
        case 4: return "D";   // Deleted
        default: return "?";
    }
}

void task_monitor_sort_by_priority(TaskInfo* tasks, uint8_t count) {
    // Simple insertion sort — descending by priority
    for (int i = 1; i < count; i++) {
        TaskInfo key;
        memcpy(&key, &tasks[i], sizeof(TaskInfo));
        int j = i - 1;
        while (j >= 0 && tasks[j].priority < key.priority) {
            memcpy(&tasks[j + 1], &tasks[j], sizeof(TaskInfo));
            j--;
        }
        memcpy(&tasks[j + 1], &key, sizeof(TaskInfo));
    }
}

// ===== Init =====

void task_monitor_init() {
    memset(&_tmData, 0, sizeof(_tmData));
    _loopStartUs = 0;
    _loopAccumUs = 0;
    _loopIterations = 0;
}

// ===== Loop timing =====

void task_monitor_loop_start() {
    _loopStartUs = micros();
}

void task_monitor_loop_end() {
    if (_loopStartUs == 0) return;
    uint32_t elapsed = micros() - _loopStartUs;
    _tmData.loopTimeUs = elapsed;
    if (elapsed > _tmData.loopTimeMaxUs) {
        _tmData.loopTimeMaxUs = elapsed;
    }
    _loopAccumUs += elapsed;
    _loopIterations++;
}

// ===== FreeRTOS snapshot =====

#ifndef NATIVE_TEST
// IDF5 SMP affinity helper: vTaskCoreAffinityGet returns a bitmask.
// IDF4 xTaskGetAffinity returns a core index (0/1) or -1 (run on any).
static int8_t get_core_id(TaskHandle_t h) {
#if defined(CONFIG_FREERTOS_UNICORE)
    (void)h;
    return 0;
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    UBaseType_t mask = vTaskCoreAffinityGet(h);
    if (mask == (1U << 0)) return 0;
    if (mask == (1U << 1)) return 1;
    return -1;  // Pinned to any / both cores
#else
    return (int8_t)xTaskGetAffinity(h);
#endif
}

void task_monitor_update() {
    // Iterate all tasks using ESP-IDF pxTaskGetNext (uxTaskGetSystemState
    // is not exported from the pre-compiled FreeRTOS library in Arduino ESP32)
    int i = 0;
    TaskHandle_t handle = NULL;
    handle = pxTaskGetNext(handle);
    while (handle && i < MAX_MONITORED_TASKS) {
        TaskInfo& info = _tmData.tasks[i];
        const char *name = pcTaskGetName(handle);
        strncpy(info.name, name ? name : "?", sizeof(info.name) - 1);
        info.name[sizeof(info.name) - 1] = '\0';
        info.stackAllocBytes = task_monitor_lookup_stack_alloc(info.name);
        // Only scan stack watermark for known app tasks (expensive: walks entire stack)
        if (info.stackAllocBytes > 0) {
            info.stackFreeBytes = uxTaskGetStackHighWaterMark(handle) * 4;
        } else {
            info.stackFreeBytes = 0;
        }
        info.priority = (uint8_t)uxTaskPriorityGet(handle);
        info.state = (uint8_t)eTaskGetState(handle);
        info.coreId = get_core_id(handle);
        i++;
        handle = pxTaskGetNext(handle);
    }
    _tmData.taskCount = (uint8_t)i;

    // Sort by priority (descending)
    task_monitor_sort_by_priority(_tmData.tasks, _tmData.taskCount);

    // Compute average loop time, then reset accumulators
    if (_loopIterations > 0) {
        _tmData.loopTimeAvgUs = _loopAccumUs / _loopIterations;
    }
    _loopAccumUs = 0;
    _loopIterations = 0;
    _tmData.loopTimeMaxUs = 0; // Reset max for next interval
}
#else
void task_monitor_update() {
    // No-op for native tests
}
#endif

const TaskMonitorData& task_monitor_get_data() {
    return _tmData;
}

// ===== Serial output =====

void task_monitor_print_serial() {
    const TaskMonitorData& tm = _tmData;
    LOG_I("[TaskMon] Tasks: %d | Loop: %luus avg, %luus max",
          tm.taskCount,
          (unsigned long)tm.loopTimeAvgUs,
          (unsigned long)tm.loopTimeMaxUs);

    for (int i = 0; i < tm.taskCount; i++) {
        const TaskInfo& t = tm.tasks[i];
        if (t.stackAllocBytes > 0) {
            LOG_D("[TaskMon]  %-12s %5lu/%-5lu P%d %s C%d",
                  t.name,
                  (unsigned long)t.stackFreeBytes,
                  (unsigned long)t.stackAllocBytes,
                  t.priority,
                  task_monitor_state_name(t.state),
                  t.coreId);
        } else {
            LOG_D("[TaskMon]  %-12s %5lu       P%d %s C%d",
                  t.name,
                  (unsigned long)t.stackFreeBytes,
                  t.priority,
                  task_monitor_state_name(t.state),
                  t.coreId);
        }
    }
}
