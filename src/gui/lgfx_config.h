/**
 * LovyanGFX hardware configuration for ALX Nova Controller
 * ST7735S 128x160 TFT on ESP32-S3 / ESP32-P4 via Hardware SPI2 (FSPI)
 *
 * Replaces legacy TFT_eSPI setup — LovyanGFX provides reliable DMA,
 * native BGR color order, and active ESP32-S3 support.
 *
 * Pin numbers are read from config.h defines (TFT_MOSI_PIN, TFT_SCLK_PIN,
 * TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN) which carry S3 defaults and can be
 * overridden per-target via platformio.ini build_flags.
 */

#pragma once
#include "../config.h"
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
            cfg.freq_write  = 40000000;   /* 40 MHz — confirmed stable on ST7735S */
            cfg.freq_read   = 16000000;
            cfg.pin_sclk    = TFT_SCLK_PIN;
            cfg.pin_mosi    = TFT_MOSI_PIN;
            cfg.pin_miso    = -1;
            cfg.pin_dc      = TFT_DC_PIN;
            // P4 uses GDMA — LovyanGFX GDMA completion interrupt not yet verified.
            // waitDMA() spins forever on P4, triggering the 30s task watchdog.
            // Use CPU-driven (non-DMA) SPI transfers until P4 DMA is confirmed.
#if defined(CONFIG_IDF_TARGET_ESP32P4)
            cfg.dma_channel = 0;
#else
            cfg.dma_channel = SPI_DMA_CH_AUTO;
#endif
            _bus.config(cfg);
        }

        _panel.setBus(&_bus);

        /* Panel configuration (ST7735S BLACKTAB)
           memory_width/height = 128x160 (GM=001 mode, confirmed working).
           If image is shifted but not diagonal, switch to 132x162 with offset_x=2, offset_y=1. */
        {
            auto cfg = _panel.config();
            cfg.pin_cs        = TFT_CS_PIN;
            cfg.pin_rst       = TFT_RST_PIN;
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
