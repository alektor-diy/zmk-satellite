/* SPDX-License-Identifier: MIT */
#include "satellite_indicator.h"
#include <draw/lv_draw_triangle.h>
#include <stdint.h>

/* Original geometric silhouette: 54px wide, 15 degrees clockwise, 6s/rev.
 * Project only this small drawing; no frame buffers, image files or float math.
 */
static lv_obj_t *icon;
static lv_timer_t *timer;
static uint32_t phase_ms, previous_tick;
static bool running;

static int32_t round_q8(int32_t value)
{
    return value < 0 ? -((-value + 128) / 256) : (value + 128) / 256;
}

static lv_point_precise_t project(int x, int y, int32_t z, bool rotating,
                                   int angle, const lv_area_t *area)
{
    int32_t px = (x - 28) * 256;
    int32_t py = (y - 20) * 256;
    if (rotating) {
        z = -px * lv_trigo_sin(angle) / 32768;
        px = px * lv_trigo_cos(angle) / 32768;
    }
    /* Camera is level with the shared body/rotor centre: no skewed orbit. */
    px = px * 25600 / (25600 - z);
    py = py * 25600 / (25600 - z);
    int32_t rx = (px * 31651 - py * 8481) / 32768; /* cos/sin(15deg) */
    int32_t ry = (px * 8481 + py * 31651) / 32768;
    return (lv_point_precise_t){
        .x = area->x1 + 30 + round_q8(rx * 54 / 56),
        .y = area->y1 + 22 + round_q8(ry * 54 / 56),
    };
}

static void quad(lv_layer_t *layer, const lv_area_t *area, int angle,
                  int x1, int y1, int x2, int y2, bool rotating, lv_color_t color)
{
    lv_point_precise_t p[4] = {
        project(x1, y1, 3 * 256, rotating, angle, area),
        project(x2, y1, 3 * 256, rotating, angle, area),
        project(x2, y2, 3 * 256, rotating, angle, area),
        project(x1, y2, 3 * 256, rotating, angle, area),
    };
    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    for (int i = 1; i < 3; ++i) {
        dsc.p[0] = p[0]; dsc.p[1] = p[i]; dsc.p[2] = p[i + 1];
        int32_t cross = (p[i].x - p[0].x) * (p[i + 1].y - p[0].y) -
                        (p[i].y - p[0].y) * (p[i + 1].x - p[0].x);
        if (cross) lv_draw_triangle(layer, &dsc);
    }
}

static void wing(lv_layer_t *layer, const lv_area_t *area, int angle,
                  bool left, lv_color_t color)
{
    quad(layer, area, angle, left ? 14 : 28, 19, left ? 28 : 42, 21, true, color);
    quad(layer, area, angle, left ? 0 : 38, 13, left ? 18 : 56, 27, true, color);
}

static void draw(lv_event_t *event)
{
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(icon, &area);
    int angle = (388 - (int)(phase_ms * 360 / 6000)) % 360;
    lv_color_t color = lv_color_hex(running ? 0x68bdd5 : 0x738795);
    /* Back wing, fixed body, front wing. The panel faces stay parallel to
     * the body, while both wings rotate about the body's long axis. */
    bool left_front = lv_trigo_sin(angle) > 0;
    wing(layer, &area, angle, !left_front, color);
    quad(layer, &area, angle, 22, 11, 34, 29, false, color);
    quad(layer, &area, angle, 27, 4, 29, 11, false, color);
    quad(layer, &area, angle, 26, 2, 30, 6, false, color);
    wing(layer, &area, angle, left_front, color);
}

static void animate(lv_timer_t *unused)
{
    (void)unused;
    uint32_t now = lv_tick_get();
    phase_ms = (phase_ms + (now - previous_tick) % 6000) % 6000;
    previous_tick = now;
    lv_obj_invalidate(icon);
}

void sat_indicator_init(lv_obj_t *parent)
{
    icon = lv_obj_create(parent);
    lv_obj_remove_style_all(icon);
    /* Padding around the projected geometry prevents edge clipping. */
    lv_obj_set_pos(icon, 176, 16);
    lv_obj_set_size(icon, 60, 44);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(icon, draw, LV_EVENT_DRAW_MAIN, NULL);
    timer = lv_timer_create(animate, 250, NULL); /* 4 fps; prioritize status responsiveness. */
    lv_timer_pause(timer);
}

void sat_indicator_receiving(bool receiving)
{
    if (running == receiving) return;
    running = receiving;
    if (running) {
        previous_tick = lv_tick_get();
        lv_timer_reset(timer);
        lv_timer_resume(timer);
    } else {
        lv_timer_pause(timer); /* Retain the last pose instead of snapping back. */
    }
    lv_obj_invalidate(icon);
}
