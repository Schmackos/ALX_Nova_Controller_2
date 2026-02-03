#ifndef ESP_RANDOM_MOCK_H
#define ESP_RANDOM_MOCK_H

#include <cstdint>
#include <cstring>

// Mock esp_random for session ID generation
namespace EspRandomMock {
    static uint32_t seedValue = 12345;
    static uint32_t currentValue = seedValue;

    void setSeed(uint32_t seed) {
        seedValue = seed;
        currentValue = seed;
    }

    uint32_t next() {
        // Simple linear congruential generator
        currentValue = (1664525 * currentValue + 1013904223) & 0xffffffff;
        return currentValue;
    }

    void reset() {
        setSeed(12345);
    }
}

// Mock esp_random function
inline uint32_t esp_random() {
    return EspRandomMock::next();
}

// Mock esp_fill_random for filling buffers
inline void esp_fill_random(void* buf, size_t len) {
    uint8_t* buffer = static_cast<uint8_t*>(buf);
    for (size_t i = 0; i < len; ++i) {
        buffer[i] = static_cast<uint8_t>(EspRandomMock::next() & 0xff);
    }
}

#endif // ESP_RANDOM_MOCK_H
