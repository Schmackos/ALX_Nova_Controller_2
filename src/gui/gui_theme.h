#ifndef GUI_THEME_H
#define GUI_THEME_H

#ifdef GUI_ENABLED

#include <lvgl.h>
#include "../design_tokens.h"

/* Color palette — uses shared design tokens */
#define COLOR_PRIMARY     lv_color_hex(DT_ACCENT)
#define COLOR_PRIMARY_DK  lv_color_hex(DT_ACCENT_DARK)
#define COLOR_BG_DARK     lv_color_hex(DT_DARK_BG)
#define COLOR_BG_CARD     lv_color_hex(DT_DARK_CARD)
#define COLOR_BG_SURFACE  lv_color_hex(DT_DARK_SURFACE)
#define COLOR_TEXT_PRI    lv_color_hex(DT_TEXT_PRIMARY)
#define COLOR_TEXT_SEC    lv_color_hex(DT_TEXT_SECONDARY)
#define COLOR_TEXT_DIM    lv_color_hex(DT_TEXT_DISABLED)
#define COLOR_SUCCESS     lv_color_hex(DT_SUCCESS)
#define COLOR_WARNING     lv_color_hex(DT_WARNING)
#define COLOR_ERROR       lv_color_hex(DT_ERROR)
#define COLOR_INFO        lv_color_hex(DT_INFO)

/* Light mode colors */
#define COLOR_BG_LIGHT      lv_color_hex(DT_LIGHT_BG)
#define COLOR_CARD_LIGHT    lv_color_hex(DT_LIGHT_CARD)
#define COLOR_SURFACE_LIGHT lv_color_hex(DT_LIGHT_SURFACE)
#define COLOR_TEXT_PRI_LT   lv_color_hex(DT_TEXT_PRIMARY_LT)
#define COLOR_TEXT_SEC_LT   lv_color_hex(DT_TEXT_SEC_LT)

/* Border colors from tokens */
#define COLOR_BORDER_DARK   lv_color_hex(DT_DARK_BORDER)
#define COLOR_BORDER_LIGHT  lv_color_hex(DT_LIGHT_BORDER)

/* Initialize theme and apply to default display */
void gui_theme_init(bool dark_mode);

/* Switch between dark and light mode at runtime */
void gui_theme_set_dark(bool dark_mode);

/* Get whether dark mode is active */
bool gui_theme_is_dark(void);

/* Style accessors for screen modules */
lv_style_t *gui_style_screen(void);
lv_style_t *gui_style_card(void);
lv_style_t *gui_style_card_focused(void);
lv_style_t *gui_style_title(void);
lv_style_t *gui_style_body(void);
lv_style_t *gui_style_dim(void);
lv_style_t *gui_style_btn(void);
lv_style_t *gui_style_btn_pressed(void);
lv_style_t *gui_style_list_item(void);
lv_style_t *gui_style_list_item_focused(void);

#endif /* GUI_ENABLED */
#endif /* GUI_THEME_H */
