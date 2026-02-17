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
#include "screens/scr_home.h"
#include "screens/scr_siggen.h"
#ifdef DSP_ENABLED
#include "screens/scr_dsp.h"
#endif
#include "screens/scr_boot_anim.h"
#include "../app_state.h"
#include "../buzzer_handler.h"
#include "../debug_serial.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#ifdef WOKWI_BUILD
#include "lgfx_config_wokwi.h"
#else
#include "lgfx_config.h"
#endif
#include <lvgl.h>

/* LVGL tick callback (millis returns unsigned long, LVGL expects uint32_t) */
static uint32_t lv_millis_cb(void) { return (uint32_t)millis(); }

/* LovyanGFX driver instance */
static LGFX tft;

/* LVGL display buffer */
static lv_color_t *draw_buf1 = nullptr;

/* Screen sleep state */
static bool screen_awake = true;
static bool screen_dimmed = false;
static unsigned long last_activity_time = 0;
static unsigned long last_sleep_time = 0;

/* Cooldown after sleep to prevent noise-triggered re-wake (ms) */
#define SLEEP_WAKE_COOLDOWN_MS 1500

/* FreeRTOS task handle */
static TaskHandle_t gui_task_handle = nullptr;

/* Dashboard refresh interval */
#define DASHBOARD_REFRESH_MS 1000
static unsigned long last_dashboard_refresh = 0;

/* LVGL display flush callback — matches official LVGL+LovyanGFX driver.
   Uses RGB565_SWAPPED format: LVGL renders directly in SPI byte order,
   so no manual byte swap is needed in the flush path.
   Cast to swap565_t tells LovyanGFX the data is already in SPI wire format,
   which triggers the direct write_bytes DMA path (no pixel conversion). */
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    tft.pushImageDMA(area->x1, area->y1, w, h, (lgfx::swap565_t *)px_map);
    tft.waitDMA();

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
    last_sleep_time = millis();
    LOG_D("[GUI] Screen sleep");
}

/* Wake display */
static void screen_wake(void) {
    last_activity_time = millis();
    screen_dimmed = false;
    if (screen_awake) return;
    screen_awake = true;
    set_backlight(AppState::getInstance().backlightBrightness);
    AppState::getInstance().setBacklightOn(true);
    LOG_D("[GUI] Screen wake");
}

/* Track last applied brightness to detect changes */
static uint8_t last_applied_brightness = 255;

/* Dim display (reduce brightness without sleeping) */
static void screen_dim(void) {
    if (screen_dimmed || !screen_awake) return;
    screen_dimmed = true;
    uint8_t dim_val = AppState::getInstance().dimBrightness;
    set_backlight(dim_val);
    last_applied_brightness = dim_val;
    LOG_D("[GUI] Screen dimmed");
}

/* GUI FreeRTOS task */
static void gui_task(void *param) {
    (void)param;

    LOG_I("[GUI] Task started on core %d", xPortGetCoreID());

    // Register GUI task with Task Watchdog Timer
    esp_task_wdt_add(NULL);

    /* Flush one black frame to overwrite any stale display RAM
       (e.g. desktop from previous boot), then turn on backlight. */
    lv_timer_handler();
    set_backlight(AppState::getInstance().backlightBrightness);
    last_applied_brightness = AppState::getInstance().backlightBrightness;

    /* Play boot animation and load desktop inside the task so all
       lv_timer_handler() calls originate from the same FreeRTOS context. */
    boot_anim_play();
    gui_nav_push(SCR_DESKTOP);
    last_activity_time = millis();

    for (;;) {
        esp_task_wdt_reset();  // Feed watchdog at top of each GUI iteration

        /* Check for input activity to wake/undim screen */
        bool any_activity = gui_input_activity();
        bool btn_activity = gui_input_press_activity();

        if (any_activity) {
            if (!screen_awake) {
                /* Wake from sleep: cooldown prevents noise-triggered re-wake
                   from encoder pin EMI right after sleep */
                if (millis() - last_sleep_time >= SLEEP_WAKE_COOLDOWN_MS) {
                    screen_wake();
                }
            } else if (screen_dimmed) {
                /* Undim on any input (rotation or press) */
                screen_dimmed = false;
                set_backlight(AppState::getInstance().backlightBrightness);
                last_applied_brightness = AppState::getInstance().backlightBrightness;
                last_activity_time = millis();
            } else {
                /* Already awake — reset activity timer */
                screen_wake();
            }
        }

        /* Poll AppState for external backlight changes (web/MQTT -> GUI) */
        static bool prev_desired_bl = true;
        bool desired = AppState::getInstance().backlightOn;
        bool bl_just_enabled = (desired && !prev_desired_bl);
        prev_desired_bl = desired;

        if (desired && !screen_awake) {
            screen_wake();
        } else if (!desired && screen_awake) {
            screen_sleep();
        } else if (bl_just_enabled && screen_awake && screen_dimmed) {
            screen_dimmed = false;
            set_backlight(AppState::getInstance().backlightBrightness);
            last_applied_brightness = AppState::getInstance().backlightBrightness;
            last_activity_time = millis();
        }

        /* Apply brightness changes while screen is awake (not dimmed) */
        if (screen_awake && !screen_dimmed) {
            uint8_t cur_brightness = AppState::getInstance().backlightBrightness;
            if (cur_brightness != last_applied_brightness) {
                set_backlight(cur_brightness);
                last_applied_brightness = cur_brightness;
            }
        }

        /* Handle dim timeout */
        unsigned long dim_ms = AppState::getInstance().dimTimeout;
        if (screen_awake && !screen_dimmed && AppState::getInstance().dimEnabled && dim_ms > 0) {
            if (millis() - last_activity_time > dim_ms) {
                screen_dim();
            }
        }

        /* Handle screen timeout */
        unsigned long timeout_ms = AppState::getInstance().screenTimeout;
        if (screen_awake && timeout_ms > 0) {
            if (millis() - last_activity_time > timeout_ms) {
                screen_sleep();
            }
        }

        /* Refresh active screen data periodically (skip when screen is asleep) */
        if (screen_awake && millis() - last_dashboard_refresh > DASHBOARD_REFRESH_MS) {
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
            } else if (cur == SCR_HOME) {
                scr_home_refresh();
            } else if (cur == SCR_SIGGEN_MENU) {
                scr_siggen_refresh();
            }
#ifdef DSP_ENABLED
            else if (cur == SCR_DSP_MENU) {
                scr_dsp_refresh();
            } else if (cur == SCR_PEQ_MENU) {
                scr_peq_refresh();
            } else if (cur == SCR_PEQ_BAND_EDIT) {
                scr_peq_band_refresh();
            }
#endif
        }

        /* Process buzzer patterns with low latency (same core as encoder ISR) */
        buzzer_update();

        /* Run LVGL timer handler */
        uint32_t time_till_next = lv_timer_handler();

        /* Process deferred navigation (safe: outside LVGL event context) */
        gui_nav_process_deferred();

        /* Delay until next LVGL tick needed.
           When screen is asleep, poll at 100ms to save CPU —
           wake-on-encoder latency is imperceptible. */
        if (screen_awake) {
            uint32_t delay_ms = (time_till_next < GUI_TICK_PERIOD_MS) ? time_till_next : GUI_TICK_PERIOD_MS;
            if (delay_ms < GUI_TICK_PERIOD_MS) delay_ms = GUI_TICK_PERIOD_MS;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
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
    gui_nav_register(SCR_HOME, scr_home_create);
    gui_nav_register(SCR_SIGGEN_MENU, scr_siggen_create);
#ifdef DSP_ENABLED
    gui_nav_register(SCR_DSP_MENU, scr_dsp_create);
    gui_nav_register(SCR_PEQ_MENU, scr_peq_create);
    gui_nav_register(SCR_PEQ_BAND_EDIT, scr_peq_band_create);
#endif
}

void gui_init(void) {
    LOG_I("[GUI] Initializing...");

    /* Initialize backlight PWM */
    ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_RESOLUTION);
    ledcAttachPin(TFT_BL_PIN, BL_PWM_CHANNEL);
    set_backlight(0);  /* Keep backlight OFF until content is ready */

    /* Initialize TFT via LovyanGFX */
    tft.init();
    tft.initDMA();
    tft.setRotation(1); /* Landscape: 160x128 */
    tft.startWrite();
    tft.fillScreen(0x0000); /* Black */
    LOG_I("[GUI] TFT initialized (LovyanGFX %dx%d)", tft.width(), tft.height());

    /* Initialize LVGL */
    lv_init();
    lv_tick_set_cb(lv_millis_cb);
    LOG_I("[GUI] LVGL initialized");

    /* Create LVGL display with RGB565_SWAPPED format.
       This matches the official LVGL+LovyanGFX driver: LVGL renders directly
       in big-endian (SPI wire) byte order, eliminating the need for a manual
       byte swap in the flush callback.  RGB565_SWAPPED is still 2 bytes/pixel. */
    lv_display_t *disp = lv_display_create(DISPLAY_HEIGHT, DISPLAY_WIDTH); /* landscape: 160x128 */
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(disp, disp_flush_cb);

    /* Allocate LVGL draw buffer — RGB565 = 2 bytes per pixel.
       Use partial-render mode: smaller DMA buffer, multiple flushes per frame. */
    size_t bpp = 2;
    size_t buf_bytes = DISP_BUF_SIZE * bpp;
    if (!draw_buf1) {
        draw_buf1 = (lv_color_t *)heap_caps_aligned_alloc(
            LV_DRAW_BUF_ALIGN, buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    }
    if (!draw_buf1) {
        draw_buf1 = (lv_color_t *)heap_caps_aligned_alloc(
            LV_DRAW_BUF_ALIGN, buf_bytes, MALLOC_CAP_DEFAULT);
    }
    lv_display_set_buffers(disp, draw_buf1, NULL, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    LOG_I("[GUI] Draw buffer: PARTIAL mode, %u bytes, format=RGB565_SWAPPED", buf_bytes);

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

    /* Start FreeRTOS GUI task on Core 1
     * NOTE: Task stacks MUST be in internal SRAM on ESP32 — PSRAM fails
     * xPortcheckValidStackMem() assertion in FreeRTOS port. */
    xTaskCreatePinnedToCore(
        gui_task,
        "gui_task",
        GUI_TASK_STACK_SIZE,
        NULL,
        GUI_TASK_PRIORITY,
        &gui_task_handle,
        GUI_TASK_CORE
    );

    LOG_I("[GUI] Initialization complete");
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

void gui_set_brightness(uint8_t brightness) {
    if (screen_awake) {
        set_backlight(brightness);
        last_applied_brightness = brightness;
    }
}

#endif /* GUI_ENABLED */
