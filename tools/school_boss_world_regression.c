#include "sh3ds/school_boss_world.h"
#include "sh3ds/map.h"
#include "sh3ds/rng.h"
#include "sh3ds/school_boss_particles.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int init, objects, sound, volume; ShSchoolBossParticles pool; } Fixture;
static void particle_init(void *u, ShSchoolBossParticleConfig *p)
{
    Fixture *f = u; f->init++;
    assert(p->flags == 9 && p->field_C == 300 && p->field_12 == -11468);
    assert(p->field_20 == SH_Q12(19.7f) && p->field_24 == SH_Q12(-19.1f));
    sh_school_boss_particles_init(&f->pool, p);
}
static void object(void *u, const ShSchoolBossObject *p)
{ Fixture *f = u; assert(!strcmp(p->model, "SPHERE_H") || !strcmp(p->model, "WHEEL_HI")); f->objects++; }
static void sound(void *u, int id) { Fixture *f = u; assert(id == 1478); f->sound++; }
static void falloff(void *u, int id, const ShPathPoint *p, int volume, int32_t distance, int pitch)
{
    Fixture *f = u;
    assert(id == 1478 && p->vx == SH_Q12(19.7f) && p->vy == SH_Q12(-1.5f) && p->vz == SH_Q12(-19.1f));
    assert(distance == 16 * 4096 && pitch == 0);
    f->volume = volume;
}
static uint8_t *load(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    *size = (size_t)ftell(file);
    rewind(file);
    uint8_t *data = malloc(*size);
    assert(data && fread(data, 1, *size, file) == *size);
    fclose(file);
    return data;
}

int main(void)
{
    ShSchoolBossWorld world;
    ShSavegame save = {0};
    int32_t light = 123;
    Fixture f = {0};
    ShSchoolBossWorldContext ctx = {.save = &save, .light = &light, .user = &f,
        .particle_init = particle_init, .object = object, .sound = sound, .falloff = falloff};
    assert(sh_school_boss_world_begin(&world, &ctx));
    assert(f.init == 1 && f.sound == 1 && light == 123 && world.particles.field_1 == 2);
    assert(!world.light_bone && !world.flare_bone && world.light_rotation.vx == -1024);
    assert(world.objects[0].position.vy == SH_Q12(1.58f));
    assert(world.objects[1].position.vy == SH_Q12(-0.06f));
    size_t first_size, second_size, glb_size;
    uint8_t *first = load("romfs/game/bg/SU0000.IPD", &first_size);
    uint8_t *second = load("romfs/game/bg/SU00FF.IPD", &second_size);
    uint8_t *glb = load("romfs/game/bg/RSU_GLB.PLM", &glb_size);
    ShMapVertex *vertices = calloc(65536, sizeof(*vertices));
    assert(vertices);
    for (int i = 0; i < 2; ++i) {
        ShMapBuilder builder;
        sh_map_builder_init(&builder, vertices, NULL, 65536, NULL, NULL);
        const ShSchoolBossObject *p = &world.objects[i];
        float position[] = {p->position.vx / 4096.0f, p->position.vy / 4096.0f, p->position.vz / 4096.0f};
        const float rotation[] = {0, 0, 0};
        bool found = sh_map_add_named_model(&builder, first, first_size, glb, glb_size, p->model, position, rotation);
        if (!found) { found = sh_map_add_named_model(&builder, second, second_size, glb, glb_size, p->model, position, rotation); }
        assert(found && builder.count > 0 && !builder.overflow);
    }
    for (int delta = 68; delta <= 136; delta += 68) {
        ctx.delta = delta;
        sh_save_event_clear(&save, 129); sh_save_event_set(&save, 130);
        assert(sh_school_boss_world_begin(&world, &ctx));
        assert(light == SH_Q12(1.3843f) && !world.particles.field_1);
        sh_rng_set_seed(1234);
        uint32_t seed = sh_rng_get_seed();
        assert(sh_school_boss_world_step(&world, &ctx));
        assert(light == SH_Q12(1.3843f) && sh_rng_get_seed() == seed);
        sh_save_event_set(&save, 129);
        int16_t angle0 = world.objects[0].rotation.vy, angle1 = world.objects[1].rotation.vy;
        for (int frame = 0; frame < 1000; ++frame) {
            world.particles.field_10 = (int16_t)(frame % 4097);
            assert(sh_school_boss_world_step(&world, &ctx));
            angle0 = (int16_t)(angle0 + sh_q12_mul(delta, -1024));
            angle1 = (int16_t)(angle1 + sh_q12_mul(delta, 170));
            assert(world.objects[0].rotation.vy == angle0 && world.objects[1].rotation.vy == angle1);
            assert(light > SH_Q12(1) && light < SH_Q12(2));
            assert(world.light_rate[0] >= 32 && world.light_rate[0] <= 95);
            assert(world.light_rate[1] >= 32 && world.light_rate[1] <= 95);
            assert(f.volume == 128 - sh_q12_mul(world.particles.field_10, 128));
        }
        assert(sh_rng_get_seed() != seed);
    }
    free(vertices); free(first); free(second); free(glb);
    puts("PASS: real sphere/wheel models, source placement, resume130, 2000 light/rotation/sound frames and conditional RNG");
    return 0;
}
