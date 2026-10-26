#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sh3ds/path.h"
struct ShRayCharacter;

typedef struct {
    uint16_t field_0;
    int16_t field_2;
    uint16_t field_4;
    int8_t field_6, unk_7;
    uint8_t field_8, chara_id, field_a, field_b;
    uint16_t field_c;
    uint8_t field_e, field_f, field_10, field_11, field_12;
} ShAttackInfo;

typedef struct {
    bool is_player;
    unsigned npc_index;
    int32_t chara_id, health, flags, last_attacker, attack_received;
    int32_t attack, hit_mask, attack_frame, attack_state;
    uint8_t shot_budget, shot_count;
    int16_t angle_c, angle_e, angle_10, angle_12;
    ShPathPoint position, attack_origin;
    ShPathPoint previous[3], current[3];
    int32_t damage_amount;
    ShPathPoint damage_position;
    const struct ShRayCharacter *shape;
    int collision_state;
} ShCombatActor;

typedef struct {
    int32_t charge, beam_color;
    void *user;
    void (*rumble)(void *, int);
    void (*motor_hit)(void *, int);
    void (*sound)(void *, int, const ShPathPoint *, int, int);
    void (*impact)(void *, const ShPathPoint *, int);
    void (*particle)(void *, ShCombatActor *, const ShPathPoint *, int, int);
} ShCombatDamageContext;

const ShAttackInfo *sh_combat_attack_info(unsigned attack);
bool sh_combat_damage_apply(ShCombatActor *attacker, ShCombatActor *target,
                           const ShPathPoint *position, int32_t scale,
                           ShCombatDamageContext *ctx, int32_t *hit_mask);
