#include "sh3ds/roof_water.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned frames = 0;
    ShEnvState env = {0};
    env.preset.flags[1] = 255;
    env.preset.world_light = 128 * 32;
    for (int occupied = 0; occupied <= 17; occupied += 17) {
        ShRoofWater actual, expected;
        sh_roof_water_init(&actual);
        assert(actual.pool.particles == actual.particles && actual.pool.count == SH_ROOF_WATER_POOL_COUNT);
        for (int i = 0; i < occupied; i++) { actual.particles[i].field_A = 14; }
        for (int round = 0; round < 4; round++) {
            bool ball = round & 1;
            sh_roof_water_prepare(&actual, ball);
            expected = actual;
            expected.pool.particles = expected.particles;
            const uint32_t seed = 0x12345678 + round;
            sh_rng_set_seed(seed);
            sh_water_pool_begin(&expected.pool, &expected.config);
            sh_water_spray_begin(&expected.pool, expected.spray, 0);
            sh_water_spray_begin(&expected.pool, expected.spray, 1);
            if (ball) { sh_water_spray_begin(&expected.pool, expected.spray, 2); }
            uint32_t final_seed = sh_rng_get_seed();
            sh_rng_set_seed(seed);
            sh_roof_water_begin(&actual);
            assert(sh_rng_get_seed() == final_seed);
            assert(!memcmp(actual.particles, expected.particles, sizeof(actual.particles)));
            assert(actual.pool.next == expected.pool.next && actual.pool.flags == expected.pool.flags);
            for (int tick = 0; tick < 360; tick++) {
                uint32_t before = sh_rng_get_seed();
                ShWaterSurfaceQuad surface[SH_WATER_SURFACE_MAX_QUADS];
                size_t count = sh_water_surface(&expected.config, tick * 17, &env, NULL,
                    surface, SH_WATER_SURFACE_MAX_QUADS, 136);
                ShWaterVisibility visibility = {{0}};
                for (int i = 0; i < expected.pool.count; i++) {
                    if (expected.particles[i].field_A == 27) { visibility.visible[i] = sh_water_particle_step(&expected.particles[i], &expected.config, 136, 17); }
                    else if (expected.particles[i].field_A == 28) { visibility.visible[i] = sh_water_spray_step(&expected.particles[i], expected.spray, 136); }
                }
                final_seed = sh_rng_get_seed();
                sh_rng_set_seed(before);
                assert(sh_roof_water_update(&actual, tick * 17, &env, NULL, 136, 17));
                assert(actual.surface_count == count && !memcmp(actual.surface, surface, count * sizeof(surface[0])));
                assert(!memcmp(actual.particles, expected.particles, sizeof(actual.particles)));
                assert(!memcmp(&actual.visibility, &visibility, sizeof(visibility)));
                assert(!memcmp(&actual.config, &expected.config, sizeof(actual.config)));
                assert(sh_rng_get_seed() == final_seed);
                frames++;
            }
            sh_roof_water_stop(&actual);
            sh_water_pool_stop(&expected.pool);
            assert(!memcmp(actual.particles, expected.particles, sizeof(actual.particles)));
            assert(actual.pool.next == expected.pool.next && actual.pool.flags == expected.pool.flags);
            assert(!actual.surface_count);
            ShWaterConfig stopped_config = actual.config;
            final_seed = sh_rng_get_seed();
            assert(sh_roof_water_update(&actual, 0, &env, NULL, 136, 17));
            assert(!actual.surface_count && !memcmp(&actual.config, &stopped_config, sizeof(stopped_config)));
            assert(sh_rng_get_seed() == final_seed);
            for (int i = 0; i < SH_ROOF_WATER_POOL_COUNT; i++) {
                assert(actual.particles[i].field_A == (i < occupied ? 14 : 0));
                assert(!actual.visibility.visible[i]);
            }
        }
    }
    printf("PASS: %u roof water frames, both circuits, restart/stop, occupied pool and source dispatch order\n", frames);
    return 0;
}
