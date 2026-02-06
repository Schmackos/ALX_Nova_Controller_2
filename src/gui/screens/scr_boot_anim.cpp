#ifdef GUI_ENABLED

#include "scr_boot_anim.h"
#include "../gui_config.h"
#include "../gui_theme.h"
#include "../../app_state.h"
#include "../../buzzer_handler.h"
#include "../../debug_serial.h"
#include <Arduino.h>
#include <lvgl.h>

/* Display dimensions in landscape mode */
#define SCR_W DISPLAY_HEIGHT /* 160 */
#define SCR_H DISPLAY_WIDTH  /* 128 */

/* Animation total duration (ms) */
#define ANIM_DURATION_MS 2500

/* Volatile flag set by the last animation's ready callback */
static volatile bool anim_finished = false;

/* ===== Helper: animation-done callback ===== */
static void anim_done_cb(lv_anim_t *a) {
    (void)a;
    anim_finished = true;
}

/* ===== Helper: create centered brand label ===== */
static lv_obj_t *create_brand_label(lv_obj_t *parent, lv_coord_t y) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "ALX Audio");
    lv_obj_set_style_text_color(lbl, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_opa(lbl, LV_OPA_TRANSP, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, y);
    return lbl;
}

/* ===== Helper: fade in a label ===== */
static void fade_in_label(lv_obj_t *lbl, uint32_t delay_ms, uint32_t dur_ms,
                          bool is_last) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, lbl);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a, dur_ms);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
        lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
    });
    if (is_last) {
        lv_anim_set_completed_cb(&a, anim_done_cb);
    }
    lv_anim_start(&a);
}

/* ================================================================
   Animation 0: Sound Wave Pulse
   4 concentric arcs expand outward, fading as they grow.
   "ALX Audio" fades in below.
   ================================================================ */
static void anim0_sound_wave(lv_obj_t *scr) {
    lv_coord_t cx = SCR_W / 2;
    lv_coord_t cy = SCR_H / 2 - 14;

    for (int i = 0; i < 4; i++) {
        lv_obj_t *arc = lv_arc_create(scr);
        lv_arc_set_rotation(arc, 225);
        lv_arc_set_bg_angles(arc, 0, 90);
        lv_arc_set_value(arc, 100);
        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(arc, 2, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, COLOR_PRIMARY, LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(arc, 0, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_opa(arc, LV_OPA_TRANSP, 0);

        int start_size = 10;
        int end_size = 30 + i * 16;
        lv_obj_set_size(arc, start_size, start_size);
        lv_obj_set_pos(arc, cx - start_size / 2, cy - start_size / 2);

        uint32_t delay = i * 200;
        uint32_t dur = 800;

        /* Fade in */
        lv_anim_t a_opa;
        lv_anim_init(&a_opa);
        lv_anim_set_var(&a_opa, arc);
        lv_anim_set_values(&a_opa, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_duration(&a_opa, dur / 2);
        lv_anim_set_delay(&a_opa, delay);
        lv_anim_set_exec_cb(&a_opa, [](void *obj, int32_t v) {
            lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
        });
        lv_anim_start(&a_opa);

        /* Fade out in second half */
        lv_anim_t a_fade;
        lv_anim_init(&a_fade);
        lv_anim_set_var(&a_fade, arc);
        lv_anim_set_values(&a_fade, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_duration(&a_fade, dur / 2);
        lv_anim_set_delay(&a_fade, delay + dur / 2);
        lv_anim_set_exec_cb(&a_fade, [](void *obj, int32_t v) {
            lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
        });
        lv_anim_start(&a_fade);

        /* Expand size */
        lv_anim_t a_w;
        lv_anim_init(&a_w);
        lv_anim_set_var(&a_w, arc);
        lv_anim_set_values(&a_w, start_size, end_size);
        lv_anim_set_duration(&a_w, dur);
        lv_anim_set_delay(&a_w, delay);
        lv_anim_set_path_cb(&a_w, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&a_w, [](void *obj, int32_t v) {
            lv_obj_set_size((lv_obj_t *)obj, v, v);
            /* Re-center */
            lv_coord_t cx = SCR_W / 2;
            lv_coord_t cy = SCR_H / 2 - 14;
            lv_obj_set_pos((lv_obj_t *)obj, cx - v / 2, cy - v / 2);
        });
        lv_anim_start(&a_w);
    }

    /* Brand label fades in after arcs */
    lv_obj_t *lbl = create_brand_label(scr, 30);
    fade_in_label(lbl, 1200, 800, true);
}

/* ================================================================
   Animation 1: Speaker Ripple
   Speaker cone arc with 3 ripple arcs expanding rightward.
   "ALX Audio" fades in.
   ================================================================ */
static void anim1_speaker_ripple(lv_obj_t *scr) {
    /* Speaker cone (left arc) */
    lv_obj_t *cone = lv_arc_create(scr);
    lv_arc_set_rotation(cone, 300);
    lv_arc_set_bg_angles(cone, 0, 120);
    lv_arc_set_value(cone, 100);
    lv_obj_remove_style(cone, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(cone, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(cone, COLOR_TEXT_PRI, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(cone, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(cone, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_size(cone, 30, 30);
    lv_obj_align(cone, LV_ALIGN_CENTER, -40, -10);
    lv_obj_set_style_opa(cone, LV_OPA_TRANSP, 0);

    /* Fade in cone */
    lv_anim_t ac;
    lv_anim_init(&ac);
    lv_anim_set_var(&ac, cone);
    lv_anim_set_values(&ac, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&ac, 400);
    lv_anim_set_exec_cb(&ac, [](void *obj, int32_t v) {
        lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
    });
    lv_anim_start(&ac);

    /* 3 ripple arcs */
    for (int i = 0; i < 3; i++) {
        lv_obj_t *ripple = lv_arc_create(scr);
        lv_arc_set_rotation(ripple, 315);
        lv_arc_set_bg_angles(ripple, 0, 90);
        lv_arc_set_value(ripple, 100);
        lv_obj_remove_style(ripple, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(ripple, 2, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(ripple, COLOR_PRIMARY, LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(ripple, 0, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(ripple, LV_OPA_TRANSP, LV_PART_MAIN);
        int sz = 40 + i * 18;
        lv_obj_set_size(ripple, sz, sz);
        lv_obj_align(ripple, LV_ALIGN_CENTER, -30 + i * 4, -10);
        lv_obj_set_style_opa(ripple, LV_OPA_TRANSP, 0);

        uint32_t delay = 300 + i * 250;

        /* Pulse: fade in then out, repeating */
        lv_anim_t ar;
        lv_anim_init(&ar);
        lv_anim_set_var(&ar, ripple);
        lv_anim_set_values(&ar, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_duration(&ar, 500);
        lv_anim_set_delay(&ar, delay);
        lv_anim_set_playback_duration(&ar, 500);
        lv_anim_set_repeat_count(&ar, 1);
        lv_anim_set_exec_cb(&ar, [](void *obj, int32_t v) {
            lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
        });
        lv_anim_start(&ar);
    }

    /* Brand label */
    lv_obj_t *lbl = create_brand_label(scr, 30);
    fade_in_label(lbl, 1200, 800, true);
}

/* ================================================================
   Animation 2: Waveform Draw
   20 line segments appear left-to-right tracing a sine wave.
   "ALX Audio" above.
   ================================================================ */
static void anim2_waveform(lv_obj_t *scr) {
    /* Brand label at top — fades in early */
    lv_obj_t *lbl = create_brand_label(scr, -25);
    fade_in_label(lbl, 0, 600, false);

    /* Waveform: 20 small vertical bars simulating sine wave */
    static const int NUM_BARS = 20;
    lv_coord_t bar_w = 4;
    lv_coord_t total_w = NUM_BARS * (bar_w + 2);
    lv_coord_t start_x = (SCR_W - total_w) / 2;
    lv_coord_t center_y = SCR_H / 2 + 10;

    for (int i = 0; i < NUM_BARS; i++) {
        /* Calculate sine height */
        float angle = (float)i / NUM_BARS * 3.14159f * 2.0f;
        int h = (int)(sinf(angle) * 20);
        int bar_h = (h < 0 ? -h : h) + 4; /* minimum height of 4 */

        lv_obj_t *bar = lv_obj_create(scr);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, bar_w, bar_h);
        lv_coord_t y = center_y - bar_h / 2 + (h > 0 ? -h / 2 : h / 2);
        lv_obj_set_pos(bar, start_x + i * (bar_w + 2), y);
        lv_obj_set_style_bg_color(bar, COLOR_PRIMARY, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar, 1, 0);
        lv_obj_set_style_opa(bar, LV_OPA_TRANSP, 0);

        uint32_t delay = 400 + i * 80;

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, bar);
        lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_duration(&a, 150);
        lv_anim_set_delay(&a, delay);
        lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
            lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
        });
        if (i == NUM_BARS - 1) {
            lv_anim_set_completed_cb(&a, anim_done_cb);
        }
        lv_anim_start(&a);
    }
}

/* ================================================================
   Animation 3: Beat Bounce
   Pulsing orange circle + "ALX Audio" drops in from top with
   overshoot bounce.
   ================================================================ */
static void anim3_beat_bounce(lv_obj_t *scr) {
    /* Orange circle */
    lv_obj_t *circle = lv_obj_create(scr);
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, 36, 36);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    lv_obj_align(circle, LV_ALIGN_CENTER, 0, -10);

    /* Pulse circle size */
    lv_anim_t ap;
    lv_anim_init(&ap);
    lv_anim_set_var(&ap, circle);
    lv_anim_set_values(&ap, 36, 44);
    lv_anim_set_duration(&ap, 400);
    lv_anim_set_playback_duration(&ap, 400);
    lv_anim_set_repeat_count(&ap, 3);
    lv_anim_set_path_cb(&ap, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&ap, [](void *obj, int32_t v) {
        lv_obj_set_size((lv_obj_t *)obj, v, v);
        lv_obj_align((lv_obj_t *)obj, LV_ALIGN_CENTER, 0, -10);
    });
    lv_anim_start(&ap);

    /* Brand label drops from top */
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "ALX Audio");
    lv_obj_set_style_text_color(lbl, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -80); /* Start above screen */

    lv_anim_t ab;
    lv_anim_init(&ab);
    lv_anim_set_var(&ab, lbl);
    lv_anim_set_values(&ab, -80, 30);
    lv_anim_set_duration(&ab, 800);
    lv_anim_set_delay(&ab, 600);
    lv_anim_set_path_cb(&ab, lv_anim_path_overshoot);
    lv_anim_set_exec_cb(&ab, [](void *obj, int32_t v) {
        lv_obj_align((lv_obj_t *)obj, LV_ALIGN_CENTER, 0, v);
    });
    lv_anim_set_completed_cb(&ab, anim_done_cb);
    lv_anim_start(&ab);
}

/* ================================================================
   Animation 4: Freq Bars Reveal
   8 vertical bars grow upward with staggered overshoot timing.
   "ALX Audio" above.
   ================================================================ */
static void anim4_freq_bars(lv_obj_t *scr) {
    /* Brand label */
    lv_obj_t *lbl = create_brand_label(scr, -35);
    fade_in_label(lbl, 0, 500, false);

    static const int NUM_BARS = 8;
    static const int bar_heights[] = {18, 30, 24, 40, 36, 22, 34, 28};
    lv_coord_t bar_w = 10;
    lv_coord_t gap = 4;
    lv_coord_t total_w = NUM_BARS * bar_w + (NUM_BARS - 1) * gap;
    lv_coord_t start_x = (SCR_W - total_w) / 2;
    lv_coord_t base_y = SCR_H / 2 + 30;

    for (int i = 0; i < NUM_BARS; i++) {
        lv_obj_t *bar = lv_obj_create(scr);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, bar_w, 0); /* Start with 0 height */
        lv_obj_set_pos(bar, start_x + i * (bar_w + gap), base_y);
        lv_obj_set_style_bg_color(bar, COLOR_PRIMARY, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar, 2, 0);

        int target_h = bar_heights[i];
        uint32_t delay = 400 + i * 150;

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, bar);
        lv_anim_set_values(&a, 0, target_h);
        lv_anim_set_duration(&a, 500);
        lv_anim_set_delay(&a, delay);
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot);

        /* Capture base_y and bar position for the exec callback */
        struct BarCtx {
            lv_coord_t x;
            lv_coord_t base;
            lv_coord_t w;
        };
        static BarCtx ctx[8];
        ctx[i] = {(lv_coord_t)(start_x + i * (bar_w + gap)), base_y, bar_w};

        lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
            lv_obj_t *o = (lv_obj_t *)obj;
            if (v < 0) v = 0;
            lv_obj_set_height(o, v);
            /* Grow upward from base */
            lv_coord_t cur_y = lv_obj_get_y(o);
            lv_coord_t cur_h = lv_obj_get_height(o);
            /* Find matching ctx by x position */
            lv_coord_t ox = lv_obj_get_x(o);
            for (int j = 0; j < 8; j++) {
                if (ctx[j].x == ox) {
                    lv_obj_set_pos(o, ox, ctx[j].base - v);
                    break;
                }
            }
        });

        if (i == NUM_BARS - 1) {
            lv_anim_set_completed_cb(&a, anim_done_cb);
        }
        lv_anim_start(&a);
    }
}

/* ================================================================
   Animation 5: Soundline Heartbeat
   Flat line fades in, then heartbeat spike pattern appears and
   pulses. "ALX Audio" below.
   ================================================================ */
static void anim5_heartbeat(lv_obj_t *scr) {
    lv_coord_t line_y = SCR_H / 2 - 10;

    /* Flat baseline */
    lv_obj_t *baseline = lv_obj_create(scr);
    lv_obj_remove_style_all(baseline);
    lv_obj_set_size(baseline, SCR_W - 30, 2);
    lv_obj_set_pos(baseline, 15, line_y);
    lv_obj_set_style_bg_color(baseline, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_bg_opa(baseline, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(baseline, LV_OPA_TRANSP, 0);

    /* Fade in baseline */
    lv_anim_t ab;
    lv_anim_init(&ab);
    lv_anim_set_var(&ab, baseline);
    lv_anim_set_values(&ab, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&ab, 500);
    lv_anim_set_exec_cb(&ab, [](void *obj, int32_t v) {
        lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
    });
    lv_anim_start(&ab);

    /* Heartbeat spike: tall orange bar in center */
    lv_obj_t *spike = lv_obj_create(scr);
    lv_obj_remove_style_all(spike);
    lv_obj_set_size(spike, 4, 0);
    lv_obj_set_pos(spike, SCR_W / 2 - 2, line_y);
    lv_obj_set_style_bg_color(spike, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(spike, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(spike, 1, 0);

    /* Secondary spike */
    lv_obj_t *spike2 = lv_obj_create(scr);
    lv_obj_remove_style_all(spike2);
    lv_obj_set_size(spike2, 4, 0);
    lv_obj_set_pos(spike2, SCR_W / 2 + 10, line_y);
    lv_obj_set_style_bg_color(spike2, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(spike2, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(spike2, 1, 0);

    /* Animate spike height — pulse up and down */
    lv_anim_t as;
    lv_anim_init(&as);
    lv_anim_set_var(&as, spike);
    lv_anim_set_values(&as, 0, 40);
    lv_anim_set_duration(&as, 200);
    lv_anim_set_delay(&as, 700);
    lv_anim_set_playback_duration(&as, 200);
    lv_anim_set_repeat_count(&as, 2);
    lv_anim_set_repeat_delay(&as, 400);
    lv_anim_set_path_cb(&as, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&as, [](void *obj, int32_t v) {
        lv_obj_set_height((lv_obj_t *)obj, v);
        lv_coord_t ly = SCR_H / 2 - 10;
        lv_obj_set_y((lv_obj_t *)obj, ly - v);
    });
    lv_anim_start(&as);

    /* Animate spike2 (smaller, slightly delayed) */
    lv_anim_t as2;
    lv_anim_init(&as2);
    lv_anim_set_var(&as2, spike2);
    lv_anim_set_values(&as2, 0, 22);
    lv_anim_set_duration(&as2, 200);
    lv_anim_set_delay(&as2, 800);
    lv_anim_set_playback_duration(&as2, 200);
    lv_anim_set_repeat_count(&as2, 2);
    lv_anim_set_repeat_delay(&as2, 400);
    lv_anim_set_path_cb(&as2, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&as2, [](void *obj, int32_t v) {
        lv_obj_set_height((lv_obj_t *)obj, v);
        lv_coord_t ly = SCR_H / 2 - 10;
        lv_obj_set_y((lv_obj_t *)obj, ly - v);
    });
    lv_anim_start(&as2);

    /* Brand label below */
    lv_obj_t *lbl = create_brand_label(scr, 30);
    fade_in_label(lbl, 1200, 800, true);
}

/* ================================================================
   Main entry point
   ================================================================ */

/* Dispatch table for animation setup functions */
typedef void (*anim_setup_fn)(lv_obj_t *scr);
static const anim_setup_fn anim_table[] = {
    anim0_sound_wave,
    anim1_speaker_ripple,
    anim2_waveform,
    anim3_beat_bounce,
    anim4_freq_bars,
    anim5_heartbeat,
};
static const int ANIM_COUNT = sizeof(anim_table) / sizeof(anim_table[0]);

void boot_anim_play(void) {
    AppState &st = AppState::getInstance();
    if (!st.bootAnimEnabled) {
        LOG_I("[GUI] Boot animation disabled, skipping");
        return;
    }

    int style = st.bootAnimStyle;
    if (style < 0 || style >= ANIM_COUNT) style = 0;

    LOG_I("[GUI] Playing boot animation %d", style);

    /* Create a temporary screen */
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_screen_load(scr);

    /* Reset flag */
    anim_finished = false;

    /* Run selected animation setup */
    anim_table[style](scr);

    /* Play startup melody alongside the animation */
    buzzer_play(BUZZ_STARTUP);

    /* Blocking loop: pump LVGL until animation signals done or timeout */
    unsigned long start = millis();
    while (!anim_finished && (millis() - start < ANIM_DURATION_MS + 500)) {
        lv_timer_handler();
        buzzer_update();
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    /* Brief hold so the final frame is visible */
    unsigned long elapsed = millis() - start;
    if (elapsed < ANIM_DURATION_MS) {
        vTaskDelay(pdMS_TO_TICKS(ANIM_DURATION_MS - elapsed));
    }

    /* Free child objects (arcs, labels, etc.) but keep the screen alive.
       Deleting the active screen leaves LVGL with no valid screen and
       prevents gui_nav_push from loading the desktop afterwards. */
    lv_obj_clean(scr);

    LOG_I("[GUI] Boot animation complete");
}

#endif /* GUI_ENABLED */
