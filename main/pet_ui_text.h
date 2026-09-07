#pragma once
#include "pet_model.h"

/* Display-only names: persisted enums and wire-protocol names stay unchanged. */
const char *pet_ui_stage_name(pet_stage_t stage);
const char *pet_ui_stage_level(pet_stage_t stage);
const char *pet_ui_legacy_stage_name(pet_stage_t stage);
const char *pet_ui_route_name(pet_route_t route);
const char *pet_ui_route_hint(pet_route_t route);

/* Read-only next-form requirements, not a second XP or persisted level system.
 * percent is the lesser of capped meal/day target completion (not stage XP).
 * A previous display stage may be supplied during an evolution animation. */
typedef struct {
    unsigned percent, meals_left, days_left;
    bool final_form;
} pet_ui_progress_t;
pet_ui_progress_t pet_ui_progress(unsigned stage, unsigned meals, unsigned days);

/* Display-only helpers; zero targets are unavailable, not complete. */
unsigned pet_ui_target_percent(unsigned value, unsigned target);
/* Exact below 10,000; larger counts are explicitly approximate, truncated to
 * one decimal. Integer-only and safe up to UINT64_MAX. Does not alter sync data. */
void pet_ui_format_tokens(char *out, size_t size, uint64_t value);
