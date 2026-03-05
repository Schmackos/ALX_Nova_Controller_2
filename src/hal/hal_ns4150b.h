#ifndef HAL_NS4150B_H
#define HAL_NS4150B_H

#ifdef DAC_ENABLED

#include "hal_device.h"

class HalNs4150b : public HalDevice {
public:
    HalNs4150b(int paPin);

    bool probe() override;
    bool init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

    void setEnable(bool enabled);
    bool isEnabled() const { return _enabled; }

private:
    int _paPin;
    bool _enabled;
};

#endif // DAC_ENABLED
#endif // HAL_NS4150B_H
