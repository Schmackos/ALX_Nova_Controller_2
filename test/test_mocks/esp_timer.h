#ifndef ESP_TIMER_MOCK_H
#define ESP_TIMER_MOCK_H

#include <stdint.h>

// Mock esp_timer state — reuses ArduinoMock::mockMicros for the timer value
// Tests should set ArduinoMock::mockTimerUs directly
namespace ArduinoMock {
static uint64_t mockTimerUs = 0;
}

inline int64_t esp_timer_get_time() {
    return (int64_t)ArduinoMock::mockTimerUs;
}

#endif // ESP_TIMER_MOCK_H
