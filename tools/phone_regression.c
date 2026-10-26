#include "sh3ds/phone.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void text_regression(void)
{
    for (int held_skip = 0; held_skip < 2; held_skip++) {
        ShPhoneText text = {0};
        assert(sh_phone_text_step(&text, 31, 136, held_skip, false, 0) == -1);
        int pages = 0, result = 0;
        for (unsigned tick = 0; tick < 1000 && result != 1; tick++) {
            const int previous = text.current, timer = text.timer;
            result = sh_phone_text_step(&text, 31, 136, held_skip, false, 0);
            if (text.current != previous) {
                assert(text.current == previous + 1 && timer <= 136);
                pages++;
            }
            if (result == 1) { assert(previous == 34 && timer <= 136); }
        }
        assert(result == 1 && pages == 3 && !text.active);
    }
    ShPhoneText text = {0};
    assert(sh_phone_text_step(&text, 31, 136, false, true, 0) == -1);
    for (int i = 0; i < 3; i++) {
        assert(sh_phone_text_step(&text, 31, 136, false, true, 0) == 0);
        assert(text.timer == -1 && !text.visible[0]);
    }
    assert(sh_phone_text_step(&text, 31, 136, false, true, 4) == 0 && !text.load_audio);
    assert(sh_phone_text_step(&text, 31, 136, false, true, 0) == 0 && text.timer == 3686);
    while (text.current == 31) { sh_phone_text_step(&text, 31, 136, false, true, 0); }
    assert(text.stop_audio && text.load_audio && text.current == 32);
    assert(sh_phone_text_step(&text, 31, 136, false, true, 0) == 0 && text.timer == -1);
    assert(sh_phone_text_step(&text, 31, 136, false, true, 4) == 0);
    assert(sh_phone_text_step(&text, 31, 136, false, true, 0) == 0 && text.timer == 12288);
    assert(strlen(text.visible) == 2);

    memset(&text, 0, sizeof(text));
    assert(sh_phone_text_step(&text, 34, 136, false, false, 0) == -1);
    assert(sh_phone_text_step(&text, 34, 136, false, false, 0) == 0);
    assert(!strcmp(text.visible, "Cheryl!") && text.timer == 8192 && text.rollout == -1);
    assert(sh_phone_text_step(&text, 35, 136, false, false, 0) == -1);
    assert(sh_phone_text_step(&text, 35, 136, true, false, 0) == 0);
    assert(!strcmp(text.visible, "That was Cheryl's voice.\nI know it.") && text.timer == 9830);
    assert(sh_phone_text_step(&text, 35, 136, true, false, 0) == -1 && text.current == 36 && text.timer == -1);
    assert(sh_phone_text_step(&text, 35, 136, true, false, 0) == 0);
    assert(!strcmp(text.visible, "Where is she?\nCheryl...") && text.timer == 8192);
    assert(sh_phone_text_step(&text, 35, 136, true, false, 0) == 1 && !text.active);
}

int main(void)
{
    text_regression();
    ShPhoneAudio audio;
    sh_phone_audio_reset(&audio);
    ShPathPlayer speaking = {.state = 97, .model = {.animation_status = 371}};
    unsigned steps = 0, pickup_sounds = 0;
    sh_rng_set_seed(1234);
    for (int frame = 1062; frame <= 1110; frame++) {
        speaking.model.keyframe = frame;
        ShPhoneAudioFrame output;
        sh_phone_audio_step(&audio, &speaking, 0, &output);
        for (unsigned i = 0; i < output.count; i++) {
            const ShPhoneSound *sound = &output.sounds[i];
            if (sound->id == 1456) {
                assert(frame == 1088 && sound->volume == 64 && (sound->flags & sh_phone_sound_no_distance_flag()));
                pickup_sounds++;
            } else {
                assert(sound->id == 1311 && sound->volume == 64 && sound->pitch >= -16 && sound->pitch < 16);
                assert(frame == 1066 || frame == 1072 || frame == 1075);
                steps++;
            }
        }
    }
    assert(steps == 3 && pickup_sounds == 1);
    const uint32_t seed = sh_rng_get_seed();
    sh_rng_set_seed(1234);
    for (int i = 1062; i <= 1110; i++) { sh_rng_rand16(); sh_rng_rand16(); }
    assert(sh_rng_get_seed() == seed);
    for (int delta = 68; delta <= 272; delta *= 2) {
        ShPathPlayer player = {.position = {SH_Q12(22), 0, SH_Q12(143)}};
        ShPhoneMotion motion;
        sh_phone_motion_begin(&motion, &player);
        assert(motion.waypoint.vx == SH_Q12(20.3f) && motion.waypoint.vz == SH_Q12(143.5f));
        assert(motion.heading == -1024);
        unsigned ticks = 0;
        while (!sh_phone_path_step(&motion, &player, delta) && ticks++ < 2000) {
            sh_path_locomotion(&motion.path, &player, player.model.keyframe, delta);
            if (player.state == 53 || player.state == 56 || player.state == 57) {
                ShAnimPoseTiming pose;
                assert(sh_path_animation_timing(&player, delta, 0, 0, &pose));
            }
            const int32_t distance = sh_q12_mul(player.move_speed, delta);
            player.position.vx += sh_q12_mul(distance, sh_fixed_sin(player.heading_angle));
            player.position.vz += sh_q12_mul(distance, sh_fixed_cos(player.heading_angle));
        }
        assert(ticks < 2000 && (player.rotation.vy & 4095) == 3072 && player.move_speed == 0);
        assert(player.position.vx > motion.waypoint.vx - SH_Q12(0.2f) && player.position.vx < motion.waypoint.vx + SH_Q12(0.2f));
        assert(player.position.vz > motion.waypoint.vz - SH_Q12(0.2f) && player.position.vz < motion.waypoint.vz + SH_Q12(0.2f));
        ShPhoneView view;
        assert(sh_phone_camera_view(&motion, &player, 5, delta, &view));
        assert(view.warp_position && view.warp_look && view.position.vx == SH_Q12(18.06f));
        assert(sh_phone_camera_view(&motion, &player, 9, delta, &view));
        assert(view.position.vx == player.position.vx + SH_Q12(4) && view.position.vz == player.position.vz);
        for (unsigned frame = 0; frame < 500; frame++) {
            assert(sh_phone_camera_view(&motion, &player, 15, delta, &view));
            assert(!view.warp_position && !view.warp_look && motion.tween[0] == motion.tween[1]);
        }
        assert(motion.tween[0] == SH_Q12(6.5f));
        assert(view.position.vx == player.position.vx + SH_Q12(4) + SH_Q12(-2.8f));
        assert(view.look.vx == player.position.vx + SH_Q12(-2.8f));
        assert(sh_phone_camera_view(&motion, &player, 16, delta, &view));
        const ShPhoneView cut = view;
        assert(sh_phone_camera_view(&motion, &player, 28, delta, &view));
        assert(view.position.vx == cut.position.vx && view.look.vz == cut.look.vz);
        assert(!sh_phone_camera_view(&motion, &player, 0, delta, &view));
    }
    ShSavegame save = {0};
    bool hidden = false;
    assert(sh_phone_animation_events(&save, 97, 1064, &hidden) && hidden && !sh_save_event_get(&save, 99));
    assert(sh_phone_animation_events(&save, 97, 1065, &hidden) && hidden && sh_save_event_get(&save, 99));
    assert(sh_phone_animation_events(&save, 97, 1089, &hidden) && hidden);
    assert(sh_phone_animation_events(&save, 97, 1090, &hidden) && !hidden);
    assert(!sh_phone_animation_events(&save, 98, 1146, &hidden) && sh_save_event_get(&save, 99));
    assert(!sh_phone_animation_events(&save, 98, 1147, &hidden) && !sh_save_event_get(&save, 99));
    unsigned count;
    const ShPhonePage *pages = sh_phone_pages(&count);
    assert(count == 6);
    for (unsigned i = 0; i < count; i++) {
        assert(pages[i].message == 31 + (int)i && pages[i].duration > 0);
        assert(pages[i].audio_type == (i < 4 ? 1 : 2));
        assert(pages[i].end == (i == 3 || i == 5));
        assert(pages[i].instant == (i == 3));
        ShPhoneText text = {0};
        assert(sh_phone_text_step(&text, pages[i].message, 136, false, false, 0) == -1);
        assert(sh_phone_text_step(&text, pages[i].message, 136, true, false, 0) == 0);
        assert(text.timer == pages[i].duration && text.audio_type == pages[i].audio_type);
        assert((text.menu == -1) == pages[i].end && text.rollout == -1);
    }
    puts("PASS: phone path/cameras/animation flags and six timed pages, rollout, skip rules and XA load gating");
    return 0;
}
