#include "sh3ds/split_head.h"
#include "sh3ds/split_head_world.h"
#include "sh3ds/fixed.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *load(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    *length = (size_t)ftell(file);
    rewind(file);
    uint8_t *data = malloc(*length);
    assert(data && fread(data, 1, *length, file) == *length);
    fclose(file);
    return data;
}

static unsigned moves;
static void movement(void *user, ShSplitHead *boss, int32_t x, int32_t z)
{
    (void)user;
    assert(x == SH_Q12(0.25f) && z == SH_Q12(-0.5f));
    assert(boss->heading_angle == boss->rotation.vy && boss->collision_state == 4);
    const bool bite = boss->model.animation_status == 5;
    assert(boss->hit_shape.cylinder_radius == (bite ? SH_Q12(1.3f) : SH_Q12(1.7f)));
    moves++;
}

int main(void)
{
    size_t model_size, animation_size, tim_size;
    uint8_t *model = load("romfs/game/chara/FAT.ILM", &model_size);
    uint8_t *animation = load("romfs/game/anim/FAT.ANM", &animation_size);
    uint8_t *image = load("romfs/game/chara/FAT.TIM", &tim_size);
    ShTim tim;
    assert(sh_tim_parse(image, tim_size, &tim));
    ShCharaClutMap cluts;
    sh_chara_clut_map_init(&cluts, true);
    ShMapVertex *vertices = calloc(32768, sizeof(*vertices));
    assert(vertices);
    unsigned weak = 0, bites = 0, pushes = 0, cases = 0, ray_hits = 0;
    ShCharaBone bones[32], translated[32];
    const ShPathPoint shift = {SH_Q12(20), 0, SH_Q12(-13)};
    for (int frame = 0; frame < 202; frame++) {
        size_t count;
        assert(sh_chara_bake_split_head(vertices, 32768, &count, model, model_size, animation, animation_size,
            &tim, frame, &cluts, 2, (frame % 3 - 1) * 192));
        unsigned bone_count = 0;
        while (bone_count < 32 && sh_chara_last_bone(bone_count, &bones[bone_count])) {
            translated[bone_count] = bones[bone_count];
            translated[bone_count].translation[0] += shift.vx >> 4;
            translated[bone_count].translation[2] += shift.vz >> 4;
            bone_count++;
        }
        assert(bone_count > 23);
        ShSplitHead moving = {0};
        sh_split_head_begin(&moving);
        moving.model.animation_status = frame >= 14 && frame <= 40 ? 5 : 23;
        moving.movement_offset[0] = SH_Q12(0.25f);
        moving.movement_offset[1] = SH_Q12(-0.5f);
        ShSplitContext motion = {.world_bones = bones, .bone_count = bone_count, .move = movement};
        assert(sh_split_head_movement(&moving, &motion));
        assert(!moving.movement_offset[0] && !moving.movement_offset[1]);
        for (int x = -12; x <= 12; x++) {
            for (int z = -12; z <= 12; z++) {
                ShSplitHead boss = {0};
                sh_split_head_begin(&boss);
                boss.model.time = SH_Q12(frame);
                ShSplitContext ctx = {.world_bones = bones, .bone_count = bone_count,
                    .player = {x * 1024, 0, z * 1024}, .player_radius = SH_Q12(0.25f), .player_attack = -1};
                sh_split_head_offset_prepare(&boss, &ctx);
                assert(sh_split_head_contact(&boss, &ctx));
                if (x == 0 && z == 0 && boss.hit_shape.radius >= 32 &&
                    boss.hit_shape.height - boss.hit_shape.top >= 32) {
                    ShRayCharacter shape = {0};
                    sh_split_head_ray_shape(&boss, &shape);
                    int32_t reach = boss.hit_shape.radius + 4096;
                    ShPathPoint origin = {
                        boss.position.vx + boss.hit_shape.offset.vx,
                        boss.position.vy + (boss.hit_shape.top + boss.hit_shape.height) / 2,
                        boss.position.vz + boss.hit_shape.offset.vz - reach};
                    ShPathPoint delta = {0, 0, reach * 2};
                    ShRayState state = {0};
                    ShRayTrace trace = {0};
                    assert(sh_combat_ray_setup(&state, false, 0, &origin, &delta, 0, 0, NULL, 0, 0));
                    sh_combat_ray_character(&state, &shape);
                    sh_combat_ray_finish(&trace, &state, &origin, &delta);
                    assert(trace.hasHit && trace.character == &shape);
                    ray_hits++;
                }
                weak += (boss.props.flags & 1) != 0;
                pushes += ctx.player_push.vx != 0 || ctx.player_push.vz != 0;
                if (ctx.player_attack >= 0) {
                    bites++;
                    assert(frame >= 31 && frame < 37 && (boss.props.flags & 512));
                    assert(ctx.sound_count == 1 && ctx.sounds[0].id == 1473 && ctx.sounds[0].volume == 255);
                    ctx.player_attack = -1; ctx.sound_count = 0;
                    assert(sh_split_head_contact(&boss, &ctx));
                    assert(ctx.player_attack == -1 && !ctx.sound_count);
                }
                ShSplitHead moved = {0};
                sh_split_head_begin(&moved);
                moved.position = shift;
                moved.model.time = SH_Q12(frame);
                ShSplitContext other = {.world_bones = translated, .bone_count = bone_count,
                    .player = {ctx.player.vx + shift.vx, 0, ctx.player.vz + shift.vz},
                    .player_radius = ctx.player_radius, .player_attack = -1};
                /* First prepare uses the previous frame's contour; seed identical valid contours. */
                assert(sh_split_head_contact(&moved, &other));
                assert(moved.hit_shape.radius == boss.hit_shape.radius);
                assert(moved.hit_shape.top == boss.hit_shape.top && moved.hit_shape.height == boss.hit_shape.height);
                assert(moved.hit_shape.offset.vx == boss.hit_shape.offset.vx && moved.hit_shape.offset.vz == boss.hit_shape.offset.vz);
                assert((moved.props.flags & 1) == (boss.props.flags & 1));
                cases++;
            }
        }
    }
    assert(ray_hits > 0);
    printf("PASS: %u real-pose boss ray intersections\n", ray_hits);
    assert(weak && bites && pushes && moves == 202 && cases == 202 * 25 * 25);
    printf("PASS: %u real-pose contact fixtures, weakpoint/mouth timing, single bite receipt, pushes and translated hit shapes\n", cases);
    free(vertices); free(model); free(animation); free(image);
    return 0;
}
