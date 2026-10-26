/* Generated from MAP1_S02 func_800DCF00. */
#define SH_BOSS_DESCENT_CALLBACK 19
static const float s_descent_sound_position[3] = {54.469970703125f, -0.000000000000f, 60.000000000000f};
static void descent_sound(ShTown *town, int sound, int volume, float distance)
{
    float position[3]; memcpy(position, s_descent_sound_position, sizeof(position));
    position[1] = -SH_Q12_TO_FLOAT(town->school.boss_box_y);
    sh_snd_falloff(sound, position, volume, distance, 0);
}
static void boss_descent(ShTown *town, ShSavegame *save, float dt)
{
switch (town->step)
    {
        case 0:
            (town->controls_locked = true);
            (town->border.state = SH_BORDER_FADEINSTART);

            town->school.descent_speed = 0;

            step_set(town, town->step + 1);
        /* fall through */

        case 1:
            step_set(town, town->step + 1);
            break;

        case 2:
            ladder_wait(town, dt, 1228);
            break;

        case 3:
            locker_animation_start(town, 106);
            step_set(town, town->step + 1);
        /* fall through */

        case 4:
            descent_fade_start(town, 122);
            step_set(town, town->step + 1);
        /* fall through */

        case 5:
            descent_fade_wait(town);

            town->school.descent_fade_speed += sh_q12_mul(SH_Q12(dt), 409);
            town->school.descent_fade_speed  = descent_min(town->school.descent_fade_speed, 12288);
            town->fade_speed = SH_Q12_TO_FLOAT(town->school.descent_fade_speed);
            break;

        default:
            
            ladder_unfreeze(town);
            finish(town);

            sh_save_event_set(save, 120);
            sh_snd_stop(1451);
            break;
    }

    if (town->step >= 2 && town->step < 6)
    {
        int32_t var_t0;

        if (town->school.descent_speed == 0)
        {
            sh_snd_call(1451);
        }

        descent_sound(town, 1451, (town->school.descent_speed + 409) >> 4, 32.0f);

        var_t0     = town->school.descent_speed + sh_q12_mul(SH_Q12(dt), 4096);
        var_t0     = descent_min(var_t0, 1638);
        town->school.descent_speed = var_t0;

        town->school.card_player.position.vy += sh_q12_mul(SH_Q12(dt), town->school.descent_speed);
        town->school.boss_box_y              += sh_q12_mul(SH_Q12(dt), town->school.descent_speed);
    }

}
