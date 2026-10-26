#include "sh3ds/player_damage.h"
#include <assert.h>
#include <stdio.h>

static int sounds, feedbacks, grabs;
static int32_t last_damage;
static void sound(void *user, int id, const ShPathPoint *p, int volume, int flags)
{ (void)user; assert(id >= 1326 && id <= 1329 && p && volume == 32 && flags == 0); sounds++; }
static void motor(void *user, int a, int b) { (void)user; assert(a == 2 && b == 3); }
static void reset(void *user) { (void)user; }
static void grab(void *user) { (void)user; grabs++; }
static void feedback(void *user, int32_t amount) { (void)user; last_damage = amount; feedbacks++; }

static ShPlayerDamage player(int attack)
{
    ShPlayerDamage p = {0};
    p.actor.health = 100 * 4096;
    p.actor.attack_received = attack;
    p.actor.last_attacker = 0;
    p.actor.damage_amount = 10 * 4096;
    p.collision_radius = 1024;
    p.model_control = p.model_step = p.extra_control = p.extra_step = 5;
    p.upper_state = p.lower_state = 5;
    p.aiming = true;
    return p;
}

int main(void)
{
    ShSavegame save = {0}; save.map_idx = 8;
    int16_t yaw[] = {0};
    ShPlayerDamageContext ctx = {.save = &save, .previous_attack = -1,
        .npc_yaws = yaw, .npc_count = 1, .sound = sound, .motor = motor,
        .collision_reset = reset, .grab_feedback = grab, .damage_feedback = feedback};
    for (int difficulty = -1; difficulty <= 1; ++difficulty) {
        ctx.difficulty = difficulty;
        ShPlayerDamage p = player(40);
        assert(sh_player_receive_damage(&p, &ctx));
        int32_t amount = difficulty == -1 ? 30720 : difficulty == 1 ? 61440 : 40960;
        assert(p.actor.health == 100 * 4096 - amount && last_damage == amount);
        assert(p.actor.damage_amount == 0 && !ctx.dead);
        assert(p.state == 23 && !p.model_step && !p.model_control && !p.extra_step && !p.extra_control);
        assert(!p.upper_state && !p.lower_state && !p.aiming && p.actor.attack_state == -1);
    }
    ctx.difficulty = 0;
    save.map_idx = 0;
    ShPlayerDamage p = player(40);
    assert(sh_player_receive_damage(&p, &ctx));
    assert(p.actor.health == 80 * 4096 && last_damage == 10 * 4096);
    save.map_idx = 8;
    p = player(47); /* Original Split Head instant-death receipt. */
    int previous_feedback = feedbacks;
    assert(sh_player_receive_damage(&p, &ctx));
    assert(p.actor.health == -1 && p.state == 9 && p.collision_radius == 0);
    assert(feedbacks == previous_feedback && !ctx.dead);
    p = player(40); ctx.disable_damage = true;
    assert(sh_player_receive_damage(&p, &ctx));
    assert(p.actor.health == 100 * 4096 && !p.actor.damage_amount && p.state == 0);
    ctx.disable_damage = false;
    p = player(40); p.actor.health = 10 * 4096;
    assert(sh_player_receive_damage(&p, &ctx));
    assert(p.actor.health == 0 && p.state == 8 && !ctx.dead);
    p = player(40); p.actor.health = 1;
    assert(sh_player_receive_damage(&p, &ctx));
    assert(p.actor.health == -1 && p.state == 8 && ctx.dead);
    ctx.dead = false;
    p = player(40); ctx.walk_to_run = true;
    assert(sh_player_receive_damage(&p, &ctx));
    assert(ctx.previous_attack == 40 && p.actor.health == 100 * 4096);
    ctx.walk_to_run = false;
    p.actor.attack_received = -1;
    assert(sh_player_receive_damage(&p, &ctx));
    assert(ctx.previous_attack == -1 && p.actor.attack_received == 40 && p.actor.health == 90 * 4096);
    unsigned cases = 0;
    for (int state = 0; state < 53; ++state) {
        for (int attack = 0; attack < 70; ++attack) {
            ctx.dead = false; ctx.previous_attack = -1;
            p = player(attack); p.state = state;
            assert(sh_player_receive_damage(&p, &ctx));
            assert(p.actor.health <= 100 * 4096);
            cases++;
        }
    }
    assert(sounds > 0 && feedbacks > 0 && grabs > 0);
    printf("PASS: %u state/attack fixtures, difficulty, intro damage, boss instant death, zero-health/death and transition deferral\n", cases);
    return 0;
}
