/* SPDX-License-Identifier: MIT */
#include "opening.h"
#include "satellite_indicator.h"

#define DURATION_MS 4000U
#define ORBIT_MS 2714U
#define WING_PERIOD_MS 2800U
#define TRANSITION_MS 3500U

static lv_obj_t *overlay, *content, *ship, *logo_top, *logo_bottom;
static lv_timer_t *timer;
static uint32_t started;
static int32_t ship_width;
static int ship_angle;
static lv_opa_t ship_opacity, background_opacity;

/* Q8 samples of x^1.6, matching the mock's depth-to-size curve. */
static const uint16_t depth_scale[] = {
    0, 3, 9, 18, 28, 40, 53, 68, 84, 102, 121, 141, 162, 184, 207, 231, 256
};

static uint32_t smooth(uint32_t now, uint32_t begin, uint32_t end)
{
    if (now <= begin) return 0;
    if (now >= end) return 256;
    uint32_t x = (now - begin) * 256 / (end - begin);
    return x * x * (768 - 2 * x) / 65536;
}

static void draw_ship(lv_event_t *event)
{
    lv_area_t area;
    lv_obj_get_coords(ship, &area);
    sat_indicator_draw(lv_event_get_layer(event), (area.x1 + area.x2 + 1) / 2,
                        (area.y1 + area.y2 + 1) / 2, ship_width, ship_angle,
                        lv_color_hex(0x68bdd5), ship_opacity);
}

void sat_opening_cancel(void)
{
    if (!overlay) return;
    lv_timer_delete(timer);
    timer = NULL;
    lv_obj_remove_flag(content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_delete(overlay);
    overlay = NULL;
}

static void frame(lv_timer_t *unused)
{
    (void)unused;
    uint32_t elapsed = lv_tick_get() - started; /* Tick wrap is intentional. */
    if (elapsed >= DURATION_MS) {
        sat_opening_cancel();
        return;
    }

    uint32_t orbit = elapsed < ORBIT_MS ? elapsed : ORBIT_MS;
    int angle = orbit * 360 / ORBIT_MS;
    int32_t depth = (32768 - lv_trigo_cos(angle)) / 256; /* 0..256 */
    int index = depth / 16;
    int32_t size = depth_scale[index];
    if (index < 16)
        size += (depth_scale[index + 1] - size) * (depth % 16) / 16;
    ship_width = 7 + 150 * size / 256; /* 56px silhouette, max 2.8x. */
    int32_t cx = 120 - 100 * lv_trigo_sin(angle) / 32768 + 20 * (256 - depth) / 256;
    int32_t cy = 120 - 20 * (256 - depth) / 256;
    /* Bound just the moving drawing, so each frame need not refresh 240x240. */
    int32_t rx = (ship_width * 35 + 55) / 56 + 2;
    int32_t ry = (ship_width * 29 + 55) / 56 + 2;
    lv_obj_set_pos(ship, cx - rx, cy - ry);
    lv_obj_set_size(ship, 2 * rx, 2 * ry);
    ship_angle = (388 - (int)(elapsed * 360 / WING_PERIOD_MS)) % 360;
    ship_opacity = smooth(elapsed, 0, 229) *
                   (256 - smooth(elapsed, 2400, ORBIT_MS)) * 255 / 65536;
    lv_obj_invalidate(ship);

    /* Logo overlaps the receding satellite, then the live UI fades in below it. */
    lv_opa_t logo = smooth(elapsed, 1714, 2429) *
                    (256 - smooth(elapsed, 3429, 3886)) * 255 / 65536;
    lv_obj_set_style_text_opa(logo_top, logo, 0);
    lv_obj_set_style_text_opa(logo_bottom, logo, 0);
    if (elapsed >= TRANSITION_MS) {
        lv_obj_remove_flag(content, LV_OBJ_FLAG_HIDDEN);
        lv_opa_t opacity = (256 - smooth(elapsed, TRANSITION_MS, DURATION_MS)) * 255 / 256;
        if (opacity != background_opacity) {
            lv_obj_set_style_bg_opa(overlay, opacity, 0);
            background_opacity = opacity;
        }
    }
}

static lv_obj_t *logo_label(const char *text, int y, const lv_font_t *font,
                            uint32_t color, int spacing)
{
    lv_obj_t *obj = lv_label_create(overlay);
    lv_obj_set_pos(obj, 12, y);
    lv_obj_set_width(obj, 216);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_text_letter_space(obj, spacing, 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(obj, LV_OPA_TRANSP, 0);
    return obj;
}

void sat_opening_start(lv_obj_t *screen, lv_obj_t *live_content)
{
    sat_opening_cancel();
    content = live_content;
    lv_obj_add_flag(content, LV_OBJ_FLAG_HIDDEN);
    overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 240, 240);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x0d1820), 0);
    background_opacity = LV_OPA_COVER;
    lv_obj_set_style_bg_opa(overlay, background_opacity, 0);
    ship = lv_obj_create(overlay);
    lv_obj_remove_style_all(ship);
    lv_obj_remove_flag(ship, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ship, draw_ship, LV_EVENT_DRAW_MAIN, NULL);
    logo_top = logo_label("ZMK", 91, &lv_font_montserrat_14, 0x9aafbb, 4);
    logo_bottom = logo_label("SATELLITE", 111, &lv_font_montserrat_28, 0xe8eff3, 3);
    started = lv_tick_get();
    /* Only the one-shot opening uses 10 fps; the normal indicator stays at 4 fps. */
    timer = lv_timer_create(frame, 100, NULL);
    frame(NULL);
}
