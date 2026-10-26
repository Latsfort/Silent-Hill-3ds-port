#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sh3ds/player_controller_flags.h"

typedef struct {
    uint16_t moving_forward, moving_backward, turning_left, turning_right;
    uint16_t step_left_tap, step_right_tap, step_left_hold, step_right_hold;
    uint16_t running, aiming, hold_attack, attacking, shooting;
    int32_t has_move_input, has_action_input;
    int rock_drill_direction;
} ShPlayerController;

typedef struct {
    uint32_t held, clicked, second_held;
    int8_t stick_x, stick_y;
    uint32_t step_left, step_right, run, aim, action;
    bool reverse_run, hold_aim, gameplay, is_aiming;
    int weapon_attack, lower_state, blaster_test;
} ShPlayerControllerContext;

void sh_player_controller_step(ShPlayerController *state, const ShPlayerControllerContext *ctx);
