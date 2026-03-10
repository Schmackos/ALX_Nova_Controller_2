#ifndef STATE_HAL_COORD_STATE_H
#define STATE_HAL_COORD_STATE_H

#include <stdint.h>

// Generic device toggle request — device-independent, works with any HAL device
struct PendingDeviceToggle {
  uint8_t halSlot = 0xFF;  // 0xFF = none/invalid
  int8_t action = 0;       // 1=enable, -1=disable, 0=none
};

// Toggle queue capacity — board- and component-agnostic.
// Covers any device type (DAC, ADC, DSP, etc.) that may need deferred
// lifecycle management. With same-slot dedup, one entry per unique halSlot.
static constexpr uint8_t PENDING_TOGGLE_CAPACITY = 8;

// HAL coordination state — cross-cutting concerns for device lifecycle.
// Not tied to any specific component type (DAC, ADC, DSP), board, or platform.
// Devices are registered via EEPROM discovery, GPIO resistor ID, or manual config.
struct HalCoordState {
  // Deferred toggle queue — replaces single pendingToggle in DacState
  // that silently dropped concurrent requests. With same-slot dedup.
  volatile PendingDeviceToggle _pendingToggles[PENDING_TOGGLE_CAPACITY] = {};
  volatile uint8_t _pendingToggleCount = 0;

  // Enqueue a deferred device toggle for any component type (DAC, ADC, DSP, etc.).
  // Returns false on overflow or invalid args.
  // Deduplicates: same halSlot updates existing entry's action.
  bool requestDeviceToggle(uint8_t halSlot, int8_t action) {
    if (halSlot >= 0xFF || action < -1 || action > 1) return false;
    for (uint8_t i = 0; i < _pendingToggleCount; i++) {
      if (_pendingToggles[i].halSlot == halSlot) {
        _pendingToggles[i].action = action;
        return true;
      }
    }
    if (_pendingToggleCount >= PENDING_TOGGLE_CAPACITY) return false;
    _pendingToggles[_pendingToggleCount].halSlot = halSlot;
    _pendingToggles[_pendingToggleCount].action = action;
    _pendingToggleCount++;
    return true;
  }

  uint8_t pendingToggleCount() const { return _pendingToggleCount; }
  PendingDeviceToggle pendingToggleAt(uint8_t idx) const {
    PendingDeviceToggle result;
    if (idx < _pendingToggleCount) {
      result.halSlot = _pendingToggles[idx].halSlot;
      result.action  = _pendingToggles[idx].action;
    }
    return result;
  }
  bool hasPendingToggles() const { return _pendingToggleCount > 0; }
  void clearPendingToggles() {
    _pendingToggleCount = 0;
    for (uint8_t i = 0; i < PENDING_TOGGLE_CAPACITY; i++) {
      _pendingToggles[i].halSlot = 0xFF;
      _pendingToggles[i].action = 0;
    }
  }
};

#endif // STATE_HAL_COORD_STATE_H
