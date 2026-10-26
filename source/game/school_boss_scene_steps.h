/* Generated from MAP1_S05 func_800D49AC and func_800D4D1C; original steps retained. */
#define BOSS_SCENE_PARTICLES 500
static void boss_intro(ShSchoolBossScene *scene, ShSchoolBossSceneContext *ctx)
{

    int32_t i;

    if (ctx->skip)
    {
        if (scene->step >= 3)
        {
            ctx->particles->field_1  = 0;
            ctx->particles->field_10 = 0;

            for (i = 0; i < 500; i++)
            {
                int32_t field_A = *boss_scene_particle(ctx, i);
                if (field_A < 12)
                {
                    if (field_A >= 8)
                    {
                        *boss_scene_particle(ctx, i) = 12;
                    }
                }
            }

            ctx->camera_return(ctx->user, true);
            boss_scene_reset(scene);
        }
    }

    switch (scene->step)
    {
        case 0:
            ctx->control(ctx->user, true);
            boss_scene_fade(scene, ctx, 0, false, 0, 0, false);

            ctx->border_show(ctx->user);
            scene->light      = 0;

            
            ctx->camera(ctx->user, false, &(ShPathPoint){80773, -21299, -113049});
            ctx->camera(ctx->user, true, &(ShPathPoint){80773, -13516, -98304});

            boss_scene_next(scene);

        /* fall through */
        case 1:
            boss_scene_wait(scene, ctx, 6144, false);
            break;

        case 2:
            ctx->particles->field_1 = 3;
            boss_scene_next(scene);

        /* fall through */
        case 3:
            boss_scene_wait(scene, ctx, 819, false);
            break;

        case 4:
            scene->light += sh_q12_mul(ctx->delta, 921);
            if (scene->light > 9216)
            {
                scene->light = 9216;
                boss_scene_next(scene);
            }
            break;

        case 5:
            scene->light -= sh_q12_mul(ctx->delta, 1843);
            if (scene->light < 5670)
            {
                sh_save_event_set(ctx->save, 129);
                boss_scene_reset(scene);
            }
            break;

        default:
            ctx->control(ctx->user, false);

            boss_scene_gameplay(scene, ctx);

            scene->light = 5670;

            boss_scene_fade(scene, ctx, 0, false, 2, 0, false);
            ctx->camera_return(ctx->user, false);

            sh_save_event_set(ctx->save, 130);
            ctx->autosave(ctx->user);
            break;
    }

}
static void boss_outro(ShSchoolBossScene *scene, ShSchoolBossSceneContext *ctx)
{
if (scene->step > 0 && scene->step < 5) { ctx->echo(ctx->user); }
switch (scene->step)
    {
        case 0:
            sh_save_event_clear(ctx->save, 129);
            scene->exit_sound_phase = 1;
            boss_scene_next(scene);

        /* fall through */
        case 1:
            boss_scene_wait(scene, ctx, 6144, false);

            scene->light += sh_q12_mul(ctx->delta, 819);
            if (scene->light > 12288)
            {
                scene->light = 12288;
            }
            break;

        case 2:
            ctx->particles->field_1 = 1;
            ctx->particles->field_2 = 3;
            boss_scene_next(scene);

        /* fall through */
        case 3:
            if (scene->exit_sound_phase & 1)
            {
                ctx->sound(ctx->user, 1359);
                scene->exit_sound_phase++;
            }

            ctx->sound_attributes(ctx->user, 1359, 0, BOSS_SCENE_MAX(0, (BOSS_SCENE_FRACT(scene->light) >> 4) - (scene->exit_sound_phase * 8)), 0);

            scene->light -= sh_q12_mul(ctx->delta, 614);
            if (scene->light < (6 - scene->exit_sound_phase) * 2048)
            {
                scene->exit_sound_phase++;
            }

            if (scene->light < 0)
            {
                scene->light = 0;
                boss_scene_next(scene);
            }
            break;

        case 4:
            boss_scene_fade(scene, ctx, 2, true, 0, 10240, false);
            break;

        default:
            ctx->control(ctx->user, false);
            boss_scene_gameplay(scene, ctx);
            sh_save_event_set(ctx->save, 132);
            break;
    }

}
