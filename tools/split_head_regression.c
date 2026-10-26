#include "sh3ds/split_head.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Fixture { unsigned rays, effects; bool wall, character; } Fixture;
static ShSplitRay ray(void *user, const ShPathPoint *from, const ShPathPoint *direction)
{
    Fixture *fixture = user;
    (void)from;
    assert(direction->vy == 0);
    fixture->rays++;
    return (ShSplitRay){fixture->wall, fixture->character};
}
static void effect(void *user, const ShPathPoint *position, int type, int count)
{
    Fixture *fixture = user;
    assert(type == 2 && count == 7 && position->vy <= -SH_Q12_ONE);
    fixture->effects++;
}

static void damage_test(void)
{
    ShSavegame save = {0};
    Fixture fixture = {0};
    ShSplitContext ctx = {.save = &save, .delta = 136, .ray = ray, .player_effect = effect, .user = &fixture};
    ShSplitHead boss = {0};
    sh_split_head_begin(&boss);
    assert(boss.health == 25500 && boss.model.control_state == 8 && boss.model.animation_status == 21);
    assert(boss.model.keyframe == 162 && boss.model.time == SH_Q12(162));
    assert(boss.props.field_ea == 196 && boss.wait_limit == SH_Q12(6.4f));
    assert(sh_split_head_control(&boss, &ctx) && boss.model.control_state == 8);
    sh_save_event_set(&save, 130);
    assert(sh_split_head_control(&boss, &ctx) && boss.model.control_state == 2 && boss.model.animation_status == 22);
    assert((boss.props.flags & 256) && !sh_save_event_get(&save, 131));

    boss.damage_amount = SH_Q12(100);
    sh_split_head_damage(&boss, &ctx);
    assert(boss.health == 25400 && !boss.damage_amount && !ctx.player_damage_disabled);
    sh_split_head_begin(&boss);
    boss.props.flags = 1;
    ctx.weapon_attack = 34;
    boss.damage_amount = SH_Q12(100);
    sh_split_head_damage(&boss, &ctx);
    assert(boss.health == 22300 && boss.model.control_state == 4 && (boss.props.flags & 16));
    sh_split_head_begin(&boss);
    boss.props.flags = 1;
    ctx.difficulty = 1;
    boss.damage_amount = SH_Q12(100);
    sh_split_head_damage(&boss, &ctx);
    assert(boss.health == 24700);
    sh_split_head_begin(&boss);
    boss.props.flags = 1;
    boss.damage_amount = 1;
    sh_split_head_damage(&boss, &ctx);
    assert(boss.health == 25484);

    boss.health = 1;
    boss.props.flags = 16;
    boss.model.animation_status = 5;
    boss.damage_amount = SH_Q12(1000);
    ctx.player_dead = true;
    sh_split_head_damage(&boss, &ctx);
    assert(boss.health == 1 && !ctx.player_damage_disabled && !sh_save_event_get(&save, 131));
    boss.damage_amount = SH_Q12(1000);
    ctx.player_dead = false;
    sh_split_head_damage(&boss, &ctx);
    assert(!boss.health && ctx.player_damage_disabled && boss.model.control_state == 6);
    assert(boss.model.animation_status == 16 && !sh_save_event_get(&save, 131));
    assert(sh_split_head_control(&boss, &ctx) && sh_save_event_get(&save, 131));
    assert(ctx.sound_count == 1 && ctx.sounds[0].id == 1479 && ctx.sounds[0].volume == 128);
    assert(sh_split_head_control(&boss, &ctx) && !ctx.sound_count);
}

static void control_test(void)
{
    ShSavegame save = {0};
    sh_save_event_set(&save, 130);
    Fixture fixture = {0};
    unsigned cases = 0, sounds = 0;
    for (int state = 1; state <= 8; state++) {
        for (int status = 0; status < 30; status++) {
            for (int geometry = 0; geometry < 3; geometry++) {
                for (int flags = 0; flags <= 1023; flags += 1023) {
                    ShSplitHead boss = {0};
                    sh_split_head_begin(&boss);
                    boss.model.control_state = state;
                    boss.model.animation_status = status;
                    boss.model.time = SH_Q12(35);
                    boss.props.flags = flags;
                    boss.props.timer_f2 = SH_Q12(0.7f);
                    boss.move_speed = SH_Q12(1.2f);
                    fixture.wall = geometry != 0;
                    fixture.character = geometry == 2;
                    ShSplitContext ctx = {.save = &save, .player = {SH_Q12(4), 0, SH_Q12(2)},
                        .delta = 136, .ray = ray, .player_effect = effect, .user = &fixture};
                    ShSplitHead replay = boss;
                    sh_rng_set_seed(1234);
                    assert(sh_split_head_control(&boss, &ctx));
                    const uint32_t seed = sh_rng_get_seed();
                    sounds += ctx.sound_count;
                    sh_rng_set_seed(1234);
                    assert(sh_split_head_control(&replay, &ctx));
                    assert(!memcmp(&boss, &replay, sizeof(boss)) && seed == sh_rng_get_seed());
                    assert(boss.model.control_state >= 1 && boss.model.control_state <= 8);
                    cases++;
                }
            }
        }
    }
    assert(cases == 1440 && fixture.rays > 1000 && fixture.effects && sounds);
}

static void playback_step(ShSplitHead *boss, ShSplitContext *ctx, ShAnimPoseTiming *pose)
{
    assert(sh_split_head_control(boss, ctx));
    assert(sh_split_head_timing(boss, ctx, pose));
    sh_split_head_sounds(boss, ctx);
}

static void sound_test(void)
{
    ShSplitHead boss = {0};
    sh_split_head_begin(&boss);
    boss.model.time = SH_Q12(187);
    boss.movement_duration = SH_Q12(24);
    boss.props.flags = 128 | 256;
    ShSplitContext ctx = {.delta = 136};
    sh_rng_set_seed(1234);
    sh_split_head_sounds(&boss, &ctx);
    assert(ctx.sound_count == 2 && ctx.sounds[0].id == 1472 && ctx.sounds[1].id == 1422);
    assert(ctx.sounds[0].kind == 4 && ctx.sounds[0].volume == 128 && ctx.sounds[0].pitch >= 0 && ctx.sounds[0].pitch < 8);
    assert(!(boss.props.flags & (128 | 256)) && boss.props.sound_timer >= SH_Q12(3) && boss.props.sound_timer < SH_Q12(5));
    ctx.sound_count = 0;
    sh_split_head_sounds(&boss, &ctx);
    assert(!ctx.sound_count);
    boss.model.time = SH_Q12(170);
    sh_split_head_sounds(&boss, &ctx);
    boss.model.time = SH_Q12(187);
    boss.movement_duration = SH_Q12(-24);
    sh_split_head_sounds(&boss, &ctx);
    assert(ctx.sound_count == 1 && ctx.sounds[0].id == 1472 && ctx.sounds[0].volume == 64);
    assert(ctx.sounds[0].pitch >= -128 && ctx.sounds[0].pitch < -120);
    ctx.sound_count = 0;
    boss.props.sound_timer = 0;
    boss.props.flags |= 128;
    sh_split_head_sounds(&boss, &ctx);
    assert(ctx.sound_count == 1 && ctx.sounds[0].id == 1474);
}

static void playback_test(int32_t delta)
{
    ShSavegame save = {0};
    Fixture fixture = {0};
    ShSplitContext ctx = {.save = &save, .player = {0, 0, SH_Q12(3)}, .delta = delta,
        .ray = ray, .player_effect = effect, .user = &fixture};
    ShSplitHead boss = {0};
    sh_split_head_begin(&boss);
    ShAnimPoseTiming pose;
    for (int frame = 0; frame < 100; frame++) {
        playback_step(&boss, &ctx, &pose);
        assert(boss.model.control_state == 8 && boss.model.keyframe >= 162 && boss.model.keyframe <= 171);
    }
    sh_save_event_set(&save, 130);
    bool walked = false, attacked = false;
    for (int frame = 0; frame < 5000; frame++) {
        playback_step(&boss, &ctx, &pose);
        walked |= boss.model.animation_status == 23;
        attacked |= boss.model.animation_status == 7;
        assert(!sh_save_event_get(&save, 131));
    }
    assert(walked && attacked && boss.health == 25500);

    sh_split_head_begin(&boss);
    boss.props.flags = 1;
    boss.damage_amount = SH_Q12(100);
    ctx.weapon_attack = 34;
    sh_split_head_damage(&boss, &ctx);
    assert(boss.model.control_state == 4);
    int frames = 0;
    while (boss.model.control_state == 4 && frames++ < 2000) {
        playback_step(&boss, &ctx, &pose);
    }
    assert(frames < 2000 && (int64_t)frames * delta > SH_Q12(6.4f));
    assert(boss.model.control_state == 5 && boss.wait_limit == SH_Q12(3.8f));

    boss.health = 1;
    boss.damage_amount = SH_Q12(1000);
    boss.model.animation_status = 5;
    sh_split_head_damage(&boss, &ctx);
    for (int frame = 0; frame < SH_Q12(16) / delta; frame++) {
        playback_step(&boss, &ctx, &pose);
    }
    assert(sh_save_event_get(&save, 131) && boss.model.control_state == 6);
    assert(boss.model.animation_status == 27 && boss.mesh_variant == 2);
    assert(boss.model.keyframe == 131 || boss.model.keyframe == 132);
}

int main(void)
{
    damage_test();
    sound_test();
    control_test();
    playback_test(68);
    playback_test(136);
    playback_test(272);
    puts("PASS: boss intro gate, weakpoint/shotgun/Hard damage, death guard/flag/sound,1440 AI fixtures and real animation transitions at68/136/272 Q12 deltas");
    return 0;
}
