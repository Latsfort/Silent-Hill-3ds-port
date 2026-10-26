#include "sh3ds/smoke.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"

#include "school_smoke_data.h"

/* sharedFunc_800CB8A0_1_s01. */
static void spawn(ShSmoke *smoke, ShSmokeParticle *particle)
{
    const ShSmokeConfig *config = &smoke->config;
    if (!config->disable_spawn) {
        particle->kind = 14;
    }
    if (!config->shape) {
        const int16_t radius = sh_rng_rand16() % config->spread;
        const int32_t angle = sh_rng_rand16() % 4096;
        particle->x = (radius * sh_fixed_cos(angle)) >> 12;
        particle->z = (radius * sh_fixed_sin(angle)) >> 12;
    } else {
        particle->x = sh_rng_rand16() % (config->spread * 2) - config->spread;
        particle->z = sh_rng_rand16() % (config->spread * 2) - config->spread;
    }
    particle->y = config->base;
    particle->variant = sh_rng_rand16() % 3;
    particle->age = 0;
}

/* sharedFunc_800CB7F4_1_s01. */
void sh_smoke_begin_acid(ShSmoke *smoke)
{
    smoke->config = s_school_acid_smoke;
    int count = smoke->config.count;
    for (int i = 0; i < SH_SMOKE_CAPACITY; i++) {
        ShSmokeParticle *particle = &smoke->particles[i];
        if (particle->kind) { continue; }
        particle->kind = 13;
        particle->age = (((smoke->config.ceiling - smoke->config.base) * 4096 / smoke->config.speed) * count) /
                         smoke->config.count;
        if (!--count) { break; }
    }
    smoke->config.phase = 0;
    smoke->accumulator = 0.0f;
    smoke->active = true;
}

/* sharedFunc_800CBB30_1_s01, before GTE projection. */
static bool particle_step(ShSmoke *smoke, ShSmokeParticle *particle, int32_t delta)
{
    const ShSmokeConfig *config = &smoke->config;
    const int brightness = 64 - (particle->age >> 6);
    const int phase = config->phase * 2 < 1024 ? config->phase * 2 : 1024;
    particle->shade = brightness >= 0 ? sh_q12_mul(brightness, (1024 - phase) * 4) : 0;
    const int32_t x = particle->x >> 6, z = particle->z >> 6;
    const int32_t speed = sh_q12_mul(delta, config->speed);
    particle->y = (int16_t)(particle->y + speed);
    const int32_t radius = sh_q12_mul(sh_fixed_sqrt0((uint32_t)(x * x + z * z)) << 10, config->spread);
    const int32_t random = sh_rng_rand16();
    /* Retail 0x800CBDB4 DIV has no software trap: zero divisor leaves HI = dividend. */
    const int32_t remainder = radius ? random % radius : random;
    const int32_t outward = sh_q12_mul(delta, remainder - (radius >> 2)) >> 1;
    const int32_t angle = sh_fixed_atan2(particle->x, particle->z);
    particle->x += (outward * sh_fixed_cos(angle)) >> 12;
    particle->z += (outward * sh_fixed_sin(angle)) >> 12;
    if (outward > 0) { particle->age += outward; }
    const int32_t denominator = ((sh_fixed_cos(config->phase) * config->ceiling) >> 12) - config->base;
    const int32_t aging = speed * 4096 / denominator;
    if (aging > 0) { particle->age += aging; }
    if (particle->age > 4096 || config->phase == 1024) {
        if (!config->emission_time && sh_fixed_cos(config->phase) < sh_rng_rand16() % 4096) {
            particle->kind = 0;
        } else {
            spawn(smoke, particle);
        }
        return false;
    }
    return true;
}

void sh_smoke_step(ShSmoke *smoke, int32_t delta)
{
    if (!smoke->active) { return; }
    ShSmokeConfig *config = &smoke->config;
    /* bodyprog_effects_8005E0DC.c: effect flag 1<<1. */
    if (config->emission_time > 0) {
        config->emission_time -= delta;
        if (config->emission_time < 0) { config->emission_time = 0; }
    } else {
        const int phase = config->phase + sh_q12_mul(delta, config->fade_rate);
        config->phase = phase < 1024 ? phase : 1024;
    }
    for (int i = 0; i < SH_SMOKE_CAPACITY; i++) {
        ShSmokeParticle *particle = &smoke->particles[i];
        particle->visible = false;
        if (particle->kind == 13) {
            particle->age += sh_q12_mul(delta, sh_rng_rand16() % SH_Q12(144.0f / 360.0f) + SH_Q12(288.0f / 360.0f));
            if ((config->ceiling - config->base) * 4096 / config->speed < particle->age) {
                spawn(smoke, particle);
            }
        } else if (particle->kind == 14) {
            particle->visible = particle_step(smoke, particle, delta);
        }
    }
}

void sh_smoke_update(ShSmoke *smoke, float dt)
{
    /* Keep retail 30 Hz RNG calls independent of the display's frame rate. */
    const float frame = 1.0f / 30.0f;
    smoke->accumulator += dt;
    while (smoke->accumulator >= frame) {
        sh_smoke_step(smoke, SH_Q12(frame));
        smoke->accumulator -= frame;
    }
}

void sh_smoke_stop(ShSmoke *smoke)
{
    for (int i = 0; i < SH_SMOKE_CAPACITY; i++) {
        ShSmokeParticle *particle = &smoke->particles[i];
        if (particle->kind == 13 || particle->kind == 14) { particle->kind = 0; }
        particle->visible = false;
    }
    smoke->active = false;
}
