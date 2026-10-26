#include "sh3ds/combat_attack.h"
#include "sh3ds/coll.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    ShCollWorld world;
    ShRayCharacter shape;
    ShCombatActor target;
    int traces, feedbacks;
} Fixture;
static void rumble(void *u, int v) { (void)u; assert(v >= 5 && v <= 7); }
static void motor(void *u, int v) { (void)u; (void)v; }
static void sound(void *u, int id, const ShPathPoint *p, int v, int f)
{ (void)u; (void)id; (void)p; (void)v; (void)f; }
static void impact(void *u, const ShPathPoint *p, int w) { (void)u; (void)p; (void)w; }
static void particle(void *u, ShCombatActor *a, const ShPathPoint *p, int k, int n)
{ (void)u; (void)a; (void)p; (void)k; (void)n; }
static bool trace(void *u, const ShPathPoint *from, ShPathPoint *delta, ShCombatActor *excluded, ShAttackTrace *result)
{
    Fixture *f = u;
    assert(excluded->is_player && delta->vz > 0);
    f->traces++;
    ShRayCharacter *characters[] = {&f->shape};
    ShRayTrace hit;
    bool success = sh_combat_ray_query(&hit, &f->world, from, delta, characters, 1, false, -1);
    result->target = hit.target;
    result->character = hit.character ? &f->target : NULL;
    return success;
}
static int melee(void *u, ShCombatActor *a, int32_t yaw, const ShAttackInfo *info, int32_t scale)
{ (void)u; (void)a; (void)yaw; (void)info; (void)scale; assert(0); return 0; }
static void feedback(void *u, int kind, bool hit, int32_t accuracy)
{ Fixture *f = u; assert(kind >= 0 && kind <= 7 && hit && accuracy >= 0); f->feedbacks++; }

int main(void)
{
    for (int weapon = 32; weapon <= 35; ++weapon) {
        for (int delta = 68; delta <= 136; delta += 68) {
          for (int variant = 0; variant < 4; ++variant) {
            Fixture f = {0};
            f.target.chara_id = 14;
            f.target.health = 25500;
            f.target.position.vz = 6 * 4096;
            f.shape.position = f.target.position;
            f.shape.collision.cylinder.field_2 = 3 * 4096;
            f.shape.collision.box.top = -4 * 4096;
            f.shape.collision.box.height = 2 * 4096;
            ShCombatDamageContext damage = {0, 0, NULL, rumble, motor, sound, impact, particle};
            ShAttackContext ctx = {.delta = delta, .player_accuracy = 4096, .target_index = -1,
                .damage = &damage, .user = &f, .trace = trace, .melee = melee, .feedback = feedback};
            ShCombatActor *targets[] = {&f.target};
            if (variant) {
                ctx.npcs = targets; ctx.npc_count = 1; ctx.target_index = 0;
                ctx.dark = variant == 2;
                ctx.player_accuracy = variant == 3 ? 8192 : 2048;
            }
            ShCombatActor actor = {0};
            actor.is_player = true; actor.chara_id = 1;
            actor.attack = weapon; actor.attack_state = 1; actor.shot_budget = 1;
            actor.attack_origin.vy = -4096;
            actor.angle_e = 1024;
            sh_rng_set_seed(42);
            int32_t mask;
            assert(sh_combat_attack_update(&actor, &ctx, &mask));
            assert(mask == 1 && actor.attack_state == -1 && !actor.shot_budget);
            assert(f.traces == sh_combat_attack_info(weapon)->chara_id);
            assert(f.feedbacks == 1 && f.target.damage_amount > 0);
            int hits = f.traces;
            assert(sh_combat_attack_update(&actor, &ctx, &mask));
            assert(mask == 0 && actor.hit_mask == 0 && f.traces == hits);
            actor.attack_state = 0; actor.shot_count = 8; actor.attack_frame = 100;
            assert(sh_combat_attack_update(&actor, &ctx, &mask));
            assert(actor.shot_count == 0 && actor.attack_frame == 0);
          }
        }
    }
    assert(sh_combat_attack_info(32)->field_0 == 12 * 4096);
    assert(sh_combat_attack_info(33)->field_0 == 15 * 4096);
    puts("PASS: animation-triggered firearms -> real ray -> retail damage, pellet count, single-fire/reset at both deltas");
    return 0;
}
