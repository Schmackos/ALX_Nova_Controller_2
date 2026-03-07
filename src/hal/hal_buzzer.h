#pragma once
#ifdef DAC_ENABLED

#include "hal_device.h"

class HalBuzzer : public HalDevice {
public:
    HalBuzzer(int pin);

    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

private:
    int _pin;
};

#endif // DAC_ENABLED
