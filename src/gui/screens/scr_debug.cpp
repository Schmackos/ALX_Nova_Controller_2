#ifdef GUI_ENABLED

#include "scr_debug.h"
#include "../gui_config.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../gui_theme.h"
#include "../../app_state.h"
#include "../../config.h"
#include "../../websocket_handler.h"
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>

/* Label handles for live updating */
static lv_obj_t *lbl_memory = nullptr;
static lv_obj_t *lbl_cpu = nullptr;
static lv_obj_t *lbl_storage = nullptr;
static lv_obj_t *lbl_network = nullptr;
static lv_obj_t *lbl_system = nullptr;

/* Back button callback */
static void on_back(lv_event_t *e) {
    (void)e;
    gui_nav_pop();
}

static void format_uptime(unsigned long ms, char *buf, int len) {
    unsigned long secs = ms / 1000;
    unsigned long mins = secs / 60;
    unsigned long hours = mins / 60;
    unsigned long days = hours / 24;
    if (days > 0) {
        snprintf(buf, len, "%lud %luh %lum", days, hours % 24, mins % 60);
    } else if (hours > 0) {
        snprintf(buf, len, "%luh %lum %lus", hours, mins % 60, secs % 60);
    } else if (mins > 0) {
        snprintf(buf, len, "%lum %lus", mins, secs % 60);
    } else {
        snprintf(buf, len, "%lus", secs);
    }
}

void scr_debug_refresh(void) {
    if (!lbl_memory) return;

    char buf[80];

    /* Memory */
    uint32_t heap_free = ESP.getFreeHeap() / 1024;
    uint32_t heap_total = ESP.getHeapSize() / 1024;
    snprintf(buf, sizeof(buf), "Heap: %uKB / %uKB\nMin: %uKB  Block: %uKB",
             (unsigned)heap_free, (unsigned)heap_total,
             (unsigned)(ESP.getMinFreeHeap() / 1024),
             (unsigned)(ESP.getMaxAllocHeap() / 1024));
    lv_label_set_text(lbl_memory, buf);

    /* CPU */
    updateCpuUsage();
    float temp = temperatureRead();
    snprintf(buf, sizeof(buf), "%dMHz %d cores\nLoad: %.0f%%/%.0f%%  %.1fC",
             (int)ESP.getCpuFreqMHz(), (int)ESP.getChipCores(),
             getCpuUsageCore0(), getCpuUsageCore1(), temp);
    lv_label_set_text(lbl_cpu, buf);

    /* Storage */
    uint32_t fs_total = LittleFS.totalBytes() / 1024;
    uint32_t fs_used = LittleFS.usedBytes() / 1024;
    uint32_t flash_mb = ESP.getFlashChipSize() / (1024 * 1024);
    snprintf(buf, sizeof(buf), "Flash: %uMB  FW: %uKB\nFS: %uKB / %uKB",
             (unsigned)flash_mb,
             (unsigned)(ESP.getSketchSize() / 1024),
             (unsigned)fs_used, (unsigned)fs_total);
    lv_label_set_text(lbl_storage, buf);

    /* Network */
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(buf, sizeof(buf), "RSSI: %ddBm  Ch: %d\nIP: %s",
                 WiFi.RSSI(), WiFi.channel(),
                 WiFi.localIP().toString().c_str());
    } else {
        snprintf(buf, sizeof(buf), "WiFi: Disconnected\nAP clients: %d",
                 WiFi.softAPgetStationNum());
    }
    lv_label_set_text(lbl_network, buf);

    /* System */
    char uptime_str[24];
    format_uptime(millis(), uptime_str, sizeof(uptime_str));
    snprintf(buf, sizeof(buf), "Up: %s\nFW: %s", uptime_str, FIRMWARE_VERSION);
    lv_label_set_text(lbl_system, buf);
}

/* Helper to create a section with header and value label */
static lv_obj_t *add_section(lv_obj_t *parent, const char *title) {
    lv_obj_t *hdr = lv_label_create(parent);
    lv_label_set_text(hdr, title);
    lv_obj_set_style_text_color(hdr, COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_t *val = lv_label_create(parent);
    lv_label_set_text(val, "...");
    lv_obj_add_style(val, gui_style_dim(), LV_PART_MAIN);
    lv_obj_set_width(val, LV_PCT(100));
    lv_label_set_long_mode(val, LV_LABEL_LONG_WRAP);

    return val;
}

lv_obj_t *scr_debug_create(void) {
    /* Reset static pointers — previous screen objects were auto-deleted */
    lbl_memory = nullptr;
    lbl_cpu = nullptr;
    lbl_storage = nullptr;
    lbl_network = nullptr;
    lbl_system = nullptr;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, gui_style_screen(), LV_PART_MAIN);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, ICON_DEBUG " Debug");
    lv_obj_add_style(title, gui_style_title(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    /* Scrollable container */
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, DISPLAY_HEIGHT, DISPLAY_WIDTH - 22);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(cont, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);

    /* Make scrollable for encoder */
    lv_group_add_obj(gui_nav_get_group(), cont);

    /* Sections */
    lbl_memory  = add_section(cont, "Memory");
    lbl_cpu     = add_section(cont, "CPU");
    lbl_storage = add_section(cont, "Storage");
    lbl_network = add_section(cont, "Network");
    lbl_system  = add_section(cont, "System");

    /* Back button at bottom */
    lv_obj_t *back_btn = lv_obj_create(scr);
    lv_obj_set_size(back_btn, 60, 16);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_add_style(back_btn, gui_style_list_item(), LV_PART_MAIN);
    lv_obj_add_style(back_btn, gui_style_list_item_focused(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_group_add_obj(gui_nav_get_group(), back_btn);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, ICON_BACK " Back");
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT_SEC, LV_PART_MAIN);
    lv_obj_add_style(back_lbl, gui_style_dim(), LV_PART_MAIN);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, on_back, LV_EVENT_CLICKED, NULL);

    /* Initial data fill */
    scr_debug_refresh();

    return scr;
}

#endif /* GUI_ENABLED */
