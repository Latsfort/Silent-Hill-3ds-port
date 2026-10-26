#include "sh3ds/silver.h"
#include "sh3ds/fixed.h"
#include <math.h>
#include <string.h>

#include "silver_motion_data.h"

void sh_silver_init(ShSilverMotion *silver)
{
    *silver = s_silver_hanging;
}

/* MapEvent_PianoPuzzleInteract skip and case 15 preserve D_800DD4FC. */
void sh_silver_ground(ShSilverMotion *silver)
{
    const int32_t velocity = silver->velocity;
    *silver = s_silver_ground;
    silver->velocity = velocity;
}

/* MapEvent_PianoPuzzleInteract case 12. */
void sh_silver_swing(ShSilverMotion *silver, int32_t delta)
{
    silver->rotation[0] = (int16_t)(silver->rotation[0] + sh_q12_mul(delta, SH_SILVER_SWING_SPEED));
    silver->position[1] = ((sh_fixed_cos(silver->rotation[0]) * SH_SILVER_SWING_RADIUS) >> 12) - SH_SILVER_SWING_Y;
    silver->position[2] = ((sh_fixed_sin(silver->rotation[0]) * SH_SILVER_SWING_RADIUS) >> 12) + SH_SILVER_SWING_Z;
}

/* MapEvent_PianoPuzzleInteract case 13; caller emits sound 1417 on true and advances. */
bool sh_silver_fall(ShSilverMotion *silver, int32_t delta, int32_t gravity)
{
    silver->velocity += gravity;
    silver->rotation[0] = (int16_t)(silver->rotation[0] + sh_q12_mul(delta, SH_SILVER_FALL_SPEED));
    silver->position[1] += sh_q12_mul(delta, silver->velocity);
    silver->position[2] += sh_q12_mul(delta, SH_SILVER_DRIFT_SPEED);
    return silver->position[1] >= 0;
}

void sh_silver_pose(const ShSilverMotion *silver, float position[3], float rotation[3])
{
    for (int axis = 0; axis < 3; axis++) {
        position[axis] = silver->position[axis] / 4096.0f;
        rotation[axis] = axis < 2 ? silver->rotation[axis] * (360.0f / 4096.0f) : 0.0f;
    }
}

/* map.c named-model Ry*Rx*Rz and Q8 translation; base is the unrotated hanging model. */
void sh_silver_transform_vertices(const ShSilverMotion *silver, const ShMapVertex *source,
                                  ShMapVertex *destination, size_t count)
{
    float position[3], angles[3];
    sh_silver_pose(silver, position, angles);
    const float d2r = 3.14159265f / 180.0f;
    const float rx = angles[0] * d2r;
    const float ry = angles[1] * d2r;
    const float cx = cosf(rx), sx = sinf(rx), cy = cosf(ry), sy = sinf(ry);
    const float rotation[3][3] = {{cy, sy * sx, sy * cx}, {0.0f, cx, -sx}, {-sy, cy * sx, cy * cx}};
    int32_t matrix[3][3];
    for (int axis = 0; axis < 3; axis++) {
        for (int column = 0; column < 3; column++) {
            matrix[axis][column] = (int32_t)(rotation[axis][column] * 4096.0f);
        }
    }
    for (size_t vertex = 0; vertex < count; vertex++) {
        int32_t local[3];
        for (int axis = 0; axis < 3; axis++) {
            local[axis] = (int32_t)(source[vertex].pos[axis] * (axis ? -256.0f : 256.0f)) -
                          s_silver_hanging.position[axis] / 16;
        }
        destination[vertex] = source[vertex];
        for (int axis = 0; axis < 3; axis++) {
            int32_t rotated = 0;
            for (int column = 0; column < 3; column++) { rotated += matrix[axis][column] * local[column]; }
            const int32_t value = (rotated >> 12) + (int32_t)(position[axis] * 256.0f);
            destination[vertex].pos[axis] = value / (axis ? -256.0f : 256.0f);
        }
    }
}
