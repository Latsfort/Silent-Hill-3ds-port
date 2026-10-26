#include "sh3ds/split_head.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define SPLIT_ANIM_INDEX(status) ((status) >> 1)
#define SPLIT_FP_FROM(value, shift) ((value) >> (shift))
#define SPLIT_MULT(a, b) ((int32_t)((uint32_t)(a) * (uint32_t)(b)) >> 12)
#define SPLIT_TIMESTEP_ANGLE(denom, shift) (((ctx->delta / (denom)) >> (shift)) + 1)
#define SPLIT_RANDOM_ADD(base, low, high) ((base) + split_random((low), (high)))
#define SPLIT_MAX(a, b) ((a) > (b) ? (a) : (b))
#define SPLIT_MIN(a, b) ((a) < (b) ? (a) : (b))
#define SPLIT_SQUARE(a) ((int32_t)((uint32_t)(a) * (uint32_t)(a)))
#define SPLIT_Q12_TO_Q6(a) ((a) >> 6)
#define SPLIT_Q6_TO_Q12(a) ((a) * 64)
#define SPLIT_Q12_TO_Q8(a) ((a) >> 4)
#define SPLIT_TIME_RANGE(time, low, high) (((time) >> 12) >= (low) && ((time) >> 12) <= (high))
#define SPLIT_FRACT(value) ((value) & 4095)
#define SPLIT_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SPLIT_DIV4(a) ((a) >> 2)
#define SPLIT_DIV(a, b) ((int32_t)((uint32_t)(a) << 12) / (b))
#define SPLIT_RESTORE(a, shift) ((int32_t)((uint32_t)(a) << (shift)))
#define SPLIT_OVERFLOW_GUARD(a) ((uint32_t)((uint32_t)(a) + SHRT_MAX) >= USHRT_MAX ? 4 : 0)

/* Math_MatrixTransform / Math_RotMatrixZxyNegGte pure Y: libkmath.s. */
void sh_split_head_world_bone(const ShSplitHead *boss, const ShCharaBone *model, ShCharaBone *world)
{
    const int32_t sine = sh_fixed_sin(boss->rotation.vy), cosine = sh_fixed_cos(boss->rotation.vy);
    ShCharaBone result;
    for (int column = 0; column < 3; column++) {
        result.rotation[0][column] = (int32_t)(((int64_t)cosine * model->rotation[0][column] + (int64_t)sine * model->rotation[2][column]) >> 12);
        result.rotation[1][column] = model->rotation[1][column];
        result.rotation[2][column] = (int32_t)((-(int64_t)sine * model->rotation[0][column] + (int64_t)cosine * model->rotation[2][column]) >> 12);
    }
    result.translation[0] = (boss->position.vx >> 4) + (int32_t)(((int64_t)cosine * model->translation[0] + (int64_t)sine * model->translation[2]) >> 12);
    result.translation[1] = (boss->position.vy >> 4) + model->translation[1];
    result.translation[2] = (boss->position.vz >> 4) + (int32_t)((-(int64_t)sine * model->translation[0] + (int64_t)cosine * model->translation[2]) >> 12);
    *world = result;
}

/* sharedFunc_800D4408_1_s05: world bone MATRIX Q8/GTE point result to Q12. */
static void split_bone_point(const ShSplitContext *ctx, ShPathPoint *point, unsigned index, int x, int y, int z)
{
    const ShCharaBone *bone = &ctx->world_bones[index];
    int32_t output[3];
    const int16_t local[3] = {(int16_t)x, (int16_t)y, (int16_t)z};
    for (int row = 0; row < 3; row++) {
        const int64_t value = (int64_t)bone->rotation[row][0] * local[0] +
            (int64_t)bone->rotation[row][1] * local[1] + (int64_t)bone->rotation[row][2] * local[2];
        output[row] = (int32_t)((value >> 12) + bone->translation[row]) * 16;
    }
    *point = (ShPathPoint){output[0], output[1], output[2]};
}

static void split_pitch_sound(ShSplitContext *ctx, const ShPathPoint *position, int id, int volume, int pitch)
{
    if (ctx->sound_count < SPLIT_ARRAY_SIZE(ctx->sounds)) {
        ctx->sounds[ctx->sound_count++] = (ShSplitSound){id, 4, volume, 0, pitch, 0, *position};
    }
}

static void split_sound_at(ShSplitContext *ctx, const ShPathPoint *position, int id, int volume, int flags)
{
    if (ctx->sound_count < SPLIT_ARRAY_SIZE(ctx->sounds)) {
        ctx->sounds[ctx->sound_count++] = (ShSplitSound){id, 0, volume, 0, 0, flags, *position};
    }
}

/* maps/shared.h APPROACH and Chara_MoveSpeedUpdate3. */
static void split_speed(ShSplitHead *boss, const ShSplitContext *ctx, int32_t speed, int32_t target)
{
    const int32_t step = sh_q12_mul(ctx->delta, speed);
    if (boss->move_speed > target) {
        boss->move_speed -= step;
        if (boss->move_speed < target) { boss->move_speed = target; }
    } else {
        boss->move_speed += step;
        if (boss->move_speed > target) { boss->move_speed = target; }
    }
}

/* math.h Math_Vector2MagCalcSafeQ6 / Math_AngleBetweenPositionsGet. */
static int32_t split_distance(int32_t x, int32_t z)
{
    x >>= 6; z >>= 6;
    return (int32_t)(sh_fixed_sqrt0((uint32_t)(x * x + z * z)) << 6);
}
static int16_t split_angle_between(ShPathPoint from, ShPathPoint to)
{
    return (int16_t)sh_fixed_atan2(to.vx - from.vx, to.vz - from.vz);
}
static int16_t split_angle_signed(int32_t angle)
{
    angle &= 4095;
    return (int16_t)(angle > 2048 ? angle - 4096 : angle);
}

/* main/rng.h Rng_GenerateUInt. */
static int32_t split_random(int32_t low, int32_t high)
{
    return (int32_t)((uint32_t)sh_rng_rand16() % (uint32_t)(high - low + 1)) + low;
}

static void split_player_damage(ShSplitContext *ctx, uint8_t *dead, bool disabled)
{
    *dead = ctx->player_dead;
    ctx->player_damage_disabled = disabled;
}

static void split_sound(const ShSplitHead *boss, ShSplitContext *ctx, int id, int kind,
    int volume, int distance, int pitch, int flags)
{
    if (ctx->sound_count < sizeof(ctx->sounds) / sizeof(ctx->sounds[0])) {
        ctx->sounds[ctx->sound_count++] = (ShSplitSound){id, kind, volume, distance, pitch, flags, boss->position};
    }
}

#include "split_head_steps.h"

/* SplitHead_Update: source damage/control/movement/offsets -> animation -> contacts -> effects/sound. */
bool sh_split_head_update(ShSplitHead *boss, ShSplitContext *ctx)
{
    if (!boss || !ctx || !ctx->save || !ctx->world_bones || ctx->bone_count <= 23 ||
        !ctx->ray || !ctx->player_effect || !ctx->move || !ctx->pose || !ctx->shed || !ctx->strand) {
        return false;
    }
    ctx->sound_count = 0;
    ctx->player_push = ctx->player_push_velocity = (ShPathPoint){0};
    if (!boss->model.control_state) { sh_split_head_begin(boss); }
    if (ctx->delta) {
        sh_split_head_damage(boss, ctx);
        if (!sh_split_head_control(boss, ctx) || !sh_split_head_movement(boss, ctx)) { return false; }
        sh_split_head_offset_prepare(boss, ctx);
    }
    ShAnimPoseTiming pose;
    if (!sh_split_head_timing(boss, ctx, &pose) ||
        !ctx->pose(ctx->user, boss, &pose, &ctx->world_bones, &ctx->bone_count) ||
        !ctx->world_bones || ctx->bone_count <= 23) { return false; }
    if (ctx->delta && !sh_split_head_contact(boss, ctx)) { return false; }
    if (!sh_split_head_effects(boss, ctx)) { return false; }
    sh_split_head_sounds(boss, ctx);
    return true;
}
