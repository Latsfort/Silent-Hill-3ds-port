#include "sh3ds/roof_water.h"
#include <string.h>

void sh_roof_water_init(ShRoofWater *water)
{
    memset(water, 0, sizeof(*water));
    water->pool.particles = water->particles;
    water->pool.count = SH_ROOF_WATER_POOL_COUNT;
}

/* DrainageValveInteract case 4. */
void sh_roof_water_prepare(ShRoofWater *water, bool ball)
{
    water->ball = ball;
    sh_water_configure_roof(&water->config, ball);
    sh_water_spray_configure_roof(water->spray, ball);
}

/* DrainageValveInteract case 8. */
void sh_roof_water_begin(ShRoofWater *water)
{
    sh_water_pool_begin(&water->pool, &water->config);
    sh_water_spray_begin(&water->pool, water->spray, 0);
    sh_water_spray_begin(&water->pool, water->spray, 1);
    if (water->ball) { sh_water_spray_begin(&water->pool, water->spray, 2); }
}

/* DrainageValveInteract case 11. */
void sh_roof_water_stop(ShRoofWater *water)
{
    sh_water_pool_stop(&water->pool);
    water->surface_count = 0;
    memset(&water->visibility, 0, sizeof(water->visibility));
}

/* Gfx_EffectsUpdate: func_9C precedes the ascending particle pass. */
bool sh_roof_water_update(ShRoofWater *water, int yaw, const ShEnvState *environment,
    const ShEnvLight *light, int32_t dt, int32_t gravity)
{
    water->surface_count = 0;
    if (water->pool.flags & (1u << 3)) {
        water->surface_count = sh_water_surface(&water->config, yaw, environment, light,
            water->surface, SH_WATER_SURFACE_MAX_QUADS, dt);
    }
    return sh_water_particles_step(&water->pool, &water->config, water->spray, dt, gravity, &water->visibility);
}
