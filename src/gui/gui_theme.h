#ifndef GUI_THEME_H
#define GUI_THEME_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Color palette — matches web design system */
#define COLOR_PRIMARY     lv_color_hex(0xFF9800) /* Orange accent */
#define COLOR_PRIMARY_DK  lv_color_hex(0xE68900) /* Darker orange for pressed */
#define COLOR_BG_DARK     lv_color_hex(0x1A1A2E) /* Dark background */
#define COLOR_BG_CARD     lv_color_hex(0x16213E) /* Card background */
#define COLOR_BG_SURFACE  lv_color_hex(0x0F3460) /* Surface / selection */
#define COLOR_TEXT_PRI    lv_color_hex(0xFFFFFF) /* Primary text */
#define COLOR_TEXT_SEC    lv_color_hex(0xB0B0B0) /* Secondary text */
#define COLOR_TEXT_DIM    lv_color_hex(0x666666) /* Dimmed text */
#define COLOR_SUCCESS     lv_color_hex(0x4CAF50) /* Green */
#define COLOR_WARNING     lv_color_hex(0xFFC107) /* Yellow */
#define COLOR_ERROR       lv_color_hex(0xF44336) /* Red */
#define COLOR_INFO        lv_color_hex(0x2196F3) /* Blue */

/* Light mode colors */
#define COLOR_BG_LIGHT      lv_color_hex(0xF5F5F5)
#define COLOR_CARD_LIGHT    lv_color_hex(0xFFFFFF)
#define COLOR_SURFACE_LIGHT lv_color_hex(0xE0E0E0)
#define COLOR_TEXT_PRI_LT   lv_color_hex(0x212121)
#define COLOR_TEXT_SEC_LT   lv_color_hex(0x757575)

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
