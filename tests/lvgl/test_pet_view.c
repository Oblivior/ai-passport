#include <assert.h>
#include <stdio.h>
#include "pet_view.h"
#include "../../assets/images/digimon_sprites.h"
#include "pet_catalog.h"

static uint16_t framebuffer[240 * 320];

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    uint16_t *src = (uint16_t *)pixels;
    for (int y = area->y1; y <= area->y2; y++)
        for (int x = area->x1; x <= area->x2; x++) framebuffer[y * 240 + x] = *src++;
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
    static _Alignas(64) uint8_t buffer[240 * 20 * 2];
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
    for (unsigned line = 0; line <= PET_CATALOG_COUNT; line++)
    for (int stage = 0; stage < PET_STAGE_COUNT; stage++) {
        bool dark = line == PET_CATALOG_COUNT;
        if (dark && stage < PET_STAGE_TITAN) continue;
        for (int pose = PET_POSE_IDLE; pose <= PET_POSE_EVOLVE; pose++) {
            lv_obj_t *pet = pet_view_create_branch_pose(panel, dark ? PET_AGUMON : pet_catalog_at(line)->id,
                (pet_stage_t)stage, dark, 49, 48, (pet_pose_t)pose);
            lv_obj_update_layout(pet);
            lv_refr_now(NULL);
            if (pose == PET_POSE_IDLE) {
                /* Geometry alone missed an indexed-image transform defect.
                 * Compare every opaque source pixel at its actual 3x center. */
                lv_area_t pos;
                lv_obj_get_coords(pet, &pos);
                const uint8_t *data = dark ? digimon_branch_sprites[stage - PET_STAGE_TITAN][0].data :
                    digimon_sprites[pet_catalog_at(line)->artwork][stage][0].data;
                for (unsigned y = 0; y < 28; y++) for (unsigned x = 0; x < 32; x++) {
                    unsigned at = y * 32 + x;
                    if (!data[32 * 28 * 2 + at]) continue;
                    uint16_t expected = data[at * 2] | (uint16_t)data[at * 2 + 1] << 8;
                    unsigned fx = pos.x1 + 2 + x * 3 + 1, fy = pos.y1 + 5 + y * 3 + 1;
                    assert(fx < 240 && fy < 320);
                    if (framebuffer[fy * 240 + fx] != expected) {
                        fprintf(stderr, "sprite mismatch stage=%d source=%u,%u\n", stage, x, y);
                        assert(false);
                    }
                }
            }
            for (unsigned frame = 0; frame < 30; frame++) {
                pet_view_frame(pet, (pet_pose_t)pose, frame);
                lv_tick_inc(150);
                lv_timer_handler();
            }
            pet_view_bounce(pet);
            lv_obj_delete(pet);
            lv_tick_inc(400);
            lv_timer_handler();
        }
    }
    lv_display_delete(display);
    puts("pet_view LVGL 9.5.0 regression: PASS (legacy + 21 Digimon, 5 poses, 3x pixel comparison, teardown)");
    return 0;
}
