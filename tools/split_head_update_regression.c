#define main npc_asset_checks
#include "npc_pose_regression.c"
#undef main
#include "sh3ds/split_head_world.h"
#include "sh3ds/school_boss_particles.h"
#include "sh3ds/mapdata.h"
#include "../source/game/mapdata_map1_s05.h"
#include <stdlib.h>

typedef struct {
    ShSplitContext *ctx;
    ShCollWorld world;
    ShCharaBone bones[SH_CHARA_MAX_BONES];
    ShSchoolBossParticles pool;
    unsigned poses, moves, rays, strands, sheds;
} FrameFixture;

static bool frame_pose(void *u, const ShSplitHead *boss, const ShAnimPoseTiming *pose,
                         const ShCharaBone **bones, unsigned *count)
{
    FrameFixture *f = u;
    const ShCharaBone *model;
    assert(sh_npc_pose_split_head_blend(0, pose, boss->mesh_variant, boss->props.field_ee, &model, count));
    for (unsigned i = 0; i < *count; ++i) { sh_split_head_world_bone(boss, &model[i], &f->bones[i]); }
    *bones = f->bones; f->poses++;
    return true;
}
static bool frame_collide(void *u, const ShSplitHead *boss, const ShPathPoint *delta, ShSplitMoveResult *result)
{ FrameFixture *f = u; return sh_split_head_map_collision(&f->world, boss, delta, result); }
static void frame_move(void *u, ShSplitHead *boss, int32_t x, int32_t z)
{ FrameFixture *f = u; assert(sh_split_head_position_step(boss, f->ctx, x, z)); f->moves++; }
static ShSplitRay frame_ray(void *u, const ShPathPoint *from, const ShPathPoint *delta)
{
    FrameFixture *f = u;
    ShRayTrace trace;
    bool hit = sh_combat_ray_query(&trace, &f->world, from, delta, NULL, 0, true, -1);
    f->rays++;
    return (ShSplitRay){hit, false};
}
static void frame_effect(void *u, const ShPathPoint *p, int type, int count)
{ (void)u; assert(p && type > 0 && count > 0); }
static void frame_shed(void *u, int32_t x, int32_t y, int32_t z, int32_t speed, int32_t heading)
{ FrameFixture *f = u; sh_school_boss_shed(&f->pool, x, y, z, speed, heading); f->sheds++; }
static void frame_strand(void *u, int32_t variant, int32_t shade, int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1)
{ FrameFixture *f = u; sh_school_boss_strand(&f->pool, variant, shade, x0, y0, z0, x1, y1, z1); f->strands++; }
static uint8_t *frame_map(FrameFixture *f, const char *path)
{
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    size_t size = (size_t)ftell(file);
    rewind(file);
    uint8_t *data = malloc(size);
    assert(data && fread(data, 1, size, file) == size);
    fclose(file);
    assert(sh_coll_world_add(&f->world, data, size));
    return data;
}

int main(void)
{
    assert(npc_asset_checks() == 0);
    FrameFixture f = {0};
    uint8_t *a = frame_map(&f, "romfs/game/bg/SU0000.IPD");
    uint8_t *b = frame_map(&f, "romfs/game/bg/SU00FF.IPD");
    ShSavegame save = {0};
    ShSplitHead boss = {0};
    sh_split_head_begin(&boss);
    boss.position.vx = SH_Q12(s_map_spawns_map1_s05[0].x);
    boss.position.vz = SH_Q12(s_map_spawns_map1_s05[0].z);
    boss.rotation.vy = 2048; boss.heading_angle = boss.rotation.vy;
    ShSplitContext ctx = {.save = &save, .delta = 136, .player = {SH_Q12(20), 0, SH_Q12(-20)},
        .player_radius = SH_Q12(0.25f), .player_attack = -1, .ray = frame_ray, .player_effect = frame_effect,
        .move = frame_move, .collide = frame_collide, .pose = frame_pose, .shed = frame_shed, .strand = frame_strand, .user = &f};
    f.ctx = &ctx;
    boss.mesh_variant = 1;
    ShAnimPoseTiming initial = {162, 162, 0};
    assert(frame_pose(&f, &boss, &initial, &ctx.world_bones, &ctx.bone_count));
    assert(sh_split_head_update(&boss, &ctx) && boss.model.control_state == 8);
    assert(f.moves == 1 && f.poses == 2);
    unsigned moves = f.moves, poses = f.poses;
    ctx.delta = 0;
    assert(sh_split_head_update(&boss, &ctx));
    assert(f.moves == moves && f.poses == poses + 1);
    ctx.delta = 136;
    sh_save_event_set(&save, 130);
    assert(sh_split_head_update(&boss, &ctx) && boss.model.control_state == 2);
    for (unsigned i = 0; i < 240; ++i) { assert(sh_split_head_update(&boss, &ctx)); }
    assert(f.rays > 0 && f.sheds > 0);
    ctx.delta = 0;
    boss.model.animation_status = 5; boss.model.keyframe = 32; boss.model.time = 32 * 4096;
    unsigned strands_before = f.strands;
    assert(sh_split_head_update(&boss, &ctx) && f.strands == strands_before + 6);
    ctx.delta = 136;
    /* A lethal damage fixture enters the original death control and ends through flag131. */
    boss.health = 1; boss.props.flags |= 16; boss.damage_amount = 4096;
    boss.move_speed = 0; boss.model.animation_status = 21; boss.model.keyframe = 162; boss.model.time = 162 * 4096;
    for (unsigned i = 0; i < 1200 && !sh_save_event_get(&save, 131); ++i) { assert(sh_split_head_update(&boss, &ctx)); }
    assert(boss.health == 0 && sh_save_event_get(&save, 131) && f.strands > 0);
    free(a); free(b);
    puts("PASS: full boss update order with real FAT poses/SU map movement/rays/shared-pool emissions, paused animation and original death flag");
    return 0;
}
