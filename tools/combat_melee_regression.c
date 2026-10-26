#include "sh3ds/combat_melee.h"
#include "sh3ds/combat_attack.h"
#include "sh3ds/fixed.h"
#include <assert.h>
#include <stdio.h>

static int effects;
static void rumble(void *u, int v) { (void)u; (void)v; }
static void motor(void *u, int v) { (void)u; (void)v; }
static void sound(void *u, int id, const ShPathPoint *p, int v, int f)
{ (void)u; (void)id; (void)p; (void)v; (void)f; }
static void impact(void *u, const ShPathPoint *p, int w) { (void)u; (void)p; (void)w; }
static void particle(void *u, ShCombatActor *a, const ShPathPoint *p, int k, int n)
{ (void)u; (void)a; (void)p; assert(k > 0 && n > 0); effects++; }
static int melee(void *u, ShCombatActor *a, int32_t yaw, const ShAttackInfo *info, int32_t scale)
{
    int32_t mask;
    assert(sh_combat_melee_apply(a, yaw, info, scale, u, &mask));
    return mask;
}
static bool trace(void *u, const ShPathPoint *p, ShPathPoint *d, ShCombatActor *a, ShAttackTrace *t)
{ (void)u; (void)p; (void)d; (void)a; (void)t; assert(0); return false; }
static void feedback(void *u, int kind, bool hit, int32_t accuracy)
{ (void)u; (void)kind; (void)hit; (void)accuracy; }
static void word(uint8_t *p, int value) { p[0] = (uint8_t)value; p[1] = (uint8_t)((unsigned)value >> 8); }

int main(void)
{
    ShCollWorld world = {0};
    ShRayCharacter shape = {0};
    shape.collision.cylinder.field_2 = 1024;
    shape.collision.box.top = -8192;
    ShCombatActor player = {.is_player = true, .chara_id = 1, .health = 100 * 4096,
        .shape = &shape, .collision_state = 1};
    ShCombatActor target = {.npc_index = 3, .chara_id = 14, .health = 25500,
        .shape = &shape, .collision_state = 1, .position = {0, 0, 4096}};
    ShCombatDamageContext damage = {0, 0, NULL, rumble, motor, sound, impact, particle};
    ShMeleeContext ctx = {.player = &player, .world = &world, .collision_flags = 1, .damage = &damage};
    ctx.npcs[3] = &target;
    player.previous[0].vy = -4096;
    player.current[0].vz = 6553;
    int32_t mask;
    assert(sh_combat_melee_apply(&player, 0, sh_combat_attack_info(0), 4096, &ctx, &mask));
    assert(mask == 8 && player.hit_mask == 8 && target.damage_amount > 0 && effects);
    target.damage_amount = 0; target.position.vz = 4 * 4096;
    assert(sh_combat_melee_apply(&player, 0, sh_combat_attack_info(0), 4096, &ctx, &mask));
    assert(!mask && !target.damage_amount);
    target.position.vz = 4096; target.collision_state = 0;
    assert(sh_combat_melee_apply(&player, 0, sh_combat_attack_info(0), 4096, &ctx, &mask) && !mask);
    target.collision_state = 1; ctx.game_over = true;
    assert(sh_combat_melee_apply(&player, 0, sh_combat_attack_info(0), 4096, &ctx, &mask) && !mask);
    ctx.game_over = false;
    uint8_t prop[10] = {1}, ranges[8] = {0}, indices[1] = {0};
    word(prop + 2, 512); word(prop + 4, -768); word(prop + 6, 100); word(prop + 8, 32);
    word(ranges + 4, 1);
    world.count = 1;
    world.chunks[0] = (ShCollChunk){.pos_x_q8 = -512, .cell_q8 = 2048, .nx = 1, .nz = 1,
        .p18_n = 1, .ptr18 = prop, .ranges = ranges, .ptr28 = indices, .n28 = 1};
    assert(sh_combat_melee_apply(&player, 0, sh_combat_attack_info(0), 4096, &ctx, &mask) && !mask);
    world.count = 0;
    /* Full animation dispatcher uses the native melee backend at source keyframes. */
    player.attack_state = 1; player.angle_e = 1024;
    player.attack_origin.vy = -4096;
    ShAttackContext attack = {.delta = 136, .animation_time = 4 * 4096, .target_index = -1,
        .damage = &damage, .user = &ctx, .trace = trace, .melee = melee, .feedback = feedback};
    target.damage_amount = 0; target.position.vz = 1024;
    assert(sh_combat_attack_update(&player, &attack, &mask));
    assert(mask == 8 && target.damage_amount > 0);
    /* An NPC attack checks Harry and returns the original -1 receipt mask. */
    target.attack = 40; target.previous[0] = (ShPathPoint){0, -4096, 1024};
    target.current[0] = (ShPathPoint){0, 0, -6553};
    assert(sh_combat_melee_apply(&target, 0, sh_combat_attack_info(40), 4096, &ctx, &mask));
    assert(mask == -1 && player.damage_amount > 0 && player.last_attacker == 3);
    assert(sh_fixed_sqrt12(4096) == 4096 && sh_fixed_sqrt12(0) == 0);
    puts("PASS: melee animation -> contact -> retail damage, source slot masks, range/ignore/game-over/obstacle gates and NPC receipt");
    return 0;
}
