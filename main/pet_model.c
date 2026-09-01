#include "pet_model.h"

#include <string.h>

static const uint8_t STAGE_THRESHOLDS[PET_STAGE_COUNT] = {
    0U, 5U, 15U, 30U, 50U, 75U, 100U,
};

static bool month_valid(uint16_t year, uint8_t month)
{
    return year >= 2020U && year <= 9999U && month >= 1U && month <= 12U;
}

static void reset_current_month(pet_model_t *model, uint16_t year, uint8_t month)
{
    model->current_year = year;
    model->current_month = month;
    model->progress = 0U;
    model->food_total = 0U;
    model->stage = PET_STAGE_EGG;
    model->route = PET_ROUTE_CORE;
    model->settled = false;
}

static void archive_current(pet_model_t *model)
{
    if (model->archive_count == PET_ARCHIVE_MAX) {
        memmove(&model->archive[0], &model->archive[1],
                sizeof(model->archive[0]) * (PET_ARCHIVE_MAX - 1U));
        model->archive_count--;
    }

    pet_archive_entry_t *entry = &model->archive[model->archive_count++];
    entry->year = model->current_year;
    entry->month = model->current_month;
    entry->stage = model->stage;
    entry->route = model->route;
    entry->progress = model->progress;
    entry->food_total = model->food_total;
}

void pet_model_init(pet_model_t *model)
{
    if (!model) return;
    memset(model, 0, sizeof(*model));
    model->version = PET_MODEL_VERSION;
    model->stage = PET_STAGE_EGG;
    model->route = PET_ROUTE_CORE;
}

pet_stage_t pet_model_stage_for_progress(uint8_t progress)
{
    pet_stage_t stage = PET_STAGE_EGG;
    if (progress > PET_PROGRESS_MAX) progress = PET_PROGRESS_MAX;
    for (size_t i = 1; i < PET_STAGE_COUNT; i++) {
        if (progress < STAGE_THRESHOLDS[i]) break;
        stage = (pet_stage_t)i;
    }
    return stage;
}

bool pet_model_sanitize(pet_model_t *model)
{
    if (!model || model->version != PET_MODEL_VERSION) return false;
    if ((model->current_year != 0U || model->current_month != 0U) &&
        !month_valid(model->current_year, model->current_month)) {
        return false;
    }
    if (model->archive_count > PET_ARCHIVE_MAX) return false;
    if (model->progress > PET_PROGRESS_MAX) model->progress = PET_PROGRESS_MAX;
    if (model->route >= PET_ROUTE_COUNT) model->route = PET_ROUTE_CORE;
    model->stage = pet_model_stage_for_progress(model->progress);

    for (size_t i = 0; i < model->archive_count; i++) {
        pet_archive_entry_t *entry = &model->archive[i];
        if (!month_valid(entry->year, entry->month)) return false;
        if (entry->progress > PET_PROGRESS_MAX) entry->progress = PET_PROGRESS_MAX;
        if (entry->route >= PET_ROUTE_COUNT) entry->route = PET_ROUTE_CORE;
        entry->stage = pet_model_stage_for_progress(entry->progress);
    }
    return true;
}

const char *pet_model_stage_name(pet_stage_t stage)
{
    static const char *const NAMES[PET_STAGE_COUNT] = {
        "EGG", "SPARK", "BYTE", "SCOUT", "RANGER", "TITAN", "APEX",
    };
    return stage < PET_STAGE_COUNT ? NAMES[stage] : "UNKNOWN";
}

const char *pet_model_route_name(pet_route_t route)
{
    static const char *const NAMES[PET_ROUTE_COUNT] = {
        "CORE", "ARMOR", "WILD", "EXPLORER", "GLITCH",
    };
    return route < PET_ROUTE_COUNT ? NAMES[route] : "UNKNOWN";
}

pet_result_t pet_model_begin_month(pet_model_t *model, uint16_t year, uint8_t month)
{
    if (!model || !month_valid(year, month)) return PET_RESULT_INVALID;
    if (model->current_year == 0U && model->current_month == 0U) {
        reset_current_month(model, year, month);
        return PET_RESULT_OK;
    }
    if (model->current_year == year && model->current_month == month) {
        return PET_RESULT_OK;
    }
    if (!model->settled) return PET_RESULT_NEEDS_SETTLEMENT;
    reset_current_month(model, year, month);
    return PET_RESULT_OK;
}

pet_result_t pet_model_apply_feed(pet_model_t *model, uint32_t seq, uint8_t food)
{
    if (!model || food == 0U || food > 5U) return PET_RESULT_INVALID;
    if (model->current_year == 0U || model->current_month == 0U) {
        return PET_RESULT_NOT_STARTED;
    }
    if (model->settled) return PET_RESULT_ALREADY_SETTLED;
    if (seq <= model->last_seq) return PET_RESULT_DUPLICATE;

    uint16_t next_progress = (uint16_t)model->progress + (uint16_t)food * 5U;
    model->progress = next_progress > PET_PROGRESS_MAX
                          ? PET_PROGRESS_MAX
                          : (uint8_t)next_progress;
    uint32_t next_food_total = (uint32_t)model->food_total + food;
    model->food_total = next_food_total > UINT16_MAX
                            ? UINT16_MAX
                            : (uint16_t)next_food_total;
    model->stage = pet_model_stage_for_progress(model->progress);
    model->last_seq = seq;
    return PET_RESULT_OK;
}

pet_result_t pet_model_settle(pet_model_t *model, uint32_t seq, uint16_t year,
                              uint8_t month, uint8_t official_progress,
                              pet_route_t route)
{
    if (!model || !month_valid(year, month) || official_progress > PET_PROGRESS_MAX ||
        route >= PET_ROUTE_COUNT) {
        return PET_RESULT_INVALID;
    }
    if (model->current_year != year || model->current_month != month) {
        return PET_RESULT_INVALID;
    }
    if (model->settled) return PET_RESULT_ALREADY_SETTLED;
    if (seq <= model->last_seq) return PET_RESULT_DUPLICATE;

    if (official_progress > model->progress) model->progress = official_progress;
    model->stage = pet_model_stage_for_progress(model->progress);
    model->route = route;
    model->last_seq = seq;
    model->settled = true;
    archive_current(model);
    return PET_RESULT_OK;
}
