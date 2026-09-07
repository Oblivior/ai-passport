#pragma once
#include "pet_life.h"
#define PET_LINE_MAX 160U
/* PET2 SYNC YYYYMMDD <today tokens> <daily goal> <31 digits 0..5> */
bool pet_protocol_parse(const char *line, pet_usage_t *usage);
