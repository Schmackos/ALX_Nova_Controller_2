#pragma once
#ifdef DAC_ENABLED

#include "hal_device.h"

class HalButton : public HalDevice {
public:
    HalButton(int pin);

    bool probe() override;
    bool init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

private:
    int _pin;
};

#endif // DAC_ENABLED
