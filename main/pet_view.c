#include "pet_view.h"

#include "ui_pixel.h"
#include "../assets/images/digimon_sprites.h"
#include "pet_catalog.h"

static lv_obj_t *pixel(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

static uint32_t body_color(pet_stage_t stage, pet_route_t route)
{
    if (stage < PET_STAGE_RANGER) return UI_ORANGE;
    if (route == PET_ROUTE_ARMOR) return 0x427AA1;
    if (route == PET_ROUTE_WILD) return 0xE87532;
    if (route == PET_ROUTE_EXPLORER) return 0x389986;
    return 0x7557D9;
}

static lv_obj_t *create(lv_obj_t *parent, pet_stage_t stage, pet_route_t route, int x, int y)
{
    lv_obj_t *pet = lv_obj_create(parent);
    lv_obj_remove_flag(pet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(pet, x, y);
    lv_obj_set_size(pet, 100, 94);
    lv_obj_set_style_bg_opa(pet, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pet, 0, 0);
    lv_obj_set_style_pad_all(pet, 0, 0);

    if (stage == PET_STAGE_EGG) {
        pixel(pet, 32, 11, 36, 6, UI_INK);
        pixel(pet, 24, 17, 52, 54, UI_INK);
        pixel(pet, 30, 17, 40, 48, UI_PAPER);
        pixel(pet, 30, 35, 10, 8, UI_SKY);
        pixel(pet, 58, 49, 12, 8, UI_ORANGE);
        return pet;
    }

    uint32_t body = body_color(stage, route);
    pixel(pet, 29, 20, 42, 45, UI_INK);
    pixel(pet, 34, 16, 32, 45, body);
    pixel(pet, 25, 28, 9, 10, body);
    pixel(pet, 66, 28, 9, 10, body);
    pixel(pet, 40, 27, 6, 8, UI_PAPER);
    pixel(pet, 54, 27, 6, 8, UI_PAPER);
    pixel(pet, 42, 29, 3, 5, UI_INK);
    pixel(pet, 56, 29, 3, 5, UI_INK);
    pixel(pet, 46, 42, 10, 4, UI_INK);
    pixel(pet, 34, 61, 12, 16, UI_INK);
    pixel(pet, 54, 61, 12, 16, UI_INK);

    if (stage >= PET_STAGE_BYTE) {
        pixel(pet, 27, 8, 10, 15, UI_INK);
        pixel(pet, 63, 8, 10, 15, UI_INK);
        pixel(pet, 29, 10, 6, 11, UI_YELLOW);
        pixel(pet, 65, 10, 6, 11, UI_YELLOW);
    }
    if (stage >= PET_STAGE_SCOUT) {
        pixel(pet, 17, 38, 13, 9, UI_INK);
        pixel(pet, 70, 38, 13, 9, UI_INK);
        pixel(pet, 10, 43, 10, 6, UI_INK);
        pixel(pet, 80, 43, 10, 6, UI_INK);
    }
    if (stage >= PET_STAGE_RANGER) {
        if (route == PET_ROUTE_ARMOR) {
            pixel(pet, 26, 17, 48, 9, 0xB9F3FF);
            pixel(pet, 31, 46, 38, 12, 0xB9F3FF);
            pixel(pet, 12, 42, 17, 24, UI_SKY_DARK);
            pixel(pet, 17, 47, 7, 14, UI_PAPER);
        } else if (route == PET_ROUTE_WILD) {
            pixel(pet, 18, 15, 14, 18, UI_RED);
            pixel(pet, 68, 15, 14, 18, UI_RED);
            pixel(pet, 37, 47, 26, 7, UI_YELLOW);
            pixel(pet, 79, 45, 15, 16, UI_ORANGE);
        } else if (route == PET_ROUTE_EXPLORER) {
            pixel(pet, 25, 14, 50, 8, UI_YELLOW);
            pixel(pet, 31, 43, 40, 8, UI_RED);
            pixel(pet, 71, 43, 17, 8, UI_RED);
            pixel(pet, 69, 51, 12, 7, UI_RED);
        } else {
            pixel(pet, 31, 17, 38, 7, UI_YELLOW);
            pixel(pet, 27, 47, 46, 7, UI_YELLOW);
        }
    }
    if (stage >= PET_STAGE_TITAN) {
        uint32_t armor = route == PET_ROUTE_WILD ? UI_RED : route == PET_ROUTE_EXPLORER ? 0x389986 : 0xB9F3FF;
        pixel(pet, 18, 22, 13, 17, armor);
        pixel(pet, 69, 22, 13, 17, armor);
        pixel(pet, 22, 55, 11, 19, UI_SKY_DARK);
        pixel(pet, 67, 55, 11, 19, UI_SKY_DARK);
    }
    if (stage >= PET_STAGE_APEX) {
        pixel(pet, 8, 15, 15, 6, UI_YELLOW);
        pixel(pet, 77, 15, 15, 6, UI_YELLOW);
        pixel(pet, 4, 9, 7, 7, UI_PAPER);
        pixel(pet, 89, 9, 7, 7, UI_PAPER);
        pixel(pet, 44, 3, 12, 13, UI_RED);
    }
    return pet;
}

lv_obj_t *pet_view_create(lv_obj_t *parent, pet_stage_t stage, int x, int y)
{
    return create(parent, stage, PET_ROUTE_CORE, x, y);
}

static void bounce_y(void *obj, int32_t value)
{
    lv_obj_set_style_translate_y((lv_obj_t *)obj, value, 0);
}

void pet_view_bounce(lv_obj_t *pet)
{
    if (!pet) return;
    lv_anim_delete(pet, bounce_y);
    /* A newly created object's computed coordinates are stale until layout.
     * Animate a relative offset, never overwrite its declared position.
     * Resetting the offset also prevents interrupted jumps accumulating drift. */
    lv_obj_set_style_translate_y(pet, 0, 0);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, pet);
    lv_anim_set_exec_cb(&anim, bounce_y);
    lv_anim_set_values(&anim, 0, -8);
    lv_anim_set_duration(&anim, 120);
    lv_anim_set_playback_duration(&anim, 170);
    lv_anim_set_path_cb(&anim, lv_anim_path_step);
    lv_anim_start(&anim);
}

lv_obj_t *pet_view_create_pose(lv_obj_t *parent, pet_stage_t stage, int x, int y, pet_pose_t pose)
{
    return pet_view_create_route_pose(parent, stage, PET_ROUTE_CORE, x, y, pose);
}

lv_obj_t *pet_view_create_route_pose(lv_obj_t *parent, pet_stage_t stage, pet_route_t route,
                                     int x, int y, pet_pose_t pose)
{
    lv_obj_t *pet = create(parent, stage, route, x, y);
    uint32_t body = body_color(stage, route);
    if (pose == PET_POSE_SLEEP && stage != PET_STAGE_EGG) {
        pixel(pet, 39, 27, 9, 9, body);
        pixel(pet, 53, 27, 9, 9, body);
        pixel(pet, 39, 31, 9, 2, UI_INK);
        pixel(pet, 53, 31, 9, 2, UI_INK);
    }
    if (pose == PET_POSE_SLEEP) {
        lv_obj_t *z = lv_label_create(pet);
        lv_label_set_text(z, "z Z");
        lv_obj_set_style_text_font(z, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(z, lv_color_hex(UI_SKY_DARK), 0);
        lv_obj_set_pos(z, 71, 0);
    } else if (pose == PET_POSE_EAT) {
        pixel(pet, 42, 42, 16, 8, UI_INK);
        pixel(pet, 45, 45, 10, 3, UI_RED);
        pixel(pet, 35, 79, 30, 8, UI_ORANGE);
        pixel(pet, 40, 86, 20, 4, UI_INK);
        pixel(pet, 40, 76, 20, 4, UI_YELLOW);
    } else if (pose == PET_POSE_HAPPY) {
        pixel(pet, 78, 27, 5, 5, UI_RED);
        pixel(pet, 87, 27, 5, 5, UI_RED);
        pixel(pet, 78, 32, 14, 5, UI_RED);
        pixel(pet, 82, 37, 6, 4, UI_RED);
    }
    return pet;
}

void pet_view_frame(lv_obj_t *pet, pet_pose_t pose, unsigned frame)
{
    if (!pet) return;
    int offset = 0;
    if (pose == PET_POSE_HAPPY) offset = frame % 2 ? -8 : 0;
    else if (pose == PET_POSE_EAT) offset = frame % 2 ? -3 : 0;
    else if (pose == PET_POSE_IDLE) offset = frame % 6 < 3 ? -1 : 0;
    lv_obj_set_style_translate_y(pet, offset, 0);
    lv_obj_set_style_translate_x(pet, pose == PET_POSE_EVOLVE ? (frame % 2 ? 2 : -2) : 0, 0);
    /* Blink without a translucent off-screen layer on this no-PSRAM board. */
    if (pose == PET_POSE_EVOLVE && frame % 2) lv_obj_add_flag(pet, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(pet, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *pet_view_create_digimon_pose(lv_obj_t *parent, unsigned species_id, pet_stage_t stage,
                                      int x, int y, pet_pose_t pose)
{
    const pet_species_info_t *species = pet_catalog_find(species_id);
    if (!species || species->artwork >= sizeof(digimon_sprites) / sizeof(digimon_sprites[0])) return NULL;
    if ((unsigned)stage >= PET_STAGE_COUNT) stage = PET_STAGE_EGG;
    lv_obj_t *pet = lv_obj_create(parent);
    lv_obj_remove_flag(pet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(pet, x, y);
    lv_obj_set_size(pet, 100, 94);
    lv_obj_set_style_bg_opa(pet, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pet, 0, 0);
    lv_obj_set_style_pad_all(pet, 0, 0);
    lv_obj_t *sprite = lv_image_create(pet);
    unsigned variant = pose == PET_POSE_SLEEP ? 2 : pose == PET_POSE_EAT ? 1 : 0;
    lv_image_set_src(sprite, &digimon_sprites[species->artwork][stage][variant]);
    lv_image_set_pivot(sprite, 0, 0);
    lv_image_set_scale(sprite, 768); /* Exact 3x nearest-neighbor, 96 x 84. */
    lv_image_set_antialias(sprite, false);
    lv_obj_set_pos(sprite, 2, 5);
    if (pose == PET_POSE_SLEEP) {
        lv_obj_t *z = lv_label_create(pet);
        lv_label_set_text(z, "z Z");
        lv_obj_set_style_text_font(z, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(z, lv_color_hex(UI_SKY_DARK), 0);
        lv_obj_set_pos(z, 71, 0);
    } else if (pose == PET_POSE_EAT) {
        pixel(pet, 38, 85, 24, 4, UI_ORANGE);
        pixel(pet, 36, 89, 28, 3, UI_INK);
    } else if (pose == PET_POSE_HAPPY) {
        pixel(pet, 79, 0, 4, 4, UI_RED);
        pixel(pet, 86, 0, 4, 4, UI_RED);
        pixel(pet, 79, 4, 11, 3, UI_RED);
        pixel(pet, 82, 7, 5, 3, UI_RED);
    }
    return pet;
}
