#pragma once
#ifdef DAC_ENABLED

#include "hal_device.h"

class HalLed : public HalDevice {
public:
    HalLed(int pin);

    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

    void setOn(bool state);

private:
    int _pin;
};

#endif // DAC_ENABLED
