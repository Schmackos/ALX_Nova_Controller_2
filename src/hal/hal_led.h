#pragma once
#ifdef DAC_ENABLED

#include "hal_device.h"

class HalLed : public HalDevice {
public:
    HalLed(int pin);

    bool probe() override;
    bool init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

private:
    int _pin;
};

#endif // DAC_ENABLED
