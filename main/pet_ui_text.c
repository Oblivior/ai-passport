#include "pet_ui_text.h"
#include "pet_catalog.h"
#include <stdio.h>

unsigned pet_ui_target_percent(unsigned value, unsigned target)
{
    if (!target) return 0;
    return value >= target ? 100 : (unsigned)((uint64_t)value * 100 / target);
}

void pet_ui_format_tokens(char *out, size_t size, uint64_t value)
{
    uint64_t unit;
    const char *suffix;
    if (value >= 1000000000000ULL) { unit = 1000000000000ULL; suffix = "万亿"; }
    else if (value >= 100000000ULL) { unit = 100000000ULL; suffix = "亿"; }
    else if (value >= 10000ULL) { unit = 10000ULL; suffix = "万"; }
    else { snprintf(out, size, "%llu", (unsigned long long)value); return; }
    snprintf(out, size, "约%llu.%llu%s", (unsigned long long)(value / unit),
        (unsigned long long)((value % unit) / (unit / 10)), suffix);
}

pet_ui_progress_t pet_ui_progress(unsigned stage, unsigned meals, unsigned days)
{
    if (stage >= PET_STAGE_APEX) return (pet_ui_progress_t){.percent = 100, .final_form = true};
    unsigned meal_goal = pet_catalog_meals(stage + 1), day_goal = pet_catalog_days(stage + 1);
    unsigned meal_percent = pet_ui_target_percent(meals, meal_goal);
    unsigned day_percent = pet_ui_target_percent(days, day_goal);
    return (pet_ui_progress_t){
        .percent = meal_percent < day_percent ? meal_percent : day_percent,
        .meals_left = meals >= meal_goal ? 0 : meal_goal - meals,
        .days_left = days >= day_goal ? 0 : day_goal - days,
    };
}

const char *pet_ui_stage_name(pet_stage_t stage)
{
    static const char *const names[PET_STAGE_COUNT] = {
        "数码蛋", "黑球兽", "滚球兽", "亚古兽", "暴龙兽", "机械暴龙兽", "战斗暴龙兽",
    };
    return (unsigned)stage < PET_STAGE_COUNT ? names[stage] : "未知形态";
}

const char *pet_ui_stage_level(pet_stage_t stage)
{
    static const char *const levels[PET_STAGE_COUNT] = {
        "孵化前", "幼年期一", "幼年期二", "成长期", "成熟期", "完全体", "究极体",
    };
    return (unsigned)stage < PET_STAGE_COUNT ? levels[stage] : "未知阶段";
}

const char *pet_ui_legacy_stage_name(pet_stage_t stage)
{
    static const char *const names[PET_STAGE_COUNT] = {
        "数码蛋", "小火苗", "小比特", "侦察员", "游侠", "巨兽", "终极形态",
    };
    return (unsigned)stage < PET_STAGE_COUNT ? names[stage] : "未知形态";
}

const char *pet_ui_route_name(pet_route_t route)
{
    static const char *const names[PET_ROUTE_COUNT] = {
        "初始", "装甲", "野性", "探索", "异变",
    };
    return (unsigned)route < PET_ROUTE_COUNT ? names[route] : "未知路线";
}

const char *pet_ui_route_hint(pet_route_t route)
{
    switch (route) {
        case PET_ROUTE_ARMOR: return "每日食量混合或并列";
        case PET_ROUTE_WILD: return "多数日子获得4-5份";
        case PET_ROUTE_EXPLORER: return "多数日子获得1-2份";
        default: return "等完整一天后再看看";
    }
}
