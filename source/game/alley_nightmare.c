#include "sh3ds/alley.h"
#include "sh3ds/controls.h"
#include "sh3ds/drain.h"
#include "sh3ds/snd.h"
#include "sh3ds/sound.h"
#include "sh3ds/fixed.h"

#include <3ds.h>
#include <limits.h>
#include <math.h>
#include <string.h>

typedef struct ShNightmareCameraCommand {
    ShPathPoint point;
    int32_t params[4];
} ShNightmareCameraCommand;

#include "alley_nightmare_data.h"

int sh_alley_nightmare_head_bone(void)
{
    return SH_NIGHTMARE_HEAD_BONE;
}

static void camera_update(ShAlley *alley, float dt)
{
    ShNightmare *scene = &alley->nightmare;
    ShPathPoint position, look;
    ShVcScriptParams params = {{0}, {0}};
    const ShNightmareCameraCommand *camera, *watch;
    bool warp = false;
    if (alley->step == 3) {
        camera = &s_nightmare_camera[0];
        watch = &s_nightmare_camera[2];
        if (!scene->camera_phase) {
            warp = true;
            scene->camera_phase++;
        }
        position = camera->point;
        look = (ShPathPoint){scene->player.position.vx + watch->point.vx,
            scene->player.position.vy + watch->point.vy, scene->player.position.vz + watch->point.vz};
    } else if (alley->step == 4) {
        if (scene->camera_phase == 1) {
            scene->hidden = true;
            camera = &s_nightmare_camera[3];
            watch = &s_nightmare_camera[4];
            position = camera->point;
            look = watch->point;
            scene->player.position = s_nightmare_player_warp;
            scene->camera_phase++;
            warp = true;
        } else {
            if (!scene->head_valid) { return; }
            camera = &s_nightmare_camera[5];
            watch = &s_nightmare_camera[6];
            ShCharaBone head;
            const int32_t root[3] = {scene->player.position.vx, scene->player.position.vy, scene->player.position.vz};
            sh_drain_valve_world_bone(&scene->head, root, scene->player.rotation.vy, &head);
            int32_t *out[3] = {&position.vx, &position.vy, &position.vz};
            for (unsigned row = 0; row < 3; row++) {
                int64_t value = 0;
                for (unsigned col = 0; col < 3; col++) {
                    value += (int64_t)head.rotation[row][col] * s_nightmare_head_offset[col];
                }
                *out[row] = (head.translation[row] + (int32_t)(value >> 12)) * 16;
            }
            look = watch->point;
        }
    } else {
        if (alley->step > 4) {
            scene->hidden = false;
            alley->cam_active = false;
            sh_vc_warp(&alley->vc);
        }
        return;
    }
    for (unsigned i = 0; i < 4; i++) {
        params.camera[i] = SH_Q12_TO_FLOAT(camera->params[i]);
        params.watch[i] = SH_Q12_TO_FLOAT(watch->params[i]);
    }
    const float p[3] = {SH_Q12_TO_FLOAT(position.vx), SH_Q12_TO_FLOAT(position.vy), SH_Q12_TO_FLOAT(position.vz)};
    const float l[3] = {SH_Q12_TO_FLOAT(look.vx), SH_Q12_TO_FLOAT(look.vy), SH_Q12_TO_FLOAT(look.vz)};
    sh_vc_script_update(&alley->vc, p, l, &params, warp, warp, dt, &alley->cam);
    alley->cam_active = true;
    alley->vc_scripted = true;
}

static void message_update(ShAlley *alley, const ShInputState *input, float dt)
{
    ShNightmare *scene = &alley->nightmare;
    sh_bgm_status_set(SH_BGM_STATUS_VOICE_DIALOG);
    scene->text_frames += (double)dt * 30.0;
    scene->text_delta += SH_Q12(dt);
    scene->text_skip |= sh_input_down(input, SH_BTN_ACTION | SH_BTN_CANCEL) || (input->held & KEY_START) != 0;
    while (scene->text_frames >= 1.0) {
        const int32_t delta = (int32_t)llround((double)scene->text_delta / scene->text_frames);
        scene->text_frames -= 1.0;
        scene->text_delta -= delta;
        const int result = sh_nightmare_text_step(&scene->text, SH_NIGHTMARE_MESSAGE, delta,
            scene->text_skip, true, sh_sound_streaming());
        scene->text_skip = false;
        if (scene->text.stop_audio) { sh_snd_call(19); }
        if (result == -1 && scene->voice_index < sizeof(s_nightmare_voices) / sizeof(s_nightmare_voices[0])) {
            sh_snd_call(s_nightmare_voices[scene->voice_index++]);
        }
        alley->subtitle = scene->text.visible[0] ? scene->text.visible : NULL;
        if (result > 0) {
            alley->subtitle = NULL;
            alley->step++;
            break;
        }
    }
}

void sh_alley_nightmare_update(ShAlley *alley, ShSavegame *save, float position[3], float *yaw,
    const ShInputState *input, float dt)
{
    ShNightmare *scene = &alley->nightmare;
    const int32_t delta = SH_Q12(dt);
    alley->controls_locked = true;
    alley->cheryl_visible = false;
    alley->letterbox = false;
    camera_update(alley, dt);
    switch (alley->step) {
    case 0:
        alley->nightmare_world = true;
        alley->nightmare_world_requested = true;
        sh_snd_stop(1358);
        sh_snd_play((uint16_t)s_nightmare_start_sound[0], s_nightmare_start_sound[1], s_nightmare_start_sound[2]);
        scene->player.position = (ShPathPoint){SH_Q12(position[0]), SH_Q12(-position[1]), SH_Q12(-position[2])};
        scene->player.rotation.vy = SH_Q12(-*yaw / (2.0f * 3.14159265358979323846f)) & 4095;
        scene->player.model.keyframe = scene->player.extra_model.keyframe = alley->torch_player.model.keyframe;
        sh_path_state_set(&scene->player, 52);
        memcpy(scene->waypoints, s_nightmare_waypoints, sizeof(scene->waypoints));
        for (unsigned i = 0; i < 2; i++) { scene->waypoints[i].vy = scene->player.position.vy; }
        scene->initialized = true;
        alley->step++;
        /* fallthrough */
    case 1:
        if (!scene->border_step) {
            alley->border.state = SH_BORDER_FADEINSTART;
            scene->border_step++;
        } else if (alley->border.state == SH_BORDER_SHOWN) { alley->step++; }
        break;
    case 2:
        if (!scene->player.move_speed) { alley->step++; }
        break;
    case 3:
        if (sh_path_player_step(&scene->path, &scene->player, 53, scene->waypoints, s_nightmare_heading, 2, delta)) {
            alley->step++;
        }
        break;
    case 4:
        message_update(alley, input, dt);
        break;
    default:
        alley->controls_locked = false;
        alley->border.state = SH_BORDER_FADEOUTSTART;
        sh_save_event_set(save, SH_EV_NIGHTMARE);
        alley->stage = SH_ALLEY_FIGHT;
        alley->step = 0;
        alley->wait = 0.0f;
        return;
    }
    /* GameState_InGame_Update: event, then player motion and animation. */
    sh_path_locomotion(&scene->path, &scene->player, scene->player.model.keyframe, delta);
    ShAnimPoseTiming pose;
    sh_path_animation_timing(&scene->player, delta, 0, 0, &pose);
    const int32_t distance = sh_q12_mul(scene->player.move_speed, delta);
    scene->player.position.vx += sh_q12_mul(distance, sh_fixed_sin(scene->player.heading_angle));
    scene->player.position.vz += sh_q12_mul(distance, sh_fixed_cos(scene->player.heading_angle));
    position[0] = SH_Q12_TO_FLOAT(scene->player.position.vx);
    position[1] = -SH_Q12_TO_FLOAT(scene->player.position.vy);
    position[2] = -SH_Q12_TO_FLOAT(scene->player.position.vz);
    *yaw = -SH_Q12_TO_FLOAT(scene->player.rotation.vy) * (2.0f * 3.14159265358979323846f);
}
