#ifndef TASK_MONITOR_H
#define TASK_MONITOR_H

#ifndef NATIVE_TEST
#include <Arduino.h>
#endif
#include <stdint.h>
#include <string.h>

#define MAX_MONITORED_TASKS 16

struct TaskInfo {
    char name[16];                // Task name (from FreeRTOS)
    uint32_t stackFreeBytes;      // Stack high water mark (bytes)
    uint32_t stackAllocBytes;     // Allocated stack size (bytes, 0 if unknown)
    uint8_t priority;             // Current priority
    uint8_t state;                // 0=Running, 1=Ready, 2=Blocked, 3=Suspended, 4=Deleted
    int8_t coreId;                // 0, 1, or -1 (no affinity)
};

struct TaskMonitorData {
    TaskInfo tasks[MAX_MONITORED_TASKS];
    uint8_t taskCount;            // Number of populated entries
    uint32_t loopTimeUs;          // Last main loop iteration (microseconds)
    uint32_t loopTimeMaxUs;       // Worst case since last report
    uint32_t loopTimeAvgUs;       // Average since last report
};

// Initialize (call once in setup())
void task_monitor_init();

// Snapshot all FreeRTOS tasks (call periodically, e.g., every 2s)
void task_monitor_update();

// Access latest snapshot
const TaskMonitorData& task_monitor_get_data();

// Main loop timing — call at top and bottom of loop()
void task_monitor_loop_start();
void task_monitor_loop_end();

// Serial dump (LOG_I level)
void task_monitor_print_serial();

// Pure helpers (testable without FreeRTOS)
uint32_t task_monitor_lookup_stack_alloc(const char* name);
const char* task_monitor_state_name(uint8_t state);
void task_monitor_sort_by_priority(TaskInfo* tasks, uint8_t count);

#endif // TASK_MONITOR_H
