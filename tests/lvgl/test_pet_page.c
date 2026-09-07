#include "demo.h"
#include "pet_service.h"
#include "pet_ble.h"
#include "lvgl.h"
#include "src/misc/lv_text_private.h"
#include "pet_ui_text.h"
#include "ui_pixel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static pet_snapshot_t snapshot;
static bool fail_ui_save;
static pet_ble_status_t ble_status;
void pet_ble_status(pet_ble_status_t *out) { *out = ble_status; }
static uint16_t framebuffer[240 * 320];

bool pet_service_snapshot(pet_snapshot_t *out) { *out = snapshot; return true; }
bool pet_service_start(void) { return true; }
bool pet_service_choose(const pet_snapshot_t *shown, unsigned id)
{
    assert(shown->house.active_id == snapshot.house.active_id);
    if (fail_ui_save) { snapshot.storage_ok = false; return true; }
    bool chosen = pet_house_choose(&snapshot.house, id);
    if (chosen) { snapshot.revision++; snapshot.storage_ok = true; }
    return chosen;
}
bool pet_service_eat(const pet_snapshot_t *shown)
{
    assert(shown->house.active_id == snapshot.house.active_id);
    bool ate = pet_house_eat(&snapshot.house);
    if (ate) snapshot.revision++;
    return ate;
}
int bsp_battery_soc(void) { return 100; }
int64_t esp_timer_get_time(void) { return (int64_t)lv_tick_get() * 1000; }

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    uint16_t *src = (uint16_t *)pixels;
    for (int y = area->y1; y <= area->y2; y++)
        for (int x = area->x1; x <= area->x2; x++) framebuffer[y * 240 + x] = *src++;
    lv_display_flush_ready(display);
}

static void advance(unsigned milliseconds)
{
    for (unsigned n = 0; n < milliseconds; n += 10) {
        lv_tick_inc(10);
        lv_timer_handler();
    }
}

static void bounds(lv_obj_t *obj)
{
    if (lv_obj_check_type(obj, &lv_label_class)) {
        const char *text = lv_label_get_text(obj);
        const lv_font_t *font = lv_obj_get_style_text_font(obj, 0);
        uint32_t offset = 0, codepoint;
        while ((codepoint = lv_text_encoded_next(text, &offset))) {
            if (codepoint == '\n') continue;
            lv_font_glyph_dsc_t glyph = {0};
            if (!font->get_glyph_dsc(font, &glyph, codepoint, 0)) {
                fprintf(stderr, "missing glyph U+%04X in %s\n", (unsigned)codepoint, text);
                assert(false);
            }
        }
        lv_area_t a, p;
        lv_obj_get_coords(obj, &a);
        lv_obj_get_coords(lv_obj_get_parent(obj), &p);
        if (a.x1 < p.x1 || a.y1 < p.y1 || a.x2 > p.x2 || a.y2 > p.y2) {
            fprintf(stderr, "clipped label %s: %d,%d..%d,%d parent %d,%d..%d,%d\n",
                    lv_label_get_text(obj), (int)a.x1, (int)a.y1, (int)a.x2, (int)a.y2,
                    (int)p.x1, (int)p.y1, (int)p.x2, (int)p.y2);
            assert(false);
        }
        lv_obj_t *parent = lv_obj_get_parent(obj);
        for (unsigned i = 0; i < lv_obj_get_child_count(parent); i++) {
            lv_obj_t *other = lv_obj_get_child(parent, i);
            bool pet = lv_obj_get_width(other) == 100 && lv_obj_get_height(other) == 94;
            if (other == obj || (!lv_obj_check_type(other, &lv_label_class) &&
                !lv_obj_check_type(other, &lv_bar_class) && !pet)) continue;
            lv_area_t b;
            lv_obj_get_coords(other, &b);
            assert(a.x2 < b.x1 || b.x2 < a.x1 || a.y2 < b.y1 || b.y2 < a.y1);
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); i++) bounds(lv_obj_get_child(obj, i));
}

static unsigned footer_count(lv_obj_t *obj)
{
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    unsigned count = lv_obj_check_type(obj, &lv_label_class) && area.y1 == 293 ? 1 : 0;
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); i++) count += footer_count(lv_obj_get_child(obj, i));
    return count;
}

static void capture(const char *name)
{
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(NULL);
    bounds(lv_screen_active());
    assert(footer_count(lv_screen_active()) == 1);
    char path[80];
    snprintf(path, sizeof(path), "pet-%s.ppm", name);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n240 320\n255\n");
    for (unsigned i = 0; i < 240 * 320; i++) {
        uint16_t c = framebuffer[i];
        uint8_t rgb[] = {(uint8_t)((c >> 11) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
}

static bool has_text(lv_obj_t *obj, const char *text)
{
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), text)) return true;
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); i++)
        if (has_text(lv_obj_get_child(obj, i), text)) return true;
    return false;
}

static void set_stage(unsigned id, unsigned stage)
{
    pet_house_init(&snapshot.house, NULL);
    assert(pet_house_choose(&snapshot.house, id));
    pet_usage_t usage = {.date = 20260901, .daily_goal = 1000};
    unsigned days = pet_catalog_days(stage), meals = pet_catalog_meals(stage);
    for (unsigned day = 1; day <= (days ? days : 1); day++) {
        usage.date = 20260900 + day;
        unsigned food = day == days ? meals : meals / (days - day + 1);
        usage.earned[day - 1] = food;
        meals -= food;
        assert(pet_house_sync(&snapshot.house, &usage));
        while (pet_house_eat(&snapshot.house)) {}
    }
    assert(pet_house_valid(&snapshot.house));
    assert(pet_house_stage(&snapshot.house, id) == stage);
    snapshot.revision++;
}

static lv_obj_t *find_bar(lv_obj_t *obj)
{
    if (lv_obj_check_type(obj, &lv_bar_class)) return obj;
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t *found = find_bar(lv_obj_get_child(obj, i));
        if (found) return found;
    }
    return NULL;
}

static void progress_values(unsigned meal_percent, unsigned day_percent)
{
    lv_obj_t *bar = find_bar(lv_screen_active());
    assert(bar);
    lv_obj_t *panel = lv_obj_get_parent(bar);
    unsigned found = 0;
    for (unsigned i = 0; i < lv_obj_get_child_count(panel); i++) {
        lv_obj_t *child = lv_obj_get_child(panel, i);
        if (!lv_obj_check_type(child, &lv_bar_class)) continue;
        assert(lv_bar_get_value(child) == (int)(found ? day_percent : meal_percent));
        found++;
    }
    assert(found == 2);
}

static void home_geometry(unsigned percent)
{
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *bar = find_bar(lv_screen_active());
    assert(bar && lv_bar_get_value(bar) == (int)percent);
    lv_obj_t *panel = lv_obj_get_parent(bar), *pet = NULL;
    for (unsigned i = 0; i < lv_obj_get_child_count(panel); i++) {
        lv_obj_t *child = lv_obj_get_child(panel, i);
        if (lv_obj_get_width(child) == 100 && lv_obj_get_height(child) == 94) pet = child;
    }
    assert(pet);
    lv_area_t a;
    lv_obj_get_coords(pet, &a);
    for (unsigned i = 0; i < lv_obj_get_child_count(panel); i++) {
        lv_obj_t *child = lv_obj_get_child(panel, i);
        if (child == pet) continue;
        lv_area_t b;
        lv_obj_get_coords(child, &b);
        assert(a.x2 < b.x1 || b.x2 < a.x1 || a.y2 < b.y1 || b.y2 < a.y1);
    }
}

static void home_progress_scenarios(void)
{
    /* Match the user's food-rich, one-companion-day black-ball pet. */
    set_stage(PET_AGUMON, PET_STAGE_SPARK);
    pet_usage_t usage = {.date = 20260901, .daily_goal = 1000, .earned = {5}};
    assert(pet_house_sync(&snapshot.house, &usage));
    while (pet_house_eat(&snapshot.house)) {}
    snapshot.revision++;
    pet_house_t saved = snapshot.house;
    lv_screen_load(lv_obj_create(NULL));
    demo_pet_enter(); advance(1800);
    capture("home-level-user");
    assert(has_text(lv_screen_active(), "Lv.1"));
    assert(has_text(lv_screen_active(), "进化 50%"));
    assert(has_text(lv_screen_active(), "还差 1 天陪伴"));
    home_geometry(50);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    for (unsigned i = 0; i < 10; i++) { advance(150); home_geometry(50); }
    capture("home-level-happy");
    advance(33000); capture("home-level-sleep"); home_geometry(50);
    assert(!memcmp(&saved, &snapshot.house, sizeof(saved)));
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK); advance(300);
    assert(!has_text(lv_screen_active(), "睡觉中，按确定唤醒"));
    capture("home-footer-cleared");
    demo_pet_exit();
    for (unsigned id = PET_AGUMON; id <= PET_PATAMON; id++) {
        for (unsigned stage = PET_STAGE_EGG; stage < PET_STAGE_COUNT; stage++) {
            set_stage(id, stage);
            saved = snapshot.house;
            unsigned percent = pet_ui_progress(stage, pet_house_meals(&saved, id), pet_house_days(&saved, id)).percent;
            lv_screen_load(lv_obj_create(NULL)); demo_pet_enter(); advance(300);
            home_geometry(percent);
            demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
            for (unsigned i = 0; i < 10; i++) { advance(150); home_geometry(percent); }
            assert(!memcmp(&saved, &snapshot.house, sizeof(saved)));
            demo_pet_exit();
        }
    }
}

static void partner_scenarios(void)
{
    set_stage(PET_AGUMON, PET_STAGE_SCOUT);
    pet_usage_t usage = {.date = 20260904, .daily_goal = 1000};
    usage.earned[3] = 5;
    assert(pet_house_sync(&snapshot.house, &usage));
    snapshot.revision++;
    lv_screen_load(lv_obj_create(NULL));
    demo_pet_enter(); advance(200);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(200);
    assert(has_text(lv_screen_active(), "啊呜啊呜，真香！"));
    for (unsigned i = 0; i < 4; i++) demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); /* Enter picker during eating. */
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK); advance(200);
    capture("partner-gabumon-new");
    assert(has_text(lv_screen_active(), "加布兽 · 伙伴 2/3"));
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(200);
    capture("partner-gabumon-confirm");
    assert(snapshot.house.active_id == PET_AGUMON);
    demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK); /* Cancel without a save. */
    assert(!pet_house_partner(&snapshot.house, PET_GABUMON)->adopted);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(200);
    capture("partner-switched-egg");
    assert(snapshot.house.active_id == PET_GABUMON);
    assert(has_text(lv_screen_active(), "数码蛋") && !has_text(lv_screen_active(), "啊呜啊呜，真香！"));
    assert(pet_house_meals(&snapshot.house, PET_AGUMON) == 7);
    assert(pet_house_meals(&snapshot.house, PET_GABUMON) == 0 && pet_life_pending(&snapshot.house.life) == 4);
    /* Back to the original partner: neither reset nor a false evolution. */
    demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK); demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK);
    advance(200); capture("partner-agumon-resting");
    assert(has_text(lv_screen_active(), "在家休息 · 陪伴 4 天"));
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(200);
    assert(snapshot.house.active_id == PET_AGUMON && pet_house_meals(&snapshot.house, PET_AGUMON) == 7);
    assert(!has_text(lv_screen_active(), "要进化啦！"));
    /* Failed adoption retains the partner and allows a visible retry. */
    demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK); demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK); demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    fail_ui_save = true;
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(2300);
    capture("partner-save-failed");
    assert(snapshot.house.active_id == PET_AGUMON && !pet_house_partner(&snapshot.house, PET_PATAMON)->adopted);
    assert(has_text(lv_screen_active(), "存档失败，请重试"));
    fail_ui_save = false;
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(200);
    assert(snapshot.house.active_id == PET_PATAMON && snapshot.storage_ok);
    assert(pet_life_pending(&snapshot.house.life) == 4);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(5200);
    assert(pet_house_meals(&snapshot.house, PET_PATAMON) == 1);
    assert(has_text(lv_screen_active(), "浮游兽"));
    usage = (pet_usage_t){.date = 20261001, .daily_goal = 1000, .earned = {1}};
    assert(pet_house_sync(&snapshot.house, &usage)); snapshot.revision++; advance(200);
    capture("partner-new-month");
    assert(!snapshot.house.active_id && snapshot.house.archive_count == 3);
    assert(has_text(lv_screen_active(), "伙伴之家"));
    /* Return without adopting, then reopen; past catalog discoveries survive. */
    for (unsigned i = 0; i < PET_CATALOG_COUNT; i++) demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(200); capture("partner-return");
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(200);
    assert(has_text(lv_screen_active(), "按确定：挑选伙伴"));
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(200);
    assert(snapshot.house.active_id == PET_AGUMON && pet_house_stage(&snapshot.house, PET_AGUMON) == 0);
    for (unsigned i = 0; i < 3; i++) demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    for (unsigned i = 0; i < 3; i++) demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    advance(200); capture("partner-lifetime-catalog");
    assert(has_text(lv_screen_active(), "已经养成 · 已解锁"));
    pet_house_t before = snapshot.house;
    for (unsigned i = 0; i < PET_STAGE_COUNT; i++) { demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(200); }
    assert(!memcmp(&before, &snapshot.house, sizeof(before)));
    demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK); /* Progress, then go back through home to archive. */
    demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK); demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK);
    demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK);
    for (unsigned i = 0; i < 3; i++) {
        advance(200); char name[32]; snprintf(name, sizeof(name), "partner-archive-%u", i); capture(name);
        demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    }
    assert(pet_house_valid(&snapshot.house));
    demo_pet_exit();
}

static void archive_key_scenarios(void)
{
    set_stage(PET_AGUMON, PET_STAGE_SPARK);
    snapshot.house.archive_count = 0;
    lv_screen_load(lv_obj_create(NULL)); demo_pet_enter(); advance(300);
    pet_house_t before = snapshot.house;
    demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK); advance(300);
    capture("archive-empty-return");
    assert(has_text(lv_screen_active(), "按确定：回到伙伴身边"));
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(300);
    assert(has_text(lv_screen_active(), "Lv.1"));
    assert(!memcmp(&before, &snapshot.house, sizeof(before)));
    demo_pet_exit();
    for (unsigned count = 1; count <= 3; count++) {
        snapshot.house.archive_count = count;
        for (unsigned i = 0; i < count; i++) snapshot.house.archive[i] = (pet_house_archive_t){
            .species_id = i + 1, .result = {.year = 2026, .month = 8, .stage = PET_STAGE_APEX}};
        before = snapshot.house;
        lv_screen_load(lv_obj_create(NULL)); demo_pet_enter();
        demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK); advance(300);
        char name[32]; snprintf(name, sizeof(name), "archive-count-%u", count); capture(name);
        assert(has_text(lv_screen_active(), count == 1 ? "仅此一只 · 确定返回" : "按确定：下一只伙伴"));
        for (unsigned press = 0; press <= count; press++) {
            demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK); advance(300);
            if (count == 1) { assert(has_text(lv_screen_active(), "Lv.1")); break; }
            char expected[150];
            snprintf(expected, sizeof(expected), "2026-08  %s\n究极体\n本地成长记录  %u/%u",
                pet_catalog_form(press % count + 1, PET_STAGE_APEX), press % count + 1, count);
            assert(has_text(lv_screen_active(), expected));
            capture("archive-cycle");
        }
        assert(!memcmp(&before, &snapshot.house, sizeof(before)));
        demo_pet_exit();
    }
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(240, 320);
    static _Alignas(64) uint8_t buffer[240 * 20 * 2];
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    assert(strcmp(pet_model_stage_name(PET_STAGE_EGG), "EGG") == 0);
    assert(strcmp(pet_ui_stage_name(PET_STAGE_EGG), "数码蛋") == 0);
    assert(strcmp(pet_ui_route_name(PET_ROUTE_ARMOR), "装甲") == 0);
    assert(strcmp(pet_ui_stage_name(PET_STAGE_SCOUT), "亚古兽") == 0);
    assert(strcmp(pet_ui_stage_level(PET_STAGE_TITAN), "完全体") == 0);
    assert(strcmp(pet_ui_stage_name(PET_STAGE_APEX), "战斗暴龙兽") == 0);
    assert(strcmp(pet_ui_legacy_stage_name(PET_STAGE_SPARK), "小火苗") == 0);
    pet_model_t old;
    pet_model_init(&old);
    pet_model_begin_month(&old, 2026, 9);
    for (unsigned i = 1; i <= 4; i++) pet_model_apply_feed(&old, i, 5);
    pet_life_t legacy;
    pet_life_init(&legacy, &old);
    pet_house_init(&snapshot.house, &legacy);
    snapshot.ready = snapshot.storage_ok = true;
    snapshot.revision = 1;
    demo_pet_enter();
    advance(300);
    capture("waiting");
    assert(has_text(lv_screen_active(), "尚未领养 · 从数码蛋开始"));
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    advance(200);
    capture("adopt-confirm");
    assert(!snapshot.house.active_id); /* Preview and confirmation do not adopt. */
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    advance(200);
    assert(snapshot.house.active_id == PET_AGUMON);
    capture("first-egg");
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(200);
    capture("lunch-empty");
    assert(has_text(lv_screen_active(), "等待第一餐"));
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    pet_usage_t usage = {.date = 20260907, .daily_goal = 1000, .tokens_today = 300};
    usage.earned[6] = 2;
    assert(pet_house_sync(&snapshot.house, &usage));
    snapshot.revision++;
    snapshot.synced_at = lv_tick_get();
    advance(300);
    capture("lunch");
    assert(has_text(lv_screen_active(), "饭盒送到啦，按确定开饭"));
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(200);
    capture("lunch-box");
    assert(has_text(lv_screen_active(), "下份还差 101"));
    assert(has_text(lv_screen_active(), "本日 2/5 份 · 已吃 0"));
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    advance(4200);
    assert(!has_text(lv_screen_active(), "饭盒送到啦，按确定开饭"));
    snapshot.revision++; /* Repeated sync must not announce duplicate food. */
    snapshot.synced_at = lv_tick_get();
    advance(300);
    assert(!has_text(lv_screen_active(), "饭盒送到啦，按确定开饭"));
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    advance(300);
    capture("eating");
    assert(has_text(lv_screen_active(), "Lv.0"));
    assert(has_text(lv_screen_active(), "进化准备就绪！"));
    for (int i = 0; i < 20; i++) demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(pet_life_meals(&snapshot.house.life) == 1);
    advance(1200);
    capture("evolving");
    assert(has_text(lv_screen_active(), "Lv.0"));
    advance(3600);
    capture("spark");
    assert(has_text(lv_screen_active(), "Lv.1"));
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    advance(5000);
    assert(pet_life_meals(&snapshot.house.life) == 2);
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    advance(200);
    capture("happy");
    advance(33000);
    capture("sleep");
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(200);
    capture("lunch-eaten");
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(200);
    capture("progress");
    progress_values(66, 50);
    snapshot.synced_wirelessly = true;
    snapshot.synced_at = lv_tick_get();
    snapshot.revision++;
    advance(200);
    capture("wireless-progress");
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(200);
    capture("digimon-lineage");
    assert(has_text(lv_screen_active(), "亚古兽进化图鉴"));
    pet_house_t before_preview = snapshot.house;
    for (unsigned i = 0; i < PET_STAGE_COUNT; i++) {
        demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
        advance(200);
        char preview_name[32];
        snprintf(preview_name, sizeof(preview_name), "digimon-lineage-%u", i);
        capture(preview_name);
        pet_stage_t preview = (snapshot.house.life.stage + i + 1) % PET_STAGE_COUNT;
        assert(has_text(lv_screen_active(), pet_ui_stage_name(preview)));
        assert(has_text(lv_screen_active(), preview <= snapshot.house.life.stage ? "已经养成 · 已解锁" : "未来形态预览"));
    }
    assert(!memcmp(&before_preview, &snapshot.house, sizeof(before_preview)));
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(200);
    capture("family");
    for (int i = 0; i < 100; i++) {
        demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
        demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        advance(200);
    }
    assert(pet_life_meals(&snapshot.house.life) == 2);
    for (unsigned day = 8; day <= 22; day++) {
        usage.date = 20260900 + day;
        usage.earned[day - 1] = 5;
        assert(pet_house_sync(&snapshot.house, &usage));
        while (pet_house_eat(&snapshot.house)) {}
    }
    assert(snapshot.house.life.stage == PET_STAGE_APEX);
    snapshot.revision++;
    advance(6000);
    demo_pet_exit();
    lv_screen_load(lv_obj_create(NULL));
    demo_pet_enter();
    advance(200);
    capture("apex");
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    advance(200);
    capture("apex-happy");
    demo_pet_exit();
    /* Every stage and its next-stage label must fit with real CJK metrics. */
    for (unsigned index = 0; index < PET_CATALOG_COUNT; index++)
    for (unsigned stage = 0; stage < PET_STAGE_COUNT; stage++) {
        set_stage(pet_catalog_at(index)->id, stage);
        lv_screen_load(lv_obj_create(NULL));
        demo_pet_enter();
        advance(300);
        char name[32];
        snprintf(name, sizeof(name), "line-%u-stage-%u", index, stage);
        capture(name);
        demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        advance(300);
        capture("zh-next-stage");
        unsigned next = stage < PET_STAGE_APEX ? stage + 1 : PET_STAGE_APEX;
        progress_values(pet_ui_target_percent(pet_house_meals(&snapshot.house, snapshot.house.active_id), pet_catalog_meals(next)),
            pet_ui_target_percent(pet_house_days(&snapshot.house, snapshot.house.active_id), pet_catalog_days(next)));
        demo_pet_exit();
    }
    snapshot.storage_ok = false;
    lv_screen_load(lv_obj_create(NULL));
    demo_pet_enter();
    advance(300);
    capture("zh-save-error");
    demo_pet_exit();
    snapshot.storage_ok = true;
    snapshot.house.archive_count = 0;
    lv_screen_load(lv_obj_create(NULL));
    demo_pet_enter();
    demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK);
    advance(300);
    capture("zh-empty-family");
    demo_pet_exit();
    /* Legacy route values remain readable but do not recolor the Agumon line. */
    snapshot.house.archive_count = 1;
    snapshot.house.archive[0].result.year = 2026;
    snapshot.house.archive[0].result.month = 8;
    for (unsigned route = PET_ROUTE_ARMOR; route <= PET_ROUTE_EXPLORER; route++) {
        snapshot.house.life.family.route = route;
        snapshot.house.archive[0].result.route = route;
        snapshot.house.archive[0].result.stage = PET_STAGE_APEX;
        snapshot.house.archive[0].species_id = 0;
        snapshot.revision++;
        lv_screen_load(lv_obj_create(NULL));
        demo_pet_enter();
        advance(300);
        capture(route == PET_ROUTE_ARMOR ? "adult-armor" : route == PET_ROUTE_WILD ? "adult-wild" : "adult-explorer");
        advance(33000);
        capture(route == PET_ROUTE_ARMOR ? "sleep-armor" : route == PET_ROUTE_WILD ? "sleep-wild" : "sleep-explorer");
        demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        advance(200);
        capture("route-locked");
        demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        advance(200);
        capture("route-family");
        demo_pet_exit();
    }
    /* Date provenance, large numbers, old food and full allowance on real LVGL. */
    snapshot.house.life.date = 20260923;
    snapshot.house.life.earned[22] = 2;
    snapshot.house.life.eaten[22] = 1;
    snapshot.house.life.earned[21] = 5;
    snapshot.house.life.eaten[21] = 2;
    snapshot.house.life.daily_goal = 1000000000000ULL;
    snapshot.house.life.tokens_today = 1;
    snapshot.synced_at = 0;
    lv_screen_load(lv_obj_create(NULL));
    demo_pet_enter();
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(300);
    capture("lunch-old-record");
    assert(has_text(lv_screen_active(), "上次的饭盒"));
    assert(has_text(lv_screen_active(), "另有旧饭 3 份，先吃旧饭"));
    assert(has_text(lv_screen_active(), "下份还差 约4000.0亿"));
    snapshot.synced_at = lv_tick_get();
    snapshot.revision++;
    snapshot.house.life.tokens_today = 9007199254740991ULL;
    snapshot.house.life.earned[22] = 5;
    advance(300);
    capture("lunch-full");
    assert(has_text(lv_screen_active(), "五份齐了，安心休息吧"));
    assert(has_text(lv_screen_active(), "今日饭盒"));
    assert(has_text(lv_screen_active(), "约9007.1万亿 Token"));
    advance(602000); /* Freshness refresh is checked once per 1.5 seconds. */
    capture("lunch-stale");
    assert(has_text(lv_screen_active(), "上次的饭盒"));
    demo_pet_exit();
    partner_scenarios();
    home_progress_scenarios();
    archive_key_scenarios();
    /* main.c loads its menu immediately, before allowing another LVGL tick. */
    lv_screen_load(lv_obj_create(NULL));
    advance(3000);
    demo_ble_enter();
    advance(600);
    capture("link-unpaired");
    assert(has_text(lv_screen_active(), "长按确定：返回菜单"));
    ble_status.paired = ble_status.ready = true;
    strcpy(ble_status.id, "1234abcd");
    advance(600);
    capture("link-ready");
    ble_status.connected = true;
    advance(600);
    capture("link-connected");
    ble_status.authenticated = true;
    advance(600);
    capture("link-delivery");
    ble_status.error = -123456789;
    advance(600);
    capture("link-error");
    demo_ble_exit();
    lv_screen_load(lv_obj_create(NULL));
    advance(3000);
    puts("pet_page: PASS (real pages, label bounds, eating, evolution, sleep, browsing, repeated keys, teardown)");
    lv_display_delete(display);
}
