/* Generated complete Player_Controller; original bit histories and branches. */
void sh_player_controller_step(ShPlayerController *state, const ShPlayerControllerContext *ctx)
{

    int32_t heldAttackActionFlag;

    state->moving_forward    = (state->moving_forward    * 2) & 0x3;
    state->step_left_tap  = (state->step_left_tap  * 2) & 0x3F;
    state->step_right_tap = (state->step_right_tap * 2) & 0x3F;

    if (ctx->stick_y < -64 || ctx->stick_y >= 64 ||
        ctx->stick_x < -64 || ctx->stick_x >= 64)
    {
        state->turning_left    = ctx->stick_x < -64 ? abs(ctx->stick_x + 64) : 0;
        state->turning_right   = ctx->stick_x >= 64 ? (ctx->stick_x - (64 - 1)) : 0;
        state->moving_forward |= ctx->stick_y < -64;
        state->moving_backward = ctx->stick_y >= 64;
        state->has_move_input     = ctx->clicked & (ctx->step_left |
                                                                          (65536    |
                                                                           131072 |
                                                                           262144  |
                                                                           524288) |
                                                                          ctx->step_right |
                                                                          ctx->aim);
    }
    else
    {
        state->turning_left    = ((ctx->held & (33554432 | 134217728)) == 134217728) << 6;
        state->turning_right   = ((ctx->held & (33554432 | 134217728)) == 33554432) << 6;
        state->moving_forward |= (ctx->held & (16777216 | 67108864)) == 16777216;
        state->moving_backward = (ctx->held & (16777216 | 67108864)) == 67108864;
        state->has_move_input     = ctx->clicked & (ctx->step_left |
                                                                          (16777216    |
                                                                           33554432 |
                                                                           67108864  |
                                                                           134217728) |
                                                                          ctx->step_right |
                                                                          ctx->aim);
    }

    state->step_left_hold  = (ctx->held & ctx->step_left) &&
                                  !(ctx->held & ctx->step_right);

    state->step_right_hold = (ctx->held & ctx->step_right) &&
                                  !(ctx->held & ctx->step_left);

    state->step_left_tap  |= (ctx->clicked & ctx->step_left)  != false;
    state->step_right_tap |= (ctx->clicked & ctx->step_right) != false;

    if (ctx->reverse_run)
    {
        state->running = !(ctx->held & ctx->run);
    }
    else
    {
        state->running = ctx->held & ctx->run;
    }

    if (ctx->hold_aim)
    {
        state->aiming = ctx->held & ctx->aim;
    }
    else
    {
        state->aiming = ctx->clicked & ctx->aim;
    }

    if (ctx->weapon_attack >= 32 &&
        ctx->lower_state >= 20)
    {
        state->shooting  = ctx->held & ctx->action;
        state->attacking = state->shooting;
    }
    else
    {
        heldAttackActionFlag = ctx->held & ctx->action;

        state->hold_attack = (state->hold_attack * 2) & 0x1F;
        state->attacking  = (state->attacking * 2) & 0x3;
        state->shooting   = (state->shooting * 2) & 0x3;

        state->hold_attack |= (heldAttackActionFlag & 0xFFFF) != false;
        state->attacking  |= (state->hold_attack & 0xF) == 0xF;

        state->shooting |= state->hold_attack != false && !(state->hold_attack & 0x11);

        if (state->shooting)
        {
            state->hold_attack = false;
        }
    }

    state->has_action_input = ctx->clicked & (ctx->run |
                                                                    ctx->action);

    if (!ctx->gameplay)
    {
        state->shooting   = false;
        state->attacking  = false;
        state->hold_attack = false;
    }

    if (ctx->weapon_attack == 35)
    {
        switch (ctx->blaster_test)
        {
            case 0: 
                state->aiming = false;
                break;

            case 1: 
                state->aiming    = ctx->second_held & 16384;
                state->shooting  = ctx->second_held & 32768;
                state->attacking = state->shooting;
                break;

            case 2: 
                break;
        }
    }

    
    if (ctx->is_aiming && (ctx->weapon_attack == 33 ||
                                            (ctx->weapon_attack < 32 &&
                                             (CONTROLLER_WEAPON_ID(ctx->weapon_attack) == 4    ||
                                              CONTROLLER_WEAPON_ID(ctx->weapon_attack) == 2 ||
                                              CONTROLLER_WEAPON_ID(ctx->weapon_attack) == 6))))
    {
        if (state->moving_forward)
        {
            state->rock_drill_direction = 1;
        }
        else if (state->moving_backward)
        {
            state->rock_drill_direction = -1;
        }
        else
        {
            state->rock_drill_direction = 0;
        }

        state->running           = false;
        state->step_right_tap  = false;
        state->step_right_hold = false;
        state->step_left_tap   = false;
        state->step_left_hold  = false;
        state->moving_backward    = false;
        state->moving_forward     = false;
    }

}
