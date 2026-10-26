/* map2_s00.c func_800E9A74. */
static void woodman_update(ShTown *t, ShSavegame *save, const ShInputState *input, float dt)
{
    switch (t->step) {
    case 0:
        t->pickup_item_id = SH_RETAIL_InvItemId_KeyOfWoodman;
        t->pickup_count = PICKUP_COUNT_DEFAULT;
        t->pickup_flag = SH_RETAIL_EventFlag_M2S00_PickupKeyOfWoodman;
        t->pickup_msg = map_msg_text(29);
        t->pickup_scale = 0.0f;
        t->pickup_spin = 0.0f;
        step_set(t, 1);
        /* fall through */
    case 1:
        /* Player_AnimStateSet clears moveSpeed; the port's locked tank control has no inertia. */
        step_set(t, 2);
        break;
    case 2:
        t->harry_state = 59;
        t->harry_time = 0.0f;
        step_set(t, 3);
        /* fall through */
    case 3:
        step_set(t, 4);
        break;
    case 4:
        if (t->harry_time >= (SH_TOWN_STATE_59_LAST - SH_TOWN_STATE_59_FIRST) / SH_TOWN_STATE_59_FPS) {
            step_set(t, 5);
        }
        break;
    case 5:
        t->pickup_active = true;
        if (t->pickup_scale >= 1.0f) {
            t->prompt_open = true;
            set_msg(t, t->pickup_msg, NULL, 0.0f);
            t->pickup_spin = wrap_angle(t->pickup_spin + PI_F * dt);
            if (sh_input_down(input, SH_BTN_ACTION)) {
                step_set(t, 6);
            } else if (sh_input_down(input, SH_BTN_CANCEL)) {
                step_set(t, 7);
            }
        } else {
            t->pickup_scale = fminf(t->pickup_scale + 4.0f * dt, 1.0f);
        }
        sh_save_event_set(save, SH_RETAIL_EventFlag_M2S00_PickupKeyOfWoodman);
        break;
    case 6:
        sh_save_item_add(save, SH_RETAIL_InvItemId_KeyOfWoodman, PICKUP_COUNT_DEFAULT);
        if (sh_save_event_get(save, SH_RETAIL_EventFlag_MapMark_OldTown_DogYardKeyLine)) {
            sh_save_event_set(save, SH_RETAIL_EventFlag_MapMark_55);
        }
        step_set(t, 8);
        break;
    case 7:
        sh_save_event_clear(save, SH_RETAIL_EventFlag_M2S00_PickupKeyOfWoodman);
        step_set(t, 8);
        /* fall through */
    case 8:
        t->pickup_active = false;
        t->prompt_open = false;
        set_msg(t, NULL, NULL, 0.0f);
        /* Event_CharaAnimPlayToEnd uses sysStateSteps[1]. */
        switch (t->script_substep) {
        case 0:
            t->harry_state = 60;
            t->harry_time = 0.0f;
            t->script_substep = 1;
            break;
        case 1:
            if (t->harry_time >= (SH_TOWN_STATE_60_LAST - SH_TOWN_STATE_60_FIRST) / SH_TOWN_STATE_60_FPS) {
                t->script_substep = 2;
            }
            break;
        default:
            step_set(t, 9);
            break;
        }
        break;
    default:
        script_end(t, save);
        break;
    }
}

/* map2_s00.c func_800E9DD8: g_Gfx_LockTimFileIdxs uses Lion/Woodman/Scarecrow bits 4/2/1. */
static unsigned eclipse_lock_image(const ShSavegame *save)
{
    return (sh_save_event_get(save, SH_RETAIL_EventFlag_M2S00_LockOfLionOpen) ? 4u : 0u) |
           (sh_save_event_get(save, SH_RETAIL_EventFlag_M2S00_LockOfWoodmanOpen) ? 2u : 0u) |
           (sh_save_event_get(save, SH_RETAIL_EventFlag_M2S00_LockOfScarecrowOpen) ? 1u : 0u);
}

/* map2_s00.c func_800E9DD8. */
static void eclipse_locks_update(ShTown *t, ShSavegame *save, const ShInputState *input, float dt)
{
    switch (t->step) {
    case 0:
        t->bg_image = eclipse_lock_image(save);
        step_set(t, 1);
        /* fall through */
    case 1:
        fade_to(t, 1.0f, FADE_SPEED);
        if (fade_done(t)) {
            step_set(t, 2);
        }
        break;
    case 2:
        fade_to(t, 0.0f, FADE_SPEED);
        step_set(t, 3);
        /* fall through */
    case 3:
        if (fade_done(t)) {
            step_set(t, 4);
        }
        break;
    case 4:
        if (sh_save_event_get(save, SH_RETAIL_EventFlag_M2S00_PickupKeyOfLion) &&
            !sh_save_event_get(save, SH_RETAIL_EventFlag_M2S00_LockOfLionOpen)) {
            sh_save_event_set(save, SH_RETAIL_EventFlag_M2S00_LockOfLionOpen);
            t->read_msg_idx = 32;
            sh_save_item_remove_one(save, SH_RETAIL_InvItemId_KeyOfLion);
            step_set(t, 5);
        } else if (sh_save_event_get(save, SH_RETAIL_EventFlag_M2S00_PickupKeyOfWoodman) &&
                   !sh_save_event_get(save, SH_RETAIL_EventFlag_M2S00_LockOfWoodmanOpen)) {
            sh_save_event_set(save, SH_RETAIL_EventFlag_M2S00_LockOfWoodmanOpen);
            t->read_msg_idx = 33;
            sh_save_item_remove_one(save, SH_RETAIL_InvItemId_KeyOfWoodman);
            step_set(t, 5);
        } else if (sh_save_event_get(save, SH_RETAIL_EventFlag_M2S00_PickupKeyOfScarecrow) &&
                   !sh_save_event_get(save, SH_RETAIL_EventFlag_M2S00_LockOfScarecrowOpen)) {
            sh_save_event_set(save, SH_RETAIL_EventFlag_M2S00_LockOfScarecrowOpen);
            t->read_msg_idx = 34;
            sh_save_item_remove_one(save, SH_RETAIL_InvItemId_KeyOfScarecrow);
            step_set(t, 5);
        } else {
            const unsigned mask = eclipse_lock_image(save);
            const unsigned locks = 3u - ((mask >> 2) & 1u) - ((mask >> 1) & 1u) - (mask & 1u);
            switch (locks) {
            case 0:
                t->read_msg_idx = 13;
                sh_save_event_set(save, SH_RETAIL_EventFlag_M2S00_DoorOfEclypseOpen);
                sh_save_event_set(save, SH_RETAIL_EventFlag_MapMark_OldTown_DoghouseArrowsOnly);
                break;
            case 1:
                t->read_msg_idx = 58;
                break;
            case 2:
                t->read_msg_idx = 57;
                break;
            default:
                t->read_msg_idx = 56;
                break;
            }
            set_msg(t, map_msg_text(t->read_msg_idx), NULL, 0.0f);
            step_set(t, 11);
        }
        break;
    case 5:
        set_msg(t, map_msg_text(t->read_msg_idx), NULL, 0.0f);
        sh_snd_play(1390, 0, 128); /* Sd_SfxPlay(Sfx_Unk1390, Q8(0), Q8(0.5)). */
        step_set(t, 6);
        break;
    case 6:
        t->bg_next_image = eclipse_lock_image(save);
        step_set(t, 7);
        /* fall through */
    case 7:
        if (sh_input_down(input, SH_BTN_ACTION)) {
            set_msg(t, NULL, NULL, 0.0f);
            step_set(t, 8);
        }
        break;
    case 8:
        t->bg_transition = 0.0f;
        step_set(t, 9);
        break;
    case 9:
        t->bg_transition += (1.0f / 32.0f) * PS1_FPS * dt;
        if (t->bg_transition > 1.0f) {
            t->bg_image = t->bg_next_image;
            step_set(t, 10);
        }
        break;
    case 10:
        if (wait_timer(t, 1.0f, dt) || sh_input_down(input, SH_BTN_ACTION) || sh_input_down(input, SH_BTN_CANCEL)) {
            step_set(t, 4);
        }
        break;
    case 11:
        if (sh_input_down(input, SH_BTN_ACTION)) {
            set_msg(t, NULL, NULL, 0.0f);
            step_set(t, 12);
        }
        break;
    case 12:
        fade_to(t, 1.0f, FADE_SPEED);
        if (fade_done(t)) {
            step_set(t, 13);
        }
        break;
    default:
        script_end(t, save);
        break;
    }
}

/* map2_s00.c MapEvent_DoorOfEclipseEnter, cases 0-5 and default. */
static void eclipse_enter_update(ShTown *t, ShSavegame *save, float *px, float *pz, float *yaw,
                                  const ShInputState *input, float dt)
{
    const int32_t delta = SH_Q12(dt);
    switch (t->step) {
    case 0:
        sh_env_set(&t->environment, 1, 1);
        sh_env_transition_begin(&t->environment, 6, 6);
        if (px) {
            *px = SH_Q12_TO_FLOAT(SH_Q12(147.7f));
        }
        if (pz) {
            *pz = -SH_Q12_TO_FLOAT(SH_Q12(376.5f));
        }
        if (yaw) {
            *yaw = PI_F * 0.5f;
        }
        t->need_ground_snap = true;
        t->letterbox = true;
        fade_to(t, 0.0f, FADE_SPEED);
        step_set(t, 1);
        /* fall through */
    case 1: {
        t->script_ticks += delta;
        const int32_t amount = sh_q12_mul(SH_Q12_ONE - sh_fixed_cos(t->script_ticks / 12), SH_Q12(60.0f)) + SH_Q12(40.0f);
        sh_env_transition_update(&t->environment, amount, 0, SH_Q12(100.0f));
        if (t->script_ticks > SH_Q12(1.5f) && t->script_ticks < SH_Q12(4.5f)) {
            set_msg(t, map_msg_text(35), NULL, 0.0f);
            if (sh_input_down(input, SH_BTN_ACTION)) {
                set_msg(t, NULL, NULL, 0.0f);
                step_set(t, 2);
            }
        } else {
            set_msg(t, NULL, NULL, 0.0f);
        }
        t->script_wait_ticks += delta;
        if (t->script_wait_ticks > SH_Q12(6.0f)) {
            step_set(t, t->step + 1);
        }
        break;
    }
    case 2:
        sh_env_transition_update(&t->environment, SH_Q12(100.0f), 0, SH_Q12(100.0f));
        sh_save_flashlight_set(save, false);
        t->script_wait_ticks += delta;
        if (t->script_wait_ticks > SH_Q12(0.2f)) {
            t->read_msg_idx = 36;
            step_set(t, 3);
        }
        break;
    case 3:
        /* MAP_MESSAGES[36] has no ~E; message 37 terminates the chain. */
        set_msg(t, map_msg_text(t->read_msg_idx), NULL, 0.0f);
        if (sh_input_down(input, SH_BTN_ACTION)) {
            if (t->read_msg_idx == 36) {
                t->read_msg_idx = 37;
                set_msg(t, map_msg_text(37), NULL, 0.0f);
            } else {
                set_msg(t, NULL, NULL, 0.0f);
                step_set(t, 4);
            }
        }
        break;
    case 4:
        sh_env_set(&t->environment, 6, 3);
        sh_save_flashlight_set(save, true);
        step_set(t, 5);
        /* fall through */
    case 5:
        t->script_wait_ticks += delta;
        if (t->script_wait_ticks > SH_Q12(1.2f)) {
            step_set(t, 6);
        }
        break;
    default:
        sh_save_event_set(save, SH_RETAIL_EventFlag_159);
        script_end(t, save);
        break;
    }
}
