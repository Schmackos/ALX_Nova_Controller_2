/**
 * TFT_eSPI User_Setup.h for ALX Nova Controller
 * ST7735S 128x160 TFT on ESP32-S3 via Hardware SPI2 (FSPI)
 *
 * This file is referenced by TFT_eSPI via the USER_SETUP_LOADED define
 * and build flags in platformio.ini.
 */

#ifndef USER_SETUP_H
#define USER_SETUP_H

/* ===== Driver selection ===== */
#define ST7735_DRIVER
#define TFT_WIDTH  128
#define TFT_HEIGHT 160

/* ST7735 variant: BLACKTAB for most 1.8" blue-PCB modules */
#define ST7735_BLACKTAB

/* ===== SPI pins (ESP32-S3 FSPI) ===== */
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC   13
#define TFT_RST  14

/* Backlight control is handled via PWM in gui_manager.cpp */

/* ===== SPI frequency ===== */
#define SPI_FREQUENCY       40000000  /* 40 MHz — ST7735S supports up to ~62 MHz */
#define SPI_READ_FREQUENCY  16000000
#define SPI_TOUCH_FREQUENCY  2500000  /* Not used (no touch) */

/* ===== Color settings ===== */
/* BGR color order for ST7735S BLACKTAB panels */
#define TFT_RGB_ORDER TFT_BGR

/* Enable 16-bit parallel reads if supported by display */
// #define TFT_SDA_READ  /* Not needed for SPI */

/* ===== Misc ===== */
/* Load common Fonts 1 and 2 */
#define LOAD_GLCD   /* Font 1: Original Adafruit 8 pixel */
#define LOAD_FONT2  /* Font 2: Small 16 pixel high */
// #define LOAD_FONT4  /* Not needed — LVGL provides fonts */
// #define LOAD_FONT6
// #define LOAD_FONT7
// #define LOAD_FONT8
#define LOAD_GFXFF  /* FreeFont support */

/* Enable smooth font support */
#define SMOOTH_FONT

/* Use FSPI port (SPI2) on ESP32-S3 */
#define USE_FSPI_PORT

#endif /* USER_SETUP_H */
