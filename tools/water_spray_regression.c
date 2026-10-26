#include "sh3ds/water.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int draw(uint32_t *seed, int low, int high)
{
    *seed = *seed * 1664525u + 1013904223u;
    return (int)((*seed >> 17) % (unsigned)(high - low + 1)) + low;
}

static void reference(ShWaterParticle *p, const ShWaterSprayConfig *all, int stream, int restart, uint32_t *seed)
{
    const ShWaterSprayConfig *c = &all[stream];
    int x = 0, z = 0, angle = 0;
    switch (c->idx_14) {
    case 0: break;
    case 1: {
        const int offset = draw(seed, 0, c->field_C - 1) - c->field_C / 2;
        x = sh_q12_mul(offset, sh_fixed_sin(c->field_A));
        z = sh_q12_mul(offset, sh_fixed_cos(c->field_A));
        break;
    }
    case 2:
        for (int attempt = 0; attempt < 8; attempt++) {
            x = draw(seed, -c->field_C, c->field_C - 1);
            z = draw(seed, -c->field_C, c->field_C - 1);
            if (x * x + z * z < c->field_C * c->field_C) { break; }
            if (attempt == 7) { x = z = 0; }
        }
        break;
    case 3:
    case 4:
        angle = c->idx_14 == 3 ? draw(seed, 0, 4095) : c->field_A + draw(seed, -1024, 1023);
        x = sh_q12_mul(c->field_C, sh_fixed_sin(angle));
        z = sh_q12_mul(c->field_C, sh_fixed_cos(angle));
        break;
    }
    p->field_0.vx_0 = c->vx_0 + x;
    p->field_4.vz_4 = c->vz_4 + z;
    p->vy_8 = c->field_8;
    p->field_C.s_0.field_0 = restart ? p->field_C.field_0 - c->field_10 : draw(seed, 0, c->field_10 - 1);
    p->field_C.s_1.field_2 = draw(seed, 0, 3);
    p->field_C.s_1.field_3 = draw(seed, 96, 159);
    p->field_10.s_2.field_0 = stream;
}

int main(void)
{
    unsigned spawns = 0, steps = 0;
    for (int ball = 0; ball <= 1; ball++) {
        ShWaterConfig water = {0};
        ShWaterSprayConfig spray[3] = {{0}};
        sh_water_configure_roof(&water, ball);
        sh_water_spray_configure_roof(spray, ball);
        ShWaterParticle particles[SH_ROOF_WATER_POOL_COUNT];
        memset(particles, 0, sizeof(particles));
        ShWaterPool pool = {particles, SH_ROOF_WATER_POOL_COUNT, 0, 0};
        sh_water_pool_begin(&pool, &water);
        for (int stream = 0; stream < (ball ? 3 : 2); stream++) { sh_water_spray_begin(&pool, spray, stream); }
        unsigned count = 0;
        for (int i = 0; i < pool.count; i++) { count += particles[i].field_A != 0; }
        assert(count == (ball ? 290u : 160u));
        sh_water_pool_stop(&pool);
        for (int i = 0; i < pool.count; i++) { assert(!particles[i].field_A); }
        for (int shape = 0; shape < 5; shape++) {
            spray[0].idx_14 = shape;
            for (unsigned trial = 0; trial < 1024; trial++) {
                for (int restart = 0; restart <= 1; restart++) {
                    ShWaterParticle actual, expected;
                    memset(&actual, 0xA5, sizeof(actual));
                    actual.field_A = 28;
                    expected = actual;
                    uint32_t seed = trial * 2654435761u;
                    sh_rng_set_seed(seed);
                    reference(&expected, spray, 0, restart, &seed);
                    sh_water_spray_spawn(&actual, spray, 0, restart);
                    assert(memcmp(&actual, &expected, sizeof(actual)) == 0 && sh_rng_get_seed() == seed);
                    spawns++;
                    if (restart) { continue; }
                    for (int frame = 0; frame < 100; frame++) {
                        const int dt = 68 + (frame % 4) * 68;
                        expected.field_C.s_0.field_0 += dt;
                        const bool active = (uint16_t)expected.field_C.s_0.field_0 <= spray[0].field_10;
                        if (!active) { reference(&expected, spray, 0, 1, &seed); }
                        assert(sh_water_spray_step(&actual, spray, dt) == active);
                        assert(memcmp(&actual, &expected, sizeof(actual)) == 0 && sh_rng_get_seed() == seed);
                        steps++;
                    }
                }
            }
        }
    }
    printf("PASS: %u spray spawns, %u lifespan steps, all five shapes and original 160/290 particle budgets\n", spawns, steps);
    return 0;
}
