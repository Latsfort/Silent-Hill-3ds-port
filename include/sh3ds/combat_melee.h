#pragma once
#include "sh3ds/combat_damage.h"
#include "sh3ds/combat_ray.h"
#include "sh3ds/coll.h"

typedef struct {
    ShCombatActor *player;
    ShCombatActor *npcs[6];
    const ShCollWorld *world;
    int16_t collision_flags;
    bool game_over, failed;
    ShCombatDamageContext *damage;
} ShMeleeContext;

bool sh_combat_melee_apply(ShCombatActor *actor, int32_t angle, const ShAttackInfo *info,
                           int32_t scale, ShMeleeContext *ctx, int32_t *hit_mask);
