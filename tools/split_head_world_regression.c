#include "sh3ds/split_head_world.h"
#include "sh3ds/mapdata.h"
#include "../source/game/mapdata_map1_s05.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct World { ShCollWorld collision; unsigned requests; ShPathPoint last; } World;

static bool collision(void *user, const ShSplitHead *boss, const ShPathPoint *offset, ShSplitMoveResult *result)
{
    World *world = user;
    world->requests++;
    world->last = *offset;
    return sh_split_head_map_collision(&world->collision, boss, offset, result);
}

static uint8_t *add(World *world, const char *path)
{
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    const size_t size = (size_t)ftell(file);
    rewind(file);
    uint8_t *bytes = malloc(size);
    assert(bytes && fread(bytes, 1, size, file) == size);
    fclose(file);
    assert(sh_coll_world_add(&world->collision, bytes, size));
    return bytes;
}

int main(void)
{
    World world = {0};
    sh_coll_world_reset(&world.collision);
    uint8_t *first = add(&world, "romfs/game/bg/SU0000.IPD");
    uint8_t *second = add(&world, "romfs/game/bg/SU00FF.IPD");
    assert(MAP_SPAWN_COUNT_MAP1_S05 == 1 && s_map_spawns_map1_s05[0].chara_id == 14);
    ShSplitHead boss = {0};
    sh_split_head_begin(&boss);
    boss.position.vx = SH_Q12(s_map_spawns_map1_s05[0].x);
    boss.position.vz = SH_Q12(s_map_spawns_map1_s05[0].z);
    boss.rotation.vy = (int16_t)(s_map_spawns_map1_s05[0].yaw_deg * (4096.0f / 360.0f));
    boss.heading_angle = boss.rotation.vy;
    boss.hit_shape.cylinder_radius = SH_Q12(1.7f);
    ShSplitContext ctx = {.delta = 136, .collide = collision, .user = &world};
    const int32_t start_z = boss.position.vz;
    boss.move_speed = SH_Q12(1.2f);
    for (int frame = 0; frame < 30; frame++) { assert(sh_split_head_position_step(&boss, &ctx, 0, 0)); }
    assert(world.requests == 30 && boss.position.vz < start_z && world.last.vz < 0);
    ShCollHit floor;
    assert(sh_coll_ground_near(&world.collision, SH_Q12_TO_FLOAT(boss.position.vx), -SH_Q12_TO_FLOAT(boss.position.vz), 0, &floor));
    const int32_t ground = -SH_Q12(floor.height);
    boss.position.vy = ground - SH_Q12(1);
    boss.move_speed = 0;
    boss.fall_speed = SH_Q12(2);
    for (int frame = 0; frame < 60; frame++) { assert(sh_split_head_position_step(&boss, &ctx, 0, 0)); }
    assert(boss.position.vy == ground && !boss.fall_speed);
    ShCharaBone model = {.rotation = {{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, .translation = {256, -128, 512}};
    ShCharaBone posed;
    boss.rotation.vy = 1024;
    sh_split_head_world_bone(&boss, &model, &posed);
    assert(posed.translation[0] == (boss.position.vx >> 4) + 512);
    assert(posed.translation[1] == (boss.position.vy >> 4) - 128);
    assert(posed.translation[2] == (boss.position.vz >> 4) - 256);
    assert(posed.rotation[0][2] == 4096 && posed.rotation[2][0] == -4096);
    puts("PASS: boss retail spawn, real SU map floor/movement and PS1 world-root bone transform");
    free(first); free(second);
    return 0;
}
