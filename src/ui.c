/* SPDX-License-Identifier: MIT */
#include "ui.h"
#include <stdio.h>
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <lvgl_zephyr.h>

#define INK 0xe8eff3
#define DIM 0x9aafbb
#define CYAN 0x68bdd5
#define WARN 0xffb66e
#define USB_COLOR 0xf3bb76

static lv_obj_t *name, *status, *layer_number, *host, *host_icon, *endpoint_row;
static lv_obj_t *battery[2], *unit[2], *ring[2];

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

static lv_obj_t *center_label(lv_obj_t *parent, const char *text, int x, int y,
                              int w, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = label(parent, text, x, y, w, font, color);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    return obj;
}

static lv_obj_t *battery_ring(lv_obj_t *screen, int x)
{
    lv_obj_t *obj = lv_arc_create(screen);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, 90);
    lv_obj_set_size(obj, 88, 88);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_arc_width(obj, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(obj, lv_color_hex(0x30434e), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(obj, false, LV_PART_MAIN);
    lv_obj_set_style_arc_width(obj, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(obj, lv_color_hex(CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(obj, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(obj, false, LV_PART_INDICATOR);
    /* Full circle, clockwise from twelve o'clock. Zero has no indicator. */
    lv_arc_set_bg_angles(obj, 0, 360);
    lv_arc_set_rotation(obj, 270);
    lv_arc_set_range(obj, 0, 100);
    lv_arc_set_value(obj, 0);
    return obj;
}

int sat_ui_init(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(dev)) return -ENODEV;
    int ret = lvgl_init();
    if (ret) return ret;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0d1820), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *brand = label(screen, "ZMK SATELLITE", 14, 14, 212,
                            &lv_font_montserrat_10, DIM);
    lv_obj_set_style_text_letter_space(brand, 1, 0);
    name = label(screen, "Keyboard", 14, 35, 212, &lv_font_montserrat_24, INK);

    /* Shared column centres at x=64 and x=176. */
    for (unsigned i = 0; i < 2; ++i) {
        int x = i ? 124 : 12;
        center_label(screen, i ? "RIGHT" : "LEFT", x, 76, 104,
                     &lv_font_montserrat_10, 0xa5b7c1);
        ring[i] = battery_ring(screen, x + 8);
        battery[i] = center_label(screen, "--", x + 10, 109, 84,
                                  &lv_font_montserrat_36, INK);
        unit[i] = center_label(screen, "%", x + 10, 148, 84,
                               &lv_font_montserrat_12, 0xa5b7c1);
        lv_obj_add_flag(unit[i], LV_OBJ_FLAG_HIDDEN);
    }

    center_label(screen, "LAYER", 12, 190, 104, &lv_font_montserrat_10, 0xa5b7c1);
    layer_number = center_label(screen, "--", 12, 204, 104,
                                &lv_font_montserrat_24, INK);
    status = center_label(screen, "WAITING", 124, 190, 104,
                          &lv_font_montserrat_10, CYAN);
    endpoint_row = lv_obj_create(screen);
    lv_obj_remove_style_all(endpoint_row);
    lv_obj_set_pos(endpoint_row, 124, 203);
    lv_obj_set_size(endpoint_row, 104, 29);
    lv_obj_remove_flag(endpoint_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(endpoint_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(endpoint_row, LV_FLEX_ALIGN_CENTER,
                         LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(endpoint_row, 5, 0);
    /* Existing LVGL font symbols: Font Awesome notices are in licenses/. */
    host_icon = label(endpoint_row, LV_SYMBOL_BLUETOOTH, 0, 0, LV_SIZE_CONTENT,
                      &lv_font_montserrat_18, CYAN);
    lv_obj_add_flag(host_icon, LV_OBJ_FLAG_HIDDEN);
    host = label(endpoint_row, "--", 0, 0, LV_SIZE_CONTENT,
                 &lv_font_montserrat_24, DIM);
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
    enum sat_bt_status bt_state = sat_bt_from_flags(s->packet.flags);
    bool usb = endpoint == SAT_HOST_USB;
    char text[32];
    if (usb) snprintf(text, sizeof(text), "USB");
    else snprintf(text, sizeof(text), "BT%u", s->packet.profile + 1);
    lv_label_set_text(host, text);
    lv_label_set_text(host_icon, usb ? LV_SYMBOL_USB : LV_SYMBOL_BLUETOOTH);
    lv_obj_remove_flag(host_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(host, lv_color_hex(online ? INK : DIM), 0);
    lv_obj_set_style_text_color(host_icon, lv_color_hex(!online ? DIM : usb ? USB_COLOR : CYAN), 0);
    const char *connection = endpoint == SAT_HOST_BOTH ? "BT + USB READY" :
        usb || bt_state == SAT_BT_CONNECTED ? "CONNECTED" :
        bt_state == SAT_BT_PAIRED ? "PAIRED / OFFLINE" : "NOT PAIRED";
    lv_label_set_text(status, online ? connection : "SIGNAL LOST");
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
        else snprintf(text, sizeof(text), "%u", values[i]);
        lv_label_set_text(battery[i], text);
        lv_obj_set_style_text_color(battery[i], lv_color_hex(!online ? DIM : low ? WARN : INK), 0);
        if (unknown) lv_obj_add_flag(unit[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(unit[i], LV_OBJ_FLAG_HIDDEN);
        lv_arc_set_value(ring[i], unknown ? 0 : values[i]);
        lv_obj_set_style_arc_color(ring[i], lv_color_hex(!online ? DIM : low ? WARN : CYAN),
                                   LV_PART_INDICATOR);
    }
    snprintf(text, sizeof(text), "%02u", s->packet.layer);
    lv_label_set_text(layer_number, text);
    lv_obj_set_style_text_color(layer_number, lv_color_hex(online ? INK : DIM), 0);
}

void sat_ui_error(const char *message)
{
    lv_label_set_text(status, message);
    lv_obj_set_style_text_color(status, lv_color_hex(WARN), 0);
}
void sat_ui_tick(void) { lv_timer_handler(); }
