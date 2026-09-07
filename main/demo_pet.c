#include "demo.h"
#include "pet_service.h"
#include "pet_view.h"
#include "ui_pixel.h"
#include "bsp_battery.h"
#include "esp_timer.h"

typedef enum { PAGE_HOME, PAGE_PROGRESS, PAGE_ROUTE, PAGE_ARCHIVE, PAGE_COUNT } pet_page_t;
static pet_snapshot_t s_state;
static pet_page_t s_page;
static pet_pose_t s_pose;
static lv_obj_t *s_scr, *s_content, *s_battery, *s_pet;
static lv_timer_t *s_timer;
static uint32_t s_action_at, s_pose_at, s_frame;
static unsigned s_archive;
static unsigned s_route_preview;
static uint8_t s_before_stage;
static bool s_eat_requested;

static lv_obj_t *label(lv_obj_t *parent, const char *text, int y, uint32_t color)
{
    lv_obj_t *obj = ui_pixel_label(parent, text, &lv_font_montserrat_14, color);
    lv_obj_set_width(obj, 192);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, y);
    return obj;
}

static const char *home_hint(void)
{
    if (!s_state.ready) return "WAKING UP...";
    if (!s_state.storage_ok) return "SAVE ERROR - RETRY";
    if (s_pose == PET_POSE_EAT) return "MUNCH MUNCH...";
    if (s_pose == PET_POSE_EVOLVE) return "EVOLVING...";
    if (s_pose == PET_POSE_HAPPY) return "HAPPY TO SEE YOU!";
    if (pet_life_pending(&s_state.life)) return "OK: OPEN LUNCHBOX";
    if (!s_state.life.date) return "CONNECT USB TO HATCH";
    if (s_pose == PET_POSE_SLEEP) return "RESTING - OK: WAKE";
    return "OK: PET YOUR BUDDY";
}

static void draw_home(void)
{
    lv_obj_t *panel = ui_pixel_panel_create(s_content, 7, 4, 216, 220, UI_PAPER);
    lv_obj_t *plate = ui_pixel_panel_create(panel, 11, 0, 174, 32, UI_YELLOW);
    lv_obj_set_style_pad_all(plate, 0, 0);
    lv_obj_set_style_border_width(plate, 2, 0);
    uint8_t stage = s_pose == PET_POSE_EVOLVE || s_pose == PET_POSE_EAT ? s_before_stage : s_state.life.stage;
    pet_route_t route = pet_life_route_locked(&s_state.life) ? pet_life_route(&s_state.life) : PET_ROUTE_CORE;
    lv_obj_t *name = ui_pixel_label(plate, pet_model_stage_name(stage),
        stage >= PET_STAGE_RANGER ? &lv_font_montserrat_14 : &lv_font_montserrat_20, UI_INK);
    if (stage >= PET_STAGE_RANGER) lv_label_set_text_fmt(name, "%s / %s",
        pet_model_stage_name(stage), pet_model_route_name(route));
    lv_obj_center(name);
    s_pet = pet_view_create_route_pose(panel, stage, route, 49, 40, s_pose);
    lv_obj_t *stats = label(panel, "", 140, UI_INK);
    lv_label_set_text_fmt(stats, "LUNCH %u    DAYS %u", pet_life_pending(&s_state.life), pet_life_days(&s_state.life));
    unsigned pending = pet_life_pending(&s_state.life);
    lv_obj_t *tray = lv_obj_create(panel);
    lv_obj_set_size(tray, 112, 12);
    lv_obj_align(tray, LV_ALIGN_TOP_MID, 0, 165);
    lv_obj_set_style_pad_all(tray, 0, 0);
    lv_obj_set_style_radius(tray, 2, 0);
    lv_obj_set_style_border_width(tray, 2, 0);
    lv_obj_set_style_border_color(tray, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_bg_color(tray, lv_color_hex(pending ? UI_ORANGE : UI_PAPER), 0);
    lv_obj_remove_flag(tray, LV_OBJ_FLAG_SCROLLABLE);
    label(panel, home_hint(), 180, pending ? UI_SKY_DARK : UI_INK);
}

static void draw_progress(void)
{
    lv_obj_t *panel = ui_pixel_panel_create(s_content, 7, 4, 216, 220, UI_PAPER);
    label(panel, s_state.life.stage == PET_STAGE_APEX ? "FINAL FORM" : "NEXT EVOLUTION", 0, UI_INK);
    const pet_life_t *life = &s_state.life;
    label(panel, pet_model_stage_name(life->stage < PET_STAGE_APEX ? life->stage + 1 : PET_STAGE_APEX), 25, UI_ORANGE);
    lv_obj_t *stats = label(panel, "", 55, UI_INK);
    lv_label_set_text_fmt(stats, "MEALS   %u / %u\n\nACTIVE DAYS   %u / %u",
        pet_life_meals(life), pet_life_next_meals(life), pet_life_days(life), pet_life_next_days(life));
    lv_obj_t *date = label(panel, "", 123, UI_SKY_DARK);
    if (life->date) lv_label_set_text_fmt(date, "%04lu-%02lu-%02lu\nKABOO / LOCAL",
        (unsigned long)(life->date / 10000), (unsigned long)(life->date / 100 % 100), (unsigned long)(life->date % 100));
    else lv_label_set_text(date, "WAITING FOR FIRST SYNC");
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    label(panel, !s_state.synced_at || now - s_state.synced_at > 600000U ? "SYNC NEEDED" :
        (s_state.synced_wirelessly ? "WIRELESS SYNC OK" : "USB SYNC OK"), 166, UI_INK);
    label(panel, "REST WITHOUT LOSS", 180, UI_SKY_DARK);
}

static void draw_route(void)
{
    lv_obj_t *panel = ui_pixel_panel_create(s_content, 7, 4, 216, 220, UI_PAPER);
    pet_route_t route = s_route_preview ? (pet_route_t)s_route_preview : pet_life_route(&s_state.life);
    label(panel, pet_model_route_name(route), 0, UI_INK);
    label(panel, s_route_preview ? "PREVIEW ONLY" : pet_life_route_locked(&s_state.life) ?
        "YOUR LOCKED ROUTE" : "YOUR TENDENCY", 22, UI_SKY_DARK);
    pet_stage_t stage = s_route_preview ? PET_STAGE_APEX : s_state.life.stage >= PET_STAGE_RANGER ?
        s_state.life.stage : PET_STAGE_RANGER;
    pet_view_create_route_pose(panel, stage, route, 49, 42, PET_POSE_IDLE);
    label(panel, pet_life_route_hint(route), 140, UI_INK);
    label(panel, s_route_preview ? "SAME GROWTH LIMITS" : pet_life_route_locked(&s_state.life) ?
        "KEPT IN YOUR FAMILY" : "LOCKS AT RANGER", 158, UI_SKY_DARK);
    label(panel, "OK: PREVIEW ROUTES", 180, UI_INK);
}

static void draw_archive(void)
{
    lv_obj_t *panel = ui_pixel_panel_create(s_content, 7, 4, 216, 220, UI_PAPER);
    label(panel, "MY PET FAMILY", 0, UI_INK);
    unsigned count = s_state.life.family.archive_count;
    if (!count) {
        label(panel, "YOUR FIRST CHAPTER\nIS STILL GROWING\n\nCOME BACK NEXT MONTH", 60, UI_SKY_DARK);
        return;
    }
    s_archive %= count;
    const pet_archive_entry_t *entry = &s_state.life.family.archive[s_archive];
    pet_view_create_route_pose(panel, entry->stage, entry->route, 49, 28, PET_POSE_IDLE);
    lv_obj_t *details = label(panel, "", 126, UI_INK);
    lv_label_set_text_fmt(details, "%04u-%02u  %s\n%s\n%s  %u/%u", entry->year, entry->month,
        pet_model_stage_name(entry->stage), pet_model_route_name(entry->route),
        s_state.life.legacy_mask & (1U << s_archive) ? "DEMO MEMORY" : "LOCAL CHAPTER",
        s_archive + 1, count);
    label(panel, "OK: NEXT MEMORY", 180, UI_SKY_DARK);
}

static void draw_page(void)
{
    if (!s_content) return;
    lv_obj_clean(s_content);
    s_pet = NULL;
    if (s_page == PAGE_HOME) draw_home();
    else if (s_page == PAGE_PROGRESS) draw_progress();
    else if (s_page == PAGE_ROUTE) draw_route();
    else draw_archive();
}

static void pose(pet_pose_t next)
{
    s_pose = next;
    s_pose_at = lv_tick_get();
    if (s_page == PAGE_HOME) draw_page();
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    pet_snapshot_t next;
    if (pet_service_snapshot(&next)) {
        bool changed = next.revision != s_state.revision || next.storage_ok != s_state.storage_ok;
        bool ate = pet_life_meals(&next.life) > pet_life_meals(&s_state.life) && next.life.date / 100 == s_state.life.date / 100;
        bool delivery = pet_life_pending(&next.life) > pet_life_pending(&s_state.life);
        s_before_stage = ate ? s_state.life.stage : s_before_stage;
        s_state = next;
        if (ate) {
            s_eat_requested = false;
            s_action_at = lv_tick_get();
            pose(PET_POSE_EAT);
        } else if (delivery && s_pose == PET_POSE_SLEEP) pose(PET_POSE_IDLE);
        else if (changed && s_pose != PET_POSE_EAT && s_pose != PET_POSE_EVOLVE) draw_page();
    }
    uint32_t elapsed = lv_tick_elaps(s_pose_at);
    if (s_pose == PET_POSE_EAT && elapsed >= 1200) {
        pose(s_before_stage != s_state.life.stage ? PET_POSE_EVOLVE : PET_POSE_HAPPY);
    } else if (s_pose == PET_POSE_EVOLVE && elapsed >= 1800) {
        pose(PET_POSE_HAPPY);
    } else if (s_pose == PET_POSE_HAPPY && elapsed >= 1600) {
        pose(PET_POSE_IDLE);
    } else if (s_pose == PET_POSE_IDLE && lv_tick_elaps(s_action_at) >= 30000 && !pet_life_pending(&s_state.life)) {
        pose(PET_POSE_SLEEP);
    }
    if (s_eat_requested && lv_tick_elaps(s_action_at) >= 2000) s_eat_requested = false;
    pet_view_frame(s_pet, s_pose, s_frame++);
    if (s_frame % 10 == 0) {
        if (s_page == PAGE_PROGRESS) draw_page();
        int soc = bsp_battery_soc();
        if (soc < 0) lv_label_set_text(s_battery, "--%");
        else lv_label_set_text_fmt(s_battery, "%d%%", soc);
    }
}

void demo_pet_enter(void)
{
    pet_life_init(&s_state.life, NULL);
    s_state.ready = false;
    pet_service_snapshot(&s_state);
    s_page = PAGE_HOME;
    s_pose = PET_POSE_IDLE;
    s_action_at = s_pose_at = lv_tick_get();
    s_eat_requested = false;
    s_route_preview = 0;
    s_archive = s_state.life.family.archive_count ? s_state.life.family.archive_count - 1 : 0;
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
    s_timer = lv_timer_create(tick, 150, NULL);
    lv_screen_load(s_scr);
}

void demo_pet_exit(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; }
    s_content = s_battery = s_pet = NULL;
}

void demo_pet_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;
    s_action_at = lv_tick_get();
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        s_page = btn == BSP_BTN_UP ? (s_page + PAGE_COUNT - 1) % PAGE_COUNT : (s_page + 1) % PAGE_COUNT;
        s_route_preview = 0;
        draw_page();
    } else if (btn == BSP_BTN_OK && s_page == PAGE_HOME) {
        if (s_pose == PET_POSE_EAT || s_pose == PET_POSE_EVOLVE || s_eat_requested) return;
        if (pet_life_pending(&s_state.life)) s_eat_requested = pet_service_eat();
        else pose(PET_POSE_HAPPY);
    } else if (btn == BSP_BTN_OK && s_page == PAGE_ROUTE) {
        s_route_preview = (s_route_preview + 1) % 4;
        draw_page();
    } else if (btn == BSP_BTN_OK && s_page == PAGE_ARCHIVE) {
        s_archive++;
        draw_page();
    }
}
