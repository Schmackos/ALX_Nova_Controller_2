#include "Arduino.h"

namespace ArduinoMock {
    unsigned long mockMillis = 0;
    int mockAnalogValue = 0;
    int mockDigitalPins[50] = {0};

    void reset() {
        mockMillis = 0;
        mockAnalogValue = 0;
        for (int i = 0; i < 50; i++) {
            mockDigitalPins[i] = 0;
        }
    }
}
