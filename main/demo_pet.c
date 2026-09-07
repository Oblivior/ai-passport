#include "demo.h"
#include "pet_service.h"
#include "pet_view.h"
#include "pet_ui_text.h"
#include "pet_lunch.h"
#include "ui_pixel.h"
#include "bsp_battery.h"
#include "esp_timer.h"

#include <stdio.h>

typedef enum { PAGE_HOME, PAGE_LUNCH, PAGE_PROGRESS, PAGE_LINEAGE, PAGE_PARTNERS, PAGE_ARCHIVE, PAGE_COUNT } pet_page_t;
static pet_snapshot_t s_state;
static pet_page_t s_page;
static pet_pose_t s_pose;
static lv_obj_t *s_scr, *s_content, *s_battery, *s_pet;
static lv_timer_t *s_timer;
static uint32_t s_action_at, s_pose_at, s_frame;
static unsigned s_archive;
static unsigned s_form_preview;
static uint8_t s_before_stage;
static bool s_eat_requested;
static bool s_delivery_notice;
static uint32_t s_delivery_at;
static bool s_lunch_recent;
static unsigned s_partner_cursor, s_choose_requested;
static bool s_selecting, s_confirming;

static unsigned active_stage(void) { return pet_house_stage(&s_state.house, s_state.house.active_id); }
static unsigned active_meals(void) { return pet_house_meals(&s_state.house, s_state.house.active_id); }
static unsigned active_days(void) { return pet_house_days(&s_state.house, s_state.house.active_id); }
static const char *active_form(unsigned stage) { return pet_catalog_form(s_state.house.active_id, stage); }
static void focus_active_partner(void)
{
    s_partner_cursor = 0;
    for (unsigned i = 0; i < PET_CATALOG_COUNT; i++)
        if (pet_catalog_at(i)->id == s_state.house.active_id) s_partner_cursor = i;
}

static bool sync_recent(void)
{
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    return s_state.synced_at && now - s_state.synced_at <= 600000U;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int y, uint32_t color)
{
    lv_obj_t *obj = ui_pixel_label(parent, text, &passport_zh_14, color);
    lv_obj_set_width(obj, 192);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, y);
    return obj;
}

static lv_obj_t *page_panel(void)
{
    return ui_pixel_panel_create(s_content, 7, 4, 216, 224, UI_PAPER);
}

static lv_obj_t *page_title(lv_obj_t *panel, const char *text)
{
    lv_obj_t *obj = label(panel, text, 0, UI_INK);
    lv_obj_set_style_text_font(obj, &passport_zh_20, 0);
    return obj;
}

static void page_footer(const char *text)
{
    /* Owned by s_content, so a page change also removes the previous hint. */
    label(s_content, text, 242, UI_INK);
}

static void progress_bar(lv_obj_t *panel, int y, unsigned percent, uint32_t color)
{
    lv_obj_t *bar = lv_bar_create(panel);
    lv_obj_set_pos(bar, 4, y);
    lv_obj_set_size(bar, 186, 8);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(UI_MUTED), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(color), LV_PART_INDICATOR);
    lv_bar_set_value(bar, (int)percent, LV_ANIM_OFF);
}

static const char *home_hint(void)
{
    if (!s_state.ready) return "正在醒来...";
    if (!s_state.storage_ok) return "存档失败，请重试";
    if (s_pose == PET_POSE_EAT) return "啊呜啊呜，真香！";
    if (s_pose == PET_POSE_EVOLVE) return "要进化啦！";
    if (s_delivery_notice) return "饭盒送到啦，按确定开饭";
    if (s_pose == PET_POSE_HAPPY) return "见到你真开心！";
    if (pet_life_pending(&s_state.house.life)) return "按确定：开饭啦";
    if (!s_state.house.life.date) return "连接电脑同步后孵化";
    if (s_pose == PET_POSE_SLEEP) return "睡觉中，按确定唤醒";
    return "按确定：摸摸它";
}

static lv_obj_t *home_text(lv_obj_t *panel, const char *text, int x, int y, int width, uint32_t color)
{
    lv_obj_t *obj = label(panel, text, y, color);
    lv_obj_set_width(obj, width);
    lv_obj_set_align(obj, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(obj, x, y);
    return obj;
}

static void draw_home(void)
{
    lv_obj_t *panel = page_panel();
    if (!s_state.house.active_id) {
        label(panel, "这个月，和谁一起？", 20, UI_INK);
        label(panel, "挑一颗数码蛋\n\n开始新的冒险吧", 70, UI_SKY_DARK);
        page_footer(!s_state.storage_ok ? "存档失败，请重试" : "按确定：挑选伙伴");
        return;
    }
    uint8_t stage = s_pose == PET_POSE_EVOLVE || s_pose == PET_POSE_EAT ? s_before_stage : active_stage();
    page_title(panel, active_form(stage));
    lv_obj_t *level = home_text(panel, "", 28, 28, 44, UI_INK);
    lv_obj_set_style_bg_opa(level, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(level, lv_color_hex(UI_YELLOW), 0);
    lv_label_set_text_fmt(level, "Lv.%u", (unsigned)stage);
    home_text(panel, pet_ui_stage_level(stage), 78, 28, 88, UI_SKY_DARK);
    /* Center the companion; even the happy pose's 8px rise clears its subtitle.
     * Reserve the bottom band for growth, with the action hint outside the panel. */
    s_pet = pet_view_create_digimon_pose(panel, s_state.house.active_id, stage, 47, 54, s_pose);
    unsigned pending = pet_life_pending(&s_state.house.life);
    lv_obj_t *food = home_text(panel, "", 112, 151, 78, UI_INK);
    lv_obj_set_style_text_align(food, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text_fmt(food, "饭盒 %u", pending);
    pet_ui_progress_t progress = pet_ui_progress(stage, active_meals(), active_days());
    lv_obj_t *caption = home_text(panel, "", 4, 151, 106, UI_INK);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_LEFT, 0);
    if (progress.final_form) lv_label_set_text(caption, "成长完成");
    else lv_label_set_text_fmt(caption, "进化 %u%%", progress.percent);
    progress_bar(panel, 173, progress.percent, UI_SKY_DARK);
    lv_obj_t *remaining = label(panel, "", 185, UI_SKY_DARK);
    if (progress.final_form) lv_label_set_text(remaining, "月末收入家族图鉴");
    else if (progress.meals_left && progress.days_left)
        lv_label_set_text_fmt(remaining, "还差 %u 份饭 / %u 天", progress.meals_left, progress.days_left);
    else if (progress.meals_left) lv_label_set_text_fmt(remaining, "还差 %u 份饭", progress.meals_left);
    else if (progress.days_left) lv_label_set_text_fmt(remaining, "还差 %u 天陪伴", progress.days_left);
    else lv_label_set_text(remaining, "进化准备就绪！");
    page_footer(home_hint());
}

static void draw_lunch(void)
{
    lv_obj_t *panel = page_panel();
    const pet_life_t *life = &s_state.house.life;
    pet_lunch_t lunch = pet_lunch_read(life);
    bool recent = s_lunch_recent = sync_recent();
    page_title(panel, !lunch.available ? "等待第一餐" : recent ? "今日饭盒" : "上次的饭盒");
    page_footer(!s_state.storage_ok ? "存档失败，请重试" : "按确定：回到伙伴身边");
    if (!lunch.available) {
        label(panel, "还没收到用量\n\n正常使用 AI 后\n连接电脑送来第一餐", 48, UI_SKY_DARK);
        return;
    }
    lv_obj_t *date = label(panel, "", 30, UI_SKY_DARK);
    lv_label_set_text_fmt(date, "%02lu-%02lu / %s", (unsigned long)(life->date / 100 % 100),
        (unsigned long)(life->date % 100), recent ? "同步快照" : "等待同步");
    for (unsigned i = 0; i < 5; i++) {
        bool eaten = i < lunch.eaten, earned = i < lunch.earned;
        lv_obj_t *slot = lv_obj_create(panel);
        lv_obj_set_pos(slot, 3 + (int)i * 38, 58);
        lv_obj_set_size(slot, 34, 31);
        lv_obj_remove_flag(slot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(slot, 0, 0);
        lv_obj_set_style_pad_all(slot, 0, 0);
        lv_obj_set_style_border_width(slot, 2, 0);
        lv_obj_set_style_border_color(slot, lv_color_hex(UI_INK), 0);
        lv_obj_set_style_bg_color(slot, lv_color_hex(eaten ? UI_MUTED : earned ? UI_ORANGE : UI_PAPER), 0);
        lv_obj_t *state = ui_pixel_label(slot, eaten ? "饱" : earned ? "饭" : "·", &passport_zh_14, UI_INK);
        lv_obj_center(state);
    }
    lv_obj_t *counts = label(panel, "", 99, UI_INK);
    lv_label_set_text_fmt(counts, "本日 %u/5 份 · 已吃 %u", lunch.earned, lunch.eaten);
    char text[80], number[40];
    pet_ui_format_tokens(number, sizeof(number), life->tokens_today);
    snprintf(text, sizeof(text), "%s Token", number);
    label(panel, text, 125, UI_SKY_DARK);
    if (lunch.earned == 5) snprintf(text, sizeof(text), "五份齐了，安心休息吧");
    else if (!lunch.remaining_tokens) snprintf(text, sizeof(text), "等待下一轮食物同步");
    else {
        pet_ui_format_tokens(number, sizeof(number), lunch.remaining_tokens);
        snprintf(text, sizeof(text), "下份还差 %s", number);
    }
    label(panel, text, 151, UI_INK);
    progress_bar(panel, 176, lunch.progress, UI_ORANGE);
    lv_obj_t *older = label(panel, "", 185, UI_SKY_DARK);
    if (lunch.older_pending) lv_label_set_text_fmt(older, "另有旧饭 %u 份，先吃旧饭", lunch.older_pending);
    else lv_label_set_text(older, "食物按来源日期记录");
}

static void draw_progress(void)
{
    lv_obj_t *panel = page_panel();
    page_title(panel, active_stage() == PET_STAGE_APEX ? "最终形态" : "下次进化");
    const pet_life_t *life = &s_state.house.life;
    unsigned next = active_stage() < PET_STAGE_APEX ? active_stage() + 1 : PET_STAGE_APEX;
    label(panel, active_form(next), 30, UI_SKY_DARK);
    lv_obj_t *meals = label(panel, "", 62, UI_INK);
    lv_label_set_text_fmt(meals, "已吃 %u / %u 份", active_meals(), pet_catalog_meals(next));
    progress_bar(panel, 87, pet_ui_target_percent(active_meals(), pet_catalog_meals(next)), UI_ORANGE);
    lv_obj_t *days = label(panel, "", 107, UI_INK);
    lv_label_set_text_fmt(days, "陪伴 %u / %u 天", active_days(), pet_catalog_days(next));
    progress_bar(panel, 132, pet_ui_target_percent(active_days(), pet_catalog_days(next)), UI_SKY_DARK);
    lv_obj_t *date = label(panel, "", 153, UI_SKY_DARK);
    if (life->date) lv_label_set_text_fmt(date, "%04lu-%02lu-%02lu · 本地",
        (unsigned long)(life->date / 10000), (unsigned long)(life->date / 100 % 100), (unsigned long)(life->date % 100));
    else lv_label_set_text(date, "等待首次同步");
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    label(panel, !s_state.synced_at || now - s_state.synced_at > 600000U ? "等待用量同步" :
        (s_state.synced_wirelessly ? "无线同步成功" : "USB 同步成功"), 180, UI_INK);
    page_footer("上下翻页 · 休息不掉级");
}

static void draw_lineage(void)
{
    lv_obj_t *panel = page_panel();
    pet_stage_t stage = (pet_stage_t)s_form_preview;
    const pet_species_info_t *species = pet_catalog_find(s_state.house.active_id);
    lv_obj_t *title = page_title(panel, "");
    lv_label_set_text_fmt(title, "%s进化图鉴", species ? species->name : "伙伴");
    label(panel, active_form(stage), 30, UI_SKY_DARK);
    pet_view_create_digimon_pose(panel, s_state.house.active_id, stage, 47, 54, PET_POSE_IDLE);
    lv_obj_t *level = label(panel, "", 153, UI_INK);
    lv_label_set_text_fmt(level, "%s · 形态 %u/7", pet_ui_stage_level(stage), s_form_preview + 1);
    const pet_partner_t *pet = pet_house_partner(&s_state.house, s_state.house.active_id);
    label(panel, pet && stage < pet->highest_plus_one ? "已经养成 · 已解锁" : "未来形态预览", 185, UI_SKY_DARK);
    page_footer("按确定：下一种形态");
}

static void draw_partners(void)
{
    lv_obj_t *panel = page_panel();
    page_title(panel, s_confirming ? "就选这位伙伴？" : "伙伴之家");
    if (s_partner_cursor == PET_CATALOG_COUNT) {
        label(panel, "先回去陪陪它吧", 78, UI_SKY_DARK);
        page_footer("上下挑选 · 确定返回");
        return;
    }
    const pet_species_info_t *species = pet_catalog_at(s_partner_cursor);
    const pet_partner_t *pet = pet_house_partner(&s_state.house, species->id);
    unsigned stage = pet->adopted ? pet_house_stage(&s_state.house, species->id) : PET_STAGE_SCOUT;
    lv_obj_t *position = label(panel, "", 30, UI_SKY_DARK);
    lv_label_set_text_fmt(position, "%s · 伙伴 %u/%u", species->name, s_partner_cursor + 1, PET_CATALOG_COUNT);
    pet_view_create_digimon_pose(panel, species->id, stage, 47, 54,
        pet->adopted && species->id != s_state.house.active_id ? PET_POSE_SLEEP : PET_POSE_IDLE);
    lv_obj_t *status = label(panel, "", 153, UI_INK);
    if (pet->adopted) lv_label_set_text_fmt(status, "%s · 陪伴 %u 天",
        species->id == s_state.house.active_id ? "在你身边" : "在家休息", pet_house_days(&s_state.house, species->id));
    else lv_label_set_text(status, "尚未领养 · 从数码蛋开始");
    if (!s_state.storage_ok) label(panel, "存档失败，请重试", 185, UI_RED);
    else label(panel, s_confirming ? (pet->adopted ? "成长保留，继续陪伴" : "共用饭盒，不额外领饭") :
        "饭盒共享 · 成长各自保留", 185, UI_SKY_DARK);
    page_footer(s_choose_requested ? "正在保存..." : s_confirming ? "确定带走 · 上下取消" :
        s_selecting ? "上下挑选 · 确定选择" : "按确定：挑选或换伙伴");
}

static void draw_archive(void)
{
    lv_obj_t *panel = page_panel();
    page_title(panel, "我的家族图鉴");
    unsigned count = s_state.house.archive_count;
    if (!count) {
        label(panel, "第一只伙伴\n还在慢慢长大\n\n下个月来翻翻图鉴吧", 60, UI_SKY_DARK);
        page_footer("上下翻页");
        return;
    }
    s_archive %= count;
    const pet_house_archive_t *record = &s_state.house.archive[s_archive];
    const pet_archive_entry_t *entry = &record->result;
    bool legacy = !record->species_id;
    if (legacy) pet_view_create_route_pose(panel, entry->stage, entry->route, 47, 34, PET_POSE_IDLE);
    else pet_view_create_digimon_pose(panel, record->species_id, entry->stage, 47, 34, PET_POSE_IDLE);
    lv_obj_t *details = label(panel, "", 138, UI_INK);
    lv_label_set_text_fmt(details, "%04u-%02u  %s\n%s\n%s  %u/%u", entry->year, entry->month,
        legacy ? pet_ui_legacy_stage_name(entry->stage) : pet_catalog_form(record->species_id, entry->stage),
        legacy ? pet_ui_route_name(entry->route) : pet_ui_stage_level(entry->stage),
        legacy ? "试玩回忆" : "本地成长记录",
        s_archive + 1, count);
    page_footer("按确定：下一只伙伴");
}

static void draw_page(void)
{
    if (!s_content) return;
    lv_obj_clean(s_content);
    s_pet = NULL;
    if (s_page == PAGE_HOME) draw_home();
    else if (s_page == PAGE_LUNCH) draw_lunch();
    else if (s_page == PAGE_PROGRESS) draw_progress();
    else if (s_page == PAGE_LINEAGE) draw_lineage();
    else if (s_page == PAGE_PARTNERS) draw_partners();
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
        bool identity_changed = next.house.active_id != s_state.house.active_id ||
            next.house.life.date / 100 != s_state.house.life.date / 100;
        bool ate = !identity_changed && pet_house_meals(&next.house, next.house.active_id) > active_meals();
        unsigned next_earned = pet_life_pending(&next.house.life) + pet_life_meals(&next.house.life);
        unsigned previous_earned = pet_life_pending(&s_state.house.life) + pet_life_meals(&s_state.house.life);
        bool delivery = next.house.life.date / 100 == s_state.house.life.date / 100 ?
            next_earned > previous_earned : next_earned > 0;
        s_before_stage = ate ? active_stage() : s_before_stage;
        s_state = next;
        if (s_choose_requested && changed && next.storage_ok && next.house.active_id == s_choose_requested) {
            s_choose_requested = 0;
            s_selecting = s_confirming = false;
            s_page = PAGE_HOME;
        }
        if (identity_changed) {
            s_pose = PET_POSE_IDLE;
            s_pose_at = s_action_at = lv_tick_get();
            s_eat_requested = false;
            s_before_stage = s_form_preview = active_stage();
            if (!next.house.active_id) {
                s_page = PAGE_PARTNERS;
                s_selecting = true;
                s_confirming = false;
                s_choose_requested = 0;
                s_partner_cursor = 0;
            }
        }
        if (delivery) { s_delivery_notice = true; s_delivery_at = lv_tick_get(); }
        if (ate) {
            s_delivery_notice = false;
            s_eat_requested = false;
            s_action_at = lv_tick_get();
            pose(PET_POSE_EAT);
        } else if (delivery && s_pose == PET_POSE_SLEEP) pose(PET_POSE_IDLE);
        else if (changed && s_pose != PET_POSE_EAT && s_pose != PET_POSE_EVOLVE) draw_page();
    }
    if (s_delivery_notice && lv_tick_elaps(s_delivery_at) >= 4000) {
        s_delivery_notice = false;
        if (s_page == PAGE_HOME) draw_page();
    }
    uint32_t elapsed = lv_tick_elaps(s_pose_at);
    if (s_pose == PET_POSE_EAT && elapsed >= 1200) {
        pose(s_before_stage != active_stage() ? PET_POSE_EVOLVE : PET_POSE_HAPPY);
    } else if (s_pose == PET_POSE_EVOLVE && elapsed >= 1800) {
        pose(PET_POSE_HAPPY);
    } else if (s_pose == PET_POSE_HAPPY && elapsed >= 1600) {
        pose(PET_POSE_IDLE);
    } else if (s_pose == PET_POSE_IDLE && lv_tick_elaps(s_action_at) >= 30000 && !pet_life_pending(&s_state.house.life)) {
        pose(PET_POSE_SLEEP);
    }
    if (s_eat_requested && lv_tick_elaps(s_action_at) >= 2000) s_eat_requested = false;
    if (s_choose_requested && lv_tick_elaps(s_action_at) >= 2000) { s_choose_requested = 0; draw_page(); }
    pet_view_frame(s_pet, s_pose, s_frame++);
    if (s_frame % 10 == 0) {
        /* Do not rebuild the lunchbox on every timer: only a new snapshot or
         * the fresh/stale transition changes it. Keep allocation churn low. */
        if (s_page == PAGE_PROGRESS || (s_page == PAGE_LUNCH && s_lunch_recent != sync_recent())) draw_page();
        int soc = bsp_battery_soc();
        if (soc < 0) lv_label_set_text(s_battery, "--%");
        else lv_label_set_text_fmt(s_battery, "%d%%", soc);
    }
}

void demo_pet_enter(void)
{
    pet_house_init(&s_state.house, NULL);
    s_state.ready = false;
    pet_service_snapshot(&s_state);
    s_page = s_state.house.active_id ? PAGE_HOME : PAGE_PARTNERS;
    s_selecting = !s_state.house.active_id;
    s_confirming = false;
    s_choose_requested = 0;
    focus_active_partner();
    s_pose = PET_POSE_IDLE;
    s_action_at = s_pose_at = lv_tick_get();
    s_eat_requested = false;
    s_delivery_notice = false;
    s_form_preview = active_stage();
    s_archive = s_state.house.archive_count ? s_state.house.archive_count - 1 : 0;
    s_scr = ui_pixel_screen_create("数码宝贝");
    s_battery = ui_pixel_label(s_scr, "--%", &passport_zh_14, UI_PAPER);
    lv_obj_set_pos(s_battery, 169, 29);
    lv_obj_set_width(s_battery, 65);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = lv_obj_create(s_scr);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_content, 5, 51);
    lv_obj_set_size(s_content, 230, 260);
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
    if (s_choose_requested) return;
    s_action_at = lv_tick_get();
    if (s_page == PAGE_PARTNERS && s_selecting) {
        if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
            if (s_confirming) s_confirming = false;
            else s_partner_cursor = (s_partner_cursor + (btn == BSP_BTN_UP ? PET_CATALOG_COUNT : 1)) % (PET_CATALOG_COUNT + 1);
        } else if (btn == BSP_BTN_OK) {
            if (s_partner_cursor == PET_CATALOG_COUNT) { s_selecting = false; s_page = PAGE_HOME; }
            else if (!s_confirming) s_confirming = true;
            else {
                unsigned id = pet_catalog_at(s_partner_cursor)->id;
                if (pet_service_choose(&s_state, id)) s_choose_requested = id;
            }
        }
        draw_page();
        return;
    }
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        s_page = btn == BSP_BTN_UP ? (s_page + PAGE_COUNT - 1) % PAGE_COUNT : (s_page + 1) % PAGE_COUNT;
        if (s_page == PAGE_PARTNERS) focus_active_partner();
        s_form_preview = active_stage();
        draw_page();
    } else if (btn == BSP_BTN_OK && s_page == PAGE_HOME) {
        if (!s_state.house.active_id) { s_page = PAGE_PARTNERS; s_selecting = true; s_partner_cursor = 0; draw_page(); return; }
        if (s_pose == PET_POSE_EAT || s_pose == PET_POSE_EVOLVE || s_eat_requested) return;
        if (pet_life_pending(&s_state.house.life)) s_eat_requested = pet_service_eat(&s_state);
        else pose(PET_POSE_HAPPY);
    } else if (btn == BSP_BTN_OK && s_page == PAGE_LUNCH) {
        s_page = PAGE_HOME;
        draw_page();
    } else if (btn == BSP_BTN_OK && s_page == PAGE_LINEAGE) {
        s_form_preview = (s_form_preview + 1) % PET_STAGE_COUNT;
        draw_page();
    } else if (btn == BSP_BTN_OK && s_page == PAGE_ARCHIVE) {
        s_archive++;
        draw_page();
    } else if (btn == BSP_BTN_OK && s_page == PAGE_PARTNERS) {
        s_selecting = true;
        draw_page();
    }
}
