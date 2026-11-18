/* SPDX-License-Identifier: MIT */
#include "satellite_indicator.h"
#include <draw/lv_draw_rect.h>
#include <draw/lv_draw_triangle.h>
#include <stdint.h>
#include <string.h>

/* Original geometric silhouette: 54px wide, 15 degrees clockwise, 6s/rev.
 * Project only this small drawing; no frame buffers, image files or float math.
 */
static lv_obj_t *icon;
static lv_timer_t *timer;
static uint32_t phase_ms, previous_tick;
static bool running;
/* UI-thread scratch only; no full-frame buffer or per-frame allocation. */
static uint8_t coverage[240];

struct pose {
    int32_t cx, cy, width;
    int angle;
    lv_color_t color;
    lv_opa_t opacity;
};

enum part {LEFT_ARM, LEFT_PANEL, BODY, ANTENNA, ANTENNA_TIP, RIGHT_ARM, RIGHT_PANEL};
static const struct {
    int x1, y1, x2, y2;
    bool rotating;
} parts[] = {
    {14, 19, 28, 21, true}, {0, 13, 18, 27, true},
    {22, 11, 34, 29, false}, {27, 4, 29, 11, false}, {26, 2, 30, 6, false},
    {28, 19, 42, 21, true}, {38, 13, 56, 27, true},
};

static int32_t round_q8(int32_t value)
{
    return value < 0 ? -((-value + 128) / 256) : (value + 128) / 256;
}

static lv_point_precise_t project(int x, int y, int32_t z, bool rotating,
                                   const struct pose *pose)
{
    int32_t px = (x - 28) * 256;
    int32_t py = (y - 20) * 256;
    if (rotating) {
        z = -px * lv_trigo_sin(pose->angle) / 32768;
        px = px * lv_trigo_cos(pose->angle) / 32768;
    }
    /* Camera is level with the shared body/rotor centre: no skewed orbit. */
    px = px * 25600 / (25600 - z);
    py = py * 25600 / (25600 - z);
    int32_t rx = (px * 31651 - py * 8481) / 32768; /* cos/sin(15deg) */
    int32_t ry = (px * 8481 + py * 31651) / 32768;
    return (lv_point_precise_t){
        .x = pose->cx + round_q8(rx * pose->width / 56),
        .y = pose->cy + round_q8(ry * pose->width / 56),
    };
}

void sat_indicator_bounds(lv_area_t *bounds, int32_t width, int angle)
{
    const struct pose pose = {.width = width, .angle = angle};
    *bounds = (lv_area_t){INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN};
    /* Share the exact geometry/projection with the renderer, including wings. */
    for (unsigned i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i) {
        for (unsigned corner = 0; corner < 4; ++corner) {
            int x = corner & 1 ? parts[i].x2 : parts[i].x1;
            int y = corner & 2 ? parts[i].y2 : parts[i].y1;
            lv_point_precise_t p = project(x, y, 3 * 256, parts[i].rotating, &pose);
            if (p.x < bounds->x1) bounds->x1 = p.x;
            if (p.x > bounds->x2) bounds->x2 = p.x;
            if (p.y < bounds->y1) bounds->y1 = p.y;
            if (p.y > bounds->y2) bounds->y2 = p.y;
        }
    }
    /* A single pixel covers the edge antialiasing; avoid scale-based padding. */
    --bounds->x1; --bounds->y1;
    ++bounds->x2; ++bounds->y2;
}

static void quad_spans(lv_layer_t *layer, const struct pose *pose,
                  int x1, int y1, int x2, int y2, bool rotating)
{
    lv_point_precise_t p[4] = {
        project(x1, y1, 3 * 256, rotating, pose),
        project(x2, y1, 3 * 256, rotating, pose),
        project(x2, y2, 3 * 256, rotating, pose),
        project(x1, y2, 3 * 256, rotating, pose),
    };
    int32_t min_x = 239, min_y = 239, max_x = 0, max_y = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (p[i].x < min_x) min_x = p[i].x;
        if (p[i].x > max_x) max_x = p[i].x;
        if (p[i].y < min_y) min_y = p[i].y;
        if (p[i].y > max_y) max_y = p[i].y;
    }
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > 239) max_x = 239;
    if (max_y > 239) max_y = 239;
    if (min_x > max_x || min_y > max_y) return;

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = pose->color;
    /* Fill the convex quad as one surface. A 4x4 sample grid smooths only
     * its outside edges: no triangle seam and no thin-triangle mask artefact. */
    for (int32_t y = min_y; y <= max_y; ++y) {
        memset(coverage + min_x, 0, max_x - min_x + 1);
        for (unsigned sy = 0; sy < 4; ++sy) {
            int32_t sample_y = y * 256 + 32 + sy * 64;
            int32_t lo = INT32_MAX, hi = INT32_MIN;
            unsigned crossings = 0;
            for (unsigned i = 0; i < 4; ++i) {
                lv_point_precise_t a = p[i], b = p[(i + 1) % 4];
                if (a.y > b.y) { lv_point_precise_t swap = a; a = b; b = swap; }
                /* Half-open edges avoid double-counting a shared vertex. */
                if (sample_y < a.y * 256 || sample_y >= b.y * 256) continue;
                int32_t x = a.x * 256 + (sample_y - a.y * 256) * (b.x - a.x) / (b.y - a.y);
                if (x < lo) lo = x;
                if (x > hi) hi = x;
                ++crossings;
            }
            if (crossings < 2) continue;
            int32_t first = lo / 256, last = hi / 256;
            if (first < min_x) first = min_x;
            if (last > max_x) last = max_x;
            for (int32_t x = first; x <= last; ++x) {
                /* Most pixels are fully inside: sample only the two edges. */
                if (x * 256 + 32 >= lo && x * 256 + 224 < hi) {
                    coverage[x] += 4;
                    continue;
                }
                for (unsigned sx = 0; sx < 4; ++sx) {
                    int32_t sample_x = x * 256 + 32 + sx * 64;
                    if (sample_x >= lo && sample_x < hi) ++coverage[x];
                }
            }
        }
        /* Coalesce equal-opacity pixels into short, ordinary fill rectangles. */
        for (int32_t x = min_x; x <= max_x;) {
            uint8_t count = coverage[x];
            int32_t start = x++;
            while (x <= max_x && coverage[x] == count) ++x;
            if (!count) continue;
            lv_area_t span = {start, y, x - 1, y};
            dsc.bg_opa = (count * pose->opacity + 8) / 16;
            lv_draw_rect(layer, &dsc, &span);
        }
    }
}

/* Large faces retain the inexpensive triangle rendering. Only the thin struts
 * need the scanline fill to avoid extra horizontal lines at shallow angles. */
static void quad(lv_layer_t *layer, const struct pose *pose,
                  int x1, int y1, int x2, int y2, bool rotating)
{
    lv_point_precise_t p[4] = {
        project(x1, y1, 3 * 256, rotating, pose),
        project(x2, y1, 3 * 256, rotating, pose),
        project(x2, y2, 3 * 256, rotating, pose),
        project(x1, y2, 3 * 256, rotating, pose),
    };
    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.bg_color = pose->color;
    dsc.bg_opa = pose->opacity;
    for (int i = 1; i < 3; ++i) {
        dsc.p[0] = p[0]; dsc.p[1] = p[i]; dsc.p[2] = p[i + 1];
        int32_t cross = (p[i].x - p[0].x) * (p[i + 1].y - p[0].y) -
                        (p[i].y - p[0].y) * (p[i + 1].x - p[0].x);
        if (cross) lv_draw_triangle(layer, &dsc);
    }
}

static void draw_part(lv_layer_t *layer, const struct pose *pose, enum part part)
{
    if (part == LEFT_ARM || part == RIGHT_ARM)
        quad_spans(layer, pose, parts[part].x1, parts[part].y1,
                   parts[part].x2, parts[part].y2, parts[part].rotating);
    else
        quad(layer, pose, parts[part].x1, parts[part].y1,
             parts[part].x2, parts[part].y2, parts[part].rotating);
}

static void wing(lv_layer_t *layer, const struct pose *pose, bool left)
{
    draw_part(layer, pose, left ? LEFT_ARM : RIGHT_ARM);
    draw_part(layer, pose, left ? LEFT_PANEL : RIGHT_PANEL);
}

void sat_indicator_draw(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t width,
                        int angle, lv_color_t color, lv_opa_t opacity)
{
    if (!opacity) return;
    const struct pose pose = {cx, cy, width, angle, color, opacity};
    /* Back wing, fixed body, front wing. The panel faces stay parallel to
     * the body, while both wings rotate about the body's long axis. */
    bool left_front = lv_trigo_sin(angle) > 0;
    wing(layer, &pose, !left_front);
    draw_part(layer, &pose, BODY);
    draw_part(layer, &pose, ANTENNA);
    draw_part(layer, &pose, ANTENNA_TIP);
    wing(layer, &pose, left_front);
}

static void draw(lv_event_t *event)
{
    lv_area_t area;
    lv_obj_get_coords(icon, &area);
    int angle = (388 - (int)(phase_ms * 360 / 6000)) % 360;
    sat_indicator_draw(lv_event_get_layer(event), area.x1 + 30, area.y1 + 22,
                        54, angle, lv_color_hex(running ? 0x68bdd5 : 0x738795),
                        LV_OPA_COVER);
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
