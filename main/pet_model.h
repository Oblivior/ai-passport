#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PET_MODEL_VERSION 1U
#define PET_ARCHIVE_MAX 12U
#define PET_PROGRESS_MAX 100U

typedef enum {
    PET_STAGE_EGG = 0,
    PET_STAGE_SPARK,
    PET_STAGE_BYTE,
    PET_STAGE_SCOUT,
    PET_STAGE_RANGER,
    PET_STAGE_TITAN,
    PET_STAGE_APEX,
    PET_STAGE_COUNT,
} pet_stage_t;

typedef enum {
    PET_ROUTE_CORE = 0,
    PET_ROUTE_ARMOR,
    PET_ROUTE_WILD,
    PET_ROUTE_EXPLORER,
    PET_ROUTE_GLITCH,
    PET_ROUTE_COUNT,
} pet_route_t;

typedef enum {
    PET_RESULT_OK = 0,
    PET_RESULT_DUPLICATE,
    PET_RESULT_INVALID,
    PET_RESULT_NOT_STARTED,
    PET_RESULT_ALREADY_SETTLED,
    PET_RESULT_NEEDS_SETTLEMENT,
} pet_result_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t stage;
    uint8_t route;
    uint8_t progress;
    uint16_t food_total;
} pet_archive_entry_t;

typedef struct {
    uint32_t version;
    uint32_t last_seq;
    uint16_t current_year;
    uint8_t current_month;
    uint8_t progress;
    uint16_t food_total;
    uint8_t stage;
    uint8_t route;
    bool settled;
    uint8_t archive_count;
    pet_archive_entry_t archive[PET_ARCHIVE_MAX];
} pet_model_t;

void pet_model_init(pet_model_t *model);
bool pet_model_sanitize(pet_model_t *model);
pet_stage_t pet_model_stage_for_progress(uint8_t progress);
const char *pet_model_stage_name(pet_stage_t stage);
const char *pet_model_route_name(pet_route_t route);

pet_result_t pet_model_begin_month(pet_model_t *model, uint16_t year, uint8_t month);
pet_result_t pet_model_apply_feed(pet_model_t *model, uint32_t seq, uint8_t food);
pet_result_t pet_model_settle(pet_model_t *model, uint32_t seq, uint16_t year,
                              uint8_t month, uint8_t official_progress,
                              pet_route_t route);
