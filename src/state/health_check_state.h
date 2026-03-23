#ifndef HEALTH_CHECK_STATE_H
#define HEALTH_CHECK_STATE_H

#include <cstdint>

struct HealthCheckState {
    uint8_t  lastPassCount = 0;
    uint8_t  lastFailCount = 0;
    uint8_t  lastSkipCount = 0;
    uint32_t lastCheckDurationMs = 0;
    uint32_t lastCheckTimestamp = 0;  // millis() of last run
    bool     deferredComplete = false;   // true after deferred phase finishes
};

#endif // HEALTH_CHECK_STATE_H
