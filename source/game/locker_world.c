#include "sh3ds/locker_world.h"
#include "sh3ds/fixed.h"
#include "sh3ds/rng.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int32_t locker_random(int32_t low, int32_t high)
{
    return low + (uint32_t)sh_rng_rand16() % (uint32_t)(high - low + 1);
}

static int32_t locker_min(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t locker_max(int32_t a, int32_t b) { return a > b ? a : b; }

static void locker_sound(ShLockerFrame *frame, int id, ShPathPoint position, int volume, int flags)
{
    if (frame->command_count >= SH_LOCKER_COMMAND_CAPACITY) { return; }
    frame->commands[frame->command_count++] = (ShLockerCommand){false, id, volume, flags, position};
}

static void locker_effect(ShLockerFrame *frame, int id, int intensity)
{
    if (frame->command_count >= SH_LOCKER_COMMAND_CAPACITY) { return; }
    frame->commands[frame->command_count++] = (ShLockerCommand){true, id, intensity, 0, {0}};
}

#define LOCKER_FROM(value, shift) ((value) >> (shift))
#include "locker_world_steps.h"

unsigned sh_locker_world_advance(ShLockerWorld *world, ShSavegame *save, ShLockerClock *clock,
    float dt, bool fade_finished, ShLockerFrame *frame, ShLockerEmit emit, void *context)
{
    if (dt <= 0) {
        sh_locker_world_step(world, save, 0, fade_finished, frame);
        return 0;
    }
    clock->pending_frames += (double)dt * 30.0;
    clock->pending_delta += SH_Q12(dt);
    frame->command_count = 0;
    unsigned frames = 0;
    while (clock->pending_frames >= 1.0) {
        const int32_t delta = (int32_t)llround((double)clock->pending_delta / clock->pending_frames);
        clock->pending_frames -= 1.0;
        clock->pending_delta -= delta;
        sh_locker_world_step(world, save, delta, fade_finished, frame);
        if (emit) { emit(frame, context); }
        frames++;
    }
    return frames;
}

/* WorldObjects_Add position is submitted in Q8 with Y down. */
void sh_world_vertical_vertices(int32_t offset_q8, const ShMapVertex *source, ShMapVertex *destination, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        destination[i] = source[i];
        destination[i].pos[1] -= offset_q8 / 256.0f;
    }
}

/* WorldObjects_Add retains the original Q12 Y angle (X/Z alone use Q10). */
void sh_locker_transform_vertices(int16_t rotation, const float pivot[3],
    const ShMapVertex *source, ShMapVertex *destination, size_t count)
{
    const int32_t sine = sh_fixed_sin(rotation), cosine = sh_fixed_cos(rotation);
    int32_t origin[3];
    for (int axis = 0; axis < 3; axis++) { origin[axis] = SH_Q12(pivot[axis]) >> 4; }
    for (size_t vertex = 0; vertex < count; vertex++) {
        int32_t local[3];
        for (int axis = 0; axis < 3; axis++) {
            local[axis] = (int32_t)(source[vertex].pos[axis] * (axis ? -256.0f : 256.0f)) - (int32_t)(pivot[axis] * 256.0f);
        }
        const int32_t x = (int32_t)(((int64_t)cosine * local[0] + (int64_t)sine * local[2]) >> 12);
        const int32_t z = (int32_t)((-(int64_t)sine * local[0] + (int64_t)cosine * local[2]) >> 12);
        destination[vertex] = source[vertex];
        destination[vertex].pos[0] = (origin[0] + x) / 256.0f;
        destination[vertex].pos[1] = -(origin[1] + local[1]) / 256.0f;
        destination[vertex].pos[2] = -(origin[2] + z) / 256.0f;
    }
}
