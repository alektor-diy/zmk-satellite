/* SPDX-License-Identifier: MIT */
#include "ui.h"
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <lvgl_zephyr.h>

static lv_obj_t *status, *layer, *battery, *host, *detail;
static lv_obj_t *label(const char *text, int y)
{
    lv_obj_t *obj = lv_label_create(lv_screen_active());
    lv_obj_set_width(obj, 216);
    lv_obj_set_pos(obj, 12, y);
    lv_obj_set_style_text_color(obj, lv_color_hex(0xe6edf3), 0);
    lv_label_set_text(obj, text);
    return obj;
}
int sat_ui_init(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(dev)) return -1;
    int ret = lvgl_init();
    if (ret) return ret;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101923), 0);
    lv_obj_set_style_border_color(screen, lv_color_hex(0x68bdd5), 0);
    lv_obj_set_style_border_width(screen, 1, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    label("ZMK SATELLITE", 12);
    status = label("Scanning...", 38);
    layer = label("Layer --", 68);
    lv_obj_set_style_text_font(layer, &lv_font_montserrat_24, 0);
    battery = label("L --    R --", 110);
    host = label("BT --", 142);
    detail = label("Waiting for status ADV", 174);
    lv_obj_t *bar = lv_obj_create(screen);
    lv_obj_set_pos(bar, 12, 218); lv_obj_set_size(bar, 216, 8);
    lv_obj_set_style_pad_all(bar, 0, 0); lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x40bb90), 0);
    if (IS_ENABLED(CONFIG_SATELLITE_DISPLAY_TEST)) {
        const uint32_t colors[] = {0xff0000, 0x00ff00, 0x0000ff};
        for (unsigned i = 0; i < 3; ++i) {
            lv_obj_t *tile = lv_obj_create(bar);
            lv_obj_set_pos(tile, i*72, 0); lv_obj_set_size(tile, 72, 8);
            lv_obj_set_style_border_width(tile, 0, 0);
            lv_obj_set_style_radius(tile, 0, 0);
            lv_obj_set_style_bg_color(tile, lv_color_hex(colors[i]), 0);
        }
    }
    lv_timer_handler();
    return display_blanking_off(dev);
}
static void battery_text(char out[8], uint8_t b, bool peripheral)
{
    if (b > 100 || (peripheral && b == 0)) snprintf(out, 8, "--");
    else snprintf(out, 8, "%u%%", b);
}
void sat_ui_update(const struct sat_state *s, int64_t now)
{
    if (!s->seen) return;
    bool online = sat_online(s, now, CONFIG_SATELLITE_TIMEOUT_MS);
    char text[96], l[8], r[8];
    lv_label_set_text(status, IS_ENABLED(CONFIG_SATELLITE_DISPLAY_TEST) ?
        "DISPLAY TEST / SIMULATED" : online ? "Receiving" : "Signal lost / stale");
    lv_obj_set_style_text_color(status, lv_color_hex(online ? 0x65ddae : 0xffb66e), 0);
    snprintf(text, sizeof(text), "%s  [%u]", s->packet.layer_name, s->packet.layer);
    lv_label_set_text(layer, text);
    battery_text(l, s->packet.battery_left, s->packet.role == 1);
    battery_text(r, s->packet.battery_right, true);
    if (s->packet.role == 0) snprintf(text, sizeof(text), "BAT %s", l);
    else snprintf(text, sizeof(text), "L %s       R %s", l, r);
    lv_label_set_text(battery, text);
    snprintf(text, sizeof(text), "BT %u: %s | USB %s", s->packet.profile + 1,
        s->packet.flags & 0x10 ? "linked" : "offline",
        s->packet.flags & 0x08 ? "ready" : "off");
    lv_label_set_text(host, text);
    snprintf(text, sizeof(text), "%02X%02X%02X%02X  %d dBm\nLast ADV: %llds ago%s",
        s->packet.id[0], s->packet.id[1], s->packet.id[2], s->packet.id[3], s->rssi,
        (long long)((now - s->last_seen) / 1000), online ? "" : " (stale)");
    lv_label_set_text(detail, text);
}
void sat_ui_error(const char *message) { lv_label_set_text(status, message); }
void sat_ui_tick(void) { lv_timer_handler(); }
