#include "sh3ds/school_boss_particles.h"
#include "sh3ds/school_boss_world.h"
#include "sh3ds/school_boss_scene.h"
#include "sh3ds/rng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { ShSchoolBossParticles pool; unsigned emits, states[13], draws; uint32_t colors[500]; } Fixture;
static void init(void *u, ShSchoolBossParticleConfig *config)
{ Fixture *f = u; sh_school_boss_particles_init(&f->pool, config); }
static void object(void *u, const ShSchoolBossObject *p) { (void)u; (void)p; }
static void sound(void *u, int id) { (void)u; (void)id; }
static void falloff(void *u, int id, const ShPathPoint *p, int v, int32_t d, int pitch)
{ (void)u; (void)id; (void)p; (void)v; (void)d; (void)pitch; }
static void emit(void *u, unsigned index, const ShPathPoint *p)
{
    Fixture *f = u;
    unsigned state = f->pool.slots[index].field_A;
    assert(index < 500 && state >= 8 && state <= 11);
    assert(p->vy == f->pool.slots[index].vy_8);
    f->states[state]++; f->emits++;
}
static void emit_copy(void *u, unsigned index, const ShPathPoint *p)
{ (void)u; assert(index < 500 && p); }
static void control(void *u, bool frozen) { (void)u; (void)frozen; }
static void border(void *u) { (void)u; }
static void camera(void *u, bool look, const ShPathPoint *p) { (void)u; (void)look; (void)p; }
static void camera_return(void *u, bool immediate) { (void)u; (void)immediate; }
static bool fade(void *u, int command, bool out, int mode, int32_t speed, bool substep)
{ (void)u; (void)out; (void)mode; (void)speed; (void)substep; return command == 1; }
static void noop(void *u) { (void)u; }
static void attributes(void *u, int id, int p, int v, int pitch)
{ (void)u; (void)id; (void)p; (void)v; (void)pitch; }
static bool project(void *u, const ShPathPoint *p, int16_t *x, int16_t *y, int32_t *depth)
{ (void)u; (void)p; *x = 0; *y = 0; *depth = 1024; return true; }
static void draw(void *u, unsigned i, const ShBossParticleQuad *quad)
{
    Fixture *f = u;
    assert(i < 500 && quad->color == f->colors[i] && quad->depth_bucket == 128);
    f->draws++;
}

int main(void)
{
    for (int delta = 68; delta <= 136; delta += 68) {
        Fixture f = {0};
        ShSchoolBossWorld world;
        ShSavegame save = {0};
        ShSchoolBossScene scene;
        sh_school_boss_scene_begin(&scene);
        ShSchoolBossWorldContext world_ctx = {.save = &save, .delta = delta, .light = &scene.light,
            .user = &f, .particle_init = init, .object = object, .sound = sound, .falloff = falloff};
        sh_rng_set_seed(12345);
        assert(sh_school_boss_world_begin(&world, &world_ctx));
        assert(world.particles.field_16 == -9420 && world.particles.field_10 == 4096);
        for (unsigned i = 0; i < 500; ++i) { assert(f.pool.slots[i].field_A == (i < 200 ? 0 : 12)); }
        assert(f.pool.effect_flags == 1 && sh_rng_get_seed() == 12345);
        ShSchoolBossSceneContext ctx = {.save = &save, .delta = delta, .raw_delta = delta,
            .particles = &world.particles, .particle_states = &f.pool.slots[0].field_A,
            .particle_count = 500, .particle_stride = sizeof(f.pool.slots[0]),
            .user = &f, .control = control, .border_show = border, .camera = camera, .camera_return = camera_return,
            .fade = fade, .echo = noop, .gameplay = noop, .autosave = noop, .sound = sound, .sound_attributes = attributes};
        unsigned frames = 0;
        while (!scene.finished && frames++ < 4000) {
            assert(sh_school_boss_intro(&scene, &ctx));
            assert(sh_school_boss_particles_step(&f.pool, &world.particles, delta, &f, emit));
        }
        assert(scene.finished && frames < 4000 && sh_save_event_get(&save, 130));
        assert(f.emits > 0 && world.particles.field_10 < 4096 && f.states[8] > 0);
        /* Skip acts directly on the interleaved20-byte shared particle records. */
        sh_school_boss_scene_begin(&scene); scene.step = 3; ctx.skip = true;
        assert(sh_school_boss_intro(&scene, &ctx));
        assert(scene.finished && !world.particles.field_1 && !world.particles.field_10);
        for (unsigned i = 0; i < 500; ++i) { assert(f.pool.slots[i].field_A == (i < 200 ? 0 : 12)); }
        assert(sh_school_boss_particles_step(&f.pool, &world.particles, delta, &f, emit));
        assert(f.pool.slots[499].field_A >= 8 && f.pool.slots[499].field_A <= 11);
        ShSchoolBossParticles copy = f.pool;
        ShSchoolBossParticleConfig config_copy = world.particles;
        for (unsigned i = 0; i < 500; ++i) {
            if (f.pool.slots[i].field_A >= 8 && f.pool.slots[i].field_A <= 11) {
                ShBossParticleQuad quad;
                sh_school_boss_particle_style(&f.pool.slots[i], &quad);
                f.colors[i] = quad.color;
            }
        }
        uint32_t seed_before = sh_rng_get_seed();
        assert(sh_school_boss_particles_step(&copy, &config_copy, delta, &f, emit_copy));
        uint32_t seed_after = sh_rng_get_seed();
        sh_rng_set_seed(seed_before);
        ShBossParticleDrawContext drawing = {.user = &f, .projection_scale = 320, .project = project, .draw = draw};
        assert(sh_school_boss_particles_draw_step(&f.pool, &world.particles, delta, &drawing));
        assert(f.draws > 0 && sh_rng_get_seed() == seed_after);
        assert(!memcmp(&f.pool, &copy, sizeof(copy)) && !memcmp(&world.particles, &config_copy, sizeof(config_copy)));
        sh_school_boss_scene_begin(&scene); scene.light = SH_Q12(1.3843f); ctx.skip = false;
        frames = 0;
        while (!scene.finished && frames++ < 4000) {
            assert(sh_school_boss_outro(&scene, &ctx));
            assert(sh_school_boss_particles_step(&f.pool, &world.particles, delta, &f, emit));
        }
        assert(scene.finished && frames < 4000 && sh_save_event_get(&save, 132));
        assert(world.particles.field_1 == 1 && world.particles.field_2 == 3 && world.particles.field_10 > 0);
    }
    puts("PASS: real300-of500 initialization, shared intro/skip/outro particle state, original spawn/lifetime and strided scene updates at both deltas");
    return 0;
}
