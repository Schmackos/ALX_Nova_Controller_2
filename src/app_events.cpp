#include "app_events.h"

#ifndef UNIT_TEST
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

static EventGroupHandle_t s_appEvents = nullptr;

void app_events_init() {
    s_appEvents = xEventGroupCreate();
    configASSERT(s_appEvents != nullptr);
}

void app_events_signal(EventBits_t bits) {
    if (s_appEvents) {
        xEventGroupSetBits(s_appEvents, bits);
    }
}

EventBits_t app_events_wait(uint32_t timeout_ms) {
    if (!s_appEvents) return 0;
    return xEventGroupWaitBits(
        s_appEvents,
        EVT_ANY,
        pdTRUE,                       // clear matched bits on exit
        pdFALSE,                      // wake on ANY bit (not all bits)
        pdMS_TO_TICKS(timeout_ms));
}
#endif
