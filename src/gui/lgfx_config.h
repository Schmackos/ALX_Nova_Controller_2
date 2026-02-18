/**
 * LovyanGFX hardware configuration for ALX Nova Controller
 * ST7735S 128x160 TFT on ESP32-S3 via Hardware SPI2 (FSPI)
 *
 * Replaces TFT_eSPI User_Setup.h — LovyanGFX provides reliable DMA,
 * native BGR color order, and active ESP32-S3 support.
 */

#pragma once
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7735S _panel;
    lgfx::Bus_SPI       _bus;

public:
    LGFX(void) {
        /* SPI bus configuration (FSPI / SPI2_HOST) */
        {
            auto cfg = _bus.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 27000000;   /* 27 MHz — safe max for ST7735S on breadboard */
            cfg.freq_read   = 16000000;
            cfg.pin_sclk    = 12;
            cfg.pin_mosi    = 11;
            cfg.pin_miso    = -1;
            cfg.pin_dc      = 13;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            _bus.config(cfg);
        }

        _panel.setBus(&_bus);

        /* Panel configuration (ST7735S BLACKTAB)
           memory_width/height = 128x160 (GM=001 mode, confirmed working in
           LovyanGFX issue #206). If image is shifted but not diagonal,
           switch to 132x162 with offset_x=2, offset_y=1. */
        {
            auto cfg = _panel.config();
            cfg.pin_cs        = 10;
            cfg.pin_rst       = 14;
            cfg.pin_busy      = -1;
            cfg.panel_width   = 128;
            cfg.panel_height  = 160;
            cfg.memory_width  = 128;   /* GM=001: GRAM matches panel */
            cfg.memory_height = 160;
            cfg.offset_x      = 0;    /* No offset when memory = panel */
            cfg.offset_y      = 0;
            cfg.offset_rotation = 2;  /* 180° rotation — panel mounted inverted */
            cfg.dlen_16bit    = false; /* 8-bit SPI commands */
            cfg.bus_shared    = false; /* SPI bus not shared */
            cfg.rgb_order     = true;  /* true = RGB data from LVGL (BLACKTAB panel handles BGR via MADCTL) */
            cfg.invert        = false;
            _panel.config(cfg);
        }

        setPanel(&_panel);
    }
};
