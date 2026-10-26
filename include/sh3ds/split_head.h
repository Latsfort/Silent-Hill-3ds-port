#pragma once
#include "sh3ds/path.h"
#include "sh3ds/savegame.h"
#include "sh3ds/chara.h"

typedef struct ShSplitHead {
    ShPathPoint position;
    struct { int16_t vy; } rotation;
    ShPathModel model;
    int32_t health, move_speed, damage_amount, fall_speed;
    uint32_t flags;
    int16_t heading_angle;
    int collision_state;
    int32_t movement_offset[2];
    struct {
        uint16_t flags;
        uint8_t field_ea;
        int16_t angle_f0, timer_f2, timer_f4;
        int16_t angle_ec, field_ee;
        int32_t anim_time;
        uint8_t sound_latches[4];
        int32_t sound_timer;
    } props;
    int16_t wait_limit, idle_timer;
    int32_t movement_duration;
    int mesh_variant;
    struct {
        int32_t top, height, bottom, offset_y, radius, cylinder_radius;
        ShPathPoint offset, cylinder_offset;
    } hit_shape;
    ShPathPoint contact_points[9];
    uint8_t edge_type[9], edge_side[9], first_edge, attack_overlap;
    int32_t player_offset[2];
} ShSplitHead;

typedef struct ShSplitRay { bool has_hit, character_hit; } ShSplitRay;
typedef struct ShSplitMoveResult {
    ShPathPoint offset;
    int32_t ground_height;
} ShSplitMoveResult;
typedef struct ShSplitSound {
    int id, kind, volume, distance, pitch, flags;
    ShPathPoint position;
} ShSplitSound;

typedef struct ShSplitContext {
    ShSavegame *save;
    ShPathPoint player;
    int32_t delta;
    int difficulty, weapon_attack;
    bool player_dead, player_damage_disabled;
    const ShCharaBone *world_bones;
    unsigned bone_count;
    int32_t player_radius;
    int player_attack;
    ShPathPoint player_push, player_push_velocity;
    ShSplitRay (*ray)(void *user, const ShPathPoint *from, const ShPathPoint *direction);
    void (*player_effect)(void *user, const ShPathPoint *position, int type, int count);
    void (*move)(void *user, ShSplitHead *boss, int32_t offset_x, int32_t offset_z);
    bool (*collide)(void *user, const ShSplitHead *boss, const ShPathPoint *offset, ShSplitMoveResult *result);
    void *user;
    bool (*pose)(void *, const ShSplitHead *, const ShAnimPoseTiming *, const ShCharaBone **, unsigned *);
    void (*shed)(void *, int32_t, int32_t, int32_t, int32_t, int32_t);
    void (*strand)(void *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
    ShSplitSound sounds[16];
    unsigned sound_count;
} ShSplitContext;

void sh_split_head_damage(ShSplitHead *boss, ShSplitContext *context);
void sh_split_head_begin(ShSplitHead *boss);
bool sh_split_head_timing(ShSplitHead *boss, ShSplitContext *context, ShAnimPoseTiming *pose);
/* Runs original controls1..8; caller supplies real geometry and player-effect adapters. */
bool sh_split_head_control(ShSplitHead *boss, ShSplitContext *context);
void sh_split_head_control_reset(ShSplitHead *boss);
void sh_split_head_offset_prepare(ShSplitHead *boss, ShSplitContext *context);
bool sh_split_head_contact(ShSplitHead *boss, ShSplitContext *context);
bool sh_split_head_movement(ShSplitHead *boss, ShSplitContext *context);
void sh_split_head_sounds(ShSplitHead *boss, ShSplitContext *context);
bool sh_split_head_effects(ShSplitHead *boss, ShSplitContext *context);
bool sh_split_head_update(ShSplitHead *boss, ShSplitContext *context);
bool sh_split_head_position_step(ShSplitHead *boss, ShSplitContext *context, int16_t offset_x, int16_t offset_z);
void sh_split_head_world_bone(const ShSplitHead *boss, const ShCharaBone *model, ShCharaBone *world);
