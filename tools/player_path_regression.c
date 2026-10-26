#include "sh3ds/path.h"
#include "sh3ds/fixed.h"

#include <assert.h>
#include <stdio.h>

typedef struct ShSchoolAnimation { int state, first, last; float fps; } ShSchoolAnimation;
#include "../source/game/school_arrival_anim.h"
typedef struct ShPathAnimDef {
    int status, playback;
    bool variable;
    ShAnimClipTiming clip;
    int32_t duration;
} ShPathAnimDef;
#include "../source/game/roof_animation_data.h"

int main(void)
{
    for (int dt = 68; dt <= 272; dt *= 2) {
        ShPathPlayer actor = {0};
        assert(sh_path_school_animation_start(&actor, 106));
        assert(actor.model.animation_status == 116);
        ShAnimPoseTiming pose;
        unsigned ticks = 0;
        while (!sh_path_school_animation_done(&actor) && ticks++ < 2000) {
            assert(sh_path_animation_timing(&actor, dt, 0, 0, &pose));
            assert(actor.model.keyframe == actor.extra_model.keyframe);
        }
        assert(ticks < 2000 && actor.model.animation_status == 117);
        for (int frame = 0; frame < 10; frame++) {
            const int keyframe = actor.model.keyframe;
            assert(sh_path_animation_timing(&actor, dt, 0, 0, &pose));
            assert(actor.model.keyframe == keyframe && sh_path_school_animation_done(&actor));
        }
    }
    for (unsigned state = 0; state < sizeof(s_roof_animation_states) / sizeof(s_roof_animation_states[0]); state++) {
        const ShPathAnimDef *playback = NULL;
        for (unsigned clip = 0; clip < sizeof(s_roof_animation_defs) / sizeof(s_roof_animation_defs[0]); clip++) {
            const ShPathAnimDef *candidate = &s_roof_animation_defs[clip];
            if (candidate->status == (s_roof_animation_states[state].status | 1)) { playback = candidate; }
        }
        assert(playback);
        for (int dt = 34; dt <= 272; dt *= 2) {
            ShPathPlayer actor = {0};
            assert(sh_path_school_animation_start(&actor, s_roof_animation_states[state].state));
            assert(actor.model.animation_status == s_roof_animation_states[state].status);
            assert(actor.extra_model.animation_status == actor.model.animation_status);
            ShAnimPoseTiming pose;
            int frames = 0;
            const int endpoint = playback->duration < 0 ? playback->clip.first : playback->clip.last;
            while (!sh_path_school_animation_done(&actor) && frames++ < 2000) {
                assert(sh_path_animation_timing(&actor, dt, 0, 0, &pose));
                assert(actor.model.keyframe == actor.extra_model.keyframe);
                if (actor.model.animation_status == playback->status) {
                    assert(actor.model.keyframe >= playback->clip.first && actor.model.keyframe <= playback->clip.last);
                }
            }
            assert(frames < 2000 && actor.model.keyframe == endpoint);
            for (int hold = 0; hold < 100; hold++) {
                assert(sh_path_animation_timing(&actor, dt, 0, 0, &pose));
                assert(sh_path_school_animation_done(&actor) && actor.model.keyframe == endpoint);
            }
        }
    }
    {
        ShPath path = {0};
        ShPathPlayer player = {.upper_body_state = 9, .lower_body_state = 8};
        player.model.state_step = player.extra_model.state_step = 7;
        sh_path_state_set(&player, 56);
        assert(!player.model.state_step && !player.extra_model.state_step);
        assert(!player.upper_body_state && !player.lower_body_state);
        player.cutscene_state = 3;
        sh_path_locomotion(&path, &player, 2, 136);
        assert(player.model.animation_status == 26 && player.extra_model.animation_status == 26);
        assert(player.model.state_step == 1 && player.extra_model.control_state == 1);
        sh_path_locomotion(&path, &player, 2, 136);
        assert(player.model.state_step == 1 && player.extra_model.control_state == 1);
        player.cutscene_state = 1;
        sh_path_locomotion(&path, &player, 2, 136);
        assert(player.state == 53 && !player.model.state_step && !player.extra_model.state_step);
        sh_path_locomotion(&path, &player, 2, 136);
        assert(player.model.animation_status == 4 && player.extra_model.animation_status == 4);
        player.animation_reset = true;
        sh_path_locomotion(&path, &player, 2, 136);
        assert(player.state == 52 && !player.animation_reset && !player.model.state_step);
    }
    for (int heading = 0; heading < 4096; heading++) {
        ShPath path = {0};
        ShPathPlayer player = {0};
        ShPathPoint point = {0};
        assert(!sh_path_player_step(&path, &player, 53, &point, heading, 1, 136));
        assert(path.step == 1);
        assert(!sh_path_player_step(&path, &player, 53, &point, heading, 1, 136));
        assert(path.step == 6);
        assert(!sh_path_player_step(&path, &player, 53, &point, heading, 1, 136));
        assert(path.step == ((heading < 64 || heading > 4032) ? 8 : 7));
        if (path.step == 7) {
            assert(player.state == (heading < 2048 ? 56 : 57));
            player.rotation.vy = heading;
            assert(!sh_path_player_step(&path, &player, 53, &point, heading, 1, 136));
            assert(path.step == 8 && player.state == 52);
        }
        assert(sh_path_player_step(&path, &player, 53, &point, heading, 1, 136));
        assert(path.step == 0 && player.animation_reset && player.move_speed == 0);
    }
    for (int dt = 68; dt <= 272; dt += 68) {
        for (unsigned clip = 1; clip < sizeof(s_school_arrival_animations) / sizeof(s_school_arrival_animations[0]); clip++) {
            const ShSchoolAnimation *animation = &s_school_arrival_animations[clip];
            ShPathPlayer actor = {.model = {.keyframe = 970, .time = 970 * 4096}};
            assert(sh_path_school_animation_start(&actor, animation->state));
            assert(!sh_path_school_animation_done(&actor));
            ShAnimPoseTiming pose;
            int frames;
            bool moved = false;
            int previous = actor.model.keyframe;
            for (frames = 0; frames < 1000 && !sh_path_school_animation_done(&actor); frames++) {
                assert(sh_path_animation_timing(&actor, dt, 0, 0, &pose));
                if (actor.model.keyframe >= animation->last && actor.model.keyframe <= animation->first &&
                    actor.model.keyframe < previous) { moved = true; }
                previous = actor.model.keyframe;
            }
            assert(frames < 1000 && actor.model.keyframe == animation->last);
            assert(animation->fps > 0 || moved);
            for (int tick = 0; tick < 100; tick++) {
                assert(sh_path_animation_timing(&actor, dt, 0, 0, &pose));
                assert(sh_path_school_animation_done(&actor) && actor.model.keyframe == animation->last);
            }
        }
        {
            ShPathPlayer actor = {0};
            sh_path_arrival_start(&actor);
            assert(actor.state == 74 && actor.model.animation_status == 106);
            ShAnimPoseTiming pose;
            bool active = false;
            int frame;
            for (frame = 0; frame < 1000; frame++) {
                assert(sh_path_animation_timing(&actor, dt, 0, 0, &pose));
                assert(pose.alpha >= 0 && pose.alpha < 4096);
                if (actor.model.animation_status == 107) { active = true; }
                if (actor.model.keyframe == 1016) { break; }
            }
            assert(frame < 1000 && active && actor.model.keyframe == 1016);
            assert(actor.model.time == 1016 * 4096);
            const float duration = (frame + 1) * dt / 4096.0f;
            assert(duration > 5.2f && duration < 5.5f);
            for (int tick = 0; tick < 100; tick++) {
                assert(sh_path_animation_timing(&actor, dt, 0, 0, &pose));
                assert(actor.model.keyframe == 1016 && pose.first == 1016 && pose.alpha == 0);
            }
        }
        {
            ShPath route = {0};
            ShPathPlayer actor = {.rotation = {1024}};
            ShPathPoint target[2] = {{0, 0, SH_Q12(2.0f)}, {0}};
            bool done = false;
            for (int frame = 0; frame < 2000 && !done; frame++) {
                done = sh_path_player_step(&route, &actor, 53, target, 3072, 1, dt);
                if (done) { break; }
                sh_path_locomotion(&route, &actor, actor.model.keyframe, dt);
                ShAnimPoseTiming pose;
                if (actor.state == 53 || actor.state == 56 || actor.state == 57) {
                    assert(sh_path_animation_timing(&actor, dt, 0, 0, &pose));
                    assert(pose.alpha >= 0 && pose.alpha < 4096);
                    assert(actor.model.keyframe == actor.extra_model.keyframe);
                }
                int32_t distance = sh_q12_mul(actor.move_speed, dt);
                actor.position.vx += sh_q12_mul(distance, sh_fixed_sin(actor.rotation.vy));
                actor.position.vz += sh_q12_mul(distance, sh_fixed_cos(actor.rotation.vy));
            }
            assert(done && actor.rotation.vy == 3072);
            assert(actor.position.vz > SH_Q12(1.8f) && actor.position.vz < SH_Q12(2.2f));
            assert(actor.position.vx > -SH_Q12(0.2f) && actor.position.vx < SH_Q12(0.2f));
        }
        ShPath path = {0};
        ShPathPlayer player = {0};
        ShPathPoint points[2] = {{0, 0, SH_Q12(2.0f)}, {0, 0, SH_Q12(4.0f)}};
        for (int i = 0; i < 5; i++) {
            assert(!sh_path_player_step(&path, &player, 53, points, 0, 2, dt));
        }
        assert(path.step == 5 && player.state == 53 && player.cutscene_state == 1);
        player.position = points[0];
        assert(!sh_path_player_step(&path, &player, 53, points, 0, 2, dt));
        assert(path.step == 4 && path.waypoint == 1);
        assert(!sh_path_player_step(&path, &player, 53, points, 0, 2, dt));
        player.position = points[1];
        assert(!sh_path_player_step(&path, &player, 53, points, 0, 2, dt));
        assert(path.step == 6 && path.waypoint == 2);
        assert(!sh_path_player_step(&path, &player, 53, points, 0, 2, dt));
        assert(path.step == 8);
        assert(sh_path_player_step(&path, &player, 53, points, 0, 2, dt));
    }
    puts("PASS: 4096 final headings, both turn directions, waypoint advancement and completion at four timesteps");
    return 0;
}
