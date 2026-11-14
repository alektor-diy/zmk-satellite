/* SPDX-License-Identifier: MIT */
#include "scanner.h"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sat_scan, LOG_LEVEL_INF);
K_MSGQ_DEFINE(rx_queue, sizeof(struct sat_rx), 16, 8);
static atomic_t dropped;

/* Only valid status senders get cache entries; unrelated name beacons cannot evict them. */
static struct {
    bt_addr_le_t address;
    uint8_t id[4];
    char name[SAT_NAME_SIZE];
    int64_t last_seen;
    bool used;
} names[8];

static void receive(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                    struct net_buf_simple *buf)
{
    ARG_UNUSED(type);
    struct sat_rx rx = {.rssi = rssi, .time = k_uptime_get()};
    bool packet = sat_parse_ad(buf->data, buf->len, &rx.packet) &&
        sat_channel_matches(CONFIG_SATELLITE_CHANNEL, rx.packet.channel);
    int entry = -1, oldest = 0;
    for (unsigned i = 0; i < ARRAY_SIZE(names); ++i) {
        if (names[i].used && !bt_addr_le_cmp(addr, &names[i].address)) entry = i;
        if (!names[i].used || names[i].last_seen < names[oldest].last_seen) oldest = i;
    }
    if (packet) {
        if (entry < 0) entry = oldest;
        if (!names[entry].used || bt_addr_le_cmp(addr, &names[entry].address) ||
            memcmp(names[entry].id, rx.packet.id, 4)) {
            memset(&names[entry], 0, sizeof(names[entry]));
            bt_addr_le_copy(&names[entry].address, addr);
            memcpy(names[entry].id, rx.packet.id, 4);
            names[entry].used = true;
        }
    }
    if (entry >= 0) {
        char name[SAT_NAME_SIZE];
        if (sat_parse_name(buf->data, buf->len, name))
            memcpy(names[entry].name, name, sizeof(name));
        names[entry].last_seen = rx.time;
    }
    if (packet) {
        memcpy(rx.keyboard_name, names[entry].name, sizeof(rx.keyboard_name));
        /* UI/state updates remain exclusively on the main thread. */
        if (k_msgq_put(&rx_queue, &rx, K_NO_WAIT)) atomic_inc(&dropped);
    }
}
int sat_scanner_start(void)
{
    int ret = bt_enable(NULL);
    if (ret) return ret;
    const struct bt_le_scan_param params = {
        .type = BT_LE_SCAN_TYPE_ACTIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = 0x60, .window = 0x60,
    };
    return bt_le_scan_start(&params, receive);
}
bool sat_scanner_take(struct sat_rx *rx) { return k_msgq_get(&rx_queue, rx, K_NO_WAIT) == 0; }
unsigned sat_scanner_dropped(void) { return (unsigned)atomic_get(&dropped); }
