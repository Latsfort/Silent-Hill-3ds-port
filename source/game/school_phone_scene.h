/* Generated from MAP1_S02 MapEvent_CutscenePhoneCall; all cases retained. */
#define SH_PHONE_CALLBACK 14
static void phone_scene(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)
{

    if (sh_input_down(input, KEY_START) &&
        town->step >= 2 && town->step < 26)
    {
        if (town->step < 5)
        {
            town->school.phone.early_skip = true;
        }

        step_set(town, 27);
    }

    switch (town->step)
    {
        case 0:
            phone_freeze(town);

            sh_save_event_set(save, 122);
            sh_snd_call(4149);
            sh_phone_motion_begin(&town->school.phone.motion, &town->school.card_player);

            town->school.phone.early_skip = false;
            step_set(town, town->step + 1);
        /* fall through */

        case 1:
            phone_wait_stop(town, dt);
            break;

        case 2:
            ladder_wait(town, dt, 6144);
            break;

        case 3:
            locker_border_in(town);
            break;

        case 4:
            phone_path(town, dt);
            ladder_wait(town, dt, 8192);
            break;

        case 5:
            (town->school.phone.hidden = true);
            (town->school.empty_hand = true);
            sh_save_flashlight_set(save, true);


            phone_camera(town, 5, dt);

            step_set(town, town->step + 1);
        /* fall through */

        case 6:
            phone_path(town, dt);
            break;

        case 7:
            locker_animation_start(town, 97);
            step_set(town, town->step + 1);
        /* fall through */

        case 8:
            ladder_wait(town, dt, 2048);
            break;

        case 9:

            phone_camera(town, 9, dt);

            step_set(town, town->step + 1);
        /* fall through */

        case 10:
            ladder_wait(town, dt, 14336);
            break;

        case 11:
            sh_snd_call(1456);
            sh_snd_call(19);

            (town->school.phone.held = true);
            step_set(town, town->step + 1);
        /* fall through */

        case 12:
            if (phone_held_ready(town))
            {
                (town->school.phone.hidden = false);
                sh_xa_preload(4150);
                step_set(town, town->step + 1);
            }
            break;

        case 13:
            phone_animation_wait(town);
            sh_xa_preload(4150);
            break;

        case 14:
            sh_snd_call(4150);
            step_set(town, town->step + 1);
        /* fall through */

        case 15:
            phone_message(town, input, 31, dt);


            phone_camera(town, 15, dt);
            break;

        case 16:

            phone_camera(town, 16, dt);

            step_set(town, town->step + 1);
        /* fall through */

        case 17:
            ladder_wait(town, dt, 7372);
            break;

        case 18:
            locker_animation_start(town, 98);
            step_set(town, town->step + 1);
        /* fall through */

        case 19:
            ladder_wait(town, dt, 4096);
            break;

        case 20:
            (town->school.phone.locked = true);
            phone_message(town, input, 35, dt);
            break;

        case 21:
            (town->school.phone.locked = false);
            step_set(town, town->step + 1);
        /* fall through */

        case 22:
            ladder_wait(town, dt, 6144);
            break;

        case 23:
            ladder_fade(town, 2, true, 0);
            break;

        case 24:
            (town->school.empty_hand = false);
            sh_snd_call(19);

            (town->school.phone.held = false);
            (town->border.state = SH_BORDER_FADEOUTSTART);

            sh_snd_call(1457);
            step_set(town, town->step + 1);
        /* fall through */

        case 25:
            ladder_wait(town, dt, 4096);
            break;

        case 26:
            if (phone_held_ready(town))
            {
                step_set(town, -1);
            }
            break;

        case 27:
            ladder_fade(town, 2, true, 0);
            break;

        case 28:
            sh_snd_call(19);
            (town->school.phone.locked = false);

            if (!town->school.phone.early_skip)
            {
                (town->school.empty_hand = false);
            }

            (town->school.phone.held = false);
            (town->school.phone.hidden = false);


            phone_camera(town, 28, dt);


            town->school.card_player.position.vx = 83148;
            town->school.card_player.position.vz = 587776;
            town->school.card_player.rotation.vy = -1024;

            if (!phone_assets_wait(town)) { break; }
            card_warp(town);
            (town->border.state = SH_BORDER_RESET);
            step_set(town, town->step + 1);
            break;

        default:
            ladder_fade(town, 0, false, 0);
            phone_unfreeze(town);
            finish(town);

            sh_save_event_set(save, 98);
            sh_save_event_clear(save, 99);

            (town->cam_active = town->script_use_cam = false);
            break;
    }

}
