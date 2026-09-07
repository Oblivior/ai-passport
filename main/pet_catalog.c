#include "pet_catalog.h"

static const pet_species_info_t CATALOG[] = {
    {PET_AGUMON, 0, "亚古兽", {"数码蛋", "黑球兽", "滚球兽", "亚古兽", "暴龙兽", "机械暴龙兽", "战斗暴龙兽"}},
    {PET_GABUMON, 1, "加布兽", {"数码蛋", "普尼兽", "独角兽", "加布兽", "加鲁鲁兽", "兽人加鲁鲁兽", "钢铁加鲁鲁兽"}},
    {PET_PATAMON, 2, "巴达兽", {"数码蛋", "浮游兽", "迪哥兽", "巴达兽", "天使兽", "神圣天使兽", "炽天使兽"}},
};
_Static_assert(sizeof(CATALOG) / sizeof(CATALOG[0]) == PET_CATALOG_COUNT, "Update the visible catalog count");
_Static_assert(PET_CATALOG_COUNT <= PET_PARTNER_CAPACITY, "Additional partners require a storage migration");
const pet_species_info_t *pet_catalog_at(unsigned index)
{
    return index < PET_CATALOG_COUNT ? &CATALOG[index] : NULL;
}
const pet_species_info_t *pet_catalog_find(unsigned id)
{
    for (unsigned i = 0; i < PET_CATALOG_COUNT; i++) if (CATALOG[i].id == id) return &CATALOG[i];
    return NULL;
}
const char *pet_catalog_form(unsigned id, unsigned stage)
{
    const pet_species_info_t *species = pet_catalog_find(id);
    return species && stage < PET_STAGE_COUNT ? species->forms[stage] : "等待领养";
}
unsigned pet_catalog_meals(unsigned stage)
{
    static const uint8_t values[] = {0, 1, 3, 6, 12, 24, 40};
    return values[stage < PET_STAGE_COUNT ? stage : PET_STAGE_APEX];
}
unsigned pet_catalog_days(unsigned stage)
{
    static const uint8_t values[] = {0, 1, 2, 3, 6, 10, 16};
    return values[stage < PET_STAGE_COUNT ? stage : PET_STAGE_APEX];
}
