#include "sh3ds/school_boss_world.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"
#include <stddef.h>
#include <string.h>

#define BOSS_WORLD_MULT(a, b) ((int32_t)((uint32_t)(a) * (uint32_t)(b)) >> 12)
#define BOSS_WORLD_FRACT(v) ((v) & 4095)

static int32_t boss_world_random(int32_t low, int32_t high)
{
    return (uint32_t)sh_rng_rand16() % (uint32_t)(high - low + 1) + low;
}

static void boss_world_object_init(ShSchoolBossObject *object, const char *model,
    int32_t x, int32_t y, int32_t z, int32_t rx, int32_t ry, int32_t rz)
{
    object->model = model;
    object->position = (ShPathPoint){x, y, z};
    object->rotation.vx = rx;
    object->rotation.vy = ry;
    object->rotation.vz = rz;
}

#include "school_boss_world_steps.h"

static bool boss_world_valid(const ShSchoolBossWorldContext *ctx)
{
    return ctx && ctx->save && ctx->light && ctx->particle_init && ctx->object && ctx->sound && ctx->falloff;
}

bool sh_school_boss_world_begin(ShSchoolBossWorld *world, ShSchoolBossWorldContext *ctx)
{
    if (!world || !boss_world_valid(ctx)) { return false; }
    memset(world, 0, sizeof(*world));
    boss_world_begin(world, ctx);
    return true;
}

bool sh_school_boss_world_step(ShSchoolBossWorld *world, ShSchoolBossWorldContext *ctx)
{
    if (!world || !boss_world_valid(ctx)) { return false; }
    boss_world_update(world, ctx);
    return true;
}
