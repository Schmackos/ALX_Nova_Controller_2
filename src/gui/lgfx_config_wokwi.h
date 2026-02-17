/**
 * LovyanGFX configuration for Wokwi Simulation
 * ILI9341 240x320 — used as simulation stand-in for ST7735S
 *
 * The LVGL display is still configured as 160x128 (via gui_config.h).
 * Content renders in the upper-left portion of the ILI9341 display.
 *
 * Pin mapping matches proven working Wokwi ESP32-S3 + ILI9341 example.
 */

#pragma once
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel;
    lgfx::Bus_SPI        _bus;

public:
    LGFX(void) {
        /* SPI bus configuration */
        {
            auto cfg = _bus.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 40000000;
            cfg.freq_read   = 16000000;
            cfg.pin_sclk    = 14;
            cfg.pin_mosi    = 13;
            cfg.pin_miso    = 19;
            cfg.pin_dc      = 2;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            _bus.config(cfg);
        }

        _panel.setBus(&_bus);

        /* Panel configuration (ILI9341) */
        {
            auto cfg = _panel.config();
            cfg.pin_cs       = 15;
            cfg.pin_rst      = 4;
            cfg.pin_busy     = -1;
            cfg.panel_width  = 240;
            cfg.panel_height = 320;
            _panel.config(cfg);
        }

        setPanel(&_panel);
    }
};
