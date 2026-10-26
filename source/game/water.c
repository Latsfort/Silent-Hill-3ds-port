#include "sh3ds/water.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"
#include <stdlib.h>

/* main/rng.h Rng_GenerateUInt. */
static int32_t water_random_range(int32_t low, int32_t high)
{
    return (int32_t)((uint32_t)sh_rng_rand16() % (uint32_t)(high - low + 1)) + low;
}

#include "water_spawn.h"

static int32_t water_min(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

#include "water_motion.h"

static int32_t water_random_add(int32_t base, int32_t low, int32_t high)
{
    return base + water_random_range(low, high);
}

#define WATER_FROM(value, shift) ((value) >> (shift))
#define WATER_TO(value, shift) ((value) << (shift))
#define WATER_SQUARE(value) ((value) * (value))
#include "water_spray.h"

#define RIBBON_Q12_TO_Q6(value) ((value) >> 6)
#define RIBBON_Q12_TO_Q8(value) ((value) >> 4)
#define RIBBON_TO_FIXED(value, shift) ((value) * (1 << (shift)))
#define RIBBON_Q12(value) ((value) * 4096)
#include "water_ribbon.h"

/* Gfx_EffectsUpdate cases 27/28: update and draw in ascending pool order. */
bool sh_water_particles_step(ShWaterPool *pool, const ShWaterConfig *config,
    const ShWaterSprayConfig *spray, int32_t dt, int32_t gravity, ShWaterVisibility *visibility)
{
    if (!pool || !pool->particles || !config || !spray || !visibility ||
        pool->count < 0 || pool->count > SH_ROOF_WATER_POOL_COUNT) { return false; }
    for (int i = 0; i < SH_ROOF_WATER_POOL_COUNT; i++) { visibility->visible[i] = 0; }
    for (int i = 0; i < pool->count; i++) {
        ShWaterParticle *particle = &pool->particles[i];
        if (particle->field_A == 27) {
            visibility->visible[i] = sh_water_particle_step(particle, config, dt, gravity);
        } else if (particle->field_A == 28) {
            visibility->visible[i] = sh_water_spray_step(particle, spray, dt);
        }
    }
    return true;
}
