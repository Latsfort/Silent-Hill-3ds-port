#include "sh3ds/water.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int random16(uint32_t *seed)
{
    *seed = *seed * 1664525u + 1013904223u;
    return (int)(*seed >> 17);
}

static void reference(ShWaterParticle *particle, const ShWaterConfig *config, int mode, int stream, uint32_t *seed)
{
    const int selected = mode == 2 ? random16(seed) % config->field_78 : stream % config->field_78;
    const int along = mode == 2 ? random16(seed) % config->field_28[selected] : 0;
    particle->field_10.s_0.field_0 = along;
    particle->field_0.vx_0 = config->field_0[selected];
    particle->vy_8 = config->field_10[selected];
    particle->field_4.vz_4 = config->field_18[selected];
    if (!config->field_30[selected]) {
        if (mode == 2) {
            particle->field_0.vx_0 += sh_q12_mul(along, sh_fixed_sin(config->field_34[selected]));
            particle->field_4.vz_4 += sh_q12_mul(along, sh_fixed_cos(config->field_34[selected]));
        }
        particle->field_10.s_0.field_2 = config->field_6c[selected] * (random16(seed) % 64 + 96) >> 7;
    } else {
        if (mode == 2) { particle->vy_8 += along; }
        particle->field_10.s_0.field_2 = random16(seed) % 32 + 112;
        particle->field_C.s_0.field_2 = 0;
    }
    const int variant = random16(seed) % 3;
    particle->field_C.s_1.field_0 = (variant ? variant + 1 : 0) + selected * 4;
}

static bool reference_step(ShWaterParticle *p, const ShWaterConfig *c, int dt, int gravity, uint32_t *seed)
{
    const int stream = p->field_C.s_1.field_0 / 4;
    int advance;
    if (!c->field_30[stream]) {
        advance = (int)((int64_t)dt * p->field_10.s_3.field_2 >> 12);
        const int progress = p->field_10.s_3.field_0;
        if (progress + advance >= c->field_28[stream] + c->field_64[stream]) {
            reference(p, c, 3, stream + 1, seed);
            return false;
        }
        const int distance = progress + advance < c->field_28[stream] ? advance :
                             progress < c->field_28[stream] ? c->field_28[stream] - progress : 0;
        p->field_0.vx_0 += sh_q12_mul(distance, sh_fixed_sin(c->field_34[stream]));
        p->field_4.vz_4 += sh_q12_mul(distance, sh_fixed_cos(c->field_34[stream]));
        p->field_10.s_0.field_0 += advance;
    } else {
        int velocity = p->field_C.s_2.field_2 + (gravity * p->field_10.s_3.field_2 >> 7);
        p->field_C.s_0.field_2 = velocity > SH_Q12(6.0f) ? SH_Q12(6.0f) : velocity;
        advance = (int)((int64_t)dt * p->field_C.s_2.field_2 >> 12);
        if (p->field_10.s_3.field_0 + advance >= c->field_28[stream] + c->field_64[stream]) {
            reference(p, c, 3, stream + 1, seed);
            return false;
        }
        p->field_10.s_3.field_0 += advance;
        p->vy_8 += advance;
    }
    return true;
}

int main(void)
{
    unsigned checks = 0;
    unsigned motion_checks = 0, transitions = 0;
    for (int ball = 0; ball <= 1; ball++) {
        ShWaterConfig config = {0};
        for (int i = 0; i < 4; i++) { config.field_54[i] = 0x1234; }
        sh_water_configure_roof(&config, ball);
        assert(config.field_78 == (ball ? 3 : 2) && config.field_74 == (ball ? 150 : 80));
        assert(config.field_28[config.field_78] == SH_Q12(0.5f));
        for (int i = 0; i < 4; i++) { assert(config.field_54[i] == 0x1234); }
        for (unsigned trial = 0; trial < 1024; trial++) {
            for (int mode = 2; mode <= 3; mode++) {
                for (int stream = 0; stream < 6; stream++) {
                    uint32_t expected_seed = trial * 2654435761u;
                    sh_rng_set_seed(expected_seed);
                    ShWaterParticle actual, expected;
                    memset(&actual, 0xA5, sizeof(actual));
                    actual.field_A = 27;
                    expected = actual;
                    reference(&expected, &config, mode, stream, &expected_seed);
                    sh_water_particle_spawn(&actual, &config, mode, stream);
                    assert(memcmp(&actual, &expected, sizeof(actual)) == 0);
                    assert(sh_rng_get_seed() == expected_seed && actual.field_A == 27);
                    checks++;
                }
            }
        }
        for (unsigned trial = 0; trial < 64; trial++) {
            for (int dt = 68; dt <= 272; dt += 68) {
                uint32_t expected_seed = trial + 1;
                sh_rng_set_seed(expected_seed);
                ShWaterParticle actual = {0}, expected = {0};
                actual.field_A = expected.field_A = 27;
                reference(&expected, &config, 2, 0, &expected_seed);
                sh_water_particle_spawn(&actual, &config, 2, 0);
                for (int frame = 0; frame < 1500; frame++) {
                    const int gravity = (frame & 31) * 31;
                    const bool expected_active = reference_step(&expected, &config, dt, gravity, &expected_seed);
                    assert(sh_water_particle_step(&actual, &config, dt, gravity) == expected_active);
                    assert(memcmp(&actual, &expected, sizeof(actual)) == 0);
                    assert(sh_rng_get_seed() == expected_seed);
                    if (!expected_active) { transitions++; }
                    motion_checks++;
                }
            }
        }
    }
    printf("PASS: %u original water spawns, both routes, RNG order and 20-byte union layout\n", checks);
    printf("PASS: %u full particle-state motion comparisons and %u stream transitions\n", motion_checks, transitions);
    return 0;
}
