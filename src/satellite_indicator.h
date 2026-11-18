/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <lvgl.h>

void sat_indicator_init(lv_obj_t *parent);
void sat_indicator_receiving(bool receiving);
/* Draw the shared silhouette at a pixel centre and width, without an image buffer. */
void sat_indicator_draw(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t width,
                        int angle, lv_color_t color, lv_opa_t opacity);
