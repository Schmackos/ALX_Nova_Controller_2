#pragma once
// Structured diagnostic event — 64 bytes fixed, used by journal + WS + MQTT + serial.
// All fields are plain data types for binary serialization and cross-platform tests.

#include <stdint.h>
#include <string.h>
#include "diag_error_codes.h"
#include "hal/hal_types.h"  // hal_safe_strcpy

#ifndef UNIT_TEST
#include <Arduino.h>
#endif

struct DiagEvent {
    uint32_t  seq;          // Monotonic counter (NVS-backed, survives reboots)
    uint32_t  bootId;       // Boot session counter (NVS, incremented in crashlog_record)
    uint32_t  timestamp;    // millis()
    uint32_t  heapFree;     // ESP.getFreeHeap() at event time
    uint16_t  code;         // DiagErrorCode
    uint16_t  corrId;       // Correlation group ID (0 = standalone event)
    uint8_t   severity;     // DiagSeverity
    uint8_t   slot;         // HAL device slot (0xFF = N/A)
    uint8_t   retryCount;   // Retry attempts so far
    uint8_t   _pad;         // Alignment padding
    char      device[16];   // Device name e.g. "PCM5102A"
    char      message[24];  // Short description (error code carries meaning)
};

static_assert(sizeof(DiagEvent) == 64, "DiagEvent must be exactly 64 bytes");

// Slot value indicating "not associated with a HAL device"
#define DIAG_SLOT_NONE 0xFF

// Helper to populate a DiagEvent (does NOT write to journal — use diag_emit() for that)
static inline DiagEvent diag_event_create(
    uint16_t code, uint8_t severity, uint8_t slot,
    const char* deviceName, const char* msg)
{
    DiagEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.code     = code;
    ev.severity = severity;
    ev.slot     = slot;
    // Safe string copy with null termination
    if (deviceName) hal_safe_strcpy(ev.device, sizeof(ev.device), deviceName);
    if (msg)        hal_safe_strcpy(ev.message, sizeof(ev.message), msg);
    return ev;
}
