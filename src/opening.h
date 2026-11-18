/* SPDX-License-Identifier: MIT */
#pragma once
#include <lvgl.h>

/* Nonblocking, one-shot opening. All calls belong to the UI thread. */
void sat_opening_start(lv_obj_t *screen, lv_obj_t *content);
void sat_opening_cancel(void);
