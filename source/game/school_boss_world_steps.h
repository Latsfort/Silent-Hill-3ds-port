/* Generated from complete MAP1_S05 Map_WorldObjectsInit/Update; do not edit. */
static void boss_world_begin(ShSchoolBossWorld *world, ShSchoolBossWorldContext *ctx)
{

    world->light_time[0] = 0;
    world->light_time[1] = 0;
    world->light_rate[0] = 0x40;
    world->light_rate[1] = 0x40;
    world->particles.flags = (1 << 3) | (1 << 0);
    world->particles.field_C = 0x12C;
    world->particles.field_12 = (int16_t)(-0x2CCCu);
    world->particles.field_14 = -0x800;
    world->particles.field_1C = 0xA00;
    world->particles.field_E = -0x1000;
    world->particles.field_4 = 0x80;
    world->particles.field_5 = 0xC4;
    world->particles.field_6 = 0x3C;
    world->particles.field_8 = 0x200;
    world->particles.field_A = 0x400;
    world->particles.field_1 = 2;
    world->particles.field_2 = 8;
    world->particles.field_3 = 0x40;
    world->particles.field_20 = 80691;
    world->particles.field_24 = -78233;
    world->light_amplitude = 48;

    if (sh_save_event_get(ctx->save, 130))
    {
        *ctx->light = 5670;
        world->particles.field_1 = 0;
    }

    ctx->particle_init(ctx->user, &world->particles);
    boss_world_object_init(&world->objects[0], "SPHERE_H", 80691, 6471, -78233, 0, 0, 0);

    boss_world_object_init(&world->objects[1], "WHEEL_HI", 80691, -245, -78233, 0, 0, 0);

    world->light_bone = NULL;
    world->light_position.vx = 80691;
    world->light_position.vy = -12288;
    world->light_position.vz = -78233;

    world->flare_bone = NULL;
    world->light_rotation.vx = -1024;
    world->light_rotation.vy = 0;
    world->light_rotation.vz = 0;

    ctx->sound(ctx->user, 1478);

}
static void boss_world_update(ShSchoolBossWorld *world, ShSchoolBossWorldContext *ctx)
{

    int32_t sp18[2];

    if (sh_save_event_get(ctx->save, 129))
    {
        world->light_time[0] += ctx->delta;
        if ((world->light_time[0] * world->light_rate[0]) >= 262144)
        {
            world->light_time[0] = 0;
            world->light_rate[0] = boss_world_random(32, 95);
            world->light_amplitude    = boss_world_random(32, 63);
        }

        sp18[0] = BOSS_WORLD_MULT(world->light_amplitude, sh_fixed_sin(world->light_time[1]));

        world->light_time[1] += ctx->delta;

        sp18[1] = (world->light_time[1] * world->light_rate[1]) >> 6;

        if (sp18[1] >= 4096)
        {
            world->light_time[1] = 0;
            world->light_rate[1] = boss_world_random(32, 95);
            sp18[1]       = BOSS_WORLD_FRACT(sp18[1]);
        }

        
        *ctx->light = 5529 + (((3686 + sh_q12_mul(sp18[0], 2457)) * sh_fixed_sin(sp18[1])) >> 15);
    }

    world->objects[0].rotation.vy += sh_q12_mul(ctx->delta, -1024);
    ctx->object(ctx->user, &world->objects[0]);

    world->objects[1].rotation.vy += sh_q12_mul(ctx->delta, 170);
    ctx->object(ctx->user, &world->objects[1]);

    ctx->falloff(ctx->user, 1478, &(ShPathPoint){80691, -6144, -78233}, 128 - sh_q12_mul(world->particles.field_10, 128), 65536, 0);

}
