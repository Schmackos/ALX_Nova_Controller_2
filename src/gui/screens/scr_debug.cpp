#ifdef GUI_ENABLED

#include "scr_debug.h"
#include "../gui_config.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../gui_theme.h"
#include "../../app_state.h"
#include "../../config.h"
#include "../../websocket_handler.h"
#include "../../i2s_audio.h"
#include "../../task_monitor.h"
#ifdef DAC_ENABLED
#include "../../dac_hal.h"
#endif
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>

/* Label handles for live updating */
static lv_obj_t *lbl_memory = nullptr;
static lv_obj_t *lbl_cpu = nullptr;
static lv_obj_t *lbl_storage = nullptr;
static lv_obj_t *lbl_network = nullptr;
static lv_obj_t *lbl_system = nullptr;
static lv_obj_t *lbl_audio_adc[NUM_AUDIO_INPUTS] = {nullptr, nullptr, nullptr};
static lv_obj_t *lbl_i2s = nullptr;
#ifdef DAC_ENABLED
static lv_obj_t *lbl_dac = nullptr;
static lv_obj_t *lbl_eeprom = nullptr;
#endif
static lv_obj_t *lbl_tasks = nullptr;
static lv_obj_t *lbl_pins = nullptr;
static lv_obj_t *lbl_sort_mode = nullptr;

/* Pin sort modes */
enum PinSortMode { SORT_BY_DEVICE = 0, SORT_BY_GPIO, SORT_BY_FUNCTION, SORT_MODE_COUNT };
static PinSortMode pin_sort_mode = SORT_BY_DEVICE;

/* Pin entry for sortable display */
struct PinEntry {
    const char *device;
    const char *function;
    int gpio;
};

static const PinEntry all_pins[] = {
    {"PCM1808 ADC 1&2", "BCK",   I2S_BCK_PIN},
    {"PCM1808 ADC 1",   "DOUT",  I2S_DOUT_PIN},
    {"PCM1808 ADC 2",   "DOUT2", I2S_DOUT2_PIN},
    {"PCM1808 ADC 1&2", "LRC",   I2S_LRC_PIN},
    {"PCM1808 ADC 1&2", "MCLK",  I2S_MCLK_PIN},
#ifdef DAC_ENABLED
    {"DAC Output",   "DOUT", I2S_TX_DATA_PIN},
    {"DAC I2C",      "SDA",  DAC_I2C_SDA_PIN},
    {"DAC I2C",      "SCL",  DAC_I2C_SCL_PIN},
#endif
    {"ST7735S TFT",  "CS",   TFT_CS_PIN},
    {"ST7735S TFT",  "MOSI", TFT_MOSI_PIN},
    {"ST7735S TFT",  "CLK",  TFT_SCLK_PIN},
    {"ST7735S TFT",  "DC",   TFT_DC_PIN},
    {"ST7735S TFT",  "RST",  TFT_RST_PIN},
    {"ST7735S TFT",  "BL",   TFT_BL_PIN},
    {"EC11 Encoder", "A",    ENCODER_A_PIN},
    {"EC11 Encoder", "B",    ENCODER_B_PIN},
    {"EC11 Encoder", "SW",   ENCODER_SW_PIN},
    {"HW-508 Buzz",  "IO",   BUZZER_PIN},
    {"Core",         "Amp",  AMPLIFIER_PIN},
    {"Core",         "Btn",  RESET_BUTTON_PIN},
};
static const int PIN_COUNT = sizeof(all_pins) / sizeof(all_pins[0]);

static const char *sort_mode_labels[] = {"Device", "GPIO#", "Function"};

/* Simple insertion sort for pin indices */
static void sort_pins(int *indices, int count, PinSortMode mode) {
    for (int i = 1; i < count; i++) {
        int key = indices[i];
        int j = i - 1;
        while (j >= 0) {
            bool swap = false;
            if (mode == SORT_BY_GPIO) {
                swap = all_pins[indices[j]].gpio > all_pins[key].gpio;
            } else if (mode == SORT_BY_FUNCTION) {
                swap = strcmp(all_pins[indices[j]].function, all_pins[key].function) > 0;
            }
            if (!swap) break;
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }
}

static void update_pins_label(void) {
    if (!lbl_pins) return;

    int indices[PIN_COUNT];
    for (int i = 0; i < PIN_COUNT; i++) indices[i] = i;

    char buf[512];
    int pos = 0;

    if (pin_sort_mode == SORT_BY_DEVICE) {
        /* Group by device — original layout */
        pos = snprintf(buf, sizeof(buf),
                 "PCM1808 ADC 1&2\n"
                 "  BCK=%d DOUT=%d DOUT2=%d\n"
                 "  LRC=%d MCLK=%d\n",
                 I2S_BCK_PIN, I2S_DOUT_PIN, I2S_DOUT2_PIN,
                 I2S_LRC_PIN, I2S_MCLK_PIN);
#ifdef DAC_ENABLED
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                 "DAC Output\n"
                 "  DOUT=%d SDA=%d SCL=%d\n",
                 I2S_TX_DATA_PIN, DAC_I2C_SDA_PIN, DAC_I2C_SCL_PIN);
#endif
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                 "ST7735S TFT 1.8\"\n"
                 "  CS=%d MOSI=%d CLK=%d\n"
                 "  DC=%d RST=%d BL=%d\n"
                 "EC11 Encoder\n"
                 "  A=%d B=%d SW=%d\n"
                 "HW-508 Buzzer\n"
                 "  IO=%d\n"
                 "Core\n"
                 "  Amp=%d Btn=%d",
                 TFT_CS_PIN, TFT_MOSI_PIN, TFT_SCLK_PIN,
                 TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN,
                 ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_SW_PIN,
                 BUZZER_PIN,
                 AMPLIFIER_PIN, RESET_BUTTON_PIN);
    } else {
        sort_pins(indices, PIN_COUNT, pin_sort_mode);
        pos = 0;
        for (int i = 0; i < PIN_COUNT && pos < (int)sizeof(buf) - 1; i++) {
            const PinEntry &p = all_pins[indices[i]];
            if (pin_sort_mode == SORT_BY_GPIO) {
                pos += snprintf(buf + pos, sizeof(buf) - pos,
                                "%2d %-4s %s\n", p.gpio, p.function, p.device);
            } else {
                pos += snprintf(buf + pos, sizeof(buf) - pos,
                                "%-4s %2d %s\n", p.function, p.gpio, p.device);
            }
        }
        /* Remove trailing newline */
        if (pos > 0 && buf[pos - 1] == '\n') buf[pos - 1] = '\0';
    }

    lv_label_set_text(lbl_pins, buf);
}

/* Sort button callback */
static void on_sort_click(lv_event_t *e) {
    (void)e;
    pin_sort_mode = (PinSortMode)((pin_sort_mode + 1) % SORT_MODE_COUNT);
    if (lbl_sort_mode) {
        lv_label_set_text(lbl_sort_mode, sort_mode_labels[pin_sort_mode]);
    }
    update_pins_label();
}

/* Back button callback */
static void on_back(lv_event_t *e) {
    (void)e;
    gui_nav_pop_deferred();
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

    char buf[128];

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

    /* Audio ADC — per-ADC diagnostics (always show both) */
    {
        static const char *status_names[] = {"OK", "NO DATA", "NOISE", "CLIP", "I2S ERR", "HW FAULT"};
        for (int a = 0; a < NUM_AUDIO_INPUTS; a++) {
            if (!lbl_audio_adc[a]) continue;
            const AppState::AdcState &adc = appState.audioAdc[a];
            const char *st2 = status_names[adc.healthStatus < 6 ? adc.healthStatus : 0];
            unsigned long age = 0;
            if (adc.lastNonZeroMs > 0) age = (millis() - adc.lastNonZeroMs) / 1000;
            const char *inputLabel = (a < NUM_AUDIO_ADCS) ? "ADC" : "USB";
            int inputNum = (a < NUM_AUDIO_ADCS) ? (a + 1) : (a - NUM_AUDIO_ADCS + 1);
            snprintf(buf, sizeof(buf), "%s %d\n%s %.0fdB\n%.3fV\nFl:%.0f\nSNR:%.0f SFDR:%.0f\nCl:%lu E:%lu\n%lus",
                     inputLabel, inputNum, st2, adc.dBFS,
                     (adc.vrms1 > adc.vrms2) ? adc.vrms1 : adc.vrms2,
                     adc.noiseFloorDbfs,
                     appState.audioSnrDb[a], appState.audioSfdrDb[a],
                     (unsigned long)adc.clippedSamples,
                     (unsigned long)adc.i2sErrors, age);
            lv_label_set_text(lbl_audio_adc[a], buf);
        }
    }

    /* Audio Quality Diagnostics */
    /* Audio DAC */
#ifdef DAC_ENABLED
    if (lbl_dac) {
        DacDriver* drv = dac_get_driver();
        const char* model = appState.dacModelName;
        const char* statusStr = appState.dacReady ? "Ready" : (appState.dacEnabled ? "Not Ready" : "Off");
        snprintf(buf, sizeof(buf), "%s  %s\nVol:%u%% %s %s\nCh:%u Det:%s\nTX Underruns:%lu",
                 model, statusStr,
                 (unsigned)appState.dacVolume,
                 appState.dacMute ? "MUTE" : "",
                 appState.dacEnabled ? "ON" : "OFF",
                 (unsigned)appState.dacOutputChannels,
                 appState.dacDetected ? "EEPROM" : "Manual",
                 (unsigned long)appState.dacTxUnderruns);
        lv_label_set_text(lbl_dac, buf);
    }
    if (lbl_eeprom) {
        const AppState::EepromDiag& ed = appState.eepromDiag;
        if (!ed.scanned) {
            lv_label_set_text(lbl_eeprom, "Not scanned");
        } else if (!ed.found) {
            snprintf(buf, sizeof(buf), "No EEPROM found\nI2C devs: %d\nR/W err: %lu/%lu",
                     ed.i2cTotalDevices,
                     (unsigned long)ed.readErrors,
                     (unsigned long)ed.writeErrors);
            lv_label_set_text(lbl_eeprom, buf);
        } else {
            snprintf(buf, sizeof(buf), "0x%02X %s\n%s rev%u\nCh:%u ID:0x%04X\nR/W err: %lu/%lu",
                     ed.eepromAddr, ed.deviceName,
                     ed.manufacturer, (unsigned)ed.hwRevision,
                     (unsigned)ed.maxChannels, ed.deviceId,
                     (unsigned long)ed.readErrors,
                     (unsigned long)ed.writeErrors);
            lv_label_set_text(lbl_eeprom, buf);
        }
    }
#endif

    /* I2S Configuration */
    if (lbl_i2s) {
        if (!(appState.debugMode && appState.debugI2sMetrics)) {
            lv_label_set_text(lbl_i2s, "Disabled");
        } else {
            I2sStaticConfig i2sCfg = i2s_audio_get_static_config();
            const auto &m = appState.i2sMetrics;
            snprintf(buf, sizeof(buf),
                     "Rate:%lukHz 32b(24) DMA:%dx%d\n"
                     "APLL: M=%s S=%s\n"
                     "Stack: %lu/%dB free\n"
                     "Buf/s: %.0f / %.0f\n"
                     "Lat: %.1f / %.1fms",
                     (unsigned long)(i2sCfg.adc[0].sampleRate / 1000),
                     i2sCfg.adc[0].dmaBufCount, i2sCfg.adc[0].dmaBufLen,
                     i2sCfg.adc[0].apllEnabled ? "On" : "Off",
                     i2sCfg.adc[1].apllEnabled ? "On" : "Off",
                     (unsigned long)m.audioTaskStackFree, TASK_STACK_SIZE_AUDIO,
                     m.buffersPerSec[0], m.buffersPerSec[1],
                     m.avgReadLatencyUs[0] / 1000.0f,
                     m.avgReadLatencyUs[1] / 1000.0f);
            lv_label_set_text(lbl_i2s, buf);
        }
    }

    /* Tasks */
    if (lbl_tasks) {
        if (!(appState.debugMode && appState.debugTaskMonitor)) {
            lv_label_set_text(lbl_tasks, "Disabled");
        } else {
            const TaskMonitorData& tm = task_monitor_get_data();
            char tbuf[256];
            int pos = 0;
            pos += snprintf(tbuf + pos, sizeof(tbuf) - pos,
                            "Loop: %luus / %luus max\n",
                            (unsigned long)tm.loopTimeAvgUs,
                            (unsigned long)tm.loopTimeMaxUs);
            // Show app-relevant tasks only (skip IDLE, ipc, Tmr Svc, wifi, tiT)
            for (int i = 0; i < tm.taskCount && pos < (int)sizeof(tbuf) - 40; i++) {
                const TaskInfo& t = tm.tasks[i];
                if (strncmp(t.name, "IDLE", 4) == 0 ||
                    strncmp(t.name, "ipc", 3) == 0 ||
                    strncmp(t.name, "Tmr", 3) == 0 ||
                    strcmp(t.name, "wifi") == 0 ||
                    strcmp(t.name, "tiT") == 0) continue;
                if (t.stackAllocBytes > 0) {
                    pos += snprintf(tbuf + pos, sizeof(tbuf) - pos,
                                    "%-10s %luK/%-3luK P%d %s C%d\n",
                                    t.name,
                                    (unsigned long)(t.stackFreeBytes / 1024),
                                    (unsigned long)(t.stackAllocBytes / 1024),
                                    t.priority,
                                    task_monitor_state_name(t.state),
                                    t.coreId);
                } else {
                    pos += snprintf(tbuf + pos, sizeof(tbuf) - pos,
                                    "%-10s %luK     P%d %s C%d\n",
                                    t.name,
                                    (unsigned long)(t.stackFreeBytes / 1024),
                                    t.priority,
                                    task_monitor_state_name(t.state),
                                    t.coreId);
                }
            }
            if (pos > 0 && tbuf[pos - 1] == '\n') tbuf[pos - 1] = '\0';
            lv_label_set_text(lbl_tasks, tbuf);
        }
    }
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
    for (int i = 0; i < NUM_AUDIO_INPUTS; i++) lbl_audio_adc[i] = nullptr;
    lbl_i2s = nullptr;
#ifdef DAC_ENABLED
    lbl_dac = nullptr;
#endif
    lbl_tasks = nullptr;
    lbl_pins = nullptr;
    lbl_sort_mode = nullptr;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, gui_style_screen(), LV_PART_MAIN);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, ICON_DEBUG " Debug");
    lv_obj_add_style(title, gui_style_title(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    /* Scrollable container */
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, DISPLAY_HEIGHT, DISPLAY_WIDTH - 36);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(cont, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 2, LV_PART_MAIN);
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

    /* Audio ADC — 2-column side-by-side layout */
    {
        lv_obj_t *adc_hdr = lv_label_create(cont);
        lv_label_set_text(adc_hdr, "Audio ADC");
        lv_obj_set_style_text_color(adc_hdr, COLOR_PRIMARY, LV_PART_MAIN);
        lv_obj_set_style_text_font(adc_hdr, &lv_font_montserrat_12, LV_PART_MAIN);

        lv_obj_t *adc_row = lv_obj_create(cont);
        lv_obj_set_size(adc_row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(adc_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(adc_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(adc_row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_column(adc_row, 4, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(adc_row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(adc_row, 0, LV_PART_MAIN);
        lv_obj_clear_flag(adc_row, LV_OBJ_FLAG_SCROLLABLE);

        for (int a = 0; a < NUM_AUDIO_INPUTS; a++) {
            lbl_audio_adc[a] = lv_label_create(adc_row);
            lv_label_set_text(lbl_audio_adc[a], "...");
            lv_obj_add_style(lbl_audio_adc[a], gui_style_dim(), LV_PART_MAIN);
            lv_obj_set_flex_grow(lbl_audio_adc[a], 1);
            lv_label_set_long_mode(lbl_audio_adc[a], LV_LABEL_LONG_WRAP);
        }
    }

    /* Audio DAC */
#ifdef DAC_ENABLED
    lbl_dac = add_section(cont, "Audio DAC");
    lbl_eeprom = add_section(cont, "EEPROM");
#endif

    /* I2S Configuration */
    lbl_i2s = add_section(cont, "I2S");

    /* Tasks */
    lbl_tasks = add_section(cont, "Tasks");

    /* GPIO Pins — sortable section */
    {
        /* Header row with sort button */
        lv_obj_t *pin_hdr = lv_obj_create(cont);
        lv_obj_set_size(pin_hdr, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(pin_hdr, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(pin_hdr, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(pin_hdr, 0, LV_PART_MAIN);
        lv_obj_clear_flag(pin_hdr, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *hdr_lbl = lv_label_create(pin_hdr);
        lv_label_set_text(hdr_lbl, "GPIO Pins");
        lv_obj_set_style_text_color(hdr_lbl, COLOR_PRIMARY, LV_PART_MAIN);
        lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_align(hdr_lbl, LV_ALIGN_LEFT_MID, 0, 0);

        /* Sort button */
        lv_obj_t *sort_btn = lv_obj_create(pin_hdr);
        lv_obj_set_size(sort_btn, LV_SIZE_CONTENT, 14);
        lv_obj_align(sort_btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_style(sort_btn, gui_style_list_item(), LV_PART_MAIN);
        lv_obj_add_style(sort_btn, gui_style_list_item_focused(), ((lv_style_selector_t)LV_PART_MAIN | (lv_style_selector_t)LV_STATE_FOCUSED));
        lv_obj_add_flag(sort_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(sort_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_hor(sort_btn, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(sort_btn, 1, LV_PART_MAIN);
        lv_group_add_obj(gui_nav_get_group(), sort_btn);

        lbl_sort_mode = lv_label_create(sort_btn);
        lv_label_set_text(lbl_sort_mode, sort_mode_labels[pin_sort_mode]);
        lv_obj_add_style(lbl_sort_mode, gui_style_dim(), LV_PART_MAIN);
        lv_obj_center(lbl_sort_mode);
        lv_obj_add_event_cb(sort_btn, on_sort_click, LV_EVENT_CLICKED, NULL);

        /* Pin data label */
        lbl_pins = lv_label_create(cont);
        lv_label_set_text(lbl_pins, "...");
        lv_obj_add_style(lbl_pins, gui_style_dim(), LV_PART_MAIN);
        lv_obj_set_width(lbl_pins, LV_PCT(100));
        lv_label_set_long_mode(lbl_pins, LV_LABEL_LONG_WRAP);

        pin_sort_mode = SORT_BY_DEVICE;
        update_pins_label();
    }

    /* Back button at bottom */
    lv_obj_t *back_btn = lv_obj_create(scr);
    lv_obj_set_size(back_btn, 60, 16);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_add_style(back_btn, gui_style_list_item(), LV_PART_MAIN);
    lv_obj_add_style(back_btn, gui_style_list_item_focused(), ((lv_style_selector_t)LV_PART_MAIN | (lv_style_selector_t)LV_STATE_FOCUSED));
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
