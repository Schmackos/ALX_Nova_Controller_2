#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#ifndef NATIVE_TEST
#include <Arduino.h>
#else
#include <stdint.h>
#include <string.h>
#endif

// Lightweight per-IP rate limiter for REST API endpoints
// Fixed-size array of 8 IP slots with sliding window (30 req/sec default)

#define RATE_LIMIT_SLOTS 8
#define RATE_LIMIT_WINDOW_MS 1000
#define RATE_LIMIT_MAX_REQUESTS 30

struct RateLimitSlot {
    uint32_t ip;
    uint16_t count;
    uint32_t windowStart;
};

// Returns true if request is allowed, false if rate limit exceeded
bool rate_limit_check(uint32_t ipAddr);

// Reset all rate limit state (for testing)
void rate_limit_reset();

#endif
