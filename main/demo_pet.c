#include "demo.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bsp_battery.h"
#include "esp_log.h"
#include "lvgl.h"
#include "pet_model.h"
#include "pet_store.h"
#include "pet_view.h"
#include "ui_pixel.h"

typedef enum {
    PAGE_HOME = 0,
    PAGE_PROGRESS,
    PAGE_ARCHIVE,
    PAGE_COUNT,
} pet_page_t;

static const char *TAG = "demo_pet";
static pet_model_t s_model;
static pet_page_t s_page;
static lv_obj_t *s_scr;
static lv_obj_t *s_content;
static lv_obj_t *s_battery;
static lv_obj_t *s_pet;
static lv_obj_t *s_hint;
static lv_timer_t *s_timer;
static uint32_t s_last_action;
static bool s_store_ready;
static bool s_sleeping;

static uint8_t build_month(void)
{
    static const char *const MONTHS[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    for (uint8_t i = 0; i < 12U; i++) {
        if (strncmp(__DATE__, MONTHS[i], 3) == 0) return (uint8_t)(i + 1U);
    }
    return 1U;
}
static uint16_t build_year(void)
{
    return (uint16_t)((__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 +
                      (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0'));
}

static void save_model(void)
{
    if (s_store_ready && !pet_store_request_save(&s_model)) {
        ESP_LOGW(TAG, "save request dropped");
    }
}

static void set_hint(const char *text)
{
    if (s_hint) lv_label_set_text(s_hint, text);
}

static void draw_home(void)
{
    lv_obj_t *panel = ui_pixel_panel_create(s_content, 7, 4, 216, 220, UI_PAPER);
    lv_obj_t *stage_plate = lv_obj_create(panel);
    lv_obj_set_size(stage_plate, 174, 32);
    lv_obj_align(stage_plate, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_remove_flag(stage_plate, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(stage_plate, 0, 0);
    lv_obj_set_style_border_width(stage_plate, 2, 0);
    lv_obj_set_style_border_color(stage_plate, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_bg_color(stage_plate, lv_color_hex(UI_YELLOW), 0);
    lv_obj_set_style_pad_all(stage_plate, 0, 0);

    lv_obj_t *name = ui_pixel_label(stage_plate,
        pet_model_stage_name((pet_stage_t)s_model.stage),
        &lv_font_montserrat_20, UI_INK);
    lv_obj_center(name);

    /* Keep every stage, including APEX's crown, below the fixed title plate. */
    s_pet = pet_view_create(panel, (pet_stage_t)s_model.stage, 49, 48);

    lv_obj_t *stats = ui_pixel_label(panel, "", &lv_font_montserrat_14, UI_INK);
    lv_label_set_text_fmt(stats, "FOOD %u   GROW %u%%",
                          (unsigned)s_model.food_total, (unsigned)s_model.progress);
    lv_obj_align(stats, LV_ALIGN_BOTTOM_MID, 0, -29);

    s_hint = ui_pixel_label(panel, s_sleeping ? "SLEEPING..." : "OKx2: FEED",
                            &lv_font_montserrat_14,
                            s_sleeping ? UI_SKY_DARK : UI_ORANGE);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -5);
}

static void draw_progress(void)
{
    lv_obj_t *panel = ui_pixel_panel_create(s_content, 7, 4, 216, 220, UI_PAPER);
    lv_obj_t *label = ui_pixel_label(panel, "MONTHLY GROWTH",
                                     &lv_font_montserrat_14, UI_INK);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *track = lv_obj_create(panel);
    lv_obj_set_size(track, 176, 28);
    lv_obj_align(track, LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_set_style_radius(track, 0, 0);
    lv_obj_set_style_border_width(track, 3, 0);
    lv_obj_set_style_border_color(track, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_pad_all(track, 3, 0);
    lv_obj_t *fill = lv_obj_create(track);
    lv_obj_set_size(fill, (int32_t)s_model.progress * 164 / 100, 16);
    lv_obj_set_style_radius(fill, 0, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_bg_color(fill, lv_color_hex(UI_GRASS), 0);
    lv_obj_align(fill, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *details = ui_pixel_label(panel, "", &lv_font_montserrat_14, UI_INK);
    lv_label_set_text_fmt(details,
        "%04u-%02u\n\nSTAGE  %s\nROUTE  %s\nFOOD   %u\n\nOKx2: SETTLE",
        (unsigned)s_model.current_year, (unsigned)s_model.current_month,
        pet_model_stage_name((pet_stage_t)s_model.stage),
        pet_model_route_name((pet_route_t)s_model.route),
        (unsigned)s_model.food_total);
    lv_obj_set_style_text_align(details, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(details, LV_ALIGN_TOP_MID, 0, 85);
}

static void draw_archive(void)
{
    lv_obj_t *panel = ui_pixel_panel_create(s_content, 7, 4, 216, 220, UI_PAPER);
    lv_obj_t *title = ui_pixel_label(panel, "AI PET FAMILY",
                                     &lv_font_montserrat_14, UI_INK);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    if (s_model.archive_count == 0U) {
        lv_obj_t *empty = ui_pixel_label(panel,
            "NO PET YET\n\nSETTLE THIS MONTH\nTO ADD ONE",
            &lv_font_montserrat_14, UI_SKY_DARK);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(empty);
        return;
    }

    pet_archive_entry_t *entry = &s_model.archive[s_model.archive_count - 1U];
    pet_view_create(panel, (pet_stage_t)entry->stage, 49, 48);
    lv_obj_t *details = ui_pixel_label(panel, "", &lv_font_montserrat_14, UI_INK);
    lv_label_set_text_fmt(details, "%04u-%02u  %s\n%s ROUTE   #%u/12",
        (unsigned)entry->year, (unsigned)entry->month,
        pet_model_stage_name((pet_stage_t)entry->stage),
        pet_model_route_name((pet_route_t)entry->route),
        (unsigned)s_model.archive_count);
    lv_obj_set_style_text_align(details, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(details, LV_ALIGN_BOTTOM_MID, 0, -8);
}

static void draw_page(void)
{
    if (!s_content) return;
    lv_obj_clean(s_content);
    s_pet = NULL;
    s_hint = NULL;
    if (s_page == PAGE_HOME) draw_home();
    else if (s_page == PAGE_PROGRESS) draw_progress();
    else draw_archive();
}

static pet_route_t demo_route(void)
{
    if (s_model.food_total != 0U && s_model.food_total % 11U == 0U) {
        return PET_ROUTE_GLITCH;
    }
    if (s_model.food_total >= 16U) return PET_ROUTE_ARMOR;
    if (s_model.food_total >= 8U) return PET_ROUTE_EXPLORER;
    if (s_model.food_total >= 3U) return PET_ROUTE_WILD;
    return PET_ROUTE_CORE;
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    int soc = bsp_battery_soc();
    if (s_battery) {
        if (soc < 0) lv_label_set_text(s_battery, "--%");
        else lv_label_set_text_fmt(s_battery, "%d%%", soc);
    }

    bool sleeping = lv_tick_elaps(s_last_action) >= 15000U;
    if (sleeping != s_sleeping) {
        s_sleeping = sleeping;
        if (s_page == PAGE_HOME && s_hint) {
            set_hint(s_sleeping ? "SLEEPING..." : "OKx2: FEED");
        }
    }
}

void demo_pet_enter(void)
{
    pet_model_init(&s_model);
    s_store_ready = pet_store_init();
    if (!s_store_ready || !pet_store_load(&s_model)) {
        pet_model_init(&s_model);
    }

    pet_result_t month_result = pet_model_begin_month(&s_model, build_year(), build_month());
    if (month_result == PET_RESULT_OK) save_model();

    s_page = PAGE_HOME;
    s_sleeping = false;
    s_last_action = lv_tick_get();
    s_scr = ui_pixel_screen_create("AI PET");
    s_battery = ui_pixel_label(s_scr, "--%", &lv_font_montserrat_14, UI_PAPER);
    lv_obj_set_pos(s_battery, 169, 29);
    lv_obj_set_width(s_battery, 65);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);

    s_content = lv_obj_create(s_scr);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_content, 5, 51);
    lv_obj_set_size(s_content, 230, 232);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 0, 0);
    draw_page();
    tick(NULL);
    s_timer = lv_timer_create(tick, 1000, NULL);
    lv_screen_load(s_scr);
}

void demo_pet_exit(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
    s_content = s_battery = s_pet = s_hint = NULL;
}

void demo_pet_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev == BSP_BTN_CLICK && (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
        s_page = btn == BSP_BTN_UP
                     ? (pet_page_t)((s_page + PAGE_COUNT - 1) % PAGE_COUNT)
                     : (pet_page_t)((s_page + 1) % PAGE_COUNT);
        s_last_action = lv_tick_get();
        s_sleeping = false;
        draw_page();
        return;
    }

    if (btn != BSP_BTN_OK) return;
    if (ev == BSP_BTN_CLICK && s_page == PAGE_HOME) {
        s_last_action = lv_tick_get();
        s_sleeping = false;
        pet_view_bounce(s_pet);
        set_hint("HELLO!");
        return;
    }
    if (ev != BSP_BTN_DOUBLE) return;

    s_last_action = lv_tick_get();
    s_sleeping = false;
    if (s_page == PAGE_HOME) {
        pet_result_t result = pet_model_apply_feed(&s_model, s_model.last_seq + 1U, 1U);
        if (result == PET_RESULT_OK) {
            save_model();
            draw_page();
            set_hint("YUM! +1 FOOD");
            pet_view_bounce(s_pet);
        } else if (result == PET_RESULT_ALREADY_SETTLED) {
            set_hint("MONTH SETTLED");
        }
    } else if (s_page == PAGE_PROGRESS) {
        pet_result_t result = pet_model_settle(
            &s_model, s_model.last_seq + 1U, s_model.current_year,
            s_model.current_month, s_model.progress, demo_route());
        if (result == PET_RESULT_OK) {
            save_model();
            s_page = PAGE_ARCHIVE;
            draw_page();
        }
    }
}
