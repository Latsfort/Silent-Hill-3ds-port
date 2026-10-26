#include "sh3ds/school_boss_particles.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"
#include <stddef.h>

typedef char BossParticleSlotSize[(sizeof(ShSchoolBossParticleSlot) == 20) ? 1 : -1];
typedef struct {
    int16_t field_C4, field_C6, field_C8, field_CA, field_CC, field_CE;
    int32_t field_D0, field_D4;
} ShBossParticleFrame;

#define BOSS_PARTICLE_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BOSS_PARTICLE_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BOSS_PARTICLE_MULT(a, b) ((int32_t)((uint32_t)(a) * (uint32_t)(b)) >> 12)
#define BOSS_PARTICLE_FROM(v, n) ((v) >> (n))

static int32_t boss_particle_random(int32_t low, int32_t high)
{
    return (int32_t)((uint32_t)sh_rng_rand16() % (uint32_t)(high - low + 1)) + low;
}

#include "school_boss_particles_steps.h"

void sh_school_boss_particles_init(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config)
{
    boss_particle_init(pool, config);
}

static bool boss_particles_step(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config,
    int32_t delta, void *user, void (*emit)(void *, unsigned, const ShPathPoint *), const ShBossParticleDrawContext *draw)
{
    if (!pool || !config || !config->field_16 || !config->field_1C || !config->field_4 || (!emit && !draw)) { return false; }
    ShBossParticleFrame frame;
    boss_particle_setup(pool, config, delta, &frame);
    for (unsigned i = 0; i < 500; ++i) {
        unsigned state = pool->slots[i].field_A;
        ShBossParticleQuad quad;
        if (draw && state >= 8 && state <= 11) { sh_school_boss_particle_style(&pool->slots[i], &quad); }
        ShPathPoint position;
        bool active = false;
        if (state == 8 || state == 10) { active = boss_particle_rise(pool, config, i, delta, &frame, &position); }
        else if (state == 9 || state == 11) { active = boss_particle_fall(pool, config, i, delta, &frame, &position); }
        if (active && emit) { emit(user, i, &position); }
        if (active && draw) {
            int16_t x, y;
            int32_t depth;
            if (draw->project(draw->user, &position, &x, &y, &depth) &&
                sh_school_boss_particle_quad(&pool->slots[i], &quad, x, y, depth, draw->projection_scale, config->field_6)) {
                draw->draw(draw->user, i, &quad);
            }
        }
    }
    return true;
}

bool sh_school_boss_particles_step(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config,
    int32_t delta, void *user, void (*emit)(void *, unsigned, const ShPathPoint *))
{
    return boss_particles_step(pool, config, delta, user, emit, NULL);
}

bool sh_school_boss_particles_draw_step(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config,
    int32_t delta, const ShBossParticleDrawContext *ctx)
{
    if (!ctx || !ctx->project || !ctx->draw || ctx->projection_scale <= 0) { return false; }
    return boss_particles_step(pool, config, delta, NULL, NULL, ctx);
}
