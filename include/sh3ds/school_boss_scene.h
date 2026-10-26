#pragma once
#include "sh3ds/savegame.h"
#include "sh3ds/path.h"
#include "sh3ds/school_boss_particle_config.h"

typedef struct {
    int step;
    int32_t timer, light;
    int exit_sound_phase;
    int fade_step;
    bool finished;
} ShSchoolBossScene;

typedef struct {
    ShSavegame *save;
    int32_t delta, raw_delta;
    bool skip;
    uint8_t *particle_states;
    unsigned particle_count, particle_stride;
    ShSchoolBossParticleConfig *particles;
    void *user;
    void (*control)(void *, bool);
    void (*border_show)(void *);
    void (*camera)(void *, bool, const ShPathPoint *);
    void (*camera_return)(void *, bool);
    bool (*fade)(void *, int, bool, int, int32_t, bool);
    void (*echo)(void *);
    void (*gameplay)(void *);
    void (*autosave)(void *);
    void (*sound)(void *, int);
    void (*sound_attributes)(void *, int, int, int, int);
} ShSchoolBossSceneContext;

void sh_school_boss_scene_begin(ShSchoolBossScene *scene);
bool sh_school_boss_intro(ShSchoolBossScene *scene, ShSchoolBossSceneContext *ctx);
bool sh_school_boss_outro(ShSchoolBossScene *scene, ShSchoolBossSceneContext *ctx);
