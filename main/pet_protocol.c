#include "pet_protocol.h"
#include <string.h>

static bool number(const char **cursor, uint64_t *value)
{
    const char *p = *cursor;
    uint64_t n = 0;
    unsigned digits = 0;
    while (*p >= '0' && *p <= '9') {
        unsigned d = *p++ - '0';
        if (n > (UINT64_MAX - d) / 10 || ++digits > 16) return false;
        n = n * 10 + d;
    }
    if (!digits || *p++ != ' ') return false;
    *cursor = p;
    *value = n;
    return true;
}

bool pet_protocol_parse(const char *line, pet_usage_t *usage)
{
    if (!line || !usage || strlen(line) >= PET_LINE_MAX || strncmp(line, "PET2 SYNC ", 10)) return false;
    pet_usage_t parsed = {0};
    const char *p = line + 10;
    uint64_t date;
    if (!number(&p, &date) || date > UINT32_MAX ||
        !number(&p, &parsed.tokens_today) || !number(&p, &parsed.daily_goal) ||
        strlen(p) != PET_LIFE_DAYS) return false;
    parsed.date = date;
    if (!pet_life_date_valid(parsed.date) || !parsed.daily_goal ||
        parsed.daily_goal > 1000000000000ULL || parsed.tokens_today > 9007199254740991ULL) return false;
    for (unsigned i = 0; i < PET_LIFE_DAYS; i++) {
        if (p[i] < '0' || p[i] > '5' || (i >= parsed.date % 100 && p[i] != '0')) return false;
        parsed.earned[i] = p[i] - '0';
    }
    *usage = parsed;
    return true;
}
