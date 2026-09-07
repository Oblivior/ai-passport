#pragma once

#include "lvgl.h"
#include "pet_model.h"

/* Original procedural pixel creature; no third-party character artwork. */
lv_obj_t *pet_view_create(lv_obj_t *parent, pet_stage_t stage, int x, int y);
void pet_view_bounce(lv_obj_t *pet);

typedef enum {
    PET_POSE_IDLE, PET_POSE_EAT, PET_POSE_HAPPY, PET_POSE_SLEEP, PET_POSE_EVOLVE,
} pet_pose_t;
lv_obj_t *pet_view_create_pose(lv_obj_t *parent, pet_stage_t stage, int x, int y, pet_pose_t pose);
lv_obj_t *pet_view_create_route_pose(lv_obj_t *parent, pet_stage_t stage, pet_route_t route,
                                     int x, int y, pet_pose_t pose);
void pet_view_frame(lv_obj_t *pet, pet_pose_t pose, unsigned frame);

/* Species-specific fan-art; the caller supplies a stable catalog ID. */
lv_obj_t *pet_view_create_digimon_pose(lv_obj_t *parent, unsigned species_id, pet_stage_t stage,
                                      int x, int y, pet_pose_t pose);
