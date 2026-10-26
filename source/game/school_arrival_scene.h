/* Generated from func_800DA384 and SHA1-verified MAP1_S02 voice table. */
#define SH_ARRIVAL_CALLBACK 5
static const uint16_t s_arrival_voices[] = {4179, 4175, 4157};
static void arrival_path_begin(ShTown *town)
{
    memset(&town->school.phone.motion, 0, sizeof(town->school.phone.motion));
    town->school.phone.motion.heading = -512;
    town->school.phone.motion.waypoint = (ShPathPoint){-74465, town->school.card_player.position.vy, -254935};
}
static void arrival_camera(ShTown *town, int index, float dt)
{
    switch (index) {
    case 0: {
        const float position[3] = {-19.959960937500f, -1.399902343750f, -61.449951171875f};
        const float look[3] = {-17.139892578125f, -1.489990234375f, -64.269775390625f};
        const ShVcScriptParams params = {{0.000000000000f, 0.000000000000f, 0.000000000000f, 0.000000000000f}, {0.000000000000f, 0.000000000000f, 0.000000000000f, 0.000000000000f}};
        sh_vc_script_update(&town->vc, position, look, &params, true, true, dt, &town->cam);
        town->cam_active = town->script_use_cam = true; break;
    }
    case 1: {
        const float position[3] = {-21.199951171875f, -2.279785156250f, -59.229980468750f};
        const float look[3] = {-13.099853515625f, -1.199951171875f, -68.299804687500f};
        const ShVcScriptParams params = {{0.000000000000f, 0.000000000000f, 0.750000000000f, 0.250000000000f}, {0.000000000000f, 0.000000000000f, 0.027343750000f, 0.087402343750f}};
        sh_vc_script_update(&town->vc, position, look, &params, false, false, dt, &town->cam);
        town->cam_active = town->script_use_cam = true; break;
    }
    case 2: {
        const float position[3] = {-17.789794921875f, -5.029785156250f, -54.089843750000f};
        const float look[3] = {-19.569824218750f, 0.000000000000f, -61.209960937500f};
        const ShVcScriptParams params = {{0.000000000000f, 0.000000000000f, 0.750000000000f, 0.250000000000f}, {0.000000000000f, 0.000000000000f, 0.027343750000f, 0.087402343750f}};
        sh_vc_script_update(&town->vc, position, look, &params, false, false, dt, &town->cam);
        town->cam_active = town->script_use_cam = true; break;
    }
    }
}
static void arrival_message(ShTown *town, const ShInputState *input, int index, float dt)
{
    school_timed_message(town, input, index, dt, true, s_arrival_voices, sizeof(s_arrival_voices) / sizeof(s_arrival_voices[0]));
}
static void arrival_scene(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)
{

    ShPathPoint viewPos;
    bool    isSkipped;

    isSkipped = false;

    if (sh_input_down(input, KEY_START) &&
        town->step > 0 && town->step < 10)
    {
        isSkipped = true;
        step_set(town, -1);
    }

    switch (town->step)
    {
        case 0:

            arrival_camera(town, 0, dt);
            break;

        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            break;

        case 7:
        case 8:
        case 9:
        case 10:
            if (town->school.arrival_camera_stage == 0)
            {

                arrival_camera(town, 1, dt);

                arrival_view_position(town, &viewPos);

                if (abs(viewPos.vx - -86835) < 1228 && abs(viewPos.vz - -242606) < 1228)
                {
                    town->school.arrival_camera_stage = 1;
                }
            }
            else
            {

                arrival_camera(town, 2, dt);
            }
            break;

        default:
            sh_save_event_set(save, 1202);
            break;
    }

    switch (town->step)
    {
        case 0:
            phone_freeze(town);
            sh_save_flashlight_set(save, true);

            (town->school.particle_mode = 6);
            (town->school.particle_reset = true);

            town->school.card_player.position.vx = -54476;
            town->school.card_player.position.vz = -278937;

            arrival_path_begin(town);
            card_warp(town);
            sh_save_event_clear(save, 92);

            town->school.arrival_audio_index = 0;

            step_set(town, town->step + 1);
        /* fall through */

        case 1:
            locker_border_in(town);
            break;

        case 2:
            ladder_wait(town, dt, 6144);
            break;

        case 3:
            phone_wait_stop(town, dt);
            break;

        case 4:
            arrival_message(town, input, 15, dt);
            break;

        case 5:
            arrival_animation(town);
            break;

        case 6:
            arrival_message(town, input, 16, dt);
            break;

        case 7:
            sh_save_event_set(save, 92);
            phone_path(town, dt);
            break;

        case 8:
            ladder_wait(town, dt, 9011);
            break;

        case 9:
            arrival_message(town, input, 17, dt);
            break;

        case 10:
            ladder_wait(town, dt, 4096);
            break;

        default:
            if (isSkipped)
            {

                town->school.card_player.position.vx = -74465;
                town->school.card_player.position.vy = 0;
                town->school.card_player.position.vz = -254935;
                town->school.card_player.rotation.vy = -512;


                arrival_unfreeze(town, true);
                boss_finish(town);


                arrival_camera_release(town, true);
            }
            else
            {

                arrival_unfreeze(town, false);
                boss_finish(town);


                arrival_camera_release(town, false);
            }

            (town->border.state = SH_BORDER_FADEOUTSTART);


            sh_save_event_set(save, 225);
            arrival_autosave(town, save);
            sh_save_event_clear(save, 225);
            break;
    }

}
