#ifndef GUI_NAVIGATION_H
#define GUI_NAVIGATION_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Maximum depth of navigation stack */
#define NAV_STACK_MAX 8

/* Screen IDs for the navigation system */
enum ScreenId {
    SCR_DESKTOP = 0,
    SCR_CONTROL_MENU,
    SCR_WIFI_MENU,
    SCR_MQTT_MENU,
    SCR_SETTINGS_MENU,
    SCR_DEBUG_MENU,
    SCR_VALUE_EDIT,
    SCR_KEYBOARD,
    SCR_WIFI_SCAN,
    SCR_WIFI_AP_MENU,
    SCR_WIFI_NET_MENU,
    SCR_INFO,
    SCR_COUNT
};

/* Screen creation callback type */
typedef lv_obj_t* (*screen_create_fn)(void);

/* Initialize navigation system */
void gui_nav_init(void);

/* Push a new screen onto the navigation stack */
void gui_nav_push(ScreenId id);

/* Pop current screen and return to previous */
void gui_nav_pop(void);

/* Pop all the way back to desktop */
void gui_nav_pop_to_root(void);

/* Get current screen ID */
ScreenId gui_nav_current(void);

/* Get navigation stack depth */
int gui_nav_depth(void);

/* Register a screen creator function for a screen ID */
void gui_nav_register(ScreenId id, screen_create_fn creator);

/* Get the LVGL group for the current screen (for encoder binding) */
lv_group_t *gui_nav_get_group(void);

#endif /* GUI_ENABLED */
#endif /* GUI_NAVIGATION_H */
