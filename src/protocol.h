/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SAT_PAYLOAD_SIZE 26
struct sat_packet {
    uint8_t version, battery_left, battery_right, layer, profile;
    uint8_t connections, flags, role, index, aux[2], id[4];
    uint8_t modifiers, wpm, channel, patch;
    bool dev;
    char layer_name[5];
};
/* Decode bytes explicitly, without packed-struct casts or alignment assumptions. */
bool sat_decode(const uint8_t *data, size_t len, struct sat_packet *out);
bool sat_parse_ad(const uint8_t *data, size_t len, struct sat_packet *out);
bool sat_channel_matches(uint8_t scanner, uint8_t keyboard);
bool sat_parse_id(const char *text, uint8_t out[4]);
