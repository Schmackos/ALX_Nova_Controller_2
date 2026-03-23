#pragma once
// health_check.h — Firmware health check module
//
// Runs a structured set of pass/fail checks against defined thresholds and
// reports results via HealthCheckReport. Two execution phases:
//   Immediate: heap, storage, DMA — safe to run before WiFi/HAL are ready.
//   Deferred:  I2C buses, HAL devices, I2S ports, network, MQTT, tasks, audio
//              — runs after a 30s boot delay or when readiness indicators fire.
//
// REST endpoint: GET /api/health  (see health_check_api.h)
// Serial trigger: send "HEALTH_CHECK\n" over serial monitor.

#include <stdint.h>
#include <stdbool.h>

// ===== Check result codes =====
enum HealthCheckResult : uint8_t {
    HC_PASS = 0,   // Check passed — within threshold
    HC_WARN = 1,   // Check passed with caveat — degraded but functional
    HC_FAIL = 2,   // Check failed — outside threshold
    HC_SKIP = 3,   // Check skipped — prerequisite not met or not applicable
};

// ===== Per-check detail record (fixed-size, no heap) =====
struct HealthCheckItem {
    char              name[24];    // Short identifier, e.g. "heap_free"
    HealthCheckResult result;
    char              detail[40];  // Human-readable verdict/value, e.g. "82KB free"
};

#define HEALTH_CHECK_MAX_ITEMS 32

// ===== Full run result =====
struct HealthCheckReport {
    HealthCheckItem items[HEALTH_CHECK_MAX_ITEMS];
    uint8_t  count;
    uint8_t  passCount;
    uint8_t  warnCount;
    uint8_t  failCount;
    uint8_t  skipCount;
    uint32_t durationMs;   // Wall time for the full run
    uint32_t timestamp;    // millis() at run start
    bool     deferredPhase; // true when deferred checks were included
};

// ===== Public API =====

// Call once from setup() — creates the deferred FreeRTOS timer.
void health_check_init();

// Call from the main loop: drains the deferred-pending flag set by the timer
// and runs health_check_run_full() if enough time has elapsed.
void health_check_poll_deferred();

// Call from the main loop: drains one byte of serial, accumulates into a
// 32-byte buffer, and triggers a full run when "HEALTH_CHECK\n" is received.
void health_check_poll_serial();

// Run all checks (immediate + deferred phases) and write results into *report.
// Uses a 5-second staleness guard — returns cached result if called within 5s.
// Safe to call from the main loop; never from ISR or audio task.
void health_check_run_full(HealthCheckReport* report);

// Run only the immediate checks (heap, storage, DMA).
// No staleness guard — always executes. Used for early-boot validation.
void health_check_run_immediate(HealthCheckReport* report);

// Return a pointer to the most recent completed report (static storage).
// Returns nullptr if no run has completed yet.
const HealthCheckReport* health_check_get_last();

#ifdef UNIT_TEST
void health_check_reset_for_test();
#endif
