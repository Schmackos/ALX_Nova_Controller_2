// Test USB Auto-Priority State Machine (Phase 5)
// Tests: pure state transitions, debounce, holdoff, matrix build, edge cases

#include <cmath>
#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline re-implementations for native testing =====
// (test_build_src = no means we cannot link src/ directly)

#ifndef DSP_MAX_CHANNELS
#define DSP_MAX_CHANNELS 6
#endif
#ifndef DSP_ENABLED
#define DSP_ENABLED
#endif

// Routing matrix struct (mirrors dsp_crossover.h)
struct DspRoutingMatrix {
    float matrix[DSP_MAX_CHANNELS][DSP_MAX_CHANNELS];
};

// State machine enum and constants (mirrors usb_auto_priority.h)
enum UsbPriorityState : uint8_t {
    USB_PRIO_IDLE = 0,
    USB_PRIO_WATCHING,
    USB_PRIO_ACTIVE,
    USB_PRIO_REVERTING
};

static const unsigned long USB_PRIO_ACTIVATE_DELAY_MS  = 50;
static const unsigned long USB_PRIO_REVERT_HOLDOFF_MS  = 500;

struct UsbPriorityResult {
    UsbPriorityState nextState;
    bool saveMatrix;
    bool applyUsbRouting;
    bool restoreMatrix;
};

// Pure step function (mirrors usb_auto_priority.cpp exactly)
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
        result.nextState = USB_PRIO_WATCHING;
        break;

    case USB_PRIO_WATCHING:
        if (usbStreaming) {
            if (streamStartMs > 0 && (nowMs - streamStartMs) >= USB_PRIO_ACTIVATE_DELAY_MS) {
                result.nextState = USB_PRIO_ACTIVE;
                result.saveMatrix = true;
                result.applyUsbRouting = true;
            }
        }
        break;

    case USB_PRIO_ACTIVE:
        if (!usbStreaming) {
            result.nextState = USB_PRIO_REVERTING;
        }
        break;

    case USB_PRIO_REVERTING:
        if (usbStreaming) {
            result.nextState = USB_PRIO_ACTIVE;
        } else if (streamStopMs > 0 && (nowMs - streamStopMs) >= USB_PRIO_REVERT_HOLDOFF_MS) {
            result.nextState = USB_PRIO_WATCHING;
            result.restoreMatrix = true;
        }
        break;
    }

    return result;
}

// Build routing function (mirrors usb_auto_priority.cpp)
void usb_auto_priority_build_routing(DspRoutingMatrix &rm) {
    memset(&rm, 0, sizeof(rm));
    rm.matrix[0][4] = 1.0f;
    rm.matrix[1][5] = 1.0f;
    for (int i = 2; i < DSP_MAX_CHANNELS; i++) {
        rm.matrix[i][i] = 1.0f;
    }
}

// ===== Tests =====

void setUp(void) {}
void tearDown(void) {}

// Test 1: IDLE → WATCHING when feature enabled
void test_idle_to_watching_on_enable(void) {
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_IDLE, true, false, 1000, 0, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_WATCHING, r.nextState);
    TEST_ASSERT_FALSE(r.saveMatrix);
    TEST_ASSERT_FALSE(r.applyUsbRouting);
    TEST_ASSERT_FALSE(r.restoreMatrix);
}

// Test 2: IDLE stays IDLE when feature disabled
void test_idle_stays_idle_when_disabled(void) {
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_IDLE, false, false, 1000, 0, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_IDLE, r.nextState);
    TEST_ASSERT_FALSE(r.restoreMatrix);
}

// Test 3: WATCHING stays WATCHING when not streaming
void test_watching_stays_when_no_stream(void) {
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_WATCHING, true, false, 1000, 0, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_WATCHING, r.nextState);
    TEST_ASSERT_FALSE(r.applyUsbRouting);
}

// Test 4: WATCHING stays WATCHING during debounce (streaming < 50ms)
void test_watching_debounce_too_early(void) {
    unsigned long streamStart = 1000;
    unsigned long now = 1030;  // Only 30ms elapsed (< 50ms)
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_WATCHING, true, true, now, streamStart, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_WATCHING, r.nextState);
    TEST_ASSERT_FALSE(r.applyUsbRouting);
}

// Test 5: WATCHING → ACTIVE after debounce (streaming >= 50ms)
void test_watching_to_active_after_debounce(void) {
    unsigned long streamStart = 1000;
    unsigned long now = 1050;  // Exactly 50ms elapsed
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_WATCHING, true, true, now, streamStart, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_ACTIVE, r.nextState);
    TEST_ASSERT_TRUE(r.saveMatrix);
    TEST_ASSERT_TRUE(r.applyUsbRouting);
    TEST_ASSERT_FALSE(r.restoreMatrix);
}

// Test 6: ACTIVE stays ACTIVE while streaming
void test_active_stays_while_streaming(void) {
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_ACTIVE, true, true, 2000, 1000, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_ACTIVE, r.nextState);
    TEST_ASSERT_FALSE(r.saveMatrix);
    TEST_ASSERT_FALSE(r.applyUsbRouting);
    TEST_ASSERT_FALSE(r.restoreMatrix);
}

// Test 7: ACTIVE → REVERTING when streaming stops
void test_active_to_reverting_on_stop(void) {
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_ACTIVE, true, false, 2000, 1000, 2000);
    TEST_ASSERT_EQUAL(USB_PRIO_REVERTING, r.nextState);
    TEST_ASSERT_FALSE(r.restoreMatrix);
}

// Test 8: REVERTING stays during holdoff (< 500ms since stop)
void test_reverting_holdoff_too_early(void) {
    unsigned long streamStop = 2000;
    unsigned long now = 2300;  // 300ms elapsed (< 500ms)
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_REVERTING, true, false, now, 1000, streamStop);
    TEST_ASSERT_EQUAL(USB_PRIO_REVERTING, r.nextState);
    TEST_ASSERT_FALSE(r.restoreMatrix);
}

// Test 9: REVERTING → WATCHING after holdoff (>= 500ms)
void test_reverting_to_watching_after_holdoff(void) {
    unsigned long streamStop = 2000;
    unsigned long now = 2500;  // Exactly 500ms elapsed
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_REVERTING, true, false, now, 1000, streamStop);
    TEST_ASSERT_EQUAL(USB_PRIO_WATCHING, r.nextState);
    TEST_ASSERT_TRUE(r.restoreMatrix);
    TEST_ASSERT_FALSE(r.applyUsbRouting);
}

// Test 10: REVERTING → ACTIVE when streaming resumes during holdoff
void test_reverting_to_active_on_resume(void) {
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_REVERTING, true, true, 2200, 2200, 2000);
    TEST_ASSERT_EQUAL(USB_PRIO_ACTIVE, r.nextState);
    TEST_ASSERT_FALSE(r.saveMatrix);  // Already saved from first activation
    TEST_ASSERT_FALSE(r.restoreMatrix);
}

// Test 11: Disable while ACTIVE → IDLE + restore
void test_disable_while_active_restores(void) {
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_ACTIVE, false, true, 3000, 1000, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_IDLE, r.nextState);
    TEST_ASSERT_TRUE(r.restoreMatrix);
    TEST_ASSERT_FALSE(r.applyUsbRouting);
}

// Test 12: Disable while REVERTING → IDLE + restore
void test_disable_while_reverting_restores(void) {
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_REVERTING, false, false, 3000, 1000, 2500);
    TEST_ASSERT_EQUAL(USB_PRIO_IDLE, r.nextState);
    TEST_ASSERT_TRUE(r.restoreMatrix);
}

// Test 13: Disable while WATCHING → IDLE (no restore needed)
void test_disable_while_watching_no_restore(void) {
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_WATCHING, false, false, 3000, 0, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_IDLE, r.nextState);
    TEST_ASSERT_FALSE(r.restoreMatrix);
}

// Test 14: Build USB routing matrix
void test_build_usb_routing_matrix(void) {
    DspRoutingMatrix rm;
    usb_auto_priority_build_routing(rm);

    // Output 0 = input 4 (USB L)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, rm.matrix[0][4]);
    // Output 1 = input 5 (USB R)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, rm.matrix[1][5]);
    // Output 0 should NOT get ADC inputs
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rm.matrix[0][0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rm.matrix[0][1]);
    // Remaining channels passthrough
    for (int i = 2; i < DSP_MAX_CHANNELS; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, rm.matrix[i][i]);
    }
}

// Test 15: Enum values stable
void test_enum_values(void) {
    TEST_ASSERT_EQUAL(0, USB_PRIO_IDLE);
    TEST_ASSERT_EQUAL(1, USB_PRIO_WATCHING);
    TEST_ASSERT_EQUAL(2, USB_PRIO_ACTIVE);
    TEST_ASSERT_EQUAL(3, USB_PRIO_REVERTING);
}

// Test 16: Full lifecycle — IDLE → WATCHING → ACTIVE → REVERTING → WATCHING
void test_full_lifecycle(void) {
    UsbPriorityState state = USB_PRIO_IDLE;
    unsigned long streamStart = 0;
    unsigned long streamStop = 0;

    // Step 1: Enable → WATCHING
    UsbPriorityResult r = usb_auto_priority_step(state, true, false, 100, 0, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_WATCHING, r.nextState);
    state = r.nextState;

    // Step 2: Streaming starts at t=200 (debounce check at t=210, not enough)
    streamStart = 200;
    r = usb_auto_priority_step(state, true, true, 210, streamStart, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_WATCHING, r.nextState);
    state = r.nextState;

    // Step 3: At t=250 (50ms elapsed) → ACTIVE
    r = usb_auto_priority_step(state, true, true, 250, streamStart, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_ACTIVE, r.nextState);
    TEST_ASSERT_TRUE(r.saveMatrix);
    TEST_ASSERT_TRUE(r.applyUsbRouting);
    state = r.nextState;

    // Step 4: Still streaming at t=1000 → stays ACTIVE
    r = usb_auto_priority_step(state, true, true, 1000, streamStart, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_ACTIVE, r.nextState);
    state = r.nextState;

    // Step 5: Streaming stops at t=1500 → REVERTING
    streamStop = 1500;
    r = usb_auto_priority_step(state, true, false, 1500, streamStart, streamStop);
    TEST_ASSERT_EQUAL(USB_PRIO_REVERTING, r.nextState);
    state = r.nextState;

    // Step 6: At t=1800 (300ms, still in holdoff) → stays REVERTING
    r = usb_auto_priority_step(state, true, false, 1800, streamStart, streamStop);
    TEST_ASSERT_EQUAL(USB_PRIO_REVERTING, r.nextState);
    TEST_ASSERT_FALSE(r.restoreMatrix);
    state = r.nextState;

    // Step 7: At t=2000 (500ms holdoff expired) → WATCHING + restore
    r = usb_auto_priority_step(state, true, false, 2000, streamStart, streamStop);
    TEST_ASSERT_EQUAL(USB_PRIO_WATCHING, r.nextState);
    TEST_ASSERT_TRUE(r.restoreMatrix);
}

// Test 17: Streaming resumes during revert holdoff — no bounce
void test_resume_during_holdoff(void) {
    UsbPriorityState state = USB_PRIO_REVERTING;
    unsigned long streamStop = 1000;

    // Resume at t=1200 (only 200ms into holdoff)
    UsbPriorityResult r = usb_auto_priority_step(
        state, true, true, 1200, 500, streamStop);
    TEST_ASSERT_EQUAL(USB_PRIO_ACTIVE, r.nextState);
    TEST_ASSERT_FALSE(r.saveMatrix);       // Don't re-save
    TEST_ASSERT_FALSE(r.restoreMatrix);    // Don't restore
    TEST_ASSERT_FALSE(r.applyUsbRouting);  // Don't re-apply (already active)
}

// Test 18: streamStartMs = 0 prevents premature activation
void test_zero_stream_start_no_activate(void) {
    UsbPriorityResult r = usb_auto_priority_step(
        USB_PRIO_WATCHING, true, true, 100, 0, 0);
    TEST_ASSERT_EQUAL(USB_PRIO_WATCHING, r.nextState);
    TEST_ASSERT_FALSE(r.applyUsbRouting);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_idle_to_watching_on_enable);
    RUN_TEST(test_idle_stays_idle_when_disabled);
    RUN_TEST(test_watching_stays_when_no_stream);
    RUN_TEST(test_watching_debounce_too_early);
    RUN_TEST(test_watching_to_active_after_debounce);
    RUN_TEST(test_active_stays_while_streaming);
    RUN_TEST(test_active_to_reverting_on_stop);
    RUN_TEST(test_reverting_holdoff_too_early);
    RUN_TEST(test_reverting_to_watching_after_holdoff);
    RUN_TEST(test_reverting_to_active_on_resume);
    RUN_TEST(test_disable_while_active_restores);
    RUN_TEST(test_disable_while_reverting_restores);
    RUN_TEST(test_disable_while_watching_no_restore);
    RUN_TEST(test_build_usb_routing_matrix);
    RUN_TEST(test_enum_values);
    RUN_TEST(test_full_lifecycle);
    RUN_TEST(test_resume_during_holdoff);
    RUN_TEST(test_zero_stream_start_no_activate);

    return UNITY_END();
}
