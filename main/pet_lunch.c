#include "pet_lunch.h"

pet_lunch_t pet_lunch_read(const pet_life_t *life)
{
    pet_lunch_t lunch = {0};
    if (!life || !pet_life_date_valid(life->date) || !life->daily_goal ||
        life->daily_goal > 1000000000000ULL) return lunch;
    unsigned day = life->date % 100 - 1;
    if (life->earned[day] > 5 || life->eaten[day] > life->earned[day]) return lunch;
    lunch.available = true;
    lunch.earned = life->earned[day];
    lunch.eaten = life->eaten[day];
    for (unsigned i = 0; i < day; i++) {
        if (life->earned[i] > life->eaten[i]) lunch.older_pending += life->earned[i] - life->eaten[i];
    }
    if (lunch.earned == 5) { lunch.progress = 100; return lunch; }
    /* Companion uses ceil(5*tokens/goal), so a new meal requires STRICTLY
     * exceeding the boundary. Retained entitlements survive downward fixes. */
    lunch.next_tokens = life->daily_goal * lunch.earned / 5 + 1;
    uint64_t start = lunch.earned ? life->daily_goal * (lunch.earned - 1) / 5 + 1 : 0;
    uint64_t tokens = life->tokens_today;
    lunch.remaining_tokens = tokens < lunch.next_tokens ? lunch.next_tokens - tokens : 0;
    if (tokens >= lunch.next_tokens) lunch.progress = 100;
    else if (tokens > start) lunch.progress = (unsigned)((tokens - start) * 100 / (lunch.next_tokens - start));
    return lunch;
}
