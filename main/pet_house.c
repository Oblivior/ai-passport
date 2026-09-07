#include "pet_house.h"
#include <string.h>

const pet_partner_t *pet_house_partner(const pet_house_t *house, unsigned id)
{
    return house && id && id <= PET_PARTNER_CAPACITY ? &house->partners[id - 1] : NULL;
}
unsigned pet_house_meals(const pet_house_t *house, unsigned id)
{
    const pet_partner_t *pet = pet_house_partner(house, id);
    unsigned total = 0;
    if (pet) for (unsigned i = 0; i < PET_LIFE_DAYS; i++) total += pet->eaten[i];
    return total;
}
unsigned pet_house_days(const pet_house_t *house, unsigned id)
{
    const pet_partner_t *pet = pet_house_partner(house, id);
    unsigned days = 0;
    bool adoption_day = false;
    if (pet) for (unsigned i = 0; i < PET_LIFE_DAYS; i++) {
        if (!pet->eaten[i]) continue;
        if (i + 1 <= pet->adopted_day) adoption_day = true;
        else days++;
    }
    return days + adoption_day;
}
unsigned pet_house_stage(const pet_house_t *house, unsigned id)
{
    unsigned stage = 0, meals = pet_house_meals(house, id), days = pet_house_days(house, id);
    for (unsigned i = 1; i < PET_STAGE_COUNT; i++) {
        if (meals < pet_catalog_meals(i) || days < pet_catalog_days(i)) break;
        stage = i;
    }
    return stage;
}
static void archive(pet_house_t *house, pet_house_archive_t entry, pet_month_record_t month)
{
    if (house->archive_count == PET_ARCHIVE_MAX) {
        memmove(house->archive, house->archive + 1, (PET_ARCHIVE_MAX - 1) * sizeof(entry));
        memmove(house->months, house->months + 1, (PET_ARCHIVE_MAX - 1) * sizeof(month));
        house->archive_count--;
    }
    house->months[house->archive_count] = month;
    house->archive[house->archive_count++] = entry;
}
static void clear_pantry_archive(pet_house_t *house)
{
    memset(house->life.family.archive, 0, sizeof(house->life.family.archive));
    house->life.family.archive_count = 0;
    house->life.legacy_mask = 0;
}
unsigned pet_house_branch(const pet_house_t *house, unsigned id)
{
    return house && pet_catalog_branch_supported(id) ? (house->branch_mask >> (id - 1)) & 1U : 0;
}
bool pet_house_form_seen(const pet_house_t *house, unsigned id, unsigned stage, unsigned branch)
{
    if (!house || !pet_catalog_find(id) || stage >= PET_STAGE_COUNT || branch > 1) return false;
    if (branch && stage >= PET_STAGE_TITAN)
        return pet_catalog_branch_supported(id) && (house->branch_seen_mask & (1U << (stage - PET_STAGE_TITAN)));
    const pet_partner_t *pet = pet_house_partner(house, id);
    return stage < pet->highest_plus_one;
}
static void discover(pet_house_t *house, unsigned id)
{
    unsigned stage = pet_house_stage(house, id);
    if (pet_house_branch(house, id) && stage >= PET_STAGE_TITAN)
        house->branch_seen_mask |= 1U << (stage - PET_STAGE_TITAN);
    else if (house->partners[id - 1].highest_plus_one <= stage)
        house->partners[id - 1].highest_plus_one = stage + 1;
}
bool pet_house_branch_choose(pet_house_t *house, unsigned expected_id, uint32_t expected_month,
                             unsigned branch, unsigned bond_points)
{
    if (!pet_house_valid(house) || !pet_catalog_branch_supported(expected_id) ||
        house->active_id != expected_id || house->life.date / 100 != expected_month || branch > 1 ||
        (branch && bond_points < PET_BRANCH_BOND)) return false;
    unsigned bit = 1U << (expected_id - 1);
    house->branch_mask = branch ? house->branch_mask | bit : house->branch_mask & ~bit;
    discover(house, expected_id);
    return true;
}
bool pet_house_upgrade_v3(pet_house_t *house)
{
    if (!house || house->version != 3 || house->branch_mask || house->branch_seen_mask) return false;
    for (unsigned i = 0; i < PET_ARCHIVE_MAX; i++) if (house->archive[i].branch) return false;
    return pet_house_upgrade_legacy(house);
}
bool pet_house_upgrade_legacy(pet_house_t *house)
{
    if (!house || (house->version != 3 && house->version != 4)) return false;
    if (house->version == 3) {
        if (house->branch_mask || house->branch_seen_mask) return false;
        for (unsigned i = 0; i < PET_ARCHIVE_MAX; i++) if (house->archive[i].branch) return false;
    }
    pet_house_t upgraded = *house;
    memset(upgraded.months, 0, sizeof(upgraded.months));
    upgraded.version = PET_HOUSE_VERSION;
    if (!pet_house_valid(&upgraded)) return false;
    *house = upgraded;
    return true;
}
unsigned pet_house_clear_legacy_archives(pet_house_t *house)
{
    if (!pet_house_valid(house)) return 0;
    unsigned kept = 0, original = house->archive_count;
    for (unsigned i = 0; i < original; i++) {
        if (house->archive[i].species_id) {
            house->months[kept] = house->months[i];
            house->archive[kept++] = house->archive[i];
        }
    }
    if (kept == original) return 0;
    memset(house->archive + kept, 0, (PET_ARCHIVE_MAX - kept) * sizeof(house->archive[0]));
    memset(house->months + kept, 0, (PET_ARCHIVE_MAX - kept) * sizeof(house->months[0]));
    house->archive_count = kept;
    return original - kept;
}
void pet_house_init(pet_house_t *house, const pet_life_t *legacy)
{
    memset(house, 0, sizeof(*house));
    house->version = PET_HOUSE_VERSION;
    pet_life_init(&house->life, NULL);
    if (!legacy) return;
    house->life = *legacy;
    for (unsigned i = 0; i < legacy->family.archive_count; i++) {
        pet_house_archive_t entry = {.result = legacy->family.archive[i]};
        archive(house, entry, (pet_month_record_t){0});
    }
    clear_pantry_archive(house);
    if (legacy->date) {
        house->active_id = PET_AGUMON;
        pet_partner_t *pet = &house->partners[PET_AGUMON - 1];
        pet->adopted = 1;
        pet->adopted_day = legacy->adopted_day;
        memcpy(pet->eaten, legacy->eaten, sizeof(pet->eaten));
        pet->highest_plus_one = pet_house_stage(house, PET_AGUMON) + 1;
    }
}
bool pet_house_valid(const pet_house_t *house)
{
    if (!house || house->version != PET_HOUSE_VERSION || !pet_life_valid(&house->life) ||
        house->life.family.archive_count || house->life.legacy_mask || house->archive_count > PET_ARCHIVE_MAX ||
        (house->branch_mask & ~1U) || (house->branch_seen_mask & ~3U) ||
        (house->branch_mask && !house->partners[PET_AGUMON - 1].adopted)) return false;
    if (house->active_id && (!pet_catalog_find(house->active_id) ||
        !pet_house_partner(house, house->active_id)->adopted)) return false;
    for (unsigned id = 1; id <= PET_PARTNER_CAPACITY; id++) {
        const pet_partner_t *pet = pet_house_partner(house, id);
        if (pet->adopted > 1 || pet->highest_plus_one > PET_STAGE_COUNT) return false;
        /* Older firmware must not overwrite saves containing unknown species. */
        if ((pet->adopted || pet->highest_plus_one) && !pet_catalog_find(id)) return false;
        if (!pet->adopted && (pet->adopted_day || pet_house_meals(house, id))) return false;
        if (pet->adopted && (!pet->highest_plus_one || !pet_house_form_seen(house, id, pet_house_stage(house, id), pet_house_branch(house, id)) ||
            (house->life.date ? (pet->adopted_day < house->life.adopted_day || pet->adopted_day > house->life.date % 100) : pet->adopted_day))) return false;
    }
    for (unsigned d = 0; d < PET_LIFE_DAYS; d++) {
        unsigned allocated = 0;
        for (unsigned id = 1; id <= PET_PARTNER_CAPACITY; id++) allocated += pet_house_partner(house, id)->eaten[d];
        if (allocated != house->life.eaten[d]) return false;
    }
    for (unsigned i = 0; i < house->archive_count; i++) {
        const pet_house_archive_t *entry = &house->archive[i];
        const pet_archive_entry_t *e = &entry->result;
        if ((entry->species_id && !pet_catalog_find(entry->species_id)) || entry->branch > 1 ||
            (entry->branch && (!pet_catalog_branch_supported(entry->species_id) || e->stage < PET_STAGE_TITAN)) ||
            !pet_life_date_valid(e->year * 10000U + e->month * 100U + 1) ||
            e->stage >= PET_STAGE_COUNT || e->route >= PET_ROUTE_COUNT) return false;
        const pet_month_record_t *m = &house->months[i];
        if (m->status > PET_MONTH_SETTLED || m->reserved[0] || m->reserved[1] || m->reserved[2]) return false;
        if (m->status == PET_MONTH_LEGACY) {
            if (m->daily_goal || m->official_tokens || m->month_meals || m->care_days || m->chosen_branch) return false;
            continue;
        }
        if (!entry->species_id || !m->daily_goal || m->daily_goal > 1000000000000ULL ||
            m->official_tokens > 9007199254740991ULL ||
            (m->status == PET_MONTH_PENDING && m->official_tokens) ||
            m->month_meals > 155 || e->food_total > m->month_meals ||
            m->care_days > e->food_total || m->care_days > 31 ||
            ((!m->care_days) != (!e->food_total)) || m->chosen_branch > 1 ||
            (m->chosen_branch && !pet_catalog_branch_supported(entry->species_id)) ||
            entry->branch != (e->stage >= PET_STAGE_TITAN ? m->chosen_branch : 0) ||
            e->year * 100U + e->month >= house->life.date / 100) return false;
        unsigned retained_meals = e->food_total;
        for (unsigned j = 0; j < i; j++) {
            const pet_house_archive_t *other = &house->archive[j];
            if (other->result.year != e->year || other->result.month != e->month) continue;
            const pet_month_record_t *n = &house->months[j];
            if (other->species_id == entry->species_id || n->status != m->status ||
                n->daily_goal != m->daily_goal || n->month_meals != m->month_meals ||
                n->official_tokens != m->official_tokens) return false;
            retained_meals += other->result.food_total;
        }
        if (retained_meals > m->month_meals) return false;
    }
    return true;
}
bool pet_house_sync(pet_house_t *house, const pet_usage_t *usage)
{
    if (!house) return false;
    pet_life_t next = house->life;
    if (!pet_life_sync(&next, usage)) return false;
    bool rollover = house->life.date && next.date / 100 != house->life.date / 100;
    if (rollover) {
        unsigned month_meals = pet_life_meals(&house->life);
        for (unsigned id = 1; id <= PET_PARTNER_CAPACITY; id++) {
            pet_partner_t *pet = &house->partners[id - 1];
            if (pet->adopted) {
                pet_house_archive_t entry = {.species_id = id,
                    .branch = pet_house_stage(house, id) >= PET_STAGE_TITAN ? pet_house_branch(house, id) : 0, .result = {
                    .year = house->life.date / 10000, .month = house->life.date / 100 % 100,
                    .stage = pet_house_stage(house, id), .food_total = pet_house_meals(house, id),
                }};
                pet_month_record_t month = {.daily_goal = house->life.daily_goal,
                    .month_meals = month_meals, .care_days = pet_house_days(house, id),
                    .chosen_branch = pet_house_branch(house, id), .status = PET_MONTH_PENDING};
                archive(house, entry, month);
            }
            uint8_t discovered = pet->highest_plus_one;
            memset(pet, 0, sizeof(*pet));
            pet->highest_plus_one = discovered;
        }
        house->active_id = PET_SPECIES_NONE;
        house->branch_mask = 0;
    }
    house->life = next;
    clear_pantry_archive(house);
    for (unsigned id = 1; id <= PET_PARTNER_CAPACITY; id++) {
        pet_partner_t *pet = &house->partners[id - 1];
        if (pet->adopted && !pet->adopted_day) pet->adopted_day = next.date % 100;
    }
    return true;
}
bool pet_house_settle(pet_house_t *house, uint32_t month, uint64_t tokens, uint32_t covered_through)
{
    if (!pet_house_valid(house) || month < 200001 || month > 209912 ||
        !pet_life_date_valid(month * 100 + 1) || !pet_life_date_valid(covered_through) ||
        month >= house->life.date / 100 || covered_through > house->life.date ||
        tokens > 9007199254740991ULL) return false;
    unsigned days = 28;
    while (days < 31 && pet_life_date_valid(month * 100 + days + 1)) days++;
    if (covered_through < month * 100 + days) return false;
    bool found = false;
    for (unsigned i = 0; i < house->archive_count; i++) {
        const pet_archive_entry_t *e = &house->archive[i].result;
        if (e->year * 100U + e->month != month) continue;
        if (house->months[i].status == PET_MONTH_LEGACY) return false;
        found = true;
    }
    if (!found) return false;
    for (unsigned i = 0; i < house->archive_count; i++) {
        pet_house_archive_t *entry = &house->archive[i];
        pet_archive_entry_t *e = &entry->result;
        if (e->year * 100U + e->month != month) continue;
        pet_month_record_t *m = &house->months[i];
        if (tokens > m->official_tokens) m->official_tokens = tokens;
        m->status = PET_MONTH_SETTLED;
        /* At most five meals/day, shared proportionally. Never create days,
         * feed an untouched egg, or use a shrinking retained-archive denominator. */
        uint64_t budget = (m->official_tokens * 5 + m->daily_goal - 1) / m->daily_goal;
        if (budget > days * 5) budget = days * 5;
        unsigned meals = m->month_meals ? budget * e->food_total / m->month_meals : 0;
        if (meals < e->food_total) meals = e->food_total;
        unsigned stage = 0;
        for (unsigned s = 1; s < PET_STAGE_COUNT; s++) {
            if (meals < pet_catalog_meals(s) || m->care_days < pet_catalog_days(s)) break;
            stage = s;
        }
        if (stage > e->stage) e->stage = stage;
        entry->branch = e->stage >= PET_STAGE_TITAN ? m->chosen_branch : 0;
        if (entry->branch) house->branch_seen_mask |= e->stage == PET_STAGE_APEX ? 3U : 1U;
        else if (house->partners[entry->species_id - 1].highest_plus_one <= e->stage)
            house->partners[entry->species_id - 1].highest_plus_one = e->stage + 1;
    }
    return true;
}
bool pet_house_choose(pet_house_t *house, unsigned id)
{
    if (!house || !pet_catalog_find(id)) return false;
    pet_partner_t *pet = &house->partners[id - 1];
    if (!pet->adopted) {
        pet->adopted = 1;
        pet->adopted_day = house->life.date % 100;
        if (!pet->highest_plus_one) pet->highest_plus_one = 1;
    }
    house->active_id = id;
    return true;
}
bool pet_house_eat(pet_house_t *house)
{
    if (!house || !house->active_id || !pet_catalog_find(house->active_id) ||
        !pet_house_partner(house, house->active_id)->adopted) return false;
    for (unsigned i = 0; i < PET_LIFE_DAYS; i++) {
        if (house->life.eaten[i] == house->life.earned[i]) continue;
        if (!pet_life_eat(&house->life)) return false;
        pet_partner_t *pet = &house->partners[house->active_id - 1];
        pet->eaten[i]++;
        discover(house, house->active_id);
        return true;
    }
    return false;
}
bool pet_house_action(pet_house_t *house, unsigned expected_id, uint32_t expected_month, unsigned choose_id)
{
    if (!house || expected_id != house->active_id || expected_month != house->life.date / 100) return false;
    return choose_id ? pet_house_choose(house, choose_id) : pet_house_eat(house);
}
