/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <lvgl.h>

void sat_indicator_init(lv_obj_t *parent);
void sat_indicator_receiving(bool receiving);
