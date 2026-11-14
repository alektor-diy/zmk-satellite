/* SPDX-License-Identifier: MIT */
#include "state.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <string.h>
#include <stdio.h>
#ifdef CONFIG_SATELLITE_BLE
#include "scanner.h"
#endif
#ifdef CONFIG_SATELLITE_DISPLAY
#include "ui.h"
#endif
LOG_MODULE_REGISTER(satellite, LOG_LEVEL_INF);

int main(void)
{
    int ret = usb_enable(NULL); /* Never wait for a host/DTR: USB-C power bank works. */
    if (ret) LOG_WRN("USB console unavailable: %d", ret);
    bool fatal = false;
    uint8_t filter[4];
    const uint8_t *filter_id = NULL;
    if (strlen(CONFIG_SATELLITE_KEYBOARD_ID)) {
        if (!sat_parse_id(CONFIG_SATELLITE_KEYBOARD_ID, filter)) {
            LOG_ERR("Invalid SATELLITE_KEYBOARD_ID (expected 8 hex chars)");
            fatal = true;
        } else filter_id = filter;
    }
#ifdef CONFIG_SATELLITE_DISPLAY
    ret = sat_ui_init();
    if (ret) { LOG_ERR("Display failed: %d", ret); return ret; }
    if (fatal) sat_ui_error("Invalid keyboard ID");
#endif
#ifdef CONFIG_SATELLITE_BLE
    if (!fatal) {
        ret = sat_scanner_start();
        if (ret) {
            LOG_ERR("BLE scan failed: %d", ret);
#ifdef CONFIG_SATELLITE_DISPLAY
            sat_ui_error("BLE start failed");
#endif
            fatal = true;
        }
    }
#endif
    struct sat_state state = {0};
    int64_t next_update = 0;
#ifdef CONFIG_SATELLITE_BLE
    int64_t next_diagnostics = 0;
#else
    ARG_UNUSED(filter_id);
#endif
    bool was_online = false;
    while (true) {
        int64_t now = k_uptime_get();
#ifdef CONFIG_SATELLITE_BLE
        struct sat_rx rx;
        if (now >= next_diagnostics) {
            LOG_INF("Scanning active; queue drops=%u", sat_scanner_dropped());
            next_diagnostics = now + 10000;
        }
        /* Bounded drain: a crowded RF environment cannot starve the display. */
        for (unsigned i = 0; !fatal && i < 16 && sat_scanner_take(&rx); ++i) {
            if (sat_update(&state, &rx.packet, rx.rssi, rx.time,
                           CONFIG_SATELLITE_CHANNEL, filter_id)) {
                if (rx.keyboard_name[0])
                    memcpy(state.keyboard_name, rx.keyboard_name, sizeof(state.keyboard_name));
                LOG_INF("ID=%02X%02X%02X%02X L=%u R=%u layer=%u(%s) BT=%u flags=%02x RSSI=%d",
                    rx.packet.id[0], rx.packet.id[1], rx.packet.id[2], rx.packet.id[3],
                    rx.packet.battery_left, rx.packet.battery_right, rx.packet.layer,
                    rx.packet.layer_name, rx.packet.profile + 1, rx.packet.flags, rx.rssi);
            }
        }
#endif
#ifdef CONFIG_SATELLITE_DISPLAY_TEST
        struct sat_packet p = {.version = 0x22, .battery_left = 76,
            .battery_right = 65, .role = 1, .profile = 2, .flags = 0x10,
            .layer = (now / 3000) % 5, .id = {0x12,0x34,0x56,0x78}};
        const char *demo_layers[] = {"BASE", "NAV", "NUM", "ADJ", "Auto"};
        snprintf(p.layer_name, sizeof(p.layer_name), "%s", demo_layers[p.layer]);
        const uint8_t demo_flags[] = {0x30, 0x20, 0x00, 0x08, 0x38};
        p.flags = demo_flags[(now / 3000) % 5];
        memcpy(state.keyboard_name, "Demo board", 11);
        /* 15s live simulation, then simulated loss; repeats every 25s. */
        if (now % 25000 < 15000)
            sat_update(&state, &p, -48, now, 0, NULL);
#endif
        if (now >= next_update && !fatal) {
            bool online = sat_online(&state, now, CONFIG_SATELLITE_TIMEOUT_MS);
            if (was_online && !online) LOG_WRN("Advertisement timeout; values are stale");
            was_online = online;
#ifdef CONFIG_SATELLITE_DISPLAY
            sat_ui_update(&state, now);
#endif
            next_update = now + 250;
        }
#ifdef CONFIG_SATELLITE_DISPLAY
        sat_ui_tick(); /* LVGL is owned exclusively by this thread. */
#endif
        k_sleep(K_MSEC(10));
    }
    return 0;
}
