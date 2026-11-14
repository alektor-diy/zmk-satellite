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
    assert(sat_bt_from_flags(0) == SAT_BT_UNPAIRED);
    assert(sat_bt_from_flags(0x20) == SAT_BT_PAIRED);
    assert(sat_bt_from_flags(0x30) == SAT_BT_CONNECTED);
    assert(sat_bt_from_flags(0x10) == SAT_BT_CONNECTED);
    assert(sat_bt_from_flags(0x08) == SAT_BT_UNPAIRED);
    assert(sat_bt_from_flags(0x28) == SAT_BT_PAIRED);

    char name[SAT_NAME_SIZE] = {0};
    const uint8_t short_name[] = {5, 0x08, 'i','O','r','a'};
    assert(sat_parse_name(short_name, sizeof(short_name), name));
    assert(!strcmp(name, "iOra"));
    const uint8_t names[] = {3,0x08,'A','B', 5,0x09,'N','a','m','e', 2,0x08,'X'};
    assert(sat_parse_name(names, sizeof(names), name) && !strcmp(name, "Name"));
    const uint8_t utf_name[] = {8,0x09,'T','w','n',0xef,0xbc,0xbf,'L'};
    assert(sat_parse_name(utf_name, sizeof(utf_name), name) && !strcmp(name, "Twn_L"));
    const uint8_t unsafe_name[] = {5,0x09,'A',0x01,0xc3,0xa9};
    assert(sat_parse_name(unsafe_name, sizeof(unsafe_name), name) && !strcmp(name, "A??"));
    const uint8_t malformed_name[] = {3,0x09,'O','K', 9,0x09,'X'};
    strcpy(name, "unchanged");
    assert(!sat_parse_name(malformed_name, sizeof(malformed_name), name));
    assert(!strcmp(name, "unchanged"));
    uint8_t long_name[60]; memset(long_name, 'A', sizeof(long_name));
    long_name[0] = 59; long_name[1] = 0x09;
    assert(sat_parse_name(long_name, sizeof(long_name), name));
    assert(strlen(name) == SAT_NAME_SIZE - 1);
    assert(!sat_parse_name(NULL, 4, name) && !sat_parse_name(short_name, 6, NULL));
    assert(sat_host_from_flags(0) == SAT_HOST_OFFLINE);
    assert(sat_host_from_flags(0x04) == SAT_HOST_OFFLINE); /* USB power is not HID-ready. */
    assert(sat_host_from_flags(0x08) == SAT_HOST_USB);
    assert(sat_host_from_flags(0x10) == SAT_HOST_BLE);
    assert(sat_host_from_flags(0x18) == SAT_HOST_BOTH);

    struct sat_packet p, q;
    assert(sat_parse_ad(fixture, sizeof(fixture), &p));
    assert(p.battery_left == 76 && p.battery_right == 65);
    assert(p.profile == 2 && p.patch == 3 && !p.dev);
    assert(p.layer == 3 && !strcmp(p.layer_name, "NAV!"));
    uint8_t renamed[sizeof(fixture)]; memcpy(renamed, fixture, sizeof(fixture));
    memcpy(renamed + 20, "EDIT", 4);
    assert(sat_parse_ad(renamed, sizeof(renamed), &q));
    assert(!strcmp(q.layer_name, "EDIT") && q.layer == p.layer);
    struct sat_state renamed_state = {0};
    assert(sat_update(&renamed_state, &p, -50, 1, 0, NULL));
    assert(sat_update(&renamed_state, &q, -50, 2, 0, NULL));
    assert(!strcmp(renamed_state.packet.layer_name, "EDIT"));
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
        sat_parse_name(data, j % sizeof(data), name);
    }
    puts("protocol/state: all assertions passed; 100000 malformed frames checked");
}
