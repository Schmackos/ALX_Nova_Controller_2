#ifndef SCR_MENU_H
#define SCR_MENU_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Maximum menu items per screen */
#define MENU_MAX_ITEMS 20

/* Menu item types */
enum MenuItemType {
    MENU_ACTION,   /* Calls a callback on select */
    MENU_SUBMENU,  /* Navigates to a sub-screen */
    MENU_INFO,     /* Read-only display, not selectable */
    MENU_BACK,     /* "< Back" item */
};

/* Menu item callback */
typedef void (*menu_action_fn)(void);

/* Menu item definition */
struct MenuItem {
    const char *label;
    const char *value;       /* Current value text shown on right side, or NULL */
    const char *icon;        /* LVGL symbol or NULL */
    MenuItemType type;
    menu_action_fn action;   /* Callback for ACTION/SUBMENU items */
};

/* Menu configuration */
struct MenuConfig {
    const char *title;       /* Screen title */
    MenuItem items[MENU_MAX_ITEMS];
    int item_count;
};

/* Create a menu screen from a MenuConfig */
lv_obj_t *scr_menu_create(const MenuConfig *config);

/* Update a menu item's value text dynamically */
void scr_menu_set_item_value(int index, const char *value);

#endif /* GUI_ENABLED */
#endif /* SCR_MENU_H */
