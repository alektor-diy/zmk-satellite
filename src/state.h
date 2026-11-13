/* SPDX-License-Identifier: MIT */
#pragma once
#include "protocol.h"
struct sat_state {
    struct sat_packet packet;
    int64_t last_seen;
    int8_t rssi;
    bool seen;
};
bool sat_online(const struct sat_state *s, int64_t now, int64_t timeout);
/* First ID stays selected, including after timeout. No mixing adjacent keyboards. */
bool sat_update(struct sat_state *s, const struct sat_packet *p, int8_t rssi,
                int64_t now, uint8_t channel, const uint8_t *filter_id);
