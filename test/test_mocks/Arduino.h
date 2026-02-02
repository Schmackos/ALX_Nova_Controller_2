#ifndef ARDUINO_MOCK_H
#define ARDUINO_MOCK_H

#include <stdint.h>
#include <string>

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

// Mock global state for testing
namespace ArduinoMock {
    extern unsigned long mockMillis;
    extern int mockAnalogValue;
    extern int mockDigitalPins[50];

    // Reset function for tests
    void reset();
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
