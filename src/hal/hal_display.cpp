#ifdef DAC_ENABLED

#include "hal_display.h"
#include "hal_device_manager.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#else
#define LOG_I(tag, ...) ((void)0)
#endif

HalDisplay::HalDisplay(int mosi, int sclk, int cs, int dc, int rst, int bl)
    : _mosi(mosi), _sclk(sclk), _cs(cs), _dc(dc), _rst(rst), _bl(bl)
{
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "sitronix,st7735s", 31);
    strncpy(_descriptor.name, "ST7735S TFT", 32);
    strncpy(_descriptor.manufacturer, "Sitronix", 32);
    _descriptor.type = HAL_DEV_DISPLAY;
    _descriptor.bus.type = HAL_BUS_SPI;
    _descriptor.bus.index = 0;
    _descriptor.bus.pinA = mosi;
    _descriptor.bus.pinB = sclk;
    _descriptor.channelCount = 1;
    _initPriority = HAL_PRIORITY_IO;
}

bool HalDisplay::probe()
{
    _state = HAL_STATE_DETECTED;
    LOG_I("[HAL:Display] probe OK — ST7735S TFT (MOSI=%d, SCLK=%d)", _mosi, _sclk);
    return true;
}

HalInitResult HalDisplay::init()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.claimPin(_mosi, HAL_BUS_SPI, 0, _slot);
    mgr.claimPin(_sclk, HAL_BUS_SPI, 0, _slot);
    mgr.claimPin(_cs,   HAL_BUS_SPI, 0, _slot);
    mgr.claimPin(_dc,   HAL_BUS_SPI, 0, _slot);
    mgr.claimPin(_rst,  HAL_BUS_SPI, 0, _slot);
    mgr.claimPin(_bl,   HAL_BUS_SPI, 0, _slot);
    _state = HAL_STATE_AVAILABLE;
    setReady(true);
    LOG_I("[HAL:Display] init — ST7735S TFT ready");
    return hal_init_ok();
}

void HalDisplay::deinit()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.releasePin(_mosi);
    mgr.releasePin(_sclk);
    mgr.releasePin(_cs);
    mgr.releasePin(_dc);
    mgr.releasePin(_rst);
    mgr.releasePin(_bl);
    setReady(false);
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL:Display] deinit — ST7735S TFT removed");
}

void HalDisplay::dumpConfig()
{
    LOG_I("[HAL:Display] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[HAL:Display]  manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[HAL:Display]  MOSI=%d SCLK=%d CS=%d DC=%d RST=%d BL=%d",
          _mosi, _sclk, _cs, _dc, _rst, _bl);
}

bool HalDisplay::healthCheck()
{
    return _ready;
}

#endif // DAC_ENABLED
