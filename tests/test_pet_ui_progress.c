#include "pet_ui_text.h"
#include "pet_catalog.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void token_format_tests(void)
{
    static const struct { uint64_t value; const char *text; } cases[] = {
        {0, "0"}, {9999, "9999"}, {10000, "约1.0万"}, {19999, "约1.9万"},
        {99999999, "约9999.9万"}, {100000000, "约1.0亿"},
        {999999999999ULL, "约9999.9亿"}, {1000000000000ULL, "约1.0万亿"},
        {9007199254740991ULL, "约9007.1万亿"}, {UINT64_MAX, "约18446744.0万亿"},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char text[40];
        pet_ui_format_tokens(text, sizeof(text), cases[i].value);
        assert(strcmp(text, cases[i].text) == 0);
    }
    char tiny[2] = {'x', 'y'};
    pet_ui_format_tokens(tiny, 1, UINT64_MAX);
    assert(tiny[0] == '\0' && tiny[1] == 'y');
    pet_ui_format_tokens(tiny, 0, 42);
    assert(tiny[0] == '\0' && tiny[1] == 'y');
    assert(pet_ui_target_percent(1, 0) == 0);
    assert(pet_ui_target_percent(1, 3) == 33);
    assert(pet_ui_target_percent(6, 3) == 100);
    assert(pet_ui_target_percent(UINT_MAX - 1, UINT_MAX) == 99);
}

int main(void)
{
    token_format_tests();
    pet_ui_progress_t p = pet_ui_progress(PET_STAGE_SPARK, 5, 1);
    assert(p.percent == 50 && p.meals_left == 0 && p.days_left == 1 && !p.final_form);
    for (unsigned stage = 0; stage < PET_STAGE_APEX; stage++) {
        unsigned meals = pet_catalog_meals(stage + 1), days = pet_catalog_days(stage + 1);
        p = pet_ui_progress(stage, 0, 0);
        assert(p.percent == 0 && p.meals_left == meals && p.days_left == days);
        p = pet_ui_progress(stage, meals - 1, days);
        assert(p.percent < 100 && p.meals_left == 1 && p.days_left == 0);
        p = pet_ui_progress(stage, meals, days - 1);
        assert(p.percent < 100 && p.meals_left == 0 && p.days_left == 1);
        p = pet_ui_progress(stage, meals, days);
        assert(p.percent == 100 && !p.meals_left && !p.days_left && !p.final_form);
        p = pet_ui_progress(stage, UINT_MAX, UINT_MAX);
        assert(p.percent == 100 && !p.meals_left && !p.days_left);
    }
    p = pet_ui_progress(PET_STAGE_APEX, 40, 16);
    assert(p.final_form && p.percent == 100 && !p.meals_left && !p.days_left);
    assert(pet_ui_progress(UINT_MAX, 0, 0).final_form);
    puts("pet_ui_progress: PASS (dual requirements, transitions, caps, final form, compact Token boundaries)");
}
