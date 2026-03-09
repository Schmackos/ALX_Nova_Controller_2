#pragma once
#ifdef DAC_ENABLED

#include "hal_device.h"

class HalEncoder : public HalDevice {
public:
    HalEncoder(int pinA, int pinB, int pinSw);

    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

    int getPinA() const { return _pinA; }
    int getPinB() const { return _pinB; }
    int getPinSw() const { return _pinSw; }

private:
    int _pinA, _pinB, _pinSw;
};

#endif // DAC_ENABLED
