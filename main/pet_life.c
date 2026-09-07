#include "pet_life.h"

#include <string.h>

static const uint8_t MEALS[] = {0, 1, 3, 6, 12, 24, 40};
static const uint8_t DAYS[] = {0, 1, 2, 3, 6, 10, 16};

bool pet_life_date_valid(uint32_t date)
{
    unsigned y = date / 10000, m = date / 100 % 100, d = date % 100;
    static const uint8_t lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (y < 2020 || y > 2099 || m < 1 || m > 12 || d < 1) return false;
    unsigned max = lengths[m - 1] + (m == 2 && y % 4 == 0);
    return d <= max;
}

unsigned pet_life_meals(const pet_life_t *life)
{
    unsigned n = 0;
    for (unsigned i = 0; i < PET_LIFE_DAYS; i++) n += life->eaten[i];
    return n;
}

unsigned pet_life_pending(const pet_life_t *life)
{
    unsigned n = 0;
    for (unsigned i = 0; i < PET_LIFE_DAYS; i++) n += life->earned[i] - life->eaten[i];
    return n;
}

unsigned pet_life_days(const pet_life_t *life)
{
    unsigned n = 0;
    /* Days are credited only when their earned food is actually eaten. */
    for (unsigned i = 0; i < PET_LIFE_DAYS; i++) n += life->eaten[i] > 0;
    return n;
}

static uint8_t stage_for(const pet_life_t *life)
{
    unsigned meals = pet_life_meals(life), days = pet_life_days(life);
    uint8_t stage = 0;
    for (unsigned i = 1; i < PET_STAGE_COUNT; i++) {
        if (meals < MEALS[i] || days < DAYS[i]) break;
        stage = i;
    }
    return stage;
}

unsigned pet_life_next_meals(const pet_life_t *life)
{
    return MEALS[life->stage < PET_STAGE_APEX ? life->stage + 1 : PET_STAGE_APEX];
}

unsigned pet_life_next_days(const pet_life_t *life)
{
    return DAYS[life->stage < PET_STAGE_APEX ? life->stage + 1 : PET_STAGE_APEX];
}

static void archive(pet_life_t *life, pet_archive_entry_t entry, bool legacy)
{
    if (life->family.archive_count == PET_ARCHIVE_MAX) {
        memmove(life->family.archive, life->family.archive + 1,
                (PET_ARCHIVE_MAX - 1) * sizeof(entry));
        life->family.archive_count--;
        life->legacy_mask >>= 1;
    }
    unsigned at = life->family.archive_count++;
    life->family.archive[at] = entry;
    if (legacy) life->legacy_mask |= 1U << at;
}

void pet_life_init(pet_life_t *life, const pet_model_t *legacy)
{
    memset(life, 0, sizeof(*life));
    life->version = PET_LIFE_VERSION;
    pet_model_init(&life->family);
    if (!legacy) return;
    for (unsigned i = 0; i < legacy->archive_count; i++) archive(life, legacy->archive[i], true);
    if (legacy->current_year && !legacy->settled) {
        pet_archive_entry_t entry = {
            .year = legacy->current_year, .month = legacy->current_month,
            .stage = legacy->stage, .route = legacy->route,
            .progress = legacy->progress, .food_total = legacy->food_total,
        };
        archive(life, entry, true);
    }
}

bool pet_life_valid(const pet_life_t *life)
{
    if (!life || life->version != PET_LIFE_VERSION || life->family.archive_count > PET_ARCHIVE_MAX ||
        life->stage >= PET_STAGE_COUNT) return false;
    if (life->date && (!pet_life_date_valid(life->date) || !life->adopted_day ||
        life->adopted_day > life->date % 100 || !life->daily_goal)) return false;
    for (unsigned i = 0; i < PET_LIFE_DAYS; i++) {
        if (life->earned[i] > 5 || life->eaten[i] > life->earned[i]) return false;
        if ((!life->date || i + 1 < life->adopted_day || i + 1 > life->date % 100) && life->earned[i]) return false;
    }
    for (unsigned i = 0; i < life->family.archive_count; i++) {
        const pet_archive_entry_t *e = &life->family.archive[i];
        if (!pet_life_date_valid(e->year * 10000U + e->month * 100U + 1) || e->stage >= PET_STAGE_COUNT) return false;
    }
    return life->stage == stage_for(life);
}

bool pet_life_sync(pet_life_t *life, const pet_usage_t *usage)
{
    if (!life || !usage || !pet_life_date_valid(usage->date) || !usage->daily_goal ||
        usage->daily_goal > 1000000000000ULL || usage->tokens_today > 9007199254740991ULL ||
        usage->date < life->date) return false;
    unsigned day = usage->date % 100;
    for (unsigned i = 0; i < PET_LIFE_DAYS; i++) {
        if (usage->earned[i] > 5 || (i >= day && usage->earned[i])) return false;
    }
    bool new_month = usage->date / 100 != life->date / 100;
    if (!new_month && life->daily_goal != usage->daily_goal) return false;
    if (new_month) {
        if (life->date) {
            pet_archive_entry_t entry = {
                .year = life->date / 10000, .month = life->date / 100 % 100,
                .stage = life->stage, .route = PET_ROUTE_CORE,
                .food_total = pet_life_meals(life),
            };
            archive(life, entry, false); /* Local record, never labeled Bits official. */
        }
        memset(life->earned, 0, sizeof(life->earned));
        memset(life->eaten, 0, sizeof(life->eaten));
        life->stage = PET_STAGE_EGG;
        /* First adoption excludes previous history; later months can catch up. */
        life->adopted_day = life->date ? 1 : day;
        life->daily_goal = usage->daily_goal;
    }
    life->date = usage->date;
    life->tokens_today = usage->tokens_today;
    for (unsigned i = life->adopted_day - 1; i < day; i++) {
        if (usage->earned[i] > life->earned[i]) life->earned[i] = usage->earned[i];
    }
    return true;
}

bool pet_life_eat(pet_life_t *life)
{
    if (!life) return false;
    for (unsigned i = 0; i < PET_LIFE_DAYS; i++) {
        if (life->eaten[i] >= life->earned[i]) continue;
        life->eaten[i]++;
        life->stage = stage_for(life);
        return true;
    }
    return false;
}
