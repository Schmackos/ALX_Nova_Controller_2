#include "usb_auto_priority.h"

#ifdef DSP_ENABLED

#include "app_state.h"
#include "debug_serial.h"
#include "dsp_api.h"

#ifdef USB_AUDIO_ENABLED
#include "usb_audio.h"
#endif

// ===== Pure State Machine (testable without hardware) =====

UsbPriorityResult usb_auto_priority_step(
    UsbPriorityState currentState,
    bool featureEnabled,
    bool usbStreaming,
    unsigned long nowMs,
    unsigned long streamStartMs,
    unsigned long streamStopMs
) {
    UsbPriorityResult result = {};
    result.nextState = currentState;

    if (!featureEnabled) {
        // Feature disabled — if we were active, restore
        if (currentState == USB_PRIO_ACTIVE || currentState == USB_PRIO_REVERTING) {
            result.nextState = USB_PRIO_IDLE;
            result.restoreMatrix = true;
        } else {
            result.nextState = USB_PRIO_IDLE;
        }
        return result;
    }

    switch (currentState) {
    case USB_PRIO_IDLE:
        // Feature just enabled — start watching
        result.nextState = USB_PRIO_WATCHING;
        break;

    case USB_PRIO_WATCHING:
        if (usbStreaming) {
            // Check debounce: streaming must persist for ACTIVATE_DELAY
            if (streamStartMs > 0 && (nowMs - streamStartMs) >= USB_PRIO_ACTIVATE_DELAY_MS) {
                result.nextState = USB_PRIO_ACTIVE;
                result.saveMatrix = true;
                result.applyUsbRouting = true;
            }
        }
        break;

    case USB_PRIO_ACTIVE:
        if (!usbStreaming) {
            // Streaming stopped — begin holdoff
            result.nextState = USB_PRIO_REVERTING;
        }
        break;

    case USB_PRIO_REVERTING:
        if (usbStreaming) {
            // Streaming resumed during holdoff — go back to active
            result.nextState = USB_PRIO_ACTIVE;
        } else if (streamStopMs > 0 && (nowMs - streamStopMs) >= USB_PRIO_REVERT_HOLDOFF_MS) {
            // Holdoff expired — revert routing
            result.nextState = USB_PRIO_WATCHING;
            result.restoreMatrix = true;
        }
        break;
    }

    return result;
}

// Build USB-to-DAC routing: output ch0 = input ch4 (USB L), output ch1 = input ch5 (USB R)
// Other output channels retain identity passthrough for metering purposes
void usb_auto_priority_build_routing(DspRoutingMatrix &rm) {
    memset(&rm, 0, sizeof(rm));
    // Output 0 (DAC L) = input 4 (USB L)
    rm.matrix[0][4] = 1.0f;
    // Output 1 (DAC R) = input 5 (USB R)
    rm.matrix[1][5] = 1.0f;
    // Passthrough for remaining channels (metering still works)
    for (int i = 2; i < DSP_MAX_CHANNELS; i++) {
        rm.matrix[i][i] = 1.0f;
    }
}

// ===== Integration (static state, calls AppState) =====

static UsbPriorityState _state = USB_PRIO_IDLE;
static DspRoutingMatrix _savedMatrix;
static bool _hasSavedMatrix = false;
static unsigned long _streamStartMs = 0;
static unsigned long _streamStopMs = 0;
static bool _prevStreaming = false;

void usb_auto_priority_init() {
    _state = USB_PRIO_IDLE;
    _hasSavedMatrix = false;
    _streamStartMs = 0;
    _streamStopMs = 0;
    _prevStreaming = false;
}

void usb_auto_priority_update(unsigned long nowMs) {
    AppState &app = AppState::getInstance();

    // Detect streaming state
    bool streaming = false;
#ifdef USB_AUDIO_ENABLED
    streaming = usb_audio_is_streaming();
#endif

    // Track streaming start/stop edges
    if (streaming && !_prevStreaming) {
        _streamStartMs = nowMs;
    } else if (!streaming && _prevStreaming) {
        _streamStopMs = nowMs;
    }
    _prevStreaming = streaming;

    // Run state machine
    UsbPriorityResult result = usb_auto_priority_step(
        _state, app.usbAutoPriority, streaming,
        nowMs, _streamStartMs, _streamStopMs
    );

    // Apply actions
    if (result.saveMatrix && !_hasSavedMatrix) {
        DspRoutingMatrix *rm = dsp_get_routing_matrix();
        if (rm) {
            memcpy(&_savedMatrix, rm, sizeof(DspRoutingMatrix));
            _hasSavedMatrix = true;
            LOG_I("[USB Prio] Saved routing matrix");
        }
    }

    if (result.applyUsbRouting) {
        DspRoutingMatrix *rm = dsp_get_routing_matrix();
        if (rm) {
            usb_auto_priority_build_routing(*rm);
            app.markDspConfigDirty();
            LOG_I("[USB Prio] Applied USB-to-DAC routing");
        }
    }

    if (result.restoreMatrix && _hasSavedMatrix) {
        DspRoutingMatrix *rm = dsp_get_routing_matrix();
        if (rm) {
            memcpy(rm, &_savedMatrix, sizeof(DspRoutingMatrix));
            _hasSavedMatrix = false;
            app.markDspConfigDirty();
            LOG_I("[USB Prio] Restored previous routing matrix");
        }
    }

    _state = result.nextState;
}

bool usb_auto_priority_is_active() {
    return _state == USB_PRIO_ACTIVE;
}

UsbPriorityState usb_auto_priority_get_state() {
    return _state;
}

#endif // DSP_ENABLED
