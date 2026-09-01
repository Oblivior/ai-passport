#include "pet_model.h"

#include <assert.h>
#include <stdio.h>

static void test_stage_thresholds(void)
{
    assert(pet_model_stage_for_progress(0) == PET_STAGE_EGG);
    assert(pet_model_stage_for_progress(4) == PET_STAGE_EGG);
    assert(pet_model_stage_for_progress(5) == PET_STAGE_SPARK);
    assert(pet_model_stage_for_progress(14) == PET_STAGE_SPARK);
    assert(pet_model_stage_for_progress(15) == PET_STAGE_BYTE);
    assert(pet_model_stage_for_progress(30) == PET_STAGE_SCOUT);
    assert(pet_model_stage_for_progress(50) == PET_STAGE_RANGER);
    assert(pet_model_stage_for_progress(75) == PET_STAGE_TITAN);
    assert(pet_model_stage_for_progress(100) == PET_STAGE_APEX);
}

static void test_feed_is_idempotent(void)
{
    pet_model_t model;
    pet_model_init(&model);
    assert(pet_model_begin_month(&model, 2026, 9) == PET_RESULT_OK);
    assert(pet_model_apply_feed(&model, 1, 1) == PET_RESULT_OK);
    assert(model.progress == 5);
    assert(model.stage == PET_STAGE_SPARK);
    assert(pet_model_apply_feed(&model, 1, 1) == PET_RESULT_DUPLICATE);
    assert(model.progress == 5);
    assert(pet_model_apply_feed(&model, 2, 5) == PET_RESULT_OK);
    assert(model.progress == 30);
    assert(model.stage == PET_STAGE_SCOUT);
}

static void test_settlement_never_downgrades(void)
{
    pet_model_t model;
    pet_model_init(&model);
    assert(pet_model_begin_month(&model, 2026, 9) == PET_RESULT_OK);
    assert(pet_model_apply_feed(&model, 1, 5) == PET_RESULT_OK);
    assert(pet_model_apply_feed(&model, 2, 5) == PET_RESULT_OK);
    assert(model.progress == 50);
    assert(pet_model_settle(&model, 3, 2026, 9, 30, PET_ROUTE_ARMOR) == PET_RESULT_OK);
    assert(model.progress == 50);
    assert(model.stage == PET_STAGE_RANGER);
    assert(model.route == PET_ROUTE_ARMOR);
    assert(model.archive_count == 1);
    assert(model.archive[0].stage == PET_STAGE_RANGER);
    assert(model.archive[0].route == PET_ROUTE_ARMOR);
    assert(pet_model_apply_feed(&model, 4, 1) == PET_RESULT_ALREADY_SETTLED);
}

static void test_month_requires_settlement(void)
{
    pet_model_t model;
    pet_model_init(&model);
    assert(pet_model_begin_month(&model, 2026, 9) == PET_RESULT_OK);
    assert(pet_model_begin_month(&model, 2026, 10) == PET_RESULT_NEEDS_SETTLEMENT);
    assert(pet_model_settle(&model, 1, 2026, 9, 15, PET_ROUTE_CORE) == PET_RESULT_OK);
    assert(pet_model_begin_month(&model, 2026, 10) == PET_RESULT_OK);
    assert(model.progress == 0);
    assert(model.stage == PET_STAGE_EGG);
    assert(model.archive_count == 1);
}

static void test_sanitize_rejects_corrupt_month(void)
{
    pet_model_t model;
    pet_model_init(&model);
    model.current_year = 2026;
    model.current_month = 13;
    assert(!pet_model_sanitize(&model));
}

static void test_food_total_saturates(void)
{
    pet_model_t model;
    pet_model_init(&model);
    assert(pet_model_begin_month(&model, 2026, 9) == PET_RESULT_OK);
    model.food_total = UINT16_MAX - 1U;
    assert(pet_model_apply_feed(&model, 1, 5) == PET_RESULT_OK);
    assert(model.food_total == UINT16_MAX);
}

int main(void)
{
    test_stage_thresholds();
    test_feed_is_idempotent();
    test_settlement_never_downgrades();
    test_month_requires_settlement();
    test_sanitize_rejects_corrupt_month();
    test_food_total_saturates();
    puts("pet_model tests: PASS");
    return 0;
}
