#pragma once
#include "sh3ds/combat_damage.h"
#include "sh3ds/combat_ray.h"

typedef struct {
    ShCombatActor *character;
    ShPathPoint target;
} ShAttackTrace;

typedef struct ShAttackContext {
    int32_t delta, animation_time, animation_start, player_accuracy;
    bool dark;
    int target_index;
    ShCombatActor **npcs;
    unsigned npc_count;
    ShCombatDamageContext *damage;
    void *user;
    bool (*trace)(void *, const ShPathPoint *, ShPathPoint *, ShCombatActor *, ShAttackTrace *);
    int (*melee)(void *, ShCombatActor *, int32_t, const ShAttackInfo *, int32_t);
    void (*feedback)(void *, int, bool, int32_t);
    bool failed;
} ShAttackContext;

bool sh_combat_attack_update(ShCombatActor *actor, ShAttackContext *ctx, int32_t *hit_mask);
