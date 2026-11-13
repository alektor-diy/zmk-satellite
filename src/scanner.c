/* SPDX-License-Identifier: MIT */
#include "scanner.h"
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sat_scan, LOG_LEVEL_INF);
K_MSGQ_DEFINE(rx_queue, sizeof(struct sat_rx), 16, 8);
static atomic_t dropped;

static void receive(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                    struct net_buf_simple *buf)
{
    ARG_UNUSED(addr); ARG_UNUSED(type);
    struct sat_rx rx = { .rssi = rssi, .time = k_uptime_get() };
    if (sat_parse_ad(buf->data, buf->len, &rx.packet) &&
        sat_channel_matches(CONFIG_SATELLITE_CHANNEL, rx.packet.channel)) {
        /* No UI, printing, allocation or blocking in the Bluetooth RX thread. */
        if (k_msgq_put(&rx_queue, &rx, K_NO_WAIT)) atomic_inc(&dropped);
    }
}
int sat_scanner_start(void)
{
    int ret = bt_enable(NULL);
    if (ret) return ret;
    const struct bt_le_scan_param params = {
        .type = BT_LE_SCAN_TYPE_ACTIVE, /* status can live in SCAN_RSP */
        .options = BT_LE_SCAN_OPT_NONE, /* repeats refresh last_seen */
        .interval = 0x60, .window = 0x60,
    };
    return bt_le_scan_start(&params, receive);
}
bool sat_scanner_take(struct sat_rx *rx)
{
    return k_msgq_get(&rx_queue, rx, K_NO_WAIT) == 0;
}
unsigned sat_scanner_dropped(void) { return (unsigned)atomic_get(&dropped); }
