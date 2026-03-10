#pragma once
#ifdef DAC_ENABLED

#include "hal_device.h"

class HalRelay : public HalDevice {
public:
    HalRelay(int pin);

    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

    // Enable or disable the relay (drives the GPIO output)
    void setEnabled(bool state);

private:
    int _pin;
    bool _enabled = false;
};

#endif // DAC_ENABLED
