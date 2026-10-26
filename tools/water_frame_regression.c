#include "sh3ds/water.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned checks = 0, hidden = 0;
    for (int ball = 0; ball <= 1; ball++) {
        for (int dt = 68; dt <= 272; dt *= 2) {
            ShWaterConfig config = {0};
            ShWaterSprayConfig spray[3] = {{0}};
            ShWaterParticle actual[SH_ROOF_WATER_POOL_COUNT] = {0};
            ShWaterParticle expected[SH_ROOF_WATER_POOL_COUNT];
            ShWaterPool pool = {actual, SH_ROOF_WATER_POOL_COUNT, 0, 0};
            sh_water_configure_roof(&config, ball);
            sh_water_spray_configure_roof(spray, ball);
            sh_rng_set_seed(0x12345678);
            sh_water_pool_begin(&pool, &config);
            for (int stream = 0; stream < (ball ? 3 : 2); stream++) { sh_water_spray_begin(&pool, spray, stream); }
            memcpy(expected, actual, sizeof(actual));
            for (int tick = 0; tick < 600; tick++) {
                const uint32_t seed = sh_rng_get_seed();
                uint8_t visible[SH_ROOF_WATER_POOL_COUNT] = {0};
                for (int i = 0; i < SH_ROOF_WATER_POOL_COUNT; i++) {
                    if (expected[i].field_A == 27) { visible[i] = sh_water_particle_step(&expected[i], &config, dt, 17); }
                    if (expected[i].field_A == 28) { visible[i] = sh_water_spray_step(&expected[i], spray, dt); }
                }
                const uint32_t final_seed = sh_rng_get_seed();
                sh_rng_set_seed(seed);
                ShWaterVisibility visibility;
                memset(&visibility, 0xA5, sizeof(visibility));
                assert(sh_water_particles_step(&pool, &config, spray, dt, 17, &visibility));
                assert(sh_rng_get_seed() == final_seed);
                assert(!memcmp(actual, expected, sizeof(actual)));
                assert(!memcmp(visible, visibility.visible, sizeof(visible)));
                for (int i = 0; i < SH_ROOF_WATER_POOL_COUNT; i++) {
                    if ((actual[i].field_A == 27 || actual[i].field_A == 28) && !visible[i]) { hidden++; }
                    checks++;
                }
            }
            ShWaterVisibility visibility;
            memset(&visibility, 0xA5, sizeof(visibility));
            pool.count++;
            uint32_t seed = sh_rng_get_seed();
            assert(!sh_water_particles_step(&pool, &config, spray, dt, 17, &visibility));
            assert(!memcmp(actual, expected, sizeof(actual)) && sh_rng_get_seed() == seed);
            assert(visibility.visible[0] == 0xA5);
            pool.count = 0;
            assert(sh_water_particles_step(&pool, &config, spray, dt, 17, &visibility));
            for (int i = 0; i < SH_ROOF_WATER_POOL_COUNT; i++) { assert(!visibility.visible[i]); }
        }
    }
    assert(hidden);
    printf("PASS: %u ordered particle updates, RNG equivalence and %u hidden restart frames\n", checks, hidden);
    return 0;
}
