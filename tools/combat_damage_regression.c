#include "sh3ds/combat_damage.h"
#include "sh3ds/split_head.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int impacts, particles, sounds, motors, rumbles;
static void rumble(void *user, int kind) { (void)user; assert(kind == 6 || kind == 7); ++rumbles; }
static void motor(void *user, int value) { (void)user; assert(value == 128); ++motors; }
static void sound(void *user, int id, const ShPathPoint *p, int volume, int flags)
{ (void)user; assert(id == 1392 && p && volume == 128 && flags == 0); ++sounds; }
static void impact(void *user, const ShPathPoint *p, int weapon)
{ (void)user; assert(p && weapon >= 0 && weapon < 70); ++impacts; }
static void particle(void *user, ShCombatActor *target, const ShPathPoint *p, int kind, int count)
{ (void)user; assert(target && p && kind > 0 && kind < 8 && count > 0); ++particles; }

static ShCombatDamageContext context = {0, 0, NULL, rumble, motor, sound, impact, particle};

static ShCombatActor attacker(unsigned attack)
{
    ShCombatActor actor = {0};
    actor.is_player = true;
    actor.attack = (int32_t)attack;
    actor.current[0].vx = 3 * 4096;
    actor.current[0].vz = 4 * 4096;
    actor.current[1].vz = 4096;
    actor.attack_frame = sh_combat_attack_info(attack)->field_e;
    return actor;
}

static ShCombatActor target(void)
{
    ShCombatActor actor = {0};
    actor.chara_id = 14; /* Chara_SplitHead */
    actor.health = 25500;
    actor.npc_index = 2;
    return actor;
}

int main(void)
{
    ShPathPoint position = {0};
    int32_t mask = 0;
    for (unsigned attack = 0; attack < 70; ++attack) {
        for (unsigned seed = 0; seed < 128; ++seed) {
            ShCombatActor a = attacker(attack), t = target();
            sh_rng_set_seed(seed);
            assert(sh_combat_damage_apply(&a, &t, &position, 4096, &context, &mask));
            assert(mask == 4 && a.hit_mask == 4 && t.attack_received == (int)attack);
            assert(t.damage_amount >= 0);
        }
    }
    assert(!sh_combat_attack_info(70));
    ShCombatActor a = attacker(34), t = target(); /* Shotgun tap */
    sh_rng_set_seed(42);
    assert(sh_combat_damage_apply(&a, &t, &position, 4096, &context, &mask));
    int32_t first = t.damage_amount;
    int32_t base = sh_combat_attack_info(34)->field_4 * 4096;
    assert(first >= 7 * base / 8 && first <= 9 * base / 8);
    assert(t.damage_position.vx == 0 && t.damage_position.vy == 0 && t.damage_position.vz == 0);
    int old_impacts = impacts;
    sh_rng_set_seed(42);
    assert(sh_combat_damage_apply(&a, &t, &position, 4096, &context, &mask));
    assert(t.damage_amount == first * 2 && impacts == old_impacts);

    /* Feed actual weapon damage into the original boss weakpoint/difficulty rules. */
    ShSplitHead closed = {0}, open = {0};
    ShSavegame save = {0};
    ShSplitContext boss_ctx = {0};
    boss_ctx.save = &save;
    boss_ctx.weapon_attack = 34;
    closed.health = open.health = 25500;
    closed.damage_amount = open.damage_amount = first;
    open.props.flags = 1;
    sh_split_head_damage(&closed, &boss_ctx);
    sh_split_head_damage(&open, &boss_ctx);
    assert(25500 - open.health == (25500 - closed.health) * 32);

    a = attacker(1); t = target();
    assert(sh_combat_damage_apply(&a, &t, &position, 4096, &context, &mask));
    assert(t.damage_position.vx > 2400 && t.damage_position.vx < 2500);
    assert(t.damage_position.vz > 3200 && t.damage_position.vz < 3350);
    t = target(); t.chara_id = 44; a.hit_mask = 0;
    int old_sounds = sounds;
    assert(sh_combat_damage_apply(&a, &t, &position, 4096, &context, &mask));
    assert(sounds == old_sounds + 1);
    t = target(); t.is_player = true; a.npc_index = 7;
    assert(sh_combat_damage_apply(&a, &t, &position, 4096, &context, &mask));
    assert(mask == -1 && t.last_attacker == 7);
    t.is_player = false; t.npc_index = 32;
    assert(!sh_combat_damage_apply(&a, &t, &position, 4096, &context, &mask));
    puts("PASS: 8960 weapon fixtures, repeated shotgun hits, boss weakpoint, melee direction, padlock/player receipts");
    return 0;
}
