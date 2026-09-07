#include "pet_ui_text.h"

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
