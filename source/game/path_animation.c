#include "sh3ds/path.h"
#include "sh3ds/fixed.h"

#include <stdlib.h>

typedef struct ShPathAnimDef {
    int status;
    int playback;
    bool variable;
    ShAnimClipTiming clip;
    int32_t duration;
} ShPathAnimDef;

#include "path_animation_data.h"
#include "roof_animation_data.h"

static const ShPathAnimDef *animation_defs(int state, unsigned *count)
{
    for (unsigned i = 0; i < sizeof(s_roof_animation_states) / sizeof(s_roof_animation_states[0]); i++) {
        if (s_roof_animation_states[i].state == state) {
            *count = sizeof(s_roof_animation_defs) / sizeof(s_roof_animation_defs[0]);
            return s_roof_animation_defs;
        }
    }
    *count = sizeof(s_path_animation_defs) / sizeof(s_path_animation_defs[0]);
    return s_path_animation_defs;
}

static bool model_timing(ShPathModel *model, int state, int32_t dt, int stick_y,
                         int stick_magnitude, ShAnimPoseTiming *pose)
{
    unsigned count;
    const ShPathAnimDef *defs = animation_defs(state, &count);
    for (unsigned i = 0; i < count; i++) {
        const ShPathAnimDef *def = &defs[i];
        if (def->status != model->animation_status) { continue; }
        ShAnimTiming timing = {model->animation_status, model->keyframe, model->time, model->alpha};
        const int32_t duration = def->variable ? path_walk_duration(stick_y, stick_magnitude) : def->duration;
        const int32_t timestep = sh_q12_mul(duration, dt);
        if (def->playback == 1) { sh_anim_blend_timing(&timing, &def->clip, timestep, pose); }
        else if (def->playback == 2) { sh_anim_once_timing(&timing, &def->clip, timestep, pose); }
        else { sh_anim_loop_timing(&timing, &def->clip, timestep, pose); }
        model->animation_status = timing.status;
        model->keyframe = timing.keyframe;
        model->time = timing.time;
        model->alpha = timing.alpha;
        return true;
    }
    return false;
}

void sh_path_arrival_start(ShPathPlayer *player)
{
    sh_path_school_animation_start(player, 74);
}

bool sh_path_school_animation_start(ShPathPlayer *player, int state)
{
    for (unsigned i = 0; i < sizeof(s_roof_animation_states) / sizeof(s_roof_animation_states[0]); i++) {
        if (s_roof_animation_states[i].state != state) { continue; }
        sh_path_state_set(player, state);
        player->model.animation_status = player->extra_model.animation_status = s_roof_animation_states[i].status;
        player->model.state_step = player->extra_model.state_step = 1;
        player->extra_model.control_state = 1;
        return true;
    }
    for (unsigned i = 0; i < sizeof(s_path_school_states) / sizeof(s_path_school_states[0]); i++) {
        if (s_path_school_states[i].state != state) { continue; }
        sh_path_state_set(player, state);
        player->model.animation_status = player->extra_model.animation_status = s_path_school_states[i].status;
        player->model.state_step = player->extra_model.state_step = 1;
        player->extra_model.control_state = 1;
        return true;
    }
    return false;
}

/* characters/player.c Player_AnimPlaybackStateGet. */
bool sh_path_school_animation_done(const ShPathPlayer *player)
{
    unsigned count;
    const ShPathAnimDef *defs = animation_defs(player->state, &count);
    for (unsigned i = 0; i < count; i++) {
        const ShPathAnimDef *def = &defs[i];
        if (def->status != player->model.animation_status || def->playback != 2) { continue; }
        return player->model.keyframe == (def->duration > 0 ? def->clip.last : def->clip.first);
    }
    return false;
}

/* Player_AnimGetSomething: startKeyframeIdx of the current animation entry. */
bool sh_path_animation_first(const ShPathPlayer *player, int *first)
{
    unsigned count;
    const ShPathAnimDef *defs = animation_defs(player->state, &count);
    for (unsigned i = 0; i < count; i++) {
        if (defs[i].status == player->model.animation_status) {
            *first = defs[i].clip.first;
            return true;
        }
    }
    return false;
}

bool sh_path_animation_timing(ShPathPlayer *player, int32_t dt, int stick_y,
                               int stick_magnitude, ShAnimPoseTiming *pose)
{
    ShAnimPoseTiming extra_pose;
    model_timing(&player->extra_model, player->state, dt, stick_y, stick_magnitude, &extra_pose);
    return model_timing(&player->model, player->state, dt, stick_y, stick_magnitude, pose);
}
