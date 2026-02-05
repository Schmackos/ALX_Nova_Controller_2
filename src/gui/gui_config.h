#ifndef GUI_CONFIG_H
#define GUI_CONFIG_H

/*====================
   TFT DISPLAY PINS
   ST7735S 128x160 via Hardware SPI2 (FSPI)
 *====================*/

#ifndef TFT_MOSI_PIN
#define TFT_MOSI_PIN 11
#endif

#ifndef TFT_SCLK_PIN
#define TFT_SCLK_PIN 12
#endif

#ifndef TFT_CS_PIN
#define TFT_CS_PIN 10
#endif

#ifndef TFT_DC_PIN
#define TFT_DC_PIN 13
#endif

#ifndef TFT_RST_PIN
#define TFT_RST_PIN 14
#endif

#ifndef TFT_BL_PIN
#define TFT_BL_PIN 21
#endif

/*====================
   EC11 ROTARY ENCODER PINS
 *====================*/

#ifndef ENCODER_A_PIN
#define ENCODER_A_PIN 6
#endif

#ifndef ENCODER_B_PIN
#define ENCODER_B_PIN 5
#endif

#ifndef ENCODER_SW_PIN
#define ENCODER_SW_PIN 7
#endif

/*====================
   DISPLAY CONFIGURATION
 *====================*/

/* Logical display dimensions after rotation (landscape) */
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 160

/* Display buffer: 1/10th of screen (single buffer) */
#define DISP_BUF_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT / 10)

/* Backlight PWM settings */
#define BL_PWM_CHANNEL 0
#define BL_PWM_FREQ 5000
#define BL_PWM_RESOLUTION 8
#define BL_BRIGHTNESS_MAX 255
#define BL_BRIGHTNESS_MIN 0
#define BL_BRIGHTNESS_DIM 40

/*====================
   TIMING CONFIGURATION
 *====================*/

/* Screen timeout in ms (default 60 seconds) */
#define DEFAULT_SCREEN_TIMEOUT 60000

/* LVGL timer handler call interval in ms */
#define GUI_TICK_PERIOD_MS 5

/* Encoder debounce time in ms */
#define ENCODER_DEBOUNCE_MS 2

/* Input polling period for LVGL indev driver */
#define INDEV_READ_PERIOD_MS 10

/*====================
   FreeRTOS GUI TASK
 *====================*/

#define GUI_TASK_STACK_SIZE 8192
#define GUI_TASK_PRIORITY 1
#define GUI_TASK_CORE 1

#endif /* GUI_CONFIG_H */
