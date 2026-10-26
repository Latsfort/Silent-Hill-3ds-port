#include "sh3ds/school_boss_particles.h"
#include "sh3ds/rng.h"

static int32_t boss_emission_random(int32_t low, int32_t high)
{
    return (int32_t)((uint32_t)sh_rng_rand16() % (uint32_t)(high - low + 1)) + low;
}

#define BOSS_EMISSION_ADD(base, low, high) ((base) + boss_emission_random((low), (high)))

#include "school_boss_emissions_steps.h"
