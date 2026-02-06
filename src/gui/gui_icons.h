#ifndef GUI_ICONS_H
#define GUI_ICONS_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Category icons using LVGL built-in symbols */
#define ICON_CONTROL    LV_SYMBOL_POWER
#define ICON_WIFI       LV_SYMBOL_WIFI
#define ICON_MQTT       LV_SYMBOL_UPLOAD   /* Cloud-like symbol for MQTT */
#define ICON_SETTINGS   LV_SYMBOL_SETTINGS
#define ICON_DEBUG       LV_SYMBOL_LIST
#define ICON_SUPPORT    LV_SYMBOL_FILE

/* Navigation icons */
#define ICON_BACK       LV_SYMBOL_LEFT
#define ICON_NEXT       LV_SYMBOL_RIGHT
#define ICON_OK         LV_SYMBOL_OK
#define ICON_EDIT       LV_SYMBOL_EDIT

/* Status icons */
#define ICON_CONNECTED  LV_SYMBOL_OK
#define ICON_DISCONNECTED LV_SYMBOL_CLOSE
#define ICON_WARNING    LV_SYMBOL_WARNING
#define ICON_REFRESH    LV_SYMBOL_REFRESH

#endif /* GUI_ENABLED */
#endif /* GUI_ICONS_H */
