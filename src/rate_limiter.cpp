#include "rate_limiter.h"

static RateLimitSlot _slots[RATE_LIMIT_SLOTS] = {};

void rate_limit_reset() {
    memset(_slots, 0, sizeof(_slots));
}

bool rate_limit_check(uint32_t ipAddr) {
#ifdef TEST_MODE
    return true;  // No rate limiting in test mode
#endif
    uint32_t now = millis();

    // Find existing slot for this IP
    for (int i = 0; i < RATE_LIMIT_SLOTS; i++) {
        if (_slots[i].ip == ipAddr && _slots[i].ip != 0) {
            // Check if window expired
            if ((now - _slots[i].windowStart) >= RATE_LIMIT_WINDOW_MS) {
                _slots[i].count = 1;
                _slots[i].windowStart = now;
                return true;
            }
            // Within window — check count
            _slots[i].count++;
            return _slots[i].count <= RATE_LIMIT_MAX_REQUESTS;
        }
    }

    // No existing slot — find empty or evict oldest
    int oldestIdx = 0;
    uint32_t oldestTime = UINT32_MAX;
    for (int i = 0; i < RATE_LIMIT_SLOTS; i++) {
        if (_slots[i].ip == 0) {
            oldestIdx = i;
            break;
        }
        if (_slots[i].windowStart < oldestTime) {
            oldestTime = _slots[i].windowStart;
            oldestIdx = i;
        }
    }

    _slots[oldestIdx].ip = ipAddr;
    _slots[oldestIdx].count = 1;
    _slots[oldestIdx].windowStart = now;
    return true;
}
