#ifndef USB_AUTO_PRIORITY_H
#define USB_AUTO_PRIORITY_H

#include "config.h"

#ifdef DSP_ENABLED

#include "dsp_crossover.h"

// ===== USB Auto-Priority State Machine =====
// When enabled, automatically routes USB audio to DAC when streaming starts,
// and reverts to previous routing when streaming stops.

enum UsbPriorityState : uint8_t {
    USB_PRIO_IDLE = 0,      // Feature disabled
    USB_PRIO_WATCHING,      // Enabled, waiting for streaming
    USB_PRIO_ACTIVE,        // USB routed to DAC
    USB_PRIO_REVERTING      // Streaming stopped, holdoff before reverting
};

// Debounce/holdoff timing (ms)
static const unsigned long USB_PRIO_ACTIVATE_DELAY_MS  = 50;   // Streaming must persist 50ms
static const unsigned long USB_PRIO_REVERT_HOLDOFF_MS  = 500;  // Wait 500ms after stream stops

// Pure function: compute next state and actions from current state + inputs.
// This is the testable core — no globals, no side effects.
struct UsbPriorityResult {
    UsbPriorityState nextState;
    bool saveMatrix;        // Save current routing matrix before override
    bool applyUsbRouting;   // Apply USB-to-DAC routing
    bool restoreMatrix;     // Restore previously saved routing matrix
};

UsbPriorityResult usb_auto_priority_step(
    UsbPriorityState currentState,
    bool featureEnabled,
    bool usbStreaming,
    unsigned long nowMs,
    unsigned long streamStartMs,
    unsigned long streamStopMs
);

// Build USB-to-DAC routing matrix: ch4/ch5 → output 0/1, rest silenced on output 0/1
void usb_auto_priority_build_routing(DspRoutingMatrix &rm);

// ===== Integration API (calls AppState + routing matrix) =====
void usb_auto_priority_init();
void usb_auto_priority_update(unsigned long nowMs);
bool usb_auto_priority_is_active();
UsbPriorityState usb_auto_priority_get_state();

#endif // DSP_ENABLED
#endif // USB_AUTO_PRIORITY_H
