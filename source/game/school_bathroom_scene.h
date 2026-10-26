/* Generated from map1_s02 func_800DBB7C. */
#define SH_BATHROOM_CALLBACK 15
static void bathroom_camera_frame(ShTown *town, const ShBathroomFrame *frame, float dt);
static void bathroom_scene(ShTown *town, ShSavegame *save, float dt)
{
switch (town->step)
    {
        case 0:
            (town->controls_locked = true);
            sh_save_event_set(save, 121);
            sh_snd_call(4152);
            step_set(town, town->step + 1);
        /* fall through */

        case 1:
            step_set(town, town->step + 1);
            break;

        case 2:
            (town->school.player_hidden = true);

            (town->school.drain_flare_suppressed = true);

            (town->school.bathroom.tween[0] = 0);
            (town->school.bathroom.tween[1] = 0);

            
            ladder_camera(town, false, 86016, -2048, 245760);
            ladder_camera(town, true, 69632, -2048, 245760);

            (town->border.state = SH_BORDER_FADEINSTART);
            step_set(town, town->step + 1);
            break;

        case 3:
            {
                ShBathroomFrame frame;
                sh_bathroom_motion_step(&town->school.bathroom, SH_Q12(dt), &frame);
                if (frame.camera) { bathroom_camera_frame(town, &frame, dt); }
                if (frame.finished) { step_set(town, town->step + 1); }
            }
            break;

        default:
            
            ladder_unfreeze(town);
            finish(town);

            (town->school.player_hidden = false);
            (town->school.drain_flare_suppressed = false);

            (town->border.state = SH_BORDER_FADEOUTSTART);
            (town->cam_active = town->script_use_cam = false);

            sh_save_event_set(save, 96);
            break;
    }

}
