#pragma once
#ifdef DAC_ENABLED

#include "hal_device.h"

class HalDisplay : public HalDevice {
public:
    HalDisplay(int mosi, int sclk, int cs, int dc, int rst, int bl);

    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

private:
    int _mosi, _sclk, _cs, _dc, _rst, _bl;
};

#endif // DAC_ENABLED
