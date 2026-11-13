/* SPDX-License-Identifier: MIT */
#include "protocol.h"
#include "state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Independent fixture following v2.2.3 sender offsets, not a struct dump. */
static const uint8_t fixture[31] = {
    2, 1, 6, 27, 0xff,
    0xff,0xff,0xab,0xcd,0x22,76,3,0x1a,1,0x18,1,0,65,0,0,
    'N','A','V','!',0x12,0x34,0x56,0x78,0,42,3
};
int main(void)
{
    struct sat_packet p, q;
    assert(sat_parse_ad(fixture, sizeof(fixture), &p));
    assert(p.battery_left == 76 && p.battery_right == 65);
    assert(p.profile == 2 && p.patch == 3 && !p.dev);
    assert(p.layer == 3 && !strcmp(p.layer_name, "NAV!"));
    assert(p.flags == 0x18 && p.channel == 3 && p.wpm == 42);
    assert(p.id[0] == 0x12 && p.id[3] == 0x78);
    /* ADV/SCAN_RSP placement: manufacturer-only report is equally valid. */
    assert(sat_parse_ad(fixture + 3, 28, &q));
    for (size_t i = 0; i < sizeof(fixture); ++i)
        assert(!sat_parse_ad(fixture, i, &q));
    uint8_t data[40]; memcpy(data, fixture, 31);
    data[3] = 255; assert(!sat_parse_ad(data, 31, &q));
    memcpy(data, fixture, 31); data[5] = 0;
    assert(!sat_parse_ad(data, 31, &q));
    memcpy(data, fixture, 31); data[9] = 0x32;
    assert(!sat_parse_ad(data, 31, &q));
    data[9] = 1; assert(sat_parse_ad(data, 31, &q));
    data[15] = 2; assert(!sat_parse_ad(data, 31, &q)); /* role 2 */
    memcpy(data, fixture, 31); data[12] = 0x1f;
    assert(!sat_parse_ad(data, 31, &q)); /* invalid masked profile */
    memcpy(data, fixture, 31); data[20] = 0xff;
    assert(sat_parse_ad(data, 31, &q) && q.layer_name[0] == '?');
    assert(sat_channel_matches(0, 3) && sat_channel_matches(3, 0));
    assert(sat_channel_matches(10, 3) && sat_channel_matches(3, 3));
    assert(!sat_channel_matches(3, 4));
    uint8_t id[4];
    assert(sat_parse_id("12345678", id) && !memcmp(id, p.id, 4));
    assert(!sat_parse_id("xyz", id) && !sat_parse_id("1234567g", id));
    struct sat_state s = {0};
    assert(!sat_online(&s, 0, 100));
    assert(!sat_update(&s, &p, -50, 100, 4, NULL));
    assert(sat_update(&s, &p, -50, 100, 3, id));
    assert(sat_online(&s, 199, 100) && !sat_online(&s, 200, 100));
    q = p; q.id[0]++;
    assert(!sat_update(&s, &q, -10, 300, 0, NULL));
    assert(sat_update(&s, &p, -60, 400, 0, NULL)); /* reacquire same ID */
    assert(sat_online(&s, 400, 100) && s.rssi == -60);
    /* Deterministic malformed AD smoke test for sanitizer/host runs. */
    uint32_t rng = 12345;
    for (unsigned j = 0; j < 100000; ++j) {
        for (size_t k = 0; k < sizeof(data); ++k) {
            rng = rng * 1664525u + 1013904223u; data[k] = rng >> 24;
        }
        sat_parse_ad(data, j % sizeof(data), &q);
    }
    puts("protocol/state: all assertions passed; 100000 malformed frames checked");
}
