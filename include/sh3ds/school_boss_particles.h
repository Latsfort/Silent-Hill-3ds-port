#pragma once
#include "sh3ds/school_boss_particle_config.h"
#include "sh3ds/school_boss_particle_slot.h"
#include "sh3ds/path.h"
#include "sh3ds/school_boss_particle_quad.h"
#include <stdbool.h>

typedef struct {
    ShSchoolBossParticleSlot slots[500];
    uint32_t effect_flags;
    int next_slot;
} ShSchoolBossParticles;

void sh_school_boss_particles_init(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config);
void sh_school_boss_shed(ShSchoolBossParticles *pool, int32_t x, int32_t y, int32_t z, int32_t speed, int32_t heading);
void sh_school_boss_strand(ShSchoolBossParticles *pool, int32_t variant, int32_t shade,
    int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1);
bool sh_school_boss_particles_step(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config,
    int32_t delta, void *user, void (*emit)(void *, unsigned, const ShPathPoint *));

typedef struct {
    void *user;
    int32_t projection_scale;
    bool (*project)(void *, const ShPathPoint *, int16_t *, int16_t *, int32_t *);
    void (*draw)(void *, unsigned, const ShBossParticleQuad *);
} ShBossParticleDrawContext;

bool sh_school_boss_particles_draw_step(ShSchoolBossParticles *pool, ShSchoolBossParticleConfig *config,
    int32_t delta, const ShBossParticleDrawContext *ctx);
