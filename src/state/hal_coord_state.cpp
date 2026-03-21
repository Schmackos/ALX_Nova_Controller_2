// hal_coord_state.cpp — portMUX spinlock for HalCoordState toggle queue.
//
// Follows the diag_journal.cpp pattern: file-static portMUX_TYPE guarded
// by NATIVE_TEST preprocessor. Under native tests the critical section
// macros in the header expand to no-ops; this file is not compiled.
//
// The spinlock protects requestDeviceToggle() and clearPendingToggles()
// against concurrent access from different FreeRTOS tasks/cores.
// Today all callers run on Core 1 (loopTask), but the lock future-proofs
// against adding a Core 0 producer (e.g., mqtt_task processing broker
// commands) without requiring any further code changes.

#ifndef NATIVE_TEST

#include <Arduino.h>

static portMUX_TYPE _halCoordMux = portMUX_INITIALIZER_UNLOCKED;

void _hal_coord_enter_critical() {
  portENTER_CRITICAL(&_halCoordMux);
}

void _hal_coord_exit_critical() {
  portEXIT_CRITICAL(&_halCoordMux);
}

#endif // !NATIVE_TEST
