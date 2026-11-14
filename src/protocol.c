/* SPDX-License-Identifier: MIT */
#include "protocol.h"
#include <string.h>

bool sat_decode(const uint8_t *d, size_t n, struct sat_packet *out)
{
    if (!d || !out || n < SAT_PAYLOAD_SIZE || d[0] != 0xff || d[1] != 0xff ||
        d[2] != 0xab || d[3] != 0xcd) return false;
    /* v1 legacy or module-major 2. Future majors require a protocol review. */
    if (d[4] != 1 && (d[4] >> 4) != 2) return false;
    if (d[10] > 1 || (d[7] & 7) > 4) return false; /* ignore split peripherals */
    struct sat_packet p = {
        .version = d[4], .battery_left = d[5], .battery_right = d[12],
        .layer = d[6], .profile = d[7] & 7, .patch = (d[7] >> 3) & 7,
        .dev = (d[7] & 0x40) != 0, .connections = d[8], .flags = d[9],
        .role = d[10], .index = d[11], .aux = {d[13], d[14]},
        .modifiers = d[23], .wpm = d[24], .channel = d[25],
    };
    memcpy(p.id, d + 19, 4);
    /* Built-in font is ASCII. Do not let partial UTF-8/control bytes reach LVGL. */
    for (size_t i = 0; i < 4; ++i)
        p.layer_name[i] = d[15+i] == 0 ? 0 :
                            (d[15+i] >= 32 && d[15+i] <= 126 ? d[15+i] : '?');
    p.layer_name[4] = 0;
    *out = p;
    return true;
}

bool sat_parse_ad(const uint8_t *d, size_t n, struct sat_packet *out)
{
    if (!d || !out) return false;
    while (n) {
        size_t field = *d++;
        --n;
        if (!field) break;
        if (field > n) return false;
        if (d[0] == 0xff && sat_decode(d + 1, field - 1, out)) return true;
        d += field;
        n -= field;
    }
    return false;
}

bool sat_channel_matches(uint8_t scanner, uint8_t keyboard)
{
    return scanner == 0 || scanner >= 10 || keyboard == 0 || scanner == keyboard;
}

static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool sat_parse_id(const char *s, uint8_t out[4])
{
    uint8_t id[4];
    if (!s || !out || strlen(s) != 8) return false;
    for (unsigned i = 0; i < 4; ++i) {
        int a = hex(s[i*2]), b = hex(s[i*2+1]);
        if (a < 0 || b < 0) return false;
        id[i] = (uint8_t)((a << 4) | b);
    }
    memcpy(out, id, 4);
    return true;
}

/* Names are separate AD reports. Keep parsing independent of the Bluetooth stack. */
bool sat_parse_name(const uint8_t *d, size_t n, char out[SAT_NAME_SIZE])
{
    if (!d || !out) return false;
    char name[SAT_NAME_SIZE] = {0};
    bool found = false, complete = false;
    while (n) {
        size_t field = *d++;
        --n;
        if (!field) break;
        if (field > n) return false;
        if ((d[0] == 0x08 || d[0] == 0x09) && field > 1 &&
            (!complete || d[0] == 0x09)) {
            size_t used = 0;
            for (size_t i = 1; i < field && used < SAT_NAME_SIZE - 1; ++i) {
                uint8_t c = d[i];
                if (!c) break;
                if (i + 2 < field && c == 0xef && d[i+1] == 0xbc && d[i+2] == 0xbf) {
                    name[used++] = '_'; /* Full-width underscore, common in keyboard names. */
                    i += 2;
                } else if (c >= 0x80) {
                    name[used++] = '?';
                    while (i + 1 < field && (d[i+1] & 0xc0) == 0x80) ++i;
                } else name[used++] = c >= 32 && c <= 126 ? (char)c : '?';
            }
            name[used] = 0;
            found = used > 0;
            complete = d[0] == 0x09;
        }
        d += field;
        n -= field;
    }
    if (found) memcpy(out, name, SAT_NAME_SIZE);
    return found;
}

enum sat_host_status sat_host_from_flags(uint8_t flags)
{
    bool usb = (flags & 0x08) != 0; /* HID-ready, not merely USB power. */
    bool ble = (flags & 0x10) != 0;
    return usb ? (ble ? SAT_HOST_BOTH : SAT_HOST_USB) :
                 (ble ? SAT_HOST_BLE : SAT_HOST_OFFLINE);
}

enum sat_bt_status sat_bt_from_flags(uint8_t flags)
{
    if (flags & 0x10) return SAT_BT_CONNECTED;
    return flags & 0x20 ? SAT_BT_PAIRED : SAT_BT_UNPAIRED;
}
