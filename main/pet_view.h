#pragma once

#include "lvgl.h"
#include "pet_model.h"

/* Original procedural pixel creature; no third-party character artwork. */
lv_obj_t *pet_view_create(lv_obj_t *parent, pet_stage_t stage, int x, int y);
void pet_view_bounce(lv_obj_t *pet);
