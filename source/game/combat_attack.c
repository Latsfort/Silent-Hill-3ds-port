#include "sh3ds/combat_attack.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"
#include <stddef.h>

typedef struct { int16_t vx, vy; } ShAttackAngles;
#define ATTACK_MULT(a, b) ((int32_t)((uint32_t)(a) * (uint32_t)(b)) >> 12)
#define ATTACK_Q12(v) ((int32_t)((uint32_t)(v) * 4096u))

static int32_t attack_random(void)
{
    int32_t value = sh_rng_rand16();
    return (((value * 2) ^ value) >> 3) & 4095;
}

static uint32_t attack_magnitude(int32_t x, int32_t z)
{
    return sh_fixed_sqrt0((uint32_t)x * (uint32_t)x + (uint32_t)z * (uint32_t)z);
}

static void attack_damage(ShCombatActor *actor, ShCombatActor *target, const ShPathPoint *position, ShAttackContext *ctx)
{
    int32_t mask;
    if (!sh_combat_damage_apply(actor, target, position, 0, ctx->damage, &mask)) {
        ctx->failed = true;
    }
}

#include "combat_attack_steps.h"

bool sh_combat_attack_update(ShCombatActor *actor, ShAttackContext *ctx, int32_t *hit_mask)
{
    if (!actor || !ctx || !hit_mask || !ctx->damage || !ctx->damage->rumble ||
        !ctx->trace || !ctx->melee || !ctx->feedback ||
        (unsigned)(uint8_t)actor->attack >= 70 ||
        (ctx->target_index >= 0 && (!ctx->npcs || (unsigned)ctx->target_index >= ctx->npc_count))) {
        return false;
    }
    ctx->failed = false;
    *hit_mask = attack_update(actor, ctx);
    return !ctx->failed;
}
