#include "demo.h"
#include "pet_service.h"
#include "pet_view.h"
#include "pet_ui_text.h"
#include "pet_lunch.h"
#include "pet_training.h"
#include "ui_pixel.h"
#include "bsp_battery.h"
#include "esp_timer.h"

#include <stdio.h>

typedef enum { PAGE_HOME, PAGE_LUNCH, PAGE_PROGRESS, PAGE_LINEAGE, PAGE_PARTNERS, PAGE_MEET, PAGE_BRANCH, PAGE_TRAINING, PAGE_ARCHIVE, PAGE_COUNT } pet_page_t;
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
static unsigned s_branch_cursor, s_branch_requested;
static bool s_branch_selecting, s_branch_confirming;
static bool s_meet_owned, s_meet_pending;
static uint32_t s_meet_old_nonce, s_meet_sent_at;
static lv_obj_t *s_meet_peer;
static pet_training_t s_training;
static pet_snapshot_t s_training_shown;
static uint32_t s_training_serial, s_training_ticket, s_training_sent_at;
static bool s_training_sent;
static bool s_training_press_consumed[3];
static lv_timer_t *s_training_timer;
static lv_obj_t *s_aim, *s_training_caption, *s_training_feedback, *s_shot;
static void draw_page(void);

static unsigned active_stage(void) { return pet_house_stage(&s_state.house, s_state.house.active_id); }
static unsigned active_meals(void) { return pet_house_meals(&s_state.house, s_state.house.active_id); }
static unsigned active_days(void) { return pet_house_days(&s_state.house, s_state.house.active_id); }
static const char *active_form(unsigned stage) { return pet_catalog_branch_form(s_state.house.active_id, stage,
    pet_house_branch(&s_state.house, s_state.house.active_id)); }
static lv_obj_t *companion(lv_obj_t *panel, const pet_house_t *house, unsigned id, unsigned stage,
                            int x, int y, pet_pose_t pose)
{
    return pet_view_create_branch_pose(panel, id, stage, pet_house_branch(house, id), x, y, pose);
}
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
    if (s_pose == PET_POSE_HAPPY) {
        unsigned bond = pet_bond_points(&s_state.bond, s_state.house.active_id);
        return bond >= 60 ? "最喜欢和你一起啦！" : bond >= 30 ? "它开心地向你摇摆！" :
            bond >= 10 ? "它一眼就认出你啦！" : "见到你真开心！";
    }
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
    s_pet = companion(panel, &s_state.house, s_state.house.active_id, stage, 47, 54, s_pose);
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
    unsigned branch = s_form_preview >= PET_STAGE_COUNT;
    pet_stage_t stage = branch ? PET_STAGE_TITAN + s_form_preview - PET_STAGE_COUNT : s_form_preview;
    const pet_species_info_t *species = pet_catalog_find(s_state.house.active_id);
    lv_obj_t *title = page_title(panel, "");
    lv_label_set_text_fmt(title, "%s进化图鉴", species ? species->name : "伙伴");
    label(panel, pet_catalog_branch_form(s_state.house.active_id, stage, branch), 30, UI_SKY_DARK);
    pet_view_create_branch_pose(panel, s_state.house.active_id, stage, branch, 47, 54, PET_POSE_IDLE);
    lv_obj_t *level = label(panel, "", 153, UI_INK);
    lv_label_set_text_fmt(level, "%s · 形态 %u/%u", pet_ui_stage_level(stage), s_form_preview + 1,
        pet_catalog_branch_supported(s_state.house.active_id) ? 9U : 7U);
    label(panel, pet_house_form_seen(&s_state.house, s_state.house.active_id, stage, branch) ?
        "已经养成 · 已解锁" : branch ? "分支预览 · 尚未养成" : "未来形态预览", 185, UI_SKY_DARK);
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
    companion(panel, &s_state.house, species->id, stage, 47, 54,
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
        label(panel, "这个月的伙伴\n还在慢慢长大\n\n下个月来翻翻图鉴吧", 60, UI_SKY_DARK);
        page_footer("按确定：回到伙伴身边");
        return;
    }
    s_archive %= count;
    const pet_house_archive_t *record = &s_state.house.archive[s_archive];
    const pet_archive_entry_t *entry = &record->result;
    bool legacy = !record->species_id;
    if (legacy) pet_view_create_route_pose(panel, entry->stage, entry->route, 47, 34, PET_POSE_IDLE);
    else pet_view_create_branch_pose(panel, record->species_id, entry->stage, record->branch, 47, 34, PET_POSE_IDLE);
    lv_obj_t *details = label(panel, "", 138, UI_INK);
    lv_label_set_text_fmt(details, "%04u-%02u  %s\n%s\n%s  %u/%u", entry->year, entry->month,
        legacy ? pet_ui_legacy_stage_name(entry->stage) : pet_catalog_branch_form(record->species_id, entry->stage, record->branch),
        legacy ? pet_ui_route_name(entry->route) : pet_ui_stage_level(entry->stage),
        legacy ? "试玩回忆" : s_state.house.months[s_archive].status == PET_MONTH_SETTLED ?
            "月度对账完成" : "本地成长记录",
        s_archive + 1, count);
    page_footer(count == 1 ? "仅此一只 · 确定返回" : "按确定：下一只伙伴");
}

static void draw_branch(void)
{
    lv_obj_t *panel = page_panel();
    page_title(panel, s_branch_confirming ? "就走这条路线？" : "进化分支");
    if (!pet_catalog_branch_supported(s_state.house.active_id)) {
        label(panel, "这位伙伴走原主线\n\n暗黑分支属于亚古兽\n\n可去伙伴之家切换", 54, UI_SKY_DARK);
        page_footer("上下翻页 · 确定返回");
        return;
    }
    if (s_branch_cursor == 2) {
        label(panel, "继续陪伴现在的它", 78, UI_SKY_DARK);
        page_footer("上下挑选 · 确定返回");
        return;
    }
    unsigned preview = active_stage() >= PET_STAGE_TITAN ? active_stage() : PET_STAGE_TITAN;
    label(panel, pet_catalog_branch_form(s_state.house.active_id, preview, s_branch_cursor), 30, UI_SKY_DARK);
    pet_view_create_branch_pose(panel, s_state.house.active_id, preview, s_branch_cursor, 47, 54, PET_POSE_IDLE);
    unsigned points = pet_bond_points(&s_state.bond, s_state.house.active_id);
    lv_obj_t *condition = label(panel, "", 153, UI_INK);
    if (s_branch_cursor) lv_label_set_text_fmt(condition, "暗黑路线 · 亲密 %u/30", points);
    else lv_label_set_text(condition, "原主线 · 不需要解锁");
    label(panel, !s_state.storage_ok ? "保存失败，请重试" :
        s_branch_cursor && !s_state.bond_storage_ok ? "亲密存档异常，请重启" :
        active_stage() < PET_STAGE_TITAN ? "先选路线，完全体时变身" : "可切回主线，成长保留", 185, UI_SKY_DARK);
    page_footer(s_branch_requested ? "正在保存..." : s_branch_confirming ? "确定切换 · 上下取消" :
        !s_branch_selecting ? "按确定：挑选进化路线" :
        s_branch_cursor && (points < PET_BRANCH_BOND || !s_state.bond_storage_ok) ? "一起训练，亲密 30 解锁" :
        "上下挑选 · 确定选择");
}

static lv_obj_t *meet_pet(lv_obj_t *panel, unsigned id, unsigned stage, unsigned branch, int x)
{
    lv_obj_t *pet = pet_view_create_branch_pose(panel, id, stage, branch, x, 58, PET_POSE_IDLE);
    if (!pet) return NULL;
    lv_obj_set_width(pet, 94);
    lv_obj_t *sprite = lv_obj_get_child(pet, 0);
    lv_image_set_scale(sprite, 512); /* Two compact, exact 2x sprites on one card. */
    lv_obj_set_pos(sprite, 12, 17);
    return pet;
}
static void draw_meet(void)
{
    lv_obj_t *panel = page_panel();
    const pet_meet_t *m = &s_state.meeting;
    page_title(panel, s_meet_owned && m->complete ? "交到新朋友啦！" : "伙伴相遇");
    if (!s_meet_owned || s_meet_pending) {
        label(panel, "双方打开这个页面\n\n把胸牌靠近一些\n\n各按确定，开始寻找", 52, UI_SKY_DARK);
        label(panel, "只打招呼，不交换用量", 185, UI_INK);
        page_footer(s_meet_pending ? "正在打开相遇..." : "确定寻找 · 最多 60 秒");
        return;
    }
    if (s_state.meeting_error) {
        label(panel, "相遇暂时没打开\n\n请确认已配对\n等饭盒同步结束后重试", 54, UI_SKY_DARK);
        page_footer("确定重试 · 上下返回");
        return;
    }
    if (!m->active && !m->complete) {
        label(panel, "这次还没碰到伙伴\n\n两台都打开相遇\n再靠近一点试试吧", 54, UI_SKY_DARK);
        page_footer("确定再找 · 上下返回");
        return;
    }
    if (!m->peer || m->sightings < 2) {
        label(panel, "正在寻找附近的伙伴", 30, UI_SKY_DARK);
        s_pet = companion(panel, &s_state.house, s_state.house.active_id, active_stage(), 47, 54, PET_POSE_IDLE);
        lv_obj_t *remaining = label(panel, "", 153, UI_INK);
        unsigned elapsed = (uint32_t)(esp_timer_get_time() / 1000) - m->started_at;
        lv_label_set_text_fmt(remaining, "还有 %u 秒", elapsed >= PET_MEET_WINDOW_MS ? 0 : (PET_MEET_WINDOW_MS - elapsed + 999) / 1000);
        label(panel, "靠近不是碰撞检测", 185, UI_SKY_DARK);
        page_footer("上下返回 · 不会扣食物");
        return;
    }
    label(panel, pet_catalog_branch_form(m->peer_species, m->peer_stage, m->peer_branch), 30, UI_SKY_DARK);
    s_pet = meet_pet(panel, m->species, m->stage, m->branch, 0);
    s_meet_peer = meet_pet(panel, m->peer_species, m->peer_stage, m->peer_branch, 100);
    label(panel, m->complete ? pet_meet_greeting(m->species, m->peer_species) :
        m->confirmed ? "你已招手，等对方回应" : "发现伙伴，一起打招呼？", 153, UI_INK);
    label(panel, m->complete ? "今天也要一起冒险呀" : "双方确认后出现彩蛋", 185, UI_SKY_DARK);
    page_footer(m->complete ? "确定返回 · 上下翻页" : m->confirmed ? "等待回应 · 上下取消" : "确定招手 · 上下取消");
}

static uint32_t training_color(void)
{
    return s_state.house.active_id == PET_AGUMON ? UI_ORANGE :
        s_state.house.active_id == PET_GABUMON ? UI_SKY : UI_GRASS;
}
static const char *bond_unlock(unsigned points)
{
    return points < 10 ? "10 点：熟悉的招呼" : points < 30 ? "30 点：摇摆招呼" :
        points < 60 ? "60 点：默契庆祝" : "默契满满，随时来玩";
}
static lv_obj_t *training_block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, w, h);
    lv_obj_set_style_pad_all(obj, 0, 0); lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0); lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static void training_submit(void)
{
    s_training_sent = pet_service_train(&s_training_shown, pet_training_score(&s_training),
        pet_training_attempts(&s_training), s_training_ticket);
    s_training_sent_at = lv_tick_get();
}
static void training_update(lv_timer_t *timer)
{
    (void)timer;
    if (s_page != PAGE_TRAINING || !s_training.active) return;
    uint32_t now = lv_tick_get();
    pet_training_tick(&s_training, now);
    if (s_training.finished) { training_submit(); draw_page(); return; }
    unsigned round = pet_training_round(&s_training, now);
    bool hit = s_training.attempts & (1U << round);
    unsigned seconds = (PET_TRAIN_ROUND_MS - (now - s_training.started_at) % PET_TRAIN_ROUND_MS + 999U) / 1000U;
    lv_label_set_text_fmt(s_training_caption, "第 %u/3 轮 · %u 秒", round + 1, seconds);
    lv_obj_set_x(s_aim, 4 + (int)pet_training_position(&s_training, now) * 183 / 100);
    lv_label_set_text(s_training_feedback, !hit ? "瞄准黄色中心，按确定" :
        s_training.scores[round] == 2 ? "正中靶心！等下一轮" :
        s_training.scores[round] == 1 ? "打中了！等下一轮" : "差一点，再接再厉");
    pet_view_frame(s_pet, hit ? PET_POSE_HAPPY : PET_POSE_IDLE, now / 150U);
    if (hit && now - s_training.hit_at < 600U) {
        lv_obj_remove_flag(s_shot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(s_shot, 111 + (int)(now - s_training.hit_at) * 57 / 600);
    } else lv_obj_add_flag(s_shot, LV_OBJ_FLAG_HIDDEN);
}
static void draw_training(void)
{
    lv_obj_t *panel = page_panel();
    unsigned points = pet_bond_points(&s_state.bond, s_state.house.active_id);
    if (s_training.active) {
        page_title(panel, "一起训练");
        s_training_caption = label(panel, "", 30, UI_SKY_DARK);
        s_pet = companion(panel, &s_state.house, s_state.house.active_id, active_stage(), 7, 56, PET_POSE_IDLE);
        training_block(panel, 164, 81, 24, 28, UI_INK);
        training_block(panel, 168, 85, 16, 20, UI_YELLOW);
        training_block(panel, 172, 91, 8, 8, UI_ORANGE);
        s_shot = training_block(panel, 111, 91, 14, 10, training_color());
        /* Species-specific pixel feedback, not extra full-size frame buffers. */
        if (s_state.house.active_id == PET_AGUMON) training_block(s_shot, 2, 2, 8, 5, UI_YELLOW);
        else if (s_state.house.active_id == PET_GABUMON) training_block(s_shot, 6, 0, 2, 10, UI_PAPER);
        else { training_block(s_shot, 0, 2, 14, 2, UI_PAPER); training_block(s_shot, 0, 6, 14, 2, UI_PAPER); }
        training_block(panel, 4, 157, 186, 16, UI_MUTED);
        training_block(panel, 68, 157, 58, 16, UI_GRASS);
        training_block(panel, 87, 157, 20, 16, UI_YELLOW);
        s_aim = training_block(panel, 4, 153, 3, 24, UI_INK);
        s_training_feedback = label(panel, "", 185, UI_INK);
        page_footer("确定出手 · 上下结束");
        training_update(NULL);
        return;
    }
    if (s_training.finished) {
        unsigned score = pet_training_score(&s_training);
        page_title(panel, score == 6 ? "完美配合！" : score >= 3 ? "配合不错！" : "再试一次！");
        lv_obj_t *result = label(panel, "", 30, UI_SKY_DARK);
        lv_label_set_text_fmt(result, "得分 %u/6 · 出手 %u/3", score, pet_training_attempts(&s_training));
        s_pet = companion(panel, &s_training_shown.house, s_training_shown.house.active_id,
            pet_house_stage(&s_training_shown.house, s_training_shown.house.active_id), 47, 56, PET_POSE_HAPPY);
        bool saved = s_state.training_ticket == s_training_ticket;
        lv_obj_t *reward = label(panel, "", 153, UI_INK);
        if (!saved) lv_label_set_text(reward, s_training_sent ? "正在保存这次默契..." : "暂未提交，请重试");
        else if (s_state.training_result == PET_TRAIN_REWARDED)
            lv_label_set_text_fmt(reward, "亲密 +%u · %u/100", s_state.training_gain, points);
        else lv_label_set_text(reward, s_state.training_result == PET_TRAIN_SAVE_ERROR ? "保存失败，成长未变" :
            s_state.training_result == PET_TRAIN_OFFLINE ? "离线练习，不计奖励" :
            s_state.training_result == PET_TRAIN_STALE ? "伙伴或日期已变更" :
            !pet_training_attempts(&s_training) ? "没有出手，不计奖励" : "自由练习，不计奖励");
        progress_bar(panel, 176, points, UI_RED);
        label(panel, bond_unlock(points), 185, UI_SKY_DARK);
        page_footer(!saved && s_training_sent ? "正在保存..." : !saved || s_state.training_result == PET_TRAIN_SAVE_ERROR ?
            "确定重试 · 上下返回" : "确定返回 · 上下翻页");
        return;
    }
    page_title(panel, "伙伴训练场");
    label(panel, active_form(active_stage()), 30, UI_SKY_DARK);
    s_pet = companion(panel, &s_state.house, s_state.house.active_id, active_stage(), 47, 54, PET_POSE_IDLE);
    lv_obj_t *bond = label(panel, "", 153, UI_INK);
    lv_label_set_text_fmt(bond, "亲密 %u/100 · 余 %u 局", points,
        pet_bond_remaining(&s_state.bond, s_state.house.life.date));
    progress_bar(panel, 176, points, UI_RED);
    label(panel, bond_unlock(points), 185, UI_SKY_DARK);
    page_footer(!s_state.bond_storage_ok ? "亲密存档异常，请重启" : !active_stage() ? "孵化后就能一起训练" :
        !sync_recent() ? "确定练习 · 同步后计奖励" : "确定开始 · 最多 15 秒");
}

static void draw_page(void)
{
    if (!s_content) return;
    if (s_training_timer) {
        if (s_page == PAGE_TRAINING && s_training.active) lv_timer_resume(s_training_timer);
        else lv_timer_pause(s_training_timer);
    }
    lv_obj_clean(s_content);
    s_pet = NULL;
    s_meet_peer = NULL;
    s_aim = s_training_caption = s_training_feedback = s_shot = NULL;
    if (s_page == PAGE_HOME) draw_home();
    else if (s_page == PAGE_LUNCH) draw_lunch();
    else if (s_page == PAGE_PROGRESS) draw_progress();
    else if (s_page == PAGE_LINEAGE) draw_lineage();
    else if (s_page == PAGE_PARTNERS) draw_partners();
    else if (s_page == PAGE_BRANCH) draw_branch();
    else if (s_page == PAGE_MEET) draw_meet();
    else if (s_page == PAGE_TRAINING) draw_training();
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
        if (s_meet_pending && (next.meeting.nonce != s_meet_old_nonce || next.meeting_error)) s_meet_pending = false;
        if (s_branch_requested && changed && next.storage_ok &&
            pet_house_branch(&next.house, next.house.active_id) + 1 == s_branch_requested && !identity_changed) {
            s_branch_requested = 0;
            s_branch_selecting = s_branch_confirming = false;
            s_page = PAGE_HOME;
            s_before_stage = active_stage();
            s_action_at = lv_tick_get();
            pose(active_stage() >= PET_STAGE_TITAN ? PET_POSE_EVOLVE : PET_POSE_HAPPY);
        }
        if (s_choose_requested && changed && next.storage_ok && next.house.active_id == s_choose_requested) {
            s_choose_requested = 0;
            s_selecting = s_confirming = false;
            s_page = PAGE_HOME;
        }
        if (identity_changed) {
            if (s_meet_owned) pet_service_meet(NULL, PET_MEET_CANCEL);
            s_meet_owned = s_meet_pending = false;
            s_branch_requested = 0;
            s_branch_selecting = s_branch_confirming = false;
            s_branch_cursor = pet_house_branch(&next.house, next.house.active_id);
            s_training = (pet_training_t){0};
            s_training_sent = false;
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
    if (s_branch_requested && lv_tick_elaps(s_action_at) >= 2000) { s_branch_requested = 0; draw_page(); }
    if (s_meet_pending && lv_tick_elaps(s_meet_sent_at) >= 2000) {
        pet_service_meet(NULL, PET_MEET_CANCEL); s_meet_owned = s_meet_pending = false; draw_page();
    }
    if (s_training.finished && s_training_sent && s_state.training_ticket != s_training_ticket &&
        lv_tick_elaps(s_training_sent_at) >= 2000) { s_training_sent = false; draw_page(); }
    if (s_page != PAGE_TRAINING || !s_training.active)
        pet_view_frame(s_pet, s_page == PAGE_TRAINING ? (s_training.finished ? PET_POSE_HAPPY : PET_POSE_IDLE) : s_pose, s_frame);
    if (s_pet && s_page == PAGE_HOME && s_pose == PET_POSE_HAPPY) {
        unsigned bond = pet_bond_points(&s_state.bond, s_state.house.active_id);
        if (bond >= 30) lv_obj_set_style_translate_x(s_pet, s_frame % 4 < 2 ? -3 : 3, 0);
        if (bond >= 60) lv_obj_set_style_translate_y(s_pet, s_frame % 4 == 0 ? -8 : s_frame % 4 == 2 ? -4 : 0, 0);
    }
    if (s_page == PAGE_MEET) {
        pet_pose_t pose = s_state.meeting.complete ? PET_POSE_HAPPY : PET_POSE_IDLE;
        pet_view_frame(s_pet, pose, s_frame); pet_view_frame(s_meet_peer, pose, s_frame + 1);
    }
    s_frame++;
    if (s_frame % 10 == 0) {
        /* Do not rebuild the lunchbox on every timer: only a new snapshot or
         * the fresh/stale transition changes it. Keep allocation churn low. */
        if (s_page == PAGE_PROGRESS || s_page == PAGE_MEET || (s_page == PAGE_LUNCH && s_lunch_recent != sync_recent())) draw_page();
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
    s_branch_requested = 0;
    s_branch_selecting = s_branch_confirming = false;
    s_branch_cursor = pet_house_branch(&s_state.house, s_state.house.active_id);
    s_meet_owned = s_meet_pending = false;
    focus_active_partner();
    s_pose = PET_POSE_IDLE;
    s_action_at = s_pose_at = lv_tick_get();
    s_eat_requested = false;
    s_training = (pet_training_t){0};
    s_training_sent = false;
    for (unsigned i = 0; i < 3; i++) s_training_press_consumed[i] = false;
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
    s_training_timer = lv_timer_create(training_update, 30, NULL);
    lv_timer_pause(s_training_timer);
    lv_screen_load(s_scr);
}

void demo_pet_exit(void)
{
    if (s_meet_owned) pet_service_meet(NULL, PET_MEET_CANCEL);
    s_meet_owned = s_meet_pending = false;
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_training_timer) { lv_timer_delete(s_training_timer); s_training_timer = NULL; }
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; }
    s_content = s_battery = s_pet = NULL;
    s_meet_peer = NULL;
    s_aim = s_training_caption = s_training_feedback = s_shot = NULL;
}

void demo_pet_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if ((unsigned)btn >= 3) return;
    /* Games judge the existing low-latency PRESS event. Swallow its eventual
     * CLICK even if that press finished/cancelled the game, so a result cannot
     * disappear immediately and a delayed release cannot hit the next round. */
    if (ev == BSP_BTN_PRESS && s_page == PAGE_TRAINING && s_training.active) {
        s_training_press_consumed[btn] = true;
    } else if (ev == BSP_BTN_CLICK) {
        if (s_training_press_consumed[btn]) { s_training_press_consumed[btn] = false; return; }
        if (s_page == PAGE_TRAINING && s_training.active && btn == BSP_BTN_OK) return;
    } else {
        if (ev == BSP_BTN_DOUBLE) s_training_press_consumed[btn] = false;
        return;
    }
    if (s_choose_requested || s_branch_requested) return;
    s_action_at = lv_tick_get();
    if (s_page == PAGE_MEET && btn == BSP_BTN_OK) {
        if (s_meet_pending) return;
        if (s_meet_owned && s_state.meeting.complete) {
            pet_service_meet(NULL, PET_MEET_CANCEL); s_meet_owned = false; s_page = PAGE_HOME;
        } else if (!s_meet_owned || !s_state.meeting.active) {
            s_meet_old_nonce = s_state.meeting.nonce; s_meet_sent_at = lv_tick_get();
            s_meet_owned = s_meet_pending = pet_service_meet(&s_state, PET_MEET_START);
        } else if (s_state.meeting.sightings >= 2) pet_service_meet(&s_state, PET_MEET_CONFIRM);
        draw_page(); return;
    }
    if (s_page == PAGE_BRANCH && s_branch_selecting) {
        if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
            if (s_branch_confirming) s_branch_confirming = false;
            else s_branch_cursor = (s_branch_cursor + (btn == BSP_BTN_UP ? 2 : 1)) % 3;
        } else if (btn == BSP_BTN_OK) {
            if (s_branch_cursor == 2) { s_branch_selecting = false; s_page = PAGE_HOME; }
            else if (!s_branch_cursor || (s_state.bond_storage_ok &&
                pet_bond_points(&s_state.bond, s_state.house.active_id) >= PET_BRANCH_BOND)) {
                if (!s_branch_confirming) s_branch_confirming = true;
                else if (pet_service_branch(&s_state, s_branch_cursor)) s_branch_requested = s_branch_cursor + 1;
            }
        }
        draw_page();
        return;
    }
    if (s_page == PAGE_TRAINING) {
        if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
            bool was_active = s_training.active;
            s_training = (pet_training_t){0};
            if (was_active) { draw_page(); return; }
        } else if (btn == BSP_BTN_OK) {
            if (s_training.active) {
                pet_training_hit(&s_training, lv_tick_get());
                if (s_training.finished) { training_submit(); draw_page(); }
                else training_update(NULL);
            } else if (s_training.finished) {
                bool saved = s_state.training_ticket == s_training_ticket;
                if ((!saved && !s_training_sent) || (saved && s_state.training_result == PET_TRAIN_SAVE_ERROR)) training_submit();
                else if (saved) s_training = (pet_training_t){0};
                draw_page();
            } else if (active_stage() && s_state.ready) {
                s_training_shown = s_state;
                if (!++s_training_serial) ++s_training_serial;
                s_training_ticket = s_training_serial;
                s_training_sent = false;
                pet_training_start(&s_training, lv_tick_get());
                draw_page();
            }
            return;
        }
    }
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
        if (s_page == PAGE_MEET && s_meet_owned) {
            pet_service_meet(NULL, PET_MEET_CANCEL); s_meet_owned = s_meet_pending = false;
        }
        s_page = btn == BSP_BTN_UP ? (s_page + PAGE_COUNT - 1) % PAGE_COUNT : (s_page + 1) % PAGE_COUNT;
        if (s_page == PAGE_PARTNERS) focus_active_partner();
        if (s_page == PAGE_BRANCH) s_branch_cursor = pet_house_branch(&s_state.house, s_state.house.active_id);
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
        s_form_preview = (s_form_preview + 1) % (pet_catalog_branch_supported(s_state.house.active_id) ? 9U : PET_STAGE_COUNT);
        draw_page();
    } else if (btn == BSP_BTN_OK && s_page == PAGE_BRANCH) {
        if (pet_catalog_branch_supported(s_state.house.active_id)) s_branch_selecting = true;
        else s_page = PAGE_HOME;
        draw_page();
    } else if (btn == BSP_BTN_OK && s_page == PAGE_ARCHIVE) {
        if (s_state.house.archive_count < 2) s_page = PAGE_HOME;
        else s_archive = (s_archive + 1) % s_state.house.archive_count;
        draw_page();
    } else if (btn == BSP_BTN_OK && s_page == PAGE_PARTNERS) {
        s_selecting = true;
        draw_page();
    }
}
