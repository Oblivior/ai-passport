#include <assert.h>
#include <stdio.h>
#include "pet_view.h"

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}

static void advance(lv_obj_t *pet, unsigned milliseconds)
{
    for (unsigned i = 0; i < milliseconds; i += 10) {
        lv_tick_inc(10);
        lv_timer_handler();
        lv_obj_update_layout(pet);
        /* Header is y=0..31; the whole animation must stay in y=40..48. */
        assert(lv_obj_get_y(pet) >= 40);
        assert(lv_obj_get_y(pet) <= 48);
    }
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(240, 320);
    static uint8_t buffer[240 * 20 * 2];
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_obj_t *panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(panel, 216, 220);
    lv_obj_set_style_pad_all(panel, 7, 0);
    lv_obj_set_style_border_width(panel, 4, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_update_layout(panel);

    for (int stage = 0; stage < PET_STAGE_COUNT; stage++) {
        lv_obj_t *pet = pet_view_create(panel, (pet_stage_t)stage, 49, 48);
        printf("stage=%d before layout: requested_y=%ld measured_y=%ld\n",
               stage, (long)lv_obj_get_style_y(pet, 0), (long)lv_obj_get_y(pet));
        fflush(stdout);
        /* This is the feed handler's exact order: create -> bounce -> layout. */
        pet_view_bounce(pet);
        advance(pet, 400);
        assert(lv_obj_get_y(pet) == 48);
        for (unsigned click = 0; click < 20; click++) {
            pet_view_bounce(pet);
            advance(pet, 140); /* Interrupt while the character is airborne. */
        }
        advance(pet, 400);
        assert(lv_obj_get_y(pet) == 48);
        pet_view_bounce(pet);
        lv_obj_delete(pet); /* Rebuilding a feeding page must cancel animation. */
        lv_tick_inc(400);
        lv_timer_handler();
    }
    lv_display_delete(display);
    puts("pet_view LVGL 9.5.0 regression: PASS (7 stages, early bounce, 20 interruptions, deletion)");
    return 0;
}
