#include "sh3ds/combat_ray.h"
#include "sh3ds/fixed.h"
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#define RAY_TO_Q8(v) ((v) >> 4)
#define RAY_TO_Q12(v) ((int32_t)(v) * 16)
#define RAY_TO_Q12_SCALE(v) ((int32_t)(v) * 4096)
#define RAY_MULT(a, b) ((int32_t)((uint32_t)(a) * (uint32_t)(b)) >> 12)
#define RAY_SQUARE(v) ((v) * (v))

static uint32_t ray_magnitude(int32_t x, int32_t z)
{
    return sh_fixed_sqrt0((uint32_t)x * (uint32_t)x + (uint32_t)z * (uint32_t)z);
}

/* func_8006C248; gpu.h loads the negation of the entire packed direction into R13/R21. */
static int16_t ray_circle(uint32_t packed, int16_t length, int16_t dx, int16_t dz, int16_t radius)
{
    int16_t along = (int16_t)(((int32_t)(int16_t)packed * dx +
                                (int32_t)(int16_t)(packed >> 16) * dz) >> 12);
    uint32_t negative = 0u - packed;
    int16_t across = (int16_t)(((int32_t)(int16_t)(negative >> 16) * dx +
                                 (int32_t)(int16_t)packed * dz) >> 12);
    int16_t distance;
    if (along < 0) {
        distance = (int16_t)ray_magnitude(along, across);
    } else if (length < along) {
        int16_t beyond = (int16_t)(along - length);
        distance = (int16_t)ray_magnitude(beyond, across);
    } else {
        distance = (int16_t)abs(across);
    }
    if (!length) {
        return distance >= radius ? -1 : 0;
    }
    if (distance >= radius) {
        return -1;
    }
    int32_t root = (int32_t)sh_fixed_sqrt0(radius * radius - across * across);
    int16_t alpha = (int16_t)(((along - root) * 4096) / length);
    return alpha < 0 ? 0 : alpha > 4096 ? 4096 : alpha;
}

#include "combat_ray_steps.h"

void sh_combat_ray_character(ShRayState *state, ShRayCharacter *character)
{
    ray_shape(&state->field_6C, state->field_0, character);
    ray_character(state, character);
}

/* Ray_TraceRun result after all world and character candidates have been visited. */
void sh_combat_ray_finish(ShRayTrace *trace, const ShRayState *state,
                          const ShPathPoint *from, const ShPathPoint *offset)
{
    if (state->hitDistance == SHRT_MAX) {
        ray_miss(trace, from, offset, state->rayDistance);
        return;
    }
    trace->hasHit = true;
    trace->target.vx = RAY_TO_Q12(state->target.vx);
    trace->target.vy = RAY_TO_Q12(state->target.vy);
    trace->target.vz = RAY_TO_Q12(state->target.vz);
    trace->character = state->character;
    trace->hitDistance = RAY_TO_Q12(state->hitDistance);
    trace->groundHeight = RAY_TO_Q12(state->groundHeight);
    trace->headingAngle = sh_fixed_atan2(state->offsetX, state->offsetZ);
    trace->groundType = state->groundType;
}
