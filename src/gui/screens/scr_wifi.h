#ifndef SCR_WIFI_H
#define SCR_WIFI_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Create the WiFi menu screen */
lv_obj_t *scr_wifi_create(void);

/* Create WiFi AP sub-menu */
lv_obj_t *scr_wifi_ap_create(void);

/* Create WiFi network config sub-menu */
lv_obj_t *scr_wifi_net_create(void);

#endif /* GUI_ENABLED */
#endif /* SCR_WIFI_H */
