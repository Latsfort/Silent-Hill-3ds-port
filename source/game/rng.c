#include "sh3ds/rng.h"

static uint32_t s_seed;

/* main/rng.c. */
uint32_t sh_rng_rand32(void)
{
    s_seed = s_seed * 1664525u + 1013904223u;
    return s_seed;
}

int32_t sh_rng_rand16(void) { return (int32_t)(sh_rng_rand32() >> 17); }
uint32_t sh_rng_get_seed(void) { return s_seed; }
void sh_rng_set_seed(uint32_t seed) { s_seed = seed; }
