#pragma once
#include "sh3ds/school_boss_particle_config.h"
#include "sh3ds/savegame.h"
#include "sh3ds/path.h"

typedef struct {
    const char *model;
    ShPathPoint position;
    struct { int16_t vx, vy, vz; } rotation;
} ShSchoolBossObject;

typedef struct {
    ShSchoolBossParticleConfig particles;
    int16_t light_time[2];
    uint8_t light_rate[2], light_amplitude;
    ShSchoolBossObject objects[2];
    ShPathPoint light_position, light_rotation;
    void *light_bone, *flare_bone;
} ShSchoolBossWorld;

typedef struct {
    ShSavegame *save;
    int32_t delta;
    int32_t *light;
    void *user;
    void (*particle_init)(void *, ShSchoolBossParticleConfig *);
    void (*object)(void *, const ShSchoolBossObject *);
    void (*sound)(void *, int);
    void (*falloff)(void *, int, const ShPathPoint *, int, int32_t, int);
} ShSchoolBossWorldContext;

bool sh_school_boss_world_begin(ShSchoolBossWorld *world, ShSchoolBossWorldContext *ctx);
bool sh_school_boss_world_step(ShSchoolBossWorld *world, ShSchoolBossWorldContext *ctx);
