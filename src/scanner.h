#pragma once
#include "state.h"
struct sat_rx { struct sat_packet packet; int8_t rssi; int64_t time; char keyboard_name[SAT_NAME_SIZE]; };
int sat_scanner_start(void);
bool sat_scanner_take(struct sat_rx *rx);
unsigned sat_scanner_dropped(void);
