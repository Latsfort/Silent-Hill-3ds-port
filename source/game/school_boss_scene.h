/* Generated from MAP1_S02 func_800DC1E0 and verified original ROM. */
#define SH_BOSS_CALLBACK 18
static const float s_boss_sound_positions[][3] = {
    {60.500000000000f, 0.699951171875f, 61.000000000000f},
    {60.500000000000f, 0.699951171875f, 59.000000000000f},
    {58.399902343750f, 1.000000000000f, 60.000000000000f},
};
#define SH_BOSS_LOOP_SOUND 1453
#define SH_BOSS_LOOP_DISTANCE 18.0f
static void boss_scene(ShTown *town, ShSavegame *save, const ShInputState *input, float dt)
{
switch (town->step)
    {
        case 0:
            (town->controls_locked = true);
            town->school.boss_camera_started = 0;
            step_set(town, town->step + 1);
        /* fall through */

        case 1:
            boss_choice(town, input, dt);

            town->school.boss_doors.reverse = 0;
            town->school.boss_doors.speed = 0;
            town->school.boss_doors.progress[1] = 0;
            town->school.boss_doors.progress[0] = 0;
            break;

        case 2:
            town->school.boss_doors.reverse = 1;
            /* fall through */

        case 4:
            if (town->school.boss_camera_started == 0)
            {
                
                boss_camera_position(town, 249856, -4915, -245760, 10240, 3072, 6144, 2048);

                ladder_wait(town, dt, 8192);

                if (town->step != 2 && town->step != 4)
                {
                    step_set(town, 5);
                    town->school.boss_camera_started++;
                }
            }
            else
            {
                step_set(town, 5);
            }
            break;

        case 5:
            if (sh_save_event_get(save, 118))
            {
                sh_snd_at(1452, s_boss_sound_positions[0], 0.5f);
            }
            else
            {
                sh_snd_at(1452, s_boss_sound_positions[1], 0.5f);
            }

            step_set(town, town->step + 1);
        /* fall through */

        case 6:
            ladder_wait(town, dt, 4096);
            break;

        case 7:
            {
                ShBossDoorFrame frame;
                sh_boss_doors_step(&town->school.boss_doors, SH_Q12(dt), sh_save_event_get(save, 118), &frame);
                if (frame.start_sound) { sh_snd_call(SH_BOSS_LOOP_SOUND); }
                sh_snd_falloff(SH_BOSS_LOOP_SOUND, s_boss_sound_positions[2], frame.volume, SH_BOSS_LOOP_DISTANCE, 0);
                if (frame.stop_sound) { sh_snd_stop(SH_BOSS_LOOP_SOUND); }
                if (frame.finished) { step_set(town, 1); }
            }
            break;

        default:
            
            ladder_unfreeze(town);
            boss_finish(town);

            sh_save_event_clear(save, 119);
            sh_save_event_clear(save, 118);

            if (town->school.boss_camera_started != 0)
            {
                sh_vc_camera_release(&town->vc, false);
            }
            break;
    }

}
