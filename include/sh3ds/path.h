#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "sh3ds/anim_timing.h"

typedef struct ShPathPoint { int32_t vx, vy, vz; } ShPathPoint;
typedef struct ShPathModel {
    int state_step, control_state, animation_status;
    int keyframe;
    int32_t time, alpha;
} ShPathModel;

typedef struct ShPathPlayer {
    ShPathPoint position;
    struct { int16_t vy; } rotation;
    int state;
    int cutscene_state;
    bool animation_reset;
    int32_t move_speed;
    ShPathModel model, extra_model;
    int upper_body_state, lower_body_state;
    int16_t heading_angle;
} ShPathPlayer;

typedef struct ShPath {
    int step, waypoint;
    int32_t previous_distance;
    int16_t target_angle, next_angle, flex_y;
} ShPath;

bool sh_path_player_step(ShPath *path, ShPathPlayer *player, int state,
                         ShPathPoint *points, int16_t heading, int count, int32_t dt);
void sh_path_state_set(ShPathPlayer *player, int state);
void sh_path_arrival_start(ShPathPlayer *player);
bool sh_path_school_animation_start(ShPathPlayer *player, int state);
bool sh_path_alley_animation_start(ShPathPlayer *player, int state);
bool sh_path_school_animation_done(const ShPathPlayer *player);
bool sh_path_animation_first(const ShPathPlayer *player, int *first);
void sh_path_locomotion(ShPath *path, ShPathPlayer *player, int keyframe, int32_t dt);
bool sh_path_animation_timing(ShPathPlayer *player, int32_t dt, int stick_y,
                               int stick_magnitude, ShAnimPoseTiming *pose);
