#pragma once
#include "sh3ds/combat_damage.h"
#include "sh3ds/savegame.h"

typedef struct {
    ShCombatActor actor;
    ShPathPoint position;
    int16_t yaw, heading_angle;
    uint32_t flags;
    int32_t move_speed, afk_timer, field_f4, field_10c, collision_radius;
    int state, model_step, model_control, extra_step, extra_control, upper_state, lower_state;
    int weapon_attack;
    bool aiming;
} ShPlayerDamage;

typedef struct {
    ShSavegame *save;
    int difficulty;
    bool disable_control, disable_damage, walk_to_run, dead;
    int previous_attack, target_npc, npc_indices[4];
    const int16_t *npc_yaws;
    unsigned npc_count;
    void *user;
    void (*sound)(void *, int, const ShPathPoint *, int, int);
    void (*motor)(void *, int, int);
    void (*collision_reset)(void *);
    void (*grab_feedback)(void *);
    void (*damage_feedback)(void *, int32_t);
} ShPlayerDamageContext;

bool sh_player_receive_damage(ShPlayerDamage *player, ShPlayerDamageContext *ctx);
