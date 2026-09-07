#include "demo.h"
#include "pet_service.h"
#include "pet_ble.h"
#include "lvgl.h"
#include "src/misc/lv_text_private.h"
#include "pet_ui_text.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static pet_snapshot_t snapshot;
static pet_ble_status_t ble_status;
void pet_ble_status(pet_ble_status_t *out) { *out = ble_status; }
static uint16_t framebuffer[240 * 320];

bool pet_service_snapshot(pet_snapshot_t *out) { *out = snapshot; return true; }
bool pet_service_start(void) { return true; }
bool pet_service_eat(void)
{
    bool ate = pet_life_eat(&snapshot.life);
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
            if (other == obj || !lv_obj_check_type(other, &lv_label_class)) continue;
            lv_area_t b;
            lv_obj_get_coords(other, &b);
            assert(a.x2 < b.x1 || b.x2 < a.x1 || a.y2 < b.y1 || b.y2 < a.y1);
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); i++) bounds(lv_obj_get_child(obj, i));
}

static void capture(const char *name)
{
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(NULL);
    bounds(lv_screen_active());
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

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(240, 320);
    static uint8_t buffer[240 * 20 * 2];
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    assert(strcmp(pet_model_stage_name(PET_STAGE_EGG), "EGG") == 0);
    assert(strcmp(pet_ui_stage_name(PET_STAGE_EGG), "数码蛋") == 0);
    assert(strcmp(pet_ui_route_name(PET_ROUTE_ARMOR), "装甲") == 0);
    pet_model_t old;
    pet_model_init(&old);
    pet_model_begin_month(&old, 2026, 9);
    for (unsigned i = 1; i <= 4; i++) pet_model_apply_feed(&old, i, 5);
    pet_life_init(&snapshot.life, &old);
    snapshot.ready = snapshot.storage_ok = true;
    snapshot.revision = 1;
    demo_pet_enter();
    advance(300);
    capture("waiting");
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(200);
    capture("lunch-empty");
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    pet_usage_t usage = {.date = 20260907, .daily_goal = 1000, .tokens_today = 300};
    usage.earned[6] = 2;
    assert(pet_life_sync(&snapshot.life, &usage));
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
    for (int i = 0; i < 20; i++) demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(pet_life_meals(&snapshot.life) == 1);
    advance(1200);
    capture("evolving");
    advance(3600);
    capture("spark");
    demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
    advance(5000);
    assert(pet_life_meals(&snapshot.life) == 2);
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
    snapshot.synced_wirelessly = true;
    snapshot.synced_at = lv_tick_get();
    snapshot.revision++;
    advance(200);
    capture("wireless-progress");
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(200);
    capture("route-pending");
    pet_life_t before_preview = snapshot.life;
    const char *preview_names[] = {"route-armor", "route-wild", "route-explorer", "route-current"};
    for (unsigned i = 0; i < 4; i++) {
        demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
        advance(200);
        capture(preview_names[i]);
    }
    assert(!memcmp(&before_preview, &snapshot.life, sizeof(before_preview)));
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(200);
    capture("family");
    for (int i = 0; i < 100; i++) {
        demo_pet_key(BSP_BTN_OK, BSP_BTN_CLICK);
        demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        advance(200);
    }
    assert(pet_life_meals(&snapshot.life) == 2);
    for (unsigned day = 8; day <= 22; day++) {
        usage.date = 20260900 + day;
        usage.earned[day - 1] = 5;
        assert(pet_life_sync(&snapshot.life, &usage));
        while (pet_life_eat(&snapshot.life)) {}
    }
    assert(snapshot.life.stage == PET_STAGE_APEX);
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
    for (unsigned stage = 0; stage < PET_STAGE_COUNT; stage++) {
        snapshot.life.stage = stage;
        snapshot.life.family.route = PET_ROUTE_EXPLORER;
        snapshot.revision++;
        lv_screen_load(lv_obj_create(NULL));
        demo_pet_enter();
        advance(300);
        char name[32];
        snprintf(name, sizeof(name), "zh-stage-%u", stage);
        capture(name);
        demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        advance(300);
        capture("zh-next-stage");
        demo_pet_exit();
    }
    snapshot.storage_ok = false;
    lv_screen_load(lv_obj_create(NULL));
    demo_pet_enter();
    advance(300);
    capture("zh-save-error");
    demo_pet_exit();
    snapshot.storage_ok = true;
    snapshot.life.family.archive_count = 0;
    lv_screen_load(lv_obj_create(NULL));
    demo_pet_enter();
    demo_pet_key(BSP_BTN_UP, BSP_BTN_CLICK);
    advance(300);
    capture("zh-empty-family");
    demo_pet_exit();
    /* Each route keeps its own final form, sleep face and archived identity. */
    snapshot.life.family.archive_count = 1;
    snapshot.life.stage = PET_STAGE_APEX;
    for (unsigned route = PET_ROUTE_ARMOR; route <= PET_ROUTE_EXPLORER; route++) {
        snapshot.life.family.route = route;
        snapshot.life.family.archive[0].route = route;
        snapshot.life.family.archive[0].stage = PET_STAGE_APEX;
        snapshot.life.legacy_mask = 0;
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
        advance(200);
        capture("route-family");
        demo_pet_exit();
    }
    /* Date provenance, large numbers, old food and full allowance on real LVGL. */
    snapshot.life.date = 20260923;
    snapshot.life.earned[22] = 2;
    snapshot.life.eaten[22] = 1;
    snapshot.life.earned[21] = 5;
    snapshot.life.eaten[21] = 2;
    snapshot.life.daily_goal = 1000000000000ULL;
    snapshot.life.tokens_today = 1;
    snapshot.synced_at = 0;
    lv_screen_load(lv_obj_create(NULL));
    demo_pet_enter();
    demo_pet_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    advance(300);
    capture("lunch-old-record");
    assert(has_text(lv_screen_active(), "上次的饭盒"));
    assert(has_text(lv_screen_active(), "另有旧饭 3 份，先吃旧饭"));
    snapshot.synced_at = lv_tick_get();
    snapshot.revision++;
    snapshot.life.tokens_today = 9007199254740991ULL;
    snapshot.life.earned[22] = 5;
    advance(300);
    capture("lunch-full");
    assert(has_text(lv_screen_active(), "五份齐了，安心休息吧"));
    assert(has_text(lv_screen_active(), "今日饭盒"));
    advance(601000);
    capture("lunch-stale");
    assert(has_text(lv_screen_active(), "上次的饭盒"));
    demo_pet_exit();
    /* main.c loads its menu immediately, before allowing another LVGL tick. */
    lv_screen_load(lv_obj_create(NULL));
    advance(3000);
    demo_ble_enter();
    advance(600);
    capture("link-unpaired");
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
