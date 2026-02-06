#ifdef GUI_ENABLED

#include "gui_manager.h"
#include "gui_config.h"
#include "gui_input.h"
#include "gui_navigation.h"
#include "gui_theme.h"
#include "screens/scr_desktop.h"
#include "screens/scr_control.h"
#include "screens/scr_wifi.h"
#include "screens/scr_mqtt.h"
#include "screens/scr_settings.h"
#include "screens/scr_debug.h"
#include "screens/scr_support.h"
#include "screens/scr_boot_anim.h"
#include "../app_state.h"
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

/* LVGL tick callback (millis returns unsigned long, LVGL expects uint32_t) */
static uint32_t lv_millis_cb(void) { return (uint32_t)millis(); }

/* TFT driver instance */
static TFT_eSPI tft = TFT_eSPI();

/* LVGL display buffer */
static lv_color_t draw_buf[DISP_BUF_SIZE];

/* Screen sleep state */
static bool screen_awake = true;
static unsigned long last_activity_time = 0;

/* FreeRTOS task handle */
static TaskHandle_t gui_task_handle = nullptr;

/* Dashboard refresh interval */
#define DASHBOARD_REFRESH_MS 1000
static unsigned long last_dashboard_refresh = 0;

/* LVGL display flush callback */
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

/* Set backlight brightness (0-255) */
static void set_backlight(uint8_t brightness) {
    ledcWrite(BL_PWM_CHANNEL, brightness);
}

/* Put display to sleep */
static void screen_sleep(void) {
    if (!screen_awake) return;
    screen_awake = false;
    set_backlight(BL_BRIGHTNESS_MIN);
    AppState::getInstance().setBacklightOn(false);
    Serial.println("[GUI] Screen sleep");
}

/* Wake display */
static void screen_wake(void) {
    last_activity_time = millis();
    if (screen_awake) return;
    screen_awake = true;
    set_backlight(BL_BRIGHTNESS_MAX);
    AppState::getInstance().setBacklightOn(true);
    Serial.println("[GUI] Screen wake");
}

/* GUI FreeRTOS task */
static void gui_task(void *param) {
    (void)param;

    Serial.println("[GUI] Task started on core " + String(xPortGetCoreID()));

    /* Flush one black frame to overwrite any stale display RAM
       (e.g. desktop from previous boot), then turn on backlight. */
    lv_timer_handler();
    set_backlight(BL_BRIGHTNESS_MAX);

    /* Play boot animation and load desktop inside the task so all
       lv_timer_handler() calls originate from the same FreeRTOS context. */
    boot_anim_play();
    gui_nav_push(SCR_DESKTOP);
    last_activity_time = millis();

    for (;;) {
        /* Check for input activity to wake screen */
        if (gui_input_activity()) {
            screen_wake();
        }

        /* Poll AppState for external backlight changes (web/MQTT -> GUI) */
        bool desired = AppState::getInstance().backlightOn;
        if (desired && !screen_awake) {
            screen_wake();
        } else if (!desired && screen_awake) {
            screen_sleep();
        }

        /* Handle screen timeout */
        unsigned long timeout_ms = AppState::getInstance().screenTimeout;
        if (screen_awake && timeout_ms > 0) {
            if (millis() - last_activity_time > timeout_ms) {
                screen_sleep();
            }
        }

        /* Refresh active screen data periodically */
        if (millis() - last_dashboard_refresh > DASHBOARD_REFRESH_MS) {
            last_dashboard_refresh = millis();
            ScreenId cur = gui_nav_current();
            if (cur == SCR_DESKTOP) {
                scr_desktop_refresh();
            } else if (cur == SCR_DEBUG_MENU) {
                scr_debug_refresh();
            } else if (cur == SCR_CONTROL_MENU) {
                scr_control_refresh();
            } else if (cur == SCR_WIFI_MENU) {
                scr_wifi_refresh();
            } else if (cur == SCR_WIFI_AP_MENU) {
                scr_wifi_ap_refresh();
            } else if (cur == SCR_MQTT_MENU) {
                scr_mqtt_refresh();
            } else if (cur == SCR_SETTINGS_MENU) {
                scr_settings_refresh();
            }
        }

        /* Run LVGL timer handler */
        uint32_t time_till_next = lv_timer_handler();

        /* Delay until next LVGL tick needed */
        uint32_t delay_ms = (time_till_next < GUI_TICK_PERIOD_MS) ? time_till_next : GUI_TICK_PERIOD_MS;
        if (delay_ms < GUI_TICK_PERIOD_MS) delay_ms = GUI_TICK_PERIOD_MS;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/* Register all screen creators */
static void register_screens(void) {
    gui_nav_register(SCR_DESKTOP, scr_desktop_create);
    gui_nav_register(SCR_CONTROL_MENU, scr_control_create);
    gui_nav_register(SCR_WIFI_MENU, scr_wifi_create);
    gui_nav_register(SCR_WIFI_AP_MENU, scr_wifi_ap_create);
    gui_nav_register(SCR_WIFI_NET_MENU, scr_wifi_net_create);
    gui_nav_register(SCR_MQTT_MENU, scr_mqtt_create);
    gui_nav_register(SCR_SETTINGS_MENU, scr_settings_create);
    gui_nav_register(SCR_SUPPORT_MENU, scr_support_create);
    gui_nav_register(SCR_DEBUG_MENU, scr_debug_create);
}

void gui_init(void) {
    Serial.println("[GUI] Initializing...");

    /* Initialize backlight PWM */
    ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_RESOLUTION);
    ledcAttachPin(TFT_BL_PIN, BL_PWM_CHANNEL);
    set_backlight(0);  /* Keep backlight OFF until content is ready */

    /* Initialize TFT */
    tft.init();
    tft.setRotation(1); /* Landscape: 160x128 */
    tft.fillScreen(TFT_BLACK);
    Serial.println("[GUI] TFT initialized (ST7735S 160x128 landscape)");

    /* Initialize LVGL */
    lv_init();
    lv_tick_set_cb(lv_millis_cb);
    Serial.println("[GUI] LVGL initialized");

    /* Create LVGL display with buffer */
    lv_display_t *disp = lv_display_create(DISPLAY_HEIGHT, DISPLAY_WIDTH); /* landscape: 160x128 */
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Initialize theme (dark mode by default) */
    gui_theme_init(true);

    /* Initialize input devices */
    gui_input_init();

    /* Initialize navigation system */
    gui_nav_init();

    /* Register all screen creators */
    register_screens();

    /* Boot animation + desktop push happen inside gui_task so all
       lv_timer_handler() calls stay in one FreeRTOS context. */

    /* Start FreeRTOS GUI task on Core 1 */
    xTaskCreatePinnedToCore(
        gui_task,
        "gui_task",
        GUI_TASK_STACK_SIZE,
        NULL,
        GUI_TASK_PRIORITY,
        &gui_task_handle,
        GUI_TASK_CORE
    );

    Serial.println("[GUI] Initialization complete");
}

void gui_wake(void) {
    AppState::getInstance().setBacklightOn(true);
    screen_wake();
}

void gui_sleep(void) {
    AppState::getInstance().setBacklightOn(false);
    screen_sleep();
}

bool gui_is_awake(void) {
    return screen_awake;
}

#endif /* GUI_ENABLED */
