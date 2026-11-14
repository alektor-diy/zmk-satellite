/* SPDX-License-Identifier: MIT */
#include "ui.h"
#include <stdio.h>
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <lvgl_zephyr.h>

#define INK 0xe6edf3
#define DIM 0x98abba
#define CYAN 0x68bdd5
#define WARN 0xffb66e
static lv_obj_t *name, *status, *layer_number, *host, *host_box;
static lv_obj_t *battery[2], *fill[2];

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int w,
                       const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, w);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_text(obj, text);
    return obj;
}

static lv_obj_t *rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

int sat_ui_init(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(dev)) return -ENODEV;
    int ret = lvgl_init();
    if (ret) return ret;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101923), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    /* Shared 14px outer margins; layer and battery columns end at x=226. */
    label(screen, "ZMK SATELLITE", 14, 14, 212, &lv_font_montserrat_10, DIM);
    name = label(screen, "Keyboard", 14, 44, 158, &lv_font_montserrat_24, INK);
    lv_obj_t *layer_heading = label(screen, "LAYER", 178, 28, 48,
                                    &lv_font_montserrat_12, DIM);
    lv_obj_set_style_text_align(layer_heading, LV_TEXT_ALIGN_CENTER, 0);
    layer_number = label(screen, "--", 178, 44, 48, &lv_font_montserrat_24, INK);
    lv_obj_set_style_text_align(layer_number, LV_TEXT_ALIGN_CENTER, 0);
    host_box = rect(screen, 14, 193, 55, 30, 0x24313c);
    lv_obj_set_style_radius(host_box, 5, 0);
    host = label(host_box, "--", 0, 0, 55, &lv_font_montserrat_18, DIM);
    lv_obj_set_style_text_align(host, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(host, LV_ALIGN_CENTER, 0, 0);
    status = label(screen, "WAITING FOR\nKEYBOARD", 81, 193, 145,
                   &lv_font_montserrat_12, CYAN);
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(status, host_box, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
    label(screen, "LEFT", 14, 101, 94, &lv_font_montserrat_12, DIM);
    label(screen, "RIGHT", 132, 101, 94, &lv_font_montserrat_12, DIM);
    for (unsigned i = 0; i < 2; ++i) {
        int x = i ? 132 : 14;
        battery[i] = label(screen, "--", x, 121, 96, &lv_font_montserrat_36, INK);
        lv_obj_t *track = rect(screen, x, 164, 94, 4, 0x2b3a46);
        fill[i] = rect(track, 0, 0, 1, 4, CYAN);
        lv_obj_add_flag(fill[i], LV_OBJ_FLAG_HIDDEN);
    }
    rect(screen, 14, 183, 212, 1, 0x2d3c49);
    lv_timer_handler();
    return display_blanking_off(dev);
}

void sat_ui_update(const struct sat_state *s, int64_t now)
{
    const char *title = CONFIG_SATELLITE_KEYBOARD_NAME[0] ?
        CONFIG_SATELLITE_KEYBOARD_NAME : s->keyboard_name[0] ? s->keyboard_name : "Keyboard";
    lv_label_set_text(name, title);
    if (!s->seen) return;
    bool online = sat_online(s, now, CONFIG_SATELLITE_TIMEOUT_MS);
    enum sat_host_status endpoint = sat_host_from_flags(s->packet.flags);
    bool usb = endpoint == SAT_HOST_USB;
    char text[32];
    if (usb) snprintf(text, sizeof(text), "USB");
    else snprintf(text, sizeof(text), "BT%u", s->packet.profile + 1);
    lv_label_set_text(host, text);
    lv_obj_set_style_bg_color(host_box, lv_color_hex(usb ? 0x3d2d1c : 0x183744), 0);
    lv_obj_set_style_text_color(host, lv_color_hex(!online ? DIM : usb ? 0xf3bb76 : CYAN), 0);
    enum sat_bt_status bt_state = sat_bt_from_flags(s->packet.flags);
    const char *connection = endpoint == SAT_HOST_BOTH ? "BT + USB READY" :
        usb ? "USB CONNECTED" :
        bt_state == SAT_BT_CONNECTED ? "CONNECTED" :
        bt_state == SAT_BT_PAIRED ? "PAIRED /\nDISCONNECTED" : "NOT PAIRED";
    lv_label_set_text(status, !online ? "SIGNAL LOST /\nSTALE" : connection);
    lv_obj_align_to(status, host_box, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
    uint32_t state_color = !online ? WARN : bt_state == SAT_BT_UNPAIRED && !usb ? 0xbdadf6 :
                           endpoint == SAT_HOST_OFFLINE ? WARN : CYAN;
    lv_obj_set_style_text_color(status, lv_color_hex(state_color), 0);
    uint8_t values[2] = {s->packet.battery_left, s->packet.battery_right};
    for (unsigned i = 0; i < 2; ++i) {
        bool unknown = values[i] > 100 ||
            (i == 1 && s->packet.role == 0) ||
            (values[i] == 0 && (i == 1 || s->packet.role == 1));
        bool low = !unknown && values[i] <= 10;
        if (unknown) snprintf(text, sizeof(text), "--");
        else snprintf(text, sizeof(text), "%u%%", values[i]);
        lv_label_set_text(battery[i], text);
        lv_obj_set_style_text_color(battery[i], lv_color_hex(!online ? DIM : low ? WARN : INK), 0);
        if (unknown || values[i] == 0) lv_obj_add_flag(fill[i], LV_OBJ_FLAG_HIDDEN);
        else {
            lv_obj_remove_flag(fill[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_width(fill[i], 94 * values[i] / 100);
        }
        lv_obj_set_style_bg_color(fill[i], lv_color_hex(!online ? DIM : low ? WARN : CYAN), 0);
    }
    snprintf(text, sizeof(text), "%02u", s->packet.layer);
    lv_label_set_text(layer_number, text);
    lv_obj_set_style_text_color(layer_number, lv_color_hex(online ? INK : DIM), 0);
}

void sat_ui_error(const char *message)
{
    lv_label_set_text(status, message);
    lv_obj_align_to(status, host_box, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
}
void sat_ui_tick(void) { lv_timer_handler(); }
