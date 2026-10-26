#include "sh3ds/school_boss_scene.h"
#include <string.h>

#define BOSS_SCENE_FRACT(v) ((v) & 4095)
#define BOSS_SCENE_MAX(a, b) ((a) > (b) ? (a) : (b))

static uint8_t *boss_scene_particle(ShSchoolBossSceneContext *ctx, unsigned index)
{
    return ctx->particle_states + index * ctx->particle_stride;
}

static void boss_scene_next(ShSchoolBossScene *scene)
{
    scene->step++;
    scene->timer = 0;
    scene->fade_step = 0;
}

static void boss_scene_reset(ShSchoolBossScene *scene)
{
    scene->step = -1;
    scene->timer = 0;
    scene->fade_step = 0;
}

static void boss_scene_wait(ShSchoolBossScene *scene, ShSchoolBossSceneContext *ctx, int32_t delay, bool substep)
{
    (void)substep;
    scene->timer += ctx->raw_delta;
    if (delay < scene->timer) { boss_scene_next(scene); }
}

static void boss_scene_gameplay(ShSchoolBossScene *scene, ShSchoolBossSceneContext *ctx)
{
    ctx->gameplay(ctx->user);
    scene->finished = true;
}

static void boss_scene_fade(ShSchoolBossScene *scene, ShSchoolBossSceneContext *ctx,
                             int command, bool black, int mode, int32_t duration, bool unused)
{
    int active = command == 2 ? scene->fade_step : command;
    bool finished = ctx->fade(ctx->user, active, black, mode, duration, unused);
    if (active == 0 && command != 0) { scene->fade_step++; }
    if (active == 1 && finished) { boss_scene_next(scene); }
}

#include "school_boss_scene_steps.h"

void sh_school_boss_scene_begin(ShSchoolBossScene *scene)
{
    memset(scene, 0, sizeof(*scene));
}

static bool boss_scene_valid(const ShSchoolBossSceneContext *ctx)
{
    return ctx && ctx->save && ctx->particles && ctx->particle_states && ctx->particle_stride && ctx->particle_count == BOSS_SCENE_PARTICLES &&
        ctx->control && ctx->border_show && ctx->camera && ctx->camera_return && ctx->fade &&
        ctx->echo && ctx->gameplay && ctx->autosave && ctx->sound && ctx->sound_attributes;
}

bool sh_school_boss_intro(ShSchoolBossScene *scene, ShSchoolBossSceneContext *ctx)
{
    if (!scene || !boss_scene_valid(ctx)) { return false; }
    if (!scene->finished) { boss_intro(scene, ctx); }
    return true;
}

bool sh_school_boss_outro(ShSchoolBossScene *scene, ShSchoolBossSceneContext *ctx)
{
    if (!scene || !boss_scene_valid(ctx)) { return false; }
    if (!scene->finished) { boss_outro(scene, ctx); }
    return true;
}
