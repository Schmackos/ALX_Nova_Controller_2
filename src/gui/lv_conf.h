/**
 * @file lv_conf.h
 * LVGL v9 configuration for ALX Nova Controller
 * 128x160 ST7735S TFT display, 16-bit color
 * SquareLine Studio v1.4.1+ compatible
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#ifndef __ASSEMBLY__
#include <stdint.h>
#endif

/*====================
   COLOR SETTINGS
 *====================*/

/* Color depth: 16-bit (RGB565) for ST7735S */
#define LV_COLOR_DEPTH 16

/*====================
   MEMORY SETTINGS
 *====================*/

/* Size of the memory available for `lv_malloc()` in bytes (>= 2kB) */
#define LV_MEM_SIZE (32 * 1024U) /* 32KB */

/* Set an address for the memory pool instead of allocating it as a normal array.
 * Can be in external SRAM too. */
#define LV_MEM_ADR 0 /* 0: unused */

/* Use the standard `malloc` and `free` instead of LVGL's built-in allocator */
#define LV_MEM_CUSTOM 0

/*====================
   HAL SETTINGS
 *====================*/

/* Default display refresh, input read, and animation step period in ms */
#define LV_DEF_REFR_PERIOD 33 /* ~30 FPS */

/* Default DPI (dots per inch). Used to initialize default sizes */
#define LV_DPI_DEF 130

/* Enable the tick interface using a custom function */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

/*====================
   FEATURE CONFIG
 *====================*/

/*-------------
 * Drawing
 *-----------*/

/* Enable complex draw engine (transformations, masks, etc.) */
#define LV_DRAW_COMPLEX 1

/* Disable ARM NEON/Helium ASM — ESP32-S3 is Xtensa, not ARM */
#define LV_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE

/* Allow buffering some shadow calculation (more memory but faster) */
#define LV_SHADOW_CACHE_SIZE 0

/* Stride alignment for images - set to 1 for minimal memory */
#define LV_DRAW_BUF_STRIDE_ALIGN 1

/* Align the start address of draw_buf to this value */
#define LV_DRAW_BUF_ALIGN 4

/*-------------
 * Logging
 *-----------*/

/* Enable the log module */
#define LV_USE_LOG 1
#if LV_USE_LOG
    /* How important log should be. Choices:
     * LV_LOG_LEVEL_TRACE, LV_LOG_LEVEL_INFO,
     * LV_LOG_LEVEL_WARN, LV_LOG_LEVEL_ERROR,
     * LV_LOG_LEVEL_USER, LV_LOG_LEVEL_NONE */
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

    /* 1: Print the log with 'printf';
     * 0: User needs to register a callback with `lv_log_register_print_cb()` */
    #define LV_LOG_PRINTF 1

    /* Set the number of log traces. 0: off */
    #define LV_LOG_TRACE_MEM 0
    #define LV_LOG_TRACE_TIMER 0
    #define LV_LOG_TRACE_INDEV 0
    #define LV_LOG_TRACE_DISP_REFR 0
    #define LV_LOG_TRACE_EVENT 0
    #define LV_LOG_TRACE_OBJ_CREATE 0
    #define LV_LOG_TRACE_LAYOUT 0
    #define LV_LOG_TRACE_ANIM 0
#endif /* LV_USE_LOG */

/*-------------
 * Asserts
 *-----------*/

#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

/*-------------
 * Others
 *-----------*/

/* 1: Show CPU usage and FPS count */
#define LV_USE_PERF_MONITOR 0

/* 1: Show memory usage */
#define LV_USE_MEM_MONITOR 0

/* 1: Enable the built-in profiler for LVGL */
#define LV_USE_PROFILER 0

/*====================
   FONT USAGE
 *====================*/

/* Montserrat fonts with ASCII range (SquareLine Studio compatible) */
#define LV_FONT_MONTSERRAT_8 0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* Special fonts */
#define LV_FONT_MONTSERRAT_12_SUBPX 0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK 0
#define LV_FONT_UNSCII_8 0
#define LV_FONT_UNSCII_16 0

/* Default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Enable subpixel rendering */
#define LV_FONT_SUBPX_BGR 0

/* Enable FreeType font engine (not needed, using built-in fonts) */
#define LV_USE_FREETYPE 0

/* Enable Tiny TTF font engine */
#define LV_USE_TINY_TTF 0

/*====================
   TEXT SETTINGS
 *====================*/

/* Select a character encoding for strings.
 * LV_TXT_ENC_UTF8 or LV_TXT_ENC_ASCII */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/*====================
   WIDGET USAGE
 *====================*/

/* Core widgets (always enabled in LVGL v9) */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BUTTON 1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_CANVAS 0
#define LV_USE_CHECKBOX 1
#define LV_USE_DROPDOWN 1
#define LV_USE_IMAGE 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TABLE 0
#define LV_USE_TEXTAREA 1

/* Extra widgets */
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_KEYBOARD 1
#define LV_USE_LED 1
#define LV_USE_LIST 1
#define LV_USE_MENU 1
#define LV_USE_METER 0
#define LV_USE_MSGBOX 1
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 1
#define LV_USE_SPINNER 1
#define LV_USE_TABVIEW 1
#define LV_USE_TILEVIEW 1
#define LV_USE_WIN 0

/*====================
   LAYOUTS
 *====================*/

#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/*====================
   THEMES
 *====================*/

/* Enable the default theme */
#define LV_USE_THEME_DEFAULT 1

/* Enable a simple theme (less code, less RAM) */
#define LV_USE_THEME_SIMPLE 1

/*====================
   3RD PARTY LIBS
 *====================*/

/* File system interfaces - not needed for embedded use */
#define LV_USE_FS_STDIO 0
#define LV_USE_FS_POSIX 0
#define LV_USE_FS_WIN32 0
#define LV_USE_FS_FATFS 0
#define LV_USE_FS_LITTLEFS 0

/* PNG/BMP/JPG/GIF decoders - not needed */
#define LV_USE_PNG 0
#define LV_USE_BMP 0
#define LV_USE_SJPG 0
#define LV_USE_GIF 0

/* Other libs */
#define LV_USE_QRCODE 0
#define LV_USE_BARCODE 0
#define LV_USE_SNAPSHOT 0
#define LV_USE_MONKEY 0
#define LV_USE_IME_PINYIN 0
#define LV_USE_GRIDNAV 0
#define LV_USE_FRAGMENT 0
#define LV_USE_OBSERVER 1
#define LV_USE_SYSMON 0

/*====================
   DEVICES
 *====================*/

/* Input device (encoder) */
#define LV_USE_INDEV 1

/*====================
   COMPILER SETTINGS
 *====================*/

/* For big endian systems set to 1 */
#define LV_BIG_ENDIAN_SYSTEM 0

/* Attribute to mark large constant arrays (e.g. fonts, images, etc.) */
#define LV_ATTRIBUTE_LARGE_CONST

/* Compiler prefix for a large array in RAM */
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY

/* Place performance-critical functions into faster memory */
#define LV_ATTRIBUTE_FAST_MEM

/* Export integer constant to binding (not needed) */
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

/* Prefix for all global extern functions and variables */
#define LV_ATTRIBUTE_EXTERN_DATA

#endif /* LV_CONF_H */
