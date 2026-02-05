/**
 * TFT_eSPI User_Setup for Wokwi Simulation
 * ILI9341 240x320 — used as simulation stand-in for ST7735S
 *
 * The LVGL display is still configured as 160x128 (via gui_config.h).
 * Content renders in the upper-left portion of the ILI9341 display.
 *
 * Pin mapping matches proven working Wokwi ESP32-S3 + ILI9341 example:
 * https://wokwi.com/projects/392051088339718145
 */

#ifndef USER_SETUP_H
#define USER_SETUP_H

#define USER_SETUP_INFO "Wokwi_ILI9341"

/* ===== Driver selection ===== */
#define ILI9341_DRIVER

/* ===== SPI pins — matches working Wokwi ESP32-S3 example ===== */
#define TFT_MISO 19
#define TFT_MOSI 13
#define TFT_SCLK 14

#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4

#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

/* ===== SPI frequency ===== */
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  16000000
#define SPI_TOUCH_FREQUENCY  2500000

/* ===== Fonts ===== */
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#endif /* USER_SETUP_H */
