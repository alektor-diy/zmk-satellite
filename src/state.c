/* SPDX-License-Identifier: MIT */
#include "state.h"
#include <string.h>
bool sat_online(const struct sat_state *s, int64_t now, int64_t timeout)
{
    return s->seen && now >= s->last_seen && now - s->last_seen < timeout;
}
bool sat_update(struct sat_state *s, const struct sat_packet *p, int8_t rssi,
                int64_t now, uint8_t channel, const uint8_t *filter_id)
{
    if (!sat_channel_matches(channel, p->channel)) return false;
    if (filter_id && memcmp(filter_id, p->id, 4)) return false;
    if (s->seen && memcmp(s->packet.id, p->id, 4)) return false;
    s->packet = *p;
    s->rssi = rssi;
    s->last_seen = now;
    s->seen = true;
    return true;
}
