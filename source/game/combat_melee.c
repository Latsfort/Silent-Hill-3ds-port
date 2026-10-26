#include "sh3ds/combat_melee.h"
#include "sh3ds/fixed.h"
#include <stddef.h>

#define MELEE_Q12(v) ((int32_t)((uint32_t)(v) * 4096u))

static int32_t melee_length(int32_t x, int32_t z)
{
    return (int32_t)sh_fixed_sqrt12((uint32_t)sh_q12_mul(x, x) + (uint32_t)sh_q12_mul(z, z));
}

static int32_t melee_distance(const ShPathPoint *from, const ShPathPoint *to)
{
    return melee_length(to->vx - from->vx, to->vz - from->vz);
}

static bool melee_world_trace(ShRayTrace *trace, const ShPathPoint *from, const ShPathPoint *to, ShMeleeContext *ctx)
{
    ShPathPoint delta = {to->vx - from->vx, to->vy - from->vy, to->vz - from->vz};
    return sh_combat_ray_query(trace, ctx->world, from, &delta, NULL, 0, false, ctx->collision_flags);
}

static int32_t melee_damage(ShCombatActor *actor, ShCombatActor *target, const ShPathPoint *position,
                            int32_t scale, ShMeleeContext *ctx)
{
    int32_t mask = 0;
    if (!sh_combat_damage_apply(actor, target, position, scale, ctx->damage, &mask)) {
        ctx->failed = true;
    }
    return mask;
}

#include "combat_melee_steps.h"

bool sh_combat_melee_apply(ShCombatActor *actor, int32_t angle, const ShAttackInfo *info,
                           int32_t scale, ShMeleeContext *ctx, int32_t *hit_mask)
{
    if (!actor || !actor->shape || !info || !ctx || !ctx->player || !ctx->world ||
        !ctx->damage || !hit_mask || (uint8_t)actor->attack >= 70) {
        return false;
    }
    for (int i = 0; i < 6; ++i) {
        if (ctx->npcs[i] && !ctx->npcs[i]->shape) { return false; }
    }
    if (!ctx->player->shape) { return false; }
    ctx->failed = false;
    *hit_mask = melee_apply(actor, angle, info, scale, ctx);
    return !ctx->failed;
}
