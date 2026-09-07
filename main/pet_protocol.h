#pragma once
#include "pet_life.h"
#define PET_LINE_MAX 160U
/* PET2 SYNC YYYYMMDD <today tokens> <daily goal> <31 digits 0..5> */
bool pet_protocol_parse(const char *line, pet_usage_t *usage);
/* PET2 SETTLE YYYYMM <tokens> YYYYMMDD (coverage watermark). */
bool pet_protocol_settlement(const char *line, uint32_t *month, uint64_t *tokens, uint32_t *covered);
