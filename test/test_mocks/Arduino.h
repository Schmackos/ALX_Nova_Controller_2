#ifndef ARDUINO_MOCK_H
#define ARDUINO_MOCK_H

#include <stdint.h>
#include <string>
#include <cstring>

// Mock Arduino types
typedef std::string String;
typedef bool boolean;
typedef uint8_t byte;

// Pin modes
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2

// Pin states
#define LOW 0x0
#define HIGH 0x1

// Mock global state for testing (header-only implementation)
namespace ArduinoMock {
    static unsigned long mockMillis = 0;
    static int mockAnalogValue = 0;
    static int mockDigitalPins[50] = {0};

    // Reset function for tests
    inline void reset() {
        mockMillis = 0;
        mockAnalogValue = 0;
        std::memset(mockDigitalPins, 0, sizeof(mockDigitalPins));
    }
}

// Mock Arduino functions
inline unsigned long millis() { return ArduinoMock::mockMillis; }
inline void delay(unsigned long ms) { ArduinoMock::mockMillis += ms; }
inline int analogRead(uint8_t pin) { return ArduinoMock::mockAnalogValue; }
inline void digitalWrite(uint8_t pin, uint8_t val) {
    if (pin < 50) ArduinoMock::mockDigitalPins[pin] = val;
}
inline int digitalRead(uint8_t pin) {
    return (pin < 50) ? ArduinoMock::mockDigitalPins[pin] : LOW;
}
inline void pinMode(uint8_t pin, uint8_t mode) { /* Mock - do nothing */ }

#endif // ARDUINO_MOCK_H
