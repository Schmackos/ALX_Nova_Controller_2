#ifndef SCR_MQTT_H
#define SCR_MQTT_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Create the MQTT main menu screen */
lv_obj_t *scr_mqtt_create(void);

#endif /* GUI_ENABLED */
#endif /* SCR_MQTT_H */
