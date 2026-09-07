#include "pet_view.h"

#include "ui_pixel.h"

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

lv_obj_t *pet_view_create(lv_obj_t *parent, pet_stage_t stage, int x, int y)
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

    uint32_t body = stage >= PET_STAGE_RANGER ? 0x7557D9 : UI_ORANGE;
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
        pixel(pet, 31, 17, 38, 7, UI_YELLOW);
        pixel(pet, 27, 47, 46, 7, UI_YELLOW);
    }
    if (stage >= PET_STAGE_TITAN) {
        pixel(pet, 18, 22, 13, 17, 0xB9F3FF);
        pixel(pet, 69, 22, 13, 17, 0xB9F3FF);
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
