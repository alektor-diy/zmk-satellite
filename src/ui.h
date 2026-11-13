#pragma once
#include "state.h"
int sat_ui_init(void);
void sat_ui_update(const struct sat_state *s, int64_t now);
void sat_ui_error(const char *message);
void sat_ui_tick(void);
